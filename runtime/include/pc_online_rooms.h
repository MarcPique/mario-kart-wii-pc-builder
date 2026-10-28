#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace PcOnlineRooms {
struct Room {std::string id,type;unsigned players=0;};
struct Snapshot {bool available=false,checked=false;uint64_t revision=0;unsigned players=0;std::vector<Room> rooms;};
void Start();
void Stop();
void Refresh();
Snapshot Get();
std::u16string Summary();
}
