module;

#include <shobjidl.h>
#include <wil/com.h>

module Utils.Dialog;

import std;
import Utils.Logger;
import Utils.String;
import Vendor.WIL;
import Vendor.Windows;

namespace Utils::Dialog {

// 辅助函数：解析文件过滤器 - 修复解析逻辑
auto parse_file_filter(const std::string& filter)
    -> std::pair<std::vector<std::wstring>, std::vector<std::wstring>> {
  std::vector<std::wstring> filter_names;
  std::vector<std::wstring> filter_patterns;

  if (filter.empty()) {
    return {filter_names, filter_patterns};
  }

  std::wstring filter_wide = Utils::String::FromUtf8(filter);

  // 按'|'分割，确保成对出现
  std::vector<std::wstring> segments;
  size_t start = 0;
  size_t pos = 0;

  while ((pos = filter_wide.find(L'|', start)) != std::wstring::npos) {
    segments.push_back(filter_wide.substr(start, pos - start));
    start = pos + 1;
  }

  // 添加最后一个段
  if (start < filter_wide.length()) {
    segments.push_back(filter_wide.substr(start));
  }

  // 确保是偶数个段（name-pattern对）
  if (segments.size() % 2 != 0) {
    Logger().warn("Filter string has odd number of segments, ignoring last segment");
    segments.pop_back();
  }

  // 分配到name和pattern数组
  for (size_t i = 0; i < segments.size(); i += 2) {
    filter_names.push_back(segments[i]);
    filter_patterns.push_back(segments[i + 1]);
  }

  return {filter_names, filter_patterns};
}

// 选择文件夹
auto select_folder(const FolderSelectorParams& params, HWND hwnd)
    -> std::expected<FolderSelectorResult, std::string> {
  try {
    // COM初始化
    auto com_init = wil::CoInitializeEx(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // 创建对话框 - 智能指针自动管理
    wil::com_ptr<IFileDialog> pFileDialog;
    Vendor::WIL::throw_if_failed(
        CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileDialog)));

    // 设置选项
    DWORD dwOptions;
    Vendor::WIL::throw_if_failed(pFileDialog->GetOptions(&dwOptions));
    Vendor::WIL::throw_if_failed(pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS));

    // 设置标题
    if (!params.title.empty()) {
      std::wstring title_wide = Utils::String::FromUtf8(params.title);
      Vendor::WIL::throw_if_failed(pFileDialog->SetTitle(title_wide.c_str()));
    } else {
      Vendor::WIL::throw_if_failed(pFileDialog->SetTitle(L"选择文件夹"));
    }

    // 显示对话框
    Vendor::WIL::throw_if_failed(pFileDialog->Show(hwnd));

    // 获取结果
    wil::com_ptr<IShellItem> pItem;
    Vendor::WIL::throw_if_failed(pFileDialog->GetResult(&pItem));

    // 获取路径 - 自动内存管理
    wil::unique_cotaskmem_string file_path;
    Vendor::WIL::throw_if_failed(pItem->GetDisplayName(SIGDN_FILESYSPATH, &file_path));

    // 返回结果
    std::filesystem::path result(file_path.get());
    Logger().info("User selected folder: {}", result.string());

    FolderSelectorResult folder_result;
    folder_result.path = result.string();
    return folder_result;

  } catch (const wil::ResultException& ex) {
    if (ex.GetErrorCode() ==
        Vendor::Windows::_HRESULT_FROM_WIN32(Vendor::Windows::kERROR_CANCELLED)) {
      return std::unexpected("User cancelled the operation");
    }
    return std::unexpected(std::string("Dialog operation failed: ") + ex.what());
  }
}

