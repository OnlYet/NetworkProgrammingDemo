#include "pch.h"
#include "IocpServer.h"
#include "Net.h"
#include "DataPacket.h"

#include <iostream>

#include <process.h>
#include <MSWSock.h>
#include <mstcpip.h>    //for struct tcp_keepalive

using namespace std;

#define EXIT_THREAD 0
constexpr int POST_ACCEPT_CNT = 10;

IocpServer::IocpServer(short listenPort) :
	m_listenPort(listenPort)
	, m_pListenClient(nullptr)
	, m_nWorkerCnt(0)
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
	if (m_pListenClient)
	{
        delete m_pListenClient;
        m_pListenClient = nullptr;
	}

    DeleteCriticalSection(&m_csConnList);

    CloseHandle(m_hComPort);

	Net::unInit();
}

bool IocpServer::init()
{
    if (!Net::init())
    {
        cout << "network initial failed" << endl;
        return false;
    }

	if (!createListenClient(m_listenPort))
		return false;

	if (!createIocpWorker())
		return false;


	//Ͷ��accept����
	for (int i = 0; i < POST_ACCEPT_CNT; ++i)
	{
		IoContext* pListenIoCtx = m_pListenClient->getIoContext(PostType::ACCEPT_EVENT);
		if (!postAccept(pListenIoCtx))
		{
			m_pListenClient->removeIoContext(pListenIoCtx);
			return false;
		}
	}

    return true;
}

bool IocpServer::start()
{
    return false;
}

bool IocpServer::exit()
{
    SetEvent(m_hExitEvent);
    for (int i = 0; i < m_nWorkerCnt; ++i)
    {
        //֪ͨ�����߳��˳�
        int ret = PostQueuedCompletionStatus(m_hComPort, 0, EXIT_THREAD, NULL);
        if (FALSE == ret)
        {
            cout << "PostQueuedCompletionStatus failed with error: " << WSAGetLastError() << endl;
        }
    }
    return true;
}

unsigned WINAPI IocpServer::IocpWorkerThread(LPVOID arg)
{
    IocpServer* pThis = static_cast<IocpServer*>(arg);

    DWORD           dwBytes;
    ClientContext*  pClientCtx = nullptr;
    IoContext*      pIoCtx = nullptr;

    while (WAIT_OBJECT_0 != WaitForSingleObject(pThis->m_hExitEvent, 0))
    {
        int ret = GetQueuedCompletionStatus(pThis->m_hComPort, &dwBytes, (PULONG_PTR)&pClientCtx, (LPOVERLAPPED*)&pIoCtx, INFINITE);
        if (EXIT_THREAD == pClientCtx)
        {
            //�˳������߳�
            break;
        }
        if(0 == ret)
        {
            cout << "GetQueuedCompletionStatus failed";
            return 0;
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
        default:
            break;
        }
    }

	return 0;
}

