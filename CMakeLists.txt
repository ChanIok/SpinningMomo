cmake_minimum_required(VERSION 3.10)
project(SpinningMomoV2)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置源文件的编码为UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 创建Windows GUI应用程序
add_executable(screen_rotator WIN32 src/main.cpp)

# 添加Windows库
target_link_libraries(screen_rotator shell32)