#include "fd_manager.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hook.h"

namespace sylar {
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
        struct stat fd_stat;                // 
        if(-1 == fstat(m_fd, &fd_stat)) {   // 检查fd是否关闭
            m_isInit = false;
            m_isSocket = false;
        } else {                            // 未关闭，那么走我们hook
            m_isInit = true;
            m_isSocket = S_ISSOCK(fd_stat.st_mode); // 是否是socket
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

    // bool FdCtx::close() {

    // }

    void FdCtx::setTimeout(int type, uint64_t v) {      //设置句柄的超时函数
        if(type == SO_RCVTIMEO) {
            m_recvTimeout = v;
        } else {
            m_sendTimeout = v;
        }

    }
    uint64_t FdCtx::getTimeout(int type) {
        if(type == SO_RCVTIMEO) {
            return m_recvTimeout;
        } else {
            return m_sendTimeout;
        }
  }

    FdManager::FdManager() {
        m_datas.resize(64);
    }

    FdCtx::ptr FdManager::get(int fd, bool auto_create) {       // 如果当前的句柄存在我们就返回，要不然就创建一个
        RWMutexType::ReadLock lock(m_mutex);
        if((int)m_datas.size() <= fd) {
            if(! auto_create) {
                return nullptr;
            }
        } else {
            if(m_datas[fd] || !auto_create) {
                return m_datas[fd];
            }
        }
        lock.unlock();

        RWMutexType::WriteLock lock2(m_mutex);
        FdCtx::ptr ctx(new FdCtx(fd));
        m_datas[fd] = ctx;
        return ctx;
    }

    void FdManager::del(int fd) {
        RWMutexType::WriteLock lock(m_mutex);
        if((int)m_datas.size() <= fd) {
            return;
        }
        m_datas[fd].reset();
    }
}