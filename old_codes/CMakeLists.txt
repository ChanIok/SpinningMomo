cmake_minimum_required(VERSION 3.20)
project(NikkiLens VERSION 0.1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 确保 Windows SDK 版本满足要求（Windows 10 1803 或更高）
set(CMAKE_SYSTEM_VERSION "10.0.17763.0")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 设置Windows子系统和字符集
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_UNICODE UNICODE)
    add_compile_definitions(WINVER=0x0A00 _WIN32_WINNT=0x0A00)  # 设置 Windows 10 目标
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

# vcpkg配置
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

# 查找依赖包
find_package(imgui CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)
find_package(DirectXTK CONFIG REQUIRED)

# 添加源代码目录
add_subdirectory(src)

# 设置安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)