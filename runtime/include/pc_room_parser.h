#pragma once
#include "pc_online_rooms.h"
#include "../third_party/pc_json.hpp"
namespace PcOnlineRooms {
inline Snapshot ParseRooms(const std::string& data) {
    const auto json=nlohmann::json::parse(data);Snapshot next;
    if(!json.is_object()||!json.contains("rooms")||!json["rooms"].is_array()||json["rooms"].size()>512)throw std::runtime_error("Invalid room list");
    for(const auto& item:json["rooms"]) {
        if(!item.is_object()||!item.contains("id")||!item["id"].is_string()||!item.contains("players")||!item["players"].is_array())throw std::runtime_error("Invalid room");
        auto id=item["id"].get<std::string>(),type=item.value("type",std::string{});
        // A status entry can contain multiple temporarily merged 12-player rooms.
        if(id.size()>64||type.size()>32||item["players"].size()>192)throw std::runtime_error("Invalid room limits");
        next.rooms.push_back({id,type,static_cast<unsigned>(item["players"].size())});next.players+=next.rooms.back().players;
    }
    next.available=true;next.checked=true;return next;
}
}
