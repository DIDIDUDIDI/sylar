#ifndef __SYLAR_FIBER_H__
#define __SYLAR_FIBER_H__

#include <memory>
#include <functional>
#include <ucontext.h>
#include "thread.h"

namespace sylar {
    class Scheduler;
    /*
        我们的fiber由主fiber调用subFiber之后执行，
        如果subFiber完成之后直接返回到主fiber，然后在执行下一个subfiber
        不会有subfiber产生新的subfiber的情况
    */
    class Fiber : public std::enable_shared_from_this<Fiber> {
    friend class Scheduler;
    public:
        typedef std::shared_ptr<Fiber> ptr;

        // 协程状态
        enum State {
            INIT,     // 初始化
            HOLD,     // 暂停
            EXEC,     // 执行
            TERM,     // 销毁
            READY,    // 准备
            EXCEPT    // 异常
        };
    private:
        Fiber();      // 默认构造器私有，为了实现单列模式

    public:
        Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);    // 构造器，提供的执行方法和协程栈大小
        ~Fiber();

        void reset(std::function<void()> cb);                     // 充值协程的回调方法和状态
        void swapIn();                                            // Scheduler切换到当前的协程执行，目标协程唤醒到当前的执行状态 
        void swapOut();                                           // Scheduler切换到后台执行
        void callIn();                                            // 原版的swapin()
        void callOut();                                           // 原版的swapOut()
        uint64_t getId() const {return m_id;}
        State getState() const {return m_state;}
        void setState(State value) {m_state = value;}
    public:
        static void SetThis(Fiber* f);   // 设置当前的协程
        static Fiber::ptr GetThis();     // 返回当前的协程
        static void YieldToReady();      // 切换到后台，并且变成ready 状态
        static void YieldToHold();       // 切换到后台，并且变成Hold 状态
        static uint64_t TotalFibers();   // 返回总协程数

        static void MainFunc();          //  执行fiber的回调函数
        static void CallerMainFunc();

        static uint64_t GetFiberID();        
    private:
        uint64_t m_id = 0;              // 协程ID
        uint32_t m_stacksize = 0;       // 协程栈大小
        State m_state = INIT;           // 协程状态

        ucontext_t m_ctx;
        void* m_stack = nullptr;        // 协程栈

        std::function<void()> m_cb;
    };

}

#endif