#include "pch.h"
#include "IocpServer.h"
#include "Net.h"
#include "IoContext.h"
#include "ClientContext.h"
//#include "DataPacket.h"
#include "LockGuard.h"

#include <iostream>

#include <process.h>
#include <mstcpip.h>    //for struct tcp_keepalive

using namespace std;

//�����߳��˳���־
#define EXIT_THREAD 0
constexpr int POST_ACCEPT_CNT = 10;

IocpServer::IocpServer(short listenPort, int maxConnectionCount) :
    m_bIsShutdown(false)
    , m_hComPort(NULL)
    , m_hExitEvent(NULL)
    , m_listenPort(listenPort)
    , m_pListenCtx(nullptr)
    , m_nWorkerCnt(0)
    , m_nConnClientCnt(0)
    , m_nMaxConnClientCnt(maxConnectionCount)
{
    m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (WSA_INVALID_EVENT == m_hExitEvent)
    {
        cout << "CreateEvent failed with error: " << WSAGetLastError() << endl;
    }
    InitializeCriticalSection(&m_csConnList);
}

IocpServer::~IocpServer()
{
    stop();

    DeleteCriticalSection(&m_csConnList);
    Net::unInit();
}

bool IocpServer::start()
{
    if (!Net::init())
    {
        cout << "network initial failed" << endl;
        return false;
    }
    if (!createListenClient(m_listenPort))
    {
        return false;
    }
    if (!createIocpWorker())
    {
        return false;
    }
    if (!initAcceptIoContext())
    {
        return false;
    }
    return true;
}

bool IocpServer::stop()
{
    //ͬ���ȴ����й����߳��˳�
    exitIocpWorker();
    //�رչ����߳̾��
    for_each(m_hWorkerThreads.begin(), m_hWorkerThreads.end(),
        [](const HANDLE& h) { CloseHandle(h); });

    for_each(m_acceptIoCtxList.begin(), m_acceptIoCtxList.end(),
        [](IoContext* pIoCtx) {

        CancelIo((HANDLE)pIoCtx->m_socket);
        closesocket(pIoCtx->m_socket);
        pIoCtx->m_socket = INVALID_SOCKET;

        while (!HasOverlappedIoCompleted(&pIoCtx->m_overlapped))
            Sleep(1);

        delete pIoCtx;
    });
    m_acceptIoCtxList.clear();

    if (m_hExitEvent)
    {
        CloseHandle(m_hExitEvent);
        m_hExitEvent = NULL;
    }
    if (m_hComPort)
    {
        CloseHandle(m_hComPort);
        m_hComPort = NULL;
    }
    if (m_pListenCtx)
    {
        closesocket(m_pListenCtx->m_socket);
        m_pListenCtx->m_socket = INVALID_SOCKET;
        delete m_pListenCtx;
        m_pListenCtx = nullptr;
    }
    removeAllClientContext();

    return true;
}

bool IocpServer::shutdown()
{
    m_bIsShutdown = true;

    int ret = CancelIoEx((HANDLE)m_pListenCtx->m_socket, NULL);
    if (0 == ret)
    {
        cout << "CancelIoEx failed with error: " << WSAGetLastError() << endl;
        return false;
    }
    closesocket(m_pListenCtx->m_socket);
    m_pListenCtx->m_socket = INVALID_SOCKET;

    for_each(m_acceptIoCtxList.begin(), m_acceptIoCtxList.end(),
        [](IoContext* pIoCtx) {
        int ret = CancelIoEx((HANDLE)pIoCtx->m_socket, &pIoCtx->m_overlapped);
        if (0 == ret)
        {
            cout << "CancelIoEx failed with error: " << WSAGetLastError() << endl;
            return false;
        }
        closesocket(pIoCtx->m_socket);
        pIoCtx->m_socket = INVALID_SOCKET;

        while (!HasOverlappedIoCompleted(&pIoCtx->m_overlapped))
            Sleep(1);

        delete pIoCtx;
    });
    m_acceptIoCtxList.clear();


    return false;
}

