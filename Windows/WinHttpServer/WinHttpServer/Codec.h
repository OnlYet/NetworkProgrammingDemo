#ifndef __CODEC_H__
#define __CODEC_H__

#include "Slice.h"
//#include "Buffer.h"
#include <string>
#include <unordered_map>

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
        HTTP_BAD_REQUEST,
        Http_Header_Incomplete,
        Http_Body_Incomplete,

        Http_Invalid_Request_Line,  //400 (Bad Request) error or a 301 (Moved Permanently)
        
    };

    void tryDecode(Slice msg);

    bool getHeader(Slice data, Slice& header);

    bool getLine(Slice data, Slice& line);

    HttpState decodeStartLine(Slice& line);
    HttpState parseHeader();
    HttpState getBody();
    HttpState decodeBody();

    HttpState handleGet();
    HttpState handlePost();
    HttpState handleUrl();

    bool informUnimplemented();
    bool informUnsupported();

    std::string getHeaderField(const std::string& strKey);

private:
    HttpState                                       m_state;
    std::unordered_map<std::string, std::string>    m_header;
    size_t                                          m_nHeaderLength;    //HTTP��Ϣͷ�����ȣ�headerLen+bodyLen=wholeMsg
};

#include <map>
void test()
{
    string http;

    //�洢������
    string cmdLine = "";
    //�洢��Ϣͷ���ֶκ�ֵ
    map<string, string> kvs;

    size_t pos = 0, posk = 0, posv = 0;
    string k = "", v = "";
    char c;
    //http����������GET����
    while (pos != http.size())
    {
        c = http.at(pos);
        if (c == ':')
        {

            //�������У�����Ϣͷ����δ����
            if (!cmdLine.empty() && k.empty())
            {

                //�洢��Ϣͷ����
                k = http.substr(posk, pos - posk);

                //����ð�źͿո�
                posv = pos + 2;
            }
        }

        //��β
        else if (c == '\r' || c == '\n')
        {

            //��δ��������Ϣͷ�ֶ����ƣ���������Ҳδ������
            if (k.empty() && cmdLine.empty())
            {
                //����Ӧ�������У�����֮
                cmdLine = http.substr(posk, pos - posk);
            }
            else
            {
                //�ѽ�������Ϣͷ�ֶ����ƣ���δ�����ֶ�ֵ
                if (!k.empty() && v.empty())
                {

                    //�洢�ֶ�ֵ
                    v = http.substr(posv, pos - posv);
                }
            }
            posk = pos + 1;
        }

        if (!k.empty() && !v.empty() && !cmdLine.empty())
        {

            //������Ϣͷ�ֶ����ƺ�ֵ
            kvs.insert(make_pair(k, v));
            k = "";
            v = "";
        }
        ++pos;
    }

}

#endif // !__CODEC_H__
