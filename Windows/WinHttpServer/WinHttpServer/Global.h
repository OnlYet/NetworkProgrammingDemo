#ifndef __GLOBAL_H__
#define __GLOBAL_H__


//���ó�4K�ı���
constexpr int IO_BUF_SIZE = 8192;
constexpr int ACCEPT_ADDRS_SIZE = (sizeof(SOCKADDR_IN) + 16) * 2;

enum PostType
{
    ACCEPT_EVENT,
    RECV_EVENT,
    SEND_EVENT,
    PARSE_EVNET,    //���ݰ�����
    CLOSE_EVENT,
};

#endif // !__GLOBAL_H__
