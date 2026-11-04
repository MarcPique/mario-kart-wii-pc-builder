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
#include <cstdint>
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
static void NoReparse(const fs::path& path,const fs::path& root){
    auto current=path;
    for(;;){
        const auto attributes=GetFileAttributesW(current.c_str());
        Ensure(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)==0,"Enlace inesperado en la instalacion");
        if(current==root)break;
        Ensure(current.has_parent_path()&&current.parent_path()!=current,"Ruta fuera de la instalacion");current=current.parent_path();
    }
}
static std::wstring Quote(const std::wstring& text){
    std::wstring result=L"\"";unsigned slashes=0;
    for(auto c:text){if(c==L'\\'){++slashes;continue;}if(c==L'\"'){result.append(slashes*2+1,L'\\');result+=c;}else{result.append(slashes,L'\\');result+=c;}slashes=0;}
    result.append(slashes*2,L'\\');return result+L'\"';
}
static bool StartPreparedUpdate(const fs::path& root,HANDLE gate){
    const auto candidate=root/L".mkwii-update",journal=root/L"mkwii-pc-update-transaction.json";
    NoReparse(candidate,root);NoReparse(journal,root);
    if(!fs::exists(candidate/L"ready.json")&&!fs::exists(journal))return false;
    NoReparse(candidate/L"ready.json",root);
    auto tool=root/L"GameData"/L"Install"/L"RetroRewind"/L"Tools"/L"MKWii-PC-Builder.exe";
    NoReparse(tool,root);
    if(!fs::is_regular_file(tool)){tool=root/L"GameData.previous"/L"Install"/L"RetroRewind"/L"Tools"/L"MKWii-PC-Builder.exe";NoReparse(tool,root);}
    Ensure(fs::is_regular_file(tool),"Falta el actualizador. Abre el builder para recuperar la instalacion");
    const auto work=root/L".mkwii-builder",helpers=work/L"Updater",temporary=work/L"update-temp";
    NoReparse(helpers,root);NoReparse(temporary,root);fs::create_directories(helpers);fs::create_directories(temporary);
    const auto helper=helpers/(L"apply-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".exe");
    fs::copy_file(tool,helper);
    HANDLE inherited=nullptr;
    Ensure(DuplicateHandle(GetCurrentProcess(),gate,GetCurrentProcess(),&inherited,0,TRUE,DUPLICATE_SAME_ACCESS),"No se pudo transferir el bloqueo de instalacion");
    Handle held(inherited);
    auto command=Quote(helper.native())+L" --apply-staged-update --install-dir "+Quote(root.native())+L" --wait-pid "+std::to_wstring(GetCurrentProcessId())+L" --install-lock-handle "+std::to_wstring(reinterpret_cast<uintptr_t>(inherited))+L" --launch-after-update";
    Ensure(SetEnvironmentVariableW(L"TEMP",temporary.c_str())&&SetEnvironmentVariableW(L"TMP",temporary.c_str()),"No se pudo preparar la carpeta temporal local");
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
    Ensure(CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&process),"No se pudo iniciar la actualizacion preparada");
    CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    bool quiet=false;
    try{
        SetCurrentProcessExplicitAppUserModelID(L"MarioKartWii.Native");
        std::wstring self(32768,L'\0');auto length=GetModuleFileNameW(nullptr,self.data(),self.size());Ensure(length&&length<self.size(),"No se pudo localizar el juego");self.resize(length);
        const auto root=InstallationRoot(fs::path(self));const auto data=root/L"GameData";
        const auto lockPath=root/L".mkwii-launch.lock";NoReparse(lockPath,root);
        Handle gate(CreateFileW(lockPath.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
        Ensure(gate.v!=INVALID_HANDLE_VALUE,"Mario Kart Wii o su actualizador ya estan usando esta carpeta");
        const auto mutexName=L"Local\\MarioKartWii_"+std::to_wstring(std::hash<std::wstring>{}(root.native()));
        Handle mutex(CreateMutexW(nullptr,TRUE,mutexName.c_str()));Ensure(mutex.v&&GetLastError()!=ERROR_ALREADY_EXISTS,"Mario Kart Wii ya esta abierto en esta carpeta");
        if(StartPreparedUpdate(root,gate.v))return 0;
        Ensure(fs::is_regular_file(data/L"portable-data.txt")&&fs::is_regular_file(data/L"Install"/L"RetroRewind"/L"RetroRewind.exe"),"Conserva GameData junto a Mario Kart Wii.exe");
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
