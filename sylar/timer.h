#ifndef __SYLAR_TIMER_H__
#define __SYLAR_TIMER_H__

#include <memory>
#include "thread.h"
#include <set>
#include "util.h"

namespace sylar {
    class TimerManager;
    class Timer : public std::enable_shared_from_this<Timer> {
        friend class TimerManager;
        public:
            typedef std::shared_ptr<Timer> ptr;
            bool cancel();
            bool refresh();
            bool reset(uint64_t ms, bool from_now);

        private:
            // 不能自己定义Timer，只能他通过TimerManager调用
            Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager);
            Timer(uint64_t next);
            
        private:
            bool m_recurring = false;       // 是否周期循环定时器
            uint64_t m_ms = 0;              // 执行周期
            uint64_t m_next = 0;            // 精确的下次执行时间
            std::function<void()> m_cb;
            TimerManager* m_manager = nullptr;
        private:
            // 比较器，给定时器用的，用来确定顺序
            struct Comparator { 
                bool operator() (const Timer::ptr &lhs, const Timer::ptr &rhs) const;
            };    

    };

    class TimerManager {
        friend class Timer;
        public:
            typedef RWMutex RWMutexType;

            TimerManager();
            virtual ~TimerManager();

            Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);
            Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb, 
                                        std::weak_ptr<void> weak_cond, bool recurring = false);

            uint64_t getNextTimer();
            void listExpiredCb(std::vector<std::function<void()> >& cbs);       //返回所有到期、要执行的回调，给scheduler 用的
            bool hasTimer();
        protected:
            virtual void onTimerInsertedAtFront() = 0;              // 快速唤醒，如果当前的定时比idle默认轮训小，需要紧急唤醒
            void addTimer(Timer::ptr val, RWMutexType::WriteLock& lock);
        private:
            bool detectClockRollover(uint64_t now_ms);
        private:
            RWMutexType m_mutex;
            // 定时器，需要有序所以用set，注意这里第一个是存放的类型，即ptr，第二个是比较器的struct
            std::set<Timer::ptr, Timer::Comparator> m_timers;
            bool m_tickled = false; 
            uint64_t m_previousTime = 0;
    };
}

#endif