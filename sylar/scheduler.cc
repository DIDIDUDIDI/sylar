#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"


namespace sylar {

    static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    static thread_local Scheduler* t_scheduler = nullptr;
    static thread_local Fiber* t_fiber = nullptr;               // 当前Scheduler的主协程

    /*
        @threads size_t, 线程的数量，默认是1
        @bool use_caller, 如果为true表示创建Scheduler的线程会归scheduler管理，默认true
        @std::string name, scheduler 的名字
    */
    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string name)
        : m_name(name) {
        SYLAR_ASSERT(threads > 0);                                           // Scheduler管理的线程数量

        if(use_caller) {                                                     // 如果将call Scheduler的线程放入调度器管理
            // Fiber::ptr test_main =  sylar::Fiber::GetThis();
            sylar::Fiber::GetThis();                                         // 创建主协程, 在这个新的放入调度管理器的线程上
            --threads;

            SYLAR_ASSERT(GetThis() == NULL);                                 // 这里的GetThis是Scheduler的方法，判断该线程是不是已经有在Scheduler里了如果是那肯定有问题，不能被两个Scheduler管理
            t_scheduler = this;
            
            // todo: 检查GetThis 的main fiber是否是下面的m_rootFiber, 进过测试，不是
            // std::bind 是 functional 的方法，用来绑定函数
            m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));  // 我们创建一个新的协程去调度, 也就是说我们创建好就有两个协程？
            sylar::Thread::SetName(m_name);

            t_fiber = m_rootFiber.get();
            m_rootThread = sylar::GetThreadID();
            m_threadIds.push_back(m_rootThread);
            // if(test_main == m_rootFiber) {
            //     SYLAR_LOG_INFO(g_logger) << "same Fiber";
            // } else {
            //     SYLAR_LOG_INFO(g_logger) << "different Fiber"; // this
            // }
        } else {
            m_rootThread = -1;
        }
        m_threadCount = threads;
    }
    Scheduler::~Scheduler() {
        SYLAR_ASSERT(m_stopping);
        if(GetThis() == this) {
            t_scheduler = nullptr;
        }
    }

    Scheduler* Scheduler::GetThis() {
        return t_scheduler;
    }   
    
    Fiber* Scheduler::GetMainFiber() {
        return t_fiber;
    }

    /*
        核心方法
    */
    void Scheduler::start() {
        //{
            MutexType::Lock lock(m_mutex);
            if(! m_stopping) {                  // 说明已经启动了
                return;
            }
            m_stopping = false;
            SYLAR_ASSERT(m_threads.empty());
            m_threads.resize(m_threadCount);
            // 创建线程池
            for(size_t i = 0 ; i < m_threadCount; ++ i) {
                m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
                m_threadIds.push_back(m_threads[i]->getId());
            }
        //}
            lock.unlock();
            /*  
                callIn 是和当前的thread main fiber交换
                和下面swapin是不一样的，swapin是和schedule的main fiber 交换
                thread main fiber != schedule main fiber 
            */
        // if(m_rootFiber) {                
            // 启动m_rootFiber, m_rootFiber是在做什么呢？看构造函数可知是执行run方法
            // 这样后续的代码都只能等run方法运行完才可以了，但是run方法运行完了整个schedule也就停止了所以不能写这里
        //     m_rootFiber -> callIn();         
        // }
    }
    
    void Scheduler::stop() {
        m_autoStop = true;
        // 这里是判断是不是use caller并且只有一个线程？ => 为什么要这么设计？
        // 这里的m_rootFiber是创建Scheduler的线程的协程里面执行Schedlder run的协程
        if(m_rootFiber 
            && m_threadCount == 0 
            && (m_rootFiber -> getState() == Fiber::TERM || m_rootFiber -> getState() == Fiber::INIT) 
          ) {
            SYLAR_LOG_INFO(g_logger) << this << " stopped";
            m_stopping = true;

            if(stopping()) {
                return;
            }
        }

        // bool exit_on_this_fiber = false;
        // 判断是否是user caller
        if(m_rootThread != -1) {
            SYLAR_ASSERT(GetThis() == this);
        } else {
            SYLAR_ASSERT(GetThis() != this);
        }

        m_stopping = true;
        // 唤醒线程，逐个stop
        for(size_t i = 0; i < m_threadCount; ++ i) {
            tickle();
        }

        // 主线程tickle
        if(m_rootFiber) {
            tickle();
        }

        if(m_rootFiber) {
            // while (!stopping())
            // {
            //     if(m_rootFiber->getState() == Fiber::TERM || m_rootFiber -> getState() == Fiber::EXCEPT) {
            //         m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true)); 
            //         SYLAR_LOG_INFO(g_logger) << "root fiber term, reset"; 
            //         t_fiber = m_rootFiber.get();
            //     }
            //     m_rootFiber->callIn();
            // }
            if(!stopping()) {
                m_rootFiber -> callIn();
            }
        }

        std::vector<Thread::ptr> thrs;
        {
            MutexType::Lock lock(m_mutex);
            thrs.swap(m_threads);
        }
        for(auto& i : thrs) {
            i -> join();
        }   
    }

    void Scheduler::setThis() {
        t_scheduler = this;
    }

    /*
        run 函数运行逻辑：
        1. 设置当前的线程的schedule
        2. 设置当前的run fiber
        3. while，协程调度循环：
            - 协程消息队列里面是否有任务执行？ 
                -> 有就执行
                -> 无任务执行idle 方法
    */
    // 新创建的线程直接在run 里面启动
    void Scheduler::run() {
        SYLAR_LOG_INFO(g_logger) << "stepin to run()";
        set_hook_enable(true);
        setThis();
        if(sylar::GetThreadID() != m_rootThread) {  // 判断运行call方法的是不是当前的主线程，如果不是就要初始化fiber，主线程的初始化fiber在构造函数中完成了
            t_fiber = Fiber::GetThis().get();
        }

        // 如果分配的事情做完了就进入idle，等消息队列来东西再唤醒
        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
        Fiber::ptr cb_fiber;                     // 如果是cb，执行这个cb的fiber

        FiberAndThread ft;
        while(true) {
            ft.reset();                          // 初始化
            bool tickle_me = false;
            bool is_active = false;
            {
                MutexType::Lock lock(m_mutex);
                auto it = m_fibers.begin();
                while(it != m_fibers.end()) {   // 检查我们的消息队列
                    if(it->thread != -1 && it -> thread != GetThreadID()) {  // 如果我们的消息队列里的任务上指定的ID不等于我们当前的ID
                        ++ it;
                        tickle_me = true;       // 通知别的线程来领任务
                        continue;
                    }
                    
                    SYLAR_ASSERT(it -> cb || it -> fiber);
                    // 如果fiber已经在运行了
                    if(it -> fiber && it -> fiber -> getState() == Fiber::EXEC) {
                        ++ it;
                        continue;
                    }
                    ft = *it;                   // 领到任务
                    m_fibers.erase(it);         // 移出队列
                    ++m_activeThreadCount;
                    is_active = true;
                    break;                      
                }
            }

            if(tickle_me) {
                tickle();
            }

            if(ft.fiber && (ft.fiber -> getState() != Fiber::TERM || ft.fiber -> getState() != Fiber::EXCEPT)) { // 如果队列中是一个fiber
                ft.fiber -> swapIn();      // 执行Fiber
                --m_activeThreadCount;

                if(ft.fiber -> getState() == Fiber::READY) {    // 说明是被fiber::yieldtoready的，还没做完，只是暂时让出cpu执行，推回队列
                    schedule(ft.fiber);
                }else if(ft.fiber -> getState() != Fiber::TERM && ft.fiber -> getState() != Fiber::EXCEPT) {
                    ft.fiber -> setState(Fiber::HOLD);
                }

                ft.reset();               // 初始化这个ft为了智能指针自动释放
            }else if(ft.cb) {             // 如果是一个回调函数
                if(cb_fiber) {                 // 如果cb_fiber 已经好了
                    cb_fiber -> reset(ft.cb);  // 直接使用
                } else {
                    cb_fiber.reset(new Fiber(ft.cb)); // 初始化并且创建一个新的fiber
                }

                ft.reset();
                cb_fiber -> swapIn();
                --m_activeThreadCount;
                if(cb_fiber->getState() == Fiber::READY) {
                    schedule(cb_fiber);
                    cb_fiber.reset();
                } else if(cb_fiber -> getState() == Fiber::TERM || cb_fiber -> getState() == Fiber::EXCEPT) {
                    cb_fiber->reset(nullptr);
                } else {
                    cb_fiber -> setState(Fiber::HOLD);
                    cb_fiber.reset();
                }
            } else {                    // 消息队列中走了一圈没有领到任务, ft没有fiber也没有cb, 那么进入idle协程
                if(is_active) {
                    --m_activeThreadCount;
                    continue;
                }
                if(idle_fiber->getState() == Fiber::TERM) {     // 如果idle协程已经是term了，说明已经没有任何任务了，
                    SYLAR_LOG_INFO(g_logger) << "idle fiber term";
                    // tickle();
                    break;
                    //continue;
                }

                ++m_idleThreadCount;
                idle_fiber -> swapIn();
                --m_idleThreadCount;
                if(idle_fiber->getState() != Fiber::TERM && idle_fiber->getState() != Fiber::EXCEPT) {
                    idle_fiber->setState(Fiber::HOLD);
                }
            }

        }
    }
    void Scheduler::tickle() {
        SYLAR_LOG_INFO(g_logger) << "tickle";
    }
    bool Scheduler::stopping() {
        MutexType::Lock lock(m_mutex);
        return m_stopping && m_autoStop && m_fibers.empty() && m_activeThreadCount == 0;
    }
    void Scheduler::idle() {
        SYLAR_LOG_INFO(g_logger) << "idle";
        while(! stopping()) {
            sylar::Fiber::YieldToHold();
        }
    }
}