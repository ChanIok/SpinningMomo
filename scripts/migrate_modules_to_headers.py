#!/usr/bin/env python3
import os
import re

def main():
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    src_dir = os.path.join(project_root, "src")
    tests_dir = os.path.join(project_root, "tests")

    print(f"Project root: {project_root}")
    print("Step 1: Building module map from files in src/...")

    # We can scan all files in src/ (or .hpp files) to build the module map
    # Before running, if .hpp files already exist, we map module_name by parsing export module or by directory structure.
    # To make this script idempotent, let's scan all .hpp and .ixx files or re-run directly.
    module_map = {}

    for root, _, files in os.walk(src_dir):
        for file in files:
            if file.endswith(".ixx") or file.endswith(".hpp"):
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, src_dir).replace("\\", "/")
                rel_hpp_path = rel_path[:-4] + ".hpp" if file.endswith(".ixx") else rel_path

                with open(full_path, "r", encoding="utf-8") as f:
                    content = f.read()
                
                match = re.search(r'export\s+module\s+([\w\.]+);', content)
                if match:
                    mod_name = match.group(1)
                    module_map[mod_name] = rel_hpp_path
                else:
                    # Deriving module name from relative path if export module already removed
                    # e.g., "core/async/async.hpp" -> "Core.Async", "app.hpp" -> "App"
                    parts = rel_hpp_path.split("/")
                    # Handle special camel/snake casing if needed, but module_map is best populated when export module exists
                    pass

    # Create PCH
    pch_path = os.path.join(src_dir, "pch.hpp")
    print(f"\nCreating/updating {pch_path}...")
    pch_content = """#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Win32 APIs
#include <windows.h>
#include <dwmapi.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <psapi.h>
#include <tlhelp32.h>

// C++ Standard Libraries
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

// External / Third-Party Libraries
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <rfl/json.hpp>
#include <rfl.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <uWebSockets/App.h>
#include <wil/com.h>
#include <wil/result.h>
#include <wil/resource.h>
#include <WebView2.h>
#include <xxhash.h>
#include <webp/decode.h>
#include <webp/encode.h>
#include <zlib.h>
#include <dkm.hpp>
"""
    with open(pch_path, "w", encoding="utf-8") as f:
        f.write(pch_content)

    print("Step 2: Processing .cpp files to include self headers...")
    cpp_files = []
    for search_dir in [src_dir, tests_dir]:
        if not os.path.exists(search_dir):
            continue
        for root, _, files in os.walk(search_dir):
            for file in files:
                if file.endswith(".cpp"):
                    cpp_files.append(os.path.join(root, file))

    # Add self include for .cpp files based on file path (e.g., src/app.cpp -> #include "app.hpp")
    for cpp_path in cpp_files:
        if cpp_path.endswith("main.cpp") or cpp_path.endswith("pch.cpp"):
            continue
        
        rel_cpp = os.path.relpath(cpp_path, src_dir).replace("\\", "/")
        corresponding_hpp = rel_cpp[:-4] + ".hpp"
        hpp_abs = os.path.join(src_dir, corresponding_hpp)

        if os.path.exists(hpp_abs):
            with open(cpp_path, "r", encoding="utf-8") as f:
                lines = f.readlines()
            
            # Check if self header is already included
            expected_inc = f'#include "{corresponding_hpp}"'
            already_has_inc = any(expected_inc in line for line in lines)

            if not already_has_inc:
                # Insert #include "corresponding_hpp" at top of inclusions
                lines.insert(0, f'{expected_inc}\n')
                with open(cpp_path, "w", encoding="utf-8") as f:
                    f.writelines(lines)
                print(f"  Added self header {expected_inc} to {rel_cpp}")

    print("\n==================================================")
    print("Self header inclusions check complete!")
    print("==================================================")

if __name__ == "__main__":
    main()
