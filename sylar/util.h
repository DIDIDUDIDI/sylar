#ifndef __SYLAR_UTIL_H__
#define __SYLAR_UTIL_H__

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <sys/time.h>

namespace sylar{
    pid_t GetThreadID();
    uint32_t GetFiberID();

    void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1);
    std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "");
    
    // 时间ms
    uint64_t GetCurrentMS();
    uint64_t GetCurrentUS();

}

#endif