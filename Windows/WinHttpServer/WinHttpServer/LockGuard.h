#ifndef __LOCK_GUARD_H__
#define __LOCK_GUARD_H__

#include <iostream>

//CRITICAL_SECTION�ǿ�����ģ�ֻҪleave��enter�Ĵ���һ�¾���
//std::mutex�ǲ�������ģ�std::recursive_mutex�ǿ������
class LockGuard
{
public:
    LockGuard(LPCRITICAL_SECTION lock)
        : m_lock(lock)
    {
        //std::cout << "try to lock: " << (int)m_lock << std::endl;
        EnterCriticalSection(m_lock);
        //std::cout << "lock: " << (int)m_lock << std::endl;
    }

    ~LockGuard()
    {
        LeaveCriticalSection(m_lock);
        //std::cout << "unlock: " << (int)m_lock << std::endl;
        m_lock = nullptr;
    }

private:
    LPCRITICAL_SECTION    m_lock;
};

#endif // !__LOCK_GUARD_H__
