#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "Slice.h"
#include <string>

class Buffer
{
public:
    Buffer();
    virtual ~Buffer();

    operator Slice();

    void clear();
    //�൱��consume����consume��ͬ��removeÿ�ζ�Ҫ�����ڴ�
    UINT remove(UINT nSize);
    UINT read(PBYTE pData, UINT nSize);
    BOOL write(PBYTE pData, UINT nSize);
    BOOL write(PCHAR pData, UINT nSize);
    BOOL write(const std::string& s);
    int scan(PBYTE pScan, UINT nPos);
    BOOL insert(PBYTE pData, UINT nSize);
    BOOL insert(const std::string& s);
    void copy(Buffer& buf);
    PBYTE getBuffer(UINT nPos = 0);
    void writeFile(const std::string& fileName);

    //���ݴ�С
    UINT getBufferLen();

protected:
    UINT reallocateBuffer(UINT nSize);
    UINT deallocateBuffer(UINT nSize);
    //ռ���ڴ��С
    UINT getMemSize();

protected:
    PBYTE       m_pBegin;   //������ͷ��λ�ã��̶����ƶ�
    PBYTE       m_pEnd;     //������β��λ��
    UINT        m_nSize;    //Bufferռ���ڴ��С
};

#endif // !__BUFFER_H__


