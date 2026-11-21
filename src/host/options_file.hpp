#pragma once

#include "host/backends.hpp"

#include <filesystem>

namespace sh {

/// Missing file or unreadable lines keep the defaults. Unknown keys are ignored.
HostOptions load_options(const std::filesystem::path& path);

/// Writes the host switches. `dirty` is not stored. Returns false if the file
/// could not be created.
bool save_options(const std::filesystem::path& path, const HostOptions& options);

} // namespace sh
