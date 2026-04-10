module;

module Utils.Path;

import std;
import Vendor.ShellApi;
import <windows.h>;

namespace Utils::Path::Detail {

constexpr std::wstring_view kPortableMarker = L"portable";
constexpr std::wstring_view kAppName = L"SpinningMomo";

auto ensure_path_exists(const std::filesystem::path& path)
    -> std::expected<std::filesystem::path, std::string> {
  auto ensure_result = Utils::Path::EnsureDirectoryExists(path);
  if (!ensure_result) {
    return std::unexpected(ensure_result.error());
  }

  return path;
}

}  // namespace Utils::Path::Detail

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

auto Utils::Path::GetAppMode() -> AppMode {
  auto exe_dir_result = GetExecutableDirectory();
  if (!exe_dir_result) {
    return AppMode::Portable;
  }

  return std::filesystem::exists(exe_dir_result.value() / Detail::kPortableMarker)
             ? AppMode::Portable
             : AppMode::Installed;
}

auto Utils::Path::GetAppDataDirectory() -> std::expected<std::filesystem::path, std::string> {
  if (GetAppMode() == AppMode::Portable) {
    auto exe_dir_result = GetExecutableDirectory();
    if (!exe_dir_result) {
      return std::unexpected("Failed to get executable directory: " + exe_dir_result.error());
    }

    return Detail::ensure_path_exists(exe_dir_result.value() / "data");
  }

  PWSTR local_app_data_raw = nullptr;
  const auto hr = Vendor::ShellApi::SHGetKnownFolderPath(Vendor::ShellApi::kFOLDERID_LocalAppData,
                                                         0, nullptr, &local_app_data_raw);
  if (FAILED(hr) || !local_app_data_raw) {
    if (local_app_data_raw) {
      Vendor::ShellApi::CoTaskMemFree(local_app_data_raw);
    }
    return std::unexpected("Failed to get LocalAppData directory, HRESULT: " + std::to_string(hr));
  }

  std::filesystem::path app_data_root =
      std::filesystem::path(local_app_data_raw) / Detail::kAppName;
  Vendor::ShellApi::CoTaskMemFree(local_app_data_raw);
  return Detail::ensure_path_exists(app_data_root);
}

auto Utils::Path::GetAppDataSubdirectory(std::string_view name)
    -> std::expected<std::filesystem::path, std::string> {
  auto app_data_dir_result = GetAppDataDirectory();
  if (!app_data_dir_result) {
    return std::unexpected(app_data_dir_result.error());
  }

  return Detail::ensure_path_exists(app_data_dir_result.value() /
                                    std::filesystem::path(std::string{name}));
}

auto Utils::Path::GetAppDataFilePath(std::string_view filename)
    -> std::expected<std::filesystem::path, std::string> {
  auto app_data_dir_result = GetAppDataDirectory();
  if (!app_data_dir_result) {
    return std::unexpected(app_data_dir_result.error());
  }

  return app_data_dir_result.value() / std::filesystem::path(std::string{filename});
}

auto Utils::Path::GetEmbeddedWebRootDirectory()
    -> std::expected<std::filesystem::path, std::string> {
  auto exe_dir_result = GetExecutableDirectory();
  if (!exe_dir_result) {
    return std::unexpected("Failed to get executable directory: " + exe_dir_result.error());
  }

  return exe_dir_result.value() / "resources" / "web";
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

// 把路径转成适合比较的统一形式：先 lexically_normal 消除冗余分隔符，
// 再转小写、统一为正斜杠。用于 Windows 大小写不敏感的前缀匹配场景。
auto Utils::Path::NormalizeForComparison(const std::filesystem::path& path) -> std::wstring {
  auto value = path.lexically_normal().generic_wstring();
  std::ranges::transform(value, value.begin(),
                         [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  return value;
}

// 判断 target 是否位于 base 目录内部（大小写不敏感，Windows 语义）。
// 通过前缀匹配实现，匹配后确认紧随字符为 '/' 或路径刚好等长，
// 防止 /foo/bar 被误判为 /foo/ba 的子目录。
auto Utils::Path::IsPathWithinBase(const std::filesystem::path& target,
                                   const std::filesystem::path& base) -> bool {
  auto normalized_base = NormalizeForComparison(base);
  auto normalized_target = NormalizeForComparison(target);

  if (!normalized_target.starts_with(normalized_base)) {
    return false;
  }

  if (normalized_target.size() == normalized_base.size()) {
    return true;
  }

  return normalized_target[normalized_base.size()] == L'/';
}

// 获取用户视频文件夹路径 (FOLDERID_Videos)
auto Utils::Path::GetUserVideosDirectory() -> std::expected<std::filesystem::path, std::string> {
  PWSTR path = nullptr;
  HRESULT hr =
      Vendor::ShellApi::SHGetKnownFolderPath(Vendor::ShellApi::kFOLDERID_Videos, 0, nullptr, &path);
  if (FAILED(hr) || !path) {
    if (path) Vendor::ShellApi::CoTaskMemFree(path);
    return std::unexpected("Failed to get user Videos directory, HRESULT: " + std::to_string(hr));
  }

  std::filesystem::path result(path);
  Vendor::ShellApi::CoTaskMemFree(path);
  return result;
}

auto Utils::Path::GetOutputDirectory(const std::string& configured_output_dir_path)
    -> std::expected<std::filesystem::path, std::string> {
  if (!configured_output_dir_path.empty()) {
    std::filesystem::path configured_path = configured_output_dir_path;
    auto ensure_result = EnsureDirectoryExists(configured_path);
    if (!ensure_result) {
      return std::unexpected("Failed to create configured output directory: " +
                             ensure_result.error());
    }
    return configured_path;
  }

  auto videos_dir_result = GetUserVideosDirectory();
  if (videos_dir_result) {
    auto output_dir = *videos_dir_result / "SpinningMomo";
    auto ensure_result = EnsureDirectoryExists(output_dir);
    if (ensure_result) {
      return output_dir;
    }
  }

  auto exe_dir_result = GetExecutableDirectory();
  if (!exe_dir_result) {
    return std::unexpected("Failed to get executable directory: " + exe_dir_result.error());
  }

  auto fallback_output_dir = *exe_dir_result / "SpinningMomo";
  auto ensure_result = EnsureDirectoryExists(fallback_output_dir);
  if (!ensure_result) {
    return std::unexpected("Failed to create fallback output directory: " + ensure_result.error());
  }

  return fallback_output_dir;
}
