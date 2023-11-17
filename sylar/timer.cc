#include "timer.h"

namespace sylar {
    bool Timer::Comparator::operator() (const Timer::ptr &lhs, const Timer::ptr &rhs) const {
        if(!lhs && !rhs) {
            return false;
        }
        if(!lhs) {
            return true;
        }
        if(!rhs) {
            return false;
        }
        if(lhs -> m_next < rhs -> m_next) {
            return true;
        }
        if(rhs -> m_next < lhs -> m_next) {
            return false;
        }
        return lhs.get() < rhs.get();
    }
    /*
     bool m_recurring = false;       // 是否周期循环定时器
            uint64_t m_ms = 0;              // 执行周期
            uint64_t m_next = 0;            // 精确的执行时间
            std::function<void()> m_cb;
            TimerManager* m_manager = nullptr;
    */
    Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager) 
        : m_recurring(recurring),
          m_ms(ms),
          m_cb(cb),
          m_manager(manager) {
            m_next = sylar::GetCurrentMS() + m_ms;
    }
    
    Timer::Timer(uint64_t next) 
        : m_next(next) {

    }

    bool Timer::cancel() {
        TimerManager::RWMutexType::WriteLock lock(m_manager -> m_mutex);
        if(m_cb) {
            m_cb = nullptr;
            auto it = m_manager->m_timers.find(shared_from_this());
            m_manager -> m_timers.erase(it);
            return true;
        }
        return false;
    }
    bool Timer::refresh() {             //刷新是时间
        TimerManager::RWMutexType::WriteLock lock(m_manager -> m_mutex);
        if(! m_cb) {                    // 都没有cb
            return false;
        }
        auto it = m_manager->m_timers.find(shared_from_this());
        if(it == m_manager->m_timers.end()) {   // 如果不存在
            return false;
        }
        // 先删除重新加，为了让数据结构重新排序
        m_manager -> m_timers.erase(it);
        m_next = sylar::GetCurrentMS() + m_next;
        m_manager -> m_timers.insert(shared_from_this());
        return true;
    }
    bool Timer::reset(uint64_t ms, bool from_now) {
        if(ms == m_ms && !from_now) {           // 现在就是
            return true;
        }
        TimerManager::RWMutexType::WriteLock lock(m_manager -> m_mutex);
        if(! m_cb) {                    
            return false;
        }
        auto it = m_manager -> m_timers.find(shared_from_this());
        if(it == m_manager -> m_timers.end()) {
            return false;
        }
        m_manager->m_timers.erase(it);
        uint64_t start = 0;
        if(from_now) {
            start = sylar::GetCurrentMS();
        } else  {
            start = m_next - m_ms;          // 算出还剩下多少时间
        }
        m_ms = ms; // reset;
        m_next = start + m_ms;
        m_manager -> addTimer(shared_from_this(), lock);
        return true;
    }

    TimerManager::TimerManager() {
        m_previousTime = sylar::GetCurrentMS();
    }
    TimerManager::~TimerManager() {

    }

    Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
        Timer::ptr timer(new Timer(ms, cb, recurring, this));
        RWMutexType::WriteLock lock(m_mutex);
        // auto it = m_timers.insert(timer).first;         // set 返回一个pair<it.location, bool success or not>
        // bool at_front = (it == m_timers.begin());       // 看看是不是放在最前面了
        // lock.unlock();

        // if(at_front) {
        //     onTimerInsertedAtFront();                   // 看看是不是要tickle下
        // }
        addTimer(timer, lock);
        return timer;                                   // 要返回timer的原因是这样如果创建timer的需要取消，可以直接调用返回的timer的取消方法
    }

    // weak pointer, 智能指针的一种，可以不使得引用计数器增加的前提下确定指针是否还存在
    static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
        std::shared_ptr<void> tmp = weak_cond.lock();       // 确认该指针是否还有效（该条件是否还有效）无效会返回空

        if(tmp) {
            cb();                                           // 如果条件还有效，执行
        }
    }

    Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring = false) {
        return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
    }

    uint64_t TimerManager::getNextTimer() {
        RWMutexType::ReadLock lock(m_mutex);
        m_tickled = false;
        if(m_timers.empty()) {
            return ~0ull;                       // 如果Timer为空说明没有任务目前，那么返回最大值，即0取反
        }

        /*
            auto it = m_timers.begin();
            const Timer::ptr& next = *it;
        */
        const Timer::ptr& next = *m_timers.begin(); 
        uint64_t now_ms = sylar::GetCurrentMS();
        if(now_ms >= next -> m_next) {
            return 0;                           // 时间都过了或者已经到了，赶紧执行
        } else {
            return next -> m_next - now_ms;
        }
    }

    // 引用穿第一个cbs vc, 到时候外部的cbs也会修改，C++技巧
    void TimerManager::listExpiredCb(std::vector<std::function<void()> >& cbs) {
        uint64_t now_ms = sylar::GetCurrentMS();
        std::vector<Timer::ptr> expired;
        {
            RWMutexType::ReadLock lock(m_mutex);
            if(m_timers.empty()) {
                return;
            }
        }
        RWMutexType::WriteLock lock(m_mutex);

        bool rollover = detectClockRollover(now_ms);
        if(! rollover && ((*m_timers.begin()) -> m_next > now_ms)) {
            return;
        }
        Timer::ptr now_timer(new Timer(now_ms));          // 因为我们的timer的比较器只关心uint64_t的next
        auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);        // lower_bound, 返回参数的下一个
        while(it != m_timers.end() && (*it) -> m_next == now_ms) {  // 可能有多个一样的时间，我们要找到最后一个
            ++ it;
        }
        expired.insert(expired.begin(), m_timers.begin(), it);  // 从第一个到我们找到的最后一个直接都是小于等于当前的，所以要执行
        m_timers.erase(m_timers.begin(), it);
        cbs.reserve(expired.size());

        for(auto& timer : expired) {
            cbs.push_back(timer -> m_cb);

            if(timer -> m_recurring) {          // 如果是循环定时器，那么重置
                timer->m_next = now_ms + timer -> m_next;
                m_timers.insert(timer);
            } else {                            // 否则清空，防止智能指针不释放
                timer -> m_cb = nullptr;
            }
        }
    }

    void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock& lock) {
        auto it = m_timers.insert(val).first;         // set 返回一个pair<it.location, bool success or not>
        bool at_front = (it == m_timers.begin()) && !m_tickled;       // 看看是不是放在最前面了
        if(at_front) {
            m_tickled = true;
        }
        lock.unlock();

        if(at_front) {
            onTimerInsertedAtFront();                   // 看看是不是要tickle下
        }
    }

    //感觉实属多余了。。。
    bool TimerManager::detectClockRollover(uint64_t now_ms) {
        bool rollover = false;
        if(now_ms < m_previousTime && now_ms < (m_previousTime - 60 * 60 * 1000)) {
            rollover = true;
        }
        m_previousTime = now_ms;
        return rollover;
    }

}