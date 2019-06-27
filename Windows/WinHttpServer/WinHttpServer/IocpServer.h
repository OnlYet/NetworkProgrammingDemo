#ifndef __IOCP_SERVER_H__
#define __IOCP_SERVER_H__

#include <WinSock2.h>
#include <list>
#include <vector>

constexpr int IO_BUF_SIZE = 8192;

struct IoContext
{
    OVERLAPPED      m_overlapped;			//ÿһ���ص�io������Ҫ��һ��OVERLAPPED�ṹ
    WSABUF          m_wsaBuf;				//�ص�io��Ҫ��buf
	BYTE            m_ioBuf[IO_BUF_SIZE];
    int             m_opType;
	SOCKET			m_socket;				//postAccept���ٲ���һ��socket�����������ӵĿͻ���

	IoContext()
	{
		SecureZeroMemory(this, sizeof(IoContext));
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

	IoContext* createIoContext()
	{
		IoContext* ioCtx = new IoContext();
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
	static unsigned WINAPI IocpWorkerThread(void* arg);

	bool createListenClient(short listenPort);
	bool createIocpWorker();

	bool postAccept(IoContext* pIoCtx);

private:
	short						m_listenPort;
	HANDLE						m_comPort;			//��ɶ˿�
	ClientContext*				m_pListenClient;
	std::list<ClientContext*>	m_ConnClients;

	void*						m_lpfnAcceptEx;		//acceptEx����ָ��

	int							m_nWorkerCnt;
};











#endif // !__IOCP_SERVER_H__

