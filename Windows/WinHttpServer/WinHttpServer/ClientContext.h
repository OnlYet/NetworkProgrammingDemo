#ifndef __CLIENT_CONTEXT_H__
#define __CLIENT_CONTEXT_H__

#include "Global.h"
#include "Buffer.h"
#include "Codec.h"
#include <string>
#include <map>
#include <algorithm>

struct IoContext;

//һ��socket�ж���ص�����
struct ClientContext
{
    ClientContext(const SOCKET& socket = INVALID_SOCKET);
    ~ClientContext();

    IoContext* getIoContext(PostType type);
    void removeIoContext(IoContext* pIoCtx);

    void appendToBuffer(const char* inBuf, size_t len);
    void appendToBuffer(const std::string& inBuf);

    bool decodePacket();

    SOCKET                              m_socket;
    SOCKADDR_IN                         m_addr;

    //std::string                         m_inBuf;        //���ݰ����뻺���������̰߳�ȫ�����Լ���
    //std::string                         m_outBuf;
    Buffer                              m_inBuf;
    HttpCodec*                          m_pCodec;
    CRITICAL_SECTION                    m_csInBuf;
    std::map<PostType, IoContext*>      m_ioCtxs;

    //CONDITION_VARIABLE      m_cvInBuf;
    //Buffer                  m_inBuf;
};


#endif // !__CLIENT_CONTEXT_H__

