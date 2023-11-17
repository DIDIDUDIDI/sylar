#ifndef __SYLAR_SCHEDULER_H__
#define __SYLAR_SCHEDULER_H__

#include <memory>
#include "thread.h"
#include "fiber.h"
#include <vector>
#include <list>

namespace sylar {
    /*
        Scheduler 类似一个线程池，提供管理线程 -> 线程对应的协程
        同时也支持让特定的线程执行开启、复用协程去执行某一个任务
        我们的结构是一个线程创建Scheduler -> 然后Scheduler创建对应的线程池 -> 每个线程还有自己的协程用来运行任务
        创建Scheduler是我们的主线程(m_rootThread)，里面的协程也是我们的主协程(m_rootFiber)
        
    */
    class Scheduler {
    public:
        typedef std::shared_ptr<Scheduler> ptr;
        typedef Mutex MutexType;

        /*
            @threads size_t, 线程的数量，默认是1
            @bool use_caller, 如果为true表示创建Scheduler的线程会归scheduler管理，默认true
            @std::string name, scheduler 的名字
        */
        Scheduler(size_t threads = 1, bool use_caller = true, const std::string name = "");  
        virtual ~Scheduler();           // 虚析构函数，我们的Scheduler是一个默认的基类

        const std::string& getName() const {return m_name;}

        static Scheduler* GetThis();    // 返回当前的Scheduler指针
        static Fiber* GetMainFiber();   // 返回Scheduler的主协程

        void start();
        void stop();

        template<class FiberOrCb>
        void schedule(FiberOrCb fc, int thread = -1) {
            bool need_tickle = false; 
            {
                MutexType::Lock lock(m_mutex);
                need_tickle = scheduleNotLock(fc, thread);
            }

            if(need_tickle) {
                tickle();
            }
            
        }

        // 批量方法，这样只要上一次锁就好了，减少context Switch的开销, 并且一次上锁后可以保证这些连续的任务都是按照我们的input的顺序去执行的
        template<class InputIterator>
        void schedule(InputIterator begin, InputIterator end) {
            bool need_tickle = false;
            {
                MutexType::Lock lock(m_mutex);
                while(begin != end) {
                    need_tickle = scheduleNotLock(&*begin, -1) || need_tickle;
                    ++ begin;
                }
            }
            if(need_tickle) {
                tickle();
            }
        }

    protected:
        virtual void tickle();      // 唤醒线程，类似型号量
        void run();
        virtual bool stopping();
        virtual void idle();        // idle，就算没有任务也可以keep住协程/线程，可以让他一直占着CPU，也可以时不时sleep下让出CPU时间，具体通过实现的子类来做

        void setThis();

        bool hasIdleThreads() { return m_idleThreadCount > 0; }
        
    private:
        /*
            无锁方法，假如目前所有线程空闲并且等待唤醒
            我们就可以用这个方法通知所有线程消息队里有东西了，要来取
        */
        template<class FiberOrCb>
        bool scheduleNotLock(FiberOrCb fc, int thread) {
            bool need_tickle = m_fibers.empty();        // 检查我们的队列是否为空
            FiberAndThread ft(fc, thread);              
            if(ft.cb || ft.fiber) {                     // 有需要做的东西就放进队里
                m_fibers.push_back(ft);
            }
            return need_tickle;                         // 如果为空我们就通知队列来取
        }
    private:
        struct FiberAndThread {
            Fiber::ptr fiber;
            std::function<void()> cb;
            int thread;                // 这个thread是thread_id, 用来表示在哪个线程上跑

            FiberAndThread(Fiber::ptr f, int thr) 
                : fiber(f), thread(thr) {

            }

            FiberAndThread(Fiber::ptr* f, int thr) 
                : thread(thr){
                fiber.swap(*f);         // 该构造函数是用来传入智能指针的构造函数，本质是不推荐这么做，因为智能指针应该让外部管理，不过先写了这个方法，不一定启用.. 要swap也是为了把f的引用放到fiber里，不影响智能指针的计数器
            }

            FiberAndThread(std::function<void()> f, int thr) 
                : cb(f), thread(thr) {

            }

            FiberAndThread(std::function<void()>* f, int thr) 
                : thread(thr) {
                cb.swap(*f);            // 同上
            }

            // 默认构造函数，没有默认构造函数放入STL的容器里会出现初始化失败的问题
            FiberAndThread()
                : thread(-1) {
            }

            void reset() {
                cb = nullptr;
                fiber = nullptr;
                thread = -1;
            }
        };
    private:
        MutexType m_mutex;
        std::vector<Thread::ptr> m_threads;       // 线程存放, 管理受Scheduler管理的线程
        std::list<FiberAndThread> m_fibers;       // 协程管理清单, 保存即将执行的协程、方法清单, 是一种消息队列.
        std::string m_name;                       // scheduler 的名字
        Fiber::ptr m_rootFiber;                   // 主协程调度器
    protected:
        std::vector<int> m_threadIds;             // 管理线程ID，之后可以通过hash的方法去把需求的任务放到一个存在的线程上执行
        size_t m_threadCount = 0;
        std::atomic<size_t> m_activeThreadCount = {0};
        std::atomic<size_t> m_idleThreadCount = {0};
        bool m_stopping = true;
        bool m_autoStop = false;
        int m_rootThread = 0;                      // thread ID
    };
}

#endif