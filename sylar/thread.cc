#include "thread.h"
#include "log.h"
#include "util.h"

namespace sylar {
    /*
        thread_local: C++ 11 新的关键字，表示该变量和该线程周期一致
    */
    // 静态初始化
    static thread_local Thread* t_thread = nullptr;   
    static thread_local std::string t_thread_name = "UNKONW";

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    Semaphore::Semaphore(uint32_t count) {
        if(sem_init(&m_semaphore, 0, count)) {
            throw std::logic_error("sem_init error");
        }
    }

    Semaphore::~Semaphore() {
        sem_destroy(&m_semaphore);
    }

    void Semaphore::wait() {
        if(sem_wait(&m_semaphore)) {
            throw std::logic_error("sem_wait error");
        }
    }

    void Semaphore::notify() {
        if(sem_post(&m_semaphore)) {
            throw std::logic_error("sem_post error");
        }
    }

    Thread* Thread::GetThis() {
        return t_thread;
    }

    const std::string& Thread::GetName() {
        return t_thread_name;
    }

    void Thread::SetName(const std::string& name) {
        if(t_thread) {
            t_thread -> m_name = name;
        }
        t_thread_name = name;
    }

    Thread::Thread(std::function<void()> cb, const std::string& name) :
        m_cb(cb), m_name(name) {
        if(name.empty()) {
            m_name = "UNKNOW";
        }
        int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
        if(rt) {
            SYLAR_LOG_ERROR(g_logger) << "pthread_create failed, rt = " << rt 
                                      << ", name = " << m_name << std::endl;
            throw std::logic_error("pthread_create error");
        }
        // 因为我们的run是一个静态函数，为了保证线程运行起来才出构造函数，我们这里设置一个Semaphore，然后在run函数里notify，这样可以run完成后这里才可以pass
        m_semaphore.wait();
    }

    Thread::~Thread() {
        if(m_thread) {
            pthread_detach(m_thread);
        }
    }

    void Thread::join() {
        if(m_thread) {
            int rt = pthread_join(m_thread, nullptr);
            if(rt) {
                SYLAR_LOG_ERROR(g_logger) << "pthread_join failed, rt = " << rt 
                                        << ", name = " << m_name << std::endl;
                throw std::logic_error("pthread_join error");
            }
            m_thread = 0;
        }
    }

    /*
        Run() 方法，我们线程库真正启动的方法
        通过传入一个Thread*来起动，为什么是void* arg， 因为这就是标准库的写法...
        使用void* 的好处也是任意类型赋值

    */
    void* Thread::run(void* arg) {
        Thread* thread = (Thread*)arg;
        t_thread = thread;
        t_thread_name = t_thread -> m_name;
        thread->m_id = sylar::GetThreadID(); // 使用的是gettid，返回的是linux的线程ID
        /*
            pthread_self 是posix描述的线程ID（并非内核真正的线程id），相对于进程中各个线程之间的标识号，对于这个进程内是唯一的，而不同进程中，每个线程的 pthread_self() 可能返回是一样的。
            而 gettid 获取的才是内核中线程ID
            posix是POSIX的线程标准，定义了创建和操纵线程的一套API，也就是我们现在用的API
        */
        pthread_setname_np(pthread_self(), thread -> m_name.substr(0, 15).c_str()); //pthread_setname_np 最大只能接收16个字符

        /*  
            swap 掉，防止智能指针不被释放。
        */
        std::function<void()> cb;
        cb.swap(thread -> m_cb);

        //直到这边搞定才继续通知构造函数继续
        thread -> m_semaphore.notify();

        //执行回调方法
        cb();
        return 0;
    }
}