#include "host/play_menu.hpp"
#include "host/ui_locale.hpp"

#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>

#include <SDL3/SDL.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(SH_HAS_FONTCONFIG)
#include <fontconfig/fontconfig.h>
#endif

namespace sh {
namespace {

// RmlUi's SDL renderer backend clears the frame and loads images through
// SDL_image. This copy draws into the existing SDL renderer and only builds
// the font textures the menu needs.
class RenderInterface_SDL : public Rml::RenderInterface {
public:
  explicit RenderInterface_SDL(SDL_Renderer* renderer) : renderer_(renderer) {
    blend_ = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
  }

  void Prepare() {
    SDL_SetRenderDrawBlendMode(renderer_, blend_);
  }

  Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                              Rml::Span<const int> indices) override {
    auto* geometry = new Geometry;
    geometry->vertices.assign(vertices.begin(), vertices.end());
    geometry->indices.assign(indices.begin(), indices.end());
    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
  }

  void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override {
    delete reinterpret_cast<Geometry*>(geometry);
  }

  void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation,
                      Rml::TextureHandle texture) override {
    auto* geometry = reinterpret_cast<Geometry*>(handle);
    if (!geometry || geometry->vertices.empty() || geometry->indices.empty())
      return;
    std::vector<SDL_Vertex> sdl(geometry->vertices.size());
    for (size_t i = 0; i < geometry->vertices.size(); ++i) {
      const Rml::Vertex& v = geometry->vertices[i];
      sdl[i].position = {v.position.x + translation.x, v.position.y + translation.y};
      sdl[i].tex_coord = {v.tex_coord.x, v.tex_coord.y};
      sdl[i].color = {v.colour.red / 255.f, v.colour.green / 255.f, v.colour.blue / 255.f,
                      v.colour.alpha / 255.f};
    }
    SDL_RenderGeometry(renderer_, reinterpret_cast<SDL_Texture*>(texture), sdl.data(),
                       int(sdl.size()), geometry->indices.data(), int(geometry->indices.size()));
  }

  Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override { return 0; }

  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                     Rml::Vector2i dimensions) override {
    if (!source.data() || dimensions.x <= 0 || dimensions.y <= 0)
      return 0;
    SDL_Surface* surface = SDL_CreateSurfaceFrom(dimensions.x, dimensions.y, SDL_PIXELFORMAT_RGBA32,
                                                 const_cast<Rml::byte*>(source.data()), dimensions.x * 4);
    if (!surface)
      return 0;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
    if (texture)
      SDL_SetTextureBlendMode(texture, blend_);
    return reinterpret_cast<Rml::TextureHandle>(texture);
  }

  void ReleaseTexture(Rml::TextureHandle texture) override {
    if (texture)
      SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(texture));
  }

  void EnableScissorRegion(bool enable) override {
    scissor_on_ = enable;
    SDL_SetRenderClipRect(renderer_, enable ? &scissor_ : nullptr);
  }

  void SetScissorRegion(Rml::Rectanglei region) override {
    scissor_.x = region.Left();
    scissor_.y = region.Top();
    scissor_.w = region.Width();
    scissor_.h = region.Height();
    if (scissor_on_)
      SDL_SetRenderClipRect(renderer_, &scissor_);
  }

private:
  struct Geometry {
    std::vector<Rml::Vertex> vertices;
    std::vector<int> indices;
  };

  SDL_Renderer* renderer_ = nullptr;
  SDL_BlendMode blend_{};
  SDL_Rect scissor_{};
  bool scissor_on_ = false;
};

