### Add tests for your code
Put your code in `Core/`, one folder per module, with the same module name in `Tests/`:

```
Core/Header/<Module>/<name>.hpp
Core/Source/<Module>/<name>.cpp
Tests/<Module>/test_<name>.cpp
```

You don't edit any CMake file. New `.cpp` files under `Core/Source/` and `Tests/` are picked up automatically.

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