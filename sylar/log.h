#ifndef _SYLAR_LOG_H_
#define _SYLAR_LOG_H_
//#pragma once

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <iostream>
#include <stdarg.h>
#include <map>
#include <unordered_map>
#include "util.h"
#include "singleton.h"
#include "thread.h"

#define SYLAR_LOG_LEVEL(logger, level) \
    if(logger -> getLevel() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, __FILE__, __LINE__, 0, sylar::GetThreadID(), sylar::GetFiberID(), time(0), sylar::Thread::GetName()))).getSS()
        
#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)


#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger -> getLevel() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, __FILE__, __LINE__, 0, sylar::GetThreadID(), sylar::GetFiberID(), time(0), sylar::Thread::GetName()))).getEvent() -> format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance() -> getRoot()
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance() -> getLogger(name)


namespace sylar {

class Logger;
class LoggerManager;

class LogLevel {
public:
    enum Level {
        UNKNOW = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5        
    };

    static const char* ToString(LogLevel::Level level);
    static LogLevel::Level FromString(const std::string& str);
};

// 日志事件
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, 
            const char* file, int32_t line, uint32_t elapse, 
            uint32_t thread_id, uint32_t fiber_id, uint64_t time, const std::string& thread_name);
    // ~LogEvent();
  
    const char* getFile() const {return m_file;}
    int32_t getLine() const {return m_line;}
    uint32_t getElapse() const {return m_elapse;}
    uint32_t getThreadId() const {return m_threadId;}
    uint32_t getFiberId() const {return m_fiberId;}
    uint64_t getTime() const {return m_time;}
    const std::string& getThreadName() const {return m_thread_name;}
    std::string getContent() const {return m_ss.str();}
    std::shared_ptr<Logger> getLogger() const {return m_logger;}
    LogLevel::Level getLevel() const {return m_level;}
    std::stringstream& getSS() {return m_ss;}
    /*
        可变参数，在不确定有多少参数的时候可使用，但是至少要保证一个明确的参数
        va_list al, 即所有参数的列表
    */
    void format(const char* fmt, ...);
    void format(const char* fmt, va_list al);
private:
    std::shared_ptr<Logger> m_logger;
    LogLevel::Level m_level;
    const char* m_file = nullptr; // 文件名
    // 内存对齐
    int32_t m_line = 0;           // 行数
    uint32_t m_elapse = 0;        // 程式从启动到现在的毫秒数
    uint32_t m_threadId = 0;     // 线程ID
    uint32_t m_fiberId = 0;      // 协程ID
    uint64_t m_time;            // 时间
    std::string m_thread_name;    // 线程名
    std::stringstream m_ss;      // 内容

    
};

// 在wrap析构的时候把event写进log里
// 这里用wrap的原因是，wrap作为临时对象，在使用完后直接析构，触发日志写入，然而日志本身的智能指针，如果声明在主函数里面，程序不结束就永远无法释放
// 这样写配合文件开头的宏，在宏中if的作用域内构造一个临时的LogEventWrap文件，当我们使用宏去写入日志的时候会产生临时作用域并且通过warp的生命周期完成日志的写入之后析构
class LogEventWrap {
public:
    LogEventWrap(LogEvent::ptr e);
    ~LogEventWrap();
    LogEvent::ptr getEvent() const {return m_event;}
    std::stringstream& getSS();
private:
    LogEvent::ptr m_event;
};

class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    LogFormatter(const std::string& pattern);

    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
    /*
        没有具体实现的父类，之后会实现成不同的格式的输出
        然后全部存在m_items里
    */
public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        // FormatItem(const std::string& fmt = "") {};  // fmt默认就是空的，因为fmt只有在少数特殊的formatterItem实现才有，比如Datetimeformatter
        virtual ~FormatItem() {}
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0; // 因为是多个part（日期行数时间等）组合起来的输出到流里，性能更好
    };

    void init(); // init 方法做Pattern的解析

    bool isError() const {return m_error;}

    const std::string getPattern() const {return m_pattern;}
