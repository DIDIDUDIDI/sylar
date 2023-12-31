#include "sylar/sylar.h"
#include "sylar/iomanager.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
int sock = 0;
void test_fiber() {
    SYLAR_LOG_INFO(g_logger) << "test_fibe sock = " << sock;

    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "180.101.50.188", &addr.sin_addr.s_addr);

    // connect(sock, (const sockaddr*)&addr, sizeof(addr));
    // iom.addEvent(sock, sylar::IOManager::READ, []() {
    //     SYLAR_LOG_INFO(g_logger) << "connected";
    // });
    // iom.addEvent(sock, sylar::IOManager::WRITE, []() {
    //     SYLAR_LOG_INFO(g_logger) << "connected";
    // });
    if(! connect(sock, (const sockaddr*)&addr, sizeof(addr))) {
        
    } else if(errno == EINPROGRESS) {
        SYLAR_LOG_INFO(g_logger) << "add event errno = " << errno << " " << strerror(errno);
        sylar::IOManager::GetThis() -> addEvent(sock, sylar::IOManager::READ, []() {
            SYLAR_LOG_INFO(g_logger) << "read callback";
        });
        sylar::IOManager::GetThis() -> addEvent(sock, sylar::IOManager::WRITE, []() {
            SYLAR_LOG_INFO(g_logger) << "write callback";
            // close(sock);
            sylar::IOManager::GetThis() -> cancelEvent(sock, sylar::IOManager::READ);
            close(sock);
        });
    } else {
        SYLAR_LOG_INFO(g_logger) << "else " << errno << " " << strerror(errno);
    }
}

void test1() {
    sylar::IOManager iom(2, false);
    iom.schedule(&test_fiber);
}

static sylar::Timer::ptr s_timer;
sylar::Mutex s_mutex;
void test_timer() {
    sylar::IOManager iom(2);
    s_timer = iom.addTimer(1000, [](){
        
        SYLAR_LOG_INFO(g_logger) << "hello timer";
        static int i = 0;
        if(i == 5) {
            s_timer -> cancel();
        }
        if(i == 2) {
            s_timer -> reset(2000, false);
        }
        {
            sylar::Mutex::Lock test_lock(s_mutex);
            ++i;
        }
    }, true);
}

int main(int argc, char** argv) {
    test1();
    // test_timer();
    return 0;
}