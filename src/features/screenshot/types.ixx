module;

export module Features.Screenshot.Folder.Types;

import std;

namespace Features::Screenshot::Folder::Types {

export struct SelectParams {};

export struct SelectResult {
  std::string path;
};

}  // namespace Features::Screenshot::Folder::Types
