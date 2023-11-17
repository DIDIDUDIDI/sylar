#include "log.h"
#include <functional>
#include <time.h>
#include <string.h>
#include "config.h"

#define SlyarVerion 1

namespace sylar{

// LOGLEVEL 模块 实现
    const char* LogLevel::ToString(LogLevel::Level level) {
        switch (level)
        {
        /*
            宏定义：这里定义了一个 叫做XX的宏，其中name是参数
            \ 表示换行，因为在宏定义里必须是一行，通过 \换行可以增加可读性， 所以该宏的原本是： #define XX(name) case LogLevel::name: return #name; break;
            下面我们call这个XX宏，将name替换成不同的string，这样就可以快速写出不同的Switch case，其中#name意思是按照string 返回
            这段宏的理解类似： 
            #define log(x) std::cout << x << std::endl 
            log("hello world"); -> std::cout << "hello world" << std::endl;
            该宏通过XX(NAME) 后预编译为：

            case LogLevel::DEBUG:
                return "DEBUG";
                break;
        
            case LogLevel::INFO:
                return "INFO";
                break;

            case LogLevel::WARN:
                return "WARN";
                break;
            
            case LogLevel::ERROR:
                return "ERROR";
                break;
            case LogLevel::FATAL:
                return "FATAL";
                break;

        */
#define XX(name) \
        case LogLevel::name: \
            return #name; \
            break;
        
        XX(DEBUG);
        XX(INFO);
        XX(WARN);
        XX(ERROR);
        XX(FATAL);

#undef XX
        default:
            return "UNKNOW";
        }
    }

    LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, name) \
        if(str == #name)\
        {\
            return LogLevel::level;\
        }
        XX(DEBUG, debug);
        XX(INFO, info);
        XX(WARN, warn);
        XX(ERROR, error);
        XX(FATAL, fatal);

        XX(DEBUG, DEBUG);
        XX(INFO, INFO);
        XX(WARN, WARN);
        XX(ERROR, ERROR);
        XX(FATAL, FATAL);

        return LogLevel::Level::UNKNOW;
#undef XX
    }

    LogEventWrap::LogEventWrap(LogEvent::ptr e) : m_event(e) {

    }
    LogEventWrap::~LogEventWrap() {
        m_event -> getLogger() -> log(m_event -> getLevel(), m_event);
    }

    std::stringstream& LogEventWrap::getSS() {
        return m_event -> getSS();
    }

    
    /*
        从可变参数列表中收集参数，然后写入stringstream里
    */
    void LogEvent::format(const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        format(fmt, ap);
        va_end(ap);
    }
    void LogEvent::format(const char* fmt, va_list al) {
        char* buff = nullptr;
        int len = vasprintf(&buff, fmt, al);
        if(len != -1) {
            m_ss << std::string(buff, len);
            free(buff);
        }
    }

    // 日志格式器
    /*
        对于不同的事件，我们有不同的样板（formatItem）输出
        在appender那里选择对应的pattern，然后从我们的m_items 中找到我们的FormatItem输出
    */
    class MessageFormatItem : public LogFormatter::FormatItem {
    public:
        //MessageFormatItem(const std::string& fmt) : FormatItem(fmt) {}
        MessageFormatItem(const std::string& fmt= "") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event -> getContent();
        }
    };
    class LevelFormatItem : public LogFormatter::FormatItem {
        public:
            //LevelFormatItem(const std::string& fmt) : FormatItem(fmt) {}
            LevelFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << LogLevel::ToString(level);
            }
    };
    class ElapseFormatItem : public LogFormatter::FormatItem {
        public:
            //ElapseFormatItem(const std::string& fmt) : FormatItem(fmt) {}
            ElapseFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << event -> getElapse();
            }
    };
    class NameFormatItem : public LogFormatter::FormatItem {
        public:
            //NameFormatItem(const std::string& fmt) : FormatItem(fmt) {}
            NameFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << event -> getLogger() -> getName();
            }
    };

    class ThreadIdFormatItem : public LogFormatter::FormatItem {
        public:
            //ThreadIdFormatItem(const std::string& fmt) : FormatItem(fmt) {}
            ThreadIdFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << event -> getThreadId();
            }
    };
    class ThreadNameFormatItem : public LogFormatter::FormatItem {
        public:
            //ThreadIdFormatItem(const std::string& fmt) : FormatItem(fmt) {}
            ThreadNameFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << event -> getThreadName();
            }
    };
    class FiberIdFormatItem : public LogFormatter::FormatItem {
        public:
            //FiberIdFormatItem(const std::string& fmt) : FormatItem(fmt) {}
            FiberIdFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << event -> getFiberId();
            }
    };
    class DateTimeFormatItem : public LogFormatter::FormatItem {
        public:
            //DateTimeFormatItem(const std::string& format = "%Y:%m:%d %H:%M:%S") : m_format(format) {}
            DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S") : m_format(format) {
                if(format.empty()) {
                    m_format = "%Y-%m-%d %H:%M:%S";
                }
            }
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                struct tm tm;
                time_t time = event -> getTime();
                localtime_r(&time, &tm);
                char buf[64];
                strftime(buf, sizeof(buf), m_format.c_str(), &tm);
                os << buf;
            }
        private:
            std::string m_format;
    };
    class FilenameFormatItem : public LogFormatter::FormatItem {
        public:
            //FilenameFormatItem(const std::string& fmt) : FormatItem(fmt) {}
            FilenameFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << event -> getFile();
            }
    };

    class LineFormatItem : public LogFormatter::FormatItem {
        public:
            //LineFormatItem(const std::string& fmt) : FormatItem(fmt) {}
            LineFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << event -> getLine();
            }
    };

    class NewLineFormatItem : public LogFormatter::FormatItem {
        public:
            // NewLineFormatItem(const std::string& fmt) : FormatItem(fmt) {}
            NewLineFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << std::endl;
            }
    };

    class StringFormatItem : public LogFormatter::FormatItem {
        public:
            StringFormatItem(const std::string& fmt) : m_string(fmt) {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << m_string;
            }
        private:
            std::string m_string;
    };

    class TabFormatItem : public LogFormatter::FormatItem {
        public:
            TabFormatItem(const std::string& fmt = "") {}
            void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
                os << "\t";
            }
    };

    LogEvent::LogEvent (std::shared_ptr<Logger> logger, LogLevel::Level level,
     const char* file, int32_t line, uint32_t elapse,
      uint32_t thread_id, uint32_t fiber_id, uint64_t time, const std::string& thread_name) 
    : m_logger(logger), m_level(level), m_file(file), m_line(line), m_elapse(elapse), m_threadId(thread_id), m_fiberId(fiber_id), m_time(time), m_thread_name(thread_name)
     {

    }

