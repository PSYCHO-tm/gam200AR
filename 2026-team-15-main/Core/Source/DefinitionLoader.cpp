/*

    Brief:
    
*/
#include "DefinitionLoader.hpp"
#include "sharedlib.hpp"

namespace Ecs{
 
    
    /*
        Private Function that reports every key in original that is missing from roundTrip
    */
    static void FindUnknownKeys(Json const& original, Json const& roundTrip, std::string const& where, ErrorChecker& errs)
    {
        if (original.is_object() && roundTrip.is_object()) {
            for (auto const& [k, v] : original.items()) {
                auto it = roundTrip.find(k);
                if (it == roundTrip.end())
                    errs.push_back(where + ": unknown key '" + k + "'");
                else
                    FindUnknownKeys(v, *it, where + "." + k, errs);
            }
        }
        else if (original.is_array() && roundTrip.is_array()) {
            for (size_t i = 0; i < original.size() && i < roundTrip.size(); ++i)
                FindUnknownKeys(original[i], roundTrip[i],
                                where + "[" + std::to_string(i) + "]", errs);
        }
    }
    
    /*
        Checks if JSON file has a "Components" key within it
    */
    static Json const* ComponentExist(Json const& file, char const* name)
    {
        auto comps = file.find("Components");
        if (comps == file.end() || !comps->is_object()) return nullptr;

        //Then checks if it contains a key 'name'
        auto key = comps->find(name);

        return key == comps->end() ? nullptr : &*key;
    }
 
    /*
        Private function that finds for a specific component field, returning it as c++ string
    */
    static std::string StrField(Json const& file, char const* key, char const* innerkey)
    {
        Json const* c = ComponentExist(file, key);
        if (!c) return {};

        auto pos = c->find(innerkey);

        return (pos != c->end() && pos->is_string()) ? pos->get<std::string>() : std::string{};
    }
 
    /*
        Seperate private function that is mainly used for Json Ingredients
        due to multiple states to account for
    */
    static bool IngredientAllows(Json const& file, std::string const& key, std::string const& val)
    {
        Json const* attributes = ComponentExist(file, "Attributes");
        if (!attributes) return false;
 
        //iterator if the key 'Allowed' exists
        auto allowed = attributes->find("Allowed");
        if (allowed == attributes->end() || !allowed->is_object()) return false;
 
        auto list = allowed->find(key);
        if (list == allowed->end() || !list->is_array()) return false;
 
        for (auto const& v : *list){
            if (v.is_string() && v.get<std::string>() == val) return true;
        }

        return false;
    }
 
    
    /*
        Reads a Json file, extracting a specific top-level object, and parses
        each entry into c++ data types. Passed onto Load_All()

        Sequence: Reads path -> json[key] (an object) -> one T per entry.
        Converts entry by entry so an error names the exact item
    */
    template<class T>
    static Template<T> Load_Table(std::string const& path, std::string const& key, ErrorChecker& errs)
    {
        //Rmb that Template is a directory
        Template<T> out;
 
        auto j = DeserializePath(path);
        if (!j) { 
            errs.push_back(path + ": unreadable or invalid JSON"); 
            return out; 
        }
 
        auto section = j->find(key);
        if (section == j->end() || !section->is_object()) {
            errs.push_back(path + ": missing top-level object [" + key + "]");
            return out;
        }
 
        for (auto const& [id, item] : section->items()) {
            // try { 
            //     out[id] = item.get<T>(); 
            // }
            try {
                T value = item.get<T>();

                // Drinks and ingredients are stored as raw Json, so there is nothing to compare
                if constexpr (!std::is_same_v<T, Json>){
                    FindUnknownKeys(item, Json(value), path + ": " + key + "." + id, errs);
                }
                out[id] = std::move(value);
            }
            catch (Json::exception const& e) {
                errs.push_back(path + ": " + key + "." + id + ": " + e.what());
            }
        }
        return out;
    }
    

    bool ValidateKnownKeys(Json const& obj, std::initializer_list<const char*> allowedKeys, std::string const& pos, 
    ErrorChecker& errs){

        if (!obj.is_object()) return true;
        bool valid = true;

        for (auto const& [k, v] : obj.items()) {
            bool found = false;

            for (auto allowed : allowedKeys) {
                if (k == allowed) { 
                    found = true; 
                    break; 
                }
            }

            if (!found) {
                errs.push_back(pos + ": unknown or typo'd key '" + k + "'");
                valid = false;
            }
        }
        
        return valid;

    }


