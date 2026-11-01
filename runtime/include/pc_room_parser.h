#pragma once
#include "pc_online_rooms.h"
#include "../third_party/pc_json.hpp"
#include <charconv>
#include <algorithm>
namespace PcOnlineRooms {
inline Snapshot ParseRooms(const std::string& data) {
    const auto json=nlohmann::json::parse(data);Snapshot next;
    if(!json.is_object()||!json.contains("rooms")||!json["rooms"].is_array()||json["rooms"].size()>512)throw std::runtime_error("Invalid room list");
    for(const auto& item:json["rooms"]) {
        if(!item.is_object()||!item.contains("id")||!item["id"].is_string()||!item.contains("players")||!item["players"].is_array())throw std::runtime_error("Invalid room");
        auto id=item["id"].get<std::string>(),type=item.value("type",std::string{});
        // A status entry can contain multiple temporarily merged 12-player rooms.
        if(id.size()>64||type.size()>32||item["players"].size()>192)throw std::runtime_error("Invalid room limits");
        Room room{id,type,static_cast<unsigned>(item["players"].size())};
        if(item.contains("roomType") && item["roomType"].is_string()) {
            room.mode=item["roomType"].get<std::string>();
            if(room.mode.size()>80)throw std::runtime_error("Invalid room mode");
        }
        // roomType is a human-readable label. Matchmaking uses the raw rk
        // value instead; labels may change or be shared by other mod packs.
        if(item.contains("rk") && item["rk"].is_string()) {
            room.roomKey=item["rk"].get<std::string>();
            if(room.roomKey.size()>32)throw std::runtime_error("Invalid room key");
        }
        // The public status endpoint identifies the host by slot, not by PID.
        // Keep the PID in memory only, and only for publicly joinable rooms.
        if(type=="anybody" && item.value("isPublic",false) && item.value("isJoinable",false) &&
           !item.value("isSuspended",false) && room.players>0 && room.players<12 &&
           item.contains("host") && item["host"].is_string()) {
            const auto host=item["host"].get<std::string>();
            for(const auto& player:item["players"]) {
                if(!player.is_object() || !player.contains("slotId") || !player["slotId"].is_string() ||
                   !player.contains("pid") || !player["pid"].is_string()) continue;
                const auto pid=player["pid"].get<std::string>();
                uint32_t parsed=0;const auto result=std::from_chars(pid.data(),pid.data()+pid.size(),parsed);
                if(!pid.empty() && result.ec==std::errc{} && result.ptr==pid.data()+pid.size() && parsed!=0) {
                    if(std::find(room.memberPids.begin(),room.memberPids.end(),parsed)==room.memberPids.end())room.memberPids.push_back(parsed);
                    if(player["slotId"].get<std::string>()==host){room.hostPid=parsed;room.publicJoinable=true;}
                }
            }
            if(!room.publicJoinable)room.memberPids.clear();
        }
        next.rooms.push_back(std::move(room));next.players+=next.rooms.back().players;
    }
    next.available=true;next.checked=true;return next;
}
}
