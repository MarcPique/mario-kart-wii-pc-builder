// Harmless Win32 stand-in for the game. Tests the actual product launcher in
// two processes without game data, SDL devices, accounts or any networking.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <array>
#include <stdexcept>
#include "pc_local_session_control.h"
namespace fs=std::filesystem;
using namespace PcLocalSessionControl;
fs::path root,directory;
HWND ownWindow=nullptr;
void Check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}
void Write(const fs::path& path,const std::string& text){
    fs::create_directories(path.parent_path());auto tmp=path;tmp+=L".fixture-tmp";
    {std::ofstream out(tmp,std::ios::binary);out<<text;Check(out.good(),"fixture write");}
    Check(MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"fixture publication");
}
std::string Read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
void Pump(){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(10);}
template<class Condition>void Wait(Condition ready,const char* why,unsigned milliseconds=10000){const auto end=GetTickCount64()+milliseconds;while(!ready()){Check(GetTickCount64()<end,why);Pump();}}
LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_CLOSE){DestroyWindow(window);return 0;}
    return DefWindowProcW(window,message,wp,lp);
}
void Status(const std::string& group,const wchar_t* name,Phase phase){
    Write(directory/fs::path(group)/name,Encode(State{group,GetCurrentProcessId(),reinterpret_cast<uintptr_t>(ownWindow),GetTickCount64(),116,67,phase,4,2}));
}
void Primary(){
    directory=root/L"UserData"/L"Online"/L"LocalSessions"/std::to_wstring(GetCurrentProcessId());
    Wait([&]{const auto ready=ParseReady(Read(directory/L"ready"));return ready&&ready->primaryPid==GetCurrentProcessId();},"launcher capability handshake");
    const auto style=GetWindowLongPtrW(ownWindow,GWL_STYLE),extended=GetWindowLongPtrW(ownWindow,GWL_EXSTYLE);
    RECT originalRectangle{};GetWindowRect(ownWindow,&originalRectangle);const auto originalDpi=GetDpiForWindow(ownWindow);
    const auto profile=root/L"UserData"/L"Profiles"/L"online-pair-2";
    Check(!fs::exists(profile/L"Saves"),"follower save fixture must start absent");
    std::array<std::string,3> groups={"0123456789abcdef0123456789abcdef","1123456789abcdef0123456789abcdef","2123456789abcdef0123456789abcdef"};
    for(unsigned round=0;round<3;++round){
        Request request{Action::Start,groups[round],GetCurrentProcessId(),reinterpret_cast<uintptr_t>(ownWindow),round==1?3u:4u};
        Status(request.group,L"primary.state",Phase::Race);Write(directory/L"request",Encode(request));
        std::optional<Reply> reply;
        Wait([&]{reply=ParseReply(Read(directory/L"reply"));return reply&&reply->group==request.group;},"start acknowledgement");
        Check(reply->result==0&&reply->followerPid&&reply->followerPid!=GetCurrentProcessId(),"distinct second process");
        const auto follower=OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE,FALSE,reply->followerPid);Check(follower!=nullptr,"open owned second process");
        Wait([&]{return GetParent(ownWindow)!=nullptr&&fs::exists(profile/L"fixture-environment.txt");},"window containment and child environment");
        auto followerStatus=ParseState(Read(directory/fs::path(request.group)/L"follower.state"));
        Wait([&]{followerStatus=ParseState(Read(directory/fs::path(request.group)/L"follower.state"));return followerStatus&&GetParent(reinterpret_cast<HWND>(static_cast<uintptr_t>(followerStatus->window)))==GetParent(ownWindow);},"two surfaces in one window");
        const auto container=GetParent(ownWindow);DWORD containerPid=0;GetWindowThreadProcessId(container,&containerPid);
        const auto ready=ParseReady(Read(directory/L"ready"));Check(ready&&containerPid==ready->launcherPid,"container belongs to launcher");
        wchar_t title[80]{};GetWindowTextW(container,title,80);Check(std::wstring(title)==L"Mario Kart Wii","original game window title");
        Wait([&]{RECT first{},second{};GetWindowRect(ownWindow,&first);GetWindowRect(reinterpret_cast<HWND>(static_cast<uintptr_t>(followerStatus->window)),&second);return first.right<=second.left+1&&first.top==second.top;},"independent adjacent views");
        const auto environment=Read(profile/L"fixture-environment.txt");
        Check(environment.find("profile=online-pair-2\n")!=environment.npos&&environment.find("players="+std::to_string(request.players)+"\n")!=environment.npos&&environment.find("role=follower\n")!=environment.npos,"isolated profile and local player count");
        Check(!fs::exists(profile/L"Saves")&&!fs::exists(profile/L"NAND"),"no cloned saves or console identity");
        Check(Read(root/L"UserData"/L"Saves"/L"sentinel")=="primary-private-data","primary save preserved");
        // Repeating an active request must not create a second child.
        Write(directory/L"request",Encode(request));const auto until=GetTickCount64()+120;while(GetTickCount64()<until)Pump();
        const auto repeated=ParseReply(Read(directory/L"reply"));Check(repeated&&repeated->followerPid==reply->followerPid,"idempotent active start");
        if(round==2)Check(TerminateProcess(follower,37),"simulate child failure");
        else{request.action=Action::Stop;Write(directory/L"request",Encode(request));}
        Wait([&]{const auto stopped=ParseReply(Read(directory/L"reply"));return stopped&&stopped->group==request.group&&stopped->result==(round==2?2u:1u);},"stop or crash acknowledgement");
        Check(WaitForSingleObject(follower,0)==WAIT_OBJECT_0,"no orphaned follower process");CloseHandle(follower);
        Check(GetParent(ownWindow)==nullptr&&GetWindowLongPtrW(ownWindow,GWL_STYLE)==style&&GetWindowLongPtrW(ownWindow,GWL_EXSTYLE)==extended,"primary window fully restored without restart");
        Wait([&]{RECT restored{};GetWindowRect(ownWindow,&restored);return EqualRect(&restored,&originalRectangle)&&GetDpiForWindow(ownWindow)==originalDpi;},"window size, position and DPI restored");
    }
    // Old groups and foreign windows cannot start a session.
    Request replay{Action::Start,groups[0],GetCurrentProcessId(),reinterpret_cast<uintptr_t>(ownWindow),4};
    Write(directory/L"request",Encode(replay));Wait([&]{const auto response=ParseReply(Read(directory/L"reply"));return response&&response->group==replay.group&&response->result==2;},"reject reused group");
    replay.group="3123456789abcdef0123456789abcdef";replay.primaryWindow=reinterpret_cast<uintptr_t>(GetDesktopWindow());
    Write(directory/L"request",Encode(replay));Wait([&]{const auto response=ParseReply(Read(directory/L"reply"));return response&&response->group==replay.group&&response->result==2;},"reject foreign HWND");
    Check(GetParent(ownWindow)==nullptr,"invalid request has no window side effect");
    Write(root/L"fixture-result.txt","PASS: 4/3-player start, two real processes, one owned window, profile isolation, independent saves, idempotency, stop, same-process restart, child crash recovery, stale groups and foreign windows\n");
}
void Follower(){
    const auto* rawGroup=std::getenv("MKW_PC_LOCAL_INPUT");Check(rawGroup&&ValidGroup(rawGroup),"follower group");
    const auto* primary=std::getenv("MKW_PC_SESSION_PRIMARY_PID");Check(primary!=nullptr,"follower primary identifier");
    directory=root/L"UserData"/L"Online"/L"LocalSessions"/primary;
    const auto profile=root/L"UserData"/L"Profiles"/std::getenv("MKW_PC_PROFILE");
    Write(profile/L"fixture-environment.txt",std::string("profile=")+std::getenv("MKW_PC_PROFILE")+"\nrole="+std::getenv("MKW_PC_LOCAL_ROLE")+"\nplayers="+std::getenv("MKW_PC_SESSION_PLAYERS")+'\n');
    while(IsWindow(ownWindow)){Status(rawGroup,L"follower.state",Phase::Race);for(unsigned i=0;i<5&&IsWindow(ownWindow);++i)Pump();}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    try{
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        std::wstring executable(32768,0);executable.resize(GetModuleFileNameW(nullptr,executable.data(),executable.size()));
        root=fs::path(executable).parent_path().parent_path().parent_path().parent_path();
        Check(fs::exists(root/L"fixture-owned.txt"),"fixture install marker");
        WNDCLASSW cls{};cls.lpfnWndProc=Procedure;cls.hInstance=instance;cls.lpszClassName=L"MkwLauncherFixture";Check(RegisterClassW(&cls)!=0,"fixture class");
        ownWindow=CreateWindowExW(WS_EX_APPWINDOW,cls.lpszClassName,L"Session lifecycle fixture",WS_OVERLAPPEDWINDOW,100,100,1000,600,nullptr,nullptr,instance,nullptr);Check(ownWindow!=nullptr,"fixture window");ShowWindow(ownWindow,SW_SHOW);
        const auto* role=std::getenv("MKW_PC_LOCAL_ROLE");if(role&&std::string_view(role)=="follower")Follower();else Primary();
        if(IsWindow(ownWindow))DestroyWindow(ownWindow);return 0;
    }catch(const std::exception& error){if(!root.empty())Write(root/(std::getenv("MKW_PC_LOCAL_ROLE")?L"fixture-follower-failure.txt":L"fixture-failure.txt"),error.what());return 1;}
}
