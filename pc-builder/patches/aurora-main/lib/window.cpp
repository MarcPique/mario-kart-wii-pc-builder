#include "video.hpp"
#include "window.hpp"

#ifdef AURORA_ENABLE_GX
#include "imgui.hpp"
#include "webgpu/gpu.hpp"
#endif
#include "gamepad.hpp"
#include "input/router.hpp"
#include "input/sdl_input.hpp"
#include "internal.hpp"

#include <aurora/aurora.h>
#include <aurora/event.h>

#include <mutex>
#include <optional>
#include <aurora/gfx.h>
#include <aurora/render_size_limits.hpp>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_pixels.h>
#include <tracy/Tracy.hpp>
#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(SDL_PLATFORM_ANDROID)
#include <jni.h>
extern "C" void Android_LockActivityMutex(void);
extern "C" void Android_UnlockActivityMutex(void);
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

#ifdef AURORA_ENABLE_RMLUI
#include "rmlui.hpp"
#endif
#include "time_internal.hpp"
#include "dolphin/vi/vi_internal.hpp"

namespace aurora::window {
namespace {
constexpr Module Log{"aurora::window"};

SDL_Window* g_window;
SDL_Renderer* g_renderer;
float g_frameBufferScale = 0.f;
std::atomic_bool g_frameBufferAspectFit = true;
std::atomic_bool g_forceAspect169{false};
std::atomic<float> g_pcForcedAspect{16.f / 9.f};
std::atomic_bool g_presentSurfaceFill = false;
int g_presentAspectWidth = 0;
int g_presentAspectHeight = 0;
AuroraWindowSize g_windowSize;
std::vector<AuroraEvent> g_events;
std::deque<std::string> g_eventStrings;
std::atomic_bool g_backgrounded = false;
std::atomic_bool g_nativeResizePending = false;
// Render-target changes requested by window events, applied by the GX producer at frame begin.
std::mutex g_pendingSurfaceMutex;
std::optional<AuroraWindowSize> g_pendingSwapchainSize;
std::atomic_bool g_pendingSurfaceRefresh = false;
std::atomic<AuroraDisplayMode> g_displayMode{AURORA_DISPLAY_MODE_WINDOWED};
std::atomic_uint64_t g_refreshPeriodNanos = 0;
#if defined(SDL_PLATFORM_ANDROID)
std::atomic_bool g_surfaceReady = false;
#else
std::atomic_bool g_surfaceReady = true;
#endif
bool g_lastPaused = false;
bool g_gotFocus = false;
#if defined(_WIN32)
WNDPROC g_prevWindowProc = nullptr;
// Native window handle captured once when the hook is installed. The presenter checks the native
// size several times per slot, which used to be an SDL lookup for a handle that never changes.
HWND g_nativeWindowHandle = nullptr;
// Packed native client size ((width << 32) | height) kept by the window procedure; zero means
// unknown. Atomic because that procedure runs on the SDL thread while the presenter reads it.
std::atomic<uint64_t> g_nativeClientSize{0};

constexpr uint64_t pack_native_size(uint32_t width, uint32_t height) noexcept {
  return (static_cast<uint64_t>(width) << 32) | static_cast<uint64_t>(height);
}

LRESULT CALLBACK aurora_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  bool sizeChanged = msg == WM_SIZE;
  if (msg == WM_WINDOWPOSCHANGED) {
    const auto* pos = reinterpret_cast<const WINDOWPOS*>(lparam);
    sizeChanged = pos != nullptr && (pos->flags & SWP_NOSIZE) == 0;
  }
  if (sizeChanged) {
    g_nativeResizePending.store(true, std::memory_order_release);
    if (msg == WM_SIZE) {
      // WM_SIZE carries the new client size directly.
      g_nativeClientSize.store(pack_native_size(static_cast<uint32_t>(LOWORD(lparam)),
                                                static_cast<uint32_t>(HIWORD(lparam))),
                               std::memory_order_release);
    } else {
      // WM_WINDOWPOSCHANGED carries no client size. Invalidate before forwarding: DefWindowProc turns
      // it into a nested WM_SIZE through this procedure, which refills the cache authoritatively.
      g_nativeClientSize.store(0, std::memory_order_release);
    }
  }
  if (g_prevWindowProc == nullptr) {
    return DefWindowProc(hwnd, msg, wparam, lparam);
  }
  return CallWindowProc(g_prevWindowProc, hwnd, msg, wparam, lparam);
}

void install_window_proc_hook() noexcept {
  const auto props = SDL_GetWindowProperties(g_window);
  auto* hwnd = static_cast<HWND>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
  if (hwnd == nullptr) {
    return;
  }
  g_nativeWindowHandle = hwnd;
  g_nativeClientSize.store(0, std::memory_order_release);
  g_prevWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC,
                                                                reinterpret_cast<LONG_PTR>(aurora_window_proc)));
}

