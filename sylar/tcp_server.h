#ifndef __SYLAR_TCP_SERVER_H
#define __SYLAR_TCP_SERVER_H

#include <memory>
#include <functional>
#include "iomanager.h"
#include <vector>
#include "socket.h"
#include "noncopyable.h"

namespace sylar {

    /*
        TCP Server normal step:
        1. bind address
        2. start
        3. stop
    */
    class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable {
    public:
        typedef std::shared_ptr<TcpServer> ptr;
        TcpServer(sylar::IOManager* worker = sylar::IOManager::GetThis(),
                  sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
        virtual ~TcpServer();

        virtual bool bind(Address::ptr addr);
        virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& failedAddrs);
        virtual bool start();
        virtual void stop();

        uint64_t getRecvTimeout() const {return m_recvTimeout;}
        std::string getName() const {return m_name;}
        void setRecvTimeout(uint64_t v) {m_recvTimeout = v;}
        void setName(const std::string& v) {m_name = v;}

        bool isStop() const {return m_isStop;}

    protected:
        virtual void handleClient(Socket::ptr client);      // 触发回调
        virtual void startAccept(Socket::ptr sock);
    private:
        std::vector<Socket::ptr> m_socks;   // 一个socket 数组来存储Listener socket, 因为有些机器是多网卡的
        IOManager* m_worker;                // 工作线程池
        IOManager* m_acceptWorker;          // 专门负责accept
        uint64_t m_recvTimeout;             // 读超时，也可以防止恶意的链接之后不发数据的攻击
        std::string m_name;                 // server 的名字
        bool m_isStop;                      // server 是否停止
    };
}

#endif