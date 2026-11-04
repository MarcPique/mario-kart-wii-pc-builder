#pragma once
#include <cstddef>
namespace PcTexturePacks {
void Initialize();
void Pump() noexcept;
bool Busy();
unsigned Failed();
void Reload();
void SetEnabled(bool enabled);
bool Enabled();
size_t Count();
void OpenFolder();
}
