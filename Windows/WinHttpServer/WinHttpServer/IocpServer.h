#ifndef __IOCP_SERVER_H__
#define __IOCP_SERVER_H__

#include <list>
#include <vector>
#include "Global.h"

struct ListenContext;
struct IoContext;
struct ClientContext;

class IocpServer
{
public:

    IocpServer(short listenPort, int maxConnectionCount = 10000);
    IocpServer(const IocpServer&) = delete;
    IocpServer& operator=(const IocpServer&) = delete;
    ~IocpServer();

    bool start();
    bool stop();
    bool shutdown();
    //bool init();
    //bool unInit();

protected:
    //����Ҫstatic _beginthreadex���ܷ���
    static unsigned WINAPI IocpWorkerThread(LPVOID arg);

    HANDLE associateWithCompletionPort(SOCKET s, ULONG_PTR completionKey);
    bool getAcceptExPtr();
    bool getAcceptExSockaddrs();
    bool setKeepAlive(SOCKET s, IoContext* pIoCtx, int time = 30, int interval = 10);

    bool createListenClient(short listenPort);
    bool createIocpWorker();
    bool exitIocpWorker();
    bool initAcceptIoContext();

    bool postAccept(IoContext* pIoCtx);
    PostResult postRecv(ClientContext* pConnClient);
    PostResult postSend(ClientContext* pConnClient);
    bool postParse(ClientContext* pConnClient, IoContext* pIoCtx);

    bool handleAccept(ClientContext* pListenClient, IoContext* pIoCtx);
    bool handleRecv(ClientContext* pConnClient, IoContext* pIoCtx);
    bool handleSend(ClientContext* pConnClient, IoContext* pIoCtx);
    bool handleParse(ClientContext* pConnClient, IoContext* pIoCtx);
    bool handleClose(ClientContext* pConnClient, IoContext* pIoCtx);

    //�̰߳�ȫ
    void addClientContext(ClientContext* pConnClient);
    void removeClientContext(ClientContext* pConnClient);
    void removeAllClientContext();
    void CloseClient(ClientContext* pConnClient);

    bool decodePacket();

    void echo(ClientContext* pConnClient);


private:
    bool                        m_bIsShutdown;          //�ر�ʱ���˳������߳�

    short                       m_listenPort;
    HANDLE                      m_hComPort;              //��ɶ˿�
    HANDLE                      m_hExitEvent;           //�˳��߳��¼�

    void*                       m_lpfnAcceptEx;		    //acceptEx����ָ��
    void*                       m_lpfnGetAcceptExAddr;  //GetAcceptExSockaddrs����ָ��

    int                         m_nWorkerCnt;           //io�����߳�����
    DWORD                       m_nConnClientCnt;       //�����ӿͻ�������
    DWORD                       m_nMaxConnClientCnt;    //���ͻ�������

    ListenContext*              m_pListenCtx;            //����������
    std::list<ClientContext*>   m_connList;             //�����ӿͻ����б�
    CRITICAL_SECTION            m_csConnList;           //���������б�

    std::vector<HANDLE>         m_hWorkerThreads;       //�����߳̾���б�
    std::vector<IoContext*>     m_acceptIoCtxList;      //�������ӵ�IO�������б�

};

#endif // !__IOCP_SERVER_H__

