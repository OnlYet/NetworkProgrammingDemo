#ifndef __IO_CONTEXT_H__
#define __IO_CONTEXT_H__

#include "Global.h"

struct IoContext
{
    IoContext(PostType type, SOCKET s = INVALID_SOCKET);
    ~IoContext();

    static IoContext* newIoContext(PostType type, SOCKET s = INVALID_SOCKET);

    void resetBuffer();

    OVERLAPPED      m_overlapped;			//ÿһ���ص�io������Ҫ��һ��OVERLAPPED�ṹ
    WSABUF          m_wsaBuf;				//�ص�io��Ҫ��buf
    char            m_ioBuf[IO_BUF_SIZE];
    PostType        m_postType;
    SOCKET			m_socket;				//��ǰ����IO������socket��postAccept���ٲ���һ��socket�����������ӵĿͻ���
    DWORD           m_dwBytesTransferred;   //����io������ֽ���
};


#endif // !__IO_CONTEXT_H__
