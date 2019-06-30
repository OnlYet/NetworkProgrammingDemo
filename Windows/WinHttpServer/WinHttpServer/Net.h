#ifndef __NET_H__
#define __NET_H__

#include <string>
#include <list>

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
        SecureZeroMemory(&m_overlapped, sizeof(OVERLAPPED));
        SecureZeroMemory(m_ioBuf, IO_BUF_SIZE);
        m_wsaBuf.buf = m_ioBuf;
        m_wsaBuf.len = IO_BUF_SIZE;
    }
    //���buffer
    void resetBuffer()
    {
        SecureZeroMemory(&m_overlapped, sizeof(OVERLAPPED));
        SecureZeroMemory(m_ioBuf, IO_BUF_SIZE);
    }
};

//һ��socket�ж���ص�����
struct ClientContext
{
    SOCKET					m_socket;
    SOCKADDR_IN				m_addr;

    std::string				m_inBuf;
    std::string				m_outBuf;

    std::list<IoContext*>	m_ioCtxs;

    ClientContext(const SOCKET& socket = INVALID_SOCKET) :
        m_socket(socket)
    {
    }

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

struct Net
{
    static bool init();
    static bool unInit();
};

#endif // !__NET_H__


