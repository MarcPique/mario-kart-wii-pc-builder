#include "pc_local_input.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>

// Integration test: run the real shared-memory channel in two OS processes.
// Does not open a game, SDL device, account or network connection.
int main(int argc,char** argv){
    using namespace PcLocalInput;
    if(argc==2){
        const bool liveness=std::string(argv[1])=="--liveness";
        assert(liveness||std::string(argv[1])=="--follower");Initialize();assert(Follower());
        const std::string group=std::getenv("MKW_PC_LOCAL_INPUT");
        const auto prefix=std::wstring(L"Local\\MkwInputTest_")+std::wstring(group.begin(),group.end());
        auto ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,(prefix+L"_ready").c_str());
        auto stale=OpenEventW(SYNCHRONIZE,FALSE,(prefix+L"_stale").c_str());assert(ready&&stale);
        Pads pads;const auto deadline=GetTickCount64()+5000;
        do{pads=Receive();if(pads[0][10]==0)break;Sleep(5);}while(GetTickCount64()<deadline);
        if(liveness){
            assert(pads[0][10]==0);SetEvent(ready);assert(WaitForSingleObject(stale,5000)==WAIT_OBJECT_0);
            assert(Receive()==Disconnected());Stop();CloseHandle(ready);CloseHandle(stale);return 0;
        }
        assert(BigEndian::Read16(pads[0].data())==0x400&&BigEndian::Read16(pads[1].data())==0x800);
        assert(pads[0][2]==static_cast<uint8_t>(-88)&&pads[1][6]==255);
        assert(pads[2][10]==255&&pads[3][10]==255);
        Motor(0,1);Motor(1,2);SetEvent(ready);
        assert(WaitForSingleObject(stale,5000)==WAIT_OBJECT_0);
        assert(BigEndian::Read16(Receive()[0].data())==0x100);
        assert(BigEndian::Read16(Receive()[0].data())==0);
        Sleep(350);const auto neutral=Receive();assert(neutral[0][10]==0&&neutral[1][10]==0);
        assert(BigEndian::Read16(neutral[0].data())==0&&neutral[0][2]==0&&neutral[1][6]==0);
        Stop();CloseHandle(ready);CloseHandle(stale);return 0;
    }
    LARGE_INTEGER counter;QueryPerformanceCounter(&counter);char group[33];
    std::snprintf(group,sizeof(group),"%016llx%016llx",static_cast<unsigned long long>(counter.QuadPart),static_cast<unsigned long long>(GetCurrentProcessId()));
    assert(_putenv_s("MKW_PC_LOCAL_INPUT","")==0);assert(_putenv_s("MKW_PC_LOCAL_ROLE","")==0);
    Initialize();assert(!Host()&&!Follower());assert(!EndHost());
    assert(!BeginHost("../invalid"));const auto initialGeneration=Generation();
    assert(BeginHost(group)&&Host()&&!Follower());assert(Generation()>initialGeneration);
    const auto firstGeneration=Generation();assert(BeginHost(group)&&Generation()==firstGeneration);
    auto otherGroup=std::string(group);otherGroup[0]=otherGroup[0]=='a'?'b':'a';assert(!BeginHost(otherGroup));
    assert(_putenv_s("MKW_PC_LOCAL_INPUT",group)==0);
    const auto prefix=std::wstring(L"Local\\MkwInputTest_")+std::wstring(group,group+32);
    auto ready=CreateEventW(nullptr,TRUE,FALSE,(prefix+L"_ready").c_str());
    auto stale=CreateEventW(nullptr,TRUE,FALSE,(prefix+L"_stale").c_str());assert(ready&&stale);
    Pads pads;for(unsigned p=0;p<4;++p)pads[p]=PadStatusContract::Encode({static_cast<uint16_t>(0x100<<p),static_cast<int8_t>(-90+p),75,20,-30,255,17,48,64,0});
    Publish(pads);
    wchar_t executable[32768];assert(GetModuleFileNameW(nullptr,executable,32768));
    std::wstring command=L"\""+std::wstring(executable)+L"\" --follower";
    SetEnvironmentVariableA("MKW_PC_LOCAL_ROLE","follower");
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION child{};
    assert(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child));
    CloseHandle(child.hThread);
    const auto deadline=GetTickCount64()+5000;
    while(WaitForSingleObject(ready,0)!=WAIT_OBJECT_0&&GetTickCount64()<deadline){Publish(pads);Sleep(5);}
    assert(WaitForSingleObject(ready,0)==WAIT_OBJECT_0);
    const auto feedback=Publish(pads);assert(feedback.connected&&feedback.motors[0]==1&&feedback.motors[1]==2);
    BigEndian::Write16(pads[2].data(),0);Publish(pads);
    BigEndian::Write16(pads[2].data(),0x100);Publish(pads);
    BigEndian::Write16(pads[2].data(),0);Publish(pads);
    SetEvent(stale);assert(WaitForSingleObject(child.hProcess,5000)==WAIT_OBJECT_0);
    DWORD exitCode=1;assert(GetExitCodeProcess(child.hProcess,&exitCode)&&exitCode==0);
    const auto stopped=Publish(pads);assert(!stopped.connected&&stopped.motors[0]==0&&stopped.motors[1]==0);
    CloseHandle(child.hProcess);ResetEvent(ready);ResetEvent(stale);
    command=L"\""+std::wstring(executable)+L"\" --liveness";
    assert(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child));CloseHandle(child.hThread);
    const auto closeDeadline=GetTickCount64()+5000;
    while(WaitForSingleObject(ready,0)!=WAIT_OBJECT_0&&GetTickCount64()<closeDeadline){Publish(pads);Sleep(5);}
    assert(WaitForSingleObject(ready,0)==WAIT_OBJECT_0);assert(EndHost()&&!Host());SetEvent(stale);
    assert(WaitForSingleObject(child.hProcess,5000)==WAIT_OBJECT_0);
    assert(GetExitCodeProcess(child.hProcess,&exitCode)&&exitCode==0);
    CloseHandle(child.hProcess);CloseHandle(ready);CloseHandle(stale);
    assert(BeginHost(otherGroup)&&Host());assert(Generation()>firstGeneration);
    assert(EndHost()&&!Host()&&!Follower());
    std::cout<<"Two-process input: J3/J4, analog input, short taps, rumble, loading pause and live enable/disable/re-enable without process restart passed\n";
}
