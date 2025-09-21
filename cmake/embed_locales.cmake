# 嵌入所有语言文件的函数
# 使用add_custom_command实现增量构建
function(embed_all_locales)
    # 确保生成目录存在
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated/locales)
    
    # 中文语言文件
    set(zh_input "${CMAKE_SOURCE_DIR}/src/locales/zh-CN.json")
    set(zh_output "${CMAKE_BINARY_DIR}/generated/locales/zh_cn_embedded.ixx")
    add_custom_command(
        OUTPUT "${zh_output}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=${zh_input}
            -DOUTPUT_FILE=${zh_output}
            -DMODULE_NAME=Core.I18n.Embedded.ZhCN
            -DVARIABLE_NAME=zh_cn_json
            -P ${CMAKE_SOURCE_DIR}/cmake/generate_locale.cmake
        DEPENDS "${zh_input}" "${CMAKE_SOURCE_DIR}/cmake/generate_locale.cmake"
        COMMENT "Generating zh-CN embedded locale"
        VERBATIM
    )
    
    # 英文语言文件
    set(en_input "${CMAKE_SOURCE_DIR}/src/locales/en-US.json")
    set(en_output "${CMAKE_BINARY_DIR}/generated/locales/en_us_embedded.ixx")
    add_custom_command(
        OUTPUT "${en_output}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=${en_input}
            -DOUTPUT_FILE=${en_output}
            -DMODULE_NAME=Core.I18n.Embedded.EnUS
            -DVARIABLE_NAME=en_us_json
            -P ${CMAKE_SOURCE_DIR}/cmake/generate_locale.cmake
        DEPENDS "${en_input}" "${CMAKE_SOURCE_DIR}/cmake/generate_locale.cmake"
        COMMENT "Generating en-US embedded locale"
        VERBATIM
    )
    
    # 返回生成的文件列表给父作用域
    set(EMBEDDED_LOCALE_SOURCES
        "${zh_output}"
        "${en_output}"
        PARENT_SCOPE
    )
    
    message(STATUS "Configured embedded locale modules for incremental build")
endfunction()
