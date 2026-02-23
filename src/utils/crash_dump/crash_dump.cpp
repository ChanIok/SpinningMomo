module;

#include <Windows.h>

#include <DbgHelp.h>

#include <cctype>
#include <cstdlib>

module Utils.CrashDump;

import std;
import Utils.Path;

namespace Utils::CrashDump::Detail {

std::atomic_bool g_installed{false};
std::atomic_flag g_dump_writing{};

struct DumpWriteScope {
  ~DumpWriteScope() { g_dump_writing.clear(std::memory_order_release); }
};

constexpr MINIDUMP_TYPE kDefaultDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules | MiniDumpWithIndirectlyReferencedMemory);

auto current_timestamp() -> std::string {
  SYSTEMTIME now{};
  GetLocalTime(&now);

  return std::format("{:04}{:02}{:02}_{:02}{:02}{:02}", now.wYear, now.wMonth, now.wDay, now.wHour,
                     now.wMinute, now.wSecond);
}

auto sanitize_reason(std::string_view reason) -> std::string {
  std::string safe;
  safe.reserve(reason.size());

  for (const auto ch : reason) {
    const auto uch = static_cast<unsigned char>(ch);
    safe.push_back(std::isalnum(uch) != 0 ? static_cast<char>(uch) : '_');
  }

  return safe.empty() ? "unknown" : safe;
}

auto make_dump_dir() -> std::expected<std::filesystem::path, std::string> {
  auto exe_dir_result = Utils::Path::GetExecutableDirectory();
  if (!exe_dir_result) {
    return std::unexpected("Failed to get executable directory: " + exe_dir_result.error());
  }

  const auto dump_dir = exe_dir_result.value() / "logs" / "dumps";
  if (auto ensure_result = Utils::Path::EnsureDirectoryExists(dump_dir); !ensure_result) {
    return std::unexpected("Failed to create dump directory: " + ensure_result.error());
  }

  return dump_dir;
}

auto build_dump_path(void* exception_pointers, std::string_view reason)
    -> std::expected<std::filesystem::path, std::string> {
  auto dump_dir_result = make_dump_dir();
  if (!dump_dir_result) {
    return std::unexpected(dump_dir_result.error());
  }

  auto* exception = static_cast<EXCEPTION_POINTERS*>(exception_pointers);
  const auto code = (exception && exception->ExceptionRecord)
                        ? exception->ExceptionRecord->ExceptionCode
                        : static_cast<DWORD>(0);

  const auto filename =
      std::format("crash_{}_pid{}_tid{}_{}_0x{:08X}.dmp", current_timestamp(),
                  GetCurrentProcessId(), GetCurrentThreadId(), sanitize_reason(reason), code);

  return dump_dir_result.value() / filename;
}

auto write_dump_internal(void* exception_pointers, std::string_view reason)
    -> std::expected<std::filesystem::path, std::string> {
  if (g_dump_writing.test_and_set(std::memory_order_acquire)) {
    return std::unexpected("Dump writing is already in progress");
  }
  DumpWriteScope scope_guard{};

  auto dump_path_result = build_dump_path(exception_pointers, reason);
  if (!dump_path_result) {
    return std::unexpected(dump_path_result.error());
  }

  const auto dump_path = dump_path_result.value();
  const auto dump_path_w = dump_path.wstring();

  HANDLE dump_file = CreateFileW(dump_path_w.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (dump_file == INVALID_HANDLE_VALUE) {
    return std::unexpected("CreateFileW failed with error: " + std::to_string(GetLastError()));
  }

  auto* exception = static_cast<EXCEPTION_POINTERS*>(exception_pointers);
  MINIDUMP_EXCEPTION_INFORMATION exception_info{
      .ThreadId = GetCurrentThreadId(),
      .ExceptionPointers = exception,
      .ClientPointers = FALSE,
  };

  BOOL ok =
      MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump_file, kDefaultDumpType,
                        exception ? &exception_info : nullptr, nullptr, nullptr);

  const auto last_error = GetLastError();
  CloseHandle(dump_file);

  if (ok == FALSE) {
    return std::unexpected("MiniDumpWriteDump failed with error: " + std::to_string(last_error));
  }

  return dump_path;
}

auto __stdcall unhandled_exception_filter(EXCEPTION_POINTERS* exception_pointers) -> LONG {
  auto result = write_dump_internal(exception_pointers, "seh");
  if (!result) {
    auto message = "MiniDump failed: " + result.error() + "\n";
    OutputDebugStringA(message.c_str());
  }
  return EXCEPTION_EXECUTE_HANDLER;
}

[[noreturn]] auto terminate_handler() -> void {
  auto result = write_dump_internal(nullptr, "terminate");
  if (!result) {
    auto message = "MiniDump failed in terminate: " + result.error() + "\n";
    OutputDebugStringA(message.c_str());
  }
  std::abort();
}

}  // namespace Utils::CrashDump::Detail

namespace Utils::CrashDump {

auto install() -> void {
  if (Detail::g_installed.exchange(true, std::memory_order_acq_rel)) {
    return;
  }

  SetUnhandledExceptionFilter(Detail::unhandled_exception_filter);
  std::set_terminate(Detail::terminate_handler);
}

auto write_dump(void* exception_pointers, std::string_view reason)
    -> std::expected<std::filesystem::path, std::string> {
  return Detail::write_dump_internal(exception_pointers, reason);
}

}  // namespace Utils::CrashDump
