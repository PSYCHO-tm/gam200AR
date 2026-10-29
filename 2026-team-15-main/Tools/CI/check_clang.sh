#!/usr/bin/env bash
# Style & lint check. Lives in Tools/, runs from anywhere.
# Usage: bash Tools/check_clang.sh

set -u

# Move to the repo root (this script is in <root>/Tools)
cd "$(dirname "$0")/../.." || exit 1

# Max diff lines shown per file when formatting fails
MAX_DIFF_LINES=40

echo "=== Starting Code Style & Linter Checks ==="

# Folders containing code we own (Extern/ and Platform/KopiTwin are deliberately excluded)
DIRECTORIES="Core Tests"

TARGET_DIRS=""
for dir in $DIRECTORIES; do
    if [ -d "$dir" ]; then
        TARGET_DIRS="$TARGET_DIRS $dir"
    fi
done

if [ -z "$TARGET_DIRS" ]; then
    echo "Error: None of the target source directories ($DIRECTORIES) were found."
    exit 1
fi

# Check the tools exist before using them
for tool in clang-format clang-tidy diff; do
    if ! command -v "$tool" > /dev/null 2>&1; then
        echo "Error: $tool not found on PATH."
        exit 1
    fi
done

# All C++ files (skipping build output and downloaded dependencies)
ALL_FILES=$(find $TARGET_DIRS \
    \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
    -not -path "*/build/*" -not -path "*/out/*" -not -path "*/.cxx/*" 2>/dev/null)

if [ -z "$ALL_FILES" ]; then
    echo "No C++ files found to check."
    exit 0
fi

# clang-tidy only gets .cpp files: headers have no entry in
# compile_commands.json and are analysed via the .cpp files that include them
CPP_FILES=$(echo "$ALL_FILES" | grep '\.cpp$')

TOTAL_FILES=$(echo "$ALL_FILES" | wc -l)
FAILED=0
FORMAT_FAILS=0
TIDY_FAILS=0

echo "--- 1. Running clang-format check ($TOTAL_FILES files) ---"
while IFS= read -r file; do
    if ! clang-format --dry-run -Werror "$file" > /dev/null 2>&1; then
        echo "[FAIL] Formatting violation: $file"
        # '-' = what the file has now, '+' = what clang-format wants
        changes=$(diff -u --label "$file (current)" --label "$file (clang-format)" \
            "$file" <(clang-format "$file"))
        total=$(echo "$changes" | wc -l)
        echo "$changes" | head -n "$MAX_DIFF_LINES"
        if [ "$total" -gt "$MAX_DIFF_LINES" ]; then
            echo "    ... $((total - MAX_DIFF_LINES)) more diff lines. Full diff:"
            echo "    diff -u $file <(clang-format $file)"
        fi
        FORMAT_FAILS=$((FORMAT_FAILS + 1))
        FAILED=1
    fi
done <<< "$ALL_FILES"

echo "--- 2. Running clang-tidy check (naming rules) ---"
if [ ! -f "build/compile_commands.json" ]; then
    echo "[WARNING] build/compile_commands.json not found! Skipping clang-tidy."
    echo "          Run: cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    FAILED=1   # a skipped check must not look like a pass
elif [ -z "$CPP_FILES" ]; then
    echo "No .cpp files to lint."
else
    # Each distinct diagnostic is printed once, even if a header is
    # included by many .cpp files.
    declare -A seen
    while IFS= read -r file; do
        # --checks / --warnings-as-errors on the command line override .clang-tidy,
        # so only the naming check runs and its violations always fail the script.
        # Naming options (CheckOptions) are still read from .clang-tidy.
        output=$(clang-tidy -p build --quiet \
            --checks='-*,readability-identifier-naming' \
            --warnings-as-errors='readability-identifier-naming' \
            "$file" 2>&1)
        rc=$?
        if [ "$rc" -ne 0 ]; then
            echo "[FAIL] Linter: $file"
            # One line per diagnostic: file:line:col: message [check]
            while IFS= read -r line; do
                if [ -z "${seen[$line]:-}" ]; then
                    seen[$line]=1
                    echo "    $line"
                fi
            done < <(echo "$output" | grep -E ' (error|warning): ')
            TIDY_FAILS=$((TIDY_FAILS + 1))
            FAILED=1
        fi
    done <<< "$CPP_FILES"
fi

echo "=========================================="
echo "Formatting failures: $FORMAT_FAILS file(s)"
echo "Linter failures:     $TIDY_FAILS file(s)"
echo "=========================================="
if [ "$FAILED" -eq 1 ]; then
    echo "Style or Linter check FAILED. Fix?"
    echo "find Core Tests \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) | xargs clang-format -i"
    exit 1
else
    echo "All files passed style and linter checks!"
    exit 0
fi
