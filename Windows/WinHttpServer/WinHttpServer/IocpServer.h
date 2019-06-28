#ifndef __IOCP_SERVER_H__
#define __IOCP_SERVER_H__

#include <WinSock2.h>
#include <list>
#include <vector>

constexpr int IO_BUF_SIZE = 8192;

enum PostType
{
    ACCEPT_EVENT,
    RECV_EVENT,
    SEND_EVENT
};

struct IoContext
{
    OVERLAPPED      m_overlapped;			//ÿһ���ص�io������Ҫ��һ��OVERLAPPED�ṹ
    WSABUF          m_wsaBuf;				//�ص�io��Ҫ��buf
	char            m_ioBuf[IO_BUF_SIZE];
    PostType        m_postType;
	SOCKET			m_socket;				//��ǰ����IO������socket��postAccept���ٲ���һ��socket�����������ӵĿͻ���

	IoContext(PostType type) :
       m_postType(type)
    {
        SecureZeroMemory(this, sizeof(IoContext));
        m_wsaBuf.buf = m_ioBuf;
        m_wsaBuf.len = IO_BUF_SIZE;
    }
};

//һ��socket�ж���ص�����
struct ClientContext
{
    SOCKET					m_socket;
    SOCKADDR_IN				m_addr;

	string					m_inBuf;
	string					m_outBuf;

	std::list<IoContext*>	m_ioCtxs;

	IoContext* createIoContext(PostType type)
	{
		IoContext* ioCtx = new IoContext(type);
		m_ioCtxs.emplace_back(ioCtx);
		return ioCtx;
	}

	void removeIoContext(IoContext* pIoCtx)
	{
		m_ioCtxs.remove(pIoCtx);
	}

};



class IocpServer
{
public:

    IocpServer(short listenPort);
    IocpServer(const IocpServer&) = delete;
    IocpServer& operator=(const IocpServer&) = delete;
    ~IocpServer();

    bool init();
    bool start();
    bool stop();



protected:
	//����Ҫstatic _beginthreadex���ܷ���
	static unsigned WINAPI IocpWorkerThread(LPVOID arg);

	bool createListenClient(short listenPort);
	bool createIocpWorker();

	bool postAccept(IoContext* pIoCtx);
    bool postRecv(IoContext* pIoCtx);
    bool postSend(IoContext* pIoCtx);

    bool handleAccept(ClientContext* pListenClient, IoContext* pIoCtx);
    bool handleRecv(ClientContext* pConnClient, IoContext* pIoCtx);
    bool handleSend(ClientContext* pConnClient, IoContext* pIoCtx);


private:
	short						m_listenPort;
	HANDLE						m_comPort;			//��ɶ˿�
	ClientContext*				m_pListenClient;
	std::list<ClientContext*>	m_ConnClients;

	void*						m_lpfnAcceptEx;		//acceptEx����ָ��
    void*                       m_lpfnGetAcceptExAddr;//GetAcceptExSockaddrs����ָ��

	int							m_nWorkerCnt;
};











#endif // !__IOCP_SERVER_H__

