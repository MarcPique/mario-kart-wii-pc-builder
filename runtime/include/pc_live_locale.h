#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace PcLiveLocale {
struct Message {uint32_t index,address,fontAddress,font;};
uint32_t RequiredBytes();
void Install(uint32_t base);
void SetRaceContext(bool race);
std::optional<Message> Find(uint32_t id);
std::optional<Message> FromIndex(uint32_t encoded);
const std::vector<std::string>& LanguageCodes();
std::u16string MenuText(std::u16string_view english);
}
