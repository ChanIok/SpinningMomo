# ==============================================================================
# Web App Build and Deployment Script
# ==============================================================================
# 此脚本负责处理Web应用资源的构建和部署
# 仅在 Release 模式下执行Web应用构建，Debug 模式使用 Vite 开发服务器

# 获取构建类型（转换为大写）
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)

# 前端构建和部署逻辑
function(setup_web_app_build TARGET_NAME)
    if(BUILD_TYPE_UPPER STREQUAL "RELEASE")
        # Release 模式：构建并复制前端资源
        if(EXISTS ${CMAKE_SOURCE_DIR}/web_app/package.json)
            find_program(NPM_EXECUTABLE npm)
            if(NPM_EXECUTABLE)
                # 添加前端构建目标
                add_custom_target(build_frontend
                    COMMAND ${NPM_EXECUTABLE} install
                    COMMAND ${NPM_EXECUTABLE} run build
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/web_app
                    COMMENT "Building web app with npm for Release..."
                )

                # 复制前端构建产物到输出目录
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/resources
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_SOURCE_DIR}/web_app/dist
                    $<TARGET_FILE_DIR:${TARGET_NAME}>/resources/web
                    COMMENT "Copying web app resources to Release output directory..."
                    DEPENDS build_frontend
                )

                # 确保前端在主程序之前构建
                add_dependencies(${TARGET_NAME} build_frontend)

                message(STATUS "Web app build enabled for Release: npm found at ${NPM_EXECUTABLE}")
            else()
                message(WARNING "npm not found - web app will not be built automatically")
                message(STATUS "Please ensure web_app/dist exists for Release builds or install Node.js")
            endif()
        else()
            message(STATUS "No web_app/package.json found - skipping web app build")
        endif()
    else()
        # Debug 模式：跳过Web应用构建
        message(STATUS "Debug mode: skipping web app build (using Vite dev server)")
    endif()
endfunction()

# 导出函数供主 CMakeLists.txt 调用
# 使用方法: setup_web_app_build(SpinningMomo) 