const char* kDocument = R"rml(
<rml>
<head>
<title>Options</title>
<style>
body {
  width: 100%;
  height: 100%;
  font-family: UiFont;
  font-size: 18px;
  color: #f4efe6;
  background: #0000;
}
button, h1, p {
  font-family: UiFont;
  color: #f4efe6;
}
#panel {
  position: absolute;
  top: 0px;
  right: 0px;
  width: 520px;
  height: 100%;
  padding: 24px 28px;
  box-sizing: border-box;
  background-color: #140c10ee;
  border-left: 3px #c4a574;
  overflow-y: scroll;
}
scrollbarvertical {
  width: 14px;
  background-color: #1c1216;
  border-left: 1px #3a2a30;
}
scrollbarhorizontal {
  height: 0px;
  display: none;
}
slidertrack {
  background-color: #1c1216;
}
sliderbar {
  background-color: #e0b15a;
  border: 0px;
  margin: 2px;
}
sliderarrowdec, sliderarrowinc {
  display: none;
  width: 0px;
  height: 0px;
}
h1, .lead, .tabs, .hint {
  display: block;
  width: 440px;
}
h1 {
  margin: 0px 0px 6px 0px;
  font-size: 30px;
  color: #f3e6c8;
}
.lead {
  margin: 0px 0px 16px 0px;
  color: #b5a398;
  font-size: 15px;
}
.tabs {
  display: flex;
  width: 440px;
  margin: 0px 0px 18px 0px;
  padding: 0px;
  border-bottom: 2px #c4a574;
}
.bump {
  width: 36px;
  margin: 0px 0px 8px 0px;
  color: #e0b15a;
  font-size: 14px;
  text-align: center;
}
.tab {
  display: inline-block;
  margin: 0px 6px -2px 0px;
  padding: 10px 22px 12px 22px;
  color: #f4efe6;
  background-color: #4a3040;
  border-top: 2px #c4a574;
  border-left: 2px #c4a574;
  border-right: 2px #c4a574;
  border-bottom: 2px #140c10;
  border-top-left-radius: 10px;
  border-top-right-radius: 10px;
}
.hit {
  display: block;
  width: 440px;
  margin: 0px 0px 8px 0px;
  padding: 10px 12px;
  background-color: #22161c;
  color: #f4efe6;
  border: 1px #3a2a30;
  font-size: 18px;
  box-sizing: border-box;
}
.pair {
  display: flex;
  width: 440px;
  margin: 0px 0px 8px 0px;
  padding: 0px;
  background-color: #22161c;
  border: 1px #3a2a30;
  box-sizing: border-box;
  tab-index: auto;
}
.pair.hit {
  display: flex;
  width: 440px;
  margin: 0px 0px 8px 0px;
  padding: 0px;
}
.arrow {
  display: block;
  width: 56px;
  margin: 0px;
  padding: 10px 0px;
  background-color: #3a2832;
  color: #f4efe6;
  border-width: 0px;
  font-size: 20px;
  text-align: center;
  tab-index: none;
}
.slot {
  display: block;
  width: 292px;
  margin: 0px;
  padding: 8px 6px;
  background-color: #3a2832;
  color: #f4efe6;
  border-width: 0px;
  font-size: 16px;
  text-align: center;
  box-sizing: border-box;
  tab-index: none;
}
.name {
  width: 148px;
  padding: 8px 10px;
  color: #f4efe6;
  font-size: 16px;
  box-sizing: border-box;
}
.value {
  flex: 1;
  padding: 10px 8px;
  color: #f4efe6;
  font-size: 16px;
  text-align: center;
}
.tab.hit {
  display: block;
  flex: 1;
  width: auto;
  margin: 0px 4px -2px 0px;
  padding: 8px 2px 10px 2px;
  font-size: 15px;
  text-align: center;
  white-space: nowrap;
  background-color: #4a3040;
  border-top: 2px #c4a574;
  border-left: 2px #c4a574;
  border-right: 2px #c4a574;
  border-bottom: 2px #140c10;
  border-top-left-radius: 10px;
  border-top-right-radius: 10px;
}
.hit:focus {
  border-color: #e0b15a;
}
#tab0.on, #tab1.on, #tab2.on, #tab3.on,
#tab0.on:focus, #tab1.on:focus, #tab2.on:focus, #tab3.on:focus {
  background-color: #e0b15a;
  border-top: 2px #fff1cc;
  border-left: 2px #fff1cc;
  border-right: 2px #fff1cc;
  border-bottom: 2px #e0b15a;
  color: #1a100c;
}
.apply {
  margin-top: 12px;
  background-color: #8f1d24;
}
#apply.idle, #apply.idle:focus {
  background-color: #3a2426;
  color: #776862;
  border-color: #3a2a30;
}
.hint {
  margin-top: 16px;
  color: #b5a398;
  font-size: 15px;
}
</style>
</head>
<body data-model="opt">
<div id="panel">
  <h1>{{ title }}</h1>
  <p class="lead" data-if="lead != ''">{{ lead }}</p>
  <div class="tabs">
    <div class="bump" data-if="pad_on == 1">{{ shoulder_l }}</div>
    <button id="tab0" class="tab hit" data-class-on="tab == 0" data-event-click="tab0">{{ tab_video }}</button>
    <button id="tab1" class="tab hit" data-class-on="tab == 1" data-event-click="tab1">{{ tab_audio }}</button>
    <button id="tab2" class="tab hit" data-class-on="tab == 2" data-event-click="tab2">{{ tab_controls }}</button>
    <button id="tab3" class="tab hit" data-class-on="tab == 3" data-event-click="tab3">{{ tab_system }}</button>
    <div class="bump" data-if="pad_on == 1">{{ shoulder_r }}</div>
  </div>

  <div data-if="tab == 0">
    <button id="full" class="hit" data-event-click="full">{{ full_label }}</button>
    <div id="scale_row" class="pair hit">
      <button id="scale_down" class="arrow" data-event-click="scale_down">&#8592;</button>
      <div class="value">{{ scale_label }}</div>
      <button id="scale_up" class="arrow" data-event-click="scale_up">&#8594;</button>
    </div>
    <button id="fit" class="hit" data-event-click="fit">{{ fit_label }}</button>
    <div id="filter_row" class="pair hit">
      <button id="filter_down" class="arrow" data-event-click="filter_down">&#8592;</button>
      <div class="value">{{ filter_label }}</div>
      <button id="filter_up" class="arrow" data-event-click="filter_up">&#8594;</button>
    </div>
    <button id="vsync" class="hit" data-event-click="vsync">{{ vsync_label }}</button>
  </div>
  <div data-if="tab == 1">
    <div id="vol_row" class="pair hit">
      <button id="vol_down" class="arrow" data-event-click="vol_down">&#8592;</button>
      <div class="value">{{ vol_label }}</div>
      <button id="vol_up" class="arrow" data-event-click="vol_up">&#8594;</button>
    </div>
    <button id="mono" class="hit" data-event-click="mono">{{ mono_label }}</button>
  </div>
  <div data-if="tab == 2">
    <div id="map_row" class="pair hit">
      <button id="map_down" class="arrow" data-event-click="map_down">&#8592;</button>
      <div class="value">{{ map_label }}</div>
      <button id="map_up" class="arrow" data-event-click="map_up">&#8594;</button>
    </div>
    <div id="pad_row" class="pair hit" data-if="map_kind == 1">
      <button id="pad_down" class="arrow" data-event-click="pad_down">&#8592;</button>
      <div class="value">{{ pad_label }}</div>
      <button id="pad_up" class="arrow" data-event-click="pad_up">&#8594;</button>
    </div>
    <div id="bind0" class="pair hit">
      <div class="name">{{ bind_up }}</div>
      <button class="slot" data-if="map_kind == 0" data-event-click="b0k">{{ k0 }}</button>
      <button class="slot" data-if="map_kind == 1" data-event-click="b0p">{{ p0 }}</button>
    </div>
    <div id="bind1" class="pair hit">
      <div class="name">{{ bind_down }}</div>
      <button class="slot" data-if="map_kind == 0" data-event-click="b1k">{{ k1 }}</button>
      <button class="slot" data-if="map_kind == 1" data-event-click="b1p">{{ p1 }}</button>
    </div>
    <div id="bind2" class="pair hit">
      <div class="name">{{ bind_left }}</div>
      <button class="slot" data-if="map_kind == 0" data-event-click="b2k">{{ k2 }}</button>
      <button class="slot" data-if="map_kind == 1" data-event-click="b2p">{{ p2 }}</button>
    </div>
    <div id="bind3" class="pair hit">
      <div class="name">{{ bind_right }}</div>
      <button class="slot" data-if="map_kind == 0" data-event-click="b3k">{{ k3 }}</button>
      <button class="slot" data-if="map_kind == 1" data-event-click="b3p">{{ p3 }}</button>
    </div>
    <div id="bind4" class="pair hit">
      <div class="name">{{ bind_attack }}</div>
      <button class="slot" data-if="map_kind == 0" data-event-click="b4k">{{ k4 }}</button>
      <button class="slot" data-if="map_kind == 1" data-event-click="b4p">{{ p4 }}</button>
    </div>
    <div id="bind5" class="pair hit">
      <div class="name">{{ bind_jump }}</div>
      <button class="slot" data-if="map_kind == 0" data-event-click="b5k">{{ k5 }}</button>
      <button class="slot" data-if="map_kind == 1" data-event-click="b5p">{{ p5 }}</button>
    </div>
    <div id="bind6" class="pair hit">
      <div class="name">{{ bind_start }}</div>
      <button class="slot" data-if="map_kind == 0" data-event-click="b6k">{{ k6 }}</button>
      <button class="slot" data-if="map_kind == 1" data-event-click="b6p">{{ p6 }}</button>
    </div>
    <div id="bind7" class="pair hit">
      <div class="name">{{ bind_coin }}</div>
      <button class="slot" data-if="map_kind == 0" data-event-click="b7k">{{ k7 }}</button>
      <button class="slot" data-if="map_kind == 1" data-event-click="b7p">{{ p7 }}</button>
    </div>
    <button id="binds_reset" class="hit" data-event-click="binds_reset">{{ binds_reset }}</button>
  </div>
  <div data-if="tab == 3">
    <div id="lang_row" class="pair hit">
      <button id="lang_down" class="arrow" data-event-click="lang_down">&#8592;</button>
      <div class="value">{{ lang_label }}</div>
      <button id="lang_up" class="arrow" data-event-click="lang_up">&#8594;</button>
    </div>
    <button id="uncap" class="hit" data-event-click="uncap">{{ uncap_label }}</button>
    <button id="reset" class="hit" data-event-click="reset">{{ reset_label }}</button>
    <button id="quit" class="hit" data-event-click="quit">{{ quit_label }}</button>
  </div>

  <button id="apply" class="hit apply" data-class-idle="pending == 0" data-event-click="apply">{{ apply_label }}</button>
  <p class="hint" data-if="hint_label != ''">{{ hint_label }}</p>
