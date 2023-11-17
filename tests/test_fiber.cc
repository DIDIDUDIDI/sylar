#include "sylar/sylar.h"
#include "sylar/fiber.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run_in_fiber() {
    SYLAR_LOG_INFO(g_logger) << "fiber test begin";
    sylar::Fiber::YieldToHold();
    SYLAR_LOG_INFO(g_logger) << "fiber test end";
    sylar::Fiber::YieldToHold();
}

void test_fiber() {
    sylar::Fiber::GetThis();
    SYLAR_LOG_INFO(g_logger) << "jump in to scope, main thread begin";
    sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));
    fiber -> swapIn();
    SYLAR_LOG_INFO(g_logger) << "back to main thread";
    fiber -> swapIn();
    SYLAR_LOG_INFO(g_logger) << "main thread end, jump out of scope";
    fiber -> swapIn();
}

int main(int argc, char** argv) {
    sylar::Thread::SetName("main thread");
    SYLAR_LOG_INFO(g_logger) << "Test Begin";
    std::vector<sylar::Thread::ptr> thrs_pool;
    for(int i = 0; i < 3; ++ i) {
        thrs_pool.emplace_back(sylar::Thread::ptr(new sylar::Thread(test_fiber, "name_" + std::to_string(i))));
    }
    for(auto t : thrs_pool) {
        t->join();
    }
    SYLAR_LOG_INFO(g_logger) << "Test end";
    return 0;
}