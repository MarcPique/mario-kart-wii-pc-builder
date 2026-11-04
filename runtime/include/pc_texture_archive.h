#pragma once
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <set>
#include <vector>
namespace PcTextureArchive {
inline constexpr uint64_t MaxFile=512ull*1024*1024,MaxTotal=32ull*1024*1024*1024;
inline std::string Lower(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c+=32;return value;}
inline bool TextureFile(const std::string& name){auto n=Lower(name);const auto slash=n.find_last_of("/\\");n=n.substr(slash==std::string::npos?0:slash+1);return n.starts_with("tex1_")&&(n.ends_with(".png")||n.ends_with(".dds"));}
inline std::string SafePath(std::string path){
    std::replace(path.begin(),path.end(),'\\','/');
    if(path.empty()||path.size()>1024||path.front()=='/'||path.find_first_of(":<>\"|?*")!=std::string::npos)throw std::runtime_error("Unsafe archive path");
    for(unsigned char c:path)if(c<32)throw std::runtime_error("Control character in archive path");
    if(path.back()=='/')path.pop_back();
    std::istringstream parts(path);std::string part;
    while(std::getline(parts,part,'/')){
        if(part.empty()||part=="."||part==".."||part.size()>255||part.back()=='.'||part.back()==' ')throw std::runtime_error("Unsafe archive component");
        const auto device=Lower(part.substr(0,part.find('.')));
        if(device=="con"||device=="prn"||device=="aux"||device=="nul"||(device.size()==4&&(device.starts_with("com")||device.starts_with("lpt"))&&device[3]>='0'&&device[3]<='9'))throw std::runtime_error("Device path in archive");
    }
    return path;
}
struct Listing {uint64_t textureBytes=0;unsigned textures=0,entries=0;};
inline Listing ValidateListing(const std::string& text){
    Listing result;std::set<std::string> paths;std::string path;uint64_t size=0;bool linked=false,encrypted=false;
    auto commit=[&]{if(path.empty())return;const auto safe=SafePath(path);if(!paths.insert(Lower(safe)).second)throw std::runtime_error("Duplicate archive path");if(linked||encrypted)throw std::runtime_error("Linked or encrypted archive entry");if(++result.entries>100000)throw std::runtime_error("Too many archive entries");const auto name=Lower(safe);if(name.ends_with(".png")||name.ends_with(".dds")){if(size>MaxFile||size>MaxTotal-result.textureBytes)throw std::runtime_error("Texture archive exceeds size limit");result.textureBytes+=size;if(TextureFile(safe))++result.textures;}path.clear();size=0;linked=encrypted=false;};
    std::istringstream lines(text);std::string line;
    while(std::getline(lines,line)){
        while(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()){commit();continue;}
        const auto split=line.find(" = ");if(split==std::string::npos)throw std::runtime_error("Invalid archive listing");
        const auto key=line.substr(0,split),value=line.substr(split+3);
        if(key=="Path"){if(!path.empty())throw std::runtime_error("Ambiguous archive path");path=value;}
        else if(key=="Size"){size_t used=0;size=std::stoull(value,&used);if(used!=value.size())throw std::runtime_error("Invalid archive size");}
        else if((key=="Symbolic Link"||key=="Hard Link")&&!value.empty())linked=true;
        else if(key=="Attributes"&&(value.find('L')!=std::string::npos||value.find(" l")!=std::string::npos))linked=true;
        else if(key=="Attributes"&&value.find("XU ")!=std::string::npos){const auto mode=std::stoull(value.substr(value.find("XU ")+3),nullptr,16)>>16;const auto kind=mode&0170000;if(kind&&kind!=0100000&&kind!=0040000)linked=true;}
        else if(key=="Encrypted"&&value=="+")encrypted=true;
    }commit();return result;
}
struct Result {std::vector<std::filesystem::path> directories;unsigned failed=0;};
Result Import(const std::vector<std::filesystem::path>& roots,const std::filesystem::path& cache,const std::filesystem::path& tool);
}