</div>
</body>
</rml>
)rml";

int clamp_int(int value, int lo, int hi) {
  if (value < lo)
    return lo;
  if (value > hi)
    return hi;
  return value;
}

// FreeType keeps a pointer at these bytes for the life of the menu.
std::vector<Rml::byte> ui_font_bytes;

bool load_ui_font_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  in.seekg(0, std::ios::end);
  const auto size = in.tellg();
  if (size <= 0)
    return false;
  in.seekg(0, std::ios::beg);
  ui_font_bytes.resize(static_cast<size_t>(size));
  in.read(reinterpret_cast<char*>(ui_font_bytes.data()), size);
  if (!in)
    return false;
  // The family name is ours, so the menu does not depend on the file's internal name.
  return Rml::LoadFontFace(ui_font_bytes, "UiFont", Rml::Style::FontStyle::Normal,
                           Rml::Style::FontWeight::Normal, true);
}

bool load_ui_font() {
#if defined(_WIN32)
  char windows[MAX_PATH] = "C:\\Windows";
  GetWindowsDirectoryA(windows, MAX_PATH);
  const char* names[] = {"segoeui.ttf", "arial.ttf", "tahoma.ttf"};
  for (const char* name : names) {
    if (load_ui_font_file(std::string(windows) + "\\Fonts\\" + name))
      return true;
  }
#else
#if defined(SH_HAS_FONTCONFIG)
  // Distro-agnostic: resolve the configured sans-serif (DejaVu, Noto, …).
  if (FcInit()) {
    FcPattern* pattern = FcPatternCreate();
    if (pattern) {
      FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>("sans-serif"));
      FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
      FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_REGULAR);
      // Prefer scalable outline fonts FreeType/RmlUi can load.
      FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
      FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
      FcDefaultSubstitute(pattern);
      FcResult result = FcResultNoMatch;
      FcPattern* match = FcFontMatch(nullptr, pattern, &result);
      if (match) {
        FcChar8* file = nullptr;
        if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file) {
          const char* path = reinterpret_cast<const char*>(file);
          if (load_ui_font_file(path)) {
            FcPatternDestroy(match);
            FcPatternDestroy(pattern);
            return true;
          }
        }
        FcPatternDestroy(match);
      }
      FcPatternDestroy(pattern);
    }
  }
#endif
  // Last resort if fontconfig is absent or the match failed to load.
  static const char* kPaths[] = {
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
      "/usr/share/fonts/noto/NotoSans-Regular.ttf",
      "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
      "/usr/local/share/fonts/DejaVuSans.ttf",
      "/mnt/c/Windows/Fonts/segoeui.ttf",
      "/mnt/c/Windows/Fonts/arial.ttf",
  };
  for (const char* path : kPaths) {
    if (load_ui_font_file(path))
      return true;
  }
#endif
  return false;
}

} // namespace

const char* filter_name(int language, int filter) {
  const MenuLocale& loc = menu_locale(language);
  if (filter < 0 || filter >= kImageFilterCount)
    filter = 0;
  return loc.filters[filter];
}

Rml::String key_name(int language, int scancode) {
  const MenuLocale& loc = menu_locale(language);
  if (scancode <= 0)
    return loc.none_key;
  const char* name = SDL_GetScancodeName(SDL_Scancode(scancode));
  if (!name || !name[0])
    return loc.key_fallback;
  return name;
}

void shoulder_names(SDL_GamepadType type, const char*& left, const char*& right) {
  switch (type) {
  case SDL_GAMEPAD_TYPE_PS3:
  case SDL_GAMEPAD_TYPE_PS4:
  case SDL_GAMEPAD_TYPE_PS5:
    left = "L1";
    right = "R1";
    break;
  case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
  case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
  case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
  case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
    left = "L";
    right = "R";
    break;
  default:
    left = "LB";
    right = "RB";
    break;
  }
}

Rml::String pad_control_name(int language, int id) {
  const MenuLocale& loc = menu_locale(language);
  switch (id) {
  case -1: return loc.none_pad;
  case 0: return "A";
  case 1: return "B";
  case 2: return "X";
  case 3: return "Y";
  case 4: return "Select";
  case 6: return "Start";
  case 7: return "L3";
  case 8: return "R3";
  case 9: return "LB";
  case 10: return "RB";
  case 11: return loc.dpad_up;
  case 12: return loc.dpad_down;
  case 13: return loc.dpad_left;
  case 14: return loc.dpad_right;
  case kPadTriggerLeft: return "LT";
  case kPadTriggerRight: return "RT";
  default: return loc.button_fallback;
  }
}

constexpr int kMenuW = 1280;
constexpr int kMenuH = 720;

