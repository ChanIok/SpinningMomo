function(embed_json_as_module input_file output_file module_name variable_name)
    # 读取JSON文件内容
    file(READ ${input_file} json_content)
    
    # 使用Raw字符串字面量，避免转义问题
    # 生成C++模块代码
    set(cpp_content "// Auto-generated from ${input_file} - DO NOT EDIT
module;

export module ${module_name};

import std;

export namespace EmbeddedLocales {
    constexpr std::string_view ${variable_name} = R\"EmbeddedJson(${json_content})EmbeddedJson\";
}
")
    
    # 写入生成的C++文件
    file(WRITE ${output_file} "${cpp_content}")
endfunction()

# 嵌入所有语言文件的函数
function(embed_all_locales)
    # 确保生成目录存在
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated/locales)
    
    # 嵌入中文
    embed_json_as_module(
        "${CMAKE_SOURCE_DIR}/src/locales/zh-CN.json"
        "${CMAKE_BINARY_DIR}/generated/locales/zh_cn_embedded.ixx"
        "Core.I18n.Embedded.ZhCN"
        "zh_cn_json"
    )
    
    # 嵌入英文
    embed_json_as_module(
        "${CMAKE_SOURCE_DIR}/src/locales/en-US.json"
        "${CMAKE_BINARY_DIR}/generated/locales/en_us_embedded.ixx"
        "Core.I18n.Embedded.EnUS"
        "en_us_json"
    )
    
    # 返回生成的文件列表给父作用域
    set(EMBEDDED_LOCALE_SOURCES
        "${CMAKE_BINARY_DIR}/generated/locales/zh_cn_embedded.ixx"
        "${CMAKE_BINARY_DIR}/generated/locales/en_us_embedded.ixx"
        PARENT_SCOPE
    )
endfunction() 