bool IocpServer::createListenClient(short listenPort)
{
	//������ɶ˿�
	m_hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (NULL == m_hComPort)
		return false;

	//���������ص����ܵ�socket
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (SOCKET_ERROR == listenSocket)
	{
		cout << "create socket failed" << endl;
		return false;
	}
    m_pListenClient = new ClientContext(listenSocket);

	//��������socket����ɶ˿�
	CreateIoCompletionPort((HANDLE)m_pListenClient->m_socket, m_hComPort, (ULONG_PTR)m_pListenClient, 0);

	SecureZeroMemory(&m_pListenClient->m_addr, sizeof(SOCKADDR));
	m_pListenClient->m_addr.sin_family = AF_INET;
	m_pListenClient->m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_pListenClient->m_addr.sin_port = htons(listenPort);

	if (SOCKET_ERROR == bind(m_pListenClient->m_socket, (SOCKADDR*)&m_pListenClient->m_addr, sizeof(SOCKADDR)))
	{
		cout << "bind failed" << endl;
        closesocket(m_pListenClient->m_socket);
		return false;
	}

	if (SOCKET_ERROR == listen(m_pListenClient->m_socket, SOMAXCONN))
	{
		cout << "listen failed" << endl;
        closesocket(m_pListenClient->m_socket);
		return false;
	}

	//��ȡacceptEx����ָ��
	DWORD dwBytes;
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	LPFN_ACCEPTEX lpfnAcceptEx = NULL;
	int ret = WSAIoctl(m_pListenClient->m_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&lpfnAcceptEx, sizeof(lpfnAcceptEx),
		&dwBytes, NULL, NULL);
	if (SOCKET_ERROR == ret)
	{
		cout << "WSAIoctl failed with error: " << WSAGetLastError();
        closesocket(m_pListenClient->m_socket);
		return false;
	}
	m_lpfnAcceptEx = lpfnAcceptEx;

    //��ȡGetAcceptExSockaddrs����ָ��
    GUID GuidAddrs = WSAID_GETACCEPTEXSOCKADDRS;
    LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExAddr = NULL;
    ret = WSAIoctl(m_pListenClient->m_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &GuidAddrs, sizeof(GuidAddrs),
        &lpfnGetAcceptExAddr, sizeof(lpfnGetAcceptExAddr),
        &dwBytes, NULL, NULL);
    if (SOCKET_ERROR == ret)
    {
        cout << "WSAIoctl failed with error: " << WSAGetLastError();
        closesocket(m_pListenClient->m_socket);
        return false;
    }
    m_lpfnGetAcceptExAddr = lpfnGetAcceptExAddr;

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
		hWorker = (HANDLE)_beginthreadex(NULL, 0, IocpWorkerThread, this,  0, NULL);
		if (NULL == hWorker)
		{
			CloseHandle(m_hComPort);
			return false;
		}

		++m_nWorkerCnt;
	}

	return true;
}

bool IocpServer::postAccept(IoContext* pIoCtx)
{
	char* pBuf					= pIoCtx->m_wsaBuf.buf;
	ULONG nLen					= pIoCtx->m_wsaBuf.len;
	OVERLAPPED* pOverlapped		= &pIoCtx->m_overlapped;
	LPFN_ACCEPTEX lpfnAcceptEx	= (LPFN_ACCEPTEX)m_lpfnAcceptEx;

	pIoCtx->m_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (SOCKET_ERROR == pIoCtx->m_socket)
	{
		cout << "create socket failed" << endl;
		return false; 
	}

	DWORD dwRecvByte;
	if (FALSE == lpfnAcceptEx(m_pListenClient->m_socket, pIoCtx->m_socket, pBuf,
		nLen - ((sizeof(SOCKADDR) + 16) * 2), sizeof(SOCKADDR) + 16,
		sizeof(SOCKADDR) + 16, &dwRecvByte, pOverlapped))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			cout << "acceptEx failed" << endl;
			return false;
		}
	}

	return true;
}

bool IocpServer::postRecv(IoContext* pIoCtx)
{
    DWORD dwBytes;
    //���������־����û�����������һ�ν���
    DWORD flag = MSG_PARTIAL;
    int ret = WSARecv(pIoCtx->m_socket, &pIoCtx->m_wsaBuf, 1,
        &dwBytes, &flag, &pIoCtx->m_overlapped, NULL);
    if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
    {
        cout << "WSARecv failed with error: " << WSAGetLastError();
        return false;
    }
    return true;
}

bool IocpServer::postSend(IoContext* pIoCtx)
{
    DWORD dwBytesSent;
    DWORD flag = MSG_PARTIAL;
    int ret = WSASend(pIoCtx->m_socket, &pIoCtx->m_wsaBuf, 1, &dwBytesSent,
        flag, &pIoCtx->m_overlapped, NULL);
    if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
    {
        cout << "WSASend failed with error: " << WSAGetLastError();
        return false;
    }
    return true;
}

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