struct PlayMenu::Impl {
  struct Draft {
    int tab = 0;
    bool fullscreen = false;
    bool stretch = false;
    bool vsync = true;
    bool uncap = false;
    bool mono = false;
    int scale = 3;
    int volume = 100;
    int gamepad = 0;
    int filter = 0;
    int language = 0;
    int key[kBindCount] = {82, 81, 80, 79, 29, 27, 30, 34};
    int pad[kBindCount] = {11, 12, 13, 14, 2, 0, 6, 4};
    Rml::String title;
    Rml::String lead;
    Rml::String tab_video;
    Rml::String tab_audio;
    Rml::String tab_controls;
    Rml::String tab_system;
    Rml::String apply_label;
    Rml::String reset_label;
    Rml::String quit_label;
    Rml::String binds_reset;
    Rml::String bind_up;
    Rml::String bind_down;
    Rml::String bind_left;
    Rml::String bind_right;
    Rml::String bind_attack;
    Rml::String bind_jump;
    Rml::String bind_start;
    Rml::String bind_coin;
    Rml::String full_label;
    Rml::String scale_label;
    Rml::String fit_label;
    Rml::String filter_label;
    Rml::String vsync_label;
    Rml::String uncap_label;
    Rml::String vol_label;
    Rml::String mono_label;
    Rml::String lang_label;
    Rml::String pad_label;
    Rml::String hint_label;
    Rml::String key_label[kBindCount];
    Rml::String pad_side[kBindCount];
  };

  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  std::unique_ptr<RenderInterface_SDL> render;
  std::unique_ptr<SystemInterface_SDL> system;
  Rml::Context* context = nullptr;
  Rml::ElementDocument* document = nullptr;
  SDL_Texture* canvas = nullptr;
  Rml::DataModelHandle model;
  Draft draft;
  Draft committed;
  HostOptions* bound = nullptr;
  bool ready = false;
  bool open = false;
  bool quit = false;
  bool reset = false;
  bool applied = false;
  bool refocus = false;
  int pad_count = 0;
  int pad_on = 0;
  int pending = 0;
  Rml::String shoulder_l;
  Rml::String shoulder_r;
  int map_kind = 0;
  Rml::String map_label;
  int capture_action = -1;
  int capture_slot = 0;
  Rml::String notice;

  void refresh_labels() {
    draft.language =
        (draft.language % kUiLanguageCount + kUiLanguageCount) % kUiLanguageCount;
    const MenuLocale& loc = menu_locale(draft.language);
    draft.title = loc.title;
    draft.lead = loc.lead;
    draft.tab_video = loc.tab_video;
    draft.tab_audio = loc.tab_audio;
    draft.tab_controls = loc.tab_controls;
    draft.tab_system = loc.tab_system;
    draft.apply_label = loc.apply;
    draft.reset_label = loc.reset;
    draft.quit_label = loc.quit;
    draft.binds_reset = loc.binds_reset;
    draft.bind_up = loc.bind_up;
    draft.bind_down = loc.bind_down;
    draft.bind_left = loc.bind_left;
    draft.bind_right = loc.bind_right;
    draft.bind_attack = loc.bind_attack;
    draft.bind_jump = loc.bind_jump;
    draft.bind_start = loc.bind_start;
    draft.bind_coin = loc.bind_coin;
    draft.full_label = draft.fullscreen ? loc.fullscreen_on : loc.fullscreen_off;
    draft.scale_label = std::string(loc.scale_prefix) + std::to_string(draft.scale) + "x";
    draft.fit_label = draft.stretch ? loc.fit_stretch : loc.fit_integer;
    draft.filter = (draft.filter % kImageFilterCount + kImageFilterCount) % kImageFilterCount;
    draft.filter_label =
        std::string(loc.filter_prefix) + filter_name(draft.language, draft.filter);
    draft.vsync_label = draft.vsync ? loc.vsync_on : loc.vsync_off;
    draft.uncap_label = draft.uncap ? loc.uncap_on : loc.uncap_off;
    draft.vol_label =
        std::string(loc.volume_prefix) + std::to_string(draft.volume) + "%";
    draft.mono_label = draft.mono ? loc.mono_on : loc.mono_off;
    draft.lang_label =
        std::string(loc.language_prefix) + ui_language_name(draft.language);
    map_kind = map_kind == 1 ? 1 : 0;
    map_label = map_kind == 0 ? loc.map_keyboard : loc.map_gamepad;
    for (int i = 0; i < kBindCount; ++i) {
      const bool listening = capture_action == i;
      draft.key_label[i] =
          listening && capture_slot == 0 ? "..." : key_name(draft.language, draft.key[i]);
      draft.pad_side[i] = listening && capture_slot == 1
                              ? "..."
                              : pad_control_name(draft.language, draft.pad[i]);
    }
    if (capture_action >= 0 && capture_slot == 0)
      draft.hint_label = loc.hint_capture_key;
    else if (capture_action >= 0)
      draft.hint_label = loc.hint_capture_pad;
    else if (!notice.empty())
      draft.hint_label = notice;
    else
      draft.hint_label = loc.hint_default;
    pending = same_settings(draft, committed) ? 0 : 1;
    if (model)
      model.DirtyAllVariables();
  }

