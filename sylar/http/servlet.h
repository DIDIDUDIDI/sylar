#ifndef __SYLAR_HTTP_SERVLET_H__
#define __SYLAR_HTTP_SERVLET_H__

#include <memory>
#include <functional>
#include <string>
#include "http.h"
#include "http_session.h"
#include <vector>
#include <unordered_map>
#include "sylar/thread.h"

namespace sylar {
    namespace http {

        class Servlet {
        public:
            typedef std::shared_ptr<Servlet> ptr;

            //Servlet() {}
            Servlet(const std::string& name) : m_name(name) {}
            virtual ~Servlet() {}
            virtual int32_t handle(sylar::http::HttpRequest::ptr request,
                           sylar::http::HttpResponse::ptr response,
                           sylar::http::HttpSession::ptr session) = 0;

            const std::string& getName() const {return m_name;}
        private:
            std::string m_name;
        };

        class FunctionServlet : public Servlet {
        public:
            typedef std::shared_ptr<FunctionServlet> ptr;
            typedef std::function<int32_t (sylar::http::HttpRequest::ptr request,
                           sylar::http::HttpResponse::ptr response,
                           sylar::http::HttpSession::ptr session)> callback;

            FunctionServlet(callback cb);
            virtual int32_t handle(sylar::http::HttpRequest::ptr request,
                           sylar::http::HttpResponse::ptr response,
                           sylar::http::HttpSession::ptr session) override;

        private:
            callback m_cb;
        };

        class ServletDispatch : public Servlet {
        public:
            typedef std::shared_ptr<ServletDispatch> ptr;  
            typedef RWMutex RWMutexType;

            ServletDispatch();

            virtual int32_t handle(sylar::http::HttpRequest::ptr request,
                           sylar::http::HttpResponse::ptr response,
                           sylar::http::HttpSession::ptr session) override;

            void addServlet(const std::string& uri, Servlet::ptr slt);
            void addServlet(const std::string& uri, FunctionServlet::callback cb);
            void addGlobServlet(const std::string& uri, Servlet::ptr slt);
            void addGlobServlet(const std::string& uri, FunctionServlet::callback cb);

            void delServlet(const std::string& uri);
            void delGlobServlet(const std::string& uri);

            Servlet::ptr getDefault() const {return m_default;}
            void setDefault(Servlet::ptr v) { m_default = v;} 

            Servlet::ptr getServlet(const std::string& uri);
            Servlet::ptr getGlobServlet(const std::string& uri);

            Servlet::ptr getMatchedServlet(const std::string& uri);
        private:
            RWMutexType m_mutex;
            //map: <uri, servlet> (/sylar/xxx) -> servlet 精准匹配，有限度更高 
            std::unordered_map<std::string, Servlet::ptr> m_datas;
            //map: <uri, servlet> (/sylar/*) -> servlet 模糊匹配，如果精准匹配不成功，再切换成模糊匹配
            std::vector<std::pair<std::string, Servlet::ptr> > m_globs;
            // 默认servlet，所有路径没有匹配到的时候使用
            Servlet::ptr m_default;
        };

        class NotFoundServlet : public Servlet {
        public:
            typedef std::shared_ptr<NotFoundServlet> ptr;
            NotFoundServlet();
            virtual int32_t handle(sylar::http::HttpRequest::ptr request,
                           sylar::http::HttpResponse::ptr response,
                           sylar::http::HttpSession::ptr session) override;
        };
    }
}

#endif