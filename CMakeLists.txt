cmake_minimum_required(VERSION 3.30 FATAL_ERROR)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")

project(SpinningMomo LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找依赖包
find_package(spdlog CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(reflectcpp CONFIG REQUIRED)
find_package(unofficial-webview2 CONFIG REQUIRED)
find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(WebP CONFIG REQUIRED)

# Windows平台和MSVC编译器配置
if(WIN32 AND MSVC)
  # 启用manifest处理
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST /MANIFESTUAC:level='requireAdministrator'"
  )

  # Windows平台宏定义
  add_compile_definitions(NOMINMAX UNICODE _UNICODE WIN32_LEAN_AND_MEAN
                          _WIN32_WINNT=0x0A00)

  # MSVC编译器选项
  add_compile_options(
    /std:c++latest
    /utf-8
    /EHsc
    /Zc:preprocessor
    /wd5050 # 抑制 C5050 (_UTF8 不匹配) 警告
    /wd4251 # 抑制 C4251 警告
    /wd4275 # 抑制 C4275 警告
    /wd4530 # 抑制 C4530 警告
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
    add_compile_options(/Od /Zi /RTC1 /MP /Gm- /bigobj)

    # Debug模式链接加速选项
    add_link_options(/INCREMENTAL /DEBUG:FASTLINK)

    # Debug模式宏定义
    add_compile_definitions(_DEBUG)
  endif()
endif()

# ============================================================================
# 语言文件嵌入系统
# ============================================================================

# 嵌入语言文件
include(${CMAKE_SOURCE_DIR}/cmake/embed_locales.cmake)
embed_all_locales()

# 创建自定义目标来管理所有生成的语言文件 这确保了语言文件会在需要时生成，并且支持增量构建
add_custom_target(
  generate_locales
  DEPENDS ${EMBEDDED_LOCALE_SOURCES}
  COMMENT "Ensuring all locale modules are generated")

# ============================================================================
# 主目标创建
# ============================================================================

# 创建Windows GUI应用程序
add_executable(SpinningMomo WIN32 src/main.cpp resources/app.rc
                                  resources/app.manifest)

# 确保在编译主目标之前生成语言文件 这是关键的依赖关系，确保模块系统能找到生成的.ixx文件
add_dependencies(SpinningMomo generate_locales)

# ============================================================================
# 源文件收集
# ============================================================================

file(GLOB_RECURSE MODULE_FILES "src/*.ixx")
file(GLOB_RECURSE MODULE_IMPL_FILES "src/*.cpp" EXCLUDE src/main.cpp)

# ============================================================================
# 模块文件配置
# ============================================================================

# 添加模块文件（包括源文件和生成的嵌入语言文件）
# 注意：EMBEDDED_LOCALE_SOURCES包含了由add_custom_command生成的.ixx文件
target_sources(SpinningMomo PRIVATE FILE_SET CXX_MODULES FILES ${MODULE_FILES}
                                    ${EMBEDDED_LOCALE_SOURCES})

# 添加模块实现文件
target_sources(SpinningMomo PRIVATE ${MODULE_IMPL_FILES})

# 启用 C++ 模块依赖扫描 (包括标准库头文件单元)
set_property(TARGET SpinningMomo PROPERTY CXX_SCAN_FOR_MODULES YES)

# 配置预编译头（仅在Debug模式下启用）
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_precompile_headers(SpinningMomo PRIVATE src/pch.hpp)
endif()

# 添加项目头文件路径
target_include_directories(
  ${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src # 主目录
)

# 添加Windows库
target_link_libraries(
  SpinningMomo
  PRIVATE dwmapi # DWM API
          windowsapp # Windows Runtime API
          RuntimeObject # WinRT support
          d3d11 # Direct3D 11
          dxgi # DXGI
          d3dcompiler # D3D Compiler
          d2d1 # Direct2D
          dwrite # DirectWrite
          shell32 # Windows Shell API
          Shlwapi # Shell Light-Weight API
          spdlog::spdlog
          asio::asio
          reflectcpp::reflectcpp
          unofficial::webview2::webview2
          unofficial::uwebsockets::uwebsockets
          WIL::WIL
          xxHash::xxhash
          SQLiteCpp
          WebP::webp)

# Web应用资源复制
include(${CMAKE_SOURCE_DIR}/cmake/copy_web_app.cmake)
setup_web_app_copy(SpinningMomo)

add_custom_command(
  TARGET SpinningMomo
  POST_BUILD
  # 复制 .ifc 文件到指定目录供 IntelliSense 使用
  COMMAND
    ${CMAKE_COMMAND}
    -Dsource_dir=${CMAKE_BINARY_DIR}/CMakeFiles/SpinningMomo.dir/
    -Dstd_module_dir=${CMAKE_BINARY_DIR}/CMakeFiles/__cmake_cxx23.dir/
    -Dtarget_dir=${CMAKE_BINARY_DIR}/ifc -P
    ${CMAKE_SOURCE_DIR}/cmake/copy_ifc_files.cmake)
