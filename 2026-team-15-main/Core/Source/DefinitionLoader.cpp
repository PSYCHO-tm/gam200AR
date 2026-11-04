/*

    Brief:
    
*/
#include "DefinitionLoader.hpp"
#include "sharedlib.hpp"

namespace Ecs{
    /*
        Private Function that reports every key in original that is missing from roundTrip
    */
    void FindUnknownKeys(Json const& original, Json const& roundTrip, std::string const& where, ErrorChecker& errs)
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
}


namespace Ecs{

    static void LoadTable(std::string const& path, Database& db, ErrorChecker& errs)
    {
        auto j = DeserializePath(path);
        if (!j || !j->is_object()) {
            errs.push_back(path + ": unreadable or invalid JSON"); 
            return;
        }

        for (auto const& [tableName, entries] : j->items()) {
            if (!entries.is_object()) {
                errs.push_back(path + ": [" + tableName + "] must be an object of id -> definition");
                continue;
            }

            auto& t = db.tables[tableName];
            for (auto const& [id, def] : entries.items()) {
                if (!t.emplace(id, def).second) {
                    errs.push_back(path + ": duplicate id '" + tableName + "." + id + "'");
                }
            }
        }
    }

    /*
        Loads and validates all - files from JSON FOLDER.
        On ANY error out is left untouched and `errs` says what and where.
        -Only populates C++ data structures so engine knows it exist
    */
    bool Load_All(std::string const& folder, Database& out, ErrorChecker& errs, DefChecker check,
                  TableRules const* tables)
    {
        //Deserialize path
        auto dPath = DeserializePath(folder);
        if(!dPath || !dPath->is_object()){
            errs.push_back(folder + ": unreadable or invalid JSON");
            return false;
        }

        //Searches for KEYWORD "STORAGE" in json file
        auto storage = dPath->find("STORAGE");
        if (storage == dPath->end() || !storage->is_array()) {
            errs.push_back(folder + ": missing 'STORAGE' array");
            return false;
        }

        // Paths in the manifest are relative to the manifest's own folder
        std::string const root = std::filesystem::path(folder).parent_path().generic_string();

        Database tmp;
        ErrorChecker local;

        //checks if variables within json file is string
        for (auto const& entry : *storage) {
            if (!entry.is_string()) {
                local.push_back(folder + ": STORAGE entries must be strings");
                continue;
            }

            LoadTable(root + "/" + entry.get<std::string>(), tmp, local);
        }

        //Function that helps read the Path for keys: "CONFIG" and "SCENES" in JSONManifest
        auto readPaths = [&](char const* key, std::unordered_map<std::string, std::string>& destination)
            {
                auto file = dPath->find(key);
                if(file == dPath->end()) return;

                if(!file->is_object()) {
                    local.push_back(folder + ": '" + key + "' must be an object of name -> path");
                    return;
                }

                for(auto const& [name, val] : file->items()){
                    if(val.is_string()){
                        destination[name] = root + "/" + val.get<std::string>();
                    }
                    else {
                        local.push_back(folder + ": " + key + "." + name + " must be a string path");
                    }
                }
            };

        readPaths("CONFIG", tmp.config);
        readPaths("SCENES", tmp.scenes);

        //This part is to only check for starting scene
        tmp.startScene = dPath->value("START_SCENE", std::string{});
        if (!tmp.startScene.empty() && !tmp.scenes.count(tmp.startScene)){
            local.push_back(folder + ": START_SCENE '" + tmp.startScene + "' is not listed in SCENES");
        }

        /*
            One validation pass over everything, after ALL files are loaded
            (so cross-references between files always resolve):
              - entries with "Components" -> entity definitions -> check (JsonScene::CheckDefinition)
              - everything else           -> the rule registered for its table (TABLE_LIST)
        */
        if (local.empty()) {
            for (auto const& [table, defs] : tmp.tables) {
                for (auto const& [id, def] : defs) {
                    ValidationS ctx{ tmp, local, table + "." + id };
                    try {
                        if (def.contains("Components")) {
                            if (check) check(def, ctx);
                        }
                        else if (tables) {
                            auto rule = tables->find(table);
                            if (rule != tables->end()) rule->second(def, ctx);
                            else ctx.Fail("no rules registered for table '" + table + "' (add it to TABLE_LIST)");
                        }
                    }
                    catch (Json::exception const& e) {
                        ctx.Fail(e.what());
                    }
                }
            }
        }

        errs.insert(errs.end(), local.begin(), local.end());
        if (!local.empty()) return false;   // rollback: out untouched

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

Json const* Database::Find(std::string const& table, std::string const& id) const {
    auto t = tables.find(table);
    if (t == tables.end()) return nullptr;

    auto e = t->second.find(id);
    return e == t->second.end() ? nullptr : &e->second;
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