void uninstall_window_proc_hook() noexcept {
  if (g_window == nullptr || g_prevWindowProc == nullptr) {
    g_nativeWindowHandle = nullptr;
    g_nativeClientSize.store(0, std::memory_order_release);
    return;
  }
  const auto props = SDL_GetWindowProperties(g_window);
  auto* hwnd = static_cast<HWND>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
  if (hwnd != nullptr) {
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_prevWindowProc));
  }
  g_prevWindowProc = nullptr;
  g_nativeWindowHandle = nullptr;
  g_nativeClientSize.store(0, std::memory_order_release);
}
#endif

// Native client size of the window without going through SDL. Returns false when the platform has
// no cheap native query, in which case callers fall back to get_window_size().
bool query_native_client_size(uint32_t& width, uint32_t& height) noexcept {
#if defined(_WIN32)
  const uint64_t cached = g_nativeClientSize.load(std::memory_order_acquire);
  if (cached != 0) {
    width = static_cast<uint32_t>(cached >> 32);
    height = static_cast<uint32_t>(cached & 0xffffffffull);
    return true;
  }
  if (g_nativeWindowHandle != nullptr) {
    RECT client{};
    if (GetClientRect(g_nativeWindowHandle, &client) != 0) {
      width = static_cast<uint32_t>(std::max<LONG>(client.right - client.left, 0));
      height = static_cast<uint32_t>(std::max<LONG>(client.bottom - client.top, 0));
      if (width != 0 && height != 0) {
        g_nativeClientSize.store(pack_native_size(width, height), std::memory_order_release);
      }
      return true;
    }
  }
#else
  (void)width;
  (void)height;
#endif
  return false;
}

void update_mouse_pointer() {
	gamepad::set_mouse_pointer(0, 0, false);
	if (g_window == nullptr || SDL_GetMouseFocus() != g_window) {
		return;
	}

	const auto size = get_window_size();
	if (size.width == 0 || size.height == 0 || size.native_fb_width == 0 || size.native_fb_height == 0) {
		return;
	}

	float x = 0;
	float y = 0;
	SDL_GetMouseState(&x, &y);
	x *= static_cast<float>(size.native_fb_width) / size.width;
	y *= static_cast<float>(size.native_fb_height) / size.height;

	float left = 0;
	float top = 0;
	float width = static_cast<float>(size.native_fb_width);
	float height = static_cast<float>(size.native_fb_height);
#ifdef AURORA_ENABLE_GX
	const auto& source = webgpu::present_source();
	const auto viewport = webgpu::calculate_present_viewport(size.native_fb_width, size.native_fb_height,
		source.size.width, source.size.height);
	left = viewport.left;
	top = viewport.top;
	width = viewport.width;
	height = viewport.height;
#endif
	if (width <= 0 || height <= 0) {
		return;
	}

	const bool inside = x >= left && y >= top && x < left + width && y < top + height;
	gamepad::set_mouse_pointer(2 * (x - left) / width - 1, 2 * (y - top) / height - 1, inside);
}


void retain_event_strings(SDL_Event& event) {
  switch (event.type) {
  case SDL_EVENT_DROP_BEGIN:
  case SDL_EVENT_DROP_FILE:
  case SDL_EVENT_DROP_TEXT:
  case SDL_EVENT_DROP_COMPLETE:
  case SDL_EVENT_DROP_POSITION:
    break;
  default:
    return;
  }
  if (event.drop.source != nullptr) {
    g_eventStrings.emplace_back(event.drop.source);
    event.drop.source = g_eventStrings.back().c_str();
  }
  if (event.drop.data != nullptr) {
    g_eventStrings.emplace_back(event.drop.data);
    event.drop.data = g_eventStrings.back().c_str();
  }
}

bool operator==(const AuroraWindowSize& lhs, const AuroraWindowSize& rhs) {
  return lhs.width == rhs.width && lhs.height == rhs.height && lhs.fb_width == rhs.fb_width &&
         lhs.fb_height == rhs.fb_height && lhs.native_fb_height == rhs.native_fb_height &&
         lhs.native_fb_width == rhs.native_fb_width && lhs.scale == rhs.scale;
}

Vec2<int> scale_frame_buffer_to_aspect(int w, int h, float scale, float aspect) {
  if (w <= 0 || h <= 0) {
    return {std::max(w, 1), std::max(h, 1)};
  }
  const auto size = render_size_limits::scale_framebuffer_to_aspect(
      static_cast<uint32_t>(w), static_cast<uint32_t>(h), scale, aspect);
  return {static_cast<int>(size.width), static_cast<int>(size.height)};
}

