#include "sylar/sylar.h"
#include "sylar/scheduler.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
static int i = 0;
void test_fiber() {
    SYLAR_LOG_INFO(g_logger) << "test in fiber";
    sleep(1);
    if(i < 5) {
        sylar::Scheduler::GetThis() -> schedule(&test_fiber);
        //, sylar::GetThreadID()
        ++ i;
    }

}

int main(int argc, char** argv) {
    SYLAR_LOG_INFO(g_logger) << "test start";
    sylar::Scheduler sc(3, true, "test");
    SYLAR_LOG_INFO(g_logger) << "sc start";
    //if(sc.)
    sc.start();
    sc.schedule(&test_fiber);
    sc.stop();
    SYLAR_LOG_INFO(g_logger) << "back to test";
    // long long t = i * 1LL;
    // std::cout << t;
    return 0;
}