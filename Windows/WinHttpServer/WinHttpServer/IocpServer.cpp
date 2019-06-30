#include "pch.h"
#include "IocpServer.h"
#include "Net.h"
#include "DataPacket.h"

#include <iostream>

#include <process.h>
#include <MSWSock.h>


using namespace std;

constexpr int POST_ACCEPT_CNT = 10;

IocpServer::IocpServer(short listenPort) :
	m_listenPort(listenPort)
	, m_pListenClient(nullptr)
	, m_nWorkerCnt(0)
{
}

IocpServer::~IocpServer()
{
	if (m_pListenClient)
	{
		if (INVALID_SOCKET != m_pListenClient->m_socket)
			closesocket(m_pListenClient->m_socket);
	}

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
		IoContext* pListenIoCtx = m_pListenClient->createIoContext(PostType::ACCEPT_EVENT);
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

bool IocpServer::stop()
{
    return false;
}

unsigned WINAPI IocpServer::IocpWorkerThread(LPVOID arg)
{
    IocpServer* pThis = static_cast<IocpServer*>(arg);

    DWORD           dwBytes;
    ClientContext*  pClientCtx = nullptr;
    IoContext*      pIoCtx = nullptr;

    while (1)
    {
        int ret = GetQueuedCompletionStatus(pThis->m_comPort, &dwBytes, (PULONG_PTR)&pClientCtx, (LPOVERLAPPED*)&pIoCtx, INFINITE);
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
	m_comPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (NULL == m_comPort)
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
	CreateIoCompletionPort((HANDLE)m_pListenClient->m_socket, m_comPort, (ULONG_PTR)m_pListenClient, 0);

	SecureZeroMemory(&m_pListenClient->m_addr, sizeof(SOCKADDR));
	m_pListenClient->m_addr.sin_family = AF_INET;
	m_pListenClient->m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_pListenClient->m_addr.sin_port = htons(listenPort);

	if (SOCKET_ERROR == bind(m_pListenClient->m_socket, (SOCKADDR*)&m_pListenClient->m_addr, sizeof(SOCKADDR)))
	{
		cout << "bind failed" << endl;
		return false;
	}

	if (SOCKET_ERROR == listen(m_pListenClient->m_socket, SOMAXCONN))
	{
		cout << "listen failed" << endl;
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
			CloseHandle(m_comPort);
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
    //���������ӵ�socket����ɶ˿ڣ�ֻ�й����ˣ������յ���socket��IO���֪ͨ��GetQueuedCompletionStatus����ȡ�ذ�
    HANDLE hRet = CreateIoCompletionPort((HANDLE)pConnClient->m_socket, m_comPort, (ULONG_PTR)pConnClient, 0);
    if (NULL == hRet)
    {
        cout << "failed to associate the accept socket with completion port" << endl;
        return false;
    }

    memcpy_s(&pConnClient->m_addr, peerAddrLen, peerAddr, peerAddrLen);
    //����һ�ν��յ������ݿ�����ClientContext::m_inBuf
    pConnClient->m_inBuf.append(pBuf, nLen - ((localAddrLen + 16) * 2));


    //���������ӿͻ����б�
    m_ConnClients.emplace_back(pConnClient);

    //postRecv,postSend,�������ݰ�
    DataPacket::parse(pConnClient->m_inBuf);

    //ΪͶ��Recv���󴴽��µ�IoContext
    IoContext* pRecvIoCtx = pConnClient->createIoContext(PostType::RECV_EVENT);

    //Ͷ��recv����
    postRecv(pRecvIoCtx);

    //Ͷ��һ���µ�accpet����
    pIoCtx->resetBuffer();
    postAccept(pIoCtx);

    return true;
}

bool IocpServer::handleRecv(ClientContext* pConnClient, IoContext* pIoCtx)
{
    char* pBuf = pIoCtx->m_wsaBuf.buf;
    int nLen = pIoCtx->m_wsaBuf.len;

    pConnClient->m_inBuf.append(pBuf, nLen - (sizeof(SOCKADDR_IN) + 16) * 2);

    //�������ݰ�
    DataPacket::parse(pConnClient->m_inBuf);

    //ΪͶ��Send���󴴽��µ�IoContext
    IoContext* pSendIoCtx = pConnClient->createIoContext(PostType::SEND_EVENT);

    //Ͷ��send����
    postSend(pSendIoCtx);

    return true;
}

bool IocpServer::handleSend(ClientContext* pConnClient, IoContext* pIoCtx)
{
    return false;
}
