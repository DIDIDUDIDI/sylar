#include "iomanager.h"
#include "macro.h"
#include "log.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    IOManager::FdContext::EventContent& IOManager::FdContext::getcontext(IOManager::Event event) {
        switch(event) {
            case IOManager::READ:
                return read;
            case IOManager::WRITE:
                return write;
            default:
                SYLAR_ASSERT2(false, "getContext");
        }
    }
    
    void IOManager::FdContext::resetContext(EventContent& ctx) {
        ctx.scheduler = nullptr;
        ctx.fiber.reset();
        ctx.cb = nullptr;
    }

    void IOManager::FdContext::triggerEvent(IOManager::Event event) {
        SYLAR_ASSERT(m_event & event);
        m_event = (Event) (m_event & ~event);
        EventContent& ctx = getcontext(event);
        if(ctx.cb) {
            ctx.scheduler -> schedule(&ctx.cb);
        } else {
            ctx.scheduler -> schedule(&ctx.fiber);
        }
        ctx.scheduler = nullptr;
        // return;
    }
    

    IOManager::IOManager(size_t threads, bool use_caller, const std::string name) 
        : Scheduler(threads, use_caller, name) {
        /*
        epoll_event 和 m_epfd 之间的关系涉及到 epoll 的工作机制。让我解释一下：

        m_epfd (epoll 文件描述符):

        m_epfd 是通过 epoll_create 创建的 epoll 实例的文件描述符。这个文件描述符是整个 epoll 实例的标识符，通过它进行对 epoll 实例的操作，例如添加或删除文件描述符，等待事件发生等。
        epoll_event:

        epoll_event 是一个结构体，用于描述发生在文件描述符上的事件。在这里，通过创建 epoll_event 结构体并设置相应的参数，你告诉 epoll 实例你对文件描述符感兴趣的事件类型以及关联的数据。
        epoll_event 并不是用来标识 epoll 实例的，而是用来描述文件描述符上的事件的。
        关于 Linux 文件系统的概念：

        在 Linux 中，“一切皆文件”是一个通用的概念，包括设备、套接字、管道等都可以通过文件描述符来访问。这里的文件描述符实际上是一个索引，通过它可以访问相应的资源。
        关于 epoll_event 的返回值：

        epoll_event 结构体中的 data 成员可以用来存储用户数据。在这里，data.fd 存储了文件描述符（例如 m_tickleFds[0]），而不是 epoll 实例的文件描述符 (m_epfd)。
        epoll_wait 等调用返回时，会告诉你哪些文件描述符上发生了事件，你可以通过遍历返回的 epoll_event 数组来获取发生事件的文件描述符。
        总结起来，m_epfd 是整个 epoll 实例的标识符，用于对整个 epoll 实例进行操作，而 epoll_event 结构体用于描述特定文件描述符上的事件。这两者是不同的概念，用途也不同。在事件发生时，你会从 epoll_wait 返回的 epoll_event 中得知哪些文件描述符上发生了事件，然后可以通过这些文件描述符进行相应的操作。
        */
        m_epfd = epoll_create(5000);            // 创建一个epoll实例，参数原本是监听这个监控实例最大有多大，但是在linux2.6.8之后被忽略，只要大于0即可，返回一个int为epoll的句柄
        SYLAR_ASSERT(m_epfd > 0);

        int rt = pipe(m_tickleFds);             // 出创建管道，其中m_tickleFds[0] 是读取端， m_tickleFds[1]是写入端
        SYLAR_ASSERT(!rt);

        epoll_event event;                       // epoll_event数据结构
        memset(&event, 0, sizeof(epoll_event));
        event.events = EPOLLIN | EPOLLET;        // 可读事件 + 边缘触发（边缘触发的效率高，但是要尽可能的使得缓冲区大，要不然可能数据不全，对应的是水平触发，效率低但是更安全, epoll_event.events是一个uint32_t的掩码
        event.data.fd = m_tickleFds[0];          // 这一行将事件的文件描述符设置为管道的读取端，表示关注读取端的事件。

        rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);        // 设置成异步, 非阻塞模式
        SYLAR_ASSERT(!rt);

        rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);  // 这一行将管道的读取端添加到之前创建的 epoll 实例中，并关注读事件。
        SYLAR_ASSERT(!rt);

        //m_fdContexts.resize(64);        //初始化到64
        contextResize(64);

        start();                        // scheduler 的start方法
    }
    IOManager::~IOManager() {
        stop();
        close(m_epfd);
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);

        for(size_t i = 0; i < m_fdContexts.size(); ++i) {
            if(m_fdContexts[i]) {
                delete m_fdContexts[i];
            }
        }
    }

    void IOManager::contextResize(size_t size) {
        m_fdContexts.resize(size);
        for(int i = 0; i < (int)(m_fdContexts.size()); ++ i) {
            if(! m_fdContexts[i]) {
                m_fdContexts[i] = new FdContext;
                m_fdContexts[i]->fd = i;
            }
        }
    }

    int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
        FdContext* fd_ctx = nullptr;        // 为什么是一个指针？因为epoll_event.data只能存一个指针或者int fd
        RWmutexType::ReadLock lock(m_mutex);
        if((int)(m_fdContexts.size()) > fd) {      // 如果可以放在队列中
            fd_ctx = m_fdContexts[fd];     
            lock.unlock();
        } else {
            lock.unlock();
            RWmutexType::WriteLock lock2(m_mutex);
            contextResize(fd * 1.5);
            fd_ctx = m_fdContexts[fd];
        }

        FdContext::MutexType::Lock lock2(fd_ctx -> m_mutex);
        if(fd_ctx->m_event & event) {               // 该句柄下的该事件已经注册了，不能重复注册
            SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd = " << fd 
                                      << " event = " << event
                                      << " fd_ctx.event = " << fd_ctx -> m_event;
            SYLAR_ASSERT(! (fd_ctx->m_event & event));
        }

        int op = fd_ctx -> m_event ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;     // 修改or添加
        epoll_event epevent;
        epevent.events = EPOLLET | fd_ctx -> m_event | event;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);  // 注册事件
        if(rt) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                      << op << ", " << fd << ", " << epevent.events << "): "
                                      << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return -1;
        }

        ++m_pendingEventCount;
        fd_ctx -> m_event = (Event)(fd_ctx->m_event | event);      // 把m_event修改成event
        FdContext::EventContent& event_ctx = fd_ctx -> getcontext(event);
        SYLAR_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb); // 初始化成功与否

        event_ctx.scheduler = Scheduler::GetThis();
        if(cb) {
            event_ctx.cb.swap(cb);
        } else {
            event_ctx.fiber = Fiber::GetThis();
            SYLAR_ASSERT(event_ctx.fiber -> getState() == Fiber::EXEC);
        }
        return 0;
    }
    bool IOManager::delEvent(int fd, Event event) {
        RWmutexType::ReadLock lock(m_mutex);
        if((int)(m_fdContexts.size()) <= fd) {
            return false;
        }
        FdContext* fd_ctx = m_fdContexts[fd];
        lock.unlock();
        
        FdContext::MutexType::Lock lock2(fd_ctx -> m_mutex);
        if(!(fd_ctx -> m_event & event)) {          // 如果不存在该事件
            return false;
        }

        Event new_events = (Event) (fd_ctx -> m_event & ~event); // 新的事件是老的事件去掉了需要删除的事件
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;     // 修改or删除
        // 修改epoll实例，把修改的注册上去
        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if(rt) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                      << op << ", " << fd << ", " << epevent.events << "): "
                                      << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        --m_pendingEventCount;
        // 把FdContext修改
        fd_ctx -> m_event = new_events;
        FdContext::EventContent& event_ctx = fd_ctx->getcontext(event);
        fd_ctx->resetContext(event_ctx);
        return true;
    }

    /*
        del和cal是有区别的
        del是直接删除掉
        cal是在找到事件后直接强制执行，而不是等到触发条件后在执行（不触发了）
    */
    bool IOManager::cancelEvent(int fd, Event event) {
        // 找到fd ctx
        RWmutexType::ReadLock lock(m_mutex);
        if((int)(m_fdContexts.size()) <= fd) {
            return false;
        }
        FdContext* fd_ctx = m_fdContexts[fd];
        lock.unlock();
        
        FdContext::MutexType::Lock lock2(fd_ctx -> m_mutex);
        if(!(fd_ctx -> m_event & event)) {          // 如果不存在该事件
            return false;
        }

        // 根据新的事件调整epoll_wait的方式
        Event new_events = (Event) (fd_ctx -> m_event & ~event); // 新的事件是老的事件去掉了需要删除的事件
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;     // 修改or删除
        // 修改epoll实例，把修改的注册上去
        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if(rt) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                      << op << ", " << fd << ", " << epevent.events << "): "
                                      << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        // FdContext::EventContent& event_ctx = fd_ctx->getcontext(event);
        fd_ctx -> triggerEvent(event);
        --m_pendingEventCount;
        return true;
    }

    bool IOManager::cancelAll(int fd) {
        // 找到fd ctx
        RWmutexType::ReadLock lock(m_mutex);
        if((int)(m_fdContexts.size()) <= fd) {
            return false;
        }
        FdContext* fd_ctx = m_fdContexts[fd];
        lock.unlock();
        
        FdContext::MutexType::Lock lock2(fd_ctx -> m_mutex);
        if(!(fd_ctx -> m_event)) {          // 如果不存在该事件
            return false;
        }

        // 根据新的事件调整epoll_wait的方式
        int op = EPOLL_CTL_DEL;     // 修改or删除
        // 修改epoll实例，把修改的注册上去
        epoll_event epevent;
        epevent.events = 0;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if(rt) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                      << op << ", " << fd << ", " << epevent.events << "): "
                                      << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        if(fd_ctx->m_event & READ) {
            // FdContext::EventContent& event_ctx = fd_ctx->getcontext(READ);
            fd_ctx->triggerEvent(READ);
            --m_pendingEventCount;
        }
        if(fd_ctx->m_event & WRITE) {
            // FdContext::EventContent& event_ctx = fd_ctx->getcontext(WRITE);
            fd_ctx->triggerEvent(WRITE);
            --m_pendingEventCount;
        }
        SYLAR_ASSERT(fd_ctx -> m_event == 0);
        return true;
    }

    IOManager* IOManager::GetThis() {
        return dynamic_cast<IOManager*>(Scheduler::GetThis());
    }

    void IOManager::tickle() {
        if(hasIdleThreads()) {
            return;
        }
        int rt = write(m_tickleFds[1], "T", 1);                         // 写入一个字符T，然后就可以idle检测到唤醒
        SYLAR_ASSERT(rt == 1);
    }

    bool IOManager::stopping() {
        return Scheduler::stopping() && m_pendingEventCount == 0;
    }

    void IOManager::idle() {
        epoll_event* events = new epoll_event[64]();
        std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr) {
            delete[] ptr;
        });

        while(true) {
            if(stopping()) {
                SYLAR_LOG_INFO(g_logger) << "name = " << getName() << " idle stopping exit";
                break; 
            }

            int rt = 0;
            do {
                static const int MAX_TIMEOUT = 5000;                   // 超时时间
                rt = epoll_wait(m_epfd, events, 64, MAX_TIMEOUT);      // 返回的监听事件，有触发会返回，如果超过timeout，也会唤醒

                if(rt < 0 && errno == EINTR) {
                    // 重新循环
                } else {
                    break;
                }
            } while (true);
            
            for(int i = 0; i < rt; ++ i) {                              // 遍历返回的监听事件
                epoll_event& event = events[i];
                if(event.data.fd == m_tickleFds[0]) {                   // 如果事件是否是用来唤醒的epollwait的事件，m_tickleFds[0]是我们的输入端
                    uint8_t dummy;                                      // 如果是去处理下数据   
                    while(read(m_tickleFds[0], &dummy, 1) == 1);        // ET 类型，边缘触发，不读干净就没了，所以要一直读，直到没有数据
                    continue;
                }

                FdContext* fd_ctx = (FdContext*)(event.data.ptr);
                FdContext::MutexType:: Lock lock(fd_ctx -> m_mutex);
                if(event.events & (EPOLLERR | EPOLLHUP)) {              // 错误或者中断
                    event.events |= EPOLLIN | EPOLLOUT;                 // 唤醒读写事件
                }
                int real_events = NONE;
                if(event.events & EPOLLIN) {                            // 如果有读事件，那么记录上
                    real_events |= READ;
                }
                if(event.events & EPOLLOUT) {
                    real_events |= WRITE;
                }

                if((fd_ctx -> m_event & real_events) == NONE) {         // 如果没有事件
                    continue;
                }

                int left_events = (fd_ctx -> m_event & ~real_events);   // 剩余事件，当前上下文上的事件与非之前的real_events
                int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;   // 非0， 修改 要不然del
                event.events = EPOLLET | left_events;                   // 复用，改成剩余的事件

                int rt2 = epoll_ctl(m_epfd, op, fd_ctx -> fd, &event);  // 剩余事件注册
                if(rt2) {
                    SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                      << op << ", " << fd_ctx -> fd << ", " << event.events << "): "
                                      << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                    continue;
                }

                if(real_events & READ) {                                // 读事件触发
                    fd_ctx -> triggerEvent(READ);
                    --m_pendingEventCount;
                }
                if(real_events & WRITE) {                               // 写事件触发
                    fd_ctx -> triggerEvent(WRITE);
                    --m_pendingEventCount;
                }
            }

            // 处理完idle之后让出运行时间，返回到协程调度器那里 YieldToHold() better? 
            Fiber::ptr cur = Fiber::GetThis();
            auto raw_ptr = cur.get();
            cur.reset();

            raw_ptr -> swapOut();
        }
    }
}