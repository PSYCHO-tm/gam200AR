#include "EquipmentDef.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

namespace
{
    using Json = nlohmann::json;

    bool ReadVec3(Json const& json, std::string const& where, Vector3D& out, std::string& error)
    {
        if (!json.is_array() || json.size() != 3 || !json[0].is_number() || !json[1].is_number() ||
            !json[2].is_number())
        {
            error = where + ": expected an array of 3 numbers";
            return false;
        }
        out = Vector3D{json[0].get<float>(), json[1].get<float>(), json[2].get<float>()};
        return true;
    }

    bool ReadString(Json const& json, char const* key, std::string const& where, std::string& out, bool required,
                    std::string& error)
    {
        if (!json.contains(key))
        {
            if (required)
            {
                error = where + "." + key + ": required";
            }
            return !required;
        }
        if (!json[key].is_string())
        {
            error = where + "." + key + ": expected a string";
            return false;
        }
        out = json[key].get<std::string>();
        return true;
    }

    bool ParseInteractions(Json const& json, std::string const& where, std::vector<InteractionDef>& out,
                           std::string& error)
    {
        if (!json.is_object())
        {
            error = where + ": expected an object of action ids";
            return false;
        }
        for (auto const& item : json.items())
        {
            std::string const here = where + "." + item.key();
            Json const&       body = item.value();
            if (!body.is_object())
            {
                error = here + ": expected an object";
                return false;
            }
            InteractionDef def;
            def.action = item.key();
            if (!ReadString(body, "Label", here, def.label, false, error) ||
                !ReadString(body, "Ingredient", here, def.ingredient, false, error) ||
                !ReadString(body, "Units", here, def.units, false, error))
            {
                return false;
            }
            if (def.label.empty())
            {
                def.label = def.action;
            }
            if (body.contains("Quantity"))
            {
                if (!body["Quantity"].is_number() || body["Quantity"].get<float>() < 0.0f)
                {
                    error = here + ".Quantity: expected a number that is 0 or more";
                    return false;
                }
                def.quantity = body["Quantity"].get<float>();
            }
            out.push_back(def);
        }
        return true;
    }
}

bool EquipmentLibrary::Parse(std::string const& text, std::string const& sourceName, std::string& error)
{
    Json const json = Json::parse(text, nullptr, false);
    if (json.is_discarded() || !json.is_object())
    {
        error = sourceName + ": invalid JSON";
        return false;
    }
    if (!json.contains("EQUIPMENT") || !json["EQUIPMENT"].is_object() || json["EQUIPMENT"].empty())
    {
        error = sourceName + ": 'EQUIPMENT' must be a non-empty object";
        return false;
    }

    std::vector<EquipmentDef> result;
    for (auto const& item : json["EQUIPMENT"].items())
    {
        std::string const where = sourceName + ": " + item.key();
        Json const&       body  = item.value();
        if (!body.is_object())
        {
            error = where + ": expected an object";
            return false;
        }
        EquipmentDef def;
        def.id   = item.key();
        def.name = item.key();
        if (!ReadString(body, "Name", where, def.name, false, error) ||
            !ReadString(body, "Mesh", where, def.meshPath, true, error))
        {
            return false;
        }
        if (body.contains("Position") && !ReadVec3(body["Position"], where + ".Position", def.position, error))
        {
            return false;
        }
        if (body.contains("YawDeg"))
        {
            if (!body["YawDeg"].is_number())
            {
                error = where + ".YawDeg: expected a number";
                return false;
            }
            def.yawDeg = body["YawDeg"].get<float>();
        }
        if (body.contains("Scale"))
        {
            if (!body["Scale"].is_number() || body["Scale"].get<float>() <= 0.0f)
            {
                error = where + ".Scale: expected a number greater than 0";
                return false;
            }
            def.scale = body["Scale"].get<float>();
        }
        if (body.contains("Interactions") &&
            !ParseInteractions(body["Interactions"], where + ".Interactions", def.interactions, error))
        {
            return false;
        }
        result.push_back(def);
    }
    std::sort(result.begin(), result.end(), [](EquipmentDef const& a, EquipmentDef const& b) { return a.id < b.id; });
    items = std::move(result);
    return true;
}

EquipmentDef const* EquipmentLibrary::Find(std::string const& id) const
{
    for (EquipmentDef const& def : items)
    {
        if (def.id == id)
        {
            return &def;
        }
    }
    return nullptr;
}
