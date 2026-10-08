#!/usr/bin/env python3
"""One command: configure, build and run all host tests. Exits non-zero on any failure.

Usage:
    python Tools/CI/build.py                  # Debug build + tests (ASan/LSan on Linux/macOS)
    python Tools/CI/build.py --config Release
    python Tools/CI/build.py --all            # Debug and Release
    python Tools/CI/build.py --leak-canary    # expects the tests to FAIL (proves leak checking works)
"""
import argparse
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def run(cmd):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, cwd=ROOT).returncode


def build_one(config, extra):
    build_dir = ROOT / "out" / "build" / f"ci-{config.lower()}"
    generator = ["-G", "Ninja"] if shutil.which("ninja") else []
    code = run(["cmake", "-S", str(ROOT), "-B", str(build_dir), *generator, f"-DCMAKE_BUILD_TYPE={config}", *extra])
    if code != 0:
        return code
    code = run(["cmake", "--build", str(build_dir), "--config", config])
    if code != 0:
        return code
    return run(["ctest", "--test-dir", str(build_dir), "-C", config, "--output-on-failure"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default="Debug", choices=["Debug", "Release"])
    parser.add_argument("--all", action="store_true", help="build and test Debug and Release")
    parser.add_argument("--leak-canary", action="store_true", help="add a deliberately leaking test; this run must fail")
    parser.add_argument("cmake_args", nargs="*", help="extra arguments passed to cmake configure")
    args = parser.parse_args()

    extra = list(args.cmake_args)
    extra.append("-DKOPIT_LEAK_CANARY=ON" if args.leak_canary else "-DKOPIT_LEAK_CANARY=OFF")

    configs = ["Debug", "Release"] if args.all else [args.config]
    for config in configs:
        code = build_one(config, extra)
        if args.leak_canary:
            print("leak canary: tests failed as expected" if code != 0 else "ERROR: leak was NOT detected")
            return 0 if code != 0 else 1
        if code != 0:
            print(f"FAILED ({config})")
            return code
    print("ALL BUILDS AND TESTS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
