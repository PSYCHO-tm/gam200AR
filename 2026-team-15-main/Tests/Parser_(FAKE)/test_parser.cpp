#include <catch2/catch_test_macros.hpp>

#include "Parser_(FAKE)/parser.hpp"

using namespace Kopi;

TEST_CASE("plain kopi has condensed milk and normal sugar")
{
    auto order = ParseOrder("kopi");
    REQUIRE(order.has_value());
    CHECK(order->milk == Milk::CONDENSED);
    CHECK(order->sugar == Sugar::NORMAL);
}

TEST_CASE("kopi c kosong is evaporated milk, no sugar")
{
    auto order = ParseOrder("kopi c kosong");
    REQUIRE(order.has_value());
    CHECK(order->milk == Milk::EVAPORATED);
    CHECK(order->sugar == Sugar::NO_SUGAR);
}

TEST_CASE("kopi o siew dai is black with less sugar")
{
    auto order = ParseOrder("kopi o siew dai");
    REQUIRE(order.has_value());
    CHECK(order->milk == Milk::NO_MILK);
    CHECK(order->sugar == Sugar::LESS);
}

TEST_CASE("input is case-insensitive")
{
    auto order = ParseOrder("KOPI O");
    REQUIRE(order.has_value());
    CHECK(order->milk == Milk::NO_MILK);
}

TEST_CASE("garbage is rejected")
{
    CHECK_FALSE(ParseOrder("").has_value());
    CHECK_FALSE(ParseOrder("teh").has_value());
    CHECK_FALSE(ParseOrder("kopi banana").has_value());
}
