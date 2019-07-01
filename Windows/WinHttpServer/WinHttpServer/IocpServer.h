#ifndef __IOCP_SERVER_H__
#define __IOCP_SERVER_H__

#include <list>

struct IoContext;
struct ClientContext;

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
    bool postParse(ClientContext* pConnClient, IoContext* pIoCtx);

    bool handleAccept(ClientContext* pListenClient, IoContext* pIoCtx);
    bool handleRecv(ClientContext* pConnClient, IoContext* pIoCtx);
    bool handleSend(ClientContext* pConnClient, IoContext* pIoCtx);
    bool handleParse(ClientContext* pConnClient, IoContext* pIoCtx);

    void addClient(ClientContext* pConnClient);

private:
	short						m_listenPort;
	HANDLE						m_comPort;              //��ɶ˿�
	ClientContext*				m_pListenClient;
	std::list<ClientContext*>	m_connList;             //�����ӿͻ����б�
    CRITICAL_SECTION            m_csConnList;           //���������б�

	void*						m_lpfnAcceptEx;		    //acceptEx����ָ��
    void*                       m_lpfnGetAcceptExAddr;  //GetAcceptExSockaddrs����ָ��

	int							m_nWorkerCnt;           //io�����߳�����
};











#endif // !__IOCP_SERVER_H__

