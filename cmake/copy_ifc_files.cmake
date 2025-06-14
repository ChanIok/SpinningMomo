# https://github.com/microsoft/vscode-cpptools/issues/6302#issuecomment-2865013004
# copy_ifc_files.cmake
# Arguments:
#   source_dir - Source directory for the project .ifc files
#   std_module_dir - Source directory for the standard library .ifc files
#   target_dir - Target directory for all .ifc files

set(source_dir ${ARGV0})
set(std_module_dir ${ARGV1})
set(target_dir ${ARGV2})

# Function to check if the copy is necessary for a source directory
function(check_copy_necessary_for_dir src_dir tgt_dir result)
    file(GLOB_RECURSE src_files "${src_dir}/*.ifc")
    
    foreach(src_file IN LISTS src_files)
        string(REPLACE "${src_dir}/" "" relative_path "${src_file}")
        set(tgt_file "${tgt_dir}/${relative_path}")

        # If a target file does not exist or is older than the source file, copying is necessary
        if (NOT EXISTS "${tgt_file}" OR "${src_file}" IS_NEWER_THAN "${tgt_file}")
            set(${result} TRUE PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # If we reach here, no copying is necessary for this directory
    set(${result} FALSE PARENT_SCOPE)
endfunction()

# Check if copying is necessary for either directory
check_copy_necessary_for_dir("${source_dir}" "${target_dir}" project_copy_necessary)
check_copy_necessary_for_dir("${std_module_dir}" "${target_dir}" std_copy_necessary)

if (NOT project_copy_necessary AND NOT std_copy_necessary)
    message(STATUS "No need to copy .ifc files; target directory is up to date.")
    return()
endif()

# Create target directory if it doesn't exist
file(MAKE_DIRECTORY "${target_dir}")

# Function to copy directory with retry mechanism
function(copy_directory_with_retry src_dir tgt_dir dir_name)
    if(NOT EXISTS "${src_dir}")
        message(WARNING "${dir_name} directory does not exist: ${src_dir}")
        return()
    endif()

    message(STATUS "Copying ${dir_name} .ifc files from ${src_dir}...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${src_dir}" "${tgt_dir}"
        RESULT_VARIABLE copy_result
    )

    if (${copy_result} EQUAL 0)
        message(STATUS "${dir_name} .ifc files copied successfully.")
        return()
    endif()

    # If the copy failed, perform taskkill and retry
    message(WARNING "Initial copy of ${dir_name} .ifc files failed. Attempting taskkill and retry...")
    
    # Terminate cpptools-srv.exe
    execute_process(
        COMMAND taskkill /IM cpptools-srv.exe /F
        RESULT_VARIABLE cpptools_taskkill_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    # Terminate vcpkgsrv.exe
    execute_process(
        COMMAND taskkill /IM vcpkgsrv.exe /F
        RESULT_VARIABLE vcpkg_taskkill_result
        OUTPUT_QUIET
        ERROR_QUIET
    )

    # taskkill result 0 means success, 128 means not found. Both are acceptable.
    set(cpptools_ok FALSE)
    if(${cpptools_taskkill_result} EQUAL 0 OR ${cpptools_taskkill_result} EQUAL 128)
      set(cpptools_ok TRUE)
    endif()

    set(vcpkgsrv_ok FALSE)
    if(${vcpkg_taskkill_result} EQUAL 0 OR ${vcpkg_taskkill_result} EQUAL 128)
      set(vcpkgsrv_ok TRUE)
    endif()

    if(${cpptools_taskkill_result} EQUAL 0)
      message(STATUS "Successfully terminated cpptools-srv.exe")
    endif()

    if(${vcpkg_taskkill_result} EQUAL 0)
      message(STATUS "Successfully terminated vcpkgsrv.exe")
    endif()

    if(NOT cpptools_ok OR NOT vcpkgsrv_ok)
        message(FATAL_ERROR "Failed to terminate processes. cpptools-srv.exe result: ${cpptools_taskkill_result}, vcpkgsrv.exe result: ${vcpkg_taskkill_result}. Copy cannot proceed.")
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${src_dir}" "${tgt_dir}"
        RESULT_VARIABLE retry_result
    )

    if (NOT ${retry_result} EQUAL 0)
        message(FATAL_ERROR "Retry failed. ${dir_name} .ifc files could not be copied.")
    endif()

    message(STATUS "${dir_name} .ifc files copied successfully after retry.")
endfunction()

# Copy project modules
if(project_copy_necessary)
    copy_directory_with_retry("${source_dir}" "${target_dir}" "Project")
endif()

# Copy standard library modules
if(std_copy_necessary)
    copy_directory_with_retry("${std_module_dir}" "${target_dir}" "Standard library")
endif()

message(STATUS "All .ifc files copied successfully.")