#pragma once
#include <cstdint>
#include <optional>
namespace PcLiveLocale {
struct Message {uint32_t index,address,fontAddress,font;};
uint32_t RequiredBytes();
void Install(uint32_t base);
void SetRaceContext(bool race);
std::optional<Message> Find(uint32_t id);
std::optional<Message> FromIndex(uint32_t encoded);
}