  void refresh_pad() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    const Rml::String prev_l = shoulder_l;
    const Rml::String prev_r = shoulder_r;
    pad_count = count;
    Rml::String label;
    int on = 0;
    const MenuLocale& loc = menu_locale(draft.language);
    if (count <= 0) {
      draft.gamepad = 0;
      label = loc.pad_none;
    } else {
      draft.gamepad = clamp_int(draft.gamepad, 0, count - 1);
      const char* name = SDL_GetGamepadNameForID(ids[draft.gamepad]);
      label = std::string(loc.pad_prefix) + (name ? name : "Gamepad");
      if (count > 1) {
        label += " (";
        label += std::to_string(draft.gamepad + 1);
        label += "/";
        label += std::to_string(count);
        label += ")";
      }
      on = 1;
      const char* left = "LB";
      const char* right = "RB";
      shoulder_names(SDL_GetGamepadTypeForID(ids[draft.gamepad]), left, right);
      shoulder_l = left;
      shoulder_r = right;
    }
    SDL_free(ids);
    const bool changed = label != draft.pad_label || on != pad_on || shoulder_l != prev_l || shoulder_r != prev_r;
    draft.pad_label = std::move(label);
    pad_on = on;
    if (model && changed) {
      model.DirtyVariable("pad_label");
      model.DirtyVariable("pad_on");
      model.DirtyVariable("shoulder_l");
      model.DirtyVariable("shoulder_r");
    }
  }

  void copy_from(const HostOptions& options) {
    draft.fullscreen = options.fullscreen;
    draft.stretch = options.stretch;
    draft.vsync = options.vsync;
    draft.uncap = options.uncap_fps;
    draft.mono = options.mono_audio;
    draft.scale = clamp_int(options.scale, 1, 6);
    draft.volume = clamp_int(options.volume, 0, 100);
    draft.gamepad = options.gamepad < 0 ? 0 : options.gamepad;
    draft.filter = clamp_int(options.image_filter, 0, kImageFilterCount - 1);
    draft.language = clamp_int(options.language, 0, kUiLanguageCount - 1);
    for (int i = 0; i < kBindCount; ++i) {
      draft.key[i] = options.key_bind[i];
      draft.pad[i] = options.pad_bind[i];
    }
    capture_action = -1;
    notice.clear();
    refresh_pad();
    committed = draft;
    refresh_labels();
  }

  bool same_settings(const Draft& a, const Draft& b) const {
    if (a.fullscreen != b.fullscreen || a.stretch != b.stretch || a.vsync != b.vsync || a.uncap != b.uncap ||
        a.mono != b.mono || a.scale != b.scale || a.volume != b.volume ||
        a.gamepad != b.gamepad || a.filter != b.filter || a.language != b.language)
      return false;
    for (int i = 0; i < kBindCount; ++i) {
      if (a.key[i] != b.key[i] || a.pad[i] != b.pad[i])
        return false;
    }
    return true;
  }

  void write_committed(HostOptions& dst) const {
    dst.fullscreen = committed.fullscreen;
    dst.stretch = committed.stretch;
    dst.image_filter = committed.filter;
    dst.vsync = committed.vsync;
    dst.uncap_fps = committed.uncap;
    dst.mono_audio = committed.mono;
    dst.scale = committed.scale;
    dst.volume = committed.volume;
    dst.gamepad = committed.gamepad;
    dst.language = committed.language;
    for (int i = 0; i < kBindCount; ++i) {
      dst.key_bind[i] = committed.key[i];
      dst.pad_bind[i] = committed.pad[i];
    }
    dst.dirty = true;
  }

  void commit() {
    if (same_settings(draft, committed))
      return;
    committed = draft;
    applied = true;
    pending = 0;
    // Write through immediately so closing the menu cannot race take_applied
    // and drop image_filter / other graphics settings.
    if (bound)
      write_committed(*bound);
    if (model)
      model.DirtyVariable("pending");
  }

  std::vector<Rml::Element*> hits() const {
    std::vector<Rml::Element*> out;
    if (!document)
      return out;
    std::vector<Rml::Element*> stack{document};
    while (!stack.empty()) {
      Rml::Element* element = stack.back();
      stack.pop_back();
      if (element->IsClassSet("hit") && element->IsVisible(true) && !element->IsClassSet("idle"))
        out.push_back(element);
      for (int i = element->GetNumChildren() - 1; i >= 0; --i)
        stack.push_back(element->GetChild(i));
    }
    return out;
  }

  void focus_move(int direction) {
    const std::vector<Rml::Element*> rows = hits();
    if (rows.empty() || !context)
      return;
    int index = -1;
    Rml::Element* current = context->GetFocusElement();
    while (current && !current->IsClassSet("hit"))
      current = current->GetParentNode();
    for (int i = 0; i < int(rows.size()); ++i) {
      if (rows[size_t(i)] == current)
        index = i;
    }
    const int count = int(rows.size());
    if (index < 0) {
      for (Rml::Element* element : rows) {
        if (!element->IsClassSet("tab")) {
          element->Focus();
          element->ScrollIntoView(true);
          return;
        }
      }
      rows.front()->Focus();
      return;
    }
    index = (index + direction) % count;
    if (index < 0)
      index += count;
    rows[size_t(index)]->Focus();
    rows[size_t(index)]->ScrollIntoView(direction > 0);
  }

  void focus_first_setting() {
    for (Rml::Element* element : hits()) {
      if (!element->IsClassSet("tab")) {
        element->Focus();
        return;
      }
    }
  }

  void click_id(const char* id) {
    if (!document)
      return;
    if (Rml::Element* element = document->GetElementById(id))
      element->Click();
  }

  void nudge(int direction) {
    if (!context)
      return;
    const char* down = nullptr;
    const char* up = nullptr;
    for (Rml::Element* element = context->GetFocusElement(); element; element = element->GetParentNode()) {
      const Rml::String id = element->GetId();
      if (id == "scale_row") {
        down = "scale_down";
        up = "scale_up";
      } else if (id == "vol_row") {
        down = "vol_down";
        up = "vol_up";
      } else if (id == "pad_row") {
        down = "pad_down";
        up = "pad_up";
      } else if (id == "filter_row") {
        down = "filter_down";
        up = "filter_up";
      } else if (id == "map_row") {
        down = "map_down";
        up = "map_up";
      } else if (id == "lang_row") {
        down = "lang_down";
        up = "lang_up";
      }
      if (down)
        break;
    }
    if (down)
      click_id(direction < 0 ? down : up);
  }

  void start_capture(int action, int slot) {
    if (action < 0 || action >= kBindCount)
      return;
    capture_action = action;
    capture_slot = slot == 0 ? 0 : 1;
    refresh_labels();
  }

  void stop_capture() {
    if (capture_action < 0)
      return;
    capture_action = -1;
    refresh_labels();
  }

  void accept_key(int scancode, bool clear) {
    if (capture_action < 0)
      return;
    if (clear) {
      if (capture_slot == 0)
        draft.key[capture_action] = 0;
      else
        draft.pad[capture_action] = -1;
      notice.clear();
      stop_capture();
      return;
    }
    if (capture_slot != 0)
      return;
    if (scancode == SDL_SCANCODE_F1 || scancode == SDL_SCANCODE_F11 || scancode == SDL_SCANCODE_F12 ||
        scancode == SDL_SCANCODE_ESCAPE)
      return;
    const int other = conflict_key(scancode);
    draft.key[capture_action] = scancode;
    if (other < 0) {
      notice.clear();
    } else {
      char buf[160];
      const MenuLocale& loc = menu_locale(draft.language);
      std::snprintf(buf, sizeof(buf), loc.conflict_fmt,
                    key_name(draft.language, scancode).c_str(), action_name(other));
      notice = buf;
    }
    stop_capture();
  }

  void accept_pad(int button) {
    if (capture_action < 0 || capture_slot != 1)
      return;
    if (button == SDL_GAMEPAD_BUTTON_LEFT_STICK || button == SDL_GAMEPAD_BUTTON_GUIDE)
      return;
    const int other = conflict_pad(button);
    draft.pad[capture_action] = button;
    if (other < 0) {
      notice.clear();
    } else {
      char buf[160];
      const MenuLocale& loc = menu_locale(draft.language);
      std::snprintf(buf, sizeof(buf), loc.conflict_fmt,
                    pad_control_name(draft.language, button).c_str(),
                    action_name(other));
      notice = buf;
    }
    stop_capture();
  }

  const char* action_name(int action) const {
    const MenuLocale& loc = menu_locale(draft.language);
    const char* names[] = {loc.bind_up,   loc.bind_down, loc.bind_left, loc.bind_right,
                           loc.bind_attack, loc.bind_jump, loc.bind_start, loc.bind_coin};
    if (action < 0 || action >= kBindCount)
      return loc.other_action;
    return names[action];
  }

  int conflict_key(int scancode) const {
    if (scancode <= 0)
      return -1;
    for (int i = 0; i < kBindCount; ++i) {
      if (i != capture_action && draft.key[i] == scancode)
        return i;
    }
    return -1;
  }

  int conflict_pad(int button) const {
    if (button < 0)
      return -1;
    for (int i = 0; i < kBindCount; ++i) {
      if (i != capture_action && draft.pad[i] == button)
        return i;
    }
    return -1;
  }
};

