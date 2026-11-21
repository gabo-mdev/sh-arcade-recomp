#include "machine/rom_prepare.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
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
#endif

namespace sh {
namespace fs = std::filesystem;

namespace {

struct SetInfo {
  const char* name;
  const char* desc;
  const char* snd0;
  std::pair<const char*, u32> prg[8];
  std::pair<const char*, u32> extra[4];
};

constexpr SetInfo kSets[] = {
    {
        "splatter",
        "World, new version (SH3)",
        "sh1_snd0b.bin",
        {{"sh1_prg0.bin", 0x000000},
         {"sh1_prg1.bin", 0x080000},
         {"sh1_prg2.bin", 0x100000},
         {"sh1_prg3.bin", 0x180000},
         {"sh1_prg4.bin", 0x200000},
         {"sh3_prg5.bin", 0x280000},
         {"sh3_prg6.bin", 0x300000},
         {"sh3_prg7.bin", 0x380000}},
        {{"sh3_prg5.bin", 0x10000},
         {"sh3_prg6.bin", 0x10000},
         {"sh3_prg7.bin", 0x10000},
         {"sh1_snd0b.bin", 0x10000}},
    },
    {
        "splatter2",
        "World, old version (SH2)",
        "sh1_snd0.bin",
        {{"sh1_prg0.bin", 0x000000},
         {"sh1_prg1.bin", 0x080000},
         {"sh1_prg2.bin", 0x100000},
         {"sh1_prg3.bin", 0x180000},
         {"sh1_prg4.bin", 0x200000},
         {"sh1_prg5.bin", 0x280000},
         {"sh2_prg6.bin", 0x300000},
         {"sh2_prg7.bin", 0x380000}},
        {{"sh1_prg5.bin", 0x10000},
         {"sh2_prg6.bin", 0x10000},
         {"sh2_prg7.bin", 0x10000},
         {"sh1_snd0.bin", 0x10000}},
    },
    {
        "splatterj",
        "Japan (SH1)",
        "sh1_snd0.bin",
        {{"sh1_prg0.bin", 0x000000},
         {"sh1_prg1.bin", 0x080000},
         {"sh1_prg2.bin", 0x100000},
         {"sh1_prg3.bin", 0x180000},
         {"sh1_prg4.bin", 0x200000},
         {"sh1_prg5.bin", 0x280000},
         {"sh1_prg6.bin", 0x300000},
         {"sh1_prg7.bin", 0x380000}},
        {{"sh1_prg5.bin", 0x10000},
         {"sh1_prg6.bin", 0x10000},
         {"sh1_prg7.bin", 0x10000},
         {"sh1_snd0.bin", 0x10000}},
    },
};

constexpr std::pair<const char*, u32> kCommon[] = {
    {"cus64-64a1.mcu", 0x1000}, {"sh_chr-0.bin", 0x20000}, {"sh_chr-1.bin", 0x20000},
    {"sh_chr-2.bin", 0x20000},  {"sh_chr-3.bin", 0x20000}, {"sh_chr-4.bin", 0x20000},
    {"sh_chr-5.bin", 0x20000},  {"sh_chr-8.bin", 0x20000}, {"sh_obj-0.bin", 0x20000},
    {"sh_obj-1.bin", 0x20000},  {"sh_obj-2.bin", 0x20000}, {"sh_obj-3.bin", 0x20000},
    {"sh_obj-4.bin", 0x20000},  {"sh_obj-5.bin", 0x20000}, {"sh_obj-6.bin", 0x20000},
    {"sh_obj-7.bin", 0x20000},  {"sh_voi-0.bin", 0x20000}, {"sh_voi-1.bin", 0x20000},
    {"sh_voi-2.bin", 0x20000},  {"sh_voi-3.bin", 0x20000}, {"sh1_snd1.bin", 0x10000},
    {"sh1_prg0.bin", 0x10000},  {"sh1_prg1.bin", 0x10000}, {"sh1_prg2.bin", 0x10000},
    {"sh1_prg3.bin", 0x10000},  {"sh1_prg4.bin", 0x10000},
};

std::string lower(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

using FileMap = std::map<std::string, fs::path>;

FileMap index_dir(const fs::path& dir) {
  FileMap files;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file())
      continue;
    files.emplace(lower(entry.path().filename().string()), entry.path());
  }
  return files;
}

bool has_file(const FileMap& files, const char* name) {
  return files.find(lower(name)) != files.end();
}

fs::path require_file(const FileMap& files, const char* name) {
  const auto it = files.find(lower(name));
  if (it == files.end())
    throw Error(std::string("Missing ROM file: ") + name);
  return it->second;
}

const SetInfo* detect_set(const FileMap& files) {
  if (has_file(files, "sh3_prg5.bin") && has_file(files, "sh3_prg6.bin") &&
      has_file(files, "sh3_prg7.bin") && has_file(files, "sh1_snd0b.bin"))
    return &kSets[0];
  if (has_file(files, "sh2_prg6.bin") && has_file(files, "sh2_prg7.bin"))
    return &kSets[1];
  if (has_file(files, "sh1_prg6.bin") && has_file(files, "sh1_prg7.bin"))
    return &kSets[2];
  return nullptr;
}

void validate(const FileMap& files, const SetInfo& set) {
  std::string err;
  auto check = [&](const char* name, u32 size) {
    const auto it = files.find(lower(name));
    if (it == files.end()) {
      err += "Missing: ";
      err += name;
      err += "\n";
      return;
    }
    const auto got = static_cast<u32>(fs::file_size(it->second));
    if (got != size) {
      err += name;
      err += ": expected ";
      err += std::to_string(size);
      err += " bytes, got ";
      err += std::to_string(got);
      err += "\n";
    }
  };
  for (const auto& [name, size] : kCommon)
    check(name, size);
  for (const auto& [name, size] : set.extra)
    check(name, size);
  if (!err.empty())
    throw Error("ROM validation failed:\n" + err);
}

std::vector<u8> read_exact(const fs::path& path, size_t expect) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw Error("Cannot open " + path.string());
  std::vector<u8> buf(expect);
  in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(expect));
  if (static_cast<size_t>(in.gcount()) != expect)
    throw Error(path.filename().string() + " size mismatch");
  return buf;
}

