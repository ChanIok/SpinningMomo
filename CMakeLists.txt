cmake_minimum_required(VERSION 3.20)
project(NikkiLens VERSION 0.1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 设置Windows子系统和字符集
if(MSVC)
    # 使用 UTF-8 编码
    add_compile_options(/utf-8 /EHsc)
    
    # 定义 WIN32_LEAN_AND_MEAN 来减少 Windows 头文件包含
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    
    # 设置为 Windows 子系统，但不使用 WinRT 编译
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

# vcpkg配置
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

# 查找依赖包
find_package(imgui CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)
find_package(DirectXTK CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 添加源代码目录
add_subdirectory(src)

# 设置安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)