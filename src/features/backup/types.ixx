module;

export module Features.Backup.Types;

import std;

namespace Features::Backup::Types {

export struct ExportParams {
  std::string destination_directory;
};

export struct ExportResult {
  std::string backup_path;
  std::string app_version;
  std::int64_t created_at = 0;
  std::uint64_t size = 0;
};

export struct RestoreParams {
  std::string backup_path;
};

export struct RestoreResult {
  bool scheduled = false;
};

}  // namespace Features::Backup::Types
