//#define YAML_CPP_STATIC_DEFINE
#include "sylar/config.h"
#include "sylar/log.h"
#include <yaml-cpp/yaml.h>
#include <string>

//~/workspcae/sylar/bin/conf/log.yml

sylar::ConfigVar<int>::ptr g_int_value_config = sylar::Config::Lookup("system.port", (int)8080, "system port number");
sylar::ConfigVar<float>::ptr g_float_value_config = sylar::Config::Lookup("system.value", (float)10.2f, "system value");
sylar::ConfigVar<std::vector<int> >::ptr g_int_vec_value_config = sylar::Config::Lookup("system.int_vec", std::vector<int>{1,2,3,4,5}, "system value");
sylar::ConfigVar<std::list<int> >::ptr g_int_lis_value_config = sylar::Config::Lookup("system.int_lis", std::list<int>{5, 10, 30}, "system value");
sylar::ConfigVar<std::set<int> >::ptr g_int_set_value_config = sylar::Config::Lookup("system.int_set", std::set<int>{100, 200}, "system value");
sylar::ConfigVar<std::unordered_set<int> >::ptr g_int_hashset_value_config = sylar::Config::Lookup("system.int_hashset", std::unordered_set<int>{1000, 2000, 1000}, "system value");
sylar::ConfigVar<std::map<std::string, int> >::ptr g_str_int_map_value_config = sylar::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"didi", 96}, {"coca", 93}}, "system value");
sylar::ConfigVar<std::unordered_map<std::string, int> >::ptr g_str_int_umap_value_config = sylar::Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"CN", 86}, {"US", 1}}, "system value");



void print_yaml(const YAML::Node& root, int level) {
    if(root.IsScalar()) {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ') << root.Scalar() << " - " << root.Type() << " - " << level;
    }else if(root.IsNull()) {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ') << "NUll - " << root.Type() << " - " << level;
    }else if(root.IsMap()) {
        for(auto it = root.begin(); it != root.end(); ++ it) {
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ') << it->first << " - " << it->second.Type() << " - " << level;
            print_yaml(it->second, level + 1);
        }
    }else if(root.IsSequence()) {
        for(size_t i = 0; i < root.size(); ++ i) {
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ') << i << " - " << root[i].Type() << " - " << level;
            print_yaml(root[i], level + 1);
        }
    }
}

void test_yaml() {
    YAML::Node root = YAML::LoadFile("/root/workspcae/sylar/bin/conf/log.yml");
    print_yaml(root, 0);
    //SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << root;
}

template<class T> 
void testContainer(T& v, std::string states) {
    for(auto& i : v) {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << states << " " << typeid(T).name() << ":  " << i;
    }
}

void test_config() {
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before: " << g_int_value_config -> getValue();
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before: " << g_float_value_config -> toString();
    auto v = g_int_vec_value_config -> getValue();
    // for(auto& i : v) {
    //     SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before vec: " << i;
    // }
    // auto v_list = g_int_lis_value_config -> getValue();
    // for(auto& i : v_list) {
    //     SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before Lis: " << i;
    // }
    // DIDI -> printFun
    // testContainer(g_int_vec_value_config -> getValue(), "before");
    // testContainer(g_int_lis_value_config -> getValue(), "before");

//Sylar -> macro

#define XX(g_name, state, type)  { \
        auto& v = g_name -> getValue(); \
        for(auto& i : v) {SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #state << " " << #type << ": " << i;} \
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #state << " " << #type << " YAML : " << g_name -> toString(); \
    }

#define XX_Map(g_name, state, type)  { \
        auto& v = g_name -> getValue(); \
        for(auto& i : v) {SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #state << " " << #type << ": {" << i.first << " , " << i.second << "}";} \
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #state << " " << #type << " YAML : " << g_name -> toString(); \
    }

    XX(g_int_vec_value_config, before, int_vec);
    XX(g_int_lis_value_config, before, int_list);
    XX(g_int_set_value_config, before, int_set);
    XX(g_int_hashset_value_config, before, int_hashset);
    XX_Map(g_str_int_map_value_config, before, str_int_map);
    XX_Map(g_str_int_umap_value_config, before, str_int_umap);

    YAML::Node root = YAML::LoadFile("/root/workspcae/sylar/bin/conf/log.yml");
    sylar::Config::LoadFromYaml(root);



    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after: " << g_int_value_config -> getValue();
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after: " << g_float_value_config -> toString();

    // v = g_int_vec_value_config -> getValue();
    // for(auto& i : v) {
    //     SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after vec: " << i;
    // }
    
    // v_list = g_int_lis_value_config -> getValue();
    // for(auto& i : v_list) {
    //     SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after Lis: " << i;
    // }
    // DIDI stye for print function
    // testContainer(g_int_vec_value_config -> getValue(), "after");  
    // testContainer(g_int_lis_value_config -> getValue(), "after");
    XX(g_int_vec_value_config, after, int_vec);
    XX(g_int_lis_value_config, after, int_list);
    XX(g_int_set_value_config, after, int_set);
    XX(g_int_hashset_value_config, after, int_hashset);
    XX_Map(g_str_int_map_value_config, after, str_int_map);
    XX_Map(g_str_int_umap_value_config, after, str_int_umap);

#undef XX
#undef XX_Map
}

