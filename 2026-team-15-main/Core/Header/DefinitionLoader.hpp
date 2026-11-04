#pragma once
#include "sharedlib.hpp"


namespace Ecs{
    
    //safety checker definition for Json
    struct ValidationS;
    using DefChecker = std::function<bool(Json const& def, ValidationS const& ctx)>;

    // Table name -> check for non-entity tables (RECIPES, SCENARIO, ...). Filled by RegisterAllTables.
    using TableRules = std::unordered_map<std::string, void (*)(Json const&, ValidationS const&)>;
   
    /*
        Pure data transfer objects that stores non entity rules
   */
    struct RecipeIngredient {
        double Amount = 0;
        std::map<std::string, std::string> Attributes;   //Drink Json
    };

    struct RecipeDef {
        std::unordered_map<std::string, RecipeIngredient> Ingredients;
    };
 
    struct OrderDef    { 
        std::string Drink; 
        int Count = 0; 
    };
    
    struct ScenarioDef {
        int Order = 0;
        double TimeLimitSec = 0;          //Under Scenario Json
        std::string Grading;              //Under Grading Json
        std::vector<OrderDef> Orders;
    };
 
    struct GradeTier { std::string Grade; double Score = 0; };
 
    // Instructs library to serialize and deserialize C++ structs to and from JSON.
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RecipeIngredient, Amount, Attributes)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RecipeDef, Ingredients)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(OrderDef, Drink, Count)                 // both required
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScenarioDef, Order, TimeLimitSec, Grading, Orders)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GradeTier, Grade, Score)                // both required

 
    //
    /*
        Maps a directory to a value
        Only used for DataBase, no need for concern
    */
    template<class T> 
    using Table = std::unordered_map<std::string, T>;
 
    /*
        The "blueprints" loaded from the five data files.
        Treat this as read-only and only a place to retreive default Json values
        DO NOT TOUCH the Json files respectively too
    */
    struct Database {

        std::unordered_map<std::string, Table<Json>> tables;   // "DRINKS" -> { "Kopi" -> {...} }
        std::unordered_map<std::string, std::string> config;   // Separate logic for those that will change
        std::unordered_map<std::string, std::string> scenes;   // "Main" -> "Assets/Json/Scenes.json"

        std::string startScene;
        Json const* Find(std::string const& table, std::string const& id) const;

        /*
            Finds table.id and converts it to T. nullopt if the id does not exist.
            Safe to call without try/catch: Load_All already checked every entry
            against the same T (see TABLE_LIST / COMPONENT_LIST).
        */
        template<typename T>
        std::optional<T> Get(std::string const& table, std::string const& id) const {
            Json const* j = Find(table, id);
            if (!j) return std::nullopt;
            return j->get<T>();
        }
    };
 
    /*
        Loads and validates all five files from JSON FOLDER.
        On ANY error out is left untouched and `errs` says what and where.
    */
    bool Load_All(std::string const& folder, Database& out, ErrorChecker& errs, DefChecker check = nullptr,
                  TableRules const* tables = nullptr);
 

    //bool ValidateKnownKeys(Json const& obj, std::initializer_list<const char*> allowedKeys, std::string const& pos, ErrorChecker& errs);
    /*
        Runs cross-reference sanity checks on the loaded database to ensure game rules 
        make logical sense before gameplay starts.
    */
    //ErrorChecker Validate(Database const& db);
}


namespace Ecs{
    /*
     Everything a rule needs: Rule --> Json files that requires a Json Obj internally
     The whole loaded database (for cross-references) where we are (for messages), and where errors go.
    */
    struct ValidationS{
        Database const& db;
        ErrorChecker& errs;

        std::string location;
        void Fail(std::string const& msg) const {
            errs.push_back(location + ": " + msg);
        }

        // Same database and error list, one level deeper: "DRINKS.Kopi" -> "DRINKS.Kopi.Temp"
        ValidationS At(std::string const& child) const {
            return { db, errs, location + "." + child };
        }
    };

    /*
       Reports every key in `original` that is missing from `roundTrip` (the same JSON
       after going through the C++ struct). Catches typos like "Amout".
   */
    void FindUnknownKeys(Json const& original, Json const& roundTrip, std::string const& where, ErrorChecker& errs);

    template<typename T>
    struct Rules {
        static void Check(T const&, ValidationS const&) {}   // default: no extra rules
    };

    /*
        The one check used for every component and table type:
        types + required fields -> typo'd keys -> extra Rules<T>
    */
    template<typename T>
    void CheckAs(Json const& j, ValidationS const& ctx) {
        T value{};
        try {
            value = j.get<T>();                                   // wrong type / missing required field throws
        }
        catch (Json::exception const&) {
            // A typo usually shows up as "required key not found"; name the typo'd key too
            Json const blank = Json(T{});
            if (j.is_object() && blank.is_object())
                for (auto const& [k, v] : j.items())
                    if (!blank.contains(k)) ctx.Fail("unknown key '" + k + "'");
            throw;                                                // caller records the conversion error
        }
        FindUnknownKeys(j, Json(value), ctx.location, ctx.errs);  // typos; Props passes naturally
        Rules<T>::Check(value, ctx);
    }
}


//Reading Json from filepath
namespace Ecs{
    using TextReader = std::function<std::optional<std::string>(std::string const& path)>;
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

    void SetTextReader(TextReader reader);

    /*
        [RETRIEVING] Passing values from a json file to individual drink objects, accepts different params
    */
    // template <typename T>
    // [[nodiscard]] std::optional<T> RetrieveValue(Json const& obj, std::string const& key);
}