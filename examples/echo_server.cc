#include "sylar/tcp_server.h"
#include "sylar/log.h"
#include "sylar/iomanager.h"
#include "sylar/bytearray.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

class EchoServer : public sylar::TcpServer {
public:
    EchoServer(int type);
    void handleClient(sylar::Socket::ptr client); 
private:
    int m_type = 0;
};


EchoServer::EchoServer(int type) : m_type(type) {

}

void EchoServer::handleClient(sylar::Socket::ptr client) {
    SYLAR_LOG_INFO(g_logger) << "handleClient" << *client;
    sylar::ByteArray::ptr ba(new sylar::ByteArray);
    while(true) {
        ba -> clear();
        std::vector<iovec> iovs;     // 接收长度
        ba -> getWriteBuffers(iovs, 1024);

        int rt = client -> recv(&iovs[0], iovs.size());
        if(rt == 0) {
            SYLAR_LOG_INFO(g_logger) << "client close " << *client;
            // SYgiLAR_LOG_INFO(g_logger) << "step0 ";
            break;
        } else if (rt < 0) {
            SYLAR_LOG_INFO(g_logger) << "client error rt = " << rt         
                                     << " errno = " << errno << " errstr = " << strerror(errno);
            break;
        }
        // SYLAR_LOG_INFO(g_logger) << "stepA ";
        ba -> setPosition(ba -> getPosition() + rt);
        // SYLAR_LOG_INFO(g_logger) << "stepB ";
        ba -> setPosition(0);
        if(m_type == 1) {           //text
            std::cout << ba -> toString();// << std::endl;
        } else {
            std::cout << ba -> toHexString();// << std::endl;
        }
        std::cout.flush();
    }
} 

int type = 1;

void run() {
    EchoServer::ptr es(new EchoServer(type));
    auto addr = sylar::Address::LookupAny("0.0.0.0:8020");
    while(!es -> bind(addr)) {
        sleep(2);
    }
    es -> start();
}

int main(int argc, char** argv) {
    if(argc < 2) {
        SYLAR_LOG_INFO(g_logger) << "used as[" << argv[0] << " -t] or [" << argv[0] << " -b]";
        return 0; 
    }

    if(!strcmp(argv[1], "-b")) {
        type = 2;
    }

    sylar::IOManager iom(2);
    iom.schedule(run);
    return 0;
}