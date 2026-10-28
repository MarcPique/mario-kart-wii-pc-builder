#include "pc_live_locale.h"
#include "pc_menu_locale.h"
#include "memory.h"
#include "runtime_log.h"
#include <fstream>
#include <unordered_map>
#include <cstring>
namespace PcLiveLocale {namespace {
std::vector<uint8_t> bytes;std::vector<uint32_t> ids;
std::unordered_map<uint32_t,uint32_t> indices;
uint32_t bankBase=0,payloadStart=0;bool prepared=false;
bool raceContext=false;
uint32_t Read(size_t p){if(p+4>bytes.size())throw std::runtime_error("Truncated language bank");return bytes[p]|uint32_t(bytes[p+1])<<8|uint32_t(bytes[p+2])<<16|uint32_t(bytes[p+3])<<24;}
void Prepare(){
    if(prepared)return;prepared=true;
    const auto path=RuntimeConfigFile::ExecutableDirectory().value_or(std::filesystem::path{})/"pc_locale"/"texts.bin";
    std::ifstream file(path,std::ios::binary);if(!file)return;
    file.seekg(0,std::ios::end);auto size=file.tellg();if(size<16||size>16*1024*1024)throw std::runtime_error("Invalid language bank size");file.seekg(0);bytes.resize(static_cast<size_t>(size));file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if(!file||std::memcmp(bytes.data(),"MKWLOC03",8))throw std::runtime_error("Invalid language bank header");
    const auto count=Read(8),payload=Read(12);if(count>65536)throw std::runtime_error("Invalid language bank count");
    payloadStart=16+count*84;if(uint64_t(payloadStart)+payload!=bytes.size())throw std::runtime_error("Invalid language bank bounds");
    for(uint32_t i=0;i<count;++i){auto id=Read(16+i*4);if(indices.contains(id))throw std::runtime_error("Duplicate language ID");ids.push_back(id);indices[id]=i;}
    for(uint32_t language=0;language<10;++language)for(uint32_t i=0;i<count;++i){auto p=16+count*4+(language*count+i)*8;if(Read(p)>=payload||Read(p+4)>5)throw std::runtime_error("Invalid language entry");}
}
std::optional<Message> Get(uint32_t index){
    if(!bankBase||index>=ids.size())return std::nullopt;
    const auto language=PcMenuLocale::LanguageIndex(PcMenuLocale::Current())+(raceContext?5:0);
    const auto record=16+static_cast<uint32_t>(ids.size())*4+(language*static_cast<uint32_t>(ids.size())+index)*8;
    return Message{0x7D000000u+index,bankBase+payloadStart+Read(record),bankBase+record+4,Read(record+4)};
}
}
uint32_t RequiredBytes(){Prepare();return static_cast<uint32_t>((bytes.size()+31)&~size_t{31});}
void Install(uint32_t base){if(bytes.empty())return;if(!Memory::Contains(base,RequiredBytes()))throw std::runtime_error("Language memory reservation is unmapped");std::memcpy(Memory::GetPointer(base,bytes.size()),bytes.data(),bytes.size());bankBase=base;RT_LOGF("pc-language","Loaded %zu original/Retro Rewind message IDs in five languages (%zu bytes); live switching ready\n",ids.size(),bytes.size());}
std::optional<Message> Find(uint32_t id){const auto it=indices.find(id);return it==indices.end()?std::nullopt:Get(it->second);}
std::optional<Message> FromIndex(uint32_t encoded){return (encoded&0xFF000000u)==0x7D000000u?Get(encoded-0x7D000000u):std::nullopt;}
void SetRaceContext(bool race){raceContext=race;}
}
