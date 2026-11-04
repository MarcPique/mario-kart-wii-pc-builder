#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <functional>
namespace fs=std::filesystem;
struct Handle {HANDLE v=nullptr;explicit Handle(HANDLE h):v(h){}~Handle(){if(v&&v!=INVALID_HANDLE_VALUE)CloseHandle(v);}Handle(const Handle&)=delete;};
static void Ensure(bool ok,const char* message){if(!ok)throw std::runtime_error(std::string(message)+" (Windows "+std::to_string(GetLastError())+")");}
static fs::path InstallationRoot(const fs::path& self){
    auto root=self.parent_path();if(fs::exists(root/L"GameData"/L"portable-data.txt"))return root;
    // A desktop copy contains only this launcher and a local installation link.
    std::ifstream file(self,std::ios::binary);file.seekg(0,std::ios::end);const auto size=file.tellg();
    if(size<16)return root;file.seekg(-16,std::ios::end);char magic[8];uint64_t length=0;file.read(magic,8);file.read(reinterpret_cast<char*>(&length),8);
    if(std::string(magic,8)!="MKWLINK1"||length==0||length>32768||static_cast<uint64_t>(size)<length+16)return root;
    file.seekg(-static_cast<std::streamoff>(length+16),std::ios::end);std::string path(length,'\0');file.read(path.data(),path.size());
    const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.data(),path.size(),nullptr,0);if(count<=0)return root;
    std::wstring wide(count,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.data(),path.size(),wide.data(),count);return fs::path(wide);
}
static void SetPath(std::string& text,const std::string& key,const std::string& value){
    const std::string setting=key+" = \""+value+"\"";
    auto pos=text.find(key+" =");
    if(pos!=std::string::npos){auto end=text.find('\n',pos);text.replace(pos,end==std::string::npos?end:end-pos,setting);return;}
    pos=text.find("[paths]");
    if(pos==std::string::npos)text+="\n[paths]\n"+setting+"\n";else text.insert(pos+7,"\n"+setting);
}
static void Configure(const fs::path& root){
    fs::create_directories(root/L"UserData"/L"Logs");
    if(!fs::exists(root/L"portable.txt"))std::ofstream(root/L"portable.txt")<<"Mario Kart Wii portable\n";
    const auto config=root/L"UserData"/L"Config.toml";
    std::ifstream source(config,std::ios::binary);std::string text((std::istreambuf_iterator<char>(source)),{});source.close();
    SetPath(text,"dvd_root","../GameData/Install/GameAssets/DATA");
    SetPath(text,"retro_rewind_root","../GameData/RetroRewind6");
    const auto temp=root/L"UserData"/L"Config.toml.launcher-tmp";
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<text;Ensure(out.good(),"No se pudo guardar la configuracion");}
    Ensure(MoveFileExW(temp.c_str(),config.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"No se pudo actualizar la configuracion");
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    bool quiet=false;
    try{
        SetCurrentProcessExplicitAppUserModelID(L"MarioKartWii.Native");
        std::wstring self(32768,L'\0');auto length=GetModuleFileNameW(nullptr,self.data(),self.size());Ensure(length&&length<self.size(),"No se pudo localizar el juego");self.resize(length);
        const auto root=InstallationRoot(fs::path(self));const auto data=root/L"GameData";
        Ensure(fs::is_regular_file(data/L"portable-data.txt")&&fs::is_regular_file(data/L"Install"/L"RetroRewind"/L"RetroRewind.exe"),"Conserva GameData junto a Mario Kart Wii.exe");
        const auto mutexName=L"Local\\MarioKartWii_"+std::to_wstring(std::hash<std::wstring>{}(root.native()));
        Handle mutex(CreateMutexW(nullptr,TRUE,mutexName.c_str()));Ensure(mutex.v&&GetLastError()!=ERROR_ALREADY_EXISTS,"Mario Kart Wii ya esta abierto en esta carpeta");
        Configure(root);
        std::ofstream log(root/L"UserData"/L"Logs"/L"PC-launcher.log",std::ios::trunc);
        const auto selection=root/L"UserData"/L"pc_mode.txt",request=root/L"UserData"/L"pc_restart.txt";
        std::string mode="retro";
        int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);std::wstring forwarded;
        for(int i=1;i<argc;++i){std::wstring arg=argv[i];Ensure(arg==L"--headless"||arg==L"--race"||arg==L"--race-cpu"||arg.rfind(L"--race=",0)==0,"Opcion de inicio desconocida");forwarded+=L" "+arg;if(arg==L"--headless")quiet=true;}
        LocalFree(argv);
        // Retro Rewind is integrated at boot. Its own menus handle online without a process switch.
        for(;;){
            if(fs::exists(request))fs::remove(request);
            {std::ofstream chosen(selection,std::ios::trunc);chosen<<mode<<'\n';}
            const auto game=data/L"Install"/L"RetroRewind"/L"RetroRewind.exe";
            std::wstring command=L"\""+game.native()+L"\""+forwarded;
            STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};const auto working=game.parent_path();
            Ensure(CreateProcessW(game.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,working.c_str(),&startup,&process),"No se pudo iniciar el juego");
            Handle child(process.hProcess),thread(process.hThread);
            log<<"Started "<<mode<<" PID "<<process.dwProcessId<<"\n"<<std::flush;
            WaitForSingleObject(child.v,INFINITE);DWORD exitCode=1;GetExitCodeProcess(child.v,&exitCode);
            log<<"Exited "<<mode<<" code "<<exitCode<<"\n"<<std::flush;
            if(fs::exists(request)){log<<"Graphics restart requested in-game\n"<<std::flush;Configure(root);continue;}
            return static_cast<int>(exitCode);
        }
    }catch(const std::exception& error){if(!quiet)MessageBoxA(nullptr,error.what(),"Mario Kart Wii",MB_OK|MB_ICONERROR);return 1;}
}
