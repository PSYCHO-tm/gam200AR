#include <cstdio>

#include "Log/Log.hpp"
#include "TestFramework.hpp"

#if defined(_MSC_VER) && defined(_DEBUG)
#	include <crtdbg.h>
#endif

int main()
{
	Core::LogInit("kopitTests.log");

	TestState& state = GetTestState();
	for (int i = 0; i < state.Count; ++i)
	{
		int const failsBefore = state.FailCount;
		state.Cases[i].Fn();
		std::printf("%s %s\n", state.FailCount == failsBefore ? "[ ok ]" : "[FAIL]", state.Cases[i].Name);
	}
	Core::LogShutdown();
	std::printf("%d test(s), %d check failure(s)\n", state.Count, state.FailCount);

#if defined(_MSC_VER) && defined(_DEBUG)
	// MSVC debug heap leak check (Linux/Clang/GCC use AddressSanitizer + LeakSanitizer instead).
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	if (_CrtDumpMemoryLeaks())
	{
		std::printf("MEMORY LEAK DETECTED\n");
		return 1;
	}
#endif
	return state.FailCount == 0 ? 0 : 1;
}
