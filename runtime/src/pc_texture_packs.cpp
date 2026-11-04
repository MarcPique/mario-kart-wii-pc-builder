#include "pc_texture_packs.h"
#include "runtime_config.h"
#include "runtime_log.h"
#include "gx_worker.h"
#include <aurora/texture.hpp>
#include <vector>
#include <filesystem>
#ifdef _WIN32
#include <shellapi.h>
#endif
namespace PcTexturePacks {namespace {
std::vector<aurora::texture::ReplacementGroup> groups;
size_t count=0;bool enabled=false;
std::filesystem::path UserRoot(){return RuntimeConfigFile::ApplicationDataDirectory();}
std::filesystem::path TextureRoot(){return UserRoot()/"Load/Textures";}
void Clear(){for(const auto& group:groups)aurora::texture::unregister_replacements(group);groups.clear();count=0;}
}
void Reload(){
    GX_SYNC_INLINE();Clear();
    if(!enabled)return;
    const auto root=TextureRoot();std::error_code ec;std::filesystem::create_directories(root/"RMCP01",ec);
    // Match Dolphin's region-specific-first convention; the region-free folder
    // remains useful for packs shared between PAL/NTSC versions.
    const auto pal=root/"RMCP01",common=root/"RMC";
    int priority=0;
    for(const auto& path:{UserRoot()/"texture_replacements",common,pal}){
        if(!std::filesystem::is_directory(path,ec))continue;
        try{auto group=aurora::texture::load_replacement_directory(path,{.priority=priority++});count+=group.registrations.size();groups.push_back(std::move(group));}
        catch(const std::exception& error){RT_LOGF("pc-textures","Pack scan failed: %s; originals retained\n",error.what());}
    }
    RT_LOGF("pc-textures","Loaded %zu Dolphin PNG/DDS texture registrations; unmatched/invalid files retain original textures\n",count);
}
void Initialize(){enabled=RuntimeConfigFile::TextureReplacements(true);Reload();}
void SetEnabled(bool value){enabled=value;RuntimeConfigFile::Mutable().textureReplacements=value;RuntimeConfigFile::WriteSetting("video","texture_replacements",value?"true":"false");Reload();}
bool Enabled(){return enabled;}
size_t Count(){return count;}
void OpenFolder(){std::error_code ec;const auto path=TextureRoot()/"RMCP01";std::filesystem::create_directories(path,ec);
#ifdef _WIN32
    ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
#endif
}
}
