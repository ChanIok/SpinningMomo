#pragma once

namespace Features::Backup::Types {

struct ExportParams {
  std::string destination_directory;
};

struct ExportResult {
  std::string backup_path;
  std::string app_version;
  std::int64_t created_at = 0;
  std::uint64_t size = 0;
};

struct RestoreParams {
  std::string backup_path;
};

struct RestoreResult {
  bool scheduled = false;
};

}  // namespace Features::Backup::Types
