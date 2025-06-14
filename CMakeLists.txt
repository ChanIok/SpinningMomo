cmake_minimum_required(VERSION 3.30 FATAL_ERROR)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")

project(SpinningMomo LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows平台相关宏定义
if(WIN32)
    # 启用manifest处理
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST /MANIFESTUAC:level='requireAdministrator'")
    add_compile_definitions(
        NOMINMAX
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
    )

    # MSVC编译器特有选项
    if(MSVC)
        add_compile_options(/std:c++latest /utf-8 /EHsc /Zc:preprocessor
            /wd5050 # 抑制 C5050 (_UTF8 不匹配) 警告
            /wd4251 # 抑制 C4251 警告
            /wd4275 # 抑制 C4275 警告
        )

        # 为 IntelliSense 指定 .ifc 文件搜索路径
        add_compile_options(/ifcSearchDir ${CMAKE_BINARY_DIR}/ifc)

        # 根据构建类型添加不同的优化选项
        string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
        if(BUILD_TYPE_UPPER STREQUAL "RELEASE")
            # 设置静态运行时库
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

            # 核心优化选项
            add_compile_options(/O2 /GL)
            add_link_options(/LTCG /OPT:REF /OPT:ICF /NODEFAULTLIB:msvcprt.lib)

            # 定义Release宏
            add_compile_definitions(NDEBUG)
        else()
            # 设置调试运行时库
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")

            # Debug模式核心加速选项
            add_compile_options(
                /Od
                /Zi
                /RTC1
                /MP
                /Gm-
                /bigobj
            )

            # Debug模式链接加速选项
            add_link_options(
                /INCREMENTAL
                /DEBUG:FASTLINK
            )

            # Debug模式宏定义
            add_compile_definitions(_DEBUG)
        endif()
    endif()
endif()

# 创建可执行文件目标
add_executable(SpinningMomo src/main.cpp)

file(GLOB_RECURSE MODULE_FILES "src/*.ixx")
file(GLOB_RECURSE MODULE_IMPL_FILES "src/*.cpp" EXCLUDE src/main.cpp)

# 添加模块文件
target_sources(SpinningMomo PRIVATE
    FILE_SET CXX_MODULES FILES
    ${MODULE_FILES}
)

# 添加模块实现文件
target_sources(SpinningMomo PRIVATE
    ${MODULE_IMPL_FILES}
)

# 启用 C++ 模块依赖扫描 (包括标准库头文件单元)
set_property(TARGET SpinningMomo PROPERTY CXX_SCAN_FOR_MODULES YES)

# 添加项目头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src # 主目录
)

# 添加Windows库
target_link_libraries(SpinningMomo PRIVATE
    dwmapi # DWM API
    windowsapp # Windows Runtime API
    RuntimeObject # WinRT support
    d3d11 # Direct3D 11
    dxgi # DXGI
    d3dcompiler # D3D Compiler
    shell32 # Windows Shell API
    Shlwapi # Shell Light-Weight API
)

# 复制 .ifc 文件到指定目录供 IntelliSense 使用
add_custom_command(TARGET SpinningMomo
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -Dsource_dir=${CMAKE_BINARY_DIR}/CMakeFiles/SpinningMomo.dir/
    -Dstd_module_dir=${CMAKE_BINARY_DIR}/CMakeFiles/__cmake_cxx23.dir/
    -Dtarget_dir=${CMAKE_BINARY_DIR}/ifc
    -P ${CMAKE_SOURCE_DIR}/cmake/copy_ifc_files.cmake
    COMMENT "Copying .ifc files for IntelliSense..."
)
