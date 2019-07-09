//#ifndef __BUFFER_H__
//#define __BUFFER_H__
//
//#include "slice.h"
//#include <string>
//
////std::copy : _DEPRECATE_UNCHECKED����Preprocessor���_SCL_SECURE_NO_WARNINGS
//
//class Buffer 
//{
//public:
//    Buffer();
//    ~Buffer();
//    Buffer(const Buffer& b);
//    Buffer& operator=(const Buffer& b);
//
//    void clear();
//    size_t size() const;
//    bool empty() const;
//    char* data() const;
//    char* begin() const;
//    char* end() const;
//    char* makeRoom(size_t len);
//    void makeRoom();
//    size_t space() const;	//������ÿռ�
//    void addSize(size_t len);
//    char* allocRoom(size_t len);
//
//    Buffer& append(const char* p, size_t len);
//    Buffer& append(std::string s);
//    Buffer& append(Slice slice);
//    Buffer& append(const char* p);
//    template <class T>
//    Buffer& appendValue(const T& v) 
//    {
//        append((const char *)&v, sizeof v);
//        return *this;
//    }
//
//    Buffer& consume(size_t len);
//    Buffer& absorb(Buffer& buf);
//    void setSuggestSize(size_t sz);
//
//    //ת��ΪSlice
//    operator Slice() { return Slice(data(), size()); }
//
//private:
//    //��δ�������ƶ���ͷ��
//    void moveHead();
//    void expand(size_t len);	//����buffer���ȣ�������vector
//    void copyFrom(const Buffer& b);
//
//private:
//    char* buf_;
//    size_t b_;      //δ����ʼλ��
//    size_t e_;      //δ������λ��
//    size_t cap_;    //��ǰ����������
//    size_t exp_;
//};
//
//#endif // !__BUFFER_H__
//
//
//// 0 <= b <= e <= cap
