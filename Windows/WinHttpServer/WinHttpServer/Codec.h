#ifndef __CODEC_H__
#define __CODEC_H__

#include "Slice.h"
#include "Buffer.h"

struct CodecBase 
{
    // > 0 ������������Ϣ����Ϣ����msg�У�������ɨ����ֽ���
    // == 0 ����������Ϣ
    // < 0 ��������
    //virtual int tryDecode(Slice data, Slice &msg) = 0;
    //virtual void encode(Slice msg, Buffer &buf) = 0;
    //virtual CodecBase *clone() = 0;
};

////��\r\n��β����Ϣ
//struct LineCodec : public CodecBase 
//{
//    int tryDecode(Slice data, Slice &msg) override;
//    void encode(Slice msg, Buffer &buf) override;
//    CodecBase *clone() override { return new LineCodec(); }
//};
//
////�������ȵ���Ϣ
//struct LengthCodec : public CodecBase 
//{
//    int tryDecode(Slice data, Slice &msg) override;
//    void encode(Slice msg, Buffer &buf) override;
//    CodecBase *clone() override { return new LengthCodec(); }
//};


struct HttpCodec : public CodecBase
{
    enum HttpState
    {
        HTTP_OK,
        HTTP_HEADER_INCORRECT,
        HeaderIncorrect,

    };

    HttpState getHeader(Slice data, Slice& header);
    HttpState decodeHeader(Slice header);
    bool decodeLine();

private:
    HttpState           m_state;
};

#endif // !__CODEC_H__
