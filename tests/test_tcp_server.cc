#include "sylar/tcp_server.h"
#include "sylar/iomanager.h"
#include "sylar/log.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run() {
    // 172.31.112.233
    auto addr = sylar::Address::LookupAny("0.0.0.0:8033");
    // auto addr2 = sylar::UnixAddress::ptr(new sylar::UnixAddress("/tmp/unix_addr"));
    // SYLAR_LOG_INFO(g_logger) << "addr : " << *addr << " - " << *addr2;
    std::vector<sylar::Address::ptr> addrs;
    addrs.push_back(addr);
    // addrs.push_back(addr2);

    sylar::TcpServer::ptr tcp_server(new sylar::TcpServer);
    std::vector<sylar::Address::ptr> failedAddr;
    while(!tcp_server -> bind(addrs, failedAddr)) {
        sleep(2);
    }
    tcp_server -> start();
}

int main(int argc, char** argv) {
    sylar::IOManager iom(2);
    iom.schedule(run);

    return 0;
}