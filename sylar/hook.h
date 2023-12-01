#ifndef __SYLAR_HOOK_H__
#define __SYLAR_HOOK_H__

/*
    hook系统底层和socket相关的API，socket IO相关的API，以及sleep系列的API。
    hook的开启控制是线程粒度的，可以自由选择。通过hook模块，可以使一些不具异步功能的API，展现出异步的性能，如MySQL。
*/
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdint.h>

namespace sylar {
    bool is_hook_enable();
    void set_hook_enable(bool flag);
}

/*
    用C风格方式编译
    实现C++与C及其它语言的混合编程。
    CPP的重载会生成一个mangIDfuncName
    所以我们用extern“C"的方法可以避免
    因为我们会自定义一套同名方法，但是也得提供原本的接口回去
    首先得一步是找到原本的函数定义，然后创建一个对应的指针类型：
    比如我们对方法sleep_fun 原本的定义为：
    unsigned int sleep (unsigned int seconds);
    改成指针类型：
    typedef unsigned int (*sleep_fun)(unsigned int seconds);
    然后成员变量
    extern sleep_fun sleep_f;
    之后声明函数实现，记得我们是替换同名，所以函数的实现应该是原本的名字而不是我们后来定义的：
    unsigned int sleep(unsigned int seconds) {
        // dosomething
    }
*/
extern "C" {
    // sleep
    typedef unsigned int (*sleep_fun)(unsigned int seconds);
    extern sleep_fun sleep_f;

    typedef int (*usleep_fun)(useconds_t usec);
    extern usleep_fun usleep_f;

    typedef int (*nanosleep_fun)(const struct timespec *req, struct timespec *rem);
    extern nanosleep_fun nanosleep_f;

    // socket
    typedef int (*socket_fun)(int domain, int type, int protocol);
    extern socket_fun socket_f;

    typedef int (*connect_fun)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    extern connect_fun connect_f;

    typedef int (*accept_fun)(int s, struct sockaddr *addr, socklen_t *addrlen);
    extern accept_fun accept_f;

    // read
    typedef ssize_t (*read_fun)(int fd, void *buf, size_t count);
    extern read_fun read_f;

    typedef ssize_t (*readv_fun)(int fd, const struct iovec *iov, int iovcnt);
    extern readv_fun readv_f;

    typedef ssize_t (*recv_fun)(int sockfd, void *buf, size_t len, int flags);
    extern recv_fun recv_f;

    typedef ssize_t (*recvfrom_fun)(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen);
    extern recvfrom_fun recvfrom_f;
    
    typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr *msg, int flags);
    extern recvmsg_fun recvmsg_f;

    // write
    typedef ssize_t (*write_fun)(int fd, const void *buf, size_t count);
    extern write_fun write_f;

    typedef ssize_t (*writev_fun)(int fd, const struct iovec *iov, int iovcnt);
    extern writev_fun writev_f;

    typedef ssize_t (*send_fun)(int s, const void *msg, size_t len, int flags);
    extern send_fun send_f;

    typedef ssize_t (*sendto_fun)(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
    extern sendto_fun sendto_f;

    typedef ssize_t (*sendmsg_fun)(int s, const struct msghdr *msg, int flags);
    extern sendmsg_fun sendmsg_f;

    //close
    typedef int (*close_fun)(int fd);
    extern close_fun close_f;


    // sock operation 
    typedef int (*fcntl_fun)(int fd, int cmd, ... /* arg */);
    extern fcntl_fun fcntl_f;

    typedef int (*ioctl_fun)(int d, unsigned long int request, ...);
    extern ioctl_fun ioctl_f;

    typedef int (*getsockopt_fun)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
    extern getsockopt_fun getsockopt_f;

    typedef int (*setsockopt_fun)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
    extern setsockopt_fun setsockopt_f;

    // typedef int (*connect_with_timeout_fun)(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms);
    // extern connect_with_timeout_fun connect_with_timeout_f;
    extern int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms);
    
}

#endif