void write_all(const fs::path& path, const std::vector<u8>& data) {
  std::ofstream out(path, std::ios::binary);
  if (!out)
    throw Error("Cannot write " + path.string());
  out.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
}

void load512(std::vector<u8>& mainrom, const fs::path& path, u32 start) {
  const auto data = read_exact(path, 0x10000);
  for (int i = 0; i < 8; ++i) {
    const u32 off = start + static_cast<u32>(i) * 0x10000u;
    std::copy(data.begin(), data.end(), mainrom.begin() + static_cast<std::ptrdiff_t>(off));
  }
}

void apply_prg7_bit16_swap(std::vector<u8>& mainrom) {
  for (u32 i = 0x380000; i < 0x400000; ++i) {
    if ((i & 0x010000u) == 0) {
      const u32 j = i + 0x010000u;
      std::swap(mainrom[i], mainrom[j]);
    }
  }
}

void clear_dir_files(const fs::path& dir) {
  std::error_code ec;
  if (!fs::exists(dir, ec))
    return;
  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (entry.is_regular_file())
      fs::remove(entry.path(), ec);
  }
}

#if defined(_WIN32)
void extract_zip_windows(const fs::path& zip, const fs::path& dest) {
  fs::create_directories(dest);
  clear_dir_files(dest);
  // Expand-Archive is available on Windows 10+ and needs no extra deps.
  std::wstring zip_w = zip.wstring();
  std::wstring dest_w = dest.wstring();
  // Escape single quotes for PowerShell single-quoted strings.
  auto ps_quote = [](std::wstring s) {
    std::wstring out;
    out.reserve(s.size() + 8);
    for (wchar_t c : s) {
      if (c == L'\'')
        out += L"''";
      else
        out += c;
    }
    return out;
  };
  std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
                     L"Expand-Archive -LiteralPath '" +
                     ps_quote(zip_w) + L"' -DestinationPath '" + ps_quote(dest_w) +
                     L"' -Force";
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> mutable_cmd(cmd.begin(), cmd.end());
  mutable_cmd.push_back(0);
  if (!CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                      nullptr, nullptr, &si, &pi)) {
    throw Error("Failed to start PowerShell to extract " + zip.string());
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (code != 0)
    throw Error("Expand-Archive failed for " + zip.string());

  // Some zips nest files one folder deep — flatten one level if needed.
  FileMap files = index_dir(dest);
  if (!detect_set(files)) {
    for (const auto& entry : fs::directory_iterator(dest)) {
      if (!entry.is_directory())
        continue;
      FileMap nested = index_dir(entry.path());
      if (!detect_set(nested))
        continue;
      for (const auto& [_, src] : nested) {
        const fs::path target = dest / src.filename();
        std::error_code ec;
        fs::rename(src, target, ec);
        if (ec)
          fs::copy_file(src, target, fs::copy_options::overwrite_existing);
      }
      break;
    }
  }
}
#else
void extract_zip_windows(const fs::path&, const fs::path&) {
  throw Error("Zip extraction requires Windows Expand-Archive, or pass an extracted folder");
}
#endif

