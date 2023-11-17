#include <atomic>
#include "fiber.h"
#include "macro.h"
#include "config.h"
#include "scheduler.h"
#include "log.h"

#define optimizerTest 1

namespace sylar{

    static std::atomic<uint64_t> s_fiber_id {0};                               // 静态：协程ID
    static std::atomic<uint64_t> s_fiber_count {0};                            // 静态：协程计数器

    /*
        这里 t_fiber 是一个线程局部变量，用于存储当前线程正在执行的协程对象指针。当 t_fiber 为 nullptr 时，表示当前线程不在执行任何协程，即主协程。当 t_fiber 不为 nullptr 时，它指向当前线程正在执行的协程对象。
        判断当前线程是否在执行主协程的方法是检查 t_fiber 是否为 nullptr。如果 t_fiber 为 nullptr，则当前线程不在执行任何协程，可以认为它是主协程。如果 t_fiber 不为 nullptr，则当前线程正在执行一个协程，而不是主协程。
    */
    static thread_local Fiber* t_fiber = nullptr;                              // 当前线程下正在执行的协程
    static thread_local Fiber::ptr t_threadFiber = nullptr;                    // main_fiber

    static ConfigVar<uint32_t>::ptr g_stack_size = Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");
    
    static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    // 协程栈内存分配器
    class MallocStackAllocator {
    public:
        static void* Alloc(size_t size) {
            return malloc(size);
        }

        static void Dealloc(void* vp, size_t size) {
            return free(vp);
        }
    };

    using StackAllocator = MallocStackAllocator;  // 现在使用Malloc这种静态的分配器，这样下面的编程中我可以一直使用StackAllocator, 如果我以后要换内存分配器，我只需要修改这行

