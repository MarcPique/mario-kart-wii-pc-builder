#include "pc_my_stuff_cache.h"
#include <iostream>
#include <iterator>
namespace fs=std::filesystem;
static std::string Read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
static void Write(const fs::path& path,const std::string& value){std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<value;}
// Deliberately small deterministic test digest. Production must supply SHA-256;
// these tests exercise snapshot lifetime/publication, not cryptography.
static PcMyStuffCache::Digest TestDigest(const fs::path& path){
    const auto text=Read(path);PcMyStuffCache::Digest d{};
    for(size_t i=0;i<text.size();++i)d[i%32]=static_cast<uint8_t>(d[i%32]*33+static_cast<unsigned char>(text[i]));return d;
}
int main(){
    const auto parent=fs::canonical(fs::temp_directory_path());
    const auto root=parent/("mkw-my-stuff-cache-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!fs::create_directory(root))return 1;Write(root/".owned","test");
    int result=0;
    try{
        const auto source=root/"source.bin",cache=root/"cache";Write(source,"original music");
        const auto first=PcMyStuffCache::Snapshot(source,cache,TestDigest,[]{return false;});
        if(first.path==source||Read(first.path)!="original music")throw 2;
        const auto stamp=fs::last_write_time(first.path);
        const auto same=PcMyStuffCache::Snapshot(source,cache,TestDigest,[]{return false;});
        if(same.path!=first.path||fs::last_write_time(first.path)!=stamp)throw 3;
        Write(source,"replacement music");
        const auto next=PcMyStuffCache::Snapshot(source,cache,TestDigest,[]{return false;});
        if(next.path==first.path||Read(first.path)!="original music"||Read(next.path)!="replacement music")throw 4;
        fs::remove(source);if(Read(first.path)!="original music"||Read(next.path)!="replacement music")throw 5;
        Write(source,"source changes during hashing");bool rejected=false;
        try{PcMyStuffCache::Snapshot(source,root/"changing",[&](const auto& p){auto digest=TestDigest(p);Write(source,"changed");return digest;},[]{return false;});}catch(const std::runtime_error&){rejected=true;}
        if(!rejected||!fs::is_empty(root/"changing"))throw 6;
        Write(source,"too large");rejected=false;
        try{PcMyStuffCache::Snapshot(source,root/"size",TestDigest,[]{return false;},2);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw 7;
        rejected=false;try{PcMyStuffCache::Snapshot(source,root/"cancel",TestDigest,[]{return true;});}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw 8;
        Write(source,"replacement music");Write(next.path,"corrupt cache");rejected=false;
        try{PcMyStuffCache::Snapshot(source,cache,TestDigest,[]{return false;});}catch(const std::runtime_error&){rejected=true;}
        if(!rejected||Read(next.path)!="corrupt cache"||Read(first.path)!="original music")throw 9;
        for(const auto& entry:fs::directory_iterator(cache))if(entry.path().filename().string().starts_with(".pending-"))throw 10;
        std::cout<<"My Stuff cache: immutable versions, deduplication, changed source, size, cancellation and damaged cache passed\n";
    }catch(int code){result=code;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=11;}
    // Delete only this exact, marked, uniquely created temporary sandbox.
    if(root.parent_path()==parent&&root.filename().string().starts_with("mkw-my-stuff-cache-test-")&&Read(root/".owned")=="test")fs::remove_all(root);
    else return 12;
    return result;
}
