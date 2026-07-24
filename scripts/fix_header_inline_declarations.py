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

    print(f"Scanning {len(hpp_files)} .hpp files to remove 'inline' from declaration-only statements ending with ';'")

    fixed_count = 0

    for hpp_path in hpp_files:
        rel_path = os.path.relpath(hpp_path, src_dir).replace("\\", "/")
        with open(hpp_path, "r", encoding="utf-8") as f:
            lines = f.readlines()

        new_lines = []
        file_modified = False

        for i, line in enumerate(lines):
            # If line contains 'inline' and is part of a function declaration ending with ';'
            if 'inline' in line and not line.strip().startswith('inline thread_local'):
                # Check if this statement ends with ';' before any '{'
                # Look ahead lines until ';' or '{'
                has_semicolon = False
                has_brace = False
                for j in range(i, min(i + 15, len(lines))):
                    text = lines[j].split('//')[0]
                    if '{' in text:
                        has_brace = True
                        break
                    if ';' in text:
                        has_semicolon = True
                        break

                if has_semicolon and not has_brace:
                    # Remove 'inline ' keyword from declaration
                    line = re.sub(r'\binline\s+', '', line)
                    file_modified = True
                    print(f"  Removed inline from declaration in {rel_path}:{i+1}: {line.strip()}")

            new_lines.append(line)

        if file_modified:
            with open(hpp_path, "w", encoding="utf-8") as f:
                f.writelines(new_lines)
            fixed_count += 1

    print(f"\nSuccessfully cleaned up declaration-only 'inline' keywords across {fixed_count} header files.")

if __name__ == "__main__":
    main()
