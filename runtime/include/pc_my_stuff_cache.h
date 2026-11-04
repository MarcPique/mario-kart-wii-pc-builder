#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace PcMyStuffCache {
namespace fs=std::filesystem;
using Digest=std::array<uint8_t,32>;
struct File {fs::path path;uint64_t size=0;};
inline std::string Hex(const Digest& digest){
    constexpr char digits[]="0123456789abcdef";std::string result(64,'0');
    for(size_t i=0;i<digest.size();++i){result[2*i]=digits[digest[i]>>4];result[2*i+1]=digits[digest[i]&15];}
    return result;
}
class Temporary {
public:
    explicit Temporary(const fs::path& root){
        static std::atomic<uint64_t> serial{0};
        for(unsigned attempt=0;attempt<64;++attempt){
            directory=root/(".pending-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(serial.fetch_add(1)));
            std::error_code ec;if(fs::create_directory(directory,ec))return;
            if(ec)throw fs::filesystem_error("My Stuff cache temporary directory",directory,ec);
        }
        throw std::runtime_error("My Stuff cache temporary name unavailable");
    }
    ~Temporary(){std::error_code ec;fs::remove(directory/"payload",ec);fs::remove(directory,ec);}
    Temporary(const Temporary&)=delete;Temporary& operator=(const Temporary&)=delete;
    fs::path directory;
};

// DigestFile must return SHA-256 in production. A completed version is never
// rewritten: old DVD extents may still refer to it after a mode/file change.
template<class DigestFile,class Stop>
File Snapshot(const fs::path& source,const fs::path& cache,DigestFile&& digestFile,Stop&& stop,
              uint64_t limit=std::numeric_limits<uint32_t>::max()){
    const auto cancelled=[&]{if(stop())throw std::runtime_error("My Stuff preparation cancelled");};
    cancelled();
    const auto size=fs::file_size(source);const auto stamp=fs::last_write_time(source);
    if(size>limit)throw std::runtime_error("My Stuff file exceeds DVD size limit");
    fs::create_directories(cache);Temporary temporary(cache);const auto payload=temporary.directory/"payload";
    std::ifstream input(source,std::ios::binary);std::ofstream output(payload,std::ios::binary|std::ios::trunc);
    if(!input||!output)throw std::runtime_error("Cannot open My Stuff snapshot");
    std::vector<char> buffer(1024*1024);uint64_t copied=0;
    while(input){
        cancelled();input.read(buffer.data(),buffer.size());const auto count=input.gcount();
        if(count>0){copied+=static_cast<uint64_t>(count);if(copied>limit)throw std::runtime_error("My Stuff file grew beyond DVD size limit");output.write(buffer.data(),count);}
    }
    if(!input.eof()||!output)throw std::runtime_error("My Stuff snapshot copy failed");
    input.close();output.close();if(!output)throw std::runtime_error("My Stuff snapshot flush failed");
    const auto digest=digestFile(payload);cancelled();
    if(copied!=size||fs::file_size(source)!=size||fs::last_write_time(source)!=stamp)
        throw std::runtime_error("My Stuff source changed while being prepared");
    const auto target=cache/(Hex(digest)+".asset");
    const auto existing=[&]{
        if(!fs::exists(target)&&!fs::is_symlink(target))return false;
        if(fs::is_symlink(target)||!fs::is_regular_file(target)||fs::file_size(target)!=copied||digestFile(target)!=digest)
            throw std::runtime_error("My Stuff cached version is damaged");
        return true;
    };
    if(existing())return {target,copied};
    std::error_code ec;fs::rename(payload,target,ec);
    // Another process with the same profile may finish this version first.
    if(ec&&!existing())throw fs::filesystem_error("My Stuff snapshot publication",payload,target,ec);
    return {target,copied};
}
}
