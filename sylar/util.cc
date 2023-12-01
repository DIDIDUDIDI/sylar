#include "util.h"
#include "log.h"
#include "fiber.h"
#include <execinfo.h>

namespace sylar {
    static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    pid_t GetThreadID() {
        return syscall(SYS_gettid);
    }

    uint32_t GetFiberID() {
        return Fiber::GetFiberID();
    }

    /*
        void** array = (void**)malloc((sizeof(void*) * size));

        这行代码分配了一个void指针数组（void**），该数组用于存储回溯信息中的函数指针。malloc函数用于在堆上分配内存，sizeof(void*) * size计算需要多少字节的内存，然后将指针转换为void**类型。这个数组将被用来存储回溯信息的函数指针。
        char** strings = backtrace_symbols(array, s);

        这行代码调用了backtrace_symbols函数，该函数用于将函数指针数组array中的信息转化为字符串数组（char**）。这些字符串表示函数调用栈中的每个函数。strings将存储这些字符串。
        void**和char**是指针类型。void**是指向void指针的指针，而char**是指向char指针的指针。在这里，void**用于存储函数指针数组，而char**用于存储函数调用栈中的字符串。

        size_t s = ::backtrace(array, size);

        ::是C++中的作用域解析操作符，用于表示全局命名空间。在这里，::backtrace表示调用全局命名空间中的backtrace函数。backtrace函数用于获取回溯信息，将其存储在array中，返回的回溯信息的数量存储在s中。
        关于backtrace和backtrace_symbols的方法：

        backtrace函数用于获取程序的回溯信息，即函数调用栈的信息。它接受两参数：一个指向void*指针数组的指针，以及一个表示数组的大小的整数。它会填充这个数组，记录函数指针的地址，并返回实际获取的回溯信息数量。

        backtrace_symbols函数用于将函数指针数组转化为函数调用栈中每个函数的可读字符串表示。它接受两参数：一个指向函数指针数组的指针，以及回溯信息的数量。它会分配内存来存储字符串，并返回一个指向字符串数组的指针。这些字符串通常包括函数名、文件名、行号等信息，以帮助调试。
    */
    void Backtrace(std::vector<std::string>& bt, int size, int skip) {
        void** array = (void**)malloc((sizeof(void*) * size));
        size_t s = ::backtrace(array, size);

        char** strings = backtrace_symbols(array, s);
        if(strings == NULL) {
            SYLAR_LOG_ERROR(g_logger) << "backtrace_symbols error.";
            free(array);
            return;
        }

        /*
            注意strings 是char*类型的指针，所以要loop完才能拿到所有的结果，
            直接输出只会输出指针的第一位，信息是不完整的
        */
        for(size_t i = skip; i < s; ++ i) {
            bt.push_back(strings[i]);
        }
        free(array);
        free(strings);
    }

    std::string BacktraceToString(int size, int skip, const std::string& prefix) {
        std::vector<std::string> bt;
        Backtrace(bt, size, skip);
        std::stringstream ss;
        for(size_t i = 0; i < bt.size(); ++i) {
            ss << prefix << bt[i] << std::endl;
        }
        return ss.str();
    }

    uint64_t GetCurrentMS() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
    }
    
    uint64_t GetCurrentUS() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
    }
}