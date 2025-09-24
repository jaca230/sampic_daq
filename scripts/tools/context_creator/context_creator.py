#!/usr/bin/env python3
"""
context_creator.py - Generate a .ctxt file with project sources
to provide context for ChatGPT.

Collects:
  - frontend.cpp
  - everything under include/
  - everything under src/

Excludes:
  - build/
  - deprecated/

Project root is inferred as three directories up from this script.
Output is always written to the current working directory.
"""

import os
import pathlib

DEFAULT_OUTPUT = "sampic_context.ctxt"
EXCLUDE_DIRS = {"build", "deprecated"}

def project_root():
    # Resolve path of this script and step 3 dirs up
    return pathlib.Path(__file__).resolve().parents[3]

def collect_files(root):
    files = []
    for dirpath, dirnames, filenames in os.walk(root):
        # prune excluded dirs
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]

        for f in filenames:
            relpath = os.path.relpath(os.path.join(dirpath, f), root)
            if (
                relpath == "frontend.cpp"
                or relpath.startswith("include/")
                or relpath.startswith("src/")
            ):
                files.append(relpath)
    return sorted(files)

def main():
    root = project_root()
    files = collect_files(str(root))

    output_path = pathlib.Path.cwd() / DEFAULT_OUTPUT
    with open(output_path, "w", encoding="utf-8") as out:
        for relpath in files:
            fullpath = root / relpath
            size = fullpath.stat().st_size if fullpath.exists() else 0
            out.write("===== FILE START =====\n")
            out.write(f"Relative Path: {relpath}\n")
            out.write(f"Absolute Path: {fullpath}\n")
            out.write(f"Size: {size} bytes\n")
            out.write("-----\n")
            try:
                with open(fullpath, "r", encoding="utf-8", errors="ignore") as f:
                    out.write(f.read())
            except Exception as e:
                out.write(f"<<ERROR reading file: {e}>>\n")
            out.write("\n===== FILE END =====\n\n")

    print(f"Wrote {len(files)} files to {output_path}")

if __name__ == "__main__":
    main()
