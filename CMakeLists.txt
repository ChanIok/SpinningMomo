cmake_minimum_required(VERSION 3.15)
project(SpinningMomo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows平台相关宏定义
if(WIN32)
    # 启用manifest处理
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST /MANIFESTUAC:level='requireAdministrator'")
    add_compile_definitions(
        UNICODE
        _UNICODE
    )

    # MSVC编译器特有选项
    if(MSVC)
        add_compile_options(/utf-8 /EHsc /await)

        # 根据构建类型添加不同的优化选项
        string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
        if(BUILD_TYPE_UPPER STREQUAL "RELEASE")
            # 设置静态运行时库
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

            # 核心优化选项
            add_compile_options(/O2 /GL)
            add_link_options(/LTCG /OPT:REF /OPT:ICF)
            
            # 定义Release宏
            add_compile_definitions(NDEBUG)
        else()
            # 设置调试运行时库
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
            
            # Debug模式核心选项
            add_compile_options(/Od /Zi /RTC1)
            add_compile_definitions(_DEBUG)
        endif()

        add_compile_definitions(WINRT_LEAN_AND_MEAN)
    endif()
endif()

# 创建Windows GUI应用程序
add_executable(SpinningMomo WIN32 
    src/main.cpp
    src/event_handler.cpp
    src/constants.cpp
    src/window_utils.cpp
    src/tray_icon.cpp
    src/menu_window.cpp
    src/preview_window.cpp
    src/notification_window.cpp
    src/notification_manager.cpp
    src/config_manager.cpp
    src/window_capturer.cpp
    src/overlay_window.cpp
    src/logger.cpp
    src/win_timer.cpp
    src/letterbox_window.cpp
    src/resource.rc
    app.manifest
)

# 添加Windows库
target_link_libraries(SpinningMomo PRIVATE
    dwmapi       # DWM API
    windowsapp   # Windows Runtime API
    RuntimeObject # WinRT support
    d3d11        # Direct3D 11
    dxgi         # DXGI
    d3dcompiler  # D3D Compiler
    shell32      # Windows Shell API
    Shlwapi      # Shell Light-Weight API
)