// LOGGER 模块 实现
    Logger::Logger(const std::string& name) : m_name(name), m_level(LogLevel::DEBUG) {
        m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n")); // sharded pointer reset，就是给sharded pointer 赋默认值

        // if(name == "root") {
        //     m_appenders.push_back(LogAppender::ptr(new StdoutLogAppender));
        // }
    }

    void Logger::addAppender(LogAppender::ptr appender) {
        MutexType::Lock lock(m_mutex);
        if(! appender->getFormatter()) {
            MutexType::Lock ll(appender -> m_mutex);
            appender-> m_formatter = m_formatter; // Logger 下面add appender的方法，如果这个appender没有formatter，那么我们把logger自己的formatter给它，logger的构造函数至少保证一个默认fomatter
        }
        m_appenders.push_back(appender);
    }

    void Logger::delAppender(LogAppender::ptr appender) {
        MutexType::Lock lock(m_mutex);
        // 使用 pre-increment ++i 更好，因为i ++ 是有复制操作在里面的, 因为 i ++ 实际上是 i = i + 1的缩写，所以最后的这个i是一个复制的左值，但是 ++ i是i自增，属于右值操作
        for(auto it = m_appenders.begin(); it != m_appenders.end(); ++ it) {
            if(*it == appender) {
                m_appenders.erase(it);
                break;
            }
        }
    }

    void Logger::clearAppenders() {
        m_appenders.clear();
    }

    void Logger::setFormatter(LogFormatter::ptr val) {
        MutexType::Lock lock(m_mutex);
        m_formatter =  val;
        for(auto& i : m_appenders) {
            MutexType::Lock ll(i -> m_mutex);
            if(! i -> m_hasFormatter) {
                i -> m_formatter = m_formatter;
            }
        }
    }
    void Logger::setFormatter(const std::string val) {
        LogFormatter::ptr logformat(new sylar::LogFormatter(val));
        if(logformat -> isError()) {
            std::cout << "Logger setFormatter name = " << m_name
                      << ", value = " << val << " is invalid." << std::endl;
            return;
        }
        // m_formatter = logformat;
        setFormatter(logformat);
    }

    LogFormatter::ptr Logger::getFormatter() {
        MutexType::Lock lock(m_mutex);
        return m_formatter;
    }

    std::string Logger::toYamlString() {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        node["name"] = m_name;
        if(m_level != LogLevel::UNKNOW) {
            node["level"] = LogLevel::ToString(m_level);
        }
        if(m_formatter) {
            node["formatter"] = m_formatter -> getPattern();
        }
        for(auto& i : m_appenders) {
            // YAML::Node n;
            // n = YAML::Load(i -> toYamlString()); 
            // node["appender"] .push_back(n);
            node["appender"].push_back(YAML::Load(i -> toYamlString()));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
        /*
            如果当前的事件等级超过我们的设定等级的时候，我们就遍历所有的输出地，然后写入该事件
            比如目前我的m_level 是Info，即2，如果发生了WARN级别的事件，那么所有的输出地都改被纪录这个事件
            但是如果发生了DEBUG级别的事件，那么久全部不会纪录
        */
        if(level >= m_level) {
            auto self = shared_from_this();
            MutexType::Lock lock(m_mutex);
            if(!m_appenders.empty()) {
                for(auto& appender : m_appenders) {
                    appender -> log(self, level, event);
                }
            }else if(m_root) {
                m_root -> log(level, event);
            }
        }
    }

    
    

    void Logger::debug(LogEvent::ptr event) {
        log(LogLevel::DEBUG, event);
    }
    void Logger::info(LogEvent::ptr event) {
        log(LogLevel::INFO, event);
    }
    void Logger::warn(LogEvent::ptr event) {
        log(LogLevel::WARN, event);
    }
    void Logger::error(LogEvent::ptr event) {
        log(LogLevel::ERROR, event);
    }
    void Logger::fatal(LogEvent::ptr event) {
        log(LogLevel::FATAL, event);
    }

    /*
        关于日志配置上锁的问题，为什么复杂类型要上锁，而类似Level这种基础的类型就不用呢？
        原因是基础类型都是原子性操作，不上锁最多出现是返回值不准的问题
        但是如果是复杂类型，一个类型中包含多个变量，有可能线程A在对其进行操作的时候，比如只修改了变量a，
        但是没有修改到变量b
        线程B就访问读取，这样会造成严重的内存错误。
    */
    void LogAppender::setForMatter(LogFormatter::ptr val) {
        MutexType::Lock lock(m_mutex);
        m_formatter = val;
        if(m_formatter) {
            m_hasFormatter = true;
        }else{
            m_hasFormatter = false;
        }
    }

    LogFormatter::ptr LogAppender::getFormatter() {
        MutexType::Lock lock(m_mutex);
        return m_formatter;
    }

// LogAppender 模块 实现
    FileLogAppender::FileLogAppender(const std::string& fileName) : m_fileName(fileName) {
        reopen();
    }

    void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
        if(level >= m_level) {
            uint64_t currTime = time(0);
            if(currTime != m_LastTime) {
                reopen();
                m_LastTime = currTime;
            }
           MutexType::Lock lock(m_mutex);
           m_fileStream << m_formatter->format(logger, level, event);
        }
    }
    
    std::string FileLogAppender::toYamlString() {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        node["type"] = "FileLogAppender";
        node["file"] = m_fileName;
        if(m_level != LogLevel::UNKNOW) {
            node["level"] = LogLevel::ToString(m_level);
        }
        if(m_hasFormatter && m_formatter) {
            node["formatter"] = m_formatter -> getPattern();
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    bool FileLogAppender::reopen() {
        MutexType::Lock lock(m_mutex);
        if(m_fileStream) {
            m_fileStream.close();
        }
        m_fileStream.open(m_fileName);
        return !!m_fileStream; // !! 将非0值转成1， 0 还是0，本质是两个！， 如果大于1第一个会返回false，第二个则会返回true
    }

    void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)  {
        if(level >= m_level) {
            MutexType::Lock lock(m_mutex);
            std::cout << m_formatter->format(logger, level, event);
        }
    }
    
    std::string StdoutLogAppender::toYamlString() {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        node["type"] = "StdoutLogAppender";
        if(m_level != LogLevel::UNKNOW) {
            node["level"] = LogLevel::ToString(m_level);
        }
        if(m_hasFormatter && m_formatter) {
            node["formatter"] = m_formatter -> getPattern();
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

// LogFormatter 模块实现
    /*  
        构造方法输入一个pattern然后
    */
    LogFormatter::LogFormatter(const std::string& pattern) : m_pattern(pattern) {
        init();
    }

    /*  
        组装，用stringstream性能更加，将解析好的格式从m_item里取出来拼接成普通的字符串
    */
    std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
        std::stringstream ss;
        for(auto& i : m_items) {
            i->format(ss, logger, level, event);
        }
        return ss.str();
    }

    /*
        日志格式的解析，每个解析类型写入m_items
        特殊的类型有：
        %d   类型d
        %d{xx:xx:xx} 类型d, 按照xx:xx:xx的方式输出
        %%   转义，就是一个%
    */
    void LogFormatter::init() {
#if SlyarVerion == 1
    /*
        str, pattern, type
        @str, 在type = 0 的时候表示正常的日志内容， type = 1的时候表示类型
        @pattern，在type = 0的为空， type = 1 的时候表示对于类型的输出格式
        @type = 0 表示存储正常的日志内容，type = 1的时候表示存储的输出格式的信息
    */
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr; // 正常的内容
    for(size_t i = 0; i < m_pattern.size(); ++ i) {
        // 正常输入
        if(m_pattern[i] != '%') {
            nstr.append(1, m_pattern[i]);
            continue;
        }
        // 遇到转义的%
        if(i + 1 < m_pattern.size() && m_pattern[i + 1] == '%') {
            nstr.append(1, '%');
            continue;
        }

        // 遇到了%， 检查输出类型，以及是否需要特殊的格式，即检查{}内的内容
        std::string str; // 输出的类型
        std::string fmt;

        int fmt_status = 0; // 检查是否存在左括号
        size_t fmt_begin = 0; // 左括号的起点位置
        size_t n = i + 1; // 跳过%直接开始检查

        while(n < m_pattern.size()) {
            // 没有左括号，并且当前字符不是字母，也不是左右括号的任何一个
            if(!fmt_status && (!isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}')) {
                // 截断，str是我们的类型
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            }
            // 在没有左括号的情况下
            if(fmt_status == 0) {
                // 碰到了，直接进入类型匹配
                if(m_pattern[n] == '{') {
                    str = m_pattern.substr(i + 1, n - i - 1); // 说明之前的那一块是我们的类型
                    fmt_status = 1; // 进入检查格式
                    fmt_begin = n;  // 设置格式的起点位置，方便之后遇到右括号截取内容
                    ++ n;
                    continue;
                }
            } else if(fmt_status == 1) { // 如果有左括号
                if(m_pattern[n] == '}') {
                    // 说明期间的使我们的格式
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    fmt_status = 0; // reset
                    ++ n;
                    break;
                }
            }
            ++ n;
            if(n == m_pattern.size()) { // 到达了末尾，直接截取
                if(str.empty()) {
                    str = m_pattern.substr(i + 1, n - i - 1);
                }
            }
        }

        // 有效
        if(fmt_status == 0) {
            // 先push之前的信息
            if(! nstr.empty()) {
                vec.emplace_back(std::make_tuple(nstr, "", 0));
                nstr.clear();
            }
            // push下一轮格式信息
            vec.emplace_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        } else if(fmt_status == 1) {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            m_error = true; 
            vec.emplace_back(std::make_tuple("<<pattern error>>", fmt, 0));
        }
    }
    // 说明走到底了，但是nstr还是有内容，直接push进去，因为正常是在下一轮的格式纪录完成后才push上一轮的nstr内容，所以要做一个边界检查
    if(! nstr.empty()) {
        vec.emplace_back(std::make_tuple(nstr, "", 0));
    }

    /*
        制作一个map<string, 返回具体FormatItem类型指针的方法，利用继承的特性，返回对应的具体实现>
        原本的写法是：
        #include <map>
        #include <string>
        #include <functional>

        class FormatItem {
        public:
            using ptr = std::shared_ptr<FormatItem>;

            FormatItem(const std::string& fmt) {
                // Initialize FormatItem based on the format string (fmt)
            }

            // Add necessary functions for FormatItem subclasses
        };

        class MessageFormatItem : public FormatItem {
        public:
            MessageFormatItem(const std::string& fmt) : FormatItem(fmt) {
                // Initialize MessageFormatItem based on the format string (fmt)
            }
        };

        class LevelFormatItem : public FormatItem {
        public:
            LevelFormatItem(const std::string& fmt) : FormatItem(fmt) {
                // Initialize LevelFormatItem based on the format string (fmt)
            }
        };

        // Define other FormatItem subclasses similarly

        static std::map<std::string, std::function<FormatItem::ptr(const std::string&)>> s_format_items = {
            {"m", [](const std::string& fmt) {return FormatItem::ptr(new MessageFormatItem(fmt));}},
            {"p", [](const std::string& fmt) { return std::make_shared<LevelFormatItem>(fmt); }},
            // Define other format items similarly
        };
        之后通过宏来简化代码生成
        其中#str表示将其string化
        C表示Class

        TODO Test: Sylar 使用的是map，这里我用Unordered_map, 下面push 进m_item的方式也不一样
        原因是因为该map只会建立一次，但是会经常查找，所以用unordered map的效率会比较高，也比较省内存
    */
    static std::unordered_map<std::string, std::function<FormatItem::ptr(const std::string& fmt)>> s_format_items = {
#define XX(str, C) \
            {#str, [](const std::string& fmt) {return FormatItem::ptr(new C(fmt));}}
        
        XX(m, MessageFormatItem),
        XX(p, LevelFormatItem),
        XX(r, ElapseFormatItem),
        XX(c, NameFormatItem),
        XX(t, ThreadIdFormatItem),
        XX(n, NewLineFormatItem),
        XX(d, DateTimeFormatItem),
        XX(f, FilenameFormatItem),
        XX(l, LineFormatItem),
        XX(T, TabFormatItem),
        XX(F, FiberIdFormatItem),
        XX(N, ThreadNameFormatItem)
#undef XX    
    };

    
    //Sylar 实现：
        // for(auto& i : vec) {
        //     if(std::get<2>(i) == 0) { // 说明是一个内容
        //         m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i)))); // 只push进第一个内容，即日志内容
        //     } else { // 非0，说明是个格式
        //         auto it = s_format_items.find(std::get<0>(i)); // loop map
        //         if(it == s_format_items.end()) { // loop完map都没有找到对应的类型，报错
        //             m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
        //             m_error = true;
        //         } else { // 写入类型
        //             m_items.push_back(it->second(std::get<1>(i)));
        //         }
        //     }
        //     //std::cout << "(" << std::get<0>(i) << ") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ")" << std::endl;
        // }
    
    for(auto& i : vec) {
        if(std::get<2>(i) == 0) {
            m_items.emplace_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
        }else{
            if(s_format_items.count(std::get<0>(i)) == 0) {
                m_error = true;
                m_items.emplace_back(FormatItem::ptr(new StringFormatItem("<<[Error Pattern]: %" + std::get<0>(i) + ">>")));
            }else{
                m_items.emplace_back(s_format_items[std::get<0>(i)](std::get<1>(i))); // 注意我们的value在map中是一个lambda函数，需要接受一个string:fmt 作为参数，因此我们根据类型找到value后还是要把参数塞进去的
            }
        }
    }

#else
    //TODO: try to re-write DIDI Version
#endif
    } 

    LoggerManager::LoggerManager() {
        m_root.reset(new Logger);
        m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));

        m_logger[m_root -> m_name] = m_root;

        init();
    }

    Logger::ptr LoggerManager::getLogger(const std::string& name) {
        MutexType::Lock lock(m_mutex);
        if(m_logger.count(name) != 0) {
            return m_logger[name];
        }
        Logger::ptr logger(new Logger(name));
        logger -> m_root = m_root;
        m_logger[name] = logger;
        return logger;
    }

    struct LogAppenderDefine {
        int type = 0; // File = 1, stdOut = 2;
        LogLevel::Level level = LogLevel::UNKNOW;
        std::string formatter;
        std::string file;

        bool operator== (const LogAppenderDefine& oth) const {
            return (type == oth.type) && (level == oth.level) && (formatter == oth.formatter) && (file == oth.file);
        }
    };

    struct LogDefine {
        std::string name;
        LogLevel::Level level = LogLevel::UNKNOW;
        std::string formatter;
        std::vector<LogAppenderDefine> appenders;

        bool operator== (const LogDefine& oth) const {
            return (name == oth.name) && (level == oth.level) && (formatter == oth.formatter) && (appenders == oth.appenders);
        }

        /*
            为什么要重载 < ? 
            因为我们的ConfigVar使用set去存储，如果我们要使用find的时候，就是走这个< 符号的
            为什么不是 == ？
            因为set中是不存在相同的元素，所以我们和find的元素对比，就可以知道是在前面还是后面了
        */
        bool operator< (const LogDefine& oth) const {
            return name < oth.name;
        }
    };

    template<>
    class LexicalCast<std::string, LogDefine> {
    public:
        LogDefine operator() (const std::string& v) {
            YAML::Node node = YAML::Load(v);
            LogDefine ld;
            // if(! node["name"].IsDefined()) {
            //     std::cout << "log config error: name is null" << node << std::endl;
            //     return ld;
            // }
            ld.name = node["name"].IsDefined() ? node["name"].as<std::string>() : "";
            ld.level = LogLevel::FromString(node["level"].IsDefined() ? node["level"].as<std::string>() : "");
            if(node["formatter"].IsDefined()) {
                ld.formatter = node["formatter"].as<std::string>();
            }
            if(node["appender"].IsDefined()) {
                for(size_t i = 0; i < node["appender"].size(); ++ i) {
                    auto a = node["appender"][i];
                    if(! a["type"].IsDefined()) {
                        std::cout << "log config error: Appender type is null" << a << std::endl;
                        continue;
                    }
                    std::string type = a["type"].as<std::string>();
                    LogAppenderDefine lad;
                    if(type == "FileLogAppender") {
                        lad.type = 1;
                        if(!a["file"].IsDefined()) {
                            std::cout << "log config error: FileAppender file is null " << a << std::endl;
                            continue;
                        }
                        lad.file = a["file"].as<std::string>();
                        if(a["formatter"].IsDefined()) {
                            lad.formatter = a["formatter"].as<std::string>();
                        }
                    } else if(type == "StdoutLogAppender") {
                        lad.type = 2;
                    } else {
                        std::cout << "log config error: Appender type is invalid" << type << std::endl;
                        continue;
                    }
                    // 记得pushback
                    ld.appenders.push_back(lad);
                }
            }
            return ld;
        }
    };

    template<>
    class LexicalCast<LogDefine, std::string> {
    public:
        std::string operator() (const LogDefine& ld) {
            YAML::Node node;
            node["name"] = ld.name;
            if(ld.level != LogLevel::UNKNOW) {
                node["level"] = LogLevel::ToString(ld.level);
            }
            if(!ld.formatter.empty()) {
                node["formatter"] = ld.formatter;
            }
            for(auto& i : ld.appenders) {
                YAML::Node n;
                if(i.type == 1) {
                    n["type"] = "FileLogAppender";
                    n["file"] = i.file; 
                }else if(i.type == 2) {
                    n["type"] = "StdoutLogAppender";
                }
                if(i.level != LogLevel::UNKNOW) {
                    n["level"] = LogLevel::ToString(i.level);
                }
                if(!i.formatter.empty()) {
                    node["formatter"] = i.formatter;
                }
                node["appender"].push_back(n);
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    sylar::ConfigVar<std::set<LogDefine> >::ptr g_log_defines = sylar::Config::Lookup("logs", std::set<LogDefine>(), "log config");
    
    struct LogIniter {
        LogIniter() {
            g_log_defines->addListener([](const std::set<LogDefine> old_value, const std::set<LogDefine> new_value) {
                SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "on_logger_conf_changed";
                for(auto& i : new_value) {
                    auto it = old_value.find(i);
                    sylar::Logger::ptr Logger;
                    if(it == old_value.end()) {
                        // 新增
                        //Logger.reset(new sylar::Logger(i.name));
                        Logger = SYLAR_LOG_NAME(i.name);
                    } else {
                        // 修改
                        // 我们只是重载了 == 所以只能这么写
                        if(!(i == *it)) {
                            Logger = SYLAR_LOG_NAME(i.name);
                        }
                    }
                    Logger -> setLevel(i.level);
                    if(! i.formatter.empty()) {
                        Logger -> setFormatter(i.formatter);
                    }
                    Logger -> clearAppenders();
                    for(auto& j : i.appenders) {
                        sylar::LogAppender::ptr app;
                        if(j.type == 1) {
                            app.reset(new FileLogAppender(j.file));
                        }else if(j.type == 2) {
                            app.reset(new StdoutLogAppender); 
                        }
                        app->setLevel(j.level);

                        if(! j.formatter.empty()) {
                            LogFormatter::ptr fmt(new LogFormatter(j.formatter));
                            if(! fmt -> isError()) {
                                app -> setForMatter(fmt);
                            }else{
                                std::cout << "appender name = " << i.name 
                                          << "formatter = " << j.formatter << " is invalid." << std::endl;
                            }
                        }
                        Logger->addAppender(app);
                    }    
                }
                
                for(auto& i : old_value) {
                    auto it = new_value.find(i);
                    if(it == new_value.end()) {
                        auto logger = SYLAR_LOG_NAME(i.name);
                        // 删除
                        // 并非真正的删除，而是给他设置一个无法写入的等级, 同时清空appenders
                        logger -> setLevel((LogLevel::Level)100); 
                        logger -> clearAppenders();
                    }
                }
            });
        }
    };
    

    // static 在main 函数之前就会执行
    static LogIniter __log_init;
    
    std::string LoggerManager::toYamlString() {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        for(auto& i : m_logger) {
            node.push_back(YAML::Load(i.second -> toYamlString()));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    void LoggerManager::init() {

    }
    

}

 