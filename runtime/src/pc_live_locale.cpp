#include "pc_live_locale.h"
#include "pc_menu_locale.h"
#include "memory.h"
#include "runtime_log.h"
#include "pc_locale_bank.h"
#include "../third_party/pc_json.hpp"
#include <fstream>
#include <unordered_map>
#include <cstring>
namespace PcLiveLocale {namespace {
std::vector<uint8_t> bytes;
PcLocaleBank::Layout layout;
std::vector<std::string> availableCodes{"es","en","fr","de","it"};
std::unordered_map<std::string,std::unordered_map<std::u16string,std::u16string>> menuTexts;
std::unordered_map<uint32_t,uint32_t> indices;
uint32_t bankBase=0,payloadStart=0;bool prepared=false;
bool raceContext=false;
uint32_t Read(size_t p){return PcLocaleBank::Read(bytes,p);}
std::u16string Utf16(std::string_view text){
    std::u16string result;size_t p=0;
    while(p<text.size()){
        const auto first=static_cast<uint8_t>(text[p++]);uint32_t code=first;unsigned extra=0;
        if(first>=0xC2&&first<=0xDF){code=first&31;extra=1;}else if(first>=0xE0&&first<=0xEF){code=first&15;extra=2;}else if(first>=0xF0&&first<=0xF4){code=first&7;extra=3;}else if(first>=128)throw std::runtime_error("Invalid menu UTF-8");
        for(unsigned i=0;i<extra;++i){if(p>=text.size()||(static_cast<uint8_t>(text[p])&0xC0)!=0x80)throw std::runtime_error("Invalid menu UTF-8");code=(code<<6)|(static_cast<uint8_t>(text[p++])&63);}
        if(code>0x10FFFF||(code>=0xD800&&code<=0xDFFF)||(extra==1&&code<128)||(extra==2&&code<2048)||(extra==3&&code<65536))throw std::runtime_error("Invalid menu UTF-8");
        if(code<=0xFFFF)result.push_back(static_cast<char16_t>(code));else {code-=0x10000;result.push_back(static_cast<char16_t>(0xD800+(code>>10)));result.push_back(static_cast<char16_t>(0xDC00+(code&1023)));}
    }return result;
}
void Prepare(){
    if(prepared)return;prepared=true;
    const auto path=RuntimeConfigFile::ExecutableDirectory().value_or(std::filesystem::path{})/"pc_locale"/"texts.bin";
    std::ifstream file(path,std::ios::binary);if(!file)return;
    file.seekg(0,std::ios::end);auto size=file.tellg();if(size<16||size>16*1024*1024)throw std::runtime_error("Invalid language bank size");file.seekg(0);bytes.resize(static_cast<size_t>(size));file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if(!file)throw std::runtime_error("Unable to read language bank");
    layout=PcLocaleBank::Parse(bytes);payloadStart=layout.payloadStart;
    for(uint32_t i=0;i<layout.ids.size();++i)indices[layout.ids[i]]=i;
    const auto menuPath=path.parent_path()/"menu.json";
    if(std::ifstream menu{menuPath};menu){
        menu.seekg(0,std::ios::end);if(menu.tellg()>2*1024*1024)throw std::runtime_error("Menu translations are too large");menu.seekg(0);
        const auto document=nlohmann::json::parse(menu);
        if(document.value("schema",0)!=1||!document.contains("languages")||!document["languages"].is_object())throw std::runtime_error("Invalid menu translation catalog");
        for(const auto& [code,entries]:document["languages"].items()){
            if(!PcLocaleCatalog::Known(code)||!entries.is_object())continue;
            for(const auto& [key,value]:entries.items())if(value.is_string())menuTexts[code][Utf16(key)]=Utf16(value.get<std::string>());
        }
    }
    availableCodes.clear();
    for(const auto& code:layout.codes)if(PcLocaleCatalog::Known(code)){
        const bool original=code=="es"||code=="en"||code=="fr"||code=="de"||code=="it";
        if(original||menuTexts.contains(code))availableCodes.push_back(code);
    }
    if(std::find(availableCodes.begin(),availableCodes.end(),"en")==availableCodes.end())throw std::runtime_error("Language bank needs English fallback");
}
std::optional<Message> Get(uint32_t index){
    if(!bankBase||index>=layout.ids.size())return std::nullopt;
    auto found=std::find(layout.codes.begin(),layout.codes.end(),PcMenuLocale::Current());
    if(found==layout.codes.end())found=std::find(layout.codes.begin(),layout.codes.end(),"en");
    const auto language=static_cast<unsigned>(found-layout.codes.begin());
    const auto record=layout.Record(language,index,raceContext);
    return Message{0x7D000000u+index,bankBase+payloadStart+Read(record),bankBase+record+4,Read(record+4)};
}
}
uint32_t RequiredBytes(){Prepare();return static_cast<uint32_t>((bytes.size()+31)&~size_t{31});}
void Install(uint32_t base){if(bytes.empty())return;if(!Memory::Contains(base,RequiredBytes()))throw std::runtime_error("Language memory reservation is unmapped");std::memcpy(Memory::GetPointer(base,bytes.size()),bytes.data(),bytes.size());bankBase=base;RT_LOGF("pc-language","Loaded %zu original/Retro Rewind message IDs in %zu languages (%zu bytes); live switching ready\n",layout.ids.size(),availableCodes.size(),bytes.size());}
std::optional<Message> Find(uint32_t id){const auto it=indices.find(id);return it==indices.end()?std::nullopt:Get(it->second);}
std::optional<Message> FromIndex(uint32_t encoded){return (encoded&0xFF000000u)==0x7D000000u?Get(encoded-0x7D000000u):std::nullopt;}
void SetRaceContext(bool race){raceContext=race;}
const std::vector<std::string>& LanguageCodes(){Prepare();return availableCodes;}
std::u16string MenuText(std::u16string_view english){Prepare();const auto language=menuTexts.find(PcMenuLocale::Current());if(language==menuTexts.end())return {};const auto text=language->second.find(std::u16string(english));return text==language->second.end()?std::u16string{}:text->second;}
}
