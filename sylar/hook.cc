#include "hook.h"
#include "fiber.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "log.h"
#include "config.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");



namespace sylar {

    static sylar::ConfigVar<int>::ptr g_tcp_connect_timeout 
        = sylar::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

    static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
        XX(sleep) \
        XX(usleep) \
        XX(nanosleep) \
        XX(socket)  \
        XX(connect) \
        XX(accept)  \
        XX(read) \
        XX(readv) \
        XX(recv) \
        XX(recvfrom) \
        XX(recvmsg) \
        XX(write) \
        XX(writev) \
        XX(send) \
        XX(sendto) \
        XX(sendmsg) \
        XX(close) \
        XX(fcntl) \
        XX(ioctl) \
        XX(getsockopt) \
        XX(setsockopt) 

    void hook_init() {
        static bool is_inited = false;
        if(is_inited) {
            return;
        }

/*  
    dlsym 打开原生动态链接库
    RTLD_NEXT 找到一个签名之后就返回 赋值给我的函数变量
    
    ##是连接前后

    sleep_f = (sleep_fun)dlsym(RTLD_NEXT, "sleep"); 
    usleep_f = (usleep_fun)dlsym(RTLD_NEXT, "usleep");
*/
    #define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
        HOOK_FUN(XX);
    #undef XX
    }

    static uint64_t s_connect_timeout = -1;

    // 利用数据初始化在main函数前初始化
    struct _HookIniter {
        _HookIniter() {
            hook_init();
            s_connect_timeout = g_tcp_connect_timeout->getValue();

            g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value) {
                SYLAR_LOG_INFO(g_logger) << "tcp connect timeout change from " 
                                         << old_value  << " to " << new_value; 
                s_connect_timeout = new_value;
            });
        }
    };

    static _HookIniter s_hook_initer;

    bool is_hook_enable() {
        return t_hook_enable;
    }

    void set_hook_enable(bool flag) {
        t_hook_enable = flag;
    }

}

struct timer_info {
    int cancelled = 0;
};

/*
    传一个我们要hook的函数名， 以及ioevent中的事件， fdmanager 超时类型
    @fd  句柄
    @fun tyepname 原本的方法
    @hook_fun_name hook 函数名
    @event 事件类型（读/写）
    @time_so 超时类型
    @args 参数列表
*/
template<typename OriginFun, typename ... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name, 
                     uint32_t event, int timeout_so, Args&&... args) {
    if(!sylar::t_hook_enable) {                         // 如果没有开启hook
        return fun(fd, std::forward<Args>(args)...);    // 直接使用原本的方法，用forward把对应的参数完美转发过去
    }

    //SYLAR_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << ">";

    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd); // 拿到ctx
    if(!ctx) {
        return fun(fd, std::forward<Args>(args)...);    //如果文件不存在直接返回
    }
    if(ctx -> isClose()) {      // 如果close
        errno = EBADF;          // 设置下error返回
    }
    if(! ctx -> isSocket() || ctx -> getUserNonblock()) {   // 如果不是socket或者用户设置了block，我们也用原本的方法
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);              //取出超时时间
    std::shared_ptr<timer_info> tinfo(new timer_info);      // 设置超时条件

