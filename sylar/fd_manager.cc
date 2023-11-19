#include "fd_manager.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hook.h"

namespace sylar {
    /*
            bool m_isInit: 1;
            bool m_isSocket: 1;
            bool m_sysNonblock: 1;
            bool m_userNonblock: 1;
            bool m_isClosed: 1;
            int m_fd;

            uint64_t m_recvTimeout;     // type 0
            uint64_t m_sendTimeout;     // type 1

            IOManager* m_iomanager;
    */
    FdCtx::FdCtx(int fd) 
        :m_isInit(false),
         m_isSocket(false),
         m_sysNonblock(false),
         m_userNonblock(false),
         m_isClosed(false),
         m_fd(fd),
         m_recvTimeout(-1),
         m_sendTimeout(-1) {
        init();
    }
    FdCtx::~FdCtx() {

    }

    bool FdCtx::init() {
        if(m_isInit) {
            return true;
        }
        m_recvTimeout = -1;
        m_sendTimeout = -1;
        
        // 判断句柄是否是socket的请求
        struct stat fd_stat;
        if(-1 == fstat(m_fd, &fd_stat)) {
            m_isInit = false;
            m_isSocket = false;
        } else {
            m_isInit = true;
            m_isSocket = S_ISSOCK(fd_stat.st_mode);
        }

        // 如果是
        if(m_isSocket) {
            // 使用原本的方法拿到flags, 注意flags是一个掩码
            int flags = fcntl_f(m_fd, F_GETFL, 0);
            if(! (flags & O_NONBLOCK)) {    // 之后设置成非阻塞
                fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
            }
            m_sysNonblock = true;
        } else {
            m_sysNonblock = false;
        }

        m_userNonblock = false;
        m_isClosed = false;
        return m_isInit;
    }

    bool FdCtx::close() {

    }

    void FdCtx::setTimeout(int type, uint64_t v) {

    }
    uint64_t getTimeout(int type);

    FdManager::FdManager() {
        m_datas.resize(64);
    }

    FdCtx::ptr get(int fd, bool auto_create = false);       // 如果当前的句柄存在我们就返回，要不然就创建一个
    void del(int fd);
}