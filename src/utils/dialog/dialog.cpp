module;

#include <windows.h>  // 必须放在最前面
#include <comdef.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <filesystem>

module Utils.Dialog;

import std;
import Utils.Logger;
import Utils.String;
import Utils.Dialog;

namespace Utils::Dialog {

// 定义COM资源的删除器
struct ComDeleter {
  void operator()(void*) const { CoUninitialize(); }
};

// 定义COM对象的删除器
template <typename T>
struct ComPtrDeleter {
  void operator()(T* ptr) const {
    if (ptr) ptr->Release();
  }
};

// 定义CoTaskMemFree的删除器
struct CoTaskMemDeleter {
  void operator()(wchar_t* ptr) const {
    if (ptr) CoTaskMemFree(ptr);
  }
};

// 选择文件夹
auto select_folder(const FolderSelectorParams& params,
                   HWND hwnd)
    -> std::expected<FolderSelectorResult, std::string> {
  // 初始化COM
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    return std::unexpected("Failed to initialize COM library");
  }

  // RAII管理COM库的释放
  auto com_guard = std::unique_ptr<void, ComDeleter>(nullptr);

  // 创建文件对话框实例
  IFileDialog* pFileDialog = nullptr;
  hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileDialog,
                        reinterpret_cast<void**>(&pFileDialog));
  if (FAILED(hr)) {
    return std::unexpected("Failed to create file dialog instance");
  }

  // RAII管理COM对象的释放
  auto dialog_guard = std::unique_ptr<IFileDialog, ComPtrDeleter<IFileDialog>>(pFileDialog);

  // 设置选项，只允许选择文件夹
  DWORD dwOptions;
  hr = pFileDialog->GetOptions(&dwOptions);
  if (SUCCEEDED(hr)) {
    hr = pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);
  }
  if (FAILED(hr)) {
    return std::unexpected("Failed to set dialog options");
  }

  // 设置标题
  if (!params.title.empty()) {
    std::wstring title_wide = Utils::String::FromUtf8(params.title);
    pFileDialog->SetTitle(title_wide.c_str());
  } else {
    pFileDialog->SetTitle(L"选择文件夹");
  }

  // 显示对话框
  hr = pFileDialog->Show(hwnd);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return std::unexpected("User cancelled the operation");
  }
  if (FAILED(hr)) {
    return std::unexpected("Failed to show dialog");
  }

  // 获取结果
  IShellItem* pItem = nullptr;
  hr = pFileDialog->GetResult(&pItem);
  if (FAILED(hr)) {
    return std::unexpected("Failed to get dialog result");
  }

  // RAII管理ShellItem的释放
  auto item_guard = std::unique_ptr<IShellItem, ComPtrDeleter<IShellItem>>(pItem);

  // 获取路径
  PWSTR pszFilePath = nullptr;
  hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
  if (FAILED(hr)) {
    return std::unexpected("Failed to get selected path");
  }

  // RAII管理字符串的释放
  auto string_guard = std::unique_ptr<wchar_t, CoTaskMemDeleter>(pszFilePath);

  // 转换为std::filesystem::path并返回
  std::filesystem::path result(pszFilePath);
  Logger().info("User selected folder: {}", result.string());
  
  FolderSelectorResult folder_result;
  folder_result.path = result.string();
  return folder_result;
}

