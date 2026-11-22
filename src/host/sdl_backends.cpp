#include "host/backends.hpp"
#include "host/play_menu.hpp"
#include "host/ui_locale.hpp"

#if defined(SH_HOST_SDL)
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <timeapi.h>
#include "../../assets/app_icon_id.h"
#pragma comment(lib, "winmm.lib")
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace sh {
namespace {

struct SdlPresent final : IPresentBackend {
  explicit SdlPresent(HostOptions* options) : options_(options) {}

  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  HostOptions* options_ = nullptr;
  bool menu_ = false;
  bool imgui_ = false;
  bool r3_was_ = false;
  bool l3_was_ = false;
  bool pad_up_ = false;
  bool pad_down_ = false;
  bool pad_left_ = false;
  bool pad_right_ = false;
  bool pad_a_ = false;
  bool pad_b_ = false;
  bool pad_lb_ = false;
  bool pad_rb_ = false;
  PlayMenu play_;
  SDL_Gamepad* menu_pad_ = nullptr;
  bool menu_pad_owned_ = false;
  SDL_JoystickID menu_pad_id_ = 0;
  int scale_ = 3;
  int applied_scale_ = -1;
  std::vector<u32> filtered_;
  std::vector<u32> hd_tmp_;
  SDL_Texture* mask_ = nullptr;
  SDL_Texture* hd_ = nullptr;
  int hd_w_ = 0;
  int hd_h_ = 0;
  bool pad_was_[32]{};
  bool lt_was_ = false;
  bool rt_was_ = false;
#if defined(_WIN32)
  bool timer_period_ = false;
#endif

  bool init(const char* title, int scale) override {
    scale_ = scale > 0 ? scale : 3;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
      return false;
    window = SDL_CreateWindow(title, kScreenW * scale_, kScreenH * scale_, SDL_WINDOW_RESIZABLE);
    if (!window)
      return false;
#if defined(_WIN32)
    // Match the .exe icon (assets/app.rc) on the window / taskbar.
    if (HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr))) {
      if (HICON icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON))) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
      }
    }
