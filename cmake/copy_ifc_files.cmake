# https://github.com/microsoft/vscode-cpptools/issues/6302#issuecomment-2865013004

set(source_dir ${ARGV0})
set(std_module_dir ${ARGV1})
set(target_dir ${ARGV2})

# 检查指定目录是否需要复制文件
function(check_copy_necessary_for_dir src_dir tgt_dir result)
  file(GLOB_RECURSE src_files "${src_dir}/*.ifc")

  foreach(src_file IN LISTS src_files)
    string(REPLACE "${src_dir}/" "" relative_path "${src_file}")
    set(tgt_file "${tgt_dir}/${relative_path}")

    # 如果目标文件不存在或比源文件旧，则需要复制
    if(NOT EXISTS "${tgt_file}" OR "${src_file}" IS_NEWER_THAN "${tgt_file}")
      set(${result}
          TRUE
          PARENT_SCOPE)
      return()
    endif()
  endforeach()

  # 如果执行到这里，说明不需要复制
  set(${result}
      FALSE
      PARENT_SCOPE)
endfunction()

# 检查是否需要复制文件
check_copy_necessary_for_dir("${source_dir}" "${target_dir}"
                             project_copy_necessary)
check_copy_necessary_for_dir("${std_module_dir}" "${target_dir}"
                             std_copy_necessary)

if(NOT project_copy_necessary AND NOT std_copy_necessary)
  return()
endif()

# 创建目标目录（如果不存在）
file(MAKE_DIRECTORY "${target_dir}")

# 带重试机制的目录复制函数
function(copy_directory_with_retry src_dir tgt_dir dir_name)
  if(NOT EXISTS "${src_dir}")
    message(WARNING "${dir_name} directory does not exist: ${src_dir}")
    return()
  endif()

  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${src_dir}"
                          "${tgt_dir}" RESULT_VARIABLE copy_result)

  if(${copy_result} EQUAL 0)
    return()
  endif()

  # 尝试终止 VS Code C++ 工具进程（失败也继续）
  execute_process(COMMAND taskkill /IM cpptools-srv.exe /F OUTPUT_QUIET
                                                           ERROR_QUIET)
  execute_process(COMMAND taskkill /IM vcpkgsrv.exe /F OUTPUT_QUIET ERROR_QUIET)

  # 重试复制
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${src_dir}"
                          "${tgt_dir}" RESULT_VARIABLE retry_result)

endfunction()

# 复制项目模块文件
if(project_copy_necessary)
  copy_directory_with_retry("${source_dir}" "${target_dir}" "Project")
endif()

# 复制标准库模块文件
if(std_copy_necessary)
  copy_directory_with_retry("${std_module_dir}" "${target_dir}"
                            "Standard library")
endif()

message(STATUS ".ifc files processing completed")