// 选择文件
auto select_file(const FileSelectorParams& params,
                 HWND hwnd)
    -> std::expected<FileSelectorResult, std::string> {
  // 初始化COM
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    return std::unexpected("Failed to initialize COM library");
  }

  // RAII管理COM库的释放
  auto com_guard = std::unique_ptr<void, ComDeleter>(nullptr);

  // 创建文件对话框实例
  IFileOpenDialog* pFileDialog = nullptr;
  hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog,
                        reinterpret_cast<void**>(&pFileDialog));
  if (FAILED(hr)) {
    return std::unexpected("Failed to create file dialog instance");
  }

  // RAII管理COM对象的释放
  auto dialog_guard = std::unique_ptr<IFileOpenDialog, ComPtrDeleter<IFileOpenDialog>>(pFileDialog);

  // 设置选项
  DWORD dwOptions;
  hr = pFileDialog->GetOptions(&dwOptions);
  if (SUCCEEDED(hr)) {
    dwOptions |= FOS_FILEMUSTEXIST;  // 文件必须存在
    if (params.allow_multiple) {
      dwOptions |= FOS_ALLOWMULTISELECT;  // 允许多选
    }
    hr = pFileDialog->SetOptions(dwOptions);
  }
  if (FAILED(hr)) {
    return std::unexpected("Failed to set dialog options");
  }

  // 设置过滤器
  if (!params.filter.empty()) {
    std::wstring filter_wide = Utils::String::FromUtf8(params.filter);

    // 将过滤器字符串转换为COM格式
    std::vector<wchar_t> filter_buffer;
    filter_buffer.reserve(filter_wide.length() + 2);  // +2 for double null termination

    // 复制过滤器字符串并替换 '|' 为 '\0'
    for (wchar_t c : filter_wide) {
      if (c == L'|') {
        filter_buffer.push_back(L'\0');
      } else {
        filter_buffer.push_back(c);
      }
    }
    filter_buffer.push_back(L'\0');  // 结尾双null
    filter_buffer.push_back(L'\0');

    // 设置文件类型过滤器
    hr = pFileDialog->SetFileTypes(
        1, reinterpret_cast<const COMDLG_FILTERSPEC*>(filter_buffer.data()));
    if (FAILED(hr)) {
      Logger().warn("Failed to set file filters, continuing without filter");
    }
  }

  // 设置标题
  if (!params.title.empty()) {
    std::wstring title_wide = Utils::String::FromUtf8(params.title);
    pFileDialog->SetTitle(title_wide.c_str());
  }

  // 显示对话框
  hr = pFileDialog->Show(hwnd);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return std::unexpected("User cancelled the operation");
  }
  if (FAILED(hr)) {
    return std::unexpected("Failed to show dialog");
  }

  // 获取结果
  IShellItemArray* pItemArray = nullptr;
  if (params.allow_multiple) {
    hr = pFileDialog->GetResults(&pItemArray);
  } else {
    IShellItem* pItem = nullptr;
    hr = pFileDialog->GetResult(&pItem);
    if (SUCCEEDED(hr)) {
      // 为单个文件创建一个item array
      hr = SHCreateShellItemArrayFromShellItem(pItem, IID_IShellItemArray,
                                               reinterpret_cast<void**>(&pItemArray));
      pItem->Release();
    }
  }

  if (FAILED(hr)) {
    return std::unexpected("Failed to get dialog result");
  }

  // RAII管理ShellItemArray的释放
  auto item_array_guard =
      std::unique_ptr<IShellItemArray, ComPtrDeleter<IShellItemArray>>(pItemArray);

  // 获取文件数量
  DWORD count = 0;
  hr = pItemArray->GetCount(&count);
  if (FAILED(hr)) {
    return std::unexpected("Failed to get selected items count");
  }

  // 收集所有文件路径
  std::vector<std::filesystem::path> selected_paths;
  selected_paths.reserve(count);

  for (DWORD i = 0; i < count; ++i) {
    IShellItem* pItem = nullptr;
    hr = pItemArray->GetItemAt(i, &pItem);
    if (FAILED(hr)) {
      Logger().warn("Failed to get item at index {}, skipping", i);
      continue;
    }

    // RAII管理ShellItem的释放
    auto item_guard = std::unique_ptr<IShellItem, ComPtrDeleter<IShellItem>>(pItem);

    // 获取路径
    PWSTR pszFilePath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (FAILED(hr)) {
      Logger().warn("Failed to get file path for item at index {}, skipping", i);
      continue;
    }

    // RAII管理字符串的释放
    auto string_guard = std::unique_ptr<wchar_t, CoTaskMemDeleter>(pszFilePath);

    // 添加到结果列表
    selected_paths.emplace_back(pszFilePath);
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
}


}  // namespace Utils::Dialog