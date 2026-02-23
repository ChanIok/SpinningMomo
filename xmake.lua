add_rules("mode.debug", "mode.release")

-- 引入自定义任务
includes("tasks/build-all.lua")
includes("tasks/release.lua")
includes("tasks/vs.lua")

-- 设置C++23标准
set_languages("c++23")

-- 统一源文件编码
add_cxflags("/utf-8")

-- 设置运行时库
set_runtimes(is_mode("debug") and "MD" or "MT")

set_policy("package.requires_lock", true)

-- 添加vcpkg依赖包
add_requires("vcpkg::uwebsockets", "vcpkg::spdlog", "vcpkg::asio", "vcpkg::reflectcpp", 
             "vcpkg::webview2", "vcpkg::wil", "vcpkg::xxhash", "vcpkg::sqlitecpp", "vcpkg::libwebp", "vcpkg::zlib")

-- ViGEmClient 静态库（虚拟手柄）
target("ViGEmClient")
    set_kind("static")
    set_languages("c11")
    add_files("third_party/ViGEmClient/src/ViGEmClient.cpp")
    add_includedirs("third_party/ViGEmClient/include", {public = true})
    add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN")
    add_links("setupapi", "hid")

target("SpinningMomo")
    -- 设置为Windows可执行文件
    set_kind("binary")
    set_plat("windows")
    set_arch("x64")
    
    -- 启用C++模块支持
    set_policy("build.c++.modules", true)
    -- set_policy("build.c++.modules.non_cascading_changes", true)

    -- Release 也保留调试符号，便于分析生产崩溃 dump
    if is_mode("release") then
        set_symbols("debug")
        add_ldflags("/DEBUG:FULL", {force = true})
    end
    
    -- Windows特定宏定义
    add_defines("NOMINMAX", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN", "_WIN32_WINNT=0x0A00", "SPDLOG_COMPILED_LIB", "yyjson_api_inline=yyjson_inline")
    
    -- 添加包含目录
    add_includedirs("src")
    
    -- 添加源文件
    add_files("src/main.cpp")
    add_files("src/**.cpp", "src/**.ixx")
    add_files("resources/*.rc", "resources/*.manifest")
    
    -- 链接vcpkg包
    add_packages("vcpkg::uwebsockets", "vcpkg::spdlog", "vcpkg::asio", "vcpkg::reflectcpp", 
                 "vcpkg::webview2", "vcpkg::wil", "vcpkg::xxhash", "vcpkg::sqlitecpp", "vcpkg::libwebp", "vcpkg::zlib")
    
    -- Windows系统库
    add_links("dwmapi", "dcomp", "windowsapp", "RuntimeObject", "d3d11", "dxgi", "d3dcompiler", 
              "d2d1", "dwrite", "shell32", "Shlwapi", "gdi32", "user32", "Ws2_32", "Secur32", 
              "Advapi32", "Dbghelp", "Userenv", "mf", "mfplat", "mfreadwrite", "mfuuid", "strmiids")

    -- vcpkg的传递依赖
    add_links("fmt", "yyjson", "sqlite3", "uSockets", "libuv")

    -- ViGEmClient（虚拟手柄）
    add_deps("ViGEmClient")
    add_links("setupapi", "hid")