#endif
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
      return false;
    SDL_SetRenderVSync(renderer, 1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    imgui_ = true;
#if defined(_WIN32)
    // Default scheduler quantum is ~15 ms, which turns a 16.5 ms frame cap
    // into an uneven ~30 Hz. 1 ms resolution lets the cap hit 60.606 Hz.
    // WINMM requires a matching timeEndPeriod for each successful begin.
    if (timeBeginPeriod(1) == TIMERR_NOERROR)
      timer_period_ = true;
#endif
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, kScreenW, kScreenH);
    if (texture)
      SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    mask_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 3, 1);
    if (mask_) {
      const u8 px[12] = {255, 96, 96, 255, 96, 255, 96, 255, 96, 96, 255, 255};
      SDL_UpdateTexture(mask_, nullptr, px, 12);
      SDL_SetTextureBlendMode(
          mask_, SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_ZERO,
                                             SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO,
                                             SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD));
      SDL_SetTextureScaleMode(mask_, SDL_SCALEMODE_NEAREST);
    }
    if (texture && !play_.init(window, renderer))
      std::fprintf(stderr, "Play menu unavailable. Escape closes the window.\n");
    return texture != nullptr;
  }

  static bool same_rgb(u32 a, u32 b) { return (a & 0x00FFFFFFu) == (b & 0x00FFFFFFu); }

  static u32 mix_rgb(u32 a, u32 b, int wa, int wb) {
    auto ch = [&](int shift) {
      const int c = (int((a >> shift) & 255) * wa + int((b >> shift) & 255) * wb) / (wa + wb);
      return u32(c) << shift;
    };
    return (a & 0xFF000000u) | ch(16) | ch(8) | ch(0);
  }

  // Scale2x, then a light blend only on pixels that changed. Flat color stays
  // sharp; diagonals become a short ramp instead of a stair.
  void scale2x(std::span<const u32> src, int w, int h, std::vector<u32>& dst) const {
    dst.assign(size_t(w * 2 * h * 2), 0);
    const int dw = w * 2;
    auto at = [&](int x, int y) {
      if (x < 0)
        x = 0;
      if (y < 0)
        y = 0;
      if (x >= w)
        x = w - 1;
      if (y >= h)
        y = h - 1;
      return src[size_t(y * w + x)];
    };
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const u32 b = at(x, y - 1);
        const u32 d = at(x - 1, y);
        const u32 e = at(x, y);
        const u32 f = at(x + 1, y);
        const u32 h = at(x, y + 1);
        u32 e0 = e, e1 = e, e2 = e, e3 = e;
        if (!same_rgb(b, h) && !same_rgb(d, f)) {
          if (same_rgb(d, b))
            e0 = mix_rgb(d, e, 3, 1);
          if (same_rgb(b, f))
            e1 = mix_rgb(f, e, 3, 1);
          if (same_rgb(d, h))
            e2 = mix_rgb(d, e, 3, 1);
          if (same_rgb(h, f))
            e3 = mix_rgb(f, e, 3, 1);
        }
        dst[size_t((y * 2) * dw + x * 2)] = e0;
        dst[size_t((y * 2) * dw + x * 2 + 1)] = e1;
        dst[size_t((y * 2 + 1) * dw + x * 2)] = e2;
        dst[size_t((y * 2 + 1) * dw + x * 2 + 1)] = e3;
      }
    }
  }

  void scale_hd(std::span<const u32> src, int w, int h) {
    scale2x(src, w, h, hd_tmp_);
    scale2x(hd_tmp_, w * 2, h * 2, filtered_);
  }

  SDL_Texture* ensure_hd(int w, int h) {
    if (hd_ && hd_w_ == w && hd_h_ == h)
      return hd_;
    if (hd_)
      SDL_DestroyTexture(hd_);
    hd_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    hd_w_ = hd_ ? w : 0;
    hd_h_ = hd_ ? h : 0;
    if (hd_) {
      SDL_SetTextureScaleMode(hd_, SDL_SCALEMODE_LINEAR);
      SDL_SetTextureBlendMode(hd_, SDL_BLENDMODE_BLEND);
    }
    return hd_;
  }

  void upload(SDL_Texture* target, std::span<const u32> frame, int w, int h) {
    void* pixels = nullptr;
    int pitch = 0;
    if (!target || frame.size() < size_t(w * h) || !SDL_LockTexture(target, nullptr, &pixels, &pitch))
      return;
    auto* dst = static_cast<u8*>(pixels);
    for (int y = 0; y < h; ++y)
      std::memcpy(dst + y * pitch, frame.data() + size_t(y * w), size_t(w) * 4);
    SDL_UnlockTexture(target);
  }

  void present(std::span<const u32> rgba8888, int w, int h) override {
    if (!texture || !renderer || w <= 0 || h <= 0)
      return;
    const int filter = active_filter();
    const bool hd = filter == 7 && rgba8888.size() >= size_t(w * h);
    SDL_Texture* picture = texture;
    if (hd) {
      scale_hd(rgba8888, w, h);
      picture = ensure_hd(w * 4, h * 4);
      if (picture)
        upload(picture, filtered_, w * 4, h * 4);
    } else {
      upload(texture, prepare_frame(rgba8888, w, h, filter), w, h);
    }
    if (!picture)
      picture = texture;
    const bool smooth = !hd && (filter == 1 || filter == 3 || filter == 4);
    SDL_SetTextureScaleMode(picture, smooth || hd ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(picture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(picture, 255);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    sync_window();
    int out_w = 0, out_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &out_w, &out_h);
    SDL_FRect dst;
    if (options_ && options_->stretch) {
      dst.x = 0.f;
      dst.y = 0.f;
      dst.w = float(out_w);
      dst.h = float(out_h);
    } else {
      int fit = 1;
      if (out_w >= kScreenW && out_h >= kScreenH) {
        const int fit_w = out_w / kScreenW;
        const int fit_h = out_h / kScreenH;
        fit = fit_w < fit_h ? fit_w : fit_h;
      }
      dst.w = float(kScreenW * fit);
      dst.h = float(kScreenH * fit);
      dst.x = (float(out_w) - dst.w) * 0.5f;
      dst.y = (float(out_h) - dst.h) * 0.5f;
    }
    if (!hd && filter == 4) {
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);
      SDL_SetTextureAlphaMod(texture, 70);
      SDL_FRect glow = dst;
      glow.x -= 3.f;
      glow.y -= 3.f;
      glow.w += 6.f;
      glow.h += 6.f;
      SDL_RenderTexture(renderer, texture, nullptr, &glow);
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
      SDL_SetTextureAlphaMod(texture, 255);
    }
    SDL_RenderTexture(renderer, picture, nullptr, &dst);
    if (!hd && (filter == 2 || filter == 3 || filter == 4))
      draw_scanlines(dst, filter == 2 ? 150 : filter == 3 ? 110 : 70);
    if (!hd && filter == 3 && mask_ && dst.w > 0.f)
      SDL_RenderTextureTiled(renderer, mask_, nullptr, dst.w / float(kScreenW * 3), &dst);
    if (play_.is_open())
      play_.draw();
    // Rml fires Aplicar during draw/Update — flush committed settings before
    // the next present so filters stick after closing the menu.
    if (options_)
      play_.take_applied(*options_);
    if (menu_ && imgui_)
      draw_menu();
    SDL_RenderPresent(renderer);
  }

  int active_filter() const {
    const int live = play_.live_filter();
    int filter = live >= 0 ? live : (options_ ? options_->image_filter : 0);
    if (filter < 0 || filter >= kImageFilterCount)
      filter = 0;
    return filter;
  }

  std::span<const u32> prepare_frame(std::span<const u32> src, int w, int h, int filter) {
    if ((filter != 5 && filter != 6) || src.size() < size_t(w * h))
      return src;
    filtered_.resize(size_t(w * h));
    auto channel = [](int c) {
      if (c < 0)
        return 0;
      if (c > 255)
        return 255;
      return c;
    };
    if (filter == 5) {
      for (int i = 0; i < w * h; ++i) {
        const u32 pix = src[size_t(i)];
        auto curve = [&](int c) {
          const float x = (float(c) / 255.f - 0.5f) * 1.38f + 0.5f;
          return channel(int(x * 255.f + 0.5f));
        };
        const int r = curve((pix >> 16) & 255);
        const int g = curve((pix >> 8) & 255);
        const int b = curve(pix & 255);
        filtered_[size_t(i)] = (pix & 0xFF000000u) | (u32(r) << 16) | (u32(g) << 8) | u32(b);
      }
      return filtered_;
    }
    auto sample = [&](int x, int y) {
      if (x < 0)
        x = 0;
      if (y < 0)
        y = 0;
      if (x >= w)
        x = w - 1;
      if (y >= h)
        y = h - 1;
      return src[size_t(y * w + x)];
    };
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const u32 pix = sample(x, y);
        const u32 up = sample(x, y - 1);
        const u32 down = sample(x, y + 1);
        const u32 left = sample(x - 1, y);
        const u32 right = sample(x + 1, y);
        u32 out = pix & 0xFF000000u;
        for (int shift = 0; shift <= 16; shift += 8) {
          const int c = int((pix >> shift) & 255);
          const int sharp = c * 5 - int((up >> shift) & 255) - int((down >> shift) & 255) -
                            int((left >> shift) & 255) - int((right >> shift) & 255);
          const int mixed = (channel(sharp) * 2 + c) / 3;
          out |= u32(mixed) << shift;
        }
        filtered_[size_t(y * w + x)] = out;
      }
    }
    return filtered_;
  }

  void draw_scanlines(const SDL_FRect& dst, int alpha) {
    if (dst.h < 2.f)
      return;
    const float row = dst.h / float(kScreenH);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, Uint8(alpha));
    for (int y = 0; y < kScreenH; ++y) {
      SDL_FRect line{dst.x, dst.y + y * row + row * 0.58f, dst.w, row * 0.42f};
      SDL_RenderFillRect(renderer, &line);
    }
  }

  void draw_menu() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    const MenuLocale& loc =
        menu_locale(options_ ? options_->language : 0);
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Appearing);
    // ### keeps a stable ImGui ID while the visible title follows language.
    char title[96];
    std::snprintf(title, sizeof(title), "%s###sh_debug", loc.debug_title);
    if (ImGui::Begin(title, &menu_)) {
      // Unlock FPS / Mono live in Escape options only — keep this panel for cheats.
      char game_hdr[64];
      std::snprintf(game_hdr, sizeof(game_hdr), "%s###dbg_game", loc.debug_section_game);
      if (ImGui::CollapsingHeader(game_hdr, ImGuiTreeNodeFlags_DefaultOpen)) {
        bool stage = options_ && options_->stage_select;
        if (ImGui::Checkbox(loc.debug_stage_select, &stage) && options_) {
          options_->stage_select = stage;
          options_->dirty = true;
        }
        bool skip = options_ && options_->skip_checksum;
        if (ImGui::Checkbox(loc.debug_fast_boot, &skip) && options_) {
          options_->skip_checksum = skip;
          options_->dirty = true;
        }
        bool god = options_ && options_->invincible;
        if (ImGui::Checkbox(loc.debug_invincible, &god) && options_) {
          options_->invincible = god;
          options_->dirty = true;
        }
        bool ghost = options_ && options_->untouchable;
        if (ImGui::Checkbox(loc.debug_untouchable, &ghost) && options_) {
          options_->untouchable = ghost;
          options_->dirty = true;
        }
      }
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
  }

  void set_vsync(bool on) override {
    if (renderer)
      SDL_SetRenderVSync(renderer, on ? 1 : 0);
  }

  bool menu_open() const override { return menu_ || play_.is_open(); }

  bool consume_reset() override { return play_.take_reset(); }

  void sync_window() {
    if (!options_ || !window)
      return;
    if (options_->fullscreen != fullscreen_)
      set_fullscreen(options_->fullscreen);
    int scale = options_->scale;
    if (scale < 1)
      scale = 1;
    if (scale > 6)
      scale = 6;
    if (fullscreen_) {
      applied_scale_ = -1;
    } else if (scale != applied_scale_) {
      SDL_SetWindowSize(window, kScreenW * scale, kScreenH * scale);
      applied_scale_ = scale;
    }
  }

  bool poll_events(bool& quit) override {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (play_.is_open() && !play_.capturing())
        play_.process(e);
      else if (!play_.is_open() && imgui_)
        ImGui_ImplSDL3_ProcessEvent(&e);
      if (e.type == SDL_EVENT_QUIT)
        quit = true;
      if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE) {
        if (menu_)
          menu_ = false;
        else if (play_.capturing())
          play_.cancel_capture();
        else if (play_.is_open())
          play_.hide();
        else if (play_.ready() && options_)
          play_.show(*options_);
        else
          quit = true;
      }
      if (play_.capturing() && e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        if (e.key.key != SDLK_ESCAPE)
          play_.take_key(int(e.key.scancode), e.key.key == SDLK_BACKSPACE || e.key.key == SDLK_DELETE);
      } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_F1) {
        if (menu_)
          menu_ = false;
        else if (options_ && options_->debug_menu)
          menu_ = true;
      }
      if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_F12 && play_.ready() &&
          options_ && !play_.capturing()) {
        if (play_.is_open())
          play_.hide();
        else
          play_.show(*options_);
      }
      if (e.type == SDL_EVENT_KEY_DOWN && play_.is_open() && !play_.capturing()) {
        if (e.key.key == SDLK_UP)
          play_.move(-1);
        else if (e.key.key == SDLK_DOWN)
          play_.move(1);
        else if (e.key.key == SDLK_LEFT)
          play_.horizontal(-1);
        else if (e.key.key == SDLK_RIGHT)
          play_.horizontal(1);
        else if (!e.key.repeat && (e.key.key == SDLK_TAB || e.key.key == SDLK_Q || e.key.key == SDLK_E)) {
          const bool back = e.key.key == SDLK_Q || (e.key.mod & SDL_KMOD_SHIFT) != 0;
          play_.cycle_tab(back ? -1 : 1);
        }
        else if (!e.key.repeat &&
                 (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER || e.key.key == SDLK_SPACE))
          play_.activate();
      }
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F11 && options_) {
        options_->fullscreen = !options_->fullscreen;
        options_->dirty = true;
      }
    }
    if (options_)
      play_.take_applied(*options_);
    if (play_.take_quit())
      quit = true;
    poll_menu_pad();
    return !quit;
  }

  void poll_menu_pad() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    SDL_Gamepad* pad = nullptr;
    if (ids && count > 0) {
      int index = options_ && options_->gamepad > 0 ? options_->gamepad : 0;
      const int chosen = play_.menu_pad_index();
      if (chosen >= 0)
        index = chosen;
      if (index >= count)
        index = 0;
      const SDL_JoystickID want = ids[index];
      if (menu_pad_id_ != want) {
        std::fill(std::begin(pad_was_), std::end(pad_was_), false);
        lt_was_ = rt_was_ = false;
        pad_up_ = pad_down_ = pad_left_ = pad_right_ = false;
        pad_a_ = pad_b_ = pad_lb_ = pad_rb_ = false;
        l3_was_ = r3_was_ = false;
      }
      pad = SDL_GetGamepadFromID(want);
      if (!pad || menu_pad_id_ != want) {
        if (menu_pad_owned_ && menu_pad_)
          SDL_CloseGamepad(menu_pad_);
        menu_pad_ = SDL_OpenGamepad(want);
        menu_pad_owned_ = menu_pad_ != nullptr;
        menu_pad_id_ = menu_pad_ ? want : 0;
        pad = menu_pad_ ? menu_pad_ : SDL_GetGamepadFromID(want);
      }
    }
    SDL_free(ids);
    if (!pad) {
      r3_was_ = false;
      return;
    }
    const bool r3 = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    if (r3 && !r3_was_) {
      if (menu_)
        menu_ = false;
      else if (options_ && options_->debug_menu)
        menu_ = true;
    }
    r3_was_ = r3;

    const bool l3 = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    if (l3 && !l3_was_ && play_.ready() && options_) {
      if (play_.capturing())
        play_.cancel_capture();
      else if (play_.is_open())
        play_.hide();
      else
        play_.show(*options_);
    }
    l3_was_ = l3;

    const bool listen = play_.is_open() && play_.capturing() && play_.capture_is_pad();
    bool took = false;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && b < 32; ++b) {
      const bool down = SDL_GetGamepadButton(pad, SDL_GamepadButton(b));
      if (listen && down && !pad_was_[b]) {
        play_.take_pad(b);
        if (!play_.capturing())
          took = true;
      }
      pad_was_[b] = down;
    }
    const bool lt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000;
    const bool rt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000;
    if (listen && lt && !lt_was_) {
      play_.take_pad(kPadTriggerLeft);
      if (!play_.capturing())
        took = true;
    }
    if (listen && rt && !rt_was_) {
      play_.take_pad(kPadTriggerRight);
      if (!play_.capturing())
        took = true;
    }
    lt_was_ = lt;
    rt_was_ = rt;
    if (took) {
      // The button that finished the bind is already held. Mark it so this
      // same press does not also activate, go back, or change tabs.
      pad_up_ = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
      pad_down_ = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
      pad_left_ = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
      pad_right_ = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
      pad_a_ = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);
      pad_b_ = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST);
      pad_lb_ = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
      pad_rb_ = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
      return;
    }

    if (!play_.is_open() || play_.capturing()) {
      if (play_.capturing() && !play_.capture_is_pad()) {
        const auto edge = [](bool down, bool& was) {
          const bool pressed = down && !was;
          was = down;
          return pressed;
        };
        if (edge(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST), pad_b_))
          play_.cancel_capture();
      }
      pad_up_ = pad_down_ = pad_left_ = pad_right_ = false;
      pad_a_ = pad_lb_ = pad_rb_ = false;
      if (!play_.capturing())
        pad_b_ = false;
      return;
    }
    const auto edge = [](bool down, bool& was) {
      const bool pressed = down && !was;
      was = down;
      return pressed;
    };
    if (edge(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP), pad_up_))
      play_.move(-1);
    if (edge(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN), pad_down_))
      play_.move(1);
    if (edge(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT), pad_left_))
      play_.horizontal(-1);
    if (edge(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT), pad_right_))
      play_.horizontal(1);
    if (edge(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH), pad_a_))
      play_.activate();
    if (edge(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST), pad_b_))
      play_.hide();
    if (edge(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER), pad_lb_))
      play_.cycle_tab(-1);
    if (edge(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER), pad_rb_))
      play_.cycle_tab(1);
  }

  void set_fullscreen(bool on) override {
    fullscreen_ = on;
    if (window)
      SDL_SetWindowFullscreen(window, on);
  }

  void shutdown() override {
    play_.shutdown();
    if (imgui_) {
      ImGui_ImplSDLRenderer3_Shutdown();
      ImGui_ImplSDL3_Shutdown();
      ImGui::DestroyContext();
      imgui_ = false;
    }
    if (menu_pad_owned_ && menu_pad_)
      SDL_CloseGamepad(menu_pad_);
    menu_pad_ = nullptr;
    menu_pad_owned_ = false;
    if (mask_)
      SDL_DestroyTexture(mask_);
    mask_ = nullptr;
    if (hd_)
      SDL_DestroyTexture(hd_);
    hd_ = nullptr;
    hd_w_ = hd_h_ = 0;
    if (texture)
      SDL_DestroyTexture(texture);
    if (renderer)
      SDL_DestroyRenderer(renderer);
    if (window)
      SDL_DestroyWindow(window);
    texture = nullptr;
    renderer = nullptr;
    window = nullptr;
#if defined(_WIN32)
    if (timer_period_) {
      timeEndPeriod(1);
      timer_period_ = false;
    }
#endif
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }

  bool fullscreen_ = false;
};

