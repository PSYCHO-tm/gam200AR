#include "Parser_(FAKE)/parser.hpp"

#include <cctype>
#include <sstream>
#include <vector>

namespace Kopi
{
    std::optional<DrinkOrder> ParseOrder(const std::string& text)
    {
        std::vector<std::string> words;
        std::istringstream stream(text);
        std::string word;
        while (stream >> word)
        {
            for (char& c : word)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            words.push_back(word);
        }

        if (words.empty() || words[0] != "kopi")
        {
            return std::nullopt;
        }

        DrinkOrder order;
        for (size_t i = 1; i < words.size(); ++i)
        {
            if (words[i] == "c")           order.milk  = Milk::EVAPORATED;
            else if (words[i] == "o")      order.milk  = Milk::NO_MILK;
            else if (words[i] == "kosong") order.sugar = Sugar::NO_SUGAR;
            else if (words[i] == "siew" && i + 1 < words.size() && words[i + 1] == "dai")
            {
                order.sugar = Sugar::LESS;
                ++i;   // skip "dai"
            }
            else
            {
                return std::nullopt;
            }
        }
        return order;
    }
}
