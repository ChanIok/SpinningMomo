add_library(imgui_impl STATIC
    imgui_impl_win32.cpp
    imgui_impl_dx11.cpp
)

target_include_directories(imgui_impl
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/include
)

target_link_libraries(imgui_impl
    PUBLIC imgui::imgui
    PRIVATE Microsoft::DirectX-Headers
) 