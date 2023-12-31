#ifndef __SYLAR_BYTEARRAY_H__
#define __SYLAR_BYTEARRAY_H__

#include <memory>
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <vector>

namespace sylar {
    class ByteArray {
    public:
        typedef std::shared_ptr<ByteArray> ptr;

        /*
            使用链表来替代Array
        */
        struct Node {
            Node(size_t s);
            Node();
            ~Node();

            char* ptr;
            size_t size;
            Node* next;
        };

        ByteArray(size_t base_size = 4096);
        ~ByteArray();

        //write
        // 固定长度
        void writeFint8(int8_t value);
        void writeFuint8(uint8_t value);
        void writeFint16(int16_t value);
        void writeFuint16(uint16_t value);
        void writeFint32(int32_t value);
        void writeFuint32(uint32_t value);
        void writeFint64(int64_t value);
        void writeFuint64(uint64_t value);

        // 可变长度
        // void writeInt8(const int8_t& value);
        // void writeUint8(const uint8_t& value);
        // void writeInt16(const int16_t& value);
        // void writeUint16(const uint16_t& value);
        void writeInt32(int32_t value);
        void writeUint32(uint32_t value);
        void writeInt64(int64_t value);
        void writeUint64(uint64_t value);

        void writeFloat(float value);
        void writeDouble(double value);
        
        // length:int16, data
        void writeStringF16(const std::string& value);
        // length:int32, data
        void writeStringF32(const std::string& value);
        // length:int64, data
        void writeStringF64(const std::string& value);
        // length:varint, data
        void writeStringVint(const std::string& value);
        // data
        void writeStringWithoutLength(const std::string& value);

        // read
        int8_t  readFint8();
        uint8_t readFuint8();
        int16_t  readFint16();
        uint16_t readFuint16();
        int32_t  readFint32();
        uint32_t readFuint32();
        int64_t  readFint64();
        uint64_t readFuint64();

        int32_t  readInt32();
        uint32_t readUint32();
        int64_t  readInt64();
        uint64_t readUint64();

        float readFloat();
        double readDouble();

        std::string readStringF16();
        std::string readStringF32();
        std::string readStringF64();
        std::string readStringVint();

        // 内部操作
        void clear();

        void write(const void* buf, size_t size);
        void read(void* buf, size_t size);
        void read(void* buf, size_t size, size_t posistion) const;

        
        size_t getPosition() const {return m_position;}
        void setPosition(size_t v);

        bool writeToFile(const std::string& name) const;
        bool readFromFile(const std::string& name);
    
        size_t getBaseSize() const {return m_baseSize;}
        size_t getReadSize() const {return m_size - m_position;}   // 还有多少可读的数据

        bool isLittleEndian() const;
        void setIsLittleEndian(bool val);
        
        std::string toString() const;
        std::string toHexString() const;

        // 只读，不修改position
        uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0) const;      // 从当前位置
        uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;  // 从指定位置
        // 增加容量，不修改m_position, 预留下要写入的大小
        uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

        size_t getSize() const {return m_size;}
    private:
        void addCapacity(size_t size);
        size_t getCapacity() const {return m_capacity - m_position;}
    private:
        size_t m_baseSize;          // 每一个node大概多大
        size_t m_position;          // 当前的操作位置，是在全局，即所有的Node组合起来的位置
        size_t m_capacity;          // 我的容量，包含所有的node的容量
        size_t m_size;              // 当前真实数据的大小，因为不一定填满capacity
        int8_t m_endian;            // 网络字节序

        Node* m_root;
        Node* m_cur;
    };
}

#endif