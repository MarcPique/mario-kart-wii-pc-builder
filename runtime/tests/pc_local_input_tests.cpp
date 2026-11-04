#include "pc_local_input.h"
#include <cassert>
#include <iostream>
#include <utility>

int main(){
    using namespace PcLocalInput;
    assert(ValidGroup("00000000000000000000000000000000"));
    assert(ValidGroup("abcdef0123456789abcdef0123456789"));
    for(auto bad:{"","../profile","abcdef0123456789abcdef012345678","abcdef0123456789abcdef01234567890","ABCDEF0123456789abcdef0123456789","abcdef0123456789abcdef012345678g"})assert(!ValidGroup(bad));
    Pads host;
    for(unsigned port=0;port<4;++port)host[port]=PadStatusContract::Encode({static_cast<uint16_t>(0x100<<port),static_cast<int8_t>(-90+port),75,20,-30,255,17,48,64,0});
    const auto original=host;
    auto pair=SecondPair(host,1000,999);
    assert(pair[0]==host[2]&&pair[1]==host[3]);
    assert(pair[0][2]==static_cast<uint8_t>(-88)&&pair[1][6]==255);
    assert(pair[2][10]==255&&pair[3][10]==255);
    assert(host==original);
    auto neutral=SecondPair(host,1500,1000,true);
    assert(neutral[0][10]==0&&neutral[1][10]==0);
    assert(BigEndian::Read16(neutral[0].data())==0&&neutral[0][2]==0&&neutral[1][6]==0);
    assert(SecondPair(host,1500,1000,false)==Disconnected());
    assert(SecondPair(host,999,1000,true)==Disconnected());
    assert(SecondPair(host,1250,1000)==pair);
    for(auto timing:{std::pair<uint64_t,uint64_t>{1251,1000},{999,1000},{0,0},{1000,0}})assert(SecondPair(host,timing.first,timing.second)==Disconnected());
    host[2][10]=255;pair=SecondPair(host,1001,1000);
    assert(pair[0][10]==255&&pair[1]==host[3]);
    Presses presses;host=original;
    host[2][0]=host[2][1]=host[3][0]=host[3][1]=0;presses.Record(host,1000);
    BigEndian::Write16(host[2].data(),0x100);presses.Record(host,1010);
    BigEndian::Write16(host[2].data(),0);presses.Record(host,1020);
    assert(BigEndian::Read16(presses.Consume(host,1030,1020)[0].data())==0x100);
    assert(BigEndian::Read16(presses.Consume(host,1031,1020)[0].data())==0);
    BigEndian::Write16(host[2].data(),0x100);presses.Record(host,1100);
    BigEndian::Write16(host[2].data(),0x200);presses.Record(host,1300);
    BigEndian::Write16(host[2].data(),0);presses.Record(host,1350);
    assert(BigEndian::Read16(presses.Consume(host,1400,1350)[0].data())==0x200);
    BigEndian::Write16(host[3].data(),0x100);presses.Record(host,1500);host[3][10]=255;presses.Record(host,1510);
    assert(presses.Consume(host,1520,1510)[1][10]==255);
    host[2][10]=0;BigEndian::Write16(host[2].data(),0x100);presses.Record(host,1600);
    BigEndian::Write16(host[2].data(),0);presses.Record(host,1610);
    assert(presses.Consume(host,2000,1610)==Disconnected());
    std::cout<<"Local input: distinct pairs, complete axes/triggers, disconnects, freshness and group names passed\n";
}
