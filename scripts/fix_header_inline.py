#!/usr/bin/env python3
import os
import re

def main():
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    src_dir = os.path.join(project_root, "src")

    hpp_files = []
    for root, _, files in os.walk(src_dir):
        for file in files:
            if file.endswith(".hpp"):
                hpp_files.append(os.path.join(root, file))

    print(f"Scanning {len(hpp_files)} .hpp files for missing inline keywords...")

    modified_count = 0

    for hpp_path in hpp_files:
        rel_path = os.path.relpath(hpp_path, src_dir).replace("\\", "/")
        with open(hpp_path, "r", encoding="utf-8") as f:
            lines = f.readlines()

        new_lines = []
        file_modified = False

        in_struct_or_class = False
        brace_depth = 0

        for i, line in enumerate(lines):
            stripped = line.strip()

            # Track struct/class nesting
            if re.search(r'\b(struct|class|union)\b\s+\w+', line) and not ';' in line:
                in_struct_or_class = True

            # Check thread_local
            if line.strip().startswith('thread_local ') and 'inline' not in line:
                line = line.replace('thread_local ', 'inline thread_local ')
                file_modified = True
                print(f"  Fixed thread_local in {rel_path}: {line.strip()}")

            # Check namespace-level free functions
            # Match start of function signature: auto FuncName(...) or [[nodiscard]] auto FuncName(...)
            elif re.match(r'^\s*(\[\[nodiscard\]\]\s*)?(auto|void|int|bool|HRESULT|BOOL|DWORD|std::\w+)\s+\w+\s*\(', line):
                # Ensure it's not inline, constexpr, static, or template
                if not re.search(r'\b(inline|constexpr|static|template)\b', line):
                    # Check if this signature is a definition (has '{' on this line or within next few lines before ';')
                    is_def = False
                    for j in range(i, min(i + 15, len(lines))):
                        if '{' in lines[j]:
                            is_def = True
                            break
                        if ';' in lines[j]:
                            break

                    if is_def:
                        # Insert inline
                        if '[[nodiscard]]' in line:
                            line = line.replace('[[nodiscard]]', '[[nodiscard]] inline')
                        else:
                            line = re.sub(r'^(\s*)(auto|void|int|bool|HRESULT|BOOL|DWORD|std::\w+)\b', r'\1inline \2', line)
                        file_modified = True
                        print(f"  Fixed function inline in {rel_path}:{i+1}: {line.strip()}")

            new_lines.append(line)

        if file_modified:
            with open(hpp_path, "w", encoding="utf-8") as f:
                f.writelines(new_lines)
            modified_count += 1

    print(f"\nSuccessfully added inline keyword across {modified_count} header files.")

if __name__ == "__main__":
    main()