class Person {
public:
    std::string m_name = "";
    int m_sex = 0;
    int m_age = 0;

    std::string toString() const {
        std::stringstream ss;
        ss << "[ name = " << m_name << ", sex = " << m_sex << ", age = " << m_age << " ]";
        return ss.str();
    }

    // 这里要const是因为我们的Person oth是const的, 之前call == 的地方的V也是const 还是引用
    bool operator== (const Person& oth) const {
        return (m_name == oth.m_name) && (m_age == oth.m_age) && (m_sex == oth.m_sex);
    }
    
};

namespace sylar{
template<>
class LexicalCast<std::string, Person> {
public:
    Person operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);
        Person p;
        p.m_name = node["name"].as<std::string>();
        p.m_sex = node["sex"].as<int>();
        p.m_age = node["age"].as<int>();
        return p;
    }
};

template<>
class LexicalCast<Person, std::string> {
public:
    std::string operator() (const Person& p) {
        YAML::Node node;
        node["name"] = p.m_name;
        node["sex"] = p.m_sex;
        node["age"] = p.m_age;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};
}

sylar::ConfigVar<Person>::ptr g_person_config = sylar::Config::Lookup("class.person", Person(), "person class test");
sylar::ConfigVar<std::map<std::string, Person> >::ptr g_person_map = sylar::Config::Lookup("class.map", std::map<std::string, Person>(), "person class test");


void test_class() {
    // 加了监听器，当我们一触发setvalue方法的时候就会遍历我们的m_cbs 然后把这个方法call到，注意m_cbs是每一个具体实现模板类的ConfigVar的成员变量，所以只有对具体实现的ConfigVar做变动的时候，才会触发这个m_cbs的方法
    g_person_config -> addListener([](const Person& old_value, const Person& new_value){
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "old Value = " << old_value.toString() << ", new Value = " << new_value.toString();
    });

    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "test start -----------------------";

    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before: " <<  g_person_config -> getValue().toString() << " -- " << g_person_config -> toString();

#define XX_PM(g_var, prefix) \
    { \
        auto& v = g_var -> getValue(); \
        for(auto& i : v) { \
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << prefix << i.first << " - " << i.second.toString(); \
        } \
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << prefix << " map size = " << v.size(); \
    } 

    XX_PM(g_person_map, "class.map before ");

    YAML::Node root = YAML::LoadFile("/root/workspcae/sylar/bin/conf/test.yml");
    sylar::Config::LoadFromYaml(root);

    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after: " <<  g_person_config -> getValue().toString() << " -- " << g_person_config -> toString();

    XX_PM(g_person_map, "class.map after ");

}

void test_log() {
    static sylar::Logger::ptr system_log = SYLAR_LOG_NAME("system");
    SYLAR_LOG_INFO(system_log) << "Hi, this is system log" << std::endl;
    std::cout << sylar::LoggerMgr::GetInstance() -> toYamlString() << std::endl;
    YAML::Node root = YAML::LoadFile("/root/workspcae/sylar/bin/conf/log.yml");
    std::cout << "---------------------LOAD FROM YAML-----------------------" << std::endl;
    sylar::Config::LoadFromYaml(root);
    std::cout << "----------------------DONE----------------------" << std::endl;
    std::cout << sylar::LoggerMgr::GetInstance() -> toYamlString() << std::endl;
    std::cout << "----------------------test system_log config----------------------" << std::endl;
    SYLAR_LOG_INFO(system_log) << "Hi, this is system log" << std::endl;
    system_log -> setFormatter("%d - [%p]%T%m%n");
    SYLAR_LOG_INFO(system_log) << "Hi, this is system log" << std::endl;
}


int main(int argc, char** argv) {


    // test_yaml();

    // test_config();
    // test_class();

    test_log();

    sylar::Config::Visit([](sylar::ConfigVarBase::ptr var) {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << " name = " << var -> getName() 
                                         << " desecrption = " << var -> getDescription()
                                         << " Typename = " << var -> getTypeName() 
                                         << " value = " << var -> toString();
    });

    return 0;
}