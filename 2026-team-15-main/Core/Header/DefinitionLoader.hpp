#pragma once
#include "sharedlib.hpp"    
 
namespace Ecs{
    
    //safety checker definition for Json
    using DefChecker = std::function<bool(Json const& def, std::string const& where, ErrorChecker& errs)>;
   
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
 
    // Instructs ibrary to serialize and deserialize C++ structs to and from JSON.
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RecipeIngredient, Amount, Attributes)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RecipeDef, Ingredients)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(OrderDef, Drink, Count)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScenarioDef, Order, TimeLimitSec, Grading, Orders)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GradeTier, Grade, Score)
 
    //
    /*
        Maps a directory to a value
        Only used for DataBase, no need for concern
    */
    template<class T> 
    using Template = std::unordered_map<std::string, T>;
 
    /*
        The "blueprints" loaded from the five data files.
        Treat this as read-only and only a place to retreive default Json values
        DO NOT TOUCH the Json files respectively too
    */
    struct Database {

        // Entity definitions. Kept as raw Json: JsonCoord::Spawn turns them into
        // entities and reports any unknown component or wrong type at that point.
        Template<Json> drinks;
        Template<Json> ingredients;
 
        // Rules and config
        Template<RecipeDef> recipes;
        Template<ScenarioDef> scenarios;
        Template<std::vector<GradeTier>> grading;
    };
 
    /*
        Loads and validates all five files from JSON FOLDER.
        On ANY error out is left untouched and `errs` says what and where.
    */
    bool Load_All(std::string const& folder, Database& out, ErrorChecker& errs, DefChecker check = nullptr);
 

    bool ValidateKnownKeys(Json const& obj, std::initializer_list<const char*> allowedKeys, std::string const& pos, ErrorChecker& errs);
    /*
        Runs cross-reference sanity checks on the loaded database to ensure game rules 
        make logical sense before gameplay starts.
    */
    ErrorChecker Validate(Database const& db);
}
 

//Reading Json from filepath
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