void update_refresh_period() noexcept {
  const SDL_DisplayID display = g_window != nullptr ? SDL_GetDisplayForWindow(g_window) : 0;
  const SDL_DisplayMode* mode = display != 0 ? SDL_GetCurrentDisplayMode(display) : nullptr;
  uint64_t period = 0;
  if (mode != nullptr && mode->refresh_rate_numerator > 0 && mode->refresh_rate_denominator > 0) {
    period = static_cast<uint64_t>(mode->refresh_rate_denominator) * 1'000'000'000ull /
             static_cast<uint64_t>(mode->refresh_rate_numerator);
  } else if (mode != nullptr && mode->refresh_rate > 0.f) {
    period = static_cast<uint64_t>(1e9 / mode->refresh_rate);
  }
  g_refreshPeriodNanos.store(period, std::memory_order_relaxed);
}

void resize_swapchain() noexcept {
  update_refresh_period();
  const auto size = get_window_size();
  if (size == g_windowSize) {
    return;
  }
  if (size.scale != g_windowSize.scale) {
    if (g_windowSize.scale > 0.f) {
      Log.info("Display scale changed to {}", size.scale);
    }
  }
  g_windowSize = size;
  if (g_renderer != nullptr) {
    SDL_SetRenderLogicalPresentation(g_renderer, static_cast<int>(size.native_fb_width),
                                     static_cast<int>(size.native_fb_height), SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderScale(g_renderer, size.scale, size.scale);
  }
#ifdef AURORA_ENABLE_GX
  std::lock_guard lock{g_pendingSurfaceMutex};
  g_pendingSwapchainSize = size;
#endif
}

} // namespace

void apply_pending_surface_changes() noexcept {
#ifdef AURORA_ENABLE_GX
  std::optional<AuroraWindowSize> size;
  {
    std::lock_guard lock{g_pendingSurfaceMutex};
    size.swap(g_pendingSwapchainSize);
  }
  if (size) {
    webgpu::resize_swapchain(size->fb_width, size->fb_height, size->native_fb_width, size->native_fb_height);
  }
  if (g_pendingSurfaceRefresh.exchange(false, std::memory_order_acq_rel)) {
    webgpu::refresh_surface(false);
  }
#endif
}

namespace {

void set_window_icon() noexcept {
  if (g_config.iconRGBA8 == nullptr) {
    return;
  }
  auto* iconSurface =
      SDL_CreateSurfaceFrom(static_cast<int>(g_config.iconWidth), static_cast<int>(g_config.iconHeight),
                            SDL_GetPixelFormatForMasks(32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000),
                            g_config.iconRGBA8, static_cast<int>(4 * g_config.iconWidth));
  AURORA_ASSERT(iconSurface != nullptr, "Failed to create icon surface: {}", SDL_GetError());
  TRY_WARN(SDL_SetWindowIcon(g_window, iconSurface), "Failed to set window icon: {}", SDL_GetError());
  SDL_DestroySurface(iconSurface);
}

bool targets_primary_window(const SDL_Event* event) noexcept {
  const SDL_Window* eventWindow = SDL_GetWindowFromEvent(event);
  return eventWindow == nullptr || eventWindow == g_window;
}

bool SDLCALL lifecycle_event_watch(void*, SDL_Event* event) {
  if (targets_primary_window(event)) {
    switch (event->type) {
#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_APPLE)
    case SDL_EVENT_WINDOW_MINIMIZED:
      time::internal::set_pause_reason(time::internal::PauseReason::Background, true);
      g_backgrounded.store(true, std::memory_order_relaxed);
      break;
    case SDL_EVENT_WINDOW_RESTORED:
      g_backgrounded.store(false, std::memory_order_relaxed);
      time::internal::set_pause_reason(time::internal::PauseReason::Background, false);
      break;
#endif
    default:
      break;
    }
  }
  return true;
}

void sync_paused() {
  const bool paused = is_paused();
  if (g_lastPaused == paused) {
    return;
  }
  g_lastPaused = paused;
  time::internal::set_pause_reason(time::internal::PauseReason::Window, paused);
  g_events.push_back(AuroraEvent{
      .type = paused ? AURORA_PAUSED : AURORA_UNPAUSED,
  });
}

bool is_alt_enter_event(const SDL_Event& event) noexcept {
  if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
    return false;
  }

  const SDL_Keymod altMask = static_cast<SDL_Keymod>(SDL_KMOD_LALT | SDL_KMOD_RALT | SDL_KMOD_ALT);
  return event.key.key == SDLK_RETURN && (event.key.mod & altMask) != 0;
}

