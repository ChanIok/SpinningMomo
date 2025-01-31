# ===== 基础配置 =====
cmake_minimum_required(VERSION 3.10)

# vcpkg配置
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" 
    CACHE STRING "Vcpkg toolchain file")

# 项目信息
project(SpinningMomo)

# ===== 编译器和语言设置 =====
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /EHsc /await)
    add_compile_definitions(WINRT_LEAN_AND_MEAN)
endif()

# ===== 依赖项配置 =====
# ONNX Runtime
set(ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime")
if(NOT EXISTS "${ONNXRUNTIME_ROOT}")
    message(FATAL_ERROR "ONNX Runtime directory not found at ${ONNXRUNTIME_ROOT}")
endif()
include_directories(${ONNXRUNTIME_ROOT}/include)
link_directories(${ONNXRUNTIME_ROOT}/lib)

# vcpkg依赖
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# ===== 源文件配置 =====
# 收集源文件
file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp")
file(GLOB_RECURSE TRACKER_SOURCES "src/tracker/*.cpp")
file(GLOB_RECURSE MEDIA_SOURCES "src/media/**/*.cpp")
file(GLOB MAIN_SOURCES 
    "src/main.cpp"
    "src/constants.cpp"
)

# ===== 目标配置 =====
# 创建可执行文件
add_executable(SpinningMomo WIN32 
    ${MAIN_SOURCES}
    ${CORE_SOURCES}
    ${TRACKER_SOURCES}
    ${MEDIA_SOURCES}
    src/resource.rc
)

# 添加头文件路径
target_include_directories(SpinningMomo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src          # 主目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core     # core模块
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tracker  # tracker模块
)

# ===== 链接库配置 =====
target_link_libraries(SpinningMomo PRIVATE
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
    onnxruntime              # ONNX Runtime
    httplib::httplib         # HTTP库
    nlohmann_json::nlohmann_json  # JSON库
    SQLite::SQLite3          # SQLite3
    spdlog::spdlog          # 日志库
)

# ===== 安装和部署配置 =====
# 复制ONNX Runtime DLL
add_custom_command(TARGET SpinningMomo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
        $<TARGET_FILE_DIR:SpinningMomo>
)

# 复制ONNX模型文件
add_custom_command(TARGET SpinningMomo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:SpinningMomo>/models
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/models/model.onnx"
        $<TARGET_FILE_DIR:SpinningMomo>/models/model.onnx
)