unsigned WINAPI IocpServer::IocpWorkerThread(LPVOID arg)
{
    IocpServer* pThis = static_cast<IocpServer*>(arg);

    int             ret;
    DWORD           dwBytes;
    ULONG_PTR       lpCompletionKey;
    LPOVERLAPPED    lpOverlapped = nullptr;
    ClientContext*  pClientCtx = nullptr;
    IoContext*      pIoCtx = nullptr;

    while (WAIT_OBJECT_0 != WaitForSingleObject(pThis->m_hExitEvent, 0))
    {
        ret = GetQueuedCompletionStatus(pThis->m_hComPort, &dwBytes, 
            &lpCompletionKey, &lpOverlapped, INFINITE);
        if (EXIT_THREAD == lpCompletionKey)
        {
            //�˳������߳�
            cout << "EXIT_THREAD" << endl;
            break;
        }
        if (0 == ret)
        {
            cout << "GetQueuedCompletionStatus failed with error: " << WSAGetLastError() << endl;
            return 0;
        }
        //shutdown״̬��ֹͣ��������
        if (pThis->m_bIsShutdown && lpCompletionKey == (ULONG_PTR)pThis)
        {
            continue;
        }

        pClientCtx = (ClientContext*)lpCompletionKey;
        pIoCtx = (IoContext*)lpOverlapped;

        pIoCtx->m_dwBytesTransferred = dwBytes;
        if (0 == pIoCtx->m_dwBytesTransferred)
        {
            pIoCtx->m_postType = PostType::CLOSE_EVENT;
        }

        switch (pIoCtx->m_postType)
        {
        case PostType::ACCEPT_EVENT:
            pThis->handleAccept(pClientCtx, pIoCtx);
            break;
        case PostType::RECV_EVENT:
            pThis->handleRecv(pClientCtx, pIoCtx);
            break;
        case PostType::SEND_EVENT:
            pThis->handleSend(pClientCtx, pIoCtx);
            break;
        case PostType::CLOSE_EVENT:
            pThis->handleClose(pClientCtx, pIoCtx);
            break;
        default:
            pThis->handleClose(pClientCtx, pIoCtx);
            break;
        }
    }

    cout << "exit" << endl;
    return 0;
}

HANDLE IocpServer::associateWithCompletionPort(SOCKET s, ULONG_PTR completionKey)
{
    HANDLE hRet;
    ClientContext* pClientCtx = (ClientContext*)completionKey;
    if (NULL == completionKey)
    {
        hRet = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    }
    else 
    {
        hRet = CreateIoCompletionPort((HANDLE)s, m_hComPort, completionKey, 0);
    }
    if (NULL == hRet)
    {
        cout << "failed to associate the socket with completion port" << endl;
    }
    return hRet;
}

bool IocpServer::getAcceptExPtr()
{
    DWORD dwBytes;
    GUID GuidAcceptEx = WSAID_ACCEPTEX;
    LPFN_ACCEPTEX lpfnAcceptEx = NULL;
    int ret = WSAIoctl(m_pListenCtx->m_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &GuidAcceptEx, sizeof(GuidAcceptEx),
        &lpfnAcceptEx, sizeof(lpfnAcceptEx),
        &dwBytes, NULL, NULL);
    if (SOCKET_ERROR == ret)
    {
        cout << "WSAIoctl failed with error: " << WSAGetLastError();
        closesocket(m_pListenCtx->m_socket);
        return false;
    }
    m_lpfnAcceptEx = lpfnAcceptEx;
    return true;
}

bool IocpServer::getAcceptExSockaddrs()
{
    DWORD dwBytes;
    GUID GuidAddrs = WSAID_GETACCEPTEXSOCKADDRS;
    LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExAddr = NULL;
    int ret = WSAIoctl(m_pListenCtx->m_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &GuidAddrs, sizeof(GuidAddrs),
        &lpfnGetAcceptExAddr, sizeof(lpfnGetAcceptExAddr),
        &dwBytes, NULL, NULL);
    if (SOCKET_ERROR == ret)
    {
        cout << "WSAIoctl failed with error: " << WSAGetLastError();
        closesocket(m_pListenCtx->m_socket);
        return false;
    }
    m_lpfnGetAcceptExAddr = lpfnGetAcceptExAddr;
    return true;
}

bool IocpServer::setKeepAlive(SOCKET s, IoContext* pIoCtx, int time, int interval)
{
    if (!Net::setKeepAlive(s, TRUE))
        return false;

    tcp_keepalive keepAlive;
    keepAlive.onoff = 1;
    keepAlive.keepalivetime = time * 1000;        //30�� 
    keepAlive.keepaliveinterval = interval * 1000;    //���10��
    DWORD dwBytes;
    //����msdn����Ҫ��һ��OVERRLAP�ṹ
    int ret = WSAIoctl(s, SIO_KEEPALIVE_VALS,
        &keepAlive, sizeof(tcp_keepalive), NULL, 0,
        &dwBytes, &pIoCtx->m_overlapped, NULL);
    if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
    {
        cout << "WSAIoctl failed with error: " << WSAGetLastError() << endl;
        return false;
    }
    return true;
}