void process_event(SDL_Event& event) {
  const bool primaryWindow = targets_primary_window(&event);
  switch (event.type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
  case SDL_EVENT_MOUSE_MOTION:
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
  case SDL_EVENT_MOUSE_WHEEL:
	if (primaryWindow) {
		video::notify_input();
	}
	break;
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
  case SDL_EVENT_GAMEPAD_BUTTON_UP:
  case SDL_EVENT_GAMEPAD_AXIS_MOTION:
	video::notify_input();
	break;
  default:
	break;
  }

  const bool routed = primaryWindow && input::sdl::is_routed_event(event);
  if (primaryWindow && !routed) {
#ifdef AURORA_ENABLE_GX
    imgui::process_event(event);
#endif
#ifdef AURORA_ENABLE_RMLUI
    rmlui::handle_window_event(event);
#endif
  }

  switch (event.type) {
  case SDL_EVENT_KEY_DOWN:
    if (is_alt_enter_event(event)) {
      set_display_mode(get_fullscreen() ? AURORA_DISPLAY_MODE_WINDOWED : AURORA_DISPLAY_MODE_BORDERLESS);
      g_nativeResizePending.store(true, std::memory_order_release);
    }
    break;
  case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
    update_refresh_period();
    break;
  case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    if (primaryWindow) {
      update_refresh_period();
    }
    break;
  case SDL_EVENT_WINDOW_MOVED: {
    if (!primaryWindow) {
      break;
    }
    g_events.push_back(AuroraEvent{
        .type = AURORA_WINDOW_MOVED,
        .windowPos = {.x = event.window.data1, .y = event.window.data2},
    });
    break;
  }
  case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: {
    if (!primaryWindow) {
      break;
    }
    resize_swapchain();
    g_events.push_back(AuroraEvent{
        .type = AURORA_DISPLAY_SCALE_CHANGED,
        .windowSize = get_window_size(),
    });
    break;
  }
  case SDL_EVENT_WINDOW_RESIZED:
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
    if (!primaryWindow) {
      break;
    }
    resize_swapchain();
    g_events.push_back(AuroraEvent{
        .type = AURORA_WINDOW_RESIZED,
        .windowSize = get_window_size(),
    });
    break;
  }
  case SDL_EVENT_GAMEPAD_ADDED: {
    auto instance = gamepad::add_controller(event.gdevice.which);
    if (instance != 0 && instance != static_cast<SDL_JoystickID>(-1)) {
      input::sdl::gamepad_added(instance);
    }
    g_events.push_back(AuroraEvent{
        .type = AURORA_CONTROLLER_ADDED,
        .controller = instance,
    });
    break;
  }
  case SDL_EVENT_GAMEPAD_REMOVED: {
    input::sdl::gamepad_removed(event.gdevice.which);
    gamepad::remove_controller(event.gdevice.which);
    g_events.push_back(AuroraEvent{
        .type = AURORA_CONTROLLER_REMOVED,
        .controller = event.gdevice.which,
    });
    break;
  }
  case SDL_EVENT_GAMEPAD_REMAPPED:
    input::sdl::gamepad_remapped(event.gdevice.which);
    // The default PAD table depends on the gamepad type, which a new mapping can change.
    if (gamepad::refresh_controller(event.gdevice.which)) {
      g_events.push_back(AuroraEvent{
          .type = AURORA_CONTROLLER_ADDED,
          .controller = event.gdevice.which,
      });
    }
    break;
  case SDL_EVENT_MOUSE_WHEEL:
    if (primaryWindow) {
      gamepad::set_mouse_scroll(event.wheel.x, event.wheel.y);
    }
    break;
  case SDL_EVENT_WINDOW_FOCUS_LOST:
    if (primaryWindow) {
      input::detail::focus_lost();
    }
    break;
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    if (primaryWindow) {
      g_events.push_back(AuroraEvent{
          .type = AURORA_EXIT,
      });
    }
    break;
  case SDL_EVENT_QUIT:
    g_events.push_back(AuroraEvent{
        .type = AURORA_EXIT,
    });
    break;
  default:
    if (event.type == g_sdlCustomEventsStart + CustomEvent::FutureResize) {
      // Queue the new size here; the GX producer applies it at the next frame boundary.
      resize_swapchain();
      g_events.push_back(AuroraEvent{
          .type = AURORA_WINDOW_RESIZED,
          .windowSize = get_window_size(),
      });
    } else if (event.type == g_sdlCustomEventsStart + CustomEvent::RefreshSurface) {
      // Refresh surface (vsync changed)
      g_pendingSurfaceRefresh.store(true, std::memory_order_release);
    }
    break;
  }

  if (routed) {
    input::sdl::dispatch(event);
  }
  if (primaryWindow) {
    sync_paused();
  }
  retain_event_strings(event);
  g_events.push_back(AuroraEvent{
      .type = AURORA_SDL_EVENT,
      .sdl = event,
  });
}
} // namespace

const AuroraEvent* poll_events() {
  ZoneScoped;
  g_events.clear();
  g_eventStrings.clear();

  SDL_Event event;
  // Clear out the previous scroll values to prevent ghost input
  gamepad::set_mouse_scroll(0, 0);
  if (is_paused()) {
    ZoneScopedN("SDL_WaitEvent (paused)");
    if (SDL_WaitEvent(&event)) {
      process_event(event);
    } else {
      Log.warn("SDL_WaitEvent failed: {}", SDL_GetError());
    }
  }
  while (true) {
    bool hasEvent = false;
    {
      ZoneScopedN("SDL_PollEvent");
      hasEvent = SDL_PollEvent(&event);
    }
    if (hasEvent) {
      process_event(event);
    } else {
      break;
    }
  }
  input::reconcile();
  input::sdl::update_cursor();
  update_mouse_pointer();
  g_events.push_back(AuroraEvent{
      .type = AURORA_NONE,
  });
  return g_events.data();
}