// TODO: change to while flag 
retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);    // 先执行一遍如果返回值有效那就直接返回
    while(n == -1 && errno == EINTR) {                   // 重试
        n = fun(fd, std::forward<Args>(args)...);
    } 
    if(n == -1 && errno == EAGAIN) {                     // 如果是EAGIN，意为重试，用在，一般出现在非阻塞操作的时候，说明在阻塞，注意我们这里的阻塞指的是我们的线程在等IO，要切换成我们的hook的异步方法把协程的执行时间让出来
        //SYLAR_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << ">" << " async";
        sylar::IOManager* iom = sylar::IOManager::GetThis();    
        sylar::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);
        if(to != (uint64_t)-1) {                         // 如果超时时间不是-1
            timer = iom -> addConditionTimer(to, [winfo, fd, iom, event]() {     //添加一个条件超时计时器，根据逻辑会加到TM中，之后会被IOmanager触发此CB，设置的to就是timer::m_ms即执行周期, 注意这是一个CB，不是现在执行的！！！
                auto t = winfo.lock();  // lock()方法返回一个指向所管理资源的shared_ptr对象，并增加引用计数。如果原始的shared_ptr已经被销毁或者过期，则返回一个空的（nullptr）指针，因为我们的winfo是我们传入的条件，这一步就是检查是否满足条件
                if(!t || t->cancelled) {    // 如果已经是过期的或者cancelled了，直接返回
                    return;
                }
                t->cancelled = ETIMEDOUT;   // 要不然我们也设置超时，不用做了，因为已经超时了
                iom -> cancelEvent(fd, (sylar::IOManager::Event)(event)); // 事件取消，因为calEvent会把我们传进去的事件从掩码中取消掉然后重新设置，之后触发去掉该事件的后的fd的所有的行为，比如我们这里设置了一个读事件，那么cal就会把读事件取消掉然后继续执行剩下的事情
            }, winfo);
        }
        
        int rt = iom -> addEvent(fd, (sylar::IOManager::Event)(event)); // 添加一个事件，之后idle会触发, epoll_wait 会监听到有数据进来来触发，cb为空，说明以当前协程为唤醒对象
        if(rt) {
            // 出错就纪录下日志
            SYLAR_LOG_ERROR(g_logger) << hook_fun_name << " addEvent("
                                      << fd << ", " << event << ")";
            if(timer) {
                timer -> cancel();          // 释放所有timer的资源
            }
            return -1;
        } else {
            sylar::Fiber::YieldToHold();    // 加成功就Yield让出执行时间等IOManager唤醒切回到当前协程为唤醒对象，返回到这里有两种情况，1.是之前的超时计时器那里cancel了，2.是addEvent里Epoll_Wait监听到了
            if(timer) {                     // 定时器存在就取消
                timer -> cancel();          
            }
            if(tinfo -> cancelled) {        // 如果条件已经被cancel
                errno = tinfo -> cancelled; // 设置下error
                return -1;                  // 返回-1
            }
            goto retry;                     // 因为从这个block开始说明epoll_wait检测到对应的fd真的有数据进来，那么返回到开头继续原本的动作
        }
    }
    return n;
}
extern "C" {
    /*
        定义函数指针初始化
        XX(sleep);
        XX(usleep);
        sleep_fun sleep_f = nullptr;
        usleep_fun usleep_f = nullptr;
    */
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

    unsigned int sleep(unsigned int seconds) {
        if(!sylar::t_hook_enable) {         // 未开启hook
            return sleep_f(seconds);        // 返回初始的方法
        }
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        // std::bind 对模板函数的用法
        // bind(模板类型，默认参数，正常的bind)
        iom -> addTimer(seconds * 1000, std::bind((void(sylar::Scheduler::*)
                        (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
                        , iom, fiber, -1));
        // iom -> addTimer(seconds * 1000, [iom, fiber](){
        //     iom->schedule(fiber);
        // });
        sylar::Fiber::YieldToHold();
        return 0;
    }

    int usleep(useconds_t usec) {
        if(!sylar::t_hook_enable) {
            return usleep_f(usec);
        }

        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        iom -> addTimer(usec / 1000, std::bind((void(sylar::Scheduler::*)
                        (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
                        , iom, fiber, -1));
        // iom -> addTimer(usec / 1000, [iom, fiber](){
        //     iom->schedule(fiber);
        // });
        sylar::Fiber::YieldToHold();
        return 0; 
    }

    int nanosleep (const struct timespec *req, struct timespec *rem) {
        if(!sylar::t_hook_enable) {
            return nanosleep_f(req, rem);
        }

        int timeout_ms = req->tv_sec * 1000 + req -> tv_nsec / 1000 / 1000;
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        iom -> addTimer(timeout_ms, std::bind((void(sylar::Scheduler::*)
                        (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
                        , iom, fiber, -1));
        // iom -> addTimer(timeout_ms, [iom, fiber](){
        //     iom->schedule(fiber);
        // });
        sylar::Fiber::YieldToHold();
        return 0;
    }

    //socket
    int socket(int domain, int type, int protocol) {
        if(!sylar::t_hook_enable) {
            return socket_f(domain, type, protocol);
        }
        int fd = socket_f(domain, type, protocol);
        if(fd == -1) {
            return fd;
        }
        sylar::FdMgr::GetInstance() -> get(fd, true);
        return fd;
    }

    int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms) {
        if(!sylar::t_hook_enable) {
            return connect_f(fd, addr, addrlen);
        }
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd);
        if(!ctx || ctx -> isClose()) {
            errno = EBADF;
            return -1;
        }

        if(!ctx -> isSocket()) {
            return connect_f(fd, addr, addrlen);
        }

        if(ctx -> getUserNonblock()) {
            return connect_f(fd, addr, addrlen);
        }

        int n = connect_f(fd, addr, addrlen);
        if(n == 0) {
            return 0;
        } else if(n != -1 || errno != EINPROGRESS) {
            return n;
        }

        sylar::IOManager* iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::shared_ptr<timer_info> tinfo(new timer_info);
        std::weak_ptr<timer_info> winfo(tinfo);

        if(timeout_ms != (uint64_t)-1) {
            timer = iom -> addConditionTimer(timeout_ms, [winfo, fd, iom]() {
                auto t = winfo.lock();
                if(!t || t -> cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom -> cancelEvent(fd, sylar::IOManager::WRITE);
            }, winfo);
        }

        int rt = iom->addEvent(fd, sylar::IOManager::WRITE);

        if(rt == 0) {
            sylar::Fiber::YieldToHold();
            if(timer) {
                timer -> cancel();
            }
            if(tinfo->cancelled) {
                errno = tinfo->cancelled;
                return -1;
            }
        } else {
            if(timer) {
                timer -> cancel();
            }
            SYLAR_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
        }

        int error = 0;
        socklen_t len = sizeof(int);
        if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
            return -1;
        }

        if(! error) {
            return 0;
        } else {
            errno = error;
            return -1;
        }
    }

    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
        return connect_with_timeout(sockfd, addr, addrlen, sylar::s_connect_timeout);     
    }
    
    int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
        int fd = do_io(s, accept_f, "accept", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
        if(fd >= 0) {
            sylar::FdMgr::GetInstance() -> get(fd, true);
        }
        return fd;
    }

    ssize_t read(int fd, void *buf, size_t count) {
        return do_io(fd, read_f, "read", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);
    }

    ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
        return do_io(fd, readv_f, "readv", sylar::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
    }   

    ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
        return do_io(sockfd, recv_f, "recv", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
    }

    ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen) {
        return do_io(sockfd, recvfrom_f, "recvfrom", 
                        sylar::IOManager::READ, SO_RCVTIMEO, 
                        buf, len, flags, src_addr, addrlen);
    }
    
    ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
        return do_io(sockfd, recvmsg_f, "recvmsg", 
                    sylar::IOManager::READ, SO_RCVTIMEO, msg, flags); 
    }

    ssize_t write(int fd, const void *buf, size_t count) {
        return do_io(fd, write_f, "write", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);
    }

    ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
        return do_io(fd, writev_f, "writev", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
    }

    ssize_t send(int s, const void *msg, size_t len, int flags) {
        return do_io(s, send_f, "send", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
    }

    ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
        return do_io(s, sendto_f, "sendto", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
    }

    ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
        return do_io(s, sendmsg_f, "sendmsg", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
    }


    int close(int fd) {
        if(!sylar::t_hook_enable) {
            return close_f(fd);
        }

        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd);
        if(ctx) {
            auto iom = sylar::IOManager::GetThis();
            if(iom) {
                iom->cancelAll(fd);
            }
            sylar::FdMgr::GetInstance() -> del(fd);
        }
        return close_f(fd);
    }

    int fcntl(int fd, int cmd, ... /* arg */) {
        // // todo: check 
        // if(!sylar::t_hook_enable) {
        //     va_list va;
        //     va_start(va, cmd);
        //     int arg = va_arg(va, int);
        //     va_end(va);
        //     return fcntl_f(fd, cmd, arg);
        // }

        va_list va;
        va_start(va, cmd);
        switch(cmd) {
            case F_SETFL:
                {
                    int arg = va_arg(va, int);
                    va_end(va);
                    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd);
                    if(!ctx || ctx -> isClose() || !ctx -> isSocket()) {
                        return fcntl_f(fd, cmd, arg);
                    }
                    ctx -> setUserNonblock(arg & O_NONBLOCK);
                    if(ctx -> getSysNonblock()) {
                        arg |=O_NONBLOCK;
                    } else {
                        arg &= ~O_NONBLOCK;
                    }
                    return fcntl_f(fd, cmd, arg);
                }
                break;
            case F_GETFL:
                {
                    va_end(va);
                    int arg = fcntl_f(fd, cmd);
                    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(fd);
                    if(!ctx || ctx -> isClose() || ! ctx -> isSocket()) {
                        return arg;
                    }
                    if(ctx -> getUserNonblock()) {
                        return arg | O_NONBLOCK;
                    } else {
                        return arg & ~O_NONBLOCK;
                    }
                }
                break;
            case F_DUPFD:
            case F_DUPFD_CLOEXEC:
            case F_SETFD:
            case F_SETOWN:
            case F_SETSIG:
            case F_SETLEASE:
            case F_NOTIFY:
            case F_SETPIPE_SZ:
                {
                    int arg = va_arg(va, int);
                    va_end(va);
                    return fcntl_f(fd, cmd, arg);
                }
                break;

            case F_GETFD:
            case F_GETOWN:
            case F_GETSIG:
            case F_GETLEASE:
            case F_GETPIPE_SZ:
                {
                    va_end(va);
                    return fcntl_f(fd, cmd);
                }
                break;
            
            case F_SETLK:
            case F_SETLKW:
            case F_GETLK:
                {
                    struct flock* arg = va_arg(va, struct flock*);
                    va_end(va);
                    return fcntl_f(fd, cmd, arg);
                }
                break;
            
            case F_GETOWN_EX:
            case F_SETOWN_EX:
                {
                    struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                    va_end(va);
                    return fcntl_f(fd, cmd, arg);
                }
                break;

            default:
                va_end(va);
                return fcntl_f(fd, cmd);
        }
    }

    int ioctl(int d, unsigned long int request, ...) {
        va_list va;
        va_start(va, request);
        void* arg = va_arg(va, void*);
        va_end(va);

        if(FIONBIO == request) {
            bool user_nonblock = !!*(int*)arg;
            sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(d);
            if(!ctx || ctx -> isClose() || !ctx->isSocket()) {
                return ioctl_f(d, request, arg);
            }
            ctx -> setUserNonblock(user_nonblock);
        }

        return ioctl_f(d, request, arg);
    }

    int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
        return getsockopt_f(sockfd, level, optname, optval, optlen);
    }

    int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
        if(!sylar::t_hook_enable) {
            return setsockopt_f(sockfd, level, optname, optval, optlen);
        }
        if(level == SOL_SOCKET) {
            if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
                sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance() -> get(sockfd);
                if(ctx) {
                    const timeval* tv = (const timeval*)optval;
                    ctx -> setTimeout(optname, tv->tv_sec * 1000 + tv -> tv_usec / 1000);
                }
            }
        }
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
}
