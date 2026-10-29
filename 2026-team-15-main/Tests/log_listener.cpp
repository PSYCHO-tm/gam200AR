// Logs every test start/end and every failed assertion to the log (console + file).
// Catch2 registers it automatically; the tests themselves don't change.

#include <catch2/catch_test_case_info.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <sstream>

#include "log.hpp"


namespace
{
    /*//////////////////////////////////////////////////
        Using Catch2's event listener base class:
        When Events (testCaseStart, assertionEnded) occurs,
        execute custom logic.
        Using log macros to write to .txt file found in
        /build/logs/kopitwin_log.
    *///////////////////////////////////////////////////
    class LogListener : public Catch::EventListenerBase
    {
    public:
        using Catch::EventListenerBase::EventListenerBase; // use base class ctor

        void testCaseStarting(Catch::TestCaseInfo const& info) override
        {
            LOG_INFO("TEST START  " << info.name);
        }

        void assertionEnded(Catch::AssertionStats const& stats) override
        {
            const Catch::AssertionResult& r = stats.assertionResult;
            if (r.isOk())
                return;

            // values from CAPTURE(...) calls in the test, if any
            std::ostringstream captured;
            for (const auto& m : stats.infoMessages)
                captured << "  [" << m.message << "]";

            LOG_ERROR("ASSERTION FAILED  " << r.getSourceInfo().file << ":" << r.getSourceInfo().line
                      << "  " << r.getExpressionInMacro()
                      << "  expanded: " << r.getExpandedExpression()
                      << captured.str());
        }

        void testCaseEnded(Catch::TestCaseStats const& stats) override
        {
            const auto& a = stats.totals.assertions;
            if (a.failed == 0)
                LOG_INFO("TEST PASS   " << stats.testInfo->name << "  (" << a.passed << " assertions)");
            else
                LOG_ERROR("TEST FAIL   " << stats.testInfo->name << "  (" << a.failed << " failed, "
                          << a.passed << " passed)");
        }
    };
}

CATCH_REGISTER_LISTENER(LogListener)