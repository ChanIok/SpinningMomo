# ===== 基础配置 =====
cmake_minimum_required(VERSION 3.10)
project(SpinningMomo)

# 导出编译命令
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ===== 编译器和语言设置 =====
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC 特定配置
if(MSVC)
    add_compile_options(/utf-8 /EHsc /await)
    add_compile_definitions(
        NOMINMAX
        WINRT_LEAN_AND_MEAN
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        # spdlog配置
        SPDLOG_WCHAR_TO_UTF8_SUPPORT
        SPDLOG_WCHAR_FILENAMES
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
        SPDLOG_HEADER_ONLY
    )
endif()

# ===== 依赖项配置 =====
# 设置第三方库根目录
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# WinDivert 配置
set(WINDIVERT_DIR "${THIRD_PARTY_DIR}/WinDivert")
set(WINDIVERT_INCLUDE_DIR "${WINDIVERT_DIR}/include")
set(WINDIVERT_LIB_DIR "${WINDIVERT_DIR}/x64")

# vcpkg依赖包
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)

# ===== 源文件配置 =====
# 项目源文件
file(GLOB_RECURSE SETTINGS_SOURCES "src/core/settings/*.cpp")
file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp")
file(GLOB_RECURSE MEDIA_SOURCES "src/media/**/*.cpp")
file(GLOB_RECURSE SNIFFER_SOURCES "src/sniffer/**/*.cpp")
file(GLOB MAIN_SOURCES 
    "src/main.cpp"
    "src/constants.cpp"
)

# ===== 目标配置 =====
# 创建可执行文件
add_executable(${PROJECT_NAME} WIN32 
    ${MAIN_SOURCES}
    ${SETTINGS_SOURCES}
    ${CORE_SOURCES}
    ${MEDIA_SOURCES}
    ${SNIFFER_SOURCES}
    src/sniffer/core/applink.c
    src/resource.rc
)

# 添加项目头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src          # 主目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core     # core模块
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/settings  # settings模块
    ${WINDIVERT_INCLUDE_DIR}                 # WinDivert头文件
)

# ===== 链接配置 =====
target_link_libraries(${PROJECT_NAME} PRIVATE
    # Windows系统库
    dwmapi           # DWM API
    windowsapp       # Windows Runtime API
    d3d11            # Direct3D 11
    shell32          # Windows Shell API
    dxgi             # DXGI
    RuntimeObject    # WinRT support
    d3dcompiler      # D3D Compiler
    Shcore           # Shell Core API
    Shlwapi          # Shell Light-Weight API
    ole32            # COM API
    
    # 第三方库
    OpenSSL::SSL                         # OpenSSL SSL库
    OpenSSL::Crypto                      # OpenSSL Crypto库
    unofficial::uwebsockets::uwebsockets # WebSocket库
    ZLIB::ZLIB                          # zlib压缩库
    nlohmann_json::nlohmann_json        # JSON库
    SQLite::SQLite3                     # SQLite3
    spdlog::spdlog_header_only         # spdlog (header-only)
    fmt::fmt                           # fmt库
    xxHash::xxhash                     # xxHash哈希库
    
    # WinDivert
    "${WINDIVERT_LIB_DIR}/WinDivert.lib"
)

# ===== 安装和部署配置 =====
# 设置输出目录变量
set(OUTPUT_DIR $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# 创建必要的目录结构
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # 数据目录
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}/data/thumbnails"
    
    # 复制 WinDivert DLL 和驱动文件到输出目录
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WINDIVERT_LIB_DIR}/WinDivert.dll"
        "${WINDIVERT_LIB_DIR}/WinDivert64.sys"
        "${OUTPUT_DIR}"

    # 复制 OpenSSL DLL 到输出目录
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpenSSL::SSL>
        $<TARGET_FILE:OpenSSL::Crypto>
        "${OUTPUT_DIR}"
)