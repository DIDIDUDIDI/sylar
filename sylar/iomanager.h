#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"

namespace sylar{
    class IOManager : public Scheduler {
    public:
        typedef std::shared_ptr<IOManager> ptr;
        typedef RWMutex RWmutexType;

        enum Event {
            NONE  = 0x0,
            READ  = 0x1,    //epollin
            WRITE = 0x4,    //epollout
        };
        
    private:
        // 事件数据结构
        struct FdContext {
            typedef Mutex MutexType;
            // 事件内容
            struct EventContent {
                Scheduler* scheduler = nullptr;  // 执行事件的scheduler
                Fiber::ptr fiber;                // 执行事件的fiber
                std::function<void()> cb;        // 执行事件的回调函数

            };

            EventContent& getcontext(Event event);
            void resetContext(EventContent& ctx);
            void triggerEvent(Event event);
            EventContent read;                   // 读事件
            EventContent write;                  // 写事件
            int fd = 0;                          // 事件关联的句柄
            Event m_event = NONE;                // 已注册的事件
            MutexType m_mutex;
        };

    public:
        IOManager(size_t threads = 1, bool use_caller = true, const std::string name = ""); 
        ~IOManager();

        //0 success, -1 fail
        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
        bool delEvent(int fd, Event event);
        bool cancelEvent(int fd, Event event);

        bool cancelAll(int fd);

        static IOManager* GetThis();
    
    protected:
        void tickle() override;
        bool stopping() override;
        void idle() override;

        void contextResize(size_t size);
    private:
        int m_epfd = 0;                // e_poll        epoll事件实例
        int m_tickleFds[2];            // pipeline      [0]:输入端 [1]输出端

        std::atomic<size_t> m_pendingEventCount = {0};
        RWmutexType m_mutex;
        std::vector<FdContext*> m_fdContexts;       // 存放事件的vector
    };
}

#endif