#include "sylar/sylar.h"
#include "sylar/thread.h"
#include <unistd.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int count = 0;
//sylar::RWMutex s_mutex;
sylar::Mutex s_mutex;

void fun1() {
    SYLAR_LOG_INFO(g_logger) << "name: " << sylar::Thread::GetName()
                             << " this.name: " << sylar::Thread::GetThis() -> getName()
                             << " id: " << sylar::GetThreadID()
                             << " this.id: " << sylar::Thread::GetThis() -> getId(); 
    
    // sleep(20);
    for(int i = 0; i < 1000000; ++i) {
        //sylar::RWMutex::WriteLock test_lock(s_mutex);
        sylar::Mutex::Lock test_lock(s_mutex);
        ++count;
    }
}

void fun2() {
    while(true) {
        SYLAR_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    }
}   

void fun3() {
    while(true) {
        SYLAR_LOG_INFO(g_logger) << "=======================================================";
    }
}

int main(int argc, char** argv) {
    SYLAR_LOG_INFO(g_logger) << "-------------Thread test begin-------------------";
    YAML::Node root = YAML::LoadFile("/root/workspcae/sylar/bin/conf/log2.yml");
    sylar::Config::LoadFromYaml(root);
    std::vector<sylar::Thread::ptr> thrds;
    for(int i = 0; i < 2; ++i) {
        // sylar::Thread::ptr thrd = std::make_shared<sylar::Thread>(&fun1, "name_" + std::to_string(i));
        sylar::Thread::ptr thrd(new sylar::Thread(&fun2, "name_" + std::to_string(i)));
        sylar::Thread::ptr thrd2(new sylar::Thread(&fun3, "name_" + std::to_string(i)));
        thrds.push_back(thrd);
        thrds.push_back(thrd2);

    }

    //sleep(10);

    for(size_t i = 0; i < thrds.size(); ++i) {
        thrds[i] -> join();
    }

    SYLAR_LOG_INFO(g_logger) << "-------------Thread test end-------------------";
    SYLAR_LOG_INFO(g_logger) << "count = " << count;
    return 0;
}