    // 主协程创建，在当前的线程上赋值我们的线程上下文, 之所以要实现是为了之后static方法调用实现单列
    Fiber::Fiber() {
        m_state = EXEC;                         // 主协程通常是在整个应用程序的生命周期内存在的
        SetThis(this);                          // 置fiber为当前的实例(主fiber)

        if(getcontext(&m_ctx)) {
            SYLAR_ASSERT2(false, "getContext");
        }

        ++s_fiber_count;

        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber()";
    }

    Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller) 
        : m_id(++s_fiber_id), m_cb(cb) {
        ++ s_fiber_count;
        m_stacksize = stacksize == 0 ? g_stack_size -> getValue() : stacksize; 

        m_stack = StackAllocator::Alloc(m_stacksize);

        // 初始化ucp结构体，将当前的上下文保存在ucp中
        if(getcontext(&m_ctx)) {
            SYLAR_ASSERT2(false, "getContext");
        } 

        m_ctx.uc_link = nullptr;                  // sylar在uc_link设置当前context执行完的下一个要执行的上下文，如果为空则执行完后退出, 非常危险，这里的退出指的是退出当前的线程！！！
        //m_ctx.uc_link = &t_threadFiber -> m_ctx;    // 优化：按照sylar的协程逻辑，直接返回到主协程 。。。update: sylar是对的，因为按照直接回到当前线程的主协程在加入调度器之后就没法正常工作了...
        m_ctx.uc_stack.ss_sp = m_stack;             // 执行当前上下文的需要的stack
        m_ctx.uc_stack.ss_size = m_stacksize;

        if(! use_caller) {
            #if optimizerTest
            m_ctx.uc_link = &Scheduler::GetMainFiber() -> m_ctx; // test
            #endif
            makecontext(&m_ctx, &Fiber::MainFunc, 0); // 调用&fiber::Mainfunc, 设置下一个要执行的上下文。如果uc_link不是空，就执行下一个context,空就返回

        } else {
            #if optimizerTest
            m_ctx.uc_link = &t_threadFiber -> m_ctx;    // test
            #endif
            makecontext(&m_ctx, &Fiber::CallerMainFunc, 0); 
        }
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber(std::function<void()> cb, size_t stacksize) id = " << m_id; 
    }

    Fiber::~Fiber() {
        --s_fiber_count;
        if(m_stack) {                                           // 如果是sub_fiber
            SYLAR_ASSERT(m_state == INIT || m_state == TERM || m_state == EXCEPT);  

            StackAllocator::Dealloc(m_stack, m_stacksize);
        } else {                                               // 如果是Main_fiber
            SYLAR_ASSERT(!m_cb);
            SYLAR_ASSERT(m_state == EXEC);

            /*
                在这段代码中，this 指的是当前正在析构的 Fiber 对象的指针。
                当析构函数被调用时，它被调用在一个特定的 Fiber 对象上，this 指向了这个对象的地址。

                cur 是一个局部变量，它保存了当前线程中的当前协程指针，即 t_fiber 变量的值。
                cur 的目的是为了比较当前正在析构的 Fiber 对象的指针（this）与当前线程正在执行的协程的指针是否相同。
                如果它们相同，说明正在析构的 Fiber 对象就是当前线程正在执行的协程。

                所以，cur == this 的比较是用来确定正在析构的 Fiber 对象是否是当前线程正在执行的协程。
                如果相等，说明当前线程的主协程正在被析构，于是会执行 SetThis(nullptr) 来将当前线程的协程指针设为 nullptr，以避免后续执行主协程。
            */
            Fiber* cur = t_fiber;                             // 将当前线程的当前协程指针保存在 cur 变量中。 
            if(cur == this) {                                 // 检查当前协程指针 cur 是否等于要销毁的协程对象 this。如果它们相等，说明正在销毁的协程就是当前线程正在执行的协程。
                SetThis(nullptr);                             // 如果当前协程正在执行，就调用 SetThis(nullptr) 来将当前线程的当前协程指针设为 nullptr，表示当前线程不再执行任何协程。这是为了确保当前协程在销毁后不再被执行。
            }
        }
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::~Fiber() id = " << m_id;;
    }

    /*
        重置方法，为了节省内存空间，如果我们的一个协程完成了上面运行的工作但还没有释放
        我们可以复用这个协程
    */
    void Fiber::reset(std::function<void()> cb) {
        SYLAR_ASSERT(m_stack);
        SYLAR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
        m_cb = cb;
        
        if(getcontext(&m_ctx)) {
            SYLAR_ASSERT2(false, "getContext");
        }

        m_ctx.uc_link = nullptr;    

        #if optimizerTest
        m_ctx.uc_link = &Scheduler::GetMainFiber() -> m_ctx;    
        #endif

        m_ctx.uc_stack.ss_sp = m_stack;           
        m_ctx.uc_stack.ss_size = m_stacksize;

        makecontext(&m_ctx, &Fiber::MainFunc, 0);
        m_state = INIT;
    }

    void Fiber::callIn() {
        SetThis(this);
        SYLAR_ASSERT(m_state != EXEC);
        m_state = EXEC;
        if(swapcontext(&t_threadFiber -> m_ctx, &m_ctx)) { // 从thread 协程切换到要执行的协程
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }
    void Fiber::swapIn() {
        SetThis(this);
        SYLAR_ASSERT(m_state != EXEC);
        m_state = EXEC;
        if(swapcontext(&Scheduler::GetMainFiber() -> m_ctx, &m_ctx)) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }

    void Fiber::swapOut() {
        SetThis(Scheduler::GetMainFiber());

        //m_state = HOLD;
        if(swapcontext(&m_ctx, &Scheduler::GetMainFiber() -> m_ctx)) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }

    void Fiber::callOut() {
        SetThis(t_threadFiber.get());

        //m_state = HOLD;
        if(swapcontext(&m_ctx, &t_threadFiber -> m_ctx)) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }

    void Fiber::SetThis(Fiber* f) {
        t_fiber = f;
    }

    Fiber::ptr Fiber::GetThis() {
        if(t_fiber) {
            return t_fiber->shared_from_this();
        }
        Fiber::ptr main_fiber(new Fiber);
        SYLAR_ASSERT(t_fiber == main_fiber.get());    // 我们上一行主动执行了默认构造函数，里面有setThis(this)所以t_fiber（当前的协程）应该是我们的新main_fiber.get()是一样的
        t_threadFiber = main_fiber;
        return t_fiber -> shared_from_this();
    }

    void Fiber::YieldToReady() {
        Fiber::ptr cur = GetThis();
        cur -> m_state = READY;
        cur -> swapOut();
    }

    void Fiber::YieldToHold() {
        Fiber::ptr cur = GetThis();
        cur -> m_state = HOLD;
        cur -> swapOut();
    }

    uint64_t Fiber::TotalFibers() {
        return s_fiber_count;
    }

    void Fiber::MainFunc() {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
        try {
            cur -> m_cb();
            cur -> m_cb = nullptr;     // 这里要把回调函数置成nullptr，为了防止如果这个cb中有智能指针，因为引用一直存在没法自动释放内存
            cur -> m_state = TERM;
        } catch(std::exception& ex) {
            cur -> m_state = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber exception: " << ex.what();
        } catch(...) {
            cur -> m_state = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber exception";
        }
        /*
            优化：原本在项目中sylar在有参构造中直接写了上下文为null，这样导致MainFunc完成之后就会自动停止整个线程
            为了完成MainFunc之后直接回到主协程，重写了uc_link参数链接回主协程
            但是这里是原本设计中，完成MainFunc直接swapOut然后利用SwapOut的上下文切换回到主协程，
            但是又为了自动释放智能指针，智能再次拿出raw ptr
            这样会导致raw ptr变成野指针！
            所以这里直接释放这个智能指针的引用，并且通过有参构造中设置的uc_link 返回
            update: 我们需要在有调度器的情况下也正确的运行，所以这里应该改回原来的
        */
        #if optimizerTest
        cur.reset();    
        #else
        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr -> swapOut();

        SYLAR_ASSERT2(false, "never reach");
        #endif
    }

    void Fiber::CallerMainFunc() {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
        try {
            cur -> m_cb();
            cur -> m_cb = nullptr;     // 这里要把回调函数置成nullptr，为了防止如果这个cb中有智能指针，因为引用一直存在没法自动释放内存
            cur -> m_state = TERM;
        } catch(std::exception& ex) {
            cur -> m_state = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber exception: " << ex.what();
        } catch(...) {
            cur -> m_state = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber exception";
        }
        #if optimizerTest
        cur.reset();
        #else
        auto raw_ptr = cur.get();    // test
        cur.reset();
        raw_ptr -> callOut();        // test

        SYLAR_ASSERT2(false, "never reach");  // test
        #endif
    }

    uint64_t Fiber::GetFiberID() {
        // 之所以判断是为了防止某些线程没有协程，这样直接用GetThis()的话创建一个主协程然后返回。
        if(t_fiber) {
            return t_fiber -> getId();
        }
        return 0;
    }
}