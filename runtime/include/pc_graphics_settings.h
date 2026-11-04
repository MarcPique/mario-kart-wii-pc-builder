#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace settings_overlay {
unsigned PcGraphicsValue(unsigned sheet,unsigned row);
unsigned PcAspectIndex();
void PcSetAspect(unsigned index);
void SetPcGraphicsValue(unsigned sheet,unsigned row,unsigned value);
unsigned PcGraphicsScroller(unsigned sheet);
void SetPcGraphicsScroller(unsigned sheet,unsigned value);
std::vector<std::u16string> PcControllerDevices();
std::vector<uint32_t> PcControllerDeviceIds();
std::u16string PcControllerPlayerName(unsigned port);
bool PcControllerSelected(unsigned port, unsigned device);
bool PcControllerAssign(unsigned port, uint32_t deviceId, bool assign);
}
