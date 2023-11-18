#include "hook.h"
#include "fiber.h"
#include "iomanager.h"

namespace sylar {

    static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
        XX(sleep) \
        XX(usleep)

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

    // 利用数据初始化在main函数前初始化
    struct _HookIniter {
        _HookIniter() {
            hook_init();
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
        //iom -> addTimer(seconds * 1000, std::bind(&sylar::IOManager::schedule, iom, fiber));
        iom -> addTimer(seconds * 1000, [iom, fiber](){
            iom->schedule(fiber);
        });
        sylar::Fiber::YieldToHold();
        return 0;
    }

    int usleep(useconds_t usec) {
        if(!sylar::t_hook_enable) {
            return usleep_f(usec);
        }

        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        //iom -> addTimer(usec / 1000, std::bind(&sylar::IOManager::schedule, iom, fiber));
        iom -> addTimer(usec / 1000, [iom, fiber](){
            iom->schedule(fiber);
        });
        sylar::Fiber::YieldToHold();
        return 0; 
    }
}
