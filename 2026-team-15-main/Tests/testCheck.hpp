#pragma once

// Tiny check helper for the host tests: no dependencies, one main() per test file.
#include <filesystem>
#include <iostream>
#include <string>

inline int& TestFailCount()
{
	static int count = 0;
	return count;
}

#define TEST_CHECK(cond)                                                                                               \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(cond))                                                                                                   \
		{                                                                                                              \
			std::cout << "FAIL " << __FILE__ << ":" << __LINE__ << "  " << #cond << "\n";                              \
			++TestFailCount();                                                                                         \
		}                                                                                                              \
	} while (0)

inline int TestFinish(char const* name)
{
	std::cout << name << (TestFailCount() != 0 ? ": FAILED" : ": ALL PASSED") << "\n";
	return TestFailCount() != 0 ? 1 : 0;
}

// Repo root, found from this header's own path (tests/testCheck.hpp), so tests work from any directory.
inline std::string RepoRoot() { return std::filesystem::path(__FILE__).parent_path().parent_path().generic_string(); }