    /*
        Runs cross-reference sanity checks on the loaded database to ensure game rules 
        make logical sense before gameplay starts.
    */
    ErrorChecker Validate(Database const& db)
    {
        ErrorChecker e;
 
        // Ingredients: MUST have 'Components' Key, with 'Name' and 'Units'
        for (auto const& [id, def] : db.ingredients) {
            if (!def.contains("Components")) {
                e.push_back("Ingredients.json: '" + id + "' is missing 'Components'");
                continue;
            }
            if (StrField(def, "Name",  "Value").empty()) e.push_back("Ingredients.json: '" + id + "' has no Name.Value");
            if (StrField(def, "Units", "Value").empty()) e.push_back("Ingredients.json: '" + id + "' has no Units.Value");
        }
 
        // Recipes: ingredients must exist, amounts positive, attribute values allowed
        for (auto const& [rid, r] : db.recipes) {
            if (r.Ingredients.empty())
                e.push_back("Recipes.json: '" + rid + "' has no Ingredients");
 
            for (auto const& [iid, use] : r.Ingredients) {
                auto ing = db.ingredients.find(iid);
                if (ing == db.ingredients.end()) {
                    e.push_back("Recipes.json: '" + rid + "' uses unknown ingredient '" + iid + "'");
                    continue;
                }
                if (use.Amount <= 0)
                    e.push_back("Recipes.json: '" + rid + "' ingredient '" + iid + "' needs Amount > 0");
 
                for (auto const& [key, val] : use.Attributes)
                    if (!IngredientAllows(ing->second, key, val))
                        e.push_back("Recipes.json: '" + rid + "' wants " + iid + "." + key +
                                    "='" + val + "', which the ingredient does not allow");
            }
        }
 
        // Drinks: MUST have a Name and a RecipeRef that points at a real recipe
        for (auto const& [did, def] : db.drinks) {
            if (!def.contains("Components")) {
                e.push_back("Drinks.json: '" + did + "' is missing 'Components'");
                continue;
            }
            if (StrField(def, "Name", "Value").empty())
                e.push_back("Drinks.json: '" + did + "' has no Name.Value");
 
            std::string recipe = StrField(def, "RecipeRef", "Id");
            if (recipe.empty())
                e.push_back("Drinks.json: '" + did + "' has no RecipeRef.Id");
            else if (!db.recipes.count(recipe))
                e.push_back("Drinks.json: '" + did + "' references unknown recipe '" + recipe + "'");
        }
 
        // Scenarios: grading set and every ordered drink must exist
        for (auto const& [sid, s] : db.scenarios) {
            if (s.TimeLimitSec < 0)
                e.push_back("Scenarios.json: '" + sid + "' has negative TimeLimitSec");
            if (!db.grading.count(s.Grading))
                e.push_back("Scenarios.json: '" + sid + "' references unknown grading set '" + s.Grading + "'");

            uint64_t totalSceneEntities{};
            
            for (auto const& o : s.Orders) {
                if (!db.drinks.count(o.Drink))
                    e.push_back("Scenarios.json: '" + sid + "' orders unknown drink '" + o.Drink + "'");

                if (o.Count <= 0){
                    e.push_back("Scenarios.json: '" + sid + "' order '" + o.Drink + "' needs Count > 0");
                }

               if(o.Count > 0) totalSceneEntities += static_cast<uint64_t>(o.Count);
            }

            if(totalSceneEntities > MAX_ENTITIES){
                e.push_back("Scenarios.json: '" + sid + "' requests " + std::to_string(totalSceneEntities) + 
                " entities, which exceeds the maximum world capacity of " + std::to_string(MAX_ENTITIES));
            }
        }
 
        // Grading: highest Score first, and the last tier is 0 so every score gets a grade
        if (db.grading.empty()) e.push_back("Grading.json: no grading sets defined");
        for (auto const& [gid, tiers] : db.grading) {
            if (tiers.empty()) { e.push_back("Grading.json: '" + gid + "' has no tiers"); continue; }
            for (size_t i = 1; i < tiers.size(); ++i)
                if (tiers[i].Score >= tiers[i - 1].Score)
                    e.push_back("Grading.json: '" + gid + "' tiers must go highest Score first");
            if (tiers.back().Score != 0)
                e.push_back("Grading.json: '" + gid + "' last tier should have Score 0");
        }
 
        return e;
    }
 
