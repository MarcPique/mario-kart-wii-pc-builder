#include "pc_locale_bank.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <iterator>
namespace {
void put(std::vector<uint8_t>& b,uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(static_cast<uint8_t>(n>>(i*8)));}
std::vector<uint8_t> bank(bool legacy){
    const std::string magic=legacy?"MKWLOC03":"MKWLOC04";std::vector<uint8_t> b(magic.begin(),magic.end());
    put(b,1);if(legacy)put(b,4);else {put(b,1);put(b,4);put(b,0);b.insert(b.end(),{'e','n',0,0,0,0,0,0});}
    put(b,42);for(unsigned i=0;i<(legacy?10:2);++i){put(b,0);put(b,1);}b.insert(b.end(),{0,'A',0,0});return b;
}
bool rejected(const std::vector<uint8_t>& b){try{PcLocaleBank::Parse(b);return false;}catch(const std::runtime_error&){return true;}}
}
int main(int argc,char** argv){
    for(const auto legacy:{false,true}){auto bytes=bank(legacy);const auto layout=PcLocaleBank::Parse(bytes);assert(layout.ids==std::vector<uint32_t>{42});assert(layout.codes.size()==(legacy?5:1));assert(layout.codes[legacy?1:0]=="en");bytes.pop_back();assert(rejected(bytes));}
    auto bytes=bank(false);bytes[12]=65;assert(rejected(bytes));
    bytes=bank(false);bytes[24]='\xff';assert(rejected(bytes));
    bytes=bank(false);bytes[36]=1;assert(rejected(bytes));
    bytes=bank(false);bytes[40]=6;assert(rejected(bytes));
    bytes=bank(false);bytes[bytes.size()-2]=1;assert(rejected(bytes));
    std::cout<<"Language bank: legacy and dynamic tables, bounds, codes, fonts and terminators passed\n";
    if(argc==2){std::ifstream file(argv[1],std::ios::binary);assert(file);const std::vector<uint8_t> data{std::istreambuf_iterator<char>(file),{}};const auto parsed=PcLocaleBank::Parse(data);std::cout<<"Real bank: "<<parsed.ids.size()<<" IDs, "<<parsed.codes.size()<<" languages\n";}
}
