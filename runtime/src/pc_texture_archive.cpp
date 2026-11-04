#include "pc_texture_archive.h"
#include "runtime_log.h"
#include <dolphin/gx.h>
#include <aurora/texture.hpp>
#include <cryptopp/sha.h>
#include <fstream>
#include <array>
#include <chrono>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#endif
namespace PcTextureArchive {namespace {
std::string Hash(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Unable to read archive/tool");
    CryptoPP::SHA256 hash;std::array<char,65536> buffer;
    while(file){file.read(buffer.data(),buffer.size());hash.Update(reinterpret_cast<const CryptoPP::byte*>(buffer.data()),file.gcount());}
    if(!file.eof())throw std::runtime_error("Archive read failed");
    std::array<CryptoPP::byte,32> digest;hash.Final(digest.data());std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto byte:digest)out<<std::setw(2)<<unsigned(byte);return out.str();
}
#ifdef _WIN32
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}Handle()=default;Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;};
std::wstring Quote(const std::wstring& value){std::wstring out=L"\"";unsigned slashes=0;for(auto c:value){if(c==L'\\'){++slashes;continue;}if(c==L'\"'){out.append(slashes*2+1,L'\\');out+=c;}else{out.append(slashes,L'\\');out+=c;}slashes=0;}out.append(slashes*2,L'\\');return out+L'\"';}
std::string Run(const std::filesystem::path& tool,const std::vector<std::wstring>& args,unsigned timeout){
    std::wstring command=Quote(tool.wstring());for(const auto& arg:args)command+=L" "+Quote(arg);
    SECURITY_ATTRIBUTES attr{sizeof(attr),nullptr,TRUE};Handle read,write,job,process,thread,input;
    if(!CreatePipe(&read.value,&write.value,&attr,0)||!SetHandleInformation(read.value,HANDLE_FLAG_INHERIT,0))throw std::runtime_error("Archive pipe failed");
    input.value=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&attr,OPEN_EXISTING,0,nullptr);
    job.value=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job.value||!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))throw std::runtime_error("Archive job failed");
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;startup.hStdOutput=write.value;startup.hStdError=write.value;startup.hStdInput=input.value;
    PROCESS_INFORMATION info{};if(!CreateProcessW(tool.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,tool.parent_path().c_str(),&startup,&info))throw std::runtime_error("Unable to start archive tool");
    process.value=info.hProcess;thread.value=info.hThread;
    if(!AssignProcessToJobObject(job.value,process.value)){TerminateProcess(process.value,1);throw std::runtime_error("Unable to contain archive process");}
    if(ResumeThread(thread.value)==DWORD(-1))throw std::runtime_error("Unable to resume archive tool");
    CloseHandle(write.value);write.value=nullptr;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(timeout);std::string output;
    for(;;){DWORD available=0;if(PeekNamedPipe(read.value,nullptr,0,nullptr,&available,nullptr)&&available){std::array<char,16384> buffer;DWORD got=0;if(!ReadFile(read.value,buffer.data(),std::min<DWORD>(available,buffer.size()),&got,nullptr))throw std::runtime_error("Archive output failed");output.append(buffer.data(),got);if(output.size()>64*1024*1024)throw std::runtime_error("Archive listing too large");continue;}
        if(WaitForSingleObject(process.value,20)==WAIT_OBJECT_0){DWORD remaining=0;if(PeekNamedPipe(read.value,nullptr,0,nullptr,&remaining,nullptr)&&remaining)continue;break;}
        if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Archive import timed out");
    }
    DWORD code=1;GetExitCodeProcess(process.value,&code);if(code!=0)throw std::runtime_error("Archive is damaged, encrypted or unsupported");return output;
}
#endif
bool ValidTextureName(const std::filesystem::path& file){
    auto name=file.filename().string();if(!TextureFile(name))return false;
    const auto dot=name.rfind('.'),mip=name.rfind("_mip");
    if(mip!=std::string::npos&&mip<dot){const auto n=name.substr(mip+4,dot-mip-4);if(n.empty()||n.find_first_not_of("0123456789")!=std::string::npos)return false;name.erase(mip,dot-mip);}
    return aurora::texture::parse_replacement_filename(name).has_value();
}
}
Result Import(const std::vector<std::filesystem::path>& roots,const std::filesystem::path& cache,const std::filesystem::path& tool){
    Result result;std::vector<std::filesystem::path> archives;std::error_code ec;
    for(const auto& root:roots){if(!std::filesystem::is_directory(root,ec))continue;for(std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
        if(ec)break;if(it->is_symlink(ec)){if(it->is_directory(ec))it.disable_recursion_pending();continue;}if(!it->is_regular_file(ec))continue;
        const auto ext=Lower(it->path().extension().string());if(ext==".zip"||ext==".7z")archives.push_back(it->path());if(archives.size()>256)break;
    }}
    std::sort(archives.begin(),archives.end());if(archives.empty())return result;
    try{if(Hash(tool)!="7ddf0a3bc451e166679296be630d6f0573eb386159c04184761c43020c484291")throw std::runtime_error("Archive tool missing or modified");}
    catch(const std::exception& e){result.failed=archives.size();RT_LOGF("pc-archives","%s; originals retained\n",e.what());return result;}
    std::filesystem::create_directories(cache);const auto canonicalCache=std::filesystem::weakly_canonical(cache);
    for(const auto& archive:archives)try{
#ifdef _WIN32
        Handle archiveLock;archiveLock.value=CreateFileW(archive.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(archiveLock.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Archive is busy; reload after copying finishes");
#endif
        if(std::filesystem::file_size(archive)>16ull*1024*1024*1024)throw std::runtime_error("Compressed pack exceeds 16 GiB");
        const auto digest=Hash(archive);const auto directory=cache/digest;
        if(std::filesystem::weakly_canonical(directory).parent_path()!=canonicalCache)throw std::runtime_error("Unsafe texture cache directory");
        if(std::filesystem::is_regular_file(directory/"complete.txt",ec)){std::ifstream marker(directory/"complete.txt");std::string value;std::getline(marker,value);if(value=="MKW-TEX-1 "+digest){result.directories.push_back(directory);continue;}}
#ifdef _WIN32
        const auto type=Lower(archive.extension().string())==".zip"?L"-tzip":L"-t7z";
        auto listing=ValidateListing(Run(tool,{L"l",L"-slt",L"-ba",L"-sccUTF-8",L"-p-",type,L"--",archive.wstring()},120));
        if(!listing.textures)throw std::runtime_error("No Dolphin textures found in archive");
        const auto unique=std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto staging=cache/(digest+".partial-"+unique);
        if(staging.parent_path()!=cache||std::filesystem::weakly_canonical(staging).parent_path()!=canonicalCache||std::filesystem::exists(staging))throw std::runtime_error("Unsafe texture cache target");
        std::filesystem::create_directory(staging);
        Run(tool,{L"x",L"-y",L"-aoa",L"-ssc-",L"-bso0",L"-bsp0",L"-bse1",L"-p-",L"-mmt=2",type,L"-o"+staging.wstring(),L"-ir!*.png",L"-ir!*.dds",L"--",archive.wstring()},1800);
        uint64_t bytes=0;unsigned textures=0;
        for(const auto& entry:std::filesystem::recursive_directory_iterator(staging)){
            const auto attributes=GetFileAttributesW(entry.path().c_str());
            if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Archive produced a linked entry");
            if(!entry.is_regular_file())continue;
            const auto relative=entry.path().lexically_relative(staging);SafePath(relative.generic_string());
            const auto size=entry.file_size();if(size>MaxFile||size>MaxTotal-bytes)throw std::runtime_error("Extracted texture limit exceeded");bytes+=size;
            if(ValidTextureName(entry.path()))++textures;
        }
        if(!textures)throw std::runtime_error("No usable textures in archive");
        std::ofstream marker(staging/"complete.txt");marker<<"MKW-TEX-1 "<<digest<<"\n";marker.close();if(!marker)throw std::runtime_error("Unable to finish texture cache");
        if(std::filesystem::exists(directory))throw std::runtime_error("Texture cache incomplete; original textures retained");
        std::filesystem::rename(staging,directory);result.directories.push_back(directory);
        RT_LOGF("pc-archives","Imported %u texture files (%llu bytes) from %s\n",textures,static_cast<unsigned long long>(bytes),archive.filename().string().c_str());
#else
        throw std::runtime_error("Archive import requires Windows");
#endif
    }catch(const std::exception& e){++result.failed;RT_LOGF("pc-archives","Import failed for %s: %s; originals retained\n",archive.filename().string().c_str(),e.what());}
    return result;
}
}
