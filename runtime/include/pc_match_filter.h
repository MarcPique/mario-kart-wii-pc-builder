#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace PcOnlineRooms {
// Only narrow a normal Retro Rewind search. The server still enforces all
// matching and reservation rules, including rating and slot availability.
struct MatchFilterBatch {std::string expression;size_t nextOffset;};
struct RoomModeRoute {uint32_t mainOffset,mainId,modeId;};
inline bool IsConnectedToSelectedRoom(bool connected,uint32_t server,std::span<const uint32_t> knownPeers){
    if(!connected||!server)return false;
    for(const auto peer:knownPeers)if(peer==server)return true;
    return false;
}
inline std::optional<RoomModeRoute> RouteForRoomMode(std::string_view key){
    if(key=="vs_10")return RoomModeRoute{0x1200,6,1};
    if(key=="vs_20")return RoomModeRoute{0x1200,6,4};
    if(key=="vs_21")return RoomModeRoute{0x1200,6,5};
    if(key=="vs_11")return RoomModeRoute{0x1454,7,6};
    if(key=="vs_12")return RoomModeRoute{0x1454,7,7};
    if(key=="vs_13")return RoomModeRoute{0x1454,7,9};
    if(key=="vs_14")return RoomModeRoute{0x16A8,8,10};
    if(key=="vs_15")return RoomModeRoute{0x16A8,8,11};
    return std::nullopt;
}
inline std::optional<std::string> BlockUnrelatedMatches(std::string_view filter){
    if(filter.find("dwc_mver = ")==std::string_view::npos || filter.find('\0')!=std::string_view::npos)return std::nullopt;
    const auto start=filter.find("dwc_pid != ");uint32_t self=0;
    if(start==std::string_view::npos)return std::nullopt;
    const auto begin=filter.data()+start+11;const auto parsed=std::from_chars(begin,filter.data()+filter.size(),self);
    if(parsed.ec!=std::errc{}||!self)return std::nullopt;
    auto blocked=std::string("(")+std::string(filter)+") and dwc_pid = "+std::to_string(self);
    if(blocked.size()>=256)return std::nullopt;
    return blocked;
}
inline bool IsRetroRoomMode(std::string_view roomKey){
    constexpr std::array<std::string_view, 9> modes{
        "vs_10", "vs_11", "vs_12", "vs_13", "vs_14", "vs_15", "vs_20", "vs_21", "vs_22"};
    for(auto mode:modes)if(roomKey==mode)return true;
    return false;
}
inline std::optional<MatchFilterBatch> NarrowMatchFilterBatch(std::string_view filter,
                                                             std::string_view roomKey,
                                                             std::span<const uint32_t> memberPids,
                                                             size_t offset) {
    if(!IsRetroRoomMode(roomKey) || memberPids.empty() || filter.find('\0')!=std::string_view::npos)return std::nullopt;
    for(auto required:{"dwc_mver = ","dwc_pid != ","maxplayers = ","numplayers < ",
                       "dwc_mtype = ","dwc_hoststate = ","dwc_suspend = "})
        if(filter.find(required)==std::string_view::npos)return std::nullopt;
    const auto roomConstraint=std::string("rk = '")+std::string(roomKey)+"'";
    if(filter.find(roomConstraint)==std::string_view::npos)return std::nullopt;
    auto narrowed=std::string("(")+std::string(filter)+") and (";
    size_t scanned=0,added=0;
    for(;scanned<memberPids.size();++scanned){
        const auto pid=memberPids[(offset+scanned)%memberPids.size()];if(!pid)continue;
        const auto term=(added?std::string(" or "):std::string{})+"dwc_pid = "+std::to_string(pid);
        // Reserve one byte for ')' and respect the SDK's 256-byte limit.
        if(narrowed.size()+term.size()+1>=256)break;
        narrowed+=term;++added;
    }
    if(!added)return std::nullopt;
    narrowed+=')';return MatchFilterBatch{std::move(narrowed),(offset+scanned)%memberPids.size()};
}
inline std::optional<std::string> NarrowMatchFilter(std::string_view filter,std::string_view roomKey,uint32_t hostPid){
    const std::array<uint32_t,1> one{hostPid};
    const auto batch=NarrowMatchFilterBatch(filter,roomKey,one,0);
    if(!batch)return std::nullopt;
    return batch->expression;
}
}
