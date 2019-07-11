#ifndef __CLIENT_CONTEXT_H__
#define __CLIENT_CONTEXT_H__

#include "Global.h"
#include "Buffer.h"
#include "Codec.h"
#include <string>
#include <map>
#include <algorithm>
#include <queue>

struct ListenContext
{
    ListenContext(short port, const std::string& ip = "0.0.0.0");

    SOCKET          m_socket;   //����socket
    SOCKADDR_IN     m_addr;     //������ַ
};

struct IoContext;
struct RecvIoContext;

struct ClientContext
{
    ClientContext(const SOCKET& socket = INVALID_SOCKET);
    ~ClientContext();

    void appendToBuffer(PBYTE pInBuf, size_t len);
    void appendToBuffer(const std::string& inBuf);

    bool decodePacket();

    SOCKET                              m_socket;       //�ͻ���socket
    SOCKADDR_IN                         m_addr;         //�ͻ��˵�ַ
    RecvIoContext*                      m_recvIoCtx;
    IoContext*                          m_sendIoCtx;
    HttpCodec*                          m_pCodec;
    Buffer                              m_inBuf;
    //Buffer                              m_outBuf;
    std::queue<Buffer>                  m_outBufList;
    CRITICAL_SECTION                    m_csLock;       //����������socket������
};

#endif // !__CLIENT_CONTEXT_H__

