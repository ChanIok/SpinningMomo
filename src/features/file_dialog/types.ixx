module;

export module Features.FileDialog.Types;

import std;

namespace Features::FileDialog::Types {

export struct FileSelectorParams {
  std::string title;
  std::string filter;
  bool allow_multiple = false;
};

export struct FileSelectorResult {
  std::vector<std::string> paths;
};

export struct FolderSelectorParams {
  std::string title;
};

export struct FolderSelectorResult {
  std::string path;
};

}  // namespace Features::FileDialog::Types