bool create_window(AuroraBackend backend) {
  SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
#if TARGET_OS_IOS || TARGET_OS_TV
  flags |= SDL_WINDOW_FULLSCREEN;
  if (g_config.startHidden) {
    flags |= SDL_WINDOW_HIDDEN;
  }
#else
  flags |= SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
  if (g_config.startFullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
  }
#endif
  auto width = static_cast<Sint32>(g_config.windowWidth);
  auto height = static_cast<Sint32>(g_config.windowHeight);
  if (width == 0 || height == 0) {
    width = 1280;
    height = 960;
  }
  if (width < 640) {
    width = 640;
  }
  if (height < 480) {
    height = 480;
  }

  const Sint32 posX = g_config.hasWindowPosition ? g_config.windowPosX : SDL_WINDOWPOS_CENTERED;
  const Sint32 posY = g_config.hasWindowPosition ? g_config.windowPosY : SDL_WINDOWPOS_CENTERED;

  const auto props = SDL_CreateProperties();
  TRY(SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, g_config.appName), "Failed to set {}: {}",
      SDL_PROP_WINDOW_CREATE_TITLE_STRING, SDL_GetError());
  TRY(SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, posX), "Failed to set {}: {}",
      SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_GetError());
  TRY(SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, posY), "Failed to set {}: {}",
      SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_GetError());
  TRY(SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width), "Failed to set {}: {}",
      SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, SDL_GetError());
  TRY(SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height), "Failed to set {}: {}",
      SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, SDL_GetError());
  TRY(SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, flags), "Failed to set {}: {}",
      SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, SDL_GetError());
  TRY(SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, backend != BACKEND_NULL),
      "Failed to set {}: {}", SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, SDL_GetError());
  g_window = SDL_CreateWindowWithProperties(props);
  if (g_window == nullptr) {
    Log.error("Failed to create window: {}", SDL_GetError());
    return false;
  }
  SDL_SetWindowMinimumSize(g_window, 640, 480);
  update_refresh_period();
  g_displayMode.store(g_config.startFullscreen ? AURORA_DISPLAY_MODE_BORDERLESS : AURORA_DISPLAY_MODE_WINDOWED,
                      std::memory_order_release);
#if defined(_WIN32)
  install_window_proc_hook();
#endif
  set_window_icon();
  return true;
}

bool create_renderer() {
  if (g_window == nullptr) {
    return false;
  }
  const auto props = SDL_CreateProperties();
  TRY(SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, g_window), "Failed to set {}: {}",
      SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, SDL_GetError());
  // Never vsync: presentation must not throttle the guest's own pacing.
  TRY(SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_RENDERER_VSYNC_DISABLED),
      "Failed to set {}: {}", SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_GetError());
  g_renderer = SDL_CreateRendererWithProperties(props);
  if (g_renderer == nullptr) {
    Log.error("Failed to create renderer: {}", SDL_GetError());
    return false;
  }
  return true;
}

void destroy_window() {
#if defined(_WIN32)
  uninstall_window_proc_hook();
#endif
  if (g_renderer != nullptr) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = nullptr;
  }
  if (g_window != nullptr) {
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
  }
}

void show_window() {
  if (g_window != nullptr) {
    TRY_WARN(SDL_ShowWindow(g_window), "Failed to show window: {}", SDL_GetError());
  }
}

bool initialize() {
  /* We don't want to initialize anything input related here, otherwise the add events will get lost to the void */
  TRY(SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight"), "Error setting {}: {}", SDL_HINT_ORIENTATIONS,
      SDL_GetError());
  TRY(SDL_InitSubSystem(SDL_INIT_EVENTS | SDL_INIT_VIDEO), "Error initializing SDL: {}", SDL_GetError());
  time::internal::set_pause_reason(time::internal::PauseReason::Surface,
                                   !g_surfaceReady.load(std::memory_order_acquire));

#if !defined(_WIN32) && !defined(__APPLE__)
  TRY(SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0"), "Error setting {}: {}",
      SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, SDL_GetError());
#endif
  TRY(SDL_SetHint(SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME, g_config.appName), "Error setting {}: {}",
      SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME, SDL_GetError());
  TRY(SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE_RUMBLE_BRAKE, "1"), "Error setting {}: {}",
      SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE_RUMBLE_BRAKE, SDL_GetError());
  // Treat touch and mouse as separate input sources
  TRY(SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0"), "Error setting {}: {}", SDL_HINT_TOUCH_MOUSE_EVENTS,
      SDL_GetError());
  TRY(SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0"), "Error setting {}: {}", SDL_HINT_MOUSE_TOUCH_EVENTS,
      SDL_GetError());

  TRY(SDL_DisableScreenSaver(), "Error disabling screensaver: {}", SDL_GetError());
  if (g_config.allowJoystickBackgroundEvents) {
    TRY(SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1"), "Error setting {}: {}",
        SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, SDL_GetError());
  }

  return true;
}

