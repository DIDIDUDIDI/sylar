#include "tcp_server.h"
#include "config.h"
#include "log.h"

namespace sylar {

    static Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout = 
                            Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2), "tcp server read timeout");

    TcpServer::TcpServer(sylar::IOManager* worker, sylar::IOManager* accept_worker) 
        : m_worker(worker), 
         m_acceptWorker(accept_worker),
          m_recvTimeout(g_tcp_server_read_timeout -> getValue()), 
          m_name("sylar_D_version/1.0.0"),
          m_isStop(true) {

    }

    TcpServer::~TcpServer() {
        for(auto& i : m_socks) {
            i -> close();
        }
        m_socks.clear();
    }
    
    bool TcpServer::bind(Address::ptr addr) {
        std::vector<Address::ptr> addrs, failedAddrs;
        addrs.push_back(addr);
        return bind(addrs, failedAddrs);
    }

    // 如果bind多个地址，有失败的就放进Failed Addrs里然后返回出去
    bool TcpServer::bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& failedAddrs) {
        bool rt = true;
        for(auto& addr : addrs) {
            Socket::ptr sock = Socket::CreateTCP(addr);
            if(!sock -> bind(addr)) {
                SYLAR_LOG_ERROR(g_logger) << "bind fail errno: "
                                          << errno << " errstr = " << strerror(errno)
                                          << " addr = [" << addr -> toString() << "]";
                rt = false;
                failedAddrs.push_back(addr);
                continue;
            }
            if(!sock -> listen()) {
                SYLAR_LOG_ERROR(g_logger) << "listen fail errno = "
                                          << errno << " errstr = " << strerror(errno)
                                          << " addr = [" << addr -> toString() << "]";
                rt = false;
                failedAddrs.push_back(addr);
                continue;
            }
            m_socks.push_back(sock);
        }

        if(!failedAddrs.empty()) {
            m_socks.clear();
            return false;
        }

        for(auto& i : m_socks) {
            SYLAR_LOG_INFO(g_logger) << "server bind success: " << *i;
        }
        return rt;
    }

    void TcpServer::startAccept(Socket::ptr sock) {
        while(!m_isStop) {
            Socket::ptr clinet = sock -> accept();
            if(clinet) {
                clinet -> setRecvTimeout(m_recvTimeout);
                // 需要传入自己的TcpServer智能指针，确保我的handleClient完成前你作为TcpServer自己不能释放
                m_worker -> schedule(std::bind(&TcpServer::handleClient, shared_from_this(), clinet));
            } else {
                SYLAR_LOG_ERROR(g_logger) << "accept errno = " << errno
                                          << " errstr = " << strerror(errno);
            }
        }
    }

    bool TcpServer::start() {
        if(!m_isStop) {
           return true; 
        }
        m_isStop = false;
        for(auto& i : m_socks) {
            m_acceptWorker -> schedule(std::bind(&TcpServer::startAccept, shared_from_this(), i));
        }
        return true;
    }

    void TcpServer::stop() {
        m_isStop = true;
        auto self = shared_from_this();
        m_acceptWorker -> schedule([this, self]() {
            for(auto& sock : m_socks) {
                sock -> cancelAll();
                sock -> close();
            }
            m_socks.clear();
        });
    }

    void TcpServer::handleClient(Socket::ptr client) {
        SYLAR_LOG_INFO(g_logger) << "handleClient: " << *client;
    }
}