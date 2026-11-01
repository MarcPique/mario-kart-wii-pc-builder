#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Retro Rewind 6.13.4's PULP v7 settings table, applied only to a freshly
// created settings file. Existing player choices must never be overwritten.
namespace PcRetroDefaults {
inline bool EnableAllTracks(uint8_t* data, size_t length) {
    constexpr size_t offset=0x4c;
    if (!data || length<=offset || std::memcmp(data,"PULP",4)!=0 ||
        data[4]!=0 || data[5]!=0 || data[6]!=0 || data[7]!=7 ||
        data[12]!=0 || data[13]!=0 || data[14]!=0 || data[15]!=4 ||
        data[16]!=0 || data[17]!=0 || data[18]!=0 || data[19]!=0x20 ||
        std::memcmp(data+0x20,"SETT",4)!=0 ||
        data[0x24]!=0 || data[0x25]!=0 || data[0x26]!=0 || data[0x27]!=1 ||
        data[0x28]!=0 || data[0x29]!=0 || data[0x2a]!=0 || data[0x2b]!=0x50 ||
        data[offset]!=0) return false;
    const uint32_t declared=(uint32_t(data[8])<<24)|(uint32_t(data[9])<<16)|(uint32_t(data[10])<<8)|data[11];
    if (declared!=length) return false;
    data[offset]=2; // TRACKSELECTION_ALL, as used by Retro Rewind's menu.
    return true;
}
}