bool IocpServer::handleAccept(ClientContext* pListenClient, IoContext* pIoCtx)
{
    LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExAddr = (LPFN_GETACCEPTEXSOCKADDRS)m_lpfnGetAcceptExAddr;
    char* pBuf = pIoCtx->m_wsaBuf.buf;
    ULONG nLen = pIoCtx->m_wsaBuf.len;
    LPSOCKADDR_IN localAddr = nullptr;
    LPSOCKADDR_IN peerAddr = nullptr;
    int localAddrLen = sizeof(SOCKADDR_IN);
    int peerAddrLen = sizeof(SOCKADDR_IN);

    lpfnGetAcceptExAddr(pBuf, nLen - ((localAddrLen + 16) * 2), localAddrLen + 16,
        peerAddrLen + 16, (LPSOCKADDR*)&localAddr, &localAddrLen,
        (LPSOCKADDR*)&peerAddr, &peerAddrLen);

    cout << "local address: " << ntohl(localAddr->sin_addr.s_addr) << ":" << ntohs(localAddr->sin_port) << "\t"
        << "peer address: " << ntohl(peerAddr->sin_addr.s_addr) << ":" << ntohs(peerAddr->sin_port) << endl;

    //�����µ�ClientContext�������µ�����socket��ԭ����IoContextҪ���������µ�����
    ClientContext* pConnClient = new ClientContext(pIoCtx->m_socket);
    memcpy_s(&pConnClient->m_addr, peerAddrLen, peerAddr, peerAddrLen);

    //���������ӵ�socket����ɶ˿ڣ�ֻ�й����ˣ������յ���socket��IO���֪ͨ��GetQueuedCompletionStatus����ȡ�ذ�
    if (!Net::associateWithCompletionPort(m_hComPort, pConnClient))
    {
        return false;
    }

    //������������
    Net::setKeepAlive(pConnClient->m_socket, TRUE);
    tcp_keepalive keepAlive;
    keepAlive.onoff = 1;
    keepAlive.keepalivetime = 30 * 1000;        //30�� 
    keepAlive.keepaliveinterval = 10 * 1000;    //���10��
    DWORD dwBytes;
    //����msdn����Ҫ��һ��OVERRLAP�ṹ
    int ret = WSAIoctl(pConnClient->m_socket, SIO_KEEPALIVE_VALS, 
        &keepAlive, sizeof(tcp_keepalive), NULL, 0, 
        &dwBytes, &pIoCtx->m_overlapped, NULL);
    if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
    {
        cout << "WSAIoctl failed with error: " << WSAGetLastError() << endl;
        return false;
    }

    /** 
    * ����һ�ν��յ������ݿ�����ClientContext::m_inBuf
    * �˴�����������Ϊ���ڻ�û�������̷߳���m_inBuf
    */
    pConnClient->m_inBuf.append(pBuf, nLen - ((localAddrLen + 16) * 2));

    //Ͷ��һ���µ�accpet����
    pIoCtx->resetBuffer();
    postAccept(pIoCtx);

    //���ͻ��˼��������б�
    addClient(pConnClient);

    //Ͷ�ݽ�������
    IoContext* pParseIoCtx = pConnClient->getIoContext(PostType::PARSE_EVNET);
    postParse(pConnClient, pParseIoCtx);

    //Ͷ��recv����
    IoContext* pRecvIoCtx = pConnClient->getIoContext(PostType::RECV_EVENT);
    postRecv(pRecvIoCtx);

    return true;
}

bool IocpServer::handleRecv(ClientContext* pConnClient, IoContext* pIoCtx)
{
    char* pBuf = pIoCtx->m_wsaBuf.buf;
    int nLen = pIoCtx->m_wsaBuf.len;

    pConnClient->m_inBuf.append(pBuf, nLen - (sizeof(SOCKADDR_IN) + 16) * 2);

    //Ͷ��send����
    IoContext* pSendIoCtx = pConnClient->getIoContext(PostType::SEND_EVENT);
    postSend(pSendIoCtx);

    //Ͷ���µ�recv����
    IoContext* pRecvIoCtx = pConnClient->getIoContext(PostType::RECV_EVENT);
    postRecv(pRecvIoCtx);

    return true;
}

bool IocpServer::handleSend(ClientContext* pConnClient, IoContext* pIoCtx)
{
    return false;
}

bool IocpServer::handleParse(ClientContext* pConnClient, IoContext* pIoCtx)
{
    //�������ݰ�
    DataPacket::parse(pConnClient->m_inBuf);

    //Ͷ��send����
    IoContext* pRecvIoCtx = pConnClient->getIoContext(PostType::SEND_EVENT);
    postSend(pRecvIoCtx);

    return true;
}

void IocpServer::addClient(ClientContext* pConnClient)
{
    EnterCriticalSection(&m_csConnList);
    m_connList.emplace_back(pConnClient);
    LeaveCriticalSection(&m_csConnList);
}
