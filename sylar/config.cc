#include "config.h"



namespace sylar {
    //Config::ConfigVarMap& s_datas = Config::getMap();
   
    ConfigVarBase::ptr Config::LookupBase(const std::string& name) {
        RWMutexType::ReadLock lock(GetMutex());
        Config::ConfigVarMap& s_datas = Config::GetDatas();
        if(s_datas.count(name) == 0) {
            return nullptr;
        }
        return s_datas[name];
    }

    // 递归读入
    static void ListAllMember (const std::string& prefix, const YAML::Node& node, std::list<std::pair<std::string, const YAML::Node> >& output) {
        if(prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._1234567890") != std::string::npos) {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Config invaild name: " << prefix << " : " << node;
            return;
        }
        output.push_back(std::make_pair(prefix, node));
        if(node.IsMap()) {
            for(auto it = node.begin(); it != node.end(); ++it) {
                ListAllMember(prefix.empty() ? it -> first.Scalar() : prefix + "." + it -> first.Scalar(), it -> second, output);
            } 
        }
    }

    /*
    "A.B" 10, "A.C" string
        A:
          B:10
          C:string
    */
   // 从配置文件读取
    void Config::LoadFromYaml(const YAML::Node& root) {
        // 全部读入到all_nodes里
        std::list<std::pair<std::string, const YAML::Node> > all_nodes;
        ListAllMember("", root, all_nodes);

        //检查是否存在我们已有的s_datas 里
        for(auto& i : all_nodes) {
            std::string key = i.first;
            if(key.empty()) {
                continue;
            }
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            ConfigVarBase::ptr var = LookupBase(key);

            if(var) {
                if(i.second.IsScalar()) { // 如果就是普通的标量
                    var -> fromString(i.second.Scalar()); // 直接转换
                }else{
                    // 否则转换成SS 在转
                    std::stringstream ss;
                    ss << i.second;
                    var -> fromString(ss.str());
                }
            }
        }
    }

    void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
        RWMutexType::ReadLock lock(GetMutex());
        auto map = GetDatas();
        for(auto it = map.begin(); it != map.end(); ++it) {
            cb(it -> second);
        }
    }

}