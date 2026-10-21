#pragma once
#include <optional>
#include <string>

namespace Kopi
{
    enum class Milk  { CONDENSED, EVAPORATED, NO_MILK };
    enum class Sugar { NORMAL, LESS, NO_SUGAR };

    struct DrinkOrder
    {
        Milk  milk  = Milk::CONDENSED;
        Sugar sugar = Sugar::NORMAL;
    };

    // "kopi c kosong" -> evaporated milk, no sugar.
    // Returns std::nullopt if the text is not a valid order.
    std::optional<DrinkOrder> ParseOrder(const std::string& text);
}