    /*
        Loads and validates all - files from JSON FOLDER.
        On ANY error out is left untouched and `errs` says what and where.
        -Only populates C++ data structures so engine knows it exist
    */
    bool Load_All(std::string const& folder, Database& out, ErrorChecker& errs, DefChecker check)
    {
        Database tmp;
        ErrorChecker local;
 
        tmp.drinks = Load_Table<Json> (folder + "/Drinks.json", "DRINKS", local);
        tmp.ingredients = Load_Table<Json> (folder + "/Ingredients.json", "INGREDIENTS", local);
        tmp.equipment = Load_Table<Json> (folder + "/Equipment.json", "EQUIPMENT", local);
        tmp.recipes = Load_Table<RecipeDef> (folder + "/Recipes.json", "RECIPES", local);
        tmp.scenarios = Load_Table<ScenarioDef> (folder + "/Scenarios.json", "SCENARIO", local);
        tmp.grading = Load_Table<std::vector<GradeTier>> (folder + "/Grading.json", "GRADING", local);
 
        //Check for schema integrity and cross-references
        if (local.empty()) {
            if(check){

                //Deep check of each entity definition
                for (auto const& [id, def] : tmp.drinks)      check(def, "Drinks.json: " + id, local);
                for (auto const& [id, def] : tmp.ingredients) check(def, "Ingredients.json: " + id, local);
                for (auto const& [id, def] : tmp.equipment)   check(def, "Equipment.json: " + id, local);
            }

            ErrorChecker another = Validate(tmp);   
            
            //insert into local error vec
            local.insert(local.end(), another.begin(), another.end());
        }

        //recieve everything from internal scope call
        errs.insert(errs.end(), local.begin(), local.end());

        //Rollback handling
        if (!local.empty()) return false; 
 
        //If no error, transfers ownership.
        out = std::move(tmp);
        return true;
    }
}



//For json filepath parser

namespace Ecs{
/*
    [READING] Parsing via stringstream, returns a Json object if successful to save
*/
    std::optional<Json> DeserializeStream(std::istream& is){
    is >> std::ws;

    if(!is || is.peek() == std::istream::traits_type::eof()) {
        std::cerr << "[DeserializeStream] Given Stringstream " << " is empty\n";
        return std::nullopt;
    }
    
    try{
     return Json::parse(is);
    }

    catch(const Json::parse_error& e){
        std::cerr << "[DeserializeStream] Invalid Json Format: " << e.what() << std::endl;
        std::cerr << "Near Byte Position: " << e.byte << std::endl;
        return std::nullopt;
    }

    catch (const Json::exception& e) {
    std::cerr << "[DeserializeStream] Unexpected Json error: " << e.what() << '\n';
    return std::nullopt;
    }
}

TextReader textReader;

void SetTextReader(TextReader reader)
{
    textReader = std::move(reader);
}

/*
    [READING] Making a Json file (delegated) by passing a file name
*/
    std::optional<Json> DeserializePath(std::string const& filepath){
    if (textReader)                                    // Android: text comes from the embedded table
    {
        std::optional<std::string> text = textReader(filepath);
        if (!text)
        {
            std::cerr << " [DeserializePath] No text found for " << filepath << '\n';
            return std::nullopt;
        }
        std::istringstream iss(*text);
        return DeserializeStream(iss);
    }

    std::ifstream ifs(filepath);                       // otherwise read a real file, as before
    if (!ifs.is_open())
    {
        std::cerr << " [DeserializePath] File " << filepath << " passed to read cannot be opened\n";
        return std::nullopt;
    }
    return DeserializeStream(ifs);
}
    


    /*
        [WRITING] Takes a Json object and transforms it into a readable Json string
    */
    bool SerializeObj(Json const& obj, std::string const& filepath, int indent) {
    // Enforce .json extension check
    if (std::filesystem::path(filepath).extension() != ".json") {
        std::cerr << "[SerializeObj] Error: Invalid file extension. Only .json is allowed.\n";
        return false;
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << " [SerializeObj] File " << filepath << " cannot be opened\n";
        return false;
    }
    file << obj.dump(indent);
    file.flush();
    return file.good();
}


/*
    [RETRIEVING] Passing values from a Json file to individual drink objects, accepts different params
// */
//     template <typename T>
//     std::optional<T> RetrieveValue(Json const& obj, std::string const& key){
   
//     if(!obj.is_object()) {
//         std::cerr << "[RetrieveValue] File " << obj << " does not exist\n";
//         return std::nullopt;
//     }

//     auto pos = obj.find(key);
//     if(pos == obj.end()){
//         std::cerr << "[RetrieveValue] Key " << key << " does not exist\n";
//         return std::nullopt;
//     }

//     try{
//         return pos->get<T>(); //nlh
//     }
//     catch(const Json::exception& e){
//         std::cerr << "[RetrieveValue] Key " << key << " failed to convert\n";
//         return std::nullopt;
//     }
// }
}