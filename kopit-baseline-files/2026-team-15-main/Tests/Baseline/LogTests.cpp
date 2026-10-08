#include <filesystem>
#include <string>

#include "Entity Component System/SceneData.hpp"
#include "Log/Log.hpp"
#include "TestFramework.hpp"

KOPIT_TEST(LogWritesToFile)
{
	std::string const path = (std::filesystem::temp_directory_path() / "kopitLogTest.log").string();
	KOPIT_CHECK(Core::LogInit(path));
	Core::LogInfo("hello-from-test");
	Core::LogError("error-from-test");
	Core::LogShutdown();
	std::string text;
	KOPIT_CHECK(Core::ReadTextFile(path, text));
	KOPIT_CHECK(text.find("hello-from-test") != std::string::npos);
	KOPIT_CHECK(text.find("error-from-test") != std::string::npos);
	std::filesystem::remove(path);
	Core::LogInit("kopitTests.log"); // restore the suite's log file
}
