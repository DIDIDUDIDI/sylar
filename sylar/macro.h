#ifndef __SYLAR_MACRO_H__
#define __SYLAR_MACRO_H__

#include <string.h>
#include "util.h"
#include <assert.h>

#if defined __GUNC__ || defined __llvm__
#   define SYLAR_LICKLY(x)     __buitin_expect(!!(x), 1)
#   define SYLAR_UNLICKLY(x)     __buitin_expect(!!(x), 0)
#else
#   define SYLAR_LICKLY(x)      (x)
#   define SYLAR_UNLICKLY(x)    (x) 
#endif

#define SYLAR_ASSERT(x) \
    if(SYLAR_UNLICKLY(!(x))) { \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x \
                                          << "\nbacktrace:\n" \
                                          << sylar::BacktraceToString(100, 0, "    "); \
        assert(x); \
    }

#define SYLAR_ASSERT2(x, w) \
    if(SYLAR_UNLICKLY(!(x))) { \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x \
                                          << "\n" << w \
                                          << "\nbacktrace:\n" \
                                          << sylar::BacktraceToString(100, 0, "    "); \
        assert(x); \
    }

#endif