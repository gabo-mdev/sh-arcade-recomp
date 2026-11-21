#include "host/options_file.hpp"
#include "host/ui_locale.hpp"

#include <exception>
#include <fstream>
#include <string>

namespace sh {
namespace {

bool parse_bool(std::string value) {
  for (char& c : value)
    if (c >= 'A' && c <= 'Z')
      c = char(c - 'A' + 'a');
  return value == "1" || value == "true" || value == "on" || value == "yes";
}

int parse_int(const std::string& value, int fallback) {
  try {
    return std::stoi(value);
  } catch (const std::exception&) {
    return fallback;
  }
}

int clamp_int(int value, int lo, int hi) {
  if (value < lo)
    return lo;
  if (value > hi)
    return hi;
  return value;
}

void apply_line(HostOptions& options, const std::string& key, const std::string& value, bool& saw_filter) {
  const bool on = parse_bool(value);
  if (key == "stage_select")
    options.stage_select = on;
  else if (key == "skip_checksum")
    options.skip_checksum = on;
  else if (key == "invincible")
    options.invincible = on;
  else if (key == "untouchable")
    options.untouchable = on;
  else if (key == "uncap_fps")
    options.uncap_fps = on;
  else if (key == "debug_menu")
    options.debug_menu = on;
  else if (key == "mono_audio")
    options.mono_audio = on;
  else if (key == "fullscreen")
    options.fullscreen = on;
  else if (key == "stretch")
    options.stretch = on;
  else if (key == "linear_filter") {
    if (!saw_filter && on)
      options.image_filter = 1;
  } else if (key == "image_filter") {
    options.image_filter = clamp_int(parse_int(value, options.image_filter), 0, kImageFilterCount - 1);
    saw_filter = true;
  } else if (key == "language") {
    const int parsed = parse_int(value, options.language);
    if (value == "en" || value == "english")
      options.language = 0;
    else if (value == "es" || value == "spanish" || value == "espanol" || value == "español")
      options.language = 1;
    else
      options.language = clamp_int(parsed, 0, kUiLanguageCount - 1);
  } else if (key == "vsync")
    options.vsync = on;
  else if (key == "scale")
    options.scale = clamp_int(parse_int(value, options.scale), 1, 6);
  else if (key == "volume")
    options.volume = clamp_int(parse_int(value, options.volume), 0, 100);
  else if (key == "gamepad")
    options.gamepad = clamp_int(parse_int(value, options.gamepad), 0, 15);
  else if (key.size() == 4 && key[0] == 'k' && key[1] == 'e' && key[2] == 'y' && key[3] >= '0' &&
           key[3] <= '7')
    options.key_bind[key[3] - '0'] = clamp_int(parse_int(value, options.key_bind[key[3] - '0']), 0, 511);
  else if (key.size() == 4 && key[0] == 'p' && key[1] == 'a' && key[2] == 'd' && key[3] >= '0' &&
           key[3] <= '7')
    options.pad_bind[key[3] - '0'] = clamp_int(parse_int(value, options.pad_bind[key[3] - '0']), -1, 101);
}

} // namespace

HostOptions load_options(const std::filesystem::path& path) {
  HostOptions options;
  std::ifstream in(path);
  if (!in) {
    // First launch (or deleted file): write defaults so users can edit them.
    save_options(path, options);
    return options;
  }
  std::string line;
  bool saw_filter = false;
  while (std::getline(in, line)) {
    const auto hash = line.find('#');
    if (hash != std::string::npos)
      line.resize(hash);
    const auto eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    std::string key = line.substr(0, eq);
    std::string value = line.substr(eq + 1);
    const auto trim = [](std::string& s) {
      while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\t'))
        s.pop_back();
      size_t i = 0;
      while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
      s.erase(0, i);
    };
    trim(key);
    trim(value);
    apply_line(options, key, value, saw_filter);
  }
  options.dirty = false;
  return options;
}

bool save_options(const std::filesystem::path& path, const HostOptions& options) {
  std::ofstream out(path, std::ios::trunc);
  if (!out)
    return false;
  out << "# Splatterhouse options. Edited by the Escape menu and the F1 menu.\n";
  out << "stage_select=" << (options.stage_select ? 1 : 0) << "\n";
  out << "skip_checksum=" << (options.skip_checksum ? 1 : 0) << "\n";
  out << "invincible=" << (options.invincible ? 1 : 0) << "\n";
  out << "untouchable=" << (options.untouchable ? 1 : 0) << "\n";
  out << "uncap_fps=" << (options.uncap_fps ? 1 : 0) << "\n";
  out << "debug_menu=" << (options.debug_menu ? 1 : 0) << "\n";
  out << "mono_audio=" << (options.mono_audio ? 1 : 0) << "\n";
  out << "fullscreen=" << (options.fullscreen ? 1 : 0) << "\n";
  out << "stretch=" << (options.stretch ? 1 : 0) << "\n";
  out << "image_filter=" << options.image_filter << "\n";
  out << "language=" << clamp_int(options.language, 0, kUiLanguageCount - 1) << "\n";
  out << "vsync=" << (options.vsync ? 1 : 0) << "\n";
  out << "scale=" << options.scale << "\n";
  out << "volume=" << options.volume << "\n";
  out << "gamepad=" << options.gamepad << "\n";
  for (int i = 0; i < kBindCount; ++i) {
    out << "key" << i << "=" << options.key_bind[i] << "\n";
    out << "pad" << i << "=" << options.pad_bind[i] << "\n";
  }
  return bool(out);
}

} // namespace sh
