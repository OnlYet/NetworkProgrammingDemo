#ifndef __IOCP_SERVER_H__
#define __IOCP_SERVER_H__

#include <list>
#include <vector>

struct IoContext;
struct ClientContext;

class IocpServer
{
public:

    IocpServer(short listenPort);
    IocpServer(const IocpServer&) = delete;
    IocpServer& operator=(const IocpServer&) = delete;
    ~IocpServer();

    bool start();
    bool stop();
    //bool init();
    //bool unInit();

protected:
    //����Ҫstatic _beginthreadex���ܷ���
    static unsigned WINAPI IocpWorkerThread(LPVOID arg);

    HANDLE associateWithCompletionPort(ClientContext* pClient);
    bool getAcceptExPtr();
    bool getAcceptExSockaddrs();
    bool setKeepAlive(SOCKET s, IoContext* pIoCtx, int time = 30, int interval = 10);

    bool createListenClient(short listenPort);
    bool createIocpWorker();
    bool exitIocpWorker();

    bool postAccept(IoContext* pIoCtx);
    bool postRecv(IoContext* pIoCtx);
    bool postSend(IoContext* pIoCtx);
    bool postParse(ClientContext* pConnClient, IoContext* pIoCtx);

    bool handleAccept(ClientContext* pListenClient, IoContext* pIoCtx);
    bool handleRecv(ClientContext* pConnClient, IoContext* pIoCtx);
    bool handleSend(ClientContext* pConnClient, IoContext* pIoCtx);
    bool handleParse(ClientContext* pConnClient, IoContext* pIoCtx);
    bool handleClose(ClientContext* pConnClient, IoContext* pIoCtx);

    //�̰߳�ȫ
    void addClient(ClientContext* pConnClient);
    void removeClient(ClientContext* pConnClient);
    void removeClients();

    bool decodePacket();

    void echo(ClientContext* pConnClient);


private:
    short                       m_listenPort;
    HANDLE                      m_hComPort;              //��ɶ˿�
    HANDLE                      m_hExitEvent;           //�˳��߳��¼�
    std::vector<HANDLE>         m_hWorkerThreads;       //�����߳̾���б�
    ClientContext*              m_pListenClient;
    std::list<ClientContext*>   m_connList;             //�����ӿͻ����б�
    CRITICAL_SECTION            m_csConnList;           //���������б�

    void*                       m_lpfnAcceptEx;		    //acceptEx����ָ��
    void*                       m_lpfnGetAcceptExAddr;  //GetAcceptExSockaddrs����ָ��

    int                         m_nWorkerCnt;           //io�����߳�����
};

#endif // !__IOCP_SERVER_H__

