#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace PcOnlineRooms {
// Keep slots stable while a list is visible. Rooms which disappear keep their
// slot until the list is reopened, so a refresh cannot retarget a user's click.
inline std::vector<std::string> StableRoomOrder(const std::vector<std::string>& previous,
                                                const std::vector<std::string>& current) {
    constexpr size_t limit=512;
    std::vector<std::string> result;
    result.reserve(std::min(limit,previous.size()+current.size()));
    const auto append=[&](const std::string& id){
        if(!id.empty() && result.size()<limit && std::find(result.begin(),result.end(),id)==result.end())
            result.push_back(id);
    };
    for(const auto& id:previous)append(id);
    for(const auto& id:current)append(id);
    return result;
}
}
