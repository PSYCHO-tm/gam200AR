#ifndef SHARED_LIB_HPP
#define SHARED_LIB_HPP

#include "nlohmann/json.hpp"
#include <iostream>
#include <optional>
#include <utility>  
#include <bitset>
#include <memory>
#include <cassert>
#include <map>
#include <array>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <algorithm>
#include <typeinfo>
#include <string>
#include <cstdint>
#include <functional>
#include "nlohmann/json.hpp"
//#include "json.hpp"

namespace Ecs{
    constexpr uint32_t MAX_ENTITIES{1000};
    constexpr uint8_t  MAX_COMPONENTS{64};

    using Entity = uint32_t;
    using Signature = std::bitset<MAX_COMPONENTS>;  //bitset initializes to all 0s
    using ComponentType = uint8_t; //used as uint8_t to save mem
    using Json = nlohmann::json;

    //ErrorChecker would be a vector that stores the errors in any place of stdcerr
    using ErrorChecker = std::vector<std::string>; 
}


#endif