private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
    bool m_error = false;
};


// 日志输出地
/*
    日志输出地，日志可能有很多输出，比如特定的log文件，或者控制台
    所以把LogAppender 作为一个父基类
*/
class LogAppender {
friend class Logger;
public:
    typedef std::shared_ptr<LogAppender> ptr;
    typedef SpinLock MutexType;
    virtual ~LogAppender(){}

    virtual void log(std::shared_ptr<Logger> logger, LogLevel:: Level level, LogEvent::ptr event) = 0;  // log作为纯虚函数，这样子类必须去实现log方法
    virtual std::string toYamlString() = 0;

    // logformatter的get & set
    void setForMatter(LogFormatter::ptr val);
    //LogFormatter::ptr getFormatter() {return m_formatter;} 
    LogFormatter::ptr getFormatter(); 


    LogLevel::Level getLevel() const {return m_level;}
    void setLevel(LogLevel::Level level) {m_level = level;}

// 修改访问级别成protected，这样子类才可以去访问
protected:
    LogLevel::Level m_level = LogLevel::DEBUG;
    bool m_hasFormatter = false;
    MutexType m_mutex; 
    LogFormatter::ptr m_formatter; // 输出格式，每个不同的输出地可能需要不同的格式
};


// 日志器
/*
    用来纪录日志的, 触发得逻辑是：
    LogEvent 被捕获 -> Logger 纪录 -> 发送到输出地Appender -> 使用不同的Format 对应不同的appender 
*/
class Logger : public std::enable_shared_from_this<Logger> {  // enable shared from this, C++ 11 特性，用来将自己的指针返回，注意class的默认访问级别是private，所以要加上public
friend class LoggerManager;
public:
    // typedef 别名，之后的ptr 就是我们新的定义类型，本质是一个Logger智能指针
    typedef std::shared_ptr<Logger> ptr;
    typedef SpinLock MutexType;
    Logger(const std::string& name = "root");

    /*
        下面是提供某个级别的直接写入方法
    */
    void log(LogLevel::Level level, LogEvent::ptr event);

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    // 增加和删除日志输出地，有些日志的在某个等级下是不会被纪录的，但是有时候如果需要，就可以通过这个方法直接输出到对应的输出地
    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppenders();

    // 该方法加了const，说明该方法不允许修改任何类变量
    LogLevel::Level getLevel() const {return m_level;}
    void setLevel(LogLevel::Level val) {m_level = val;} 

    const std::string& getName() const {return m_name;}

    void setFormatter(LogFormatter::ptr val);
    void setFormatter(const std::string val);
    LogFormatter::ptr getFormatter();

    std::string toYamlString();
private:
    std::string m_name;                         // 日志名称
    LogLevel::Level m_level;                    // 日至等级，只有满足了日志等级才会被append
    MutexType m_mutex;
    std::list<LogAppender::ptr> m_appenders;   // Appender 是一个列表，即我们的输出目的地集合
    LogFormatter::ptr m_formatter;
    Logger::ptr m_root;
};

class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
};

class FileLogAppender : public LogAppender {
public: 
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string& fileName); // fileLogAppender 和别的appender不一样，因为要输出到别的文件里，所以要写入文件名， 必须有参
    void log(Logger::ptr logger, LogLevel::Level Level, LogEvent::ptr event) override;
    std::string toYamlString() override;
    bool reopen(); // 重新打开文件，文件打开成功返回true
private:
    std::string m_fileName;      // 文件名
    std::ofstream m_fileStream;  // 文件流
    uint64_t m_LastTime = 0;
};

class LoggerManager {
public:
    typedef SpinLock MutexType;
    LoggerManager();
    Logger::ptr getLogger(const std::string& name);

    void init();

    Logger::ptr getRoot() const {return m_root;}

    std::string toYamlString();
private:
    MutexType m_mutex;
    std::unordered_map<std::string, Logger::ptr> m_logger;
    Logger::ptr m_root;
};

typedef Singleton<LoggerManager> LoggerMgr;

} 

#endif


