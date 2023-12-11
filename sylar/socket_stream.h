#ifndef __SYLAR_SOCKET_STREAM_H__
#define __SYLAR_SOCKET_STREAM_H__

#include "stream.h"
#include "socket.h"

// 一般业务都会用包装过得stream socket来提到直接使用了raw socket
namespace sylar {
    
    class SocketStream : public Stream {
    public:
        typedef std::shared_ptr<SocketStream> ptr;
        SocketStream(Socket::ptr sock, bool owner = true);
        ~SocketStream();

        virtual int read(void* buffer, size_t length) override;
        virtual int read(ByteArray::ptr ba, size_t length) override;
        virtual int write(const void* buffer, size_t length) override;
        virtual int write(ByteArray::ptr ba, size_t length) override;
        virtual void close() override;

        Socket::ptr getSocket() const {return m_socket;}
        bool isConnected() const;
    protected:
        Socket::ptr m_socket;
        bool m_owner;               // 是否是全权交给该类管理，如果是那么析构会直接关掉该socket
    };

}

#endif