PlayMenu::PlayMenu() : impl_(std::make_unique<Impl>()) {}
PlayMenu::~PlayMenu() { shutdown(); }

bool PlayMenu::ready() const { return impl_->ready; }
bool PlayMenu::is_open() const { return impl_->open; }
int PlayMenu::menu_pad_index() const {
  if (!impl_->open)
    return -1;
  return impl_->draft.gamepad;
}

bool PlayMenu::init(SDL_Window* window, SDL_Renderer* renderer) {
  impl_->window = window;
  impl_->renderer = renderer;
  impl_->render = std::make_unique<RenderInterface_SDL>(renderer);
  impl_->canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, kMenuW, kMenuH);
  if (!impl_->canvas) {
    std::fprintf(stderr, "Play menu canvas failed: %s\n", SDL_GetError());
    return false;
  }
  SDL_SetTextureBlendMode(impl_->canvas, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(impl_->canvas, SDL_SCALEMODE_LINEAR);
  impl_->system = std::make_unique<SystemInterface_SDL>();
  impl_->system->SetWindow(window);
  Rml::SetRenderInterface(impl_->render.get());
  Rml::SetSystemInterface(impl_->system.get());
  if (!Rml::Initialise()) {
    std::fprintf(stderr, "RmlUi failed to initialise\n");
    return false;
  }
  if (!load_ui_font())
    std::fprintf(stderr, "Play menu font missing; text may not draw\n");

  impl_->context = Rml::CreateContext("play", Rml::Vector2i(kMenuW, kMenuH));
  if (!impl_->context) {
    std::fprintf(stderr, "RmlUi failed to create the play menu context\n");
    Rml::Shutdown();
    return false;
  }

  Rml::DataModelConstructor ctor = impl_->context->CreateDataModel("opt");
  if (!ctor) {
    std::fprintf(stderr, "RmlUi failed to create the play menu model\n");
    Rml::Shutdown();
    impl_->context = nullptr;
    return false;
  }
  ctor.Bind("tab", &impl_->draft.tab);
  ctor.Bind("title", &impl_->draft.title);
  ctor.Bind("lead", &impl_->draft.lead);
  ctor.Bind("tab_video", &impl_->draft.tab_video);
  ctor.Bind("tab_audio", &impl_->draft.tab_audio);
  ctor.Bind("tab_controls", &impl_->draft.tab_controls);
  ctor.Bind("tab_system", &impl_->draft.tab_system);
  ctor.Bind("apply_label", &impl_->draft.apply_label);
  ctor.Bind("reset_label", &impl_->draft.reset_label);
  ctor.Bind("quit_label", &impl_->draft.quit_label);
  ctor.Bind("binds_reset", &impl_->draft.binds_reset);
  ctor.Bind("bind_up", &impl_->draft.bind_up);
  ctor.Bind("bind_down", &impl_->draft.bind_down);
  ctor.Bind("bind_left", &impl_->draft.bind_left);
  ctor.Bind("bind_right", &impl_->draft.bind_right);
  ctor.Bind("bind_attack", &impl_->draft.bind_attack);
  ctor.Bind("bind_jump", &impl_->draft.bind_jump);
  ctor.Bind("bind_start", &impl_->draft.bind_start);
  ctor.Bind("bind_coin", &impl_->draft.bind_coin);
  ctor.Bind("full_label", &impl_->draft.full_label);
  ctor.Bind("scale_label", &impl_->draft.scale_label);
  ctor.Bind("fit_label", &impl_->draft.fit_label);
  ctor.Bind("filter_label", &impl_->draft.filter_label);
  ctor.Bind("vsync_label", &impl_->draft.vsync_label);
  ctor.Bind("uncap_label", &impl_->draft.uncap_label);
  ctor.Bind("vol_label", &impl_->draft.vol_label);
  ctor.Bind("mono_label", &impl_->draft.mono_label);
  ctor.Bind("lang_label", &impl_->draft.lang_label);
  ctor.Bind("pad_label", &impl_->draft.pad_label);
  ctor.Bind("pad_on", &impl_->pad_on);
  ctor.Bind("pending", &impl_->pending);
  ctor.Bind("shoulder_l", &impl_->shoulder_l);
  ctor.Bind("shoulder_r", &impl_->shoulder_r);
  ctor.Bind("map_kind", &impl_->map_kind);
  ctor.Bind("map_label", &impl_->map_label);
  ctor.Bind("hint_label", &impl_->draft.hint_label);
  static const char* key_names[] = {"k0", "k1", "k2", "k3", "k4", "k5", "k6", "k7"};
  static const char* pad_names[] = {"p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7"};
  static const char* key_events[] = {"b0k", "b1k", "b2k", "b3k", "b4k", "b5k", "b6k", "b7k"};
  static const char* pad_events[] = {"b0p", "b1p", "b2p", "b3p", "b4p", "b5p", "b6p", "b7p"};
  for (int i = 0; i < kBindCount; ++i) {
    ctor.Bind(key_names[i], &impl_->draft.key_label[i]);
    ctor.Bind(pad_names[i], &impl_->draft.pad_side[i]);
  }
  Impl* menu = impl_.get();
  auto on = [menu](auto fn) {
    return [menu, fn](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { fn(menu); };
  };
  ctor.BindEventCallback("tab0", on([](Impl* m) { m->draft.tab = 0; m->refresh_labels(); m->refocus = true; }));
  ctor.BindEventCallback("tab1", on([](Impl* m) { m->draft.tab = 1; m->refresh_labels(); m->refocus = true; }));
  ctor.BindEventCallback("tab2", on([](Impl* m) { m->draft.tab = 2; m->refresh_pad(); m->refresh_labels(); m->refocus = true; }));
  ctor.BindEventCallback("tab3", on([](Impl* m) { m->draft.tab = 3; m->refresh_labels(); m->refocus = true; }));
  ctor.BindEventCallback("full", on([](Impl* m) { m->draft.fullscreen = !m->draft.fullscreen; m->refresh_labels(); }));
  ctor.BindEventCallback("fit", on([](Impl* m) { m->draft.stretch = !m->draft.stretch; m->refresh_labels(); }));
  ctor.BindEventCallback("filter_down", on([](Impl* m) {
    m->draft.filter = (m->draft.filter + kImageFilterCount - 1) % kImageFilterCount;
    m->refresh_labels();
  }));
  ctor.BindEventCallback("filter_up", on([](Impl* m) {
    m->draft.filter = (m->draft.filter + 1) % kImageFilterCount;
    m->refresh_labels();
  }));
  ctor.BindEventCallback("vsync", on([](Impl* m) { m->draft.vsync = !m->draft.vsync; m->refresh_labels(); }));
  ctor.BindEventCallback("uncap", on([](Impl* m) { m->draft.uncap = !m->draft.uncap; m->refresh_labels(); }));
  ctor.BindEventCallback("mono", on([](Impl* m) { m->draft.mono = !m->draft.mono; m->refresh_labels(); }));
  ctor.BindEventCallback("lang_down", on([](Impl* m) {
    m->draft.language =
        (m->draft.language + kUiLanguageCount - 1) % kUiLanguageCount;
    m->refresh_pad();
    m->refresh_labels();
  }));
  ctor.BindEventCallback("lang_up", on([](Impl* m) {
    m->draft.language = (m->draft.language + 1) % kUiLanguageCount;
    m->refresh_pad();
    m->refresh_labels();
  }));
  ctor.BindEventCallback("scale_down", on([](Impl* m) {
    m->draft.scale = clamp_int(m->draft.scale - 1, 1, 6);
    m->refresh_labels();
  }));
  ctor.BindEventCallback("scale_up", on([](Impl* m) {
    m->draft.scale = clamp_int(m->draft.scale + 1, 1, 6);
    m->refresh_labels();
  }));
  ctor.BindEventCallback("vol_down", on([](Impl* m) {
    m->draft.volume = clamp_int(m->draft.volume - 5, 0, 100);
    m->refresh_labels();
  }));
  ctor.BindEventCallback("vol_up", on([](Impl* m) {
    m->draft.volume = clamp_int(m->draft.volume + 5, 0, 100);
    m->refresh_labels();
  }));
  ctor.BindEventCallback("pad_down", on([](Impl* m) {
    if (m->pad_count > 0)
      m->draft.gamepad = (m->draft.gamepad + m->pad_count - 1) % m->pad_count;
    m->refresh_pad();
  }));
  ctor.BindEventCallback("pad_up", on([](Impl* m) {
    if (m->pad_count > 0)
      m->draft.gamepad = (m->draft.gamepad + 1) % m->pad_count;
    m->refresh_pad();
  }));
  ctor.BindEventCallback("map_down", on([](Impl* m) {
    m->capture_action = -1;
    m->map_kind = m->map_kind == 0 ? 1 : 0;
    m->refresh_labels();
  }));
  ctor.BindEventCallback("map_up", on([](Impl* m) {
    m->capture_action = -1;
    m->map_kind = m->map_kind == 0 ? 1 : 0;
    m->refresh_labels();
  }));
  for (int i = 0; i < kBindCount; ++i) {
    ctor.BindEventCallback(key_events[i], on([i](Impl* m) { m->start_capture(i, 0); }));
    ctor.BindEventCallback(pad_events[i], on([i](Impl* m) { m->start_capture(i, 1); }));
  }
  ctor.BindEventCallback("binds_reset", on([](Impl* m) {
    for (int i = 0; i < kBindCount; ++i) {
      m->draft.key[i] = kDefaultKey[i];
      m->draft.pad[i] = kDefaultPad[i];
    }
    m->capture_action = -1;
    m->refresh_labels();
  }));
  ctor.BindEventCallback("apply", on([](Impl* m) { m->commit(); }));
  ctor.BindEventCallback("reset", on([](Impl* m) { m->reset = true; m->open = false; }));
  ctor.BindEventCallback("quit", on([](Impl* m) { m->quit = true; }));
  impl_->model = ctor.GetModelHandle();

  impl_->refresh_labels();
  impl_->document = impl_->context->LoadDocumentFromMemory(kDocument, "play.rml");
  if (!impl_->document) {
    std::fprintf(stderr, "RmlUi failed to load the play menu\n");
    Rml::Shutdown();
    impl_->context = nullptr;
    return false;
  }
  impl_->document->Hide();
  impl_->ready = true;
  return true;
}

void PlayMenu::shutdown() {
  if (!impl_)
    return;
  if (impl_->ready || impl_->context) {
    if (impl_->document)
      impl_->document->Close();
    impl_->document = nullptr;
    impl_->context = nullptr;
    Rml::Shutdown();
  }
  impl_->ready = false;
  impl_->open = false;
  if (impl_->canvas) {
    SDL_DestroyTexture(impl_->canvas);
    impl_->canvas = nullptr;
  }
  impl_->render.reset();
  impl_->system.reset();
}

void PlayMenu::show(HostOptions& current) {
  if (!impl_->ready)
    return;
  impl_->bound = &current;
  impl_->draft.tab = 0;
  impl_->copy_from(current);
  impl_->open = true;
  if (impl_->document)
    impl_->document->Show();
  if (impl_->context && impl_->renderer) {
    impl_->context->SetDimensions(Rml::Vector2i(kMenuW, kMenuH));
    impl_->context->SetDensityIndependentPixelRatio(1.f);
    impl_->context->Update();
  }
  move(0);
}

void PlayMenu::hide() {
  // Flush a pending Aplicar before tearing the binding down (Escape/F12 the
  // same frame as the click must not lose image_filter).
  if (impl_->applied && impl_->bound) {
    impl_->write_committed(*impl_->bound);
    impl_->applied = false;
  }
  impl_->capture_action = -1;
  impl_->open = false;
  impl_->bound = nullptr;
  if (impl_->document)
    impl_->document->Hide();
}

void PlayMenu::draw() {
  if (!impl_->ready || !impl_->open || !impl_->context || !impl_->renderer || !impl_->canvas)
    return;
  int out_w = 0;
  int out_h = 0;
  SDL_GetCurrentRenderOutputSize(impl_->renderer, &out_w, &out_h);
  if (out_w < 1 || out_h < 1)
    return;

  impl_->refresh_pad();
  impl_->context->SetDimensions(Rml::Vector2i(kMenuW, kMenuH));
  impl_->context->SetDensityIndependentPixelRatio(1.f);
  impl_->context->Update();
  if (impl_->refocus) {
    impl_->refocus = false;
    impl_->focus_first_setting();
  }

  SDL_SetRenderTarget(impl_->renderer, impl_->canvas);
  SDL_SetRenderDrawColor(impl_->renderer, 0, 0, 0, 0);
  SDL_RenderClear(impl_->renderer);
  impl_->render->Prepare();
  impl_->context->Render();
  SDL_SetRenderClipRect(impl_->renderer, nullptr);
  SDL_SetRenderTarget(impl_->renderer, nullptr);

  SDL_FRect dst{0.f, 0.f, float(out_w), float(out_h)};
  SDL_RenderTexture(impl_->renderer, impl_->canvas, nullptr, &dst);
}

void PlayMenu::process(SDL_Event& event) {
  if (!impl_->ready || !impl_->open || !impl_->context || impl_->capture_action >= 0)
    return;
  if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
      event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
      event.type == SDL_EVENT_WINDOW_RESIZED)
    return;
  if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
    const SDL_Keycode key = event.key.key;
    if (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT || key == SDLK_RIGHT ||
        key == SDLK_TAB || key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE ||
        key == SDLK_Q || key == SDLK_E || key == SDLK_ESCAPE || key == SDLK_F1 || key == SDLK_F12)
      return;
  }
  SDL_Event mapped = event;
  float cx = 0.f;
  float cy = 0.f;
  bool pointing = false;
  if (mapped.type == SDL_EVENT_MOUSE_MOTION) {
    cx = mapped.motion.x;
    cy = mapped.motion.y;
    pointing = true;
  } else if (mapped.type == SDL_EVENT_MOUSE_BUTTON_DOWN || mapped.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    cx = mapped.button.x;
    cy = mapped.button.y;
    pointing = true;
  } else if (mapped.type == SDL_EVENT_MOUSE_WHEEL) {
    cx = mapped.wheel.mouse_x;
    cy = mapped.wheel.mouse_y;
    pointing = true;
  }
  if (pointing && impl_->window) {
    int win_w = 1;
    int win_h = 1;
    SDL_GetWindowSize(impl_->window, &win_w, &win_h);
    if (win_w < 1)
      win_w = 1;
    if (win_h < 1)
      win_h = 1;
    cx = cx * float(kMenuW) / float(win_w);
    cy = cy * float(kMenuH) / float(win_h);
    if (mapped.type == SDL_EVENT_MOUSE_MOTION) {
      mapped.motion.x = cx;
      mapped.motion.y = cy;
    } else if (mapped.type == SDL_EVENT_MOUSE_WHEEL) {
      mapped.wheel.mouse_x = cx;
      mapped.wheel.mouse_y = cy;
    } else {
      mapped.button.x = cx;
      mapped.button.y = cy;
    }
    // Button and wheel events are tested against the last mouse position.
    if (mapped.type != SDL_EVENT_MOUSE_MOTION)
      impl_->context->ProcessMouseMove(int(cx), int(cy), 0);
  }
  RmlSDL::InputEventHandler(impl_->context, impl_->window, mapped);
  if (pointing && mapped.type == SDL_EVENT_MOUSE_BUTTON_DOWN && mapped.button.button == SDL_BUTTON_LEFT) {
    Rml::Element* element = impl_->context->GetElementAtPoint(Rml::Vector2f(cx, cy));
    while (element && !element->IsClassSet("hit"))
      element = element->GetParentNode();
    if (element) {
      element->Focus();
      element->ScrollIntoView(false);
    }
  }
}

