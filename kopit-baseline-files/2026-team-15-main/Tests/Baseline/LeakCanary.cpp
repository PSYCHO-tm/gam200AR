// Only built with -DKOPIT_LEAK_CANARY=ON. Proves the leak check works: this test run MUST fail.
#include "TestFramework.hpp"

KOPIT_TEST(DeliberateLeak)
{
	int* volatile leaked = new int[16];
	leaked[0]            = 1;
	KOPIT_CHECK(leaked[0] == 1);
}
