#pragma once
#include <cstdlib>
#include <cstdio>
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
inline std::optional<std::string> Select(const char* name,const char* role){
    const std::string_view localRole=role?role:"";
    if(!localRole.empty()&&localRole!="host"&&localRole!="follower")throw std::invalid_argument("Invalid local session role");
    if(!name||!*name){
        if(localRole=="follower")throw std::invalid_argument("A follower requires an independent local profile");
        return std::nullopt;
    }
    if(!ValidName(name))throw std::invalid_argument("Invalid MKW_PC_PROFILE");
    return std::string(name);
}
inline const std::optional<std::string>& Current(){
    // Freeze the selection before any subsystem resolves a mutable path.
    static const std::optional<std::string> profile=[]()->std::optional<std::string>{
        try{return Select(std::getenv("MKW_PC_PROFILE"),std::getenv("MKW_PC_LOCAL_ROLE"));}
        catch(const std::invalid_argument& error){
            // Some runtime paths are initialized before main. A configuration
            // rejection must not become an uncaught static-initializer crash.
            std::fprintf(stderr,"Invalid local session configuration: %s\n",error.what());
            std::fflush(stderr);std::_Exit(EXIT_FAILURE);
        }
    }();return profile;
}
}
