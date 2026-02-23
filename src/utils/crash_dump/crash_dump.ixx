module;

export module Utils.CrashDump;

import std;

namespace Utils::CrashDump {

// 安装崩溃转储处理器（SEH + terminate）
export auto install() -> void;

// 手动写入转储（exception_pointers 可传 nullptr）
export auto write_dump(void* exception_pointers, std::string_view reason)
    -> std::expected<std::filesystem::path, std::string>;

}  // namespace Utils::CrashDump
