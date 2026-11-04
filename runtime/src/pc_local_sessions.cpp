#include "pc_local_sessions.h"
#include "pc_local_input.h"
#include "runtime_config.h"
#include "runtime_log.h"
#include <aurora/aurora.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_properties.h>
#include <array>
#include <cstdlib>
#include <fstream>
#if defined(_WIN32)
#include <objbase.h>
#endif

namespace PcLocalSessions {
namespace {
using namespace PcLocalSessionControl;
std::string group;
unsigned players=2,lastError=0;
bool ending=false,leftMain=false;
uint64_t reportedAt=0,requestAt=0;
#if defined(_WIN32)
AuroraDisplayMode previousDisplay=AURORA_DISPLAY_MODE_WINDOWED;
std::string Read(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);std::array<char,513> bytes{};input.read(bytes.data(),bytes.size());
    if(input.gcount()>512)return {};return {bytes.data(),static_cast<size_t>(input.gcount())};
}
uint32_t EnvironmentPid(const char* key){uint32_t value=0;const auto* raw=std::getenv(key);return raw&&Number(std::string_view(raw),value)?value:0;}
uint32_t PrimaryPid(){return PcLocalInput::Follower()?EnvironmentPid("MKW_PC_SESSION_PRIMARY_PID"):GetCurrentProcessId();}
std::filesystem::path Directory(){
    // Validate the frozen profile before resolving shared IPC paths, just as
    // ordinary mutable profile paths do. IPC contains no save/account data.
    PcSessionProfile::Current();return RuntimeConfigFile::BaseApplicationDataDirectory()/"Online"/"LocalSessions"/std::to_string(PrimaryPid());
}
bool Write(const std::filesystem::path& path,const std::string& text){
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);if(error)return false;
    auto temporary=path;temporary+=".native-tmp";
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output<<text;if(!output.good())return false;}
    return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
uint64_t Window(){
    auto* window=aurora_get_window();if(!window)return 0;
    const auto native=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
    // Do not let the containing window expose a client before its normal
    // background shader/startup preparation has made the game ready to show.
    return native&&IsWindowVisible(native)?reinterpret_cast<uintptr_t>(native):0;
}
bool LauncherAlive(){
    const auto launcher=EnvironmentPid("MKW_PC_LAUNCHER_PID");if(!launcher)return false;
    const auto ready=ParseReady(Read(Directory()/"ready"));if(!ready||ready->launcherPid!=launcher||ready->primaryPid!=PrimaryPid())return false;
    const auto process=OpenProcess(SYNCHRONIZE,FALSE,launcher);if(!process)return false;
    const bool alive=WaitForSingleObject(process,0)==WAIT_TIMEOUT;CloseHandle(process);return alive;
}
void Restore(){
    PcLocalInput::EndHost();group.clear();ending=false;leftMain=false;reportedAt=0;
    // These are also the normal interactive runtime preferences in main.cpp.
    aurora_set_background_input(true);aurora_set_pause_on_focus_lost(false);
    if(aurora_get_display_mode()==AURORA_DISPLAY_MODE_WINDOWED&&previousDisplay!=AURORA_DISPLAY_MODE_WINDOWED)aurora_set_display_mode(previousDisplay);
}
#endif
}
unsigned Players(){
    const auto count=RuntimeConfigFile::Get().localOnlinePlayers.value_or(2);
    return count>2&&!Available()?2:count;
}
bool Available(){
#if defined(_WIN32)
    return !PcLocalInput::Follower()&&LauncherAlive();
#else
    return false;
#endif
}
bool SetPlayers(unsigned count){
    if(count<1||count>4||(!Available()&&count>2)||Active())return false;
    if(!RuntimeConfigFile::WriteSetting("retro","local_online_players",std::to_string(count)))return false;
    RuntimeConfigFile::Mutable().localOnlinePlayers=count;return true;
}
bool Active(){return !group.empty();}
unsigned Error(){return lastError;}
bool Begin(){
#if defined(_WIN32)
    const auto count=Players();if(count<3||!Available())return false;if(Active())return !ending&&players==count;
    GUID id{};if(FAILED(CoCreateGuid(&id)))return false;
    constexpr char hex[]="0123456789abcdef";std::string candidate;
    const auto* bytes=reinterpret_cast<const unsigned char*>(&id);
    for(unsigned i=0;i<sizeof(id);++i){candidate+=hex[bytes[i]>>4];candidate+=hex[bytes[i]&15];}
    const auto window=Window();if(!window||!PcLocalInput::BeginHost(candidate))return false;
    previousDisplay=aurora_get_display_mode();if(previousDisplay!=AURORA_DISPLAY_MODE_WINDOWED)aurora_set_display_mode(AURORA_DISPLAY_MODE_WINDOWED);
    aurora_set_background_input(true);aurora_set_pause_on_focus_lost(false);
    group=candidate;players=count;lastError=0;ending=false;leftMain=false;requestAt=GetTickCount64();reportedAt=0;
    if(!Write(Directory()/"request",Encode(Request{Action::Start,group,GetCurrentProcessId(),window,players}))){lastError=GetLastError();Restore();return false;}
    RT_LOGF("pc-local-sessions","Requested %u local online players\n",players);return true;
#else
    return false;
#endif
}
void End(){
#if defined(_WIN32)
    if(!Active()||ending||PcLocalInput::Follower())return;
    ending=true;requestAt=GetTickCount64();
    if(!Write(Directory()/"request",Encode(Request{Action::Stop,group,GetCurrentProcessId(),Window(),players})))lastError=GetLastError();
#endif
}
void Stop(){
#if defined(_WIN32)
    End();if(!PcLocalInput::Follower()&&Active())Restore();
#endif
}
void Report(unsigned section,unsigned page,Phase phase,unsigned totalPlayers,unsigned localPlayers){
#if defined(_WIN32)
    const bool follower=PcLocalInput::Follower();
    if(follower&&group.empty()){
        const auto* raw=std::getenv("MKW_PC_LOCAL_INPUT");if(!raw||!ValidGroup(raw)||!PrimaryPid())return;group=raw;
        unsigned count=0;const auto* selected=std::getenv("MKW_PC_SESSION_PLAYERS");players=selected&&Number(std::string_view(selected),count)&&(count==3||count==4)?count:4;
    }
    if(!Active())return;const auto now=GetTickCount64();if(reportedAt&&now-reportedAt<250)return;reportedAt=now;
    if(!follower){
        const auto reply=ParseReply(Read(Directory()/"reply"));
        if(reply&&reply->group==group&&reply->primaryPid==GetCurrentProcessId()&&reply->result!=0){lastError=reply->result==2?reply->error:0;Restore();return;}
        if(!LauncherAlive()){lastError=ERROR_BROKEN_PIPE;Restore();return;}
        if(now-requestAt>15000&&(!reply||reply->group!=group)&&!ending){lastError=WAIT_TIMEOUT;End();}
        if(page!=90)leftMain=true;else if(leftMain&&!ending)End();
    }
    const State state{group,GetCurrentProcessId(),Window(),now,section,page,phase,totalPlayers,localPlayers};
    Write(Directory()/group/(follower?"follower.state":"primary.state"),Encode(state));
#endif
}
}