bool IocpServer::createListenClient(short listenPort)
{
    m_pListenCtx = new ListenContext(listenPort);

    //������ɶ˿�
    m_hComPort = associateWithCompletionPort(INVALID_SOCKET, NULL);
    if (NULL == m_hComPort)
        return false;

    //��������socket����ɶ˿ڣ����ｫthisָ����ΪcompletionKey����ɶ˿�
    if (NULL == associateWithCompletionPort(m_pListenCtx->m_socket, (ULONG_PTR)this))
    {
        return false;
    }

    if (SOCKET_ERROR == Net::bind(m_pListenCtx->m_socket, &m_pListenCtx->m_addr))
    {
        cout << "bind failed" << endl;
        return false;
    }

    if (SOCKET_ERROR == Net::listen(m_pListenCtx->m_socket))
    {
        cout << "listen failed" << endl;
        return false;
    }

    //��ȡacceptEx����ָ��
    if (!getAcceptExPtr())
        return false;

    //��ȡGetAcceptExSockaddrs����ָ��
    if (!getAcceptExSockaddrs())
        return false;

    return true;
}

bool IocpServer::createIocpWorker()
{
    //����CPU��������IO�߳�
    HANDLE hWorker;
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors; ++i)
    {
        hWorker = (HANDLE)_beginthreadex(NULL, 0, IocpWorkerThread, this, 0, NULL);
        if (NULL == hWorker)
        {
            return false;
        }
        m_hWorkerThreads.emplace_back(hWorker);
        ++m_nWorkerCnt;
    }
    cout << "started iocp worker thread count: " << m_nWorkerCnt << endl;
    return true;
}

bool IocpServer::exitIocpWorker()
{
    int ret = 0;
    SetEvent(m_hExitEvent);
    for (int i = 0; i < m_nWorkerCnt; ++i)
    {
        //֪ͨ�����߳��˳�
        ret = PostQueuedCompletionStatus(m_hComPort, 0, EXIT_THREAD, NULL);
        if (FALSE == ret)
        {
            cout << "PostQueuedCompletionStatus failed with error: " << WSAGetLastError() << endl;
        }
    }

    //���ﲻ����Ϊʲô�᷵��0������Ӧ�÷���m_nWorkerCnt-1��
    ret = WaitForMultipleObjects(m_nWorkerCnt, m_hWorkerThreads.data(), TRUE, INFINITE);

    return true;
}

bool IocpServer::initAcceptIoContext()
{
    //Ͷ��accept����
    for (int i = 0; i < POST_ACCEPT_CNT; ++i)
    {
        IoContext* pListenIoCtx = IoContext::newIoContext(PostType::ACCEPT_EVENT);

        pListenIoCtx->newBuffer();

        m_acceptIoCtxList.emplace_back(pListenIoCtx);
        if (!postAccept(pListenIoCtx))
        {
            return false;
        }
    }
    return true;
}

bool IocpServer::postAccept(IoContext* pIoCtx)
{
    pIoCtx->resetBuffer();

    char* pBuf = pIoCtx->m_wsaBuf.buf;
    ULONG nLen = pIoCtx->m_wsaBuf.len;
    OVERLAPPED* pOverlapped = &pIoCtx->m_overlapped;
    LPFN_ACCEPTEX lpfnAcceptEx = (LPFN_ACCEPTEX)m_lpfnAcceptEx;

    pIoCtx->m_socket = Net::WSASocket_();
    if (SOCKET_ERROR == pIoCtx->m_socket)
    {
        cout << "create socket failed" << endl;
        return false;
    }

    DWORD dwRecvByte;
    if (FALSE == lpfnAcceptEx(m_pListenCtx->m_socket, pIoCtx->m_socket, pBuf,
        nLen - ACCEPT_ADDRS_SIZE, sizeof(SOCKADDR) + 16, sizeof(SOCKADDR) + 16,
        &dwRecvByte, pOverlapped))
    {
        if (WSA_IO_PENDING != WSAGetLastError())
        {
            cout << "acceptEx failed" << endl;
            return false;
        }
    }

    return true;
}

