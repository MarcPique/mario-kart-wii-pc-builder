#pragma once
#include "pc_local_session_control.h"
#include "pc_session_profile.h"
#include <algorithm>
#include <array>
#include <cwchar>
#include <map>
#include <optional>
#include <set>

// The launcher owns both processes and the containing window. It never reads
// guest memory, copies console identities, or fabricates network messages.
namespace LocalSessions {
using namespace PcLocalSessionControl;
inline std::string Read(const fs::path& path){
    std::ifstream input(path,std::ios::binary);std::array<char,513> bytes{};
    input.read(bytes.data(),bytes.size());
    if(input.gcount()>512)return {};
    return {bytes.data(),static_cast<size_t>(input.gcount())};
}
inline void Write(const fs::path& path,const std::string& text){
    auto temporary=path;temporary+=L".launcher-tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<text;Ensure(out.good(),"Unable to write local session status");}
    Ensure(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"Unable to publish local session status");
}
struct EnvironmentOrder {bool operator()(const std::wstring& a,const std::wstring& b)const{return _wcsicmp(a.c_str(),b.c_str())<0;}};
inline std::vector<wchar_t> Environment(const std::map<std::wstring,std::wstring,EnvironmentOrder>& changes){
    std::map<std::wstring,std::wstring,EnvironmentOrder> values;
    const auto block=GetEnvironmentStringsW();Ensure(block!=nullptr,"Unable to read process environment");
    for(auto p=block;*p;p+=wcslen(p)+1){std::wstring line(p);const auto equals=line.find(L'=',line[0]==L'='?1:0);if(equals!=line.npos)values[line.substr(0,equals)]=line.substr(equals+1);}
    FreeEnvironmentStringsW(block);
    for(const auto& [key,value]:changes){if(value.empty())values.erase(key);else values[key]=value;}
    std::vector<wchar_t> result;
    for(const auto& [key,value]:values){const auto line=key+L"="+value;result.insert(result.end(),line.begin(),line.end());result.push_back(0);}
    result.push_back(0);if(result.size()==1)result.push_back(0);return result;
}
inline bool Owned(HWND window,DWORD pid){DWORD actual=0;return window&&IsWindow(window)&&GetWindowThreadProcessId(window,&actual)&&actual==pid;}
inline void SetSetting(std::string& text,const std::string& section,const std::string& key,const std::string& value){
    const auto heading="["+section+"]";auto start=text.find(heading);
    if(start==text.npos){text+='\n'+heading+'\n'+key+" = "+value+'\n';return;}
    start+=heading.size();auto end=text.find("\n[",start);if(end==text.npos)end=text.size();
    for(auto line=text.find('\n',start);line!=text.npos&&line<end;line=text.find('\n',line+1)){
        auto first=text.find_first_not_of(" \t\r",line+1);if(first==text.npos||first>=end)break;
        if(text.compare(first,key.size(),key)!=0)continue;
        auto equals=text.find_first_not_of(" \t",first+key.size());if(equals==text.npos||text[equals]!='=')continue;
        auto last=text.find('\n',equals);if(last==text.npos||last>end)last=end;
        text.replace(first,last-first,key+" = "+value);return;
    }
    text.insert(start,"\n"+key+" = "+value);
}
inline std::string TomlPath(const fs::path& path){
    // A TOML literal string keeps Windows backslashes and Unicode intact.
    const auto utf8=path.generic_u8string();std::string value(utf8.begin(),utf8.end());
    // Apostrophes are valid Windows path characters, so use escaped basic strings.
    std::string quoted="\"";for(char c:value){if(c=='"'||c=='\\')quoted+='\\';quoted+=c;}return quoted+'"';
}
class Host {
    struct Surface {HWND window=nullptr,parent=nullptr;LONG_PTR style=0,extended=0;RECT rectangle{};bool visible=false;};
    fs::path root_,directory_,game_;
    DWORD primaryPid_=0,followerPid_=0;
    HANDLE follower_=nullptr;
    HANDLE job_=nullptr;
    HWND window_=nullptr;
    std::array<Surface,2> surfaces_{};
    std::optional<Request> active_;
    std::optional<State> primaryState_,followerState_;
    std::set<std::string> usedGroups_;
    uint64_t started_=0,stopping_=0;
    bool split_=true,closing_=false;
    std::ostream& log_;
    static constexpr wchar_t ClassName[]=L"MarioKartWiiLocalSessions";
    static LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
        auto* self=reinterpret_cast<Host*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE){self=static_cast<Host*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(self){
            if(message==WM_SIZE){self->Layout();return 0;}
            if(message==WM_SETFOCUS){if(self->surfaces_[0].window)SetFocus(self->surfaces_[0].window);return 0;}
            if(message==WM_CLOSE){self->closing_=true;for(auto& surface:self->surfaces_)if(surface.window)PostMessageW(surface.window,WM_CLOSE,0,0);return 0;}
        }
        return DefWindowProcW(window,message,wp,lp);
    }
    void Layout(){
        if(!window_)return;RECT area{};GetClientRect(window_,&area);
        for(unsigned i=0;i<2;++i)if(const auto child=surfaces_[i].window){
            const bool show=i==0||split_;
            if(show)SetWindowPos(child,nullptr,split_?int(i)*area.right/2:0,0,split_?(i==0?area.right/2:area.right-area.right/2):area.right,area.bottom,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
            else ShowWindow(child,SW_HIDE);
        }
    }
    void Attach(unsigned index,HWND child,DWORD pid){
        Ensure(Owned(child,pid)&&GetParent(child)==nullptr,"Local session window belongs to another owner");
        auto& surface=surfaces_[index];surface.window=child;surface.parent=GetParent(child);surface.style=GetWindowLongPtrW(child,GWL_STYLE);surface.extended=GetWindowLongPtrW(child,GWL_EXSTYLE);surface.visible=IsWindowVisible(child);GetWindowRect(child,&surface.rectangle);
        SetWindowLongPtrW(child,GWL_STYLE,(surface.style&~(WS_POPUP|WS_CAPTION|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU))|WS_CHILD|WS_CLIPSIBLINGS);
        SetWindowLongPtrW(child,GWL_EXSTYLE,surface.extended&~WS_EX_APPWINDOW);
        SetLastError(0);const auto previous=SetParent(child,window_);const auto error=GetLastError();
        Ensure(previous||!error,"Unable to contain local session window");
        SetWindowPos(child,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }
    void Restore(){
        for(auto& surface:surfaces_){if(surface.window&&IsWindow(surface.window)){
            SetParent(surface.window,surface.parent);SetWindowLongPtrW(surface.window,GWL_STYLE,surface.style);SetWindowLongPtrW(surface.window,GWL_EXSTYLE,surface.extended);
            const auto& r=surface.rectangle;SetWindowPos(surface.window,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED|(surface.visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
        }surface={};}
        if(window_){DestroyWindow(window_);window_=nullptr;}
    }
    void ReplyTo(const Request& request,unsigned result,unsigned error=0){Write(directory_/L"reply",Encode(Reply{request.group,primaryPid_,followerPid_,result,error}));}
    fs::path GroupDirectory()const{return directory_/fs::path(active_->group);}
    void ConfigureFollower(const fs::path& profile){
        fs::create_directories(profile);const auto path=profile/L"Config.toml";
        // Only preferences are inherited. Saves, NAND, controller mappings and
        // console keys are deliberately created by the second client itself.
        std::ifstream input(fs::exists(path)?path:root_/L"UserData"/L"Config.toml",std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(input)),{});input.close();
        SetSetting(text,"paths","dvd_root",TomlPath(root_/L"GameData"/L"Install"/L"GameAssets"/L"DATA"));
        SetSetting(text,"paths","retro_rewind_root",TomlPath(root_/L"GameData"/L"RetroRewind6"));
        SetSetting(text,"video","display_mode","\"windowed\"");SetSetting(text,"video","window_width","640");SetSetting(text,"video","window_height","720");
        SetSetting(text,"audio","muted","true");Write(path,text);
    }
    void Start(const Request& request){
        if(active_){if(active_->group==request.group&&active_->players==request.players)ReplyTo(request,0);else ReplyTo(request,2,ERROR_BUSY);return;}
        const auto primaryWindow=reinterpret_cast<HWND>(static_cast<uintptr_t>(request.primaryWindow));
        if(!Owned(primaryWindow,primaryPid_)||GetParent(primaryWindow)||usedGroups_.contains(request.group)){ReplyTo(request,2,ERROR_INVALID_PARAMETER);return;}
        usedGroups_.insert(request.group);
        active_=request;started_=GetTickCount64();closing_=false;stopping_=0;split_=true;
        fs::create_directories(GroupDirectory());
        const char* inheritedProfile=std::getenv("MKW_PC_PROFILE");
        const std::string name=inheritedProfile&&std::string_view(inheritedProfile)=="online-pair-2"?"online-pair-3":"online-pair-2";
        ConfigureFollower(PcSessionProfile::Directory(root_/L"UserData",name));
        auto environment=Environment({{L"MKW_PC_PROFILE",std::wstring(name.begin(),name.end())},{L"MKW_PC_LOCAL_ROLE",L"follower"},{L"MKW_PC_LOCAL_INPUT",std::wstring(request.group.begin(),request.group.end())},{L"MKW_PC_SESSION_PRIMARY_PID",std::to_wstring(primaryPid_)},{L"MKW_PC_SESSION_PLAYERS",std::to_wstring(request.players)},{L"MKW_PC_LAUNCHER_PID",std::to_wstring(GetCurrentProcessId())}});
        std::wstring command=L"\""+game_.native()+L"\"";STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
        job_=CreateJobObjectW(nullptr,nullptr);Ensure(job_!=nullptr,"Unable to create local session job");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        Ensure(SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limits,sizeof(limits)),"Unable to protect local session lifecycle");
        Ensure(CreateProcessW(game_.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_UNICODE_ENVIRONMENT|CREATE_SUSPENDED,environment.data(),game_.parent_path().c_str(),&startup,&process),"Unable to start second local session");
        follower_=process.hProcess;followerPid_=process.dwProcessId;
        if(!AssignProcessToJobObject(job_,follower_)){const auto error=GetLastError();TerminateProcess(follower_,1);CloseHandle(process.hThread);SetLastError(error);Ensure(false,"Unable to own second local session");}
        const auto resumed=ResumeThread(process.hThread);CloseHandle(process.hThread);Ensure(resumed!=DWORD(-1),"Unable to resume second local session");
        WNDCLASSW cls{};cls.lpfnWndProc=Procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=ClassName;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(cls.hInstance,MAKEINTRESOURCEW(1));
        Ensure(RegisterClassW(&cls)||GetLastError()==ERROR_CLASS_ALREADY_EXISTS,"Unable to register local game window");
        RECT rectangle{};GetWindowRect(reinterpret_cast<HWND>(static_cast<uintptr_t>(request.primaryWindow)),&rectangle);
        window_=CreateWindowExW(0,ClassName,L"Mario Kart Wii",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,rectangle.left,rectangle.top,std::max(960L,rectangle.right-rectangle.left),std::max(540L,rectangle.bottom-rectangle.top),nullptr,nullptr,cls.hInstance,this);
        Ensure(window_!=nullptr,"Unable to create local game window");Attach(0,reinterpret_cast<HWND>(static_cast<uintptr_t>(request.primaryWindow)),primaryPid_);Layout();
        ShowWindow(window_,SW_SHOW);ShowWindow(window_,SW_SHOW);SetForegroundWindow(window_);
        ReplyTo(request,0);log_<<"Local online session started with "<<request.players<<" players\n"<<std::flush;
    }
    std::optional<State> Status(const wchar_t* name,DWORD pid){
        auto state=ParseState(Read(GroupDirectory()/name));
        if(!state||state->group!=active_->group||state->pid!=pid||state->stamp>GetTickCount64()||GetTickCount64()-state->stamp>5000)return {};
        if(state->window&&!Owned(reinterpret_cast<HWND>(static_cast<uintptr_t>(state->window)),pid))return {};
        return state;
    }
    void Finish(unsigned result,unsigned error=0){
        if(!active_)return;const auto request=*active_;
        Restore();if(follower_){CloseHandle(follower_);follower_=nullptr;}if(job_){CloseHandle(job_);job_=nullptr;}followerPid_=0;primaryState_.reset();followerState_.reset();active_.reset();stopping_=0;
        ReplyTo(request,result,error);log_<<"Local online session stopped ("<<error<<")\n"<<std::flush;
    }
public:
    Host(const fs::path& root,DWORD primaryPid,std::ostream& log):root_(root),directory_(root/L"UserData"/L"Online"/L"LocalSessions"/std::to_wstring(primaryPid)),game_(root/L"GameData"/L"Install"/L"RetroRewind"/L"RetroRewind.exe"),primaryPid_(primaryPid),log_(log){
        fs::create_directories(directory_);
        for(const auto name:{L"request",L"reply"}){std::error_code error;fs::remove(directory_/name,error);}
        Write(directory_/L"ready","MKW_READY1\n"+std::to_string(GetCurrentProcessId())+'\n'+std::to_string(primaryPid_)+"\nend\n");
    }
    Host(const Host&)=delete;
    ~Host(){
        // Main has already exited here. Give only our own child time to flush
        // its saves; a stuck child must not remain as an invisible game.
        Stop();if(follower_&&WaitForSingleObject(follower_,5000)==WAIT_TIMEOUT){TerminateProcess(follower_,1);WaitForSingleObject(follower_,1000);}
        try{Finish(1);}catch(...){if(follower_)CloseHandle(follower_);if(job_)CloseHandle(job_);Restore();}
        std::error_code error;fs::remove(directory_/L"ready",error);
    }
    void Stop(){
        if(!active_||stopping_)return;stopping_=GetTickCount64();
        if(surfaces_[1].window)PostMessageW(surfaces_[1].window,WM_CLOSE,0,0);
        else if(followerPid_)EnumWindows([](HWND window,LPARAM pid)->BOOL{if(Owned(window,static_cast<DWORD>(pid)))PostMessageW(window,WM_CLOSE,0,0);return TRUE;},followerPid_);
    }
    void Tick(){
        MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        const auto path=directory_/L"request";
        if(fs::exists(path)){
            const auto request=ParseRequest(Read(path));std::error_code ignored;fs::remove(path,ignored);
            if(request&&request->primaryPid==primaryPid_){
                if(request->action==Action::Stop){if(active_&&active_->group==request->group)Stop();else ReplyTo(*request,1);}
                else try{Start(*request);}catch(const std::exception& error){const DWORD code=GetLastError()?GetLastError():ERROR_INVALID_DATA;log_<<"Local online start failed: "<<error.what()<<'\n'<<std::flush;if(active_)Stop();else ReplyTo(*request,2,code);}
            }
        }
        if(!active_)return;
        if(!follower_){Finish(2,ERROR_INVALID_DATA);return;}
        if(WaitForSingleObject(follower_,0)==WAIT_OBJECT_0){DWORD code=1;GetExitCodeProcess(follower_,&code);Finish(stopping_?1:2,code);return;}
        if(stopping_){if(GetTickCount64()-stopping_>5000){TerminateProcess(follower_,1);}return;}
        primaryState_=Status(L"primary.state",primaryPid_);followerState_=Status(L"follower.state",followerPid_);
        if(followerState_&&followerState_->window&&!surfaces_[1].window){
            try{Attach(1,reinterpret_cast<HWND>(static_cast<uintptr_t>(followerState_->window)),followerPid_);Layout();}
            catch(const std::exception& error){log_<<error.what()<<'\n'<<std::flush;Stop();}
        }
        if(!surfaces_[1].window&&GetTickCount64()-started_>120000){log_<<"Second local session did not create a ready window\n"<<std::flush;Stop();}
        // Keep setup and independent selections visible. Advanced settings in
        // the primary can use the full window after both clients are online.
        const bool split=!(primaryState_&&followerState_&&primaryState_->phase==Phase::Menu&&(followerState_->phase==Phase::Online||followerState_->phase==Phase::Lobby));
        if(split_!=split){split_=split;Layout();}
    }
};
}