bool initialize_event_watch() {
  TRY(SDL_AddEventWatch(lifecycle_event_watch, nullptr), "Error adding SDL event watch: {}", SDL_GetError());
  return true;
}

void shutdown() {
  SDL_RemoveEventWatch(lifecycle_event_watch, nullptr);
  destroy_window();
  TRY_WARN(SDL_EnableScreenSaver(), "Error enabling screensaver: {}", SDL_GetError());
  SDL_Quit();
}

AuroraWindowSize get_window_size() {
  int width = 0;
  int height = 0;
  int native_fb_w = 0;
  int native_fb_h = 0;
  AURORA_ASSERT(SDL_GetWindowSize(g_window, &width, &height), "Failed to get window size: {}", SDL_GetError());
  AURORA_ASSERT(SDL_GetWindowSizeInPixels(g_window, &native_fb_w, &native_fb_h), "Failed to get window size in pixels: {}",
         SDL_GetError());

  int fb_w = native_fb_w;
  int fb_h = native_fb_h;
  const auto [baseW, baseH] = vi::configured_fb_size();
  if (g_forceAspect169.load(std::memory_order_acquire) && native_fb_w > 0 && native_fb_h > 0) {
    if (g_frameBufferScale > 0.f && baseW > 0 && baseH > 0) {
      const auto [scaledW, scaledH] =
          scale_frame_buffer_to_aspect(static_cast<int>(baseW), static_cast<int>(baseH),
                                       g_frameBufferScale, g_pcForcedAspect.load(std::memory_order_acquire));
      fb_w = scaledW;
      fb_h = scaledH;
    } else {
      fb_w = std::min(native_fb_w,
                      std::max(1, static_cast<int>(std::lround(native_fb_h * (g_pcForcedAspect.load(std::memory_order_acquire))))));
      fb_h = std::min(native_fb_h,
                      std::max(1, static_cast<int>(std::lround(native_fb_w * (1.f / g_pcForcedAspect.load(std::memory_order_acquire))))));
    }
  } else if (g_frameBufferAspectFit.load(std::memory_order_acquire) && baseW > 0 && baseH > 0) {
    float renderScale = g_frameBufferScale > 0.f ? g_frameBufferScale : 1.f;
    if (g_frameBufferScale <= 0.f) {
      renderScale = std::min(static_cast<float>(native_fb_w) / static_cast<float>(baseW),
                             static_cast<float>(native_fb_h) / static_cast<float>(baseH));
      if (renderScale < 1.f) {
        renderScale = 1.f;
      }
    }
    fb_w = std::max(1, static_cast<int>(std::lround(static_cast<float>(baseW) * renderScale)));
    fb_h = std::max(1, static_cast<int>(std::lround(static_cast<float>(baseH) * renderScale)));
  } else if (g_frameBufferScale > 0.f && baseW > 0 && baseH > 0) {
    const auto [scaledW, scaledH] =
        scale_frame_buffer_to_aspect(static_cast<int>(baseW), static_cast<int>(baseH), g_frameBufferScale,
                                     static_cast<float>(fb_w) / static_cast<float>(fb_h));
    fb_w = scaledW;
    fb_h = scaledH;
  }

  const float scale = SDL_GetWindowDisplayScale(g_window);
  return {
      .width = static_cast<uint32_t>(width),
      .height = static_cast<uint32_t>(height),
      .fb_width = static_cast<uint32_t>(fb_w),
      .fb_height = static_cast<uint32_t>(fb_h),
      .native_fb_width = static_cast<uint32_t>(native_fb_w),
      .native_fb_height = static_cast<uint32_t>(native_fb_h),
      .scale = scale,
  };
}

SDL_Window* get_sdl_window() { return g_window; }

SDL_Renderer* get_sdl_renderer() { return g_renderer; }

bool is_paused() noexcept {
  if (!is_presentable()) {
    return true;
  }
  const auto flags = SDL_GetWindowFlags(g_window);
  if ((flags & SDL_WINDOW_HIDDEN) != 0u) {
    return !g_config.startHidden;
  }
  // Wait until the window has received focus before respecting pauseOnFocusLost
  if (!g_gotFocus) {
    g_gotFocus = (flags & SDL_WINDOW_INPUT_FOCUS) != 0u;
    return false;
  }
  return g_config.pauseOnFocusLost && ((flags & SDL_WINDOW_INPUT_FOCUS) == 0u || (flags & SDL_WINDOW_MINIMIZED) != 0u);
}