// 选择文件
auto select_file(const FileSelectorParams& params, HWND hwnd)
    -> std::expected<FileSelectorResult, std::string> {
  try {
    // COM初始化
    auto com_init = wil::CoInitializeEx(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // 创建对话框 - 智能指针自动管理
    wil::com_ptr<IFileOpenDialog> pFileDialog;
    Vendor::WIL::throw_if_failed(
        CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileDialog)));

    // 设置选项
    DWORD dwOptions;
    Vendor::WIL::throw_if_failed(pFileDialog->GetOptions(&dwOptions));
    dwOptions |= FOS_FILEMUSTEXIST;  // 文件必须存在
    if (params.allow_multiple) {
      dwOptions |= FOS_ALLOWMULTISELECT;  // 允许多选
    }
    Vendor::WIL::throw_if_failed(pFileDialog->SetOptions(dwOptions));

    // 设置过滤器 - 使用修复后的解析逻辑
    if (!params.filter.empty()) {
      auto [filter_names, filter_patterns] = parse_file_filter(params.filter);

      // 创建COMDLG_FILTERSPEC数组
      if (!filter_names.empty() && !filter_patterns.empty()) {
        std::vector<COMDLG_FILTERSPEC> filter_specs;
        filter_specs.resize(filter_names.size());

        for (size_t i = 0; i < filter_names.size(); ++i) {
          filter_specs[i].pszName = filter_names[i].c_str();
          filter_specs[i].pszSpec = filter_patterns[i].c_str();
        }

        // 设置文件类型过滤器
        HRESULT hr =
            pFileDialog->SetFileTypes(static_cast<UINT>(filter_specs.size()), filter_specs.data());
        if (FAILED(hr)) {
          Logger().warn("Failed to set file filters (HRESULT: 0x{}), continuing without filter",
                        std::to_string(static_cast<unsigned>(hr)));
        }
      }
    }

    // 设置标题
    if (!params.title.empty()) {
      std::wstring title_wide = Utils::String::FromUtf8(params.title);
      Vendor::WIL::throw_if_failed(pFileDialog->SetTitle(title_wide.c_str()));
    }

    // 显示对话框
    Vendor::WIL::throw_if_failed(pFileDialog->Show(hwnd));

    // 获取结果
    std::vector<std::filesystem::path> selected_paths;

    if (params.allow_multiple) {
      // 多文件选择
      wil::com_ptr<IShellItemArray> pItemArray;
      Vendor::WIL::throw_if_failed(pFileDialog->GetResults(&pItemArray));

      // 获取文件数量
      DWORD count = 0;
      Vendor::WIL::throw_if_failed(pItemArray->GetCount(&count));
      selected_paths.reserve(count);

      for (DWORD i = 0; i < count; ++i) {
        wil::com_ptr<IShellItem> pItem;
        if (SUCCEEDED(pItemArray->GetItemAt(i, &pItem))) {
          wil::unique_cotaskmem_string file_path;
          if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &file_path))) {
            selected_paths.emplace_back(file_path.get());
          } else {
            Logger().warn("Failed to get file path for item at index {}, skipping", i);
          }
        } else {
          Logger().warn("Failed to get item at index {}, skipping", i);
        }
      }
    } else {
      // 单文件选择
      wil::com_ptr<IShellItem> pItem;
      Vendor::WIL::throw_if_failed(pFileDialog->GetResult(&pItem));

      wil::unique_cotaskmem_string file_path;
      Vendor::WIL::throw_if_failed(pItem->GetDisplayName(SIGDN_FILESYSPATH, &file_path));
      selected_paths.emplace_back(file_path.get());
    }

    if (selected_paths.empty()) {
      return std::unexpected("No valid files were selected");
    }

    Logger().info("Selected {} file(s)", selected_paths.size());

    // 转换为FileSelectorResult
    FileSelectorResult file_selector_result;
    file_selector_result.paths.reserve(selected_paths.size());
    for (const auto& path : selected_paths) {
      file_selector_result.paths.push_back(path.string());
    }

    return file_selector_result;

  } catch (const wil::ResultException& ex) {
    if (ex.GetErrorCode() ==
        Vendor::Windows::_HRESULT_FROM_WIN32(Vendor::Windows::kERROR_CANCELLED)) {
      return std::unexpected("User cancelled the operation");
    }
    return std::unexpected(std::string("Dialog operation failed: ") + ex.what());
  }
}

}  // namespace Utils::Dialog