#pragma once
#include <string>
#include <vector>
namespace settings_overlay {
unsigned PcGraphicsValue(unsigned sheet,unsigned row);
void SetPcGraphicsValue(unsigned sheet,unsigned row,unsigned value);
unsigned PcGraphicsScroller(unsigned sheet);
void SetPcGraphicsScroller(unsigned sheet,unsigned value);
std::vector<std::u16string> PcControllerDevices();
std::u16string PcControllerPlayerName(unsigned port);
bool PcControllerSelected(unsigned port, unsigned device);
void PcControllerAssign(unsigned port, unsigned device, bool assign);
}