struct SdlAudio final : IAudioBackend {
  SDL_AudioStream* stream = nullptr;
  int rate_ = 48000;
  bool ym_mute_ = false;
  bool mono_ = false;
  bool paused_ = false;
  float ost_gain_ = 1.f;
  float output_gain_ = 1.f;
  std::vector<float> scratch_;

  bool init(int sample_rate, int channels) override {
    rate_ = sample_rate;
    SDL_AudioSpec spec{};
    spec.freq = sample_rate;
    spec.format = SDL_AUDIO_F32;
    spec.channels = channels;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream)
      return false;
    SDL_ResumeAudioStreamDevice(stream);
    return true;
  }

  void queue(std::span<const float> interleaved) override {
    if (!stream || interleaved.empty())
      return;
    // One emulated frame = 792 stereo floats @ 48 kHz ≈ 16.5 ms.
    // Clearing the stream on backlog makes an audible click/crackle; drop the
    // newest frame instead and keep whatever is already queued contiguous.
    // Pad a frame of silence when the queue is empty so a vsync hitch does
    // not underrun into noise.
    const int frame_bytes = int(interleaved.size() * sizeof(float));
    const int queued = SDL_GetAudioStreamQueued(stream);
    if (queued > frame_bytes * 6)
      return;
    if (queued < frame_bytes / 4) {
      scratch_.assign(interleaved.size(), 0.f);
      SDL_PutAudioStreamData(stream, scratch_.data(), frame_bytes);
    }
    if (ym_mute_) {
      // OST path: host may inject replacement elsewhere; queue silence for YM mix.
      scratch_.assign(interleaved.size(), 0.f);
      SDL_PutAudioStreamData(stream, scratch_.data(), frame_bytes);
      return;
    }
    if (mono_ && interleaved.size() >= 2) {
      scratch_.resize(interleaved.size());
      for (size_t i = 0; i + 1 < interleaved.size(); i += 2) {
        const float m = 0.5f * (interleaved[i] + interleaved[i + 1]);
        scratch_[i] = m;
        scratch_[i + 1] = m;
      }
      SDL_PutAudioStreamData(stream, scratch_.data(), frame_bytes);
      return;
    }
    SDL_PutAudioStreamData(stream, interleaved.data(), frame_bytes);
  }

  int sample_rate() const override { return rate_; }
  void set_ym_mute(bool mute) override { ym_mute_ = mute; }
  void set_ost_gain(float gain) override { ost_gain_ = gain; }
  void set_mono(bool mono) override { mono_ = mono; }
  void set_output_gain(float gain) override {
    if (gain < 0.f)
      gain = 0.f;
    if (gain > 1.f)
      gain = 1.f;
    if (!stream || gain == output_gain_)
      return;
    output_gain_ = gain;
    SDL_SetAudioStreamGain(stream, gain);
  }
  void set_paused(bool pause) override {
    if (!stream || pause == paused_)
      return;
    paused_ = pause;
    if (pause)
      SDL_PauseAudioStreamDevice(stream);
    else
      SDL_ResumeAudioStreamDevice(stream);
  }
  bool load_ost_track(std::string_view) override {
    // Phase 4: decode ogg/flac into a ring buffer and mix with ost_gain_.
    return false;
  }

  void shutdown() override {
    if (stream) {
      SDL_DestroyAudioStream(stream);
      stream = nullptr;
    }
  }
};

