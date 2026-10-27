/*

    Brief:

    Option A:
    struct yourStruct : System{
    Coordinator* world = nullptr; // Pointer to access components

    void Update() {
        for (Entity e : entityVec) { // entityVec is inherited automatically
            auto& transform = world->GetComponent<Transform>(e);
            your logic subsequently...
    }

    Option B:
    Call this instead ==>
    auto sys = AddExternalSystem<YourStructName, ... >(world);
    
    where ... is any Component types you require from the ECS
    ==> Under RegisterComponents.hpp

*/
#pragma once
#include "sharedlib.hpp"
#include "Coordinator.hpp"
#include "JsonSceneCoordinator.hpp"
#include "DefinitionLoader.hpp"
#include "RegisterComponents.hpp"
#include <type_traits>

//This file marks the information getting and handling during gameloop
    namespace Ecs{
 
    // ------------------------------------------------------------------
    //  1) Registration helpers (generic: they know nothing about coffee)
    // ------------------------------------------------------------------
 
    /*
        At compile time, make a signature accordingly to the 
        variables of the struct given
    */
    template<typename... Types>
    Signature SignatureOf(Coordinator& world)
    {
        Signature s;
        (s.set(world.GetComponentType<Types>()), ...);       // one set() per listed type
        return s;
    }
 
    /*
        Function automates setting up a system with ECS
    */
    template<typename T, typename... Types>
    std::shared_ptr<T> AddSystem(Coordinator& world)
    {
        static_assert(std::is_base_of<System, T>::value, "A system must derive from Ecs::System:  struct MySystem : Ecs::System");
 
        //Registers the system
        auto sys = world.RegisterSystem<T>();
        world.SetSystemSignature<T>(SignatureOf<Types...>(world));
        return sys;
    }

    /*
        Multi inheritance Wrapper struct that takes in the 
        System logic and any inputting struct logic
    */
    template<typename yourStructs>
    struct AdaptedSystem : System, yourStructs {};
 
    template<typename yourStructs, typename... Types>
    std::shared_ptr<AdaptedSystem<yourStructs>> AddExternalSystem(Coordinator& world)
    {
        static_assert(!std::is_base_of<System, yourStructs>::value, "This struct already derives from Ecs::System, use AddSystem instead");
 
        return AddSystem<AdaptedSystem<yourStructs>, Types...>(world);
    }
 
 

    /*
        Function checks how much the user score according to how close they were
        to replicate the recipe JSON 
    */ 
    double ScoringCup(RecipeDef const& recipe, Contents const& cup);
 
    /*
        Picks a grade from tiers that are ordered highest Score first 
        taken from Grading Json
    */
    std::string GradeFor(double score, std::vector<GradeTier> const& tiers);
}


/*
    Seperate, to store the logic for ScenarioRunner
*/
namespace Ecs{
    /*
        Default Way to call inheritance from System
    */
    struct DisplaySystem : System {
        Coordinator* world = nullptr;      // set once after AddSystem (systems are default-constructed)
        void Show() const;
    };
 
    /*
        Defauly way 2 to call inheritance from system. Points at the Database OBJECT
        so reloaded data is seen automatically
    */
    struct RecipeCheckSystem : System {
        Coordinator*    world = nullptr;
        Database const* db    = nullptr;   
 
        /*
            Scores every entity in this system's list and gives each a Result component.
            gradingSet = a key in Grading.json. Returns false (message in errs) if something is missing.
        */
        bool Evaluate(std::string const& gradingSet, ErrorChecker& errs);
    };
 
    /*
        Personal struct to make a drink report for end of game
    */
    struct DrinkReport {
        Entity entity = 0;
        std::string drink;
        double score = 0;
        std::string grade;
    };
 
    /*
        Personal struct to make a scenario report for end of game
    */
    struct ScenarioReport {
        std::string scenario;
        std::vector<DrinkReport> drinks;
        double average = 0;
        std::string grade;
    };
 
    class ScenarioRunner {
    public:
        /*
            Default Constructor
        */
        ScenarioRunner(Coordinator& w, JsonScene& s, Database const& d, RecipeCheckSystem& c) : 
        world(w), scene(s), db(d), check(c) {}
 
        /*
            Spawns every order in the scenario (an empty cup each). Replaces any earlier run.
        */
        bool Start(std::string const& scenarioName, ErrorChecker& errs);
 
        /*
            Puts some of an ingredient into a drink's cup. Amounts add up if you call it twice.
        */
        bool Add(Entity drink, std::string const& ingredient, double amount, std::map<std::string, std::string> const& attributes, 
        ErrorChecker& errs);
 
        /*
            Scores every order and fills the report.
        */
        bool Finish(ScenarioReport& out, ErrorChecker& errs);
 
        std::vector<Entity> const& Orders() const { return orders; }
 
    private:
        void ClearOrders();
 
        Coordinator&       world;
        JsonScene&         scene;
        Database const&    db;             // the Database object itself, not a copy
        RecipeCheckSystem& check;
 
        std::string         current;
        std::vector<Entity> orders;
    };
 
 
   
}

// ------------------------------------------------------------------
//  Systems will also be stored under one location for easy access
// ------------------------------------------------------------------
namespace Ecs{
    // One pointer per system, so the app can call them later.
    struct GameSystems {
        //Sample format
        // std::shared_ptr<MySystem>       sys;

        std::shared_ptr<DisplaySystem>     display;
        std::shared_ptr<RecipeCheckSystem> recipeCheck;


    };
 
    /*
        The system-side twin of RegisterAllComponents.
        Call ONCE, after RegisterAllComponents and BEFORE any entity is spawned.
        db must be the same Database object that Load_All fills.
    */
    inline GameSystems RegisterAllSystems(Coordinator& world, Database const& db)
    {
        GameSystems s;
 
        s.display = AddSystem<DisplaySystem, Name, Temp>(world);
        s.display->world = &world;
 
        s.recipeCheck = AddSystem<RecipeCheckSystem, RecipeRef, Contents>(world);
        s.recipeCheck->world = &world;
        s.recipeCheck->db = &db;
 
        // Sample format
        //   s.mine = AddSystem<MySystem, ComponentA, ComponentB>(world);
        //   s.mine->world = &world;                    // only if your system keeps a pointer
 
        return s;
    }
}