#include "pc_online_label.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
namespace {
std::vector<uint8_t> text(std::u16string_view value){std::vector<uint8_t> bytes;for(auto c:value){bytes.push_back(c>>8);bytes.push_back(c);}bytes.insert(bytes.end(),{0,0});return bytes;}
void put(std::vector<uint8_t>& b,uint32_t value){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(value>>(8*i)));}
std::vector<uint8_t> bank(bool legacy){
 const auto message=text(u"Join Retro WFC.");const std::string magic=legacy?"MKWLOC03":"MKWLOC04";std::vector<uint8_t> b(magic.begin(),magic.end());
 put(b,1);if(legacy)put(b,message.size());else{put(b,1);put(b,message.size());put(b,0);b.insert(b.end(),{'e','n',0,0,0,0,0,0});}
 put(b,42);for(unsigned i=0;i<(legacy?10:2);++i){put(b,0);put(b,1);}b.insert(b.end(),message.begin(),message.end());return b;
}
}
int main(int argc,char** argv){
 for(const auto name:{u"Retro WFC",u"RETRO WFC",u"RetroWFC",u"Retro\nWFC"})assert(PcOnlineLabel::Message(text(name))==text(u"Online"));
 assert(PcOnlineLabel::Message(text(u"Join Retro WFC. RetroWFC!"))==text(u"Join Online. Online!"));
 assert(!PcOnlineLabel::Message(text(u"Retro Rewind")));
 auto source=text(u"Retro WFC");source.resize(source.size()-2);source.insert(source.end(),{0,0x1A,8,0,0,1,0,0});const auto tail=text(u" %02d");source.insert(source.end(),tail.begin(),tail.end());
 auto expected=text(u"Online");expected.resize(expected.size()-2);expected.insert(expected.end(),{0,0x1A,8,0,0,1,0,0});expected.insert(expected.end(),tail.begin(),tail.end());
 assert(PcOnlineLabel::Message(source)==expected);
 auto command=text(u"Retro WFC");command.resize(command.size()-2);command.insert(command.begin(),{0,0x1A,22,0});command.insert(command.end(),{0,0});assert(!PcOnlineLabel::Message(command));
 for(bool legacy:{true,false}){
  auto bytes=bank(legacy);const auto before=bytes;const auto original=PcLocaleBank::Parse(bytes);
  assert(PcOnlineLabel::RebrandBank(bytes)==(legacy?10:2));const auto parsed=PcLocaleBank::Parse(bytes);
  assert(parsed.ids==original.ids&&parsed.codes==original.codes);
  for(unsigned i=0;i<parsed.codes.size();++i)for(unsigned race=0;race<2;++race){const auto record=parsed.Record(i,0,race);assert(PcLocaleBank::Read(bytes,record+4)==1);const auto start=parsed.payloadStart+PcLocaleBank::Read(bytes,record);assert(std::vector<uint8_t>(bytes.begin()+start,bytes.end())==text(u"Join Online."));}
  assert(PcOnlineLabel::RebrandBank(bytes)==0);
 }
 if(argc==2){std::ifstream in(argv[1],std::ios::binary);assert(in);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),{}};std::cout<<"Real localized records renamed: "<<PcOnlineLabel::RebrandBank(bytes)<<'\n';}
 std::cout<<"Online label: formatting commands, variables, case variants, aliases, fonts and legacy/dynamic banks passed\n";
}
