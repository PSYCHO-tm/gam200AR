#!/usr/bin/env bash
# Usage: bash Tools/CI/build.sh        (opens a menu)
#        bash Tools/CI/build.sh <letter>   (runs one menu choice without the menu, e.g. x; the exit code is its result)
cd "$(dirname "$0")/../.."          # always run from the repo root, takes script's directory and move up 2 levels

# Debug builds go in build/ (check_clang.sh reads build/compile_commands.json), Release builds in build-release/
build_dir()  { [ "$1" = release ] && echo build-release || echo build; }
cmake_type() { [ "$1" = release ] && echo Release || echo Debug; }

run_tests() {
    local cfg=${1:-debug} rc=0
    run_build_tests "$cfg" || rc=$?
    if [ "$cfg" = debug ]; then      # clang-tidy reads the Debug build's compile_commands.json, so lint runs for Debug only
        run_checks || echo "WARNING: style/lint issues found (advisory only, does not fail the run)." # if want lint to fail, change back to rc=$?
    fi
    return "$rc"
}


run_build_tests() {
    local cfg=${1:-debug} dir type rc
    dir=$(build_dir "$cfg"); type=$(cmake_type "$cfg")
    mkdir -p "$dir/logs"
    rm -f "$dir/logs/kopitwin_log.txt"         # one log file per run

    {
        time cmake -S . -B "$dir" -DCMAKE_BUILD_TYPE="$type" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && # Config: Cmake builds in Debug or Release mode and writes a JSON file for clang-tidy to read
        time cmake --build "$dir" --config "$type" --parallel && # Builds project in that mode, compiles multiple files at once instead of one at a time
        ctest --test-dir "$dir" -C "$type" --output-on-failure --no-tests=error
    } 2>&1 | tee "$dir/logs/test_run.txt"      # whole console output, errors included, 2>&1 redirects stderr to stdout
    rc=${PIPESTATUS[0]}                        # exit code of the commands, not of tee

    [ "$rc" -eq 0 ] && echo "Success! All $cfg tests passed."
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
    local cfg=${1:-debug} task=assembleDebug
    [ "$cfg" = release ] && task=assembleRelease
    ( cd Platform/KopiTwin && cmd.exe /c gradlew.bat "$task" ) &&
    echo "Success! Android $cfg build OK."
}

run_both() { run_tests && build_android; }          # debug only

# Every combination: desktop + Android, Debug + Release. Keeps going after a failure so one run shows everything that
# is broken, then fails if anything failed.
run_all() {
    local failed=()
    run_tests debug       || failed+=("desktop-debug")
    run_tests release     || failed+=("desktop-release")
    build_android debug   || failed+=("android-debug")
    build_android release || failed+=("android-release")

    echo
    if [ "${#failed[@]}" -eq 0 ]; then
        echo "ALL COMBINATIONS OK: desktop and android, debug and release."
        return 0
    fi
    echo "FAILED COMBINATIONS: ${failed[*]}"
    return 1
}

clean_all() {
    rm -rf build build-release Platform/KopiTwin/app/.cxx Platform/KopiTwin/app/build
    echo "Clean done. (Extern/ kept: the downloaded libraries are not removed.)"
}

status=0; ran=0
try() { ran=1; "$@"; status=$?; [ "$status" -eq 0 ] || echo ">>> FAILED"; }

# One menu letter. Returns 1 if the letter is not a choice (a real choice always returns 0, its result is in $status).
run_choice() {
    case "${1,,}" in
        t) try run_tests debug ;;
        r) try run_tests release ;;
        a) try build_android debug ;;
        s) try build_android release ;;
        b) try run_both ;;
        x) try run_all ;;
        c) try clean_all ;;
        *) return 1 ;;
    esac
}

# With a letter on the command line, run that choice once and exit with its result (no menu).
if [ "$#" -gt 0 ]; then
    run_choice "$1" || { echo "Unknown choice '$1'. Use t, r, a, s, b, x or c, or no argument for the menu." >&2; exit 2; }
    exit "$status"
fi

while true; do
    echo
    echo "  T - Build + run tests (desktop, debug) + Clang Tidy"
    echo "  R - Build + run tests (desktop, release)"
    echo "  A - Build Android (debug)"
    echo "  S - Build Android (release)"
    echo "  B - Both (desktop + Android, debug)"
    echo "  X - Every combination (desktop + Android, debug + release)"
    echo "  C - Clean build files"
    echo "  Q - Quit"
    read -rp "Choose an option: " choice || { [ "$ran" -eq 1 ] && exit "$status"; echo "No input: nothing was built." >&2; exit 2; }
    case "${choice,,}" in
        q) exit "$status" ;;
        *) run_choice "$choice" || echo "Invalid choice" ;;
    esac
done