PostResult IocpServer::postRecv(ClientContext* pConnClient)
{
    PostResult result = PostResult::PostResultSuccesful;
    IoContext* pIoCtx = pConnClient->m_recvIoCtx;

    pIoCtx->newBuffer();

    pIoCtx->resetBuffer();

    LockGuard lk(&pConnClient->m_csLock);
    if (INVALID_SOCKET != pConnClient->m_socket)
    {
        DWORD dwBytes;
        //���������־����û�����������һ�ν���
        DWORD flag = MSG_PARTIAL;
        int ret = WSARecv(pIoCtx->m_socket, &pIoCtx->m_wsaBuf, 1,
            &dwBytes, &flag, &pIoCtx->m_overlapped, NULL);
        if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
        {
            cout << "WSARecv failed with error: " << WSAGetLastError() << endl;
            result = PostResult::PostResultFailed;
        }
    }
    return result;
}

PostResult IocpServer::postSend(ClientContext* pConnClient)
{
    PostResult result = PostResult::PostResultSuccesful;
    DWORD dwBytesSent;
    DWORD flag = MSG_PARTIAL;
    IoContext* pSendIoCtx = pConnClient->m_sendIoCtx;

    pSendIoCtx->newBuffer();
    pSendIoCtx->m_buf = pConnClient->m_recvIoCtx->m_buf;

    //pSendIoCtx->m_wsaBuf.buf = pConnClient->m_outBuf.begin();
    //pSendIoCtx->m_wsaBuf.len = pConnClient->m_outBuf.size();

    LockGuard lk(&pConnClient->m_csLock);
    if (INVALID_SOCKET != pConnClient->m_socket)
    {
        int ret = WSASend(pSendIoCtx->m_socket, &pSendIoCtx->m_wsaBuf, 1, &dwBytesSent,
            flag, &pSendIoCtx->m_overlapped, NULL);
        if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
        {
            cout << "WSASend failed with error: " << WSAGetLastError() << endl;
            result = PostResult::PostResultFailed;
        }
    }
    return result;
}

#if 0
bool IocpServer::postParse(ClientContext* pConnClient, IoContext* pIoCtx)
{
    DWORD dwTransferred = pIoCtx->m_wsaBuf.len;
    int ret = PostQueuedCompletionStatus(m_hComPort, dwTransferred, (ULONG_PTR)pConnClient, &pIoCtx->m_overlapped);
    if (FALSE == ret)
    {
        cout << "PostQueuedCompletionStatus failed with error: " << WSAGetLastError() << endl;
        return false;
    }
    return true;
}
#endif

bool IocpServer::handleAccept(ClientContext* pListenClient, IoContext* pIoCtx)
{
    Net::updateAcceptContext(pListenClient->m_socket, pIoCtx->m_socket);

    //�ﵽ�����������ر��µ�socket
    if (m_nConnClientCnt == m_nMaxConnClientCnt)
    {
        closesocket(pIoCtx->m_socket);
        pIoCtx->m_socket = INVALID_SOCKET;
        postAccept(pIoCtx);
        return true;
    }
    InterlockedIncrement(&m_nConnClientCnt);

    LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExAddr = (LPFN_GETACCEPTEXSOCKADDRS)m_lpfnGetAcceptExAddr;
    char* pBuf = pIoCtx->m_wsaBuf.buf;
    ULONG nLen = pIoCtx->m_wsaBuf.len;
    LPSOCKADDR_IN localAddr = nullptr;
    LPSOCKADDR_IN peerAddr = nullptr;
    int localAddrLen = sizeof(SOCKADDR_IN);
    int peerAddrLen = sizeof(SOCKADDR_IN);

    lpfnGetAcceptExAddr(pBuf, nLen - ACCEPT_ADDRS_SIZE, localAddrLen + 16,
        peerAddrLen + 16, (LPSOCKADDR*)&localAddr, &localAddrLen,
        (LPSOCKADDR*)&peerAddr, &peerAddrLen);

    char localAddrBuf[1024] = { 0 };
    inet_ntop(AF_INET, &localAddr->sin_addr, localAddrBuf, 1024);
    char peerAddrBuf[1024] = { 0 };
    inet_ntop(AF_INET, &peerAddr->sin_addr, peerAddrBuf, 1024);
    //cout << "local address: " << inet_ntoa(localAddr->sin_addr) << ":" << ntohs(localAddr->sin_port) << "\t"
    //    << "peer address: " << inet_ntoa(peerAddr->sin_addr) << ":" << ntohs(peerAddr->sin_port) << endl;
    cout << "local address: " << localAddrBuf << ":" << ntohs(localAddr->sin_port) << "\t"
        << "peer address: " << peerAddrBuf << ":" << ntohs(peerAddr->sin_port) << endl;

    //�����µ�ClientContext�������µ�����socket��ԭ����IoContextҪ���������µ�����
    ClientContext* pConnClient = new ClientContext(pIoCtx->m_socket);
    memcpy_s(&pConnClient->m_addr, peerAddrLen, peerAddr, peerAddrLen);

    if (NULL == associateWithCompletionPort(pConnClient->m_socket, (ULONG_PTR)pConnClient))
    {
        return false;
    }

    ////������������
    //setKeepAlive(pConnClient->m_socket, pIoCtx);

    pConnClient->appendToBuffer(pBuf, pIoCtx->m_dwBytesTransferred);

    //Ͷ��һ���µ�accpet����
    postAccept(pIoCtx);

    //���ͻ��˼��������б�
    addClientContext(pConnClient);

    ////�����ݰ�
    //pConnClient->decodePacket();

    echo(pConnClient);

    //Ͷ��recv����
    PostResult result = postRecv(pConnClient);
    if (PostResult::PostResultFailed == result)
    {
        CloseClient(pConnClient);
        removeClientContext(pConnClient);
    }

    return true;
}

