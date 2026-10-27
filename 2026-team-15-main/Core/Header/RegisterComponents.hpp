#pragma once
#include "sharedlib.hpp"
#include "JsonSceneCoordinator.hpp"
#include "DefinitionLoader.hpp"
 
/*
  Struct Mapping::
   HOW TO ADD A NEW COMPONENT (3 steps, all in this file)
   1) Write the struct    
   2) Add its NLOHMANN_DEFINE_TYPE_... line (or to_json/from_json)
   3) Add one X(YourStruct) to COMPONENT_LIST below
  The struct name IS the key used in the JSON files ("Name", "Temp", ...)
*/
 

struct Name      { std::string Value; };
struct Units     { std::string Value; };
struct RecipeRef { std::string Id; };                 
struct Temp      { std::string Status; };   

/*
    Kind = which table: "DRINKS" or "INGREDIENTS"
    Id = the definition's key, e.g. "Kopi"
    Overrule = the changes the scene file applied on top ({} if none)
*/
  struct DefRef{ 
    std::string Kind; 
    std::string Id; 

    //Pass instance-specific modifications or overrides at the moment of spawning
    Ecs::Json Overrides = Ecs::Json::object();
};


struct Attributes {                    
    std::map<std::string, 
    std::vector<std::string>> Allowed;
};

//What player adds during his gameplay
struct Contents { 
    std::map<std::string, Ecs::RecipeIngredient> Items; 
};

//For the game-ending
struct Result   { 
    double Score = 0; 
    std::string Grade;
};

//delete if unusued
// struct Transform { 
//     float x = 0, 
//     y = 0, 
//     rotation = 0; 
// };
 
//This macro expects every single listed field to be present in JSON Obj
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Name, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Units, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RecipeRef, Id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Temp, Status)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DefRef, Kind, Id, Overrides)

//This macro allows the listed fields to be optional in the JSON
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Attributes, Allowed)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Contents, Items)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Result, Score, Grade)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Transform, x, y, rotation) //del if unused
 

/*
 ---------------- Props: loose data no system queries by type ---------------- 
    Acts as a bucket for (visual paths, audio paths, flavour data).
    A new property is a JSON edit only, no C++ and no recompile.
    "Props": { "Visual": { "Model": "Models/water.obj" }, "Roast": "dark" }
*/
struct Props { 
    std::map<std::string, Ecs::Json> Values;
};

/*
    Explicit json term for nlohmann's to_json function
    May result in clang clash, but lohmann requires this exact naming convention
    NOLINTNEXTLINE(readability-identifier-naming) -- 
*/
inline void to_json(Ecs::Json& j, Props const& p)
{
    j = Ecs::Json::object();
    for (auto const& [key, value] : p.Values) 
    {
        j[key] = value;
    }
}

/*
    Explicit json term for nlohmann's from_json function
    May result in clang clash, but lohmann requires this exact naming convention
    NOLINTNEXTLINE(readability-identifier-naming) -- 
*/
inline void from_json(Ecs::Json const& j, Props& p)
{
    if (!j.is_object()) throw Ecs::Json::type_error::create(302, "Props must be an object", &j);

    p.Values.clear();

    for (auto const& [key, value] : j.items())
    {
        p.Values[key] = value;
    }
}
 

/*
    X-Macro Design pattern to reduce registration boilerplate
    Also to prevent any duplication and sync error
*/
#define COMPONENT_LIST(X) \
    X(Name)               \
    X(Units)              \
    X(RecipeRef)          \
    X(Temp)               \
    X(Attributes)         \
    X(Props)              \
    X(DefRef)             \
    X(Contents)           \
    X(Result)
    //X(Transform) put a '\' on this when needed
    
/*
    Registers every component in the list with the ECS and the JSON layer.
    The string passed to Register is the struct's own name (via #T), so the
    struct and the JSON key won't drift apart.
*/ 
inline void RegisterAllComponents(Ecs::JsonScene& jc)
{
#define X(T) jc.Register<T>(#T);
    COMPONENT_LIST(X)
#undef X
}
 

//Building Entity from Json
namespace Ecs{

    //std::string doesnt work here..
    inline constexpr char kDrinks[]      = "DRINKS";
    inline constexpr char kIngredients[] = "INGREDIENTS";

    /*
        [Factory]
        Builds ONE entity from a definition in the Database. 
        kind      = kDrinks or kIngredients
        id        = the definition's key, e.g. "Kopi"
        overrides = changes to apply on top, keyed by component name, e.g.

        The new entity always gets a DefRef {Kind, Id, Overrule}.
        On any problem: returns nullopt, writes a message to errs, destroys
    */
    std::optional<Entity> SpawnInstance(Ecs::JsonScene& jc, Database const& db, std::string const& kind, 
        std::string const& id, Json const& overrides, ErrorChecker& errs);
 
    /*
        Offloads to SpawnInstance. Just no overrides
        Recap: Overrides let one definition produce instances that differ from each other, 
        without writing a new definition for every variation.

        Which means the addition of additional content not in original json
    */
    inline std::optional<Entity> SpawnDef(JsonScene& jc, Database const& db, std::string const& kind, 
        std::string const& id, ErrorChecker& errs)
    {
        return SpawnInstance(jc, db, kind, id, Json::object(), errs);
    }
 
    /*
        Reads a scene file and spawns every instance listed in it.
        Adds to the world (it does not clear it): *Call world.DestroyAll() first if you
        want to replace the current scene
        
        If ANY instance fails, the ones already created are destroyed again,
        and nullopt is returned. On success returns the new entities, in file order.
    */
    std::optional<std::vector<Entity>> LoadScene(Coordinator& world, JsonScene& jc, Database const& db, 
        std::string const& path, ErrorChecker& errs);
}