struct SdlInput final : IInputBackend {
  explicit SdlInput(HostOptions* options) : options_(options) {}

  SDL_Gamepad* pad = nullptr;
  HostOptions* options_ = nullptr;

  ~SdlInput() override {
    if (pad)
      SDL_CloseGamepad(pad);
  }

  void ensure_pad() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    int index = options_ && options_->gamepad > 0 ? options_->gamepad : 0;
    if (count <= 0)
      index = -1;
    else if (index >= count)
      index = 0;
    const SDL_JoystickID want = index >= 0 ? ids[index] : 0;
    SDL_free(ids);
    if (index < 0) {
      if (pad) {
        SDL_CloseGamepad(pad);
        pad = nullptr;
      }
      return;
    }
    if (pad && SDL_GamepadConnected(pad) && SDL_GetGamepadID(pad) == want)
      return;
    if (pad) {
      SDL_CloseGamepad(pad);
      pad = nullptr;
    }
    pad = SDL_OpenGamepad(want);
  }

  void poll(InputState& out) override {
    out.control0 = 0xFF;
    out.control1 = 0xFF;
    out.dipsw = 0xFF;
    if (options_ && options_->stage_select)
      out.dipsw = u8(out.dipsw & ~0x04u);
    out.coin = 0xFF;
    const bool* keys = SDL_GetKeyboardState(nullptr);
    auto clear = [&](u8& port, u8 bit, SDL_Scancode sc) {
      if (keys[sc])
        port = u8(port & ~bit);
    };
    auto press = [](u8& port, u8 bit) { port = u8(port & ~bit); };
    auto fire = [&](int action) {
      switch (action) {
      case 0: press(out.control0, 0x08); break;
      case 1: press(out.control0, 0x04); break;
      case 2: press(out.control0, 0x02); break;
      case 3: press(out.control0, 0x01); break;
      case 4: press(out.control0, 0x10); break;
      case 5: press(out.control0, 0x20); break;
      case 6: press(out.control0, 0x80); break;
      case 7: press(out.coin, 0x10); break;
      default: break;
      }
    };
    if (options_) {
      for (int action = 0; action < kBindCount; ++action) {
        const int sc = options_->key_bind[action];
        if (sc > 0 && sc < SDL_SCANCODE_COUNT && keys[sc])
          fire(action);
      }
    }
    clear(out.coin, 0x08, SDL_SCANCODE_6);     // COIN2
    clear(out.coin, 0x40, SDL_SCANCODE_9);     // SERVICE1
    clear(out.dipsw, 0x01, SDL_SCANCODE_F2);   // service DIP / test

    ensure_pad();
    if (!pad)
      return;
    auto held = [&](SDL_GamepadButton b) { return SDL_GetGamepadButton(pad, b); };
    constexpr int kDead = 12000;
    const int lx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    const int ly = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    if (lx > kDead)
      press(out.control0, 0x01);
    if (lx < -kDead)
      press(out.control0, 0x02);
    if (ly > kDead)
      press(out.control0, 0x04);
    if (ly < -kDead)
      press(out.control0, 0x08);
    if (!options_)
      return;
    for (int action = 0; action < kBindCount; ++action) {
      const int pad_id = options_->pad_bind[action];
      if (pad_id >= 0 && pad_id < SDL_GAMEPAD_BUTTON_COUNT && held(SDL_GamepadButton(pad_id)))
        fire(action);
      else if (pad_id == kPadTriggerLeft &&
               SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kDead)
        fire(action);
      else if (pad_id == kPadTriggerRight &&
               SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > kDead)
        fire(action);
    }
  }
};

} // namespace

std::unique_ptr<IPresentBackend> make_sdl_present(HostOptions* options) {
  return std::make_unique<SdlPresent>(options);
}
std::unique_ptr<IAudioBackend> make_sdl_audio() { return std::make_unique<SdlAudio>(); }
std::unique_ptr<IInputBackend> make_sdl_input(HostOptions* options) {
  return std::make_unique<SdlInput>(options);
}

} // namespace sh

#endif
