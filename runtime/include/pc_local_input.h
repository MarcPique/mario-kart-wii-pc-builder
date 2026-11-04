#pragma once
#include "hle/controller_status_contract.h"
#include <array>
#include <cstdint>
#include <string_view>

// The coordinator uses an unpredictable local group name. No network packets,
// account identifiers or SDL instance IDs are part of this input-only channel.
namespace PcLocalInput {
using Pads = std::array<PadStatusContract::GuestStatus,4>;
inline bool ValidGroup(std::string_view value){
    if(value.size()!=32)return false;
    for(auto c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
inline Pads Disconnected(){
    Pads pads{};
    for(auto& pad:pads)pad[10]=static_cast<uint8_t>(-1);
    return pads;
}
inline bool Fresh(uint64_t now,uint64_t stamp,uint64_t maximumAge){
    return stamp!=0&&now>=stamp&&now-stamp<=maximumAge;
}
inline Pads SecondPair(const Pads& host,uint64_t now,uint64_t stamp,bool hostAlive=false){
    auto result=Disconnected();
    if(Fresh(now,stamp,250)){result[0]=host[2];result[1]=host[3];}
    else if(hostAlive&&stamp&&now>=stamp){
        // A loading screen may temporarily stop guest PAD reads. Release old
        // controls without inventing a physical disconnection in the follower.
        result[0][10]=host[2][10];result[1][10]=host[3][10];
    }
    return result;
}
struct Presses {
    std::array<uint16_t,2> previous{};
    std::array<std::array<uint64_t,16>,2> at{};
    void Record(const Pads& pads,uint64_t now){
        for(unsigned p=0;p<2;++p){
            if(pads[p+2][10]!=0){previous[p]=0;at[p]={};continue;}
            const auto buttons=BigEndian::Read16(pads[p+2].data());
            const auto down=static_cast<uint16_t>(buttons&~previous[p]);previous[p]=buttons;
            for(unsigned bit=0;bit<16;++bit)if(down&(1u<<bit))at[p][bit]=now;
        }
    }
    Pads Consume(const Pads& pads,uint64_t now,uint64_t stamp,bool hostAlive=false){
        auto result=SecondPair(pads,now,stamp,hostAlive);
        for(unsigned p=0;p<2;++p){
            auto buttons=BigEndian::Read16(result[p].data());
            if(result[p][10]==0)for(unsigned bit=0;bit<16;++bit)if(Fresh(now,at[p][bit],250))buttons|=1u<<bit;
            BigEndian::Write16(result[p].data(),buttons);at[p]={};
        }
        return result;
    }
};
struct Feedback {bool connected=false;std::array<uint32_t,2> motors{};};
void Initialize();
bool BeginHost(std::string_view group);
bool EndHost();
uint64_t Generation();
bool Host();
bool Follower();
Feedback Publish(const Pads& pads);
Pads Receive();
void Motor(unsigned port,uint32_t command);
void Stop();
}
