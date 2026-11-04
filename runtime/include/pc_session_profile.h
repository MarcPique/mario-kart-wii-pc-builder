#pragma once
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

// Independent native processes may share immutable GameData, never mutable
// config, NAND, saves, controller mappings, logs or caches.
namespace PcSessionProfile {
inline bool ValidName(std::string_view value){
    if(value.empty()||value.size()>32)return false;
    for(const unsigned char c:value)
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;
    if(value=="con"||value=="prn"||value=="aux"||value=="nul")return false;
    if(value.size()==4&&(value.starts_with("com")||value.starts_with("lpt"))&&value[3]>='1'&&value[3]<='9')return false;
    return true;
}
inline std::filesystem::path Directory(const std::filesystem::path& base,std::string_view name){
    if(!ValidName(name))throw std::invalid_argument("Invalid local session profile");
    return base/"Profiles"/std::string(name);
}
inline const std::optional<std::string>& Current(){
    // Freeze the selection before any subsystem resolves a mutable path.
    static const std::optional<std::string> profile=[]()->std::optional<std::string>{
        const char* value=std::getenv("MKW_PC_PROFILE");
        if(!value||!*value)return std::nullopt;
        if(!ValidName(value))throw std::invalid_argument("Invalid MKW_PC_PROFILE");
        return std::string(value);
    }();return profile;
}
}
