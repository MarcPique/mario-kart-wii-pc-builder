#include "runtime_product.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>

namespace {
BOOL CALLBACK ApplyMarioKartWindowIdentity(HWND window, LPARAM) {
    DWORD owner = 0;
    GetWindowThreadProcessId(window, &owner);
    if (owner != GetCurrentProcessId() || GetWindow(window, GW_OWNER)) return TRUE;
    wchar_t className[128]{};
    GetClassNameW(window, className, 128);
    if (wcsncmp(className, L"SDL", 3) != 0) return TRUE;
    SetWindowTextW(window, L"Mario Kart Wii");
    const auto module = GetModuleHandleW(nullptr);
    auto small = LoadImageW(module, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_SHARED);
    auto large = LoadImageW(module, MAKEINTRESOURCEW(1), IMAGE_ICON, 256, 256, LR_SHARED);
    DWORD_PTR ignored = 0;
    if (small) SendMessageTimeoutW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small), SMTO_ABORTIFHUNG, 1000, &ignored);
    if (large) SendMessageTimeoutW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large), SMTO_ABORTIFHUNG, 1000, &ignored);
    SetLastError(ERROR_SUCCESS);
    return FALSE;
}
DWORD WINAPI SetMarioKartWindowIdentity(void*) {
    for (int attempt = 0; attempt < 80; ++attempt) {
        SetLastError(ERROR_NOT_FOUND);
        if (!EnumWindows(ApplyMarioKartWindowIdentity, 0) && GetLastError() == ERROR_SUCCESS) break;
        Sleep(250);
    }
    return 0;
}
}
#endif

namespace RuntimeProduct {

const Descriptor& Active() noexcept {
#ifdef _WIN32
    static const bool initialized = [] {
        SetCurrentProcessExplicitAppUserModelID(L"MarioKartWii.Native");
        if (auto thread = CreateThread(nullptr, 0, SetMarioKartWindowIdentity, nullptr, 0, nullptr)) CloseHandle(thread);
        return true;
    }();
    (void)initialized;
#endif
    static constexpr Descriptor descriptor{
        Kind::BaseGame,
        "Mario Kart Wii",
    };
    return descriptor;
}

} // namespace RuntimeProduct
