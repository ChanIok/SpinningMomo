cmake_minimum_required(VERSION 3.10)
project(SpinningMomo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置源文件的编码为UTF-8
if(MSVC)
    add_compile_options(/utf-8 /EHsc /await)
    add_compile_definitions(WINRT_LEAN_AND_MEAN)
endif()

# ONNX Runtime配置
set(ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime")

# 检查ONNX Runtime路径
if(NOT EXISTS "${ONNXRUNTIME_ROOT}")
    message(FATAL_ERROR "ONNX Runtime directory not found at ${ONNXRUNTIME_ROOT}")
endif()

# 添加ONNX Runtime的include和lib路径
include_directories(${ONNXRUNTIME_ROOT}/include)
link_directories(${ONNXRUNTIME_ROOT}/lib)

# 创建Windows GUI应用程序
add_executable(SpinningMomo WIN32 
    src/main.cpp
    src/constants.cpp
    src/window_utils.cpp
    src/tray_icon.cpp
    src/menu_window.cpp
    src/preview_window.cpp
    src/notification_window.cpp
    src/notification_manager.cpp
    src/config_manager.cpp
    src/window_capturer.cpp
    src/parameter_tracker.cpp
    src/parameter_ocr.cpp
    src/resource.rc  # 添加资源文件
)

# 添加Windows库
target_link_libraries(SpinningMomo PRIVATE
    dwmapi       # DWM API
    windowsapp   # Windows Runtime API
    d3d11        # Direct3D 11
    shell32      # Windows Shell API
    dxgi         # DXGI
    RuntimeObject # WinRT support
    d3dcompiler  # D3D Compiler
    Shcore       # Shell Core API
    Shlwapi      # Shell Light-Weight API
    onnxruntime  # ONNX Runtime
)

# 复制ONNX Runtime DLL到输出目录
add_custom_command(TARGET SpinningMomo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
        $<TARGET_FILE_DIR:SpinningMomo>
)