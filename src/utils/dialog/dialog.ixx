module;

export module Utils.Dialog;

import std;
import Vendor.Windows;

namespace Utils::Dialog {

export struct FileSelectorParams {
  std::string title;
  std::string filter;
  bool allow_multiple{false};
  bool parent_to_webview{true};
};

export struct FileSelectorResult {
  std::vector<std::string> paths;
};

export struct FolderSelectorParams {
  std::string title;
  bool parent_to_webview{true};
};

export struct FolderSelectorResult {
  std::string path;
};

export auto select_folder(const FolderSelectorParams& params, Vendor::Windows::HWND hwnd = nullptr)
    -> std::expected<FolderSelectorResult, std::string>;

export auto select_file(const FileSelectorParams& params, Vendor::Windows::HWND hwnd = nullptr)
    -> std::expected<FileSelectorResult, std::string>;

}  // namespace Utils::Dialog