#include "host/backends.hpp"
#include "host/options_file.hpp"
#include "machine/machine.hpp"
#include "machine/rom_prepare.hpp"
#include "machine/romset.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

#if defined(_WIN32)
// WINDOWS subsystem: no console on Explorer double-click. If launched from an
// existing terminal, attach so printf still works for --headless / debugging.
static void attach_parent_console() {
  if (!AttachConsole(ATTACH_PARENT_PROCESS))
    return;
  FILE* stream = nullptr;
  freopen_s(&stream, "CONOUT$", "w", stdout);
  freopen_s(&stream, "CONOUT$", "w", stderr);
  freopen_s(&stream, "CONIN$", "r", stdin);
}

static void show_error(const char* message) {
  std::fprintf(stderr, "error: %s\n", message);
  MessageBoxA(nullptr, message, "Splatterhouse", MB_OK | MB_ICONERROR);
}

#else
static void attach_parent_console() {}

static void show_error(const char* message) {
  std::fprintf(stderr, "error: %s\n", message);
}
#endif

static void usage(const char* argv0) {
  std::printf(
      "Usage: %s [rom_path] [options]\n"
      "  rom_path          Prepared rom/ folder, MAME ROM folder, or splatter.zip\n"
      "                    (default: auto-find rom/ or splatter.zip next to the exe)\n"
      "  --game PATH       Same as rom_path (for launchers)\n"
      "  --data PATH       Folder for options.cfg and prepared rom/ (RECOMPLAUNCHER_DATA)\n"
      "  --prepare [path]  Build rom/ from a zip or MAME folder, then exit\n"
      "  --headless        Null present/audio (CI / smoke)\n"
      "  --frames N        Run N frames then exit\n"
      "  --probe N         Headless N frames + write build/probe status/PPM\n"
      "  --probe-out DIR   Probe output directory (default build/probe)\n"
      "  --coin-frame N    Inject Coin 1 at 0-based frame N\n"
      "  --start-frame N   Inject Start 1 at 0-based frame N\n"
      "  --input-hold N    Hold injected inputs N frames (default 4)\n"
      "  --right-frame N   Inject Player 1 right at 0-based frame N\n"
      "  --right-hold N    Hold right N frames (default 60)\n"
      "  --trace-main N    Dump N main-CPU instructions then exit\n"
      "  --scale N         Integer window scale (default 3)\n"
      "  --skip-checksum   Run RAM/ROM/VRAM tests uncapped on this launch\n",
      argv0);
}

static fs::path exe_dir_of(const fs::path& argv0) {
  std::error_code ec;
  return fs::absolute(argv0, ec).parent_path();
}

static bool take_path_arg(int& i, int argc, char** argv, const char* flag, fs::path& out) {
  const size_t n = std::strlen(flag);
  if (std::strncmp(argv[i], flag, n) != 0)
    return false;
  if (argv[i][n] == '=') {
    out = argv[i] + n + 1;
    return true;
  }
  if (argv[i][n] != '\0')
    return false;
  if (i + 1 >= argc || argv[i + 1][0] == '-')
    throw sh::Error(std::string(flag) + " needs a path");
  out = argv[++i];
  return true;
}

static fs::path resolve_data_dir(const fs::path& argv0, const fs::path& cli_data) {
  if (!cli_data.empty())
    return cli_data;
  if (const char* env = std::getenv("RECOMPLAUNCHER_DATA")) {
    if (env[0])
      return env;
  }
  return exe_dir_of(argv0);
}

static fs::path find_default_rom_source(const fs::path& argv0, const fs::path& data_dir) {
  std::error_code ec;
  const fs::path exe_dir = exe_dir_of(argv0);
  const fs::path cwd = fs::current_path();
  const fs::path dirs[] = {data_dir, cwd, exe_dir, cwd.parent_path(), exe_dir.parent_path()};

  for (const auto& dir : dirs) {
    if (sh::is_prepared_rom_dir(dir / "rom"))
      return fs::weakly_canonical(dir / "rom", ec);
  }
  for (const auto& dir : dirs) {
    const fs::path zip = sh::find_romset_zip(dir);
    if (!zip.empty())
      return zip;
  }
  for (const auto& dir : dirs) {
    if (sh::is_mame_rom_dir(dir / "rom"))
      return fs::weakly_canonical(dir / "rom", ec);
    if (sh::is_mame_rom_dir(dir))
      return fs::weakly_canonical(dir, ec);
  }
  return data_dir / "rom";
}

