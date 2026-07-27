add_rules("mode.debug", "mode.release")

-- 引入自定义任务
includes("tasks/release.lua")
includes("tasks/vs.lua")
includes("tests")

-- 设置C++23标准
set_languages("c++23")

-- 默认使用 LLVM 工具链，可通过 --toolchain 覆盖
set_config("toolchain", "clang-cl[llvm]")

-- 统一源文件编码
add_cxflags("/utf-8", "/bigobj")

-- 设置运行时库
set_runtimes(is_mode("debug") and "MD" or "MT")

set_policy("package.requires_lock", true)

-- 锁定 vcpkg 注册表快照（2026-05-21）
add_requireconfs("vcpkg::*", {configs = {baseline = "1ea949145db9db7c9b254062f94acdaeed947767"}})

-- 添加vcpkg依赖包
add_requires("vcpkg::uwebsockets", "vcpkg::spdlog", "vcpkg::asio", "vcpkg::reflectcpp", 
             "vcpkg::webview2", "vcpkg::wil", "vcpkg::xxhash", "vcpkg::sqlitecpp", "vcpkg::libwebp", "vcpkg::zlib")

target("SpinningMomo")
    -- 设置为Windows可执行文件
    set_kind("binary")
    set_plat("windows")
    set_arch("x64")
    -- 设置预编译头文件
    set_pcxxheader("src/pch.hpp")
    add_cxflags("clang_cl::-Wno-microsoft-include")

    -- Release 也保留调试符号，便于分析生产崩溃 dump
    if is_mode("release") then
        set_symbols("debug")
        add_ldflags("/DEBUG:FULL", {force = true})
        add_ldflags("/NODEFAULTLIB:libucrt.lib", {force = true})
        add_ldflags("/DEFAULTLIB:ucrt.lib", {force = true})
    end
    
    -- Windows特定宏定义
    add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN", "_WIN32_WINNT=0x0A00", "SPDLOG_COMPILED_LIB", "yyjson_api_inline=yyjson_inline")
    
    -- 添加包含目录
    add_includedirs("src")
    add_includedirs("third_party/dkm/include")
    
    -- 添加源文件
    add_files("src/main.cpp")
    add_files("src/**.cpp")
    add_files("resources/*.rc")
    
    -- 链接vcpkg包
    add_packages("vcpkg::uwebsockets", "vcpkg::spdlog", "vcpkg::asio", "vcpkg::reflectcpp", 
                 "vcpkg::webview2", "vcpkg::wil", "vcpkg::xxhash", "vcpkg::sqlitecpp", "vcpkg::libwebp", "vcpkg::zlib")
    
    -- Windows系统库
    add_links("dwmapi", "dcomp", "windowsapp", "RuntimeObject", "d3d11", "dxgi", "d3dcompiler", 
              "d2d1", "dwrite", "shell32", "Shlwapi", "gdi32", "user32", "Ws2_32", "Secur32", 
              "Advapi32", "Bcrypt", "Dbghelp", "Userenv", "mf", "mfplat", "mfreadwrite", "mfuuid", "strmiids")

    -- vcpkg的传递依赖
    add_links("fmt", "yyjson", "sqlite3", "uSockets", "libuv")