void PlayMenu::move(int direction) {
  if (!impl_->open || impl_->capture_action >= 0)
    return;
  impl_->focus_move(direction);
}

void PlayMenu::horizontal(int direction) { 
  if (impl_->open && impl_->capture_action < 0)
    impl_->nudge(direction);
}

void PlayMenu::activate() {
  if (!impl_->open || !impl_->context || impl_->capture_action >= 0)
    return;
  Rml::Element* element = impl_->context->GetFocusElement();
  for (; element; element = element->GetParentNode()) {
    const Rml::String id = element->GetId();
    if (id.size() == 5 && id[0] == 'b' && id[1] == 'i' && id[2] == 'n' && id[3] == 'd' && id[4] >= '0' &&
        id[4] <= '7') {
      impl_->start_capture(id[4] - '0', impl_->map_kind);
      return;
    }
  }
  if (Rml::Element* focused = impl_->context->GetFocusElement()) {
    if (!focused->IsClassSet("idle"))
      focused->Click();
  }
}

void PlayMenu::cycle_tab(int direction) {
  if (!impl_->open || impl_->capture_action >= 0)
    return;
  impl_->draft.tab = (impl_->draft.tab + direction + 4) % 4;
  impl_->refresh_labels();
  impl_->refocus = true;
}

int PlayMenu::live_filter() const {
  if (!impl_->open)
    return -1;
  return impl_->draft.filter;
}

bool PlayMenu::capturing() const { return impl_->open && impl_->capture_action >= 0; }

bool PlayMenu::capture_is_pad() const { return impl_->capture_slot == 1; }

void PlayMenu::cancel_capture() { impl_->stop_capture(); }

void PlayMenu::take_key(int scancode, bool clear) { impl_->accept_key(scancode, clear); }

void PlayMenu::take_pad(int button) { impl_->accept_pad(button); }

bool PlayMenu::take_quit() {
  const bool quit = impl_->quit;
  impl_->quit = false;
  return quit;
}

bool PlayMenu::take_reset() {
  const bool reset = impl_->reset;
  impl_->reset = false;
  if (reset)
    hide();
  return reset;
}

bool PlayMenu::take_applied(HostOptions& dst) {
  if (!impl_->applied)
    return false;
  impl_->applied = false;
  impl_->write_committed(dst);
  return true;
}

} // namespace sh
