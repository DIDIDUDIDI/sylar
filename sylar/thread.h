#ifndef __SYLAR_THREAD_H__
#define __SYLAR_THREAD_H__

#include <thread>
#include <functional>
#include <memory> 
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>

/*
    实现线程库
*/
namespace sylar {
    
    /*
        信号量类，包装semaphore.h
        线程同步
    */
    class Semaphore {
    public:
        Semaphore(uint32_t count = 0);
        ~Semaphore();

        void wait();
        void notify();
    private:
        Semaphore(const Semaphore&) = delete;
        Semaphore(const Semaphore&&) = delete;
        Semaphore& operator= (const Semaphore&) = delete;
    private:
        sem_t m_semaphore;    // typedef sem_t sem_t -> 实际上是一个长整数
    };
    
    /*
        互斥量 + Locak Guard
        RAII基本概念
        使用的是scope 锁 -> 当构造的时候生成锁，析构的时候释放
        比如我们的ScopedLockImpl<T> 创建出来之后，T会存储在m_mutex里，
        同时构造函数执行的时候会执行T的上锁方法，这样确保T上锁。
        当我们析构ScopedLockImpl<T> 的时候，会触发unlock 方法，这样也会自动触发T的解锁
        这种设计基于生命周期实现自动化，来避免死锁
    */
    
    // lock guard 
    template<class T>
    struct ScopedLockImpl {
    public:
        ScopedLockImpl(T& mutex) 
            : m_mutex(mutex)
        {
            m_mutex.lock();
            m_locked = true;
        }

        ~ScopedLockImpl() {
            unlock();
        }

        void lock() {
            if(! m_locked) {
                m_mutex.lock();
                m_locked = true;
            }
        }

        void unlock() {
            if(m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T& m_mutex;
        bool m_locked;
    };

    template<class T>
    struct ReadScopedLockImpl {
    public:
        ReadScopedLockImpl(T& mutex) 
            : m_mutex(mutex)
        {
            m_mutex.rdlock();
            m_locked = true;
        }

        ~ReadScopedLockImpl() {
            unlock();
        }

        void lock() {
            if(! m_locked) {
                m_mutex.rdlock();
                m_locked = true;
            }
        }

        void unlock() {
            if(m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T& m_mutex;
        bool m_locked;
    };

    template<class T>
    struct WriteScopedLockImpl {
    public:
        WriteScopedLockImpl(T& mutex) 
            : m_mutex(mutex)
        {
            m_mutex.wrlock();
            m_locked = true;
        }

        ~WriteScopedLockImpl() {
            unlock();
        }

        void lock() {
            if(! m_locked) {
                m_mutex.wrlock();
                m_locked = true;
            }
        }

        void unlock() {
            if(m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T& m_mutex;
        bool m_locked;
    };
    
    // 互斥量
    class Mutex {
    public:
        typedef ScopedLockImpl<Mutex> Lock;
        Mutex() {
            pthread_mutex_init(&m_mutex, nullptr);
        }

        ~Mutex() {
            pthread_mutex_destroy(&m_mutex);
        }

        void lock() {
            pthread_mutex_lock(&m_mutex);
        }

        void unlock() {
            pthread_mutex_unlock(&m_mutex);
        }
    private:
        pthread_mutex_t m_mutex;
    };

    //测试用空锁
    class NullMutex {
    public:
        typedef ScopedLockImpl<NullMutex> Lock;
        NullMutex() {}
        ~NullMutex() {}
        void lock() {}
        void unlock() {}
    };

    // 读写互斥量
    class RWMutex {
    public:
        typedef ReadScopedLockImpl<RWMutex> ReadLock;
        typedef WriteScopedLockImpl<RWMutex> WriteLock;
        RWMutex() {
            pthread_rwlock_init(&m_lock, nullptr);
        }

        ~RWMutex() {
            pthread_rwlock_destroy(&m_lock);
        }

        void rdlock() {
            pthread_rwlock_rdlock(&m_lock);
        }

        void wrlock() {
            pthread_rwlock_wrlock(&m_lock);
        }

        void unlock() {
            pthread_rwlock_unlock(&m_lock);
        }

    private:
        pthread_rwlock_t m_lock;
    };

    class NullRWMutex {
    public:
        typedef ReadScopedLockImpl<NullMutex> ReadLock;
        typedef WriteScopedLockImpl<NullMutex> WriteLock;

        NullRWMutex() {}
        ~NullRWMutex() {}
        void rdlock() {}
        void wrlock() {}
        void unlock() {}
    };

    /*
        spinlock
        小时候最熟悉的锁，不说了       
    */
   class SpinLock {
    public:
        typedef ScopedLockImpl<SpinLock> Lock;
        SpinLock() {
            pthread_spin_init(&m_spinlock, 0);
        }

        ~SpinLock() {
            pthread_spin_destroy(&m_spinlock);
        }

        void lock() {
            pthread_spin_lock(&m_spinlock);
        }

        void unlock() {
            pthread_spin_unlock(&m_spinlock);
        }
    private:
        pthread_spinlock_t m_spinlock;
   };

    /*
        CAS 原子锁
        CAS 是保证锁安全的最小原子操作，由硬件/OS实现
        Spinlock就是包装了CAS实现的。
        但是慎用无锁编程！！！
    */
    class CASLock {
    public:
        typedef ScopedLockImpl<CASLock> Lock;
        CASLock() {
            m_CASlock.clear();
        }
        ~CASLock() {
        }

        void lock() {
            while(std::atomic_flag_test_and_set_explicit(&m_CASlock, std::memory_order_acquire));
        }

        void unlock() {
            std::atomic_flag_clear_explicit(&m_CASlock, std::memory_order_release);
        }
    private:
        volatile std::atomic_flag m_CASlock;
    };

    /*  
        线程类，主要是包装Pthread完成
    */
    class Thread {
    public:
        typedef std::shared_ptr<Thread> ptr;
        Thread(std::function<void()> cb, const std::string& name);
        ~Thread();

        pid_t getId() const {return m_id;}
        const std::string& getName() const {return m_name;}

        void join();
        
        // static 方法：
        static Thread* GetThis();                       // 返回当前的线程
        static const std::string& GetName();            // 返回线程的名字
        static void SetName(const std::string& name);   // 静态的setname方法，这样可以得到非自己创建的thread的信息，比如main thread等。
    private:
        // 写线程我们要禁止拷贝，因为拷贝会导致信号量和互斥量失效
        Thread(const Thread&) = delete;             // 禁止拷贝
        Thread(const Thread&&) = delete;            // 禁止右值拷贝
        Thread& operator= (const Thread&) = delete; // 禁止 = 符号拷贝

        static void* run(void* arg);                // 执行方法
    private:
        pid_t m_id = -1;               // pid: typedef int pid 返回OS中的线程ID
        pthread_t m_thread = 0;        // typedef unsign long pthread_t; 返回pthread_id，pthread_id是POSIX的标识符
        std::function<void()> m_cb;    // m_cb 实例，存储一个返回为空，输入为空的方法
        std::string m_name;            // 线程名字

        Semaphore m_semaphore;
    };
}

#endif