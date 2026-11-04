#pragma once
#include "hle/riivolution_contract.h"
#include <array>
#include <optional>
#include <string_view>

namespace PcMyStuff {
struct Plan {RiivolutionContract::Patch patch;uint8_t runtimeFlag=0;};
// Select only the file-replacement patch. Pack defaults and save redirects
// must never be pulled into a live My Stuff change.
inline std::optional<Plan> Prepare(std::string_view xml,const std::string& gameId,unsigned mode){
    if(mode>4)return std::nullopt;
    const auto disc=RiivolutionContract::ParseString(xml);
    if(!disc||!disc->IsValidForGame(gameId,std::nullopt,std::nullopt))return std::nullopt;
    if(mode==0)return Plan{};
    constexpr std::array<std::string_view,4> ids{"RRCTGPLoad","RRLoad","RRCTGPLoadMusic","RRLoadMusic"};
    const auto found=std::find_if(disc->patches.begin(),disc->patches.end(),[&](const auto& patch){return patch.id==ids[mode-1];});
    if(found==disc->patches.end()||!found->savegamePatches.empty())return std::nullopt;
    return Plan{*found,static_cast<uint8_t>(mode>=3?2:1)};
}
}
