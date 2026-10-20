#pragma once
#include "sharedlib.hpp"

namespace Ecs{
/*
    [READING] Parsing via stringstream, returns a json object if successful to save
*/
[[nodiscard]] std::optional<Json> DeserializeStream(std::istream& is);

/*
    [READING] Making a JSON file (delegated) by passing a file name
*/
[[nodiscard]] std::optional<Json> DeserializePath(std::string const& filepath);

/*
    [WRITING] Takes a json object and transforms it into a readable json string
*/

[[nodiscard]] bool SerializeObj (Json const& obj, std::string const& filepath, int indent = 4);

/*
    [RETRIEVING] Passing values from a json file to individual drink objects, accepts different params
*/
// template <typename T>
// [[nodiscard]] std::optional<T> RetrieveValue(Json const& obj, std::string const& key);

}