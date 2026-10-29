# Team Project Starter

This repository is the starting point for your team's project.

- `.clang-format` / `.clang-tidy` — C/C++ style and lint rules for this project.
- `.gitignore` — ignores common build artifacts for C++/CMake, web, Android, and Python.

## Repository Structure

```text
root/
├── Assets/            # Audio, JSON, 3D model assets
├── Cmake/             #CMake src file
├   ├── GetDependencies
├── Core/
├   ├── Header/        # Core Header files (.h)
├   ├── Source/        # Core Source files (.cpp)
├   ├── CmakeLists/    # Core CMakeList
├── Extern/            # External libraries
├── Platform/
├   ├── desktop/       # Windows dev shell
├   ├── KopiTwin/      # Android user shell
├── Tests/             # Unit and integration tests
├── Tools/             # Tools to assist with tests
├── CMakeLists         # Root CMakeList for whole proj
├── .clang-format      # C/C++ style and formatting rules
├── .clang-tidy        # C/C++ static analysis and lint rules
└── .gitignore         # Ignores build artifacts and temporary files
```

## Branching Model & Workflow

We follow a feature-branch workflow:
- **`main`**: Production-ready branch. Any merge conflicts must be dealt with on the sub-branch before merging.
- **`JIRAkey-Milestone-Subcategory-Task`**: Branch off `main` from JIRA for new taskings or bug fixes.
- **Pull Requests (PRs)**: Receive at least one code review approval before merging.

## Code Style & Enforcement

Team conventions are enforced with `.clang-format` (formatting) and `.clang-tidy` (naming rules and static analysis). One documented command checks every source file and reports each one that doesn't conform.

### Prerequisites (Windows)

Install [MSYS2](https://www.msys2.org), then open **MSYS2 UCRT64** from the Start menu and run:

```bash
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-clang-tools-extra mingw-w64-ucrt-x86_64-gcc
```

Use the UCRT64 terminal for every command below. If you use Git Bash instead, add the tools to your `PATH` first:

```bash
export PATH="/c/msys64/ucrt64/bin:$PATH"
```

Verify: `cmake --version && ninja --version && clang-format --version && clang-tidy --version`

Notes:
- `pacman` may print many mirror 404/timeout errors. This is normal as long as the install finishes.
- The first `cmake -B build` downloads all dependencies and takes a while. Later runs are fast.
- The `.clang-tidy` config targets LLVM 18; the MSYS2 package is 17.x. If clang-tidy reports an unknown check or option, tell the team.

### Running the Style Check

Open a bash shell (Git Bash or MSYS2 UCRT64) at the repo root.

1. **Generate the compilation database** (needed by `clang-tidy`; once per fresh clone, and again if you add or remove source files):

   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
   ```

2. **Run the check:**

   ```bash
   bash Tools/check_clang.sh
   ```

The script scans `Core/`, `Platform/desktop` and `Tests/` (`Extern/` is excluded). It prints a `[FAIL]` line for each file that violates the rules and exits with a non-zero code if anything fails, so it can also be used in CI or a pre-commit hook.

Notes:
- If `build/compile_commands.json` is missing, the script fails and tells you to run step 1.
- Run the check before every commit and before opening a PR.
- Existing code may currently fail the check, and that this is expected until a cleanup commit.

### Fixing Formatting Violations

`clang-format` can fix formatting automatically. Run from the repo root:

```bash
find Core Tests \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) | xargs clang-format -i
```

Naming violations reported by `clang-tidy` must be fixed by hand. The rules are defined in `.clang-tidy`.