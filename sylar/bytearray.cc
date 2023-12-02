#include "bytearray.h"
#include "endian.h"
#include <cstring>
#include <fstream>
#include "log.h"
#include <sstream>
#include <iomanip>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    ByteArray::Node::Node(size_t s)
        : ptr(new char[s]),
          size(s),
          next(nullptr) {

    }
    ByteArray::Node::Node() 
        : ptr(nullptr),
          size(0),
          next(nullptr) {

    }

    ByteArray::Node::~Node() {
        if(ptr) {
            delete[] ptr;
        }      
    }

    ByteArray::ByteArray(size_t base_size) 
        : m_baseSize(base_size),         
        m_position(0),
        m_capacity(base_size),   
        m_size(0),
        m_endian(SYLAR_BIG_ENDIAN),
        m_root(new Node(base_size)),
        m_cur(m_root) {
    }

    ByteArray::~ByteArray() {
        Node* tmp = m_root;
        while(tmp) {
            m_cur = tmp;
            tmp = tmp -> next;
            delete m_cur;
        }
    }

    bool ByteArray::isLittleEndian() const {
        return m_endian == SYLAR_LITTLE_ENDIAN;
    }
    void ByteArray::setIsLittleEndian(bool val) {
        if(val) {
            m_endian = SYLAR_LITTLE_ENDIAN;
        } else {
            m_endian = SYLAR_BIG_ENDIAN;
        }
    }

    //write
    // 固定长度
    void ByteArray::writeFint8(int8_t value) {
        write(&value, sizeof(value));
    }
    void ByteArray::writeFuint8(uint8_t value) {
        write(&value, sizeof(value));
    }
    void ByteArray::writeFint16(int16_t value) {
        if(m_endian != SYLAR_BYTER_ORDER) {         // 如果字节序和机器的不一致，转一下 
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }
    void ByteArray::writeFuint16(uint16_t value) {
        if(m_endian != SYLAR_BYTER_ORDER) {         // 如果字节序和机器的不一致，转一下 
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }
    void ByteArray::writeFint32(int32_t value) {
        if(m_endian != SYLAR_BYTER_ORDER) {         // 如果字节序和机器的不一致，转一下 
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }
    void ByteArray::writeFuint32(uint32_t value) {
        if(m_endian != SYLAR_BYTER_ORDER) {         // 如果字节序和机器的不一致，转一下 
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }
    void ByteArray::writeFint64(int64_t value) {
        if(m_endian != SYLAR_BYTER_ORDER) {         // 如果字节序和机器的不一致，转一下 
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }
    void ByteArray::writeFuint64(uint64_t value) {
        if(m_endian != SYLAR_BYTER_ORDER) {         // 如果字节序和机器的不一致，转一下 
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    static uint32_t EncodeZigzag32(const int32_t& v) {
        if(v < 0) {
            return ((uint32_t)(-v)) * 2 - 1;
        } else {
            return v * 2;
        }
    }

    static uint32_t EncodeZigzag64(const int64_t& v) {
        if(v < 0) {
            return ((uint64_t)(-v)) * 2 - 1;
        } else {
            return v * 2;
        }
    }

    static int32_t DecodeZigzag32(const uint32_t& v) {
        return (v >> 1) ^ -(v & 1);
    }

    static int64_t DecodeZigzag64(const uint64_t& v) {
        return (v >> 1) ^ -(v & 1);
    }

    // 可变长度, 压缩
    void ByteArray::writeInt32(int32_t value) {
        writeUint32(EncodeZigzag32(value));
    }
    void ByteArray::writeUint32(uint32_t value) {
        uint8_t tmp[5];
        uint8_t i = 0;
        /*
            参考Google的压缩算法，如果最高位是1，就说明后续可能还有数据
            7bit压缩法
        */
        while(value >= 0x80) {
            tmp[i ++] = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        tmp[i ++] = value;
        write(tmp, i);
    }
    void ByteArray::writeInt64(int64_t value) {
        writeUint64(EncodeZigzag64(value));
    }
    void ByteArray::writeUint64(uint64_t value) {
        uint8_t tmp[10];
        uint8_t i = 0;
        while(value >= 0x80) {
            tmp[i ++] = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        tmp[i ++] = value;
        write(tmp, i);
    }

    void ByteArray::writeFloat(float value) {
        uint32_t v;
        std::memcpy(&v, &value, sizeof(value));
        writeFuint32(v);
    }

    void ByteArray::writeDouble(double value) {
        uint64_t v;
        std::memcpy(&v, &value, sizeof(value));
        writeFuint64(v);
    }
    
    // length:int16, data
    void ByteArray::writeStringF16(const std::string& value) {
        writeFuint16(value.size());         // 长度
        write(value.c_str(), value.size()); // 数据
    }
    // length:int32, data
    void ByteArray::writeStringF32(const std::string& value) {
        writeFuint32(value.size());
        write(value.c_str(), value.size());
    }
    // length:int64, data
    void ByteArray::writeStringF64(const std::string& value) {
        writeFuint64(value.size());
        write(value.c_str(), value.size());
    }
    // length:varint, data
    void ByteArray::writeStringVint(const std::string& value) {
        writeUint64(value.size());
        write(value.c_str(), value.size());
    }
    // data
    void ByteArray::writeStringWithoutLength(const std::string& value) {
        write(value.c_str(), value.size()); // 不要长度，只要数据
    }


    // read
    int8_t  ByteArray::readFint8() {
        int8_t v;
        read(&v, sizeof(v));
        return v;
    }
    uint8_t ByteArray::readFuint8() {
        uint8_t v;
        read(&v, sizeof(v));
        return v;
    }

#define XX(type) \
        type v; \
        read(&v, sizeof(v)); \
        if(m_endian != SYLAR_BYTER_ORDER) { \
            return byteswap(v); \
        } \
        return v;

    int16_t  ByteArray::readFint16() {
        XX(int16_t);
    }
    uint16_t ByteArray::readFuint16() {
        XX(uint16_t);
    }
    int32_t  ByteArray::readFint32() {
        XX(int32_t);
    }
    uint32_t ByteArray::readFuint32() {
        XX(uint32_t);
    }
    int64_t  ByteArray::readFint64() {
        XX(int64_t);
    }
    uint64_t ByteArray::readFuint64() {
        XX(uint64_t);
    }
#undef XX

    int32_t  ByteArray::readInt32() {
        return DecodeZigzag32(readUint32());
    }

    uint32_t ByteArray::readUint32() {
        uint32_t result = 0;
        for(int i = 0; i < 32; i += 7) {
            uint8_t b = readFuint8();
            if(b < 0x80) {
                result |= ((uint32_t)b) << i;
                break;
            } else {
                result |= (((uint32_t)(b & 0x7F)) << i);
            }
        }
        return result;
    }

    int64_t  ByteArray::readInt64() {
        return DecodeZigzag64(readUint64());
    }

    uint64_t ByteArray::readUint64() {
        uint64_t result = 0;
        for(int i = 0; i < 64; i += 7) {
            uint8_t b = readFuint8();
            if(b < 0x80) {
                result |= ((uint64_t)b) << i;
                break;
            } else {
                result |= (((uint64_t)(b & 0x7F)) << i);
            }
        }
        return result;
    }

    float ByteArray::readFloat() {
        uint32_t v = readFuint32();
        float value;
        std::memcpy(&value, &v, sizeof(v));
        return value;
    }
        
    double ByteArray::readDouble() {
        uint64_t v = readFuint64();
        float value;
        std::memcpy(&value, &v, sizeof(v));
        return value;
    }

    std::string ByteArray::readStringF16() {
        uint16_t len = readFuint16();                     // 先read长度
        std::string buff;
        buff.resize(len);                                    
        read(&buff[0], len);                              // 然后把数据放进数据体             
        return buff;                                      // 返回
    }
    std::string ByteArray::readStringF32() {
        uint32_t len = readFuint32();                     // 先read长度
        std::string buff;
        buff.resize(len);                                      
        read(&buff[0], len);                              // 然后把数据放进数据体             
        return buff;                                      // 返回
    }
    std::string ByteArray::readStringF64() {
        uint64_t len = readFuint64();                     // 先read长度
        std::string buff;
        buff.resize(len);                                      
        read(&buff[0], len);                              // 然后把数据放进数据体             
        return buff;                                      // 返回
    }
    std::string ByteArray::readStringVint() {
        uint64_t len = readUint64();                     // 先read长度
        std::string buff;
        buff.resize(len);                                      
        read(&buff[0], len);                              // 然后把数据放进数据体             
        return buff;                                      // 返回
    }
    // 内部操作
    void ByteArray::clear() {
        // 只保留一个节点，剩下的信息全部重置成0
        m_position = m_size = 0;
        m_capacity = m_baseSize;
        Node* tmp = m_root -> next;
        while(tmp) {
            m_cur = tmp;
            tmp = tmp -> next;
            delete m_cur;
        }
        m_cur = m_root;
        m_root -> next = NULL;
    }

    void ByteArray::write(const void* buf, size_t size) {
        if(size == 0) {
            return;
        }
        addCapacity(size);                      // 先保证我的容量

        size_t npos = m_position % m_baseSize;  // 当前node 的操作位置，即全局所在的位置mod每个node的大小
        size_t ncap = m_cur->size - npos;       // 当前node 的容量
        size_t bpos = 0;                        // 偏移量
        
        while(size > 0) {
            if(ncap >= size) {                  // 如果当前的node存的下
                std::memcpy(m_cur -> ptr + npos, (const char*)buf + bpos, size); // 直接赋值（当前的node和写入的起始位置，写入数据+偏移量，大小）
                if(m_cur -> size == (npos + size)) {
                    m_cur = m_cur -> next;
                }
                m_position += size;             // 当前的全局操作位置就是要算上这次我们的写入
                bpos += size;                   // 偏移量
                size = 0;                       // 写完了
            } else {
                std::memcpy(m_cur -> ptr + npos, (const char*)buf + bpos, ncap); // 同上，唯一的差距是这里只能写入足够存放的量
                m_position += ncap;             // 更改修改位置
                bpos += ncap;                   // 修改偏移量，因为ncap的大小是写过的了，之后要从偏移量的位置再开始写
                size -= ncap;                   // 还剩下多少没写
                m_cur = m_cur -> next;          // 放到下一个node去写
                ncap = m_cur -> size;
                npos = 0;
            }
        }

        if(m_position > m_size) {               // 我们的位置比size大就要重置size了
            m_size = m_position;
        }
    }
    
    void ByteArray::read(void* buf, size_t size) {
        if(size > getReadSize()) {
            throw std::out_of_range("not enough len");
        }

        size_t npos = m_position % m_baseSize;
        size_t ncap = m_cur -> size - npos;
        size_t bpos = 0;

        // 同上，只不是从从m_cur 写入到buf中
        while(size > 0) {
            if(ncap >= size) {
                std::memcpy((char*)buf + bpos, m_cur -> ptr + npos, size);
                if(m_cur -> size == (npos + size)) {
                    m_cur = m_cur -> next;
                }
                m_position += size;
                bpos += size;
                size = 0;
            } else {
                std::memcpy((char*)buf + bpos, m_cur -> ptr + npos, ncap);
                m_position += ncap;
                bpos += ncap;
                size -= ncap;
                m_cur = m_cur -> next;
                ncap = m_cur -> size;
                npos = 0;
            }
        }
    }
    
    // readOnly
    void ByteArray::read(void* buf, size_t size, size_t posistion) const {
        if(size > getReadSize()) {
            throw std::out_of_range("not enough len");
        }

        size_t npos = posistion % m_baseSize;
        size_t ncap = m_cur -> size - npos;
        size_t bpos = 0;
        Node* cur = m_cur;
        // 同上，只不是从从m_cur 写入到buf中
        while(size > 0) {
            if(ncap >= size) {
                std::memcpy((char*)buf + bpos, cur -> ptr + npos, size);
                if(cur -> size == (npos + size)) {
                    cur = cur -> next;
                }
                posistion += size;
                bpos += size;
                size = 0;
            } else {
                std::memcpy((char*)buf + bpos, cur -> ptr + npos, ncap);
                posistion += ncap;
                bpos += ncap;
                size -= ncap;
                cur = cur -> next;
                ncap = cur -> size;
                npos = 0;
            }
        }
    }

    void ByteArray::setPosition(size_t v) {
        if(v > m_size) {
            throw std::out_of_range("set position out of range");
        }
        m_position = v;
        m_cur = m_root;
        while(v > m_cur -> size) {
            v -= m_cur->size;
            m_cur = m_cur->next;
        }
        if(v == m_cur->size) {
            m_cur = m_cur -> next;
        }
    }
    bool ByteArray::writeToFile(const std::string& name) const {
        std::ofstream ofs;
        ofs.open(name, std::ios::trunc | std::ios::binary);
        if(! ofs) {
            SYLAR_LOG_ERROR(g_logger) << "writeToFile name = " << name << "error, "
                                      << "errno = " << errno << " errstr = " << strerror(errno);
            return false;
        }

        int64_t read_size = getReadSize();          // 还剩下多少数据可读
        int64_t pos = m_position;
        Node* cur = m_cur;

        while(read_size > 0) {                      // 还需要读的话
            int diff = pos % m_baseSize;            // 找到当前的Node操作位置，即全局所在的位置mod每个node的大小
            int64_t len = (read_size > (int64_t)m_baseSize ? m_baseSize : read_size) - diff;    // 写入的长度是，如果当前node空间都写不下那就全部写入，要不然的话就把剩下的写入，但是要去掉偏移量，即之前这个node写掉的量
            ofs.write(cur -> ptr + diff, len);      // 写入node
            cur = cur -> next; 
            pos += len;
            read_size -= len;
        }

        return true;
    }

    bool ByteArray::readFromFile(const std::string& name) {
        std::ifstream ifs;
        ifs.open(name, std::ios::binary);
        if(!ifs) {
            SYLAR_LOG_ERROR(g_logger) << "readFromFile name = " << name
                                      << " error, errno = " << errno << " errstr = " 
                                      << strerror(errno);
            return false;
        }

        std::shared_ptr<char> buff(new char[m_baseSize], [](char* ptr){
            delete[] ptr;
        });
        while(!ifs.eof()) {
            ifs.read(buff.get(), m_baseSize);
            write(buff.get(), ifs.gcount());
        }
        return true;
    }
    
    void ByteArray::addCapacity(size_t size) {
        if(size == 0 || getCapacity() >= size) {
            return;
        }
        
        size_t old_capacity = getCapacity();
        
        size = size - old_capacity;
        size_t count = (size / m_baseSize) + (((size % m_baseSize) > old_capacity) ? 1 : 0); // 还需要加多少个节点
        Node* tmp = m_root;
        while(tmp -> next) {
            tmp = tmp -> next;
        }
        // 我们需要纪录下来新增加的节点
        Node* first = NULL;
        // 增加节点
        for(size_t i = 0; i < count; ++ i) {
            tmp -> next = new Node(m_baseSize);
            tmp = tmp -> next;
            if(first == NULL) {
                first = tmp;
            }
            m_capacity += m_baseSize;
        }

        // 如果老的节点已经到底了，那么新的写入肯定在新的node，所以更新m_cur到新的node
        if(old_capacity == 0) {
            m_cur = first;
        }
    }

    std::string ByteArray::toString() const {
        std::string str;
        str.resize(getReadSize());
        if(str.empty()) {
            return str;
        }
        read(&str[0], str.size(), m_position);  // 不希望改变我们的m_position
        return str;
    }

    std::string ByteArray::toHexString() const {
        std::string str = toString();
        std::stringstream ss;
        
        for(size_t i = 0; i < str.size(); ++ i) {
            if(i > 0 && i % 32 == 0) {
                ss << std::endl;
            }
            ss << std::setw(2) << std::setfill('0') << std::hex
               << (int)(uint8_t)str[i] << " ";
        }
        
        return ss.str();
    }

    uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
        len = len > getReadSize() ? getReadSize() : len;
        if(len == 0) {
            return 0;
        }
        uint64_t size = len;

        size_t npos = m_position % m_baseSize;
        size_t ncap = m_cur -> size - npos;
        struct iovec iov;
        Node* cur = m_cur;

        while(len > 0) {
            if(ncap >= len) {
                iov.iov_base = cur -> ptr + npos;
                iov.iov_len = len;
                len = 0;
            } else {
                iov.iov_base = cur -> ptr + npos;
                iov.iov_len = ncap;
                len -= ncap;
                cur = cur -> next;
                ncap = cur -> size;
                npos = 0;
            }
            buffers.push_back(iov);
        }
        return size;
    }

    uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const {
        len = len > getReadSize() ? getReadSize() : len;
        if(len == 0) {
            return 0;
        }
        uint64_t size = len;

        size_t npos = position % m_baseSize;
        size_t count = position / m_baseSize;
        Node* cur = m_root;
        while (count > 0) {
            cur = cur -> next;
            -- count;
        }
        
        size_t ncap = m_cur -> size - npos;
        struct iovec iov;

        while(len > 0) {
            if(ncap >= len) {
                iov.iov_base = cur -> ptr + npos;
                iov.iov_len = len;
                len = 0;
            } else {
                iov.iov_base = cur -> ptr + npos;
                iov.iov_len = ncap;
                len -= ncap;
                cur = cur -> next;
                ncap = cur -> size;
                npos = 0;
            }
            buffers.push_back(iov);
        }
        return size;
    }


    uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
        if(len == 0) {
            return 0;
        }
        addCapacity(len);
        uint64_t size = len;

        size_t npos = m_position % m_baseSize;
        size_t ncap = m_cur -> size - npos;
        struct iovec iov;
        Node* cur = m_cur;
        while(len > 0) {
            if(ncap >= len) {
                iov.iov_base = cur -> ptr + npos;
                iov.iov_len = len;
                len = 0;
            } else {
                iov.iov_base = cur -> ptr + npos;
                iov.iov_len = ncap;

                len -= ncap;
                cur = cur->next;
                ncap = cur -> size;
                npos = 0;
            }
            buffers.push_back(iov);
        }
        return size;
    }
}