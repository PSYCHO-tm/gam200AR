#!/usr/bin/env bash
# Usage: bash Tools/CI/build.sh     (opens a menu)
cd "$(dirname "$0")/../.."          # always run from the repo root, takes script's directory and move up 2 levels

run_tests() {
    local rc=0
    run_build_tests || rc=$?
    run_checks || echo "WARNING: style/lint issues found (advisory only, does not fail the run)." # if want lint to fail, change back to rc=$?
    return "$rc"
}


run_build_tests() {
    local rc
    mkdir -p build/logs
    rm -f build/logs/kopitwin_log.txt          # one log file per run

    {
        time cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && # Config: Cmake builds in Debug mode and writes a JSON file for clang-tidy to read
        time cmake --build build --config Debug --parallel && # Builds project in Debug mode, compiles multiple files at once instead of one at a time
        ctest --test-dir build --output-on-failure --no-tests=error
    } 2>&1 | tee build/logs/test_run.txt       # whole console output, errors included, 2>&1 redirects stderr to stdout
    rc=${PIPESTATUS[0]}                        # exit code of the commands, not of tee

    [ "$rc" -eq 0 ] && echo "Success! All tests passed."
    return "$rc"
}

run_checks() {
    local rc
    mkdir -p build/logs
    bash Tools/CI/check_clang.sh 2>&1 | tee build/logs/style_check.txt # runs check_clang.sh and outputs whole console, errors included into style_check.txt
    rc=${PIPESTATUS[0]} # exit code of the commands, not of tee
    return "$rc"
}

build_android() {
    ( cd Platform/KopiTwin && cmd.exe /c gradlew.bat assembleDebug ) &&
    echo "Success! Android build OK."
}

run_both() { run_tests && build_android; }

clean_all() {
    rm -rf build Platform/KopiTwin/app/.cxx Platform/KopiTwin/app/build
    echo "Clean done. (Extern/ kept: the downloaded libraries are not removed.)"
}

status=0
try() { "$@"; status=$?; [ "$status" -eq 0 ] || echo ">>> FAILED"; }

while true; do
    echo
    echo "  T - Build + run tests (desktop) + Clang Tidy"
    echo "  A - Build Android"
    echo "  B - Both"
    echo "  C - Clean build files"
    echo "  Q - Quit"
    read -rp "Choose an option: " choice || exit "$status"
    case "${choice,,}" in
        t) try run_tests ;;
        a) try build_android ;;
        b) try run_both ;;
        c) try clean_all ;;
        q) exit "$status" ;;
        *) echo "Invalid choice" ;;
    esac
done
