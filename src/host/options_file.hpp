#pragma once

#include "host/backends.hpp"

#include <filesystem>

namespace sh {

/// Loads host options. If the file is missing, writes a default `options.cfg`
/// and returns those defaults. Unreadable lines / unknown keys are ignored.
HostOptions load_options(const std::filesystem::path& path);

/// Writes the host switches. `dirty` is not stored. Returns false if the file
/// could not be created.
bool save_options(const std::filesystem::path& path, const HostOptions& options);

} // namespace sh