bool is_presentable() noexcept {
  return g_window != nullptr && !g_backgrounded.load(std::memory_order_acquire) &&
         g_surfaceReady.load(std::memory_order_acquire);
}

void set_surface_ready(bool ready) noexcept {
  g_surfaceReady.store(ready, std::memory_order_release);
  time::internal::set_pause_reason(time::internal::PauseReason::Surface, !ready);
}

SurfaceLock::SurfaceLock() noexcept {
#if defined(SDL_PLATFORM_ANDROID)
  Android_LockActivityMutex();
#endif
}

SurfaceLock::~SurfaceLock() {
#if defined(SDL_PLATFORM_ANDROID)
  Android_UnlockActivityMutex();
#endif
}

bool push_custom_event(CustomEvent eventType) {
  SDL_Event event{.type = g_sdlCustomEventsStart + eventType};
  return SDL_PushEvent(&event);
}

void set_title(const char* title) {
  TRY_WARN(SDL_SetWindowTitle(g_window, title), "Failed to set window title: {}", SDL_GetError());
}

void set_fullscreen(bool fullscreen) {
  set_display_mode(fullscreen ? AURORA_DISPLAY_MODE_BORDERLESS : AURORA_DISPLAY_MODE_WINDOWED);
}

bool get_fullscreen() { return (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN) != 0u; }

void set_display_mode(AuroraDisplayMode mode) {
  if (g_window == nullptr) {
    return;
  }
  if (mode < AURORA_DISPLAY_MODE_WINDOWED || mode > AURORA_DISPLAY_MODE_EXCLUSIVE) {
    Log.warn("Ignoring invalid display mode {}", static_cast<int>(mode));
    return;
  }
  const AuroraDisplayMode currentMode = g_displayMode.load(std::memory_order_acquire);
  // Exclusive mode is re-applied even when already active: the desired refresh
  // rate tracks the frame interpolation target, which can change at runtime.
  if (mode == currentMode && mode != AURORA_DISPLAY_MODE_EXCLUSIVE) {
    return;
  }

  bool success = true;
  if (mode == AURORA_DISPLAY_MODE_WINDOWED) {
    success = SDL_SetWindowFullscreen(g_window, false);
    if (success) {
      success = SDL_SetWindowFullscreenMode(g_window, nullptr);
    }
  } else if (mode == AURORA_DISPLAY_MODE_BORDERLESS) {
    if (SDL_GetWindowFullscreenMode(g_window) != nullptr) {
      success = SDL_SetWindowFullscreen(g_window, false);
    }
    if (success) {
      success = SDL_SetWindowFullscreenMode(g_window, nullptr);
    }
    if (success) {
      success = SDL_SetWindowFullscreen(g_window, true);
    }
  } else {
    const SDL_DisplayID display = SDL_GetDisplayForWindow(g_window);
    const SDL_DisplayMode* desktop = display != 0 ? SDL_GetDesktopDisplayMode(display) : nullptr;
    const uint32_t interpolationFps = aurora_get_frame_interpolation_fps();
    const float targetHz = interpolationFps > 60 ? static_cast<float>(interpolationFps) : 60.0f;
    SDL_DisplayMode closest{};
    if (desktop == nullptr ||
        !SDL_GetClosestFullscreenDisplayMode(display, desktop->w, desktop->h, targetHz, true, &closest)) {
      Log.warn("No exclusive fullscreen mode near {:.0f} Hz is available: {}", targetHz, SDL_GetError());
      return;
    }
    const SDL_DisplayMode* active = SDL_GetWindowFullscreenMode(g_window);
    if (mode == currentMode && active != nullptr && active->w == closest.w && active->h == closest.h &&
        active->refresh_rate == closest.refresh_rate) {
      return;
    }
    if (get_fullscreen() && mode != currentMode) {
      success = SDL_SetWindowFullscreen(g_window, false);
    }
    if (success) {
      success = SDL_SetWindowFullscreenMode(g_window, &closest);
    }
    if (success) {
      success = SDL_SetWindowFullscreen(g_window, true);
    }
    if (success) {
      Log.info("Exclusive fullscreen requested at {}x{} {:.3f} Hz", closest.w, closest.h, closest.refresh_rate);
    }
  }

  if (!success) {
    Log.warn("Failed to change display mode: {}", SDL_GetError());
    return;
  }
  g_displayMode.store(mode, std::memory_order_release);
  g_nativeResizePending.store(true, std::memory_order_release);
  TRY_WARN(push_custom_event(CustomEvent::FutureResize), "Failed to queue fullscreen resize event: {}",
           SDL_GetError());
}

AuroraDisplayMode get_display_mode() { return g_displayMode.load(std::memory_order_acquire); }

