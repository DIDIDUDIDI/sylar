#include "http_server.h"
#include "sylar/log.h"

namespace sylar {
    namespace http {
        static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

        HttpServer::HttpServer(bool keepalive, sylar::IOManager* worker, sylar::IOManager* accept_worker)
            : TcpServer(worker, accept_worker),
              m_isKeeplive(keepalive) {
            m_dispatch.reset(new ServletDispatch);
        }

        void HttpServer::handleClient(Socket::ptr client) {
            sylar::http::HttpSession::ptr session(new HttpSession(client));
            // SYLAR_LOG_DEBUG(g_logger) << "test1";
            do {
                auto req = session -> recvRequest();
                if(!req) {
                    SYLAR_LOG_WARN(g_logger) << "recv http request fail, errno = " 
                                             << errno << " errstr = " << strerror(errno)
                                             << " client: " << *client;
                    break;
                }
                

                HttpResponse::ptr rsp(new HttpResponse(req -> getVersion(), req -> isClose() || !m_isKeeplive));

                // 为什么不直接respond? 因为这样我们就可以做一些类似Java AOP的概念，在handle前和后做些东西，然后一起sendrespond
                m_dispatch->handle(req, rsp, session);
                
                // rsp -> setBody("hello DIDI");
                // SYLAR_LOG_INFO(g_logger) << "request: " << std::endl << *req;
                // SYLAR_LOG_INFO(g_logger) << "response: " << std::endl << *rsp;

                session -> sendResponse(rsp);
            } while(m_isKeeplive);
            session -> close();
        }
    }
}