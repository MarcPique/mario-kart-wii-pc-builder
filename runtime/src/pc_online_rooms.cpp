#include "pc_online_rooms.h"
#include "runtime_config.h"
#include "runtime_log.h"
#include "pc_room_parser.h"
#include "pc_menu_locale.h"
#include "pc_rr_update_manifest.h"
#include "nand_path.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <fstream>
#ifdef _WIN32
#include <winhttp.h>
#endif
namespace PcOnlineRooms { namespace {
std::mutex mutex;std::condition_variable wake;Snapshot state;
bool stopping=false,requested=false,updateRequested=false;std::thread worker;
UpdateSnapshot updateState;
using Clock=std::chrono::steady_clock;
Clock::time_point lastRequest{};
Clock::time_point lastUpdate{};
#ifdef _WIN32
struct HttpHandle {HINTERNET value;explicit HttpHandle(HINTERNET v):value(v){}~HttpHandle(){if(value)WinHttpCloseHandle(value);}operator HINTERNET()const{return value;}};
std::string Download(const wchar_t* host,const wchar_t* path) {
    HttpHandle session(WinHttpOpen(L"MarioKartWii-PC/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value)throw std::runtime_error("HTTP initialization");
    WinHttpSetTimeouts(session,3000,3000,3000,3000);
    HttpHandle connection(WinHttpConnect(session,host,INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection.value)throw std::runtime_error("HTTP connection");
    HttpHandle request(WinHttpOpenRequest(connection,L"GET",path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request.value)throw std::runtime_error("HTTP request");
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
    if(!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))throw std::runtime_error("HTTP unavailable");
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)throw std::runtime_error("HTTP status");
    std::string data;char buffer[8192];DWORD got=0;
    do {if(!WinHttpReadData(request,buffer,sizeof(buffer),&got))throw std::runtime_error("HTTP read");if(data.size()+got>1024*1024)throw std::runtime_error("Oversize room response");data.append(buffer,got);}while(got);
    return data;
}
#else
std::string Download(const wchar_t*,const wchar_t*){throw std::runtime_error("Room service requires Windows");}
#endif
UpdateSnapshot CheckUpdates() {
    UpdateSnapshot result;result.checked=true;
    const auto root=RuntimeConfigFile::RetroRewindRoot();
    if(root.empty())throw std::runtime_error("Retro Rewind root missing");
    std::ifstream file(RuntimeNandPath::ResolveConfiguredPath(root)/"version.txt");
    std::string installed;if(!std::getline(file,installed))throw std::runtime_error("Retro Rewind version missing");
    if(!installed.empty()&&installed.back()=='\r')installed.pop_back();
    const auto current=PcRetroUpdate::ParseVersion(installed);
    const auto latest=PcRetroUpdate::Latest(Download(L"update.rwfc.net",L"/RetroRewind/RetroRewindVersion.txt"));
    if(!current||!latest)throw std::runtime_error("Invalid update version");
    result.installed=PcRetroUpdate::Format(*current);result.latest=PcRetroUpdate::Format(*latest);
    result.newer=*latest>*current;result.available=true;return result;
}
void Save(const Snapshot& snapshot) {
    // Persist only public room IDs and aggregate counts. Never send save/profile data.
    nlohmann::json json;json["source"]="https://rwfc.net/api/roomstatus";json["checked_at"]=std::time(nullptr);json["players"]=snapshot.players;json["rooms"]=nlohmann::json::array();
    for(const auto& r:snapshot.rooms)json["rooms"].push_back({{"id",r.id},{"type",r.type},{"players",r.players}});
    auto dir=RuntimeConfigFile::ApplicationDataDirectory()/"Online";std::filesystem::create_directories(dir);
    auto temporary=dir/"rooms.json.tmp",target=dir/"rooms.json";
    {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file<<json.dump(2);if(!file)throw std::runtime_error("Room cache write");}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Room cache replace");
#else
    std::filesystem::rename(temporary,target);
#endif
}
void Run() {
    std::unique_lock lock(mutex);
    while(!stopping) {
        const auto now=Clock::now();
        const bool checkUpdate=updateRequested || lastUpdate==Clock::time_point{} || now-lastUpdate>=std::chrono::hours(6);
        if(checkUpdate)lastUpdate=now;
        requested=false;updateRequested=false;lastRequest=now;lock.unlock();
        try {
            auto next=ParseRooms(Download(L"rwfc.net",L"/api/roomstatus"));
            try {Save(next);}catch(const std::exception& e){RT_LOGF("online-rooms","Cache: %s\n",e.what());}
            lock.lock();next.revision=state.revision+1;state=std::move(next);
            RT_LOGF("online-rooms","Updated automatically: %zu rooms, %u players\n",state.rooms.size(),state.players);
        }catch(const std::exception& e){lock.lock();state.available=false;state.checked=true;++state.revision;RT_LOGF("online-rooms","Room check unavailable: %s\n",e.what());}
        if(checkUpdate){
            lock.unlock();
            try {auto next=CheckUpdates();lock.lock();updateState=std::move(next);RT_LOGF("pc-update","Installed %s; latest %s; update %s\n",updateState.installed.c_str(),updateState.latest.c_str(),updateState.newer?"available":"not needed");}
            catch(const std::exception& e){lock.lock();updateState.checked=true;updateState.available=false;RT_LOGF("pc-update","Check unavailable: %s\n",e.what());}
        }
        wake.wait_for(lock,std::chrono::seconds(40),[]{return stopping||requested||updateRequested;});
    }
}
}
void Start(){std::lock_guard lock(mutex);if(worker.joinable())return;if(!RuntimeConfigFile::Get().networkEnabled.value_or(false)){updateState.checked=true;updateState.available=false;return;}stopping=false;worker=std::thread(Run);}
void Stop(){{std::lock_guard lock(mutex);stopping=true;wake.notify_all();}if(worker.joinable())worker.join();}
void Refresh(){std::lock_guard lock(mutex);if(Clock::now()-lastRequest<std::chrono::seconds(10))return;requested=true;wake.notify_all();}
Snapshot Get(){std::lock_guard lock(mutex);return state;}
void RefreshUpdates(){std::lock_guard lock(mutex);if(!worker.joinable()){updateState.checked=true;updateState.available=false;return;}if(Clock::now()-lastUpdate<std::chrono::seconds(10))return;updateState.checked=false;updateRequested=true;RT_LOGF("pc-update","Manual version check requested\n");wake.notify_all();}
UpdateSnapshot GetUpdates(){std::lock_guard lock(mutex);return updateState;}
std::u16string UpdateSummary(){const auto s=GetUpdates();using PcMenuLocale::T;if(!s.checked)return T(u"Sin comprobar",u"Not checked",u"Non vérifié",u"Nicht geprüft",u"Non verificato");if(!s.available)return T(u"Sin conexión",u"Unavailable",u"Indisponible",u"Nicht verfügbar",u"Non disponibile");const auto& v=s.newer?s.latest:s.installed;return std::u16string(v.begin(),v.end());}
std::u16string Summary(){const auto s=Get();using PcMenuLocale::T;if(!s.checked)return T(u"Buscando salas...",u"Checking rooms...",u"Recherche de salles...",u"Räume werden geprüft...",u"Ricerca stanze...");if(!s.available)return T(u"Salas sin conexión",u"Rooms offline",u"Salles hors ligne",u"Räume offline",u"Stanze offline");const auto n=std::to_string(s.rooms.size());return T(u"Salas",u"Rooms",u"Salles",u"Räume",u"Stanze")+u": "+std::u16string(n.begin(),n.end());}
}
