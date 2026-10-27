#!/usr/bin/env bash
# Usage: bash Tools/CI/build.sh     (opens a menu)
cd "$(dirname "$0")/../.."          # always run from the repo root

run_tests() {
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug &&
    cmake --build build --config Debug --parallel &&
    ctest --test-dir build --output-on-failure --no-tests=error &&
    echo "Success! All tests passed."
}

build_android() {
    ( cd Platform/KopiTwin && cmd.exe /c gradlew.bat assembleDebug ) &&
    echo "Success! Android build OK."
}

run_both() { run_tests && build_android; }

status=0
try() { "$@"; status=$?; [ "$status" -eq 0 ] || echo ">>> FAILED"; }

while true; do
    echo
    echo "  T - Build + run tests (desktop)"
    echo "  A - Build Android"
    echo "  B - Both"
    echo "  Q - Quit"
    read -rp "Choose an option: " choice
    case "${choice,,}" in
        t) try run_tests ;;
        a) try build_android ;;
        b) try run_both ;;
        q) exit "$status" ;;
        *) echo "Invalid choice" ;;
    esac
done
