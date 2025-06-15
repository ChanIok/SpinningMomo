module;

#include <windows.h>
#include <filesystem>
#include <expected>
#include <vector>
#include <string>
#include <exception>

module Utils.Path;

// 获取当前程序的完整路径
auto Utils::Path::GetExecutablePath() -> std::expected<std::filesystem::path, std::string> {
  try {
    std::vector<wchar_t> buffer(MAX_PATH);

    while (true) {
      DWORD size = GetModuleFileNameW(NULL, buffer.data(), static_cast<DWORD>(buffer.size()));

      if (size == 0) {
        return std::unexpected("Failed to get executable path, error: " +
                               std::to_string(GetLastError()));
      }

      if (size < buffer.size()) {
        // 成功获取完整路径
        return std::filesystem::path(buffer.data(), buffer.data() + size);
      }

      // 需要更大的缓冲区
      if (buffer.size() >= 32767) {
        return std::unexpected("Path too long for GetModuleFileNameW");
      }

      buffer.resize(buffer.size() * 2);
    }
  } catch (const std::exception& e) {
    return std::unexpected("Exception: " + std::string(e.what()));
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