bool IocpServer::handleRecv(ClientContext* pConnClient, IoContext* pIoCtx)
{
    char* pBuf = pIoCtx->m_wsaBuf.buf;
    int nLen = pIoCtx->m_wsaBuf.len;

    //����
    pConnClient->appendToBuffer(pBuf, pIoCtx->m_dwBytesTransferred);

    ////�����ݰ�
    //pConnClient->decodePacket();

    ////Ͷ��send����
    //IoContext* pSendIoCtx = pConnClient->getIoContext(PostType::SEND_EVENT);
    //postSend(pSendIoCtx);

    echo(pConnClient);

    //Ͷ��recv����
    PostResult result = postRecv(pConnClient);
    if (PostResult::PostResultFailed == result)
    {
        CloseClient(pConnClient);
        removeClientContext(pConnClient);
    }

    return true;
}

bool IocpServer::handleSend(ClientContext* pConnClient, IoContext* pIoCtx)
{
    return true;
}

#if 0
bool IocpServer::handleParse(ClientContext* pConnClient, IoContext* pIoCtx)
{
    //�������ݰ�������
    //DataPacket::parse(pConnClient);

    //Ͷ��send����
    PostResult result = postSend(pConnClient);
    if (PostResult::PostResultFailed == result)
    {
        CloseClient(pConnClient);
        removeClientContext(pConnClient);
    }

    return true;
}
#endif

bool IocpServer::handleClose(ClientContext* pConnClient, IoContext* pIoCtx)
{
    CloseClient(pConnClient);
    removeClientContext(pConnClient);
    return true;
}

void IocpServer::addClientContext(ClientContext* pConnClient)
{
    LockGuard lk(&m_csConnList);
    m_connList.emplace_back(pConnClient);
}

void IocpServer::removeClientContext(ClientContext* pConnClient)
{
    LockGuard lk(&m_csConnList);
    m_connList.remove(pConnClient);
    delete pConnClient;
}

void IocpServer::removeAllClientContext()
{
    LockGuard lk(&m_csConnList);
    for_each(m_connList.begin(), m_connList.end(),
        [](ClientContext* it) { delete it; });
    m_connList.erase(m_connList.begin(), m_connList.end());
}

void IocpServer::CloseClient(ClientContext* pConnClient)
{
    SOCKET s;

    {
        LockGuard lk(&pConnClient->m_csLock);
        s = pConnClient->m_socket;
        pConnClient->m_socket = INVALID_SOCKET;
    }

    if (INVALID_SOCKET != s)
    {
        if (!Net::setLinger(s))
            return;

        int ret = CancelIoEx((HANDLE)s, NULL);
        if (0 == ret)
        {
            cout << "CancelIoEx failed with error: " << WSAGetLastError() << endl;
            return;
        }

        closesocket(s);

        InterlockedDecrement(&m_nConnClientCnt);
    }
}

bool IocpServer::decodePacket()
{

    return false;
}

void IocpServer::echo(ClientContext* pConnClient)
{
    //pConnClient->m_outBuf = pConnClient->m_inBuf;
    //pConnClient->m_inBuf.consume(pConnClient->m_inBuf.size());

    IoContext* pSendIoCtx = pConnClient->m_sendIoCtx;
    pSendIoCtx->m_buf = new Buffer();
    //pSendIoCtx->m_wsaBuf.buf = (char*)pSendIoCtx->m_buf->begin();
    //pSendIoCtx->m_wsaBuf.len = pSendIoCtx->m_buf->length();

    postSend(pConnClient);
}

