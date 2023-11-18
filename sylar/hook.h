#ifndef __SYLAR_HOOK_H__
#define __SYLAR_HOOK_H__

/*
    hook系统底层和socket相关的API，socket IO相关的API，以及sleep系列的API。
    hook的开启控制是线程粒度的，可以自由选择。通过hook模块，可以使一些不具异步功能的API，展现出异步的性能，如MySQL。
*/
#include <unistd.h>
//#include "sylar.h"
#include <dlfcn.h>
#include <fcntl.h>

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
*/
extern "C" {
typedef unsigned int (*sleep_fun)(unsigned int seconds);
extern sleep_fun sleep_f;

typedef int (*usleep_fun)(useconds_t usec);
extern usleep_fun usleep_f;
}

#endif