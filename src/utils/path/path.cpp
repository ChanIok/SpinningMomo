module;

module Utils.Path;

import std;
import <windows.h>;

// 获取当前程序的完整路径
auto Utils::Path::GetExecutablePath() -> std::expected<std::filesystem::path, std::string> {
  // 静态缓存，只在第一次调用时初始化
  static std::optional<std::filesystem::path> cached_path;
  static std::optional<std::string> cached_error;

  if (!cached_path.has_value() && !cached_error.has_value()) {
    try {
      std::vector<wchar_t> buffer(MAX_PATH);

      while (true) {
        DWORD size = GetModuleFileNameW(NULL, buffer.data(), static_cast<DWORD>(buffer.size()));

        if (size == 0) {
          cached_error = "Failed to get executable path, error: " + std::to_string(GetLastError());
          break;
        }

        if (size < buffer.size()) {
          cached_path = std::filesystem::path(buffer.data(), buffer.data() + size);
          break;
        }

        if (buffer.size() >= 32767) {
          cached_error = "Path too long for GetModuleFileNameW";
          break;
        }

        buffer.resize(buffer.size() * 2);
      }
    } catch (const std::exception& e) {
      cached_error = "Exception: " + std::string(e.what());
    }
  }

  if (cached_path.has_value()) {
    return cached_path.value();
  } else {
    return std::unexpected(cached_error.value());
  }
}

// 获取当前程序所在的目录路径
auto Utils::Path::GetExecutableDirectory() -> std::expected<std::filesystem::path, std::string> {
  auto pathResult = GetExecutablePath();
  if (!pathResult) {
    return std::unexpected(pathResult.error());
  }

  try {
    return pathResult.value().parent_path();
  } catch (const std::exception& e) {
    return std::unexpected("Exception getting directory: " + std::string(e.what()));
  }
}

// 确保目录存在，如果不存在则创建
auto Utils::Path::EnsureDirectoryExists(const std::filesystem::path& dir)
    -> std::expected<void, std::string> {
  try {
    if (!std::filesystem::exists(dir)) {
      std::filesystem::create_directories(dir);
    }
    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to create directory: " + std::string(e.what()));
  }
}

// 组合路径，类似于 path1 / path2
auto Utils::Path::Combine(const std::filesystem::path& base, const std::string& filename)
    -> std::filesystem::path {
  return base / filename;
}

// 规范化路径为绝对路径，默认相对于程序目录
auto Utils::Path::NormalizePath(const std::filesystem::path& path,
                                std::optional<std::filesystem::path> base)
    -> std::expected<std::filesystem::path, std::string> {
  try {
    std::filesystem::path base_path;

    if (base.has_value()) {
      base_path = base.value();
    } else {
      // 默认使用程序目录作为base
      auto exe_dir_result = GetExecutableDirectory();
      if (!exe_dir_result) {
        return std::unexpected("Failed to get executable directory: " + exe_dir_result.error());
      }
      base_path = exe_dir_result.value();
    }

    if (!base_path.is_absolute()) {
      return std::unexpected("Base path must be an absolute path.");
    }

    std::filesystem::path combined_path;
    if (path.is_absolute()) {
      combined_path = path;
    } else {
      combined_path = base_path / path;
    }

    std::filesystem::path normalized_path = std::filesystem::weakly_canonical(combined_path);

    // 统一使用正斜杠格式，确保跨平台一致性
    return std::filesystem::path(normalized_path.generic_string());

  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected(std::string(e.what()));
  }
}
