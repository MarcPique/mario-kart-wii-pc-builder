#pragma once
#include <cstddef>
namespace PcTexturePacks {
void Initialize();
void Reload();
void SetEnabled(bool enabled);
bool Enabled();
size_t Count();
void OpenFolder();
}
