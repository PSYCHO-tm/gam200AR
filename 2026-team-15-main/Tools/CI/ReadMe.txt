## Running tests (CI)

One script builds the shared code (`Core/`), builds every test in `Tests/`, and runs them with AddressSanitizer on, so memory leaks fail the run.

### Requirements
- Linux or WSL (Windows users: run everything inside WSL, not PowerShell)
Run this:
sudo apt update
sudo apt install -y build-essential cmake git
sudo apt install clang-format clang-tidy
- Internet on the **first** run only (it downloads Catch2 into `build/`)

### Run all tests
From anywhere inside the repo:

```bash
bash Tools/CI/build.sh
```

- Success ends with `Success! All tests passed [Debug].`
- Any failure (compile error, failed test, memory leak) stops the script and the success line does not print.
- The first run is slow (Catch2 download and build). Later runs only rebuild what changed.

### Add tests for your code
Put your code in `Core/`, one folder per module, with the same module name in `Tests/`:

```
Core/Header/<Module>/<name>.hpp
Core/Source/<Module>/<name>.cpp
Tests/<Module>/test_<name>.cpp
```

You don't edit any CMake file. New `.cpp` files under `Core/Source/` and `Tests/` are picked up automatically.

For tests, call your functions the way they are declared.

Example `Tests/Parser/test_parser.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "Parser/parser.hpp"      // path starts at Core/Header/

TEST_CASE("garbage is rejected")
{
    REQUIRE_FALSE(Kopi::ParseOrder("hello").has_value());
}
```

Rules:
- Do **not** write `main()` in a test file. Catch2 provides it.
- Each `TEST_CASE` name must be unique, because it becomes the test's name in the report.
- Use `REQUIRE(...)` for checks. A failed `REQUIRE` stops that test case.
- Tests must not include Android or OpenGL headers. Only test logic that builds on a desktop.

### Reading the output
```
1/5 Test #1: garbage is rejected ........   Passed
2/5 Test #2: input is case-insensitive ..***Failed
   test_parser.cpp:34: FAILED:  REQUIRE( order.has_value() )  with expansion: false
```
The failed test's file and line are printed. After you fix the code, run the script again.

A memory leak shows up as an `AddressSanitizer: LeakSanitizer` report under the test that caused it, with the file and line of the allocation.


### Troubleshooting
- **`$'\r': command not found`**: the script has Windows line endings. Run `sed -i 's/\r$//' Tools/CI/build.sh`. (`.gitattributes` should prevent this on a fresh clone.)
- **CMake can't find your header**: the include path starts at `Core/Header/`, so write `#include "Parser/parser.hpp"`, not `"Core/Header/Parser/parser.hpp"`.
- **Linker error `undefined reference`**: your `.cpp` isn't under `Core/Source/`, or the function is declared but never defined.
- **Strange build errors after switching branches**: delete the `build/` folder and run the script again. It's git-ignored, so this is safe.