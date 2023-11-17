#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__

#include <memory>
#include <sstream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <set>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "log.h"
#include "thread.h"

namespace sylar{

class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    ConfigVarBase(const std::string& name, const std::string description = "") 
        : m_name(name), m_description(description) {
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
        }
    
    virtual ~ConfigVarBase() {}

    const std::string& getName() const {return m_name;}
    const std::string& getDescription() const {return m_description;}

    virtual std::string toString() = 0;
    virtual bool fromString(const std::string& val) = 0; 
    virtual std::string getTypeName() const = 0;
protected:
    std::string m_name;
    std::string m_description;
};

// F from_type, T to_type
/*
    LexicalCast 包装了 boost::lexical_cast 方法
*/
template<class F, class T>
class LexicalCast {
public:
    T operator() (const F& v) {
        return boost::lexical_cast<T>(v);
    }
};

// 模板类的模板类型子类，特例化
// vector
template<class T> 
class LexicalCast<std::string, std::vector<T> >
{
public:
    std::vector<T> operator() (const std::string& v) {
        typename std::vector<T> vec;
        YAML::Node node = YAML::Load(v);
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::vector<T>, std::string> 
{
public:
    std::string operator() (const std::vector<T>& v) {
        YAML::Node node;
        for(auto& i : v) {
        node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<class T>
class LexicalCast<std::string, std::list<T> >
{
public:
    std::list<T> operator() (const std::string& v) {
        typename std::list<T> lis;
        YAML::Node node = YAML::Load(v);
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++ i) {
            ss.str("");
            ss << node[i];
            lis.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return lis;
    }
};

template<class T>
class LexicalCast<std::list<T>, std::string>
{
public:
    std::string operator() (const std::list<T>& v) {
        YAML::Node node;
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<class T>
class LexicalCast<std::string, std::set<T> >
{
public:
    std::set<T> operator() (const std::string& v) {
        typename std::set<T> ret;
        YAML::Node node = YAML::Load(v);
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++ i) {
            ss.str("");
            ss << node[i];
            ret.insert(LexicalCast<std::string, T>() (ss.str()));
        }
        return ret;
    }
};

template<class T>
class LexicalCast<std::set<T>, std::string>
{
public:
    std::string operator() (const std::set<T>& v) {
        YAML::Node node;
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<class T>
class LexicalCast<std::string, std::unordered_set<T> > 
{
public:
    std::unordered_set<T> operator() (const std::string& v) {
        typename std::unordered_set<T> ret;
        YAML::Node node = YAML::Load(v);
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            ret.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return ret;
    }
};

template<class T>
class LexicalCast<std::unordered_set<T>, std::string> 
{
public:
    std::string operator() (const std::unordered_set<T>& v) {
        YAML::Node node;
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<class T>
class LexicalCast<std::string, std::map<std::string, T> > 
{
public:
    std::map<std::string, T> operator() (const std::string& v) {
        typename std::map<std::string, T> ret;
        YAML::Node node = YAML::Load(v);
        std::stringstream ss;
        for(auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it->second;
            ret.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return ret;
    }
};

template<class T>
class LexicalCast<std::map<std::string, T>, std::string> 
{
public:
    std::string operator() (const std::map<std::string, T>& v) {
        YAML::Node node;
        for(auto& i : v) {
            node[i.first]= YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<class T>
class LexicalCast<std::string, std::unordered_map<std::string, T> > 
{
public:
    std::unordered_map<std::string, T> operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::unordered_map<std::string, T> ret;
        std::stringstream ss;
        for(auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it -> second;
            ret.insert(std::make_pair(it -> first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return ret;
    }
};

template<class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string>
{
public:
    std::string operator() (const std::unordered_map<std::string, T>& v) {
        YAML::Node node;
        for(auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};
//FromStr T operator() (const std::string&)
//ToStr std::string operator() (const T&)
/*
    解析：
        这里的Class T就是默认要填入的类，不做解释
        要注意的是FromStr和ToStr，这里采用了上面的模板类LexicalCast 的定义创作的新的class
        并且上面的class LexicalCast 提供了一个operator 方法() 来进行转换。
        这里的Lexical 是全特化，但是最后根据T选择了什么事Lexical的偏特化
        这样我们下面使用的时候就可以直接转换了。
*/
template<class T, class FromStr = LexicalCast<std::string, T>, class ToStr = LexicalCast<T, std::string> >
class ConfigVar : public ConfigVarBase {
public:
    typedef RWMutex RWMutexType;
    typedef std::shared_ptr<ConfigVar> ptr;
    // 修改配置事件的callback 函数
    typedef std::function<void (const T& old_value, const T& new_value)> on_change_cb;

    ConfigVar(const std::string& name, const T& default_Val, const std::string& description)
        : ConfigVarBase(name, description),
          m_val(default_Val) {

          }

    std::string toString() override {
        try {
            // return boost::lexical_cast<std::string> (m_val);
            RWMutexType::ReadLock lock(m_mutex);
            return ToStr()(m_val);
        } catch (std::exception& e) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString() Exception"  
            << e.what() << " convert: " << typeid(m_val).name() << " to string"; // typeid 也可以在模板中使用，以确定模板参数的类型
        }
        return "";
    }

    // 将string val 转成 ConfigVar<T> 
    bool fromString(const std::string& val) override {
        try {
            // m_val = boost::lexical_cast<T>(val);
            // return true;
            setValue(FromStr()(val));
        } catch (std::exception& e) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::FromString() Exception"
            << e.what() << " convert: String to" << typeid(m_val).name(); 
        }

        return false;
    }

    /*
        这里我们返回的时候是一个const T的类型，
        因为如果我们需要获取这个元素，即m_val的时候，基本都是使用
        auto& v = g_name -> getValue();
        如果不设置成const的话那么就会报错，
        当然我们也可以将v修改成复制而不是引用，这样getValue就算不是const也没有关系, 比如：
        T getValue() const {return m_val;}
        auto v = g_name -> getValue();
        以上是ok的
        不过如果我们的getValue的方法是const的，那么之后对应的方法都得是const的了，
        比如我们的Person类，如果Person类下面的toString方法就得是const才可以了，因为const T返回的是一个const的左值
        那么也就只能执行const的方法，确保该方法内部是不会对这个const T的内部变量进行修改
        std::string toString() const{}
        注意这里说的const是签名后的const
        如果const在前，说明返回的是一个不可变的返回值
        但是如果在签名后，说明该方法内部不能对成员变量进行修改

        10/29 更新：
        由于更新了线程安全，所以我们加读锁之后要把const去掉才可以：
        const T getValue() const -> const T getValue()
         
        当然也可以把m_mutex 改成 mutable也是可以的
    */
    const T getValue() { 
        RWMutexType::ReadLock lock(m_mutex);
        return m_val;
    } 
    void setValue(const T& v) { 
        {
            RWMutexType::ReadLock lock(m_mutex);
            if(m_val == v) {
                return;
            } 
            for(auto& i : m_cbs) {
                i.second(m_val, v);
            }
        }
        RWMutexType::WriteLock lock(m_mutex);
        m_val = v;
    }
    std::string getTypeName() const override {return typeid(T).name();}

    uint64_t addListener(on_change_cb cb) {
        static uint64_t s_fun_id = 0;
        RWMutexType::WriteLock lock(m_mutex);
        ++s_fun_id;
        m_cbs[s_fun_id] = cb;
        return s_fun_id;
    }

    void delListener(uint64_t key) {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.erase(key);
    }
    void cleanListener() {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.clear();
    }
    on_change_cb getListener(uint64_t key) {
        RWMutexType::ReadLock lock(m_mutex);
        return m_cbs.count(key) == 0 ? nullptr : m_cbs[key];
    }
private:
    RWMutexType m_mutex;
    T m_val;
    /*
        变更设置回调函数组
        为什么要用map ? -> 因为我们on_change_cb 是一个functional的指针函数，functional是没有提供对比函数的，所以我们是没有办法知道是不是同一个functional
        所以我们用map的key来作为标签进行修改
        key：uint64_t 作为key，要求唯一，我们可以用一个hash值来表示
    */
    std::unordered_map<uint64_t, on_change_cb> m_cbs;
};

class Config {
public:
    /*
        存的是模板类的父类，之后用dynamic_pointer_cast 转型成实际的模板类
    */
    typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;
    typedef RWMutex RWMutexType;
    // Lookup 方法，我们用来生成config的主要方法，如果一个config已经存在与ConfigVarMap，我们直接拿出来用，不存在就创建一个然后存入map
    template<class T>
    static typename ConfigVar<T>::ptr Lookup (const std::string& name, const T& default_Val, const std::string& description = "") {
        RWMutexType::WriteLock lock(GetMutex());
        ConfigVarMap& s_datas = GetDatas();
        if(s_datas.count(name) != 0) {
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(s_datas[name]);
            if(tmp) {
                SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name: " << name << "exists.";
                return tmp;
            }else{
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name: " << name << "exists, but type is not: " 
                << typeid(T).name() << ", real type is " << s_datas[name] -> getTypeName() << " " << s_datas[name] -> toString();
                return nullptr; 
            }
        }
        // LookUp<T> 写了之后死锁了，TODO: 死锁debug 练习
        // auto tmp = Lookup<T>(name);
        // if(tmp) {
        //     SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name: " << name << "exists.";
        //     return tmp;
        // }

        // 前面父类构造的时候会把所有的大写统一转换成小写，这样用户无论用大小写都会route到同一个配置上
        //ABCDEFGHIJKLMNOPQRSTUVWXYZ 
        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._1234567890") != std::string::npos) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name: " << name << " invalied.";
            throw std::invalid_argument(name);
        }
        // Typename 才行
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_Val, description));
        s_datas[name] = v;
        return v;
    }

    template<class T>
    static typename ConfigVar<T>::ptr Lookup (const std::string& name) {
        RWMutexType::ReadLock lock(GetMutex());
        ConfigVarMap& s_datas = GetDatas();
        if(s_datas.count(name) == 0) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T> >(s_datas[name]);
    }

    static void LoadFromYaml(const YAML::Node& root);

    static ConfigVarBase::ptr LookupBase(const std::string& name);

    //static ConfigVarMap& getMap() {return GetDatas();}

    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);
private:
    /*
        为什么要用静态方法包装起来？
        因为之前的lookup静态方法可能会在sdata之前生成，会报错
        为了保证初始化的顺序，所以我们先静态方法包装好s_datas
    */
    static ConfigVarMap& GetDatas() {
        static ConfigVarMap s_datas;
        return s_datas;
    }

    static RWMutexType& GetMutex() {
        static RWMutexType m_mutex;
        return m_mutex;
    }
};

}

#endif