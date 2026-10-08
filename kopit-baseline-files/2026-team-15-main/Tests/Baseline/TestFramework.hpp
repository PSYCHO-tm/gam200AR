#pragma once

// Tiny test framework: no dependencies, no heap-allocated globals (so leak checks stay clean).
// Add a test with KOPIT_TEST(Name) { KOPIT_CHECK(...); } in any Tests/Baseline/*.cpp file.

#include <cstdio>

struct TestCase
{
	char const* Name;
	void (*Fn)();
};

struct TestState
{
	TestCase Cases[256];
	int      Count     = 0;
	int      FailCount = 0;
};

inline TestState& GetTestState()
{
	static TestState state;
	return state;
}

struct TestRegistrar
{
	TestRegistrar(char const* name, void (*fn)())
	{
		TestState& state           = GetTestState();
		state.Cases[state.Count++] = {name, fn};
	}
};

#define KOPIT_TEST(name)                                                                                               \
	static void                name();                                                                                 \
	static TestRegistrar const registrar_##name(#name, name);                                                          \
	static void                name()

#define KOPIT_CHECK(cond)                                                                                              \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(cond))                                                                                                   \
		{                                                                                                              \
			std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                              \
			++GetTestState().FailCount;                                                                                \
		}                                                                                                              \
	} while (0)