void build_images(const fs::path& out_dir, const SetInfo& set, const FileMap& files) {
  std::vector<u8> mainrom(0x400000, 0);
  for (const auto& [name, start] : set.prg)
    load512(mainrom, require_file(files, name), start);
  apply_prg7_bit16_swap(mainrom);
  write_all(out_dir / "mainrom.bin", mainrom);

  std::vector<u8> snd = read_exact(require_file(files, set.snd0), 0x10000);
  const auto snd1 = read_exact(require_file(files, "sh1_snd1.bin"), 0x10000);
  snd.insert(snd.end(), snd1.begin(), snd1.end());
  write_all(out_dir / "audiocpu.bin", snd);

  write_all(out_dir / "mcu.bin", read_exact(require_file(files, "cus64-64a1.mcu"), 0x1000));

  std::vector<u8> voice;
  voice.reserve(0x80000);
  for (int i = 0; i < 4; ++i) {
    const std::string name = "sh_voi-" + std::to_string(i) + ".bin";
    const auto chunk = read_exact(require_file(files, name.c_str()), 0x20000);
    voice.insert(voice.end(), chunk.begin(), chunk.end());
  }
  write_all(out_dir / "voice.bin", voice);

  std::vector<u8> tiles;
  tiles.reserve(0xC0000);
  for (int i = 0; i < 6; ++i) {
    const std::string name = "sh_chr-" + std::to_string(i) + ".bin";
    const auto chunk = read_exact(require_file(files, name.c_str()), 0x20000);
    tiles.insert(tiles.end(), chunk.begin(), chunk.end());
  }
  write_all(out_dir / "c123tmap.bin", tiles);
  write_all(out_dir / "c123mask.bin",
            read_exact(require_file(files, "sh_chr-8.bin"), 0x20000));

  std::vector<u8> sprites;
  sprites.reserve(0x100000);
  for (int i = 0; i < 8; ++i) {
    const std::string name = "sh_obj-" + std::to_string(i) + ".bin";
    const auto chunk = read_exact(require_file(files, name.c_str()), 0x20000);
    sprites.insert(sprites.end(), chunk.begin(), chunk.end());
  }
  write_all(out_dir / "sprites.bin", sprites);

  std::ofstream meta(out_dir / "set.txt");
  meta << "set=" << set.name << "\n";
  meta << "desc=" << set.desc << "\n";
}

void prepare_from_mame_files(const FileMap& files, const fs::path& out_dir) {
  const SetInfo* set = detect_set(files);
  if (!set)
    throw Error(
        "Could not detect Splatterhouse set. Need SH3 (sh3_prg*), SH2 (sh2_prg*), or SH1 "
        "(sh1_prg6/7).");
  validate(files, *set);
  fs::create_directories(out_dir);
  // Keep source MAME files if out_dir is the same folder; still rewrite mapped images.
  build_images(out_dir, *set, files);
  std::printf("Prepared set '%s' (%s) in %s\n", set->name, set->desc, out_dir.string().c_str());
}

} // namespace

bool is_prepared_rom_dir(const fs::path& dir) {
  return fs::is_regular_file(dir / "mainrom.bin") && fs::is_regular_file(dir / "audiocpu.bin") &&
         fs::is_regular_file(dir / "mcu.bin") && fs::is_regular_file(dir / "voice.bin") &&
         fs::is_regular_file(dir / "c123tmap.bin") && fs::is_regular_file(dir / "c123mask.bin") &&
         fs::is_regular_file(dir / "sprites.bin");
}

bool is_mame_rom_dir(const fs::path& dir) {
  if (!fs::is_directory(dir))
    return false;
  return detect_set(index_dir(dir)) != nullptr;
}

fs::path find_romset_zip(const fs::path& dir) {
  static const char* names[] = {"splatter.zip", "splatter2.zip", "splatterj.zip"};
  for (const char* name : names) {
    const fs::path p = dir / name;
    if (fs::is_regular_file(p))
      return p;
  }
  return {};
}

fs::path prepare_rom_dir(const fs::path& zip_or_dir, const fs::path& out_dir) {
  std::error_code ec;
  const fs::path src = fs::weakly_canonical(zip_or_dir, ec);
  if (fs::is_regular_file(src) && lower(src.extension().string()) == ".zip") {
    const fs::path extract_to = out_dir / "_mame_extract";
    std::printf("Extracting %s\n", src.string().c_str());
    extract_zip_windows(src, extract_to);
    prepare_from_mame_files(index_dir(extract_to), out_dir);
    // Drop loose extract after mapping so install stays small.
    fs::remove_all(extract_to, ec);
    return out_dir;
  }
  if (fs::is_directory(src)) {
    if (is_prepared_rom_dir(src))
      return src;
    if (detect_set(index_dir(src))) {
      prepare_from_mame_files(index_dir(src), out_dir);
      return out_dir;
    }
    const fs::path nested_zip = find_romset_zip(src);
    if (!nested_zip.empty())
      return prepare_rom_dir(nested_zip, out_dir);
  }
  throw Error("Not a Splatterhouse zip or ROM folder: " + zip_or_dir.string());
}

RomSet load_rom_path(const fs::path& path, const fs::path& prepare_out) {
  if (is_prepared_rom_dir(path))
    return RomSet::load(path);
  const fs::path prepared = prepare_rom_dir(path, prepare_out);
  return RomSet::load(prepared);
}

} // namespace sh