uint64_t refresh_period_nanos() noexcept { return g_refreshPeriodNanos.load(std::memory_order_relaxed); }

void set_window_size(uint32_t width, uint32_t height) {
  TRY_WARN(SDL_RestoreWindow(g_window), "Failed to un-maximize window: {}", SDL_GetError());
  TRY_WARN(SDL_SetWindowSize(g_window, width, height), "Failed to set window size: {}", SDL_GetError());
}

void set_window_position(uint32_t x, uint32_t y) {
  TRY_WARN(SDL_SetWindowPosition(g_window, x, y), "Failed to set window position: {}", SDL_GetError());
}

void center_window() {
  TRY_WARN(SDL_SetWindowPosition(g_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED),
           "Failed to center window: {}", SDL_GetError());
}

void sync_frame_buffer_size() noexcept {
  if (g_window != nullptr) {
    resize_swapchain();
  }
}

static void push_future_resize_event() {
  TRY_WARN(push_custom_event(CustomEvent::FutureResize), "Failed to push SDL event for future resize: {}",
           SDL_GetError());
}

void request_frame_buffer_resize() {
  if (g_window != nullptr) {
    // Publish the request before waking the SDL loop: the render worker consumes it at an ordered
    // frame boundary, and the main thread must never recreate WebGPU targets mid-frame.
    g_nativeResizePending.store(true, std::memory_order_release);
    push_future_resize_event();
  }
}

void set_frame_buffer_scale(float scale) {
  if (scale < 0.f) {
    scale = 0.f;
  }
  if (g_frameBufferScale == scale) {
    return;
  }
  g_frameBufferScale = scale;
  request_frame_buffer_resize();
}

void set_frame_buffer_aspect_fit(bool fit) {
  if (g_frameBufferAspectFit.exchange(fit, std::memory_order_acq_rel) == fit) {
    return;
  }

  request_frame_buffer_resize();
}

extern "C" void aurora_pc_set_fixed_aspect(unsigned width,unsigned height) noexcept {
  const float value=height?static_cast<float>(width)/height:16.f/9.f;
  if(!std::isfinite(value)||value<0.25f||value>6.f)return;
  if(g_pcForcedAspect.exchange(value,std::memory_order_acq_rel)!=value)request_frame_buffer_resize();
}

void set_force_aspect_16_9(bool force) {
  if (g_forceAspect169.load(std::memory_order_relaxed) == force) {
    return;
  }
  g_forceAspect169.store(force, std::memory_order_release);
  request_frame_buffer_resize();
}

void set_present_surface_fill(bool fill) {
  g_presentSurfaceFill.store(fill, std::memory_order_release);
}

void lock_present_aspect_ratio(int width, int height) {
  if (width <= 0 || height <= 0) {
    unlock_present_aspect_ratio();
    return;
  }
  if (g_presentAspectWidth == width && g_presentAspectHeight == height) {
    return;
  }
  g_presentAspectWidth = width;
  g_presentAspectHeight = height;
}

void unlock_present_aspect_ratio() {
  g_presentAspectWidth = 0;
  g_presentAspectHeight = 0;
}

bool get_present_aspect_ratio(float& aspect) noexcept {
  if (g_forceAspect169.load(std::memory_order_acquire)) {
    aspect = g_pcForcedAspect.load(std::memory_order_acquire);
    return true;
  }
  if (g_presentSurfaceFill.load(std::memory_order_acquire) && g_window != nullptr) {
    // Queried once per presentation snapshot; use the cached native client size
    // instead of re-entering SDL for a value the window procedure already knows.
    uint32_t nativeWidth = 0;
    uint32_t nativeHeight = 0;
    if (query_native_client_size(nativeWidth, nativeHeight) && nativeWidth > 0 && nativeHeight > 0) {
      aspect = static_cast<float>(nativeWidth) / static_cast<float>(nativeHeight);
      return true;
    }
    int width = 0;
    int height = 0;
    if (SDL_GetWindowSizeInPixels(g_window, &width, &height) && width > 0 && height > 0) {
      aspect = static_cast<float>(width) / static_cast<float>(height);
      return true;
    }
  }
  if (g_presentAspectWidth <= 0 || g_presentAspectHeight <= 0) {
    aspect = 0.f;
    return false;
  }
  aspect = (static_cast<float>(g_presentAspectWidth) / static_cast<float>(g_presentAspectHeight)) *
           vi::present_aspect_correction();
  return aspect > 0.f;
}

void set_background_input(bool value) { SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, value ? "1" : "0"); }

} // namespace aurora::window

#if defined(SDL_PLATFORM_ANDROID)
extern "C" JNIEXPORT void JNICALL Java_dev_encounter_aurora_AuroraSurface_nativeSetSurfaceReady(JNIEnv*, jclass,
                                                                                                jboolean ready) {
  aurora::window::set_surface_ready(ready == JNI_TRUE);
}
#endif
