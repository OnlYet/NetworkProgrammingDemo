#ifndef __IO_CONTEXT_H__
#define __IO_CONTEXT_H__

#include "Global.h"
#include "Buffer.h"

struct IoContext
{
    IoContext(PostType type, SOCKET s = INVALID_SOCKET);
    ~IoContext();

    static IoContext* newIoContext(PostType type, SOCKET s = INVALID_SOCKET);

    void newBuffer();

    void resetBuffer();

    OVERLAPPED      m_overlapped;			//ÿһ���ص�io������Ҫ��һ��OVERLAPPED�ṹ
    PostType        m_postType;
    SOCKET			m_socket;				//��ǰ����IO������socket
    DWORD           m_dwBytesTransferred;   //����io������ֽ���
    WSABUF          m_wsaBuf;				//�ص�io��Ҫ��buf
    //char            m_ioBuf[IO_BUF_SIZE];
    //Buffer*         m_buf;
};


#endif // !__IO_CONTEXT_H__
