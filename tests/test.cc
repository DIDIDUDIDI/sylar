#include <iostream>
#include "sylar/log.h"
#include "sylar/util.cc"
//include <thread>
//#include "sylar/person.h"

int main(int argc, char** argv) {
    std::cout << "build success" << std::endl;
    sylar::Logger::ptr logger(new sylar::Logger);
    logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));

    //sylar::LogEvent::ptr event(new sylar::LogEvent(__FILE__, __LINE__, 0, std::hash<std::thread::id>{}(std::this_thread::get_id()), 2, time(0)));
    // sylar::LogEvent::ptr event(new sylar::LogEvent(logger, sylar::LogLevel::DEBUG, __FILE__, __LINE__, 0, sylar::GetThreadID(), sylar::GetFiberID(), time(0)));
    // event->getSS() << "test";
    // logger->log(sylar::LogLevel::DEBUG, event);  
    // macro test 
   
    sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("./log.txt"));
    sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%p%T%m%n"));
    file_appender->setForMatter(fmt);
    file_appender->setLevel(sylar::LogLevel::ERROR);
    logger->addAppender(file_appender);
   

    SYLAR_LOG_INFO(logger) << "test macro";
    SYLAR_LOG_ERROR(logger) << "test macro error";

    SYLAR_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");
    
    
    auto l = sylar::LoggerMgr::GetInstance() -> getLogger("xxxxx");
    SYLAR_LOG_DEBUG(l) << "xxx";
    
    std::cout << "end test" << std::endl;

    return 0;
}