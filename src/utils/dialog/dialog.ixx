module;

export module Utils.Dialog;

import std;
import Vendor.Windows;

namespace Utils::Dialog {

export struct FileSelectorParams {
  std::string title;
  std::string filter;
  bool allow_multiple{false};
  std::int8_t parent_window_mode{0};  // 0 = 无父窗口, 1 = webview2, 2 = 激活窗口
};

export struct FileSelectorResult {
  std::vector<std::string> paths;
};

export struct FolderSelectorParams {
  std::string title;
  std::int8_t parent_window_mode{0};  // 0 = 无父窗口, 1 = webview2, 2 = 激活窗口
};

export struct FolderSelectorResult {
  std::string path;
};

export auto select_folder(const FolderSelectorParams& params, Vendor::Windows::HWND hwnd = nullptr)
    -> std::expected<FolderSelectorResult, std::string>;

export auto select_file(const FileSelectorParams& params, Vendor::Windows::HWND hwnd = nullptr)
    -> std::expected<FileSelectorResult, std::string>;

}  // namespace Utils::Dialog