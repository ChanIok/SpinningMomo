add_executable(${PROJECT_NAME}
    main.cpp
    window_capture.cpp
    gui_manager.cpp
    app.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui::imgui
    imgui_impl
    Microsoft::DirectX-Headers
    Microsoft::DirectXTK
) 