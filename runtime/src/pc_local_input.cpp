#include "pc_local_input.h"
#include "runtime_log.h"
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <mutex>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace PcLocalInput {
namespace {
enum class Role { Off,Host,Follower };
struct Selection {
    std::atomic<Role> role{Role::Off};std::string group;
    Selection(){
        const auto* groupValue=std::getenv("MKW_PC_LOCAL_INPUT");
        const auto* roleValue=std::getenv("MKW_PC_LOCAL_ROLE");
        if(!groupValue&&!roleValue)return;
        if(!groupValue||!roleValue||!ValidGroup(groupValue))throw std::runtime_error("Invalid local input group");
        group=groupValue;
        if(std::string_view(roleValue)=="host")role=Role::Host;
        else if(std::string_view(roleValue)=="follower")role=Role::Follower;
        else throw std::runtime_error("Invalid local input role");
    }
};
Selection& Selected(){static Selection value;return value;}
std::mutex& LocalMutex(){static std::mutex value;return value;}
std::atomic<uint64_t> generation{0};
#if defined(_WIN32)
constexpr uint32_t magic=0x4D4B4931;
struct SharedFrame {
    uint32_t signature=0,version=0,owner=0,client=0;
    uint64_t frameAt=0,clientAt=0;
    Pads pads{};std::array<uint32_t,2> motors{};Presses presses{};
};
struct Channel {
    HANDLE mapping=nullptr,mutex=nullptr,owner=nullptr;SharedFrame* shared=nullptr;bool invalid=false;
    ~Channel(){Close();}
    void Close(){if(shared)UnmapViewOfFile(shared);shared=nullptr;if(mapping)CloseHandle(mapping);mapping=nullptr;if(mutex)CloseHandle(mutex);mutex=nullptr;if(owner)CloseHandle(owner);owner=nullptr;invalid=false;}
    bool OwnerAlive(){return !invalid&&owner&&WaitForSingleObject(owner,0)==WAIT_TIMEOUT;}
    bool Open(){
        if(shared)return true;
        const auto& selection=Selected();if(selection.role==Role::Off)return false;
        const auto name=std::wstring(L"Local\\MarioKartWiiInput_")+std::wstring(selection.group.begin(),selection.group.end());
        if(selection.role==Role::Host){
            mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(SharedFrame),name.c_str());
            if(!mapping)return false;
            if(GetLastError()==ERROR_ALREADY_EXISTS){Close();throw std::runtime_error("Local input group already has a host");}
            mutex=CreateMutexW(nullptr,FALSE,(name+L"_lock").c_str());
        }else{
            mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name.c_str());
            if(!mapping)return false;
            mutex=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(name+L"_lock").c_str());
        }
        if(!mutex){Close();return false;}
        shared=static_cast<SharedFrame*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedFrame)));
        if(!shared){Close();return false;}
        if(selection.role==Role::Host){
            const auto wait=WaitForSingleObject(mutex,0);
            if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){Close();return false;}
            *shared={};shared->version=2;shared->owner=GetCurrentProcessId();shared->pads=Disconnected();shared->signature=magic;
            ReleaseMutex(mutex);
            RT_LOGF("pc-local-input","Host channel initialized; physical player order stays in this process\n");
        }
        return true;
    }
    bool Lock(){
        if(!Open())return false;
        const auto wait=WaitForSingleObject(mutex,0);
        if(wait==WAIT_ABANDONED){invalid=true;shared->signature=0;ReleaseMutex(mutex);return false;}
        if(wait!=WAIT_OBJECT_0)return false;
        if(shared->signature!=magic||shared->version!=2||!shared->owner){if(owner)invalid=true;ReleaseMutex(mutex);return false;}
        if(Selected().role==Role::Follower&&!owner)owner=OpenProcess(SYNCHRONIZE,FALSE,shared->owner);
        return true;
    }
    void Unlock(){ReleaseMutex(mutex);}
};
Channel& Current(){static Channel value;return value;}
std::array<std::atomic<uint32_t>,2>& DesiredMotors(){static std::array<std::atomic<uint32_t>,2> values{};return values;}
struct CapturedInput {Presses presses;std::array<bool,2> disconnected{};};
CapturedInput& Captured(){static CapturedInput value;return value;}
void CloseChannel(){
    auto& channel=Current();
    if(Selected().role==Role::Host&&channel.shared&&channel.Lock()){
        channel.shared->signature=0;channel.shared->frameAt=0;channel.Unlock();
    }
    if(Selected().role==Role::Follower&&channel.shared&&channel.Lock()){
        if(channel.shared->client==GetCurrentProcessId()){channel.shared->client=0;channel.shared->clientAt=0;channel.shared->motors={};}
        channel.Unlock();
    }
    channel.Close();Captured()={};
    for(auto& motor:DesiredMotors())motor.store(0,std::memory_order_relaxed);
    if(Selected().role==Role::Host){Selected().role.store(Role::Off,std::memory_order_release);Selected().group.clear();generation.fetch_add(1);}
}
#endif
}
void Initialize(){
    std::lock_guard guard(LocalMutex());
    const auto& selection=Selected();
#if defined(_WIN32)
    if(selection.role==Role::Host&&!Current().Open())throw std::runtime_error("Could not create local input channel");
#else
    if(selection.role!=Role::Off)throw std::runtime_error("Local session input currently requires Windows");
#endif
}
bool BeginHost(std::string_view group){
    if(!ValidGroup(group))return false;
    std::lock_guard guard(LocalMutex());
#if defined(_WIN32)
    auto& selection=Selected();
    if(selection.role==Role::Follower)return false;
    if(selection.role==Role::Host)return selection.group==group;
    selection.group=group;selection.role.store(Role::Host,std::memory_order_release);Captured()={};
    try{if(Current().Open()){generation.fetch_add(1);return true;}}
    catch(const std::exception& e){RT_LOGF("pc-local-input","Cannot activate local session: %s\n",e.what());}
    Current().Close();selection.role.store(Role::Off,std::memory_order_release);selection.group.clear();
#endif
    return false;
}
bool EndHost(){
    std::lock_guard guard(LocalMutex());
#if defined(_WIN32)
    if(!Host())return false;CloseChannel();return true;
#else
    return false;
#endif
}
uint64_t Generation(){return generation.load(std::memory_order_acquire);}
bool Host(){return Selected().role.load(std::memory_order_acquire)==Role::Host;}
bool Follower(){return Selected().role.load(std::memory_order_acquire)==Role::Follower;}
Feedback Publish(const Pads& pads){
    Feedback result;
#if defined(_WIN32)
    std::lock_guard guard(LocalMutex());
    if(Selected().role!=Role::Host)return result;
    auto& captured=Captured().presses;auto& disconnected=Captured().disconnected;
    const auto now=GetTickCount64();captured.Record(pads,now);
    for(unsigned p=0;p<2;++p)if(pads[p+2][10]!=0)disconnected[p]=true;
    auto& channel=Current();if(!channel.Lock())return result;
    auto& shared=*channel.shared;
    for(unsigned p=0;p<2;++p){
        if(disconnected[p]){shared.presses.at[p]={};disconnected[p]=false;}
        for(unsigned bit=0;bit<16;++bit){shared.presses.at[p][bit]=std::max(shared.presses.at[p][bit],captured.at[p][bit]);captured.at[p][bit]=0;}
    }
    shared.pads=pads;shared.frameAt=now;
    result.connected=shared.client&&Fresh(now,shared.clientAt,1000);
    if(result.connected&&Fresh(now,shared.clientAt,250))result.motors=shared.motors;
    channel.Unlock();
#endif
    return result;
}
Pads Receive(){
    auto result=Disconnected();
#if defined(_WIN32)
    std::lock_guard guard(LocalMutex());
    if(!Follower())return result;
    static Pads previous=Disconnected();static uint64_t previousAt=0;
    auto& channel=Current();if(!channel.Lock())return channel.OwnerAlive()?SecondPair(previous,GetTickCount64(),previousAt,true):Disconnected();
    auto& shared=*channel.shared;const auto now=GetTickCount64();
    if(!shared.client||shared.client==GetCurrentProcessId()||!Fresh(now,shared.clientAt,1000)){
        if(shared.client!=GetCurrentProcessId())shared.motors={};
        shared.client=GetCurrentProcessId();shared.clientAt=now;
        for(unsigned p=0;p<2;++p)shared.motors[p]=DesiredMotors()[p].load(std::memory_order_relaxed);
        previous=shared.pads;previousAt=shared.frameAt;
        result=channel.OwnerAlive()?shared.presses.Consume(shared.pads,now,shared.frameAt,true):Disconnected();
    }else {previous=Disconnected();previousAt=0;}
    channel.Unlock();
#endif
    return result;
}
void Motor(unsigned port,uint32_t command){
#if defined(_WIN32)
    std::lock_guard guard(LocalMutex());
    if(!Follower()||port>=2||command>2)return;
    DesiredMotors()[port].store(command,std::memory_order_relaxed);
    auto& channel=Current();if(!channel.Lock())return;
    auto& shared=*channel.shared;
    if(shared.client==GetCurrentProcessId())shared.motors[port]=command;
    channel.Unlock();
#endif
}
void Stop(){
#if defined(_WIN32)
    std::lock_guard guard(LocalMutex());CloseChannel();
#endif
}
}
