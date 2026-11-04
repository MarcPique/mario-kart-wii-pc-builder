#pragma once
#include "pc_locale_bank.h"
#include <optional>
#include <unordered_map>

// Presentation-only naming. BMG formatting/variables and the on-disk language
// bank remain intact; networking still uses its ordinary service/protocol.
namespace PcOnlineLabel {
inline uint16_t Unit(std::span<const uint8_t> data,size_t p){return uint16_t(data[p])<<8|data[p+1];}
inline bool Letter(uint16_t c,char expected){return c==uint16_t(expected)||c==uint16_t(expected-'a'+'A');}
inline size_t BrandLength(std::span<const uint8_t> data,size_t p){
    constexpr std::string_view first="retro",last="wfc";
    auto cursor=p;
    for(char c:first){if(cursor+2>data.size()||!Letter(Unit(data,cursor),c))return 0;cursor+=2;}
    while(cursor+2<=data.size()){const auto c=Unit(data,cursor);if(c!=' '&&c!='\n'&&c!='\r'&&c!='\t')break;cursor+=2;}
    for(char c:last){if(cursor+2>data.size()||!Letter(Unit(data,cursor),c))return 0;cursor+=2;}
    return cursor-p;
}
inline std::optional<std::vector<uint8_t>> Message(std::span<const uint8_t> data){
    std::vector<uint8_t> output;bool changed=false;
    for(size_t p=0;p+1<data.size()&&p<65536;){
        const auto c=Unit(data,p);
        if(!c){output.push_back(0);output.push_back(0);return changed?std::optional{std::move(output)}:std::nullopt;}
        if(c==0x1A){
            if(p+4>data.size())throw std::runtime_error("Truncated online-label BMG command");
            const auto length=data[p+2];
            if(length<4||(length&1)||length>data.size()-p)throw std::runtime_error("Invalid online-label BMG command");
            output.insert(output.end(),data.begin()+p,data.begin()+p+length);p+=length;continue;
        }
        if(const auto length=BrandLength(data,p)){
            for(char letter:std::string_view("Online")){output.push_back(0);output.push_back(letter);}
            p+=length;changed=true;
        }else{output.push_back(data[p]);output.push_back(data[p+1]);p+=2;}
    }
    throw std::runtime_error("Unterminated online-label BMG message");
}
inline void Write(std::vector<uint8_t>& data,size_t p,uint32_t value){for(unsigned i=0;i<4;++i)data[p+i]=uint8_t(value>>(8*i));}
inline unsigned RebrandBank(std::vector<uint8_t>& data){
    const auto layout=PcLocaleBank::Parse(data);const auto originalSize=data.size();
    std::unordered_map<uint32_t,uint32_t> remapped;unsigned changed=0;
    for(unsigned language=0;language<layout.codes.size();++language)for(unsigned race=0;race<2;++race)for(unsigned index=0;index<layout.ids.size();++index){
        const auto record=layout.Record(language,index,race),offset=PcLocaleBank::Read(data,record);
        auto found=remapped.find(offset);
        if(found==remapped.end()){
            uint32_t replacement=offset;const auto start=size_t(layout.payloadStart)+offset;
            if(auto message=Message(std::span<const uint8_t>(data).subspan(start,originalSize-start))){
                if(data.size()+message->size()>16*1024*1024)throw std::runtime_error("Online-label bank exceeds language limit");
                replacement=static_cast<uint32_t>(data.size()-layout.payloadStart);data.insert(data.end(),message->begin(),message->end());
            }
            found=remapped.emplace(offset,replacement).first;
        }
        if(found->second!=offset){Write(data,record,found->second);++changed;}
    }
    Write(data,std::memcmp(data.data(),"MKWLOC03",8)==0?12:16,static_cast<uint32_t>(data.size()-layout.payloadStart));
    (void)PcLocaleBank::Parse(data);return changed;
}
}
