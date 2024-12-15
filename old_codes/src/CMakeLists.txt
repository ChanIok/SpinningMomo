add_executable(${PROJECT_NAME}
    main.cpp
    window_capture.cpp
    gui_manager.cpp
    app.cpp
    capture_test.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui::imgui
    Microsoft::DirectX-Headers
    Microsoft::DirectXTK
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
    RuntimeObject.lib
    dwmapi.lib
    windowsapp.lib
)

target_compile_options(${PROJECT_NAME} PRIVATE /EHsc /await)
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    WINVER=0x0A00 
    _WIN32_WINNT=0x0A00
) 