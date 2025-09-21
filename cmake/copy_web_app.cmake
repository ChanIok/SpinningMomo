# ==============================================================================
# Web App Copy Script
# ==============================================================================
# 此脚本负责处理Web应用资源的复制 仅在 Release 模式下复制已构建的前端资源，Debug 模式使用 Vite 开发服务器

# 获取构建类型（转换为大写）
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)

# 前端资源复制逻辑
function(setup_web_app_copy TARGET_NAME)
  if(BUILD_TYPE_UPPER STREQUAL "RELEASE")
    # Release 模式：直接复制已构建的前端资源
    if(EXISTS ${CMAKE_SOURCE_DIR}/web/dist)
      # 复制前端构建产物到输出目录
      add_custom_command(
        TARGET ${TARGET_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:${TARGET_NAME}>/resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/web/dist
                $<TARGET_FILE_DIR:${TARGET_NAME}>/resources/web
        COMMENT
          "Copying pre-built web app resources to Release output directory...")

      message(
        STATUS
          "Web app copy enabled for Release: using pre-built resources from web/dist"
      )
    else()
      message(
        WARNING "web/dist not found - web app resources will not be copied")
      message(STATUS "Please ensure web/dist exists for Release builds")
    endif()
  else()
    # Debug 模式：跳过Web应用复制
    message(STATUS "Debug mode: skipping web app copy (using Vite dev server)")
  endif()
endfunction()

# 导出函数供主 CMakeLists.txt 调用 使用方法: setup_web_app_copy(SpinningMomo)