int main(int argc, char** argv) {
  attach_parent_console();

  bool rom_path_set = false;
  bool prepare_only = false;
  fs::path rom_path;
  fs::path cli_data;
  sh::MachineConfig cfg;
  cfg.headless = false;
  int frames = 0;
  bool cli_skip_checksum = false;
  bool cli_scale = false;

  try {
    for (int i = 1; i < argc; ++i) {
      if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
        usage(argv[0]);
        return 0;
      }
      if (std::strcmp(argv[i], "--prepare") == 0) {
        prepare_only = true;
        if (i + 1 < argc && argv[i + 1][0] != '-') {
          rom_path = argv[++i];
          rom_path_set = true;
        }
        continue;
      }
      if (take_path_arg(i, argc, argv, "--game", rom_path)) {
        rom_path_set = true;
        continue;
      }
      if (take_path_arg(i, argc, argv, "--data", cli_data))
        continue;
      if (std::strcmp(argv[i], "--headless") == 0) {
        cfg.headless = true;
        continue;
      }
      if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
        frames = std::atoi(argv[++i]);
        continue;
      }
      if (std::strcmp(argv[i], "--probe") == 0 && i + 1 < argc) {
        frames = std::atoi(argv[++i]);
        cfg.headless = true;
        cfg.probe = true;
        continue;
      }
      if (std::strcmp(argv[i], "--probe-out") == 0 && i + 1 < argc) {
        cfg.probe_out = argv[++i];
        continue;
      }
      if (std::strcmp(argv[i], "--coin-frame") == 0 && i + 1 < argc) {
        cfg.coin_frame = std::atoi(argv[++i]);
        continue;
      }
      if (std::strcmp(argv[i], "--start-frame") == 0 && i + 1 < argc) {
        cfg.start_frame = std::atoi(argv[++i]);
        continue;
      }
      if (std::strcmp(argv[i], "--input-hold") == 0 && i + 1 < argc) {
        cfg.input_hold_frames = std::atoi(argv[++i]);
        continue;
      }
      if (std::strcmp(argv[i], "--right-frame") == 0 && i + 1 < argc) {
        cfg.right_frame = std::atoi(argv[++i]);
        continue;
      }
      if (std::strcmp(argv[i], "--right-hold") == 0 && i + 1 < argc) {
        cfg.right_hold_frames = std::atoi(argv[++i]);
        continue;
      }
      if (std::strcmp(argv[i], "--trace-main") == 0 && i + 1 < argc) {
        cfg.trace_main = true;
        cfg.max_main_steps = std::strtoull(argv[++i], nullptr, 10);
        cfg.headless = true;
        continue;
      }
      if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
        cfg.scale = std::atoi(argv[++i]);
        cli_scale = true;
        continue;
      }
      if (std::strcmp(argv[i], "--skip-checksum") == 0) {
        cli_skip_checksum = true;
        continue;
      }
      if (argv[i][0] != '-') {
        rom_path = argv[i];
        rom_path_set = true;
      }
    }

    const fs::path data_dir = resolve_data_dir(argv[0] ? argv[0] : "splatterhouse", cli_data);
    std::error_code path_ec;
    fs::create_directories(data_dir, path_ec);
    const fs::path options_path = data_dir / "options.cfg";
    const sh::HostOptions saved = sh::load_options(options_path);
    cfg.options_path = options_path.string();

    if (!rom_path_set)
      rom_path = find_default_rom_source(argv[0] ? argv[0] : "splatterhouse", data_dir);

    const fs::path prepare_out = data_dir / "rom";

    if (prepare_only) {
      const bool from_launcher = std::getenv("RECOMPLAUNCHER") != nullptr;
      const fs::path out = sh::prepare_rom_dir(rom_path, prepare_out, from_launcher);
      if (!from_launcher)
        std::printf("Ready: %s\n", (out / "mainrom.bin").string().c_str());
      return 0;
    }
    if (cfg.input_hold_frames < 1)
      throw sh::Error("--input-hold must be positive");
    if (cfg.right_hold_frames < 1)
      throw sh::Error("--right-hold must be positive");

    auto roms = sh::load_rom_path(rom_path, prepare_out);
    std::printf("Loaded set '%s' (%s)\n", roms.set_name.c_str(), roms.description.c_str());

    if (!cli_scale) {
      int scale = saved.scale;
      if (scale < 1)
        scale = 1;
      if (scale > 6)
        scale = 6;
      cfg.scale = scale;
    }
    sh::Machine machine(cfg);
    machine.options().stage_select = saved.stage_select;
    machine.options().skip_checksum = saved.skip_checksum || cli_skip_checksum;
    machine.options().invincible = saved.invincible;
    machine.options().untouchable = saved.untouchable;
    machine.options().uncap_fps = saved.uncap_fps;
    machine.options().debug_menu = saved.debug_menu;
    machine.options().mono_audio = saved.mono_audio;
    machine.options().fullscreen = saved.fullscreen;
    machine.options().stretch = saved.stretch;
    machine.options().image_filter = saved.image_filter;
    machine.options().language = saved.language;
    machine.options().vsync = saved.vsync;
    machine.options().scale = cfg.scale;
    machine.options().volume = saved.volume;
    machine.options().gamepad = saved.gamepad;
    for (int i = 0; i < sh::kBindCount; ++i) {
      machine.options().key_bind[i] = saved.key_bind[i];
      machine.options().pad_bind[i] = saved.pad_bind[i];
    }
#if defined(SH_HOST_SDL)
    if (!cfg.headless) {
      machine.set_present(sh::make_sdl_present(&machine.options()));
      machine.set_audio_backend(sh::make_sdl_audio());
      machine.set_input(sh::make_sdl_input(&machine.options()));
    }
#else
    if (!cfg.headless) {
      std::fprintf(stderr,
                   "No SDL window in this build. Rebuild with -DSH_HOST_SDL=ON,\n"
                   "or run: splatterhouse.exe --headless --frames 5\n");
      cfg.headless = true;
      if (frames <= 0)
        frames = 5;
    }
#endif
    machine.load(roms);
    machine.reset();

    const sh::u8 hi = machine.c117().read(sh::C117::Main, 0xFFFE);
    const sh::u8 lo = machine.c117().read(sh::C117::Main, 0xFFFF);
    std::printf("Reset vector %02X%02X  PC=%04X\n", hi, lo, machine.maincpu().pc);

    const int ran = machine.run(frames);
    std::printf("Ran %d frame(s). main cycles=%llu\n", ran,
                static_cast<unsigned long long>(machine.maincpu().cycles_total));
    return 0;
  } catch (const std::exception& ex) {
    show_error(ex.what());
    return 1;
  }
}
