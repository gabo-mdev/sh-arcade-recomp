#pragma once

#include "machine/romset.hpp"

#include <filesystem>

namespace sh {

/// True when `dir` already has mapped images (`mainrom.bin`, …).
bool is_prepared_rom_dir(const std::filesystem::path& dir);

/// True when `dir` looks like a loose MAME World SH3 (splatter) romset.
bool is_mame_rom_dir(const std::filesystem::path& dir);

/// Look for `splatter.zip` in `dir`.
std::filesystem::path find_romset_zip(const std::filesystem::path& dir);

/// Build mapped images into `out_dir` from a MAME folder or `splatter.zip`.
/// Overwrites files in `out_dir`. Returns the prepared directory.
/// When `progress` is true, prints progress lines: `progress 0.4 Text`.
std::filesystem::path prepare_rom_dir(const std::filesystem::path& zip_or_dir,
                                      const std::filesystem::path& out_dir,
                                      bool progress = false);

/// Resolve a user path (zip, MAME folder, or prepared rom/) into a prepared
/// rom directory under `out_dir` when needed, then load it.
RomSet load_rom_path(const std::filesystem::path& path,
                     const std::filesystem::path& prepare_out);

} // namespace sh
