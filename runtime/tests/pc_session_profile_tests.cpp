#include "pc_session_profile.h"
#include <iostream>

int main(int argc,char** argv){
    if(argc==2&&std::string_view(argv[1])=="--resolve"){(void)PcSessionProfile::Current();return 0;}
    for(const auto name:{"online-pair-1","online-pair-2","player_4","guest-2026"})
        if(!PcSessionProfile::ValidName(name))return 1;
    for(const auto name:{"",".","..","../main","a/b","a\\b","F:escape","a:b","CON","con","nul","aux","prn","com1","lpt9","%TEMP%","a.b"," profile"})
        if(PcSessionProfile::ValidName(name))return 2;
    if(PcSessionProfile::ValidName(std::string(33,'a')))return 3;
    const std::filesystem::path base="lab/UserData";
    const auto first=PcSessionProfile::Directory(base,"online-pair-1");
    const auto second=PcSessionProfile::Directory(base,"online-pair-2");
    if(first==second||first==base||first.parent_path()!=base/"Profiles")return 4;
    for(const auto file:{"Config.toml","NAND/title/save.dat","Saves/RetroWFC/RMCP/rksys.dat","Logs/console.log","Cache/pipelines.db","keyboard_bindings.dat"})
        if(first/file==second/file||first/file==base/file)return 5;
    try{PcSessionProfile::Directory(base,"../../main");return 6;}catch(const std::invalid_argument&){}
    if(PcSessionProfile::Select(nullptr,nullptr)||PcSessionProfile::Select("","host"))return 7;
    if(PcSessionProfile::Select("online-pair-2","follower")!=std::optional<std::string>("online-pair-2"))return 8;
    for(const auto role:{"follower","invalid-role"}){
        try{PcSessionProfile::Select(nullptr,role);return 9;}catch(const std::invalid_argument&){}
    }
    try{PcSessionProfile::Select("../main","follower");return 10;}catch(const std::invalid_argument&){}
    std::cout<<"Session profiles: isolated mutable paths and invalid names rejected\n";
}
