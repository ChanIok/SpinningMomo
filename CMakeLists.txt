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
# vcpkg配置
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" 
    CACHE STRING "Vcpkg toolchain file")

# 设置第三方库根目录
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# ONNX Runtime配置
set(ONNXRUNTIME_ROOT "${THIRD_PARTY_DIR}/onnxruntime")
if(NOT EXISTS "${ONNXRUNTIME_ROOT}")
    message(FATAL_ERROR "ONNX Runtime directory not found at ${ONNXRUNTIME_ROOT}")
endif()

# 添加第三方库头文件路径
include_directories(
    # ONNX Runtime
    ${ONNXRUNTIME_ROOT}/include
)

# 添加第三方库链接路径
link_directories(
    ${ONNXRUNTIME_ROOT}/lib
)

# vcpkg依赖包
find_package(ZLIB REQUIRED)
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
file(GLOB_RECURSE TRACKER_SOURCES "src/tracker/*.cpp")
file(GLOB_RECURSE MEDIA_SOURCES "src/media/**/*.cpp")
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
    ${TRACKER_SOURCES}
    ${MEDIA_SOURCES}
    src/resource.rc
)

# 添加项目头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src          # 主目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core     # core模块
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/settings  # settings模块
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tracker  # tracker模块
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
    
    # 第三方库
    onnxruntime                          # ONNX Runtime
    unofficial::uwebsockets::uwebsockets # WebSocket库
    ZLIB::ZLIB                          # zlib压缩库
    nlohmann_json::nlohmann_json        # JSON库
    SQLite::SQLite3                     # SQLite3
    spdlog::spdlog_header_only         # spdlog (header-only)
    fmt::fmt                           # fmt库
    xxHash::xxhash                     # xxHash哈希库
)

# ===== 安装和部署配置 =====
# 设置输出目录变量
set(OUTPUT_DIR $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# 复制运行时依赖
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # ONNX Runtime
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
        "${OUTPUT_DIR}"
)

# 创建必要的目录结构
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # 模型目录
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/models/model.onnx"
        "${OUTPUT_DIR}/models/model.onnx"

    # 数据目录
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}/data/thumbnails"
)