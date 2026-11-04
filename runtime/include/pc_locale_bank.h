#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace PcLocaleBank {
inline uint32_t Read(std::span<const uint8_t> bytes,size_t offset){
    if(offset>bytes.size()||bytes.size()-offset<4)throw std::runtime_error("Truncated language bank");
    return bytes[offset]|uint32_t(bytes[offset+1])<<8|uint32_t(bytes[offset+2])<<16|uint32_t(bytes[offset+3])<<24;
}
struct Layout {
    std::vector<uint32_t> ids;
    std::vector<std::string> codes;
    uint32_t payloadStart=0,tableStart=0;
    uint32_t Record(unsigned language,unsigned index,bool race)const{
        return tableStart+((language+(race?static_cast<unsigned>(codes.size()):0u))*static_cast<unsigned>(ids.size())+index)*8u;
    }
};
inline Layout Parse(std::span<const uint8_t> bytes){
    if(bytes.size()<16||bytes.size()>16*1024*1024)throw std::runtime_error("Invalid language bank size");
    const bool legacy=std::memcmp(bytes.data(),"MKWLOC03",8)==0;
    if(!legacy&&std::memcmp(bytes.data(),"MKWLOC04",8))throw std::runtime_error("Invalid language bank header");
    Layout result;const auto count=Read(bytes,8);
    if(count>65536)throw std::runtime_error("Invalid language bank count");
    uint32_t payload=0;size_t idsStart=16;
    if(legacy){result.codes={"es","en","fr","de","it"};payload=Read(bytes,12);}
    else {
        const auto languages=Read(bytes,12);payload=Read(bytes,16);
        if(languages==0||languages>64||Read(bytes,20)!=0)throw std::runtime_error("Invalid language descriptors");
        idsStart=24+languages*8u;
        if(idsStart>bytes.size())throw std::runtime_error("Truncated language descriptors");
        std::unordered_set<std::string> seen;
        for(unsigned i=0;i<languages;++i){
            const auto start=24+i*8;std::string code;
            for(unsigned n=0;n<8&&bytes[start+n];++n){const auto c=bytes[start+n];
                if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))throw std::runtime_error("Invalid language code");
                code.push_back(static_cast<char>(c));}
            if(code.empty()||code.size()==8||!seen.insert(code).second)throw std::runtime_error("Duplicate or invalid language code");
            result.codes.push_back(std::move(code));
        }
    }
    const uint64_t tables=idsStart+uint64_t(count)*4;
    const uint64_t payloadStart=tables+uint64_t(count)*result.codes.size()*16;
    if(payloadStart+payload!=bytes.size()||payload<2||(payload&1))throw std::runtime_error("Invalid language bank bounds");
    result.tableStart=static_cast<uint32_t>(tables);result.payloadStart=static_cast<uint32_t>(payloadStart);
    std::unordered_set<uint32_t> ids;
    for(unsigned i=0;i<count;++i){const auto id=Read(bytes,idsStart+i*4);if(!ids.insert(id).second)throw std::runtime_error("Duplicate language ID");result.ids.push_back(id);}
    for(unsigned language=0;language<result.codes.size();++language)for(unsigned race=0;race<2;++race)for(unsigned i=0;i<count;++i){
        const auto p=result.Record(language,i,race!=0),offset=Read(bytes,p);
        if((offset&1)||offset>=payload||Read(bytes,p+4)>5)throw std::runtime_error("Invalid language entry");
        bool terminated=false;
        for(size_t j=result.payloadStart+offset;j+1<bytes.size()&&j<size_t(result.payloadStart)+offset+65536;j+=2)
            if(!bytes[j]&&!bytes[j+1]){terminated=true;break;}
        if(!terminated)throw std::runtime_error("Unterminated language entry");
    }
    return result;
}
}
