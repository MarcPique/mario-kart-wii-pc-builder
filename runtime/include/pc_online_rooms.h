#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace PcOnlineRooms {
struct Room {std::string id,type;unsigned players=0;std::string mode,roomKey;uint32_t hostPid=0;bool publicJoinable=false;std::vector<uint32_t> memberPids;};
struct Snapshot {bool available=false,checked=false;uint64_t revision=0;unsigned players=0;std::vector<Room> rooms;};
struct UpdateSnapshot {bool available=false,checked=false,newer=false;std::string installed,latest;};
void Start();
void Stop();
void Refresh();
Snapshot Get();
std::u16string Summary();
void RefreshUpdates();
UpdateSnapshot GetUpdates();
std::u16string UpdateSummary();
}
