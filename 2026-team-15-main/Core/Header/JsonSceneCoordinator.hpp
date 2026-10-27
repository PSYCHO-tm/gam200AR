#pragma once
#include "sharedlib.hpp"
#include "Coordinator.hpp"

namespace Ecs{

    /*
        Helper function to convert raw JSON Chunk into a specific T and
        attaching to an entity
    */

    /*
        Builds a default-constructed T as Json. Gives the field names and types of a
        component TYPE without needing any entity.
    */
    template <typename T>
    Json DefaultComponent(){ return Json(T{}); }


    /*
        Helper function to extract a component from an active entity and
        converting back to Json Object
    */
    template<typename T>
    std::optional<Json> SaveComponent(Coordinator& cord, Entity e)
    {
        if (!cord.HasComponent<T>(e)) return std::nullopt;
        return Json(cord.GetComponent<T>(e));         
    }

    /*
        Helper function to test if a JSON object is valid for a given component type 
        without actually modifying the game world

        Low level to try and parse Json chunk into c++ struct T
    */
    template<typename T>
    void CheckComponent(Ecs::Json const& jsonfile){
        (void) jsonfile.get<T>();
    }

    /*
        Function sets a component whether or not it exists.
    */
    template <typename T>
    void AssignComponent(Coordinator& cord, Entity e, Json const& j){
        T val = j.get<T>();
        
        if(cord.HasComponent<T>(e)){
            cord.GetComponent<T>(e) = std::move(val);
        }
        else{
            cord.AddComponent<T>(e, std::move(val));
        }

    }

    //==========================================================================================================================
    // Holds the two functions for one component type
    struct IoParts{
        //(*load) is a function pointer that points to load
        // void (*load)(Coordinator&, Entity, Json const&);

        //(*save) is a function pointer that points to save etc
        std::optional<Json> (*save)(Coordinator&, Entity);

        void (*check)(Json const&);

        void (*assign)(Coordinator&, Entity, Json const&);

        //for reflection 
        Json (*bydefault)();

    };

    //==========================================================================================================================
    /*
        This whole Json class SHOULD only be used for IO SCENES
        Focus on this part
    */
    class JsonScene{
        public: 
        /*
            Initialize JsonScene Coordinator with the default coordinator
            Must be called AFTER Coordinator + Init();
        */
        explicit JsonScene(Coordinator& c) noexcept: cord(c) {}
        
        /*
            Registers a component type T into internal handler maps (io) to link load,
            save, and check functions dynamically
        */
        template <typename T>
        void Register(std::string const& name);

        /*
            Validates the presence of a "Components" object and instantiates a fresh entity 
            populated with those components.
        */
        std::optional<Entity> Spawn(Json const& jsonfile, std::string const& path, ErrorChecker& errsvec);

        /*
            Generates a complete world snapshot and serializes it out to a target JSON file path
        */
        bool Saving(std::string const& path, ErrorChecker& errsvec);
        
        /*
            Parses a scene file and saves old details in a snapshot, attempts to load the new entities,
            and automatically restores the snapshot if validation or parsing fails
        */
        bool Loading(std::string const& path, ErrorChecker& errsvec);

        /*
            Ensures that a template / blueprint is correctly formated before use
        */
        bool CheckDefinition(Json const& def, std::string const& where, ErrorChecker& errs);
        

        std::vector<std::string> ComponentNames() const;

        /*
            Returns one live entity's component as Json
        */
        Json Describe(Entity e);

        /*
            Changes a field of a live component.
            Steps: Checks component name -> Checks alive status -> component available ->
            field alr exist -> Insert new value into component's JSON and converts it back
        */
        bool SetField(Entity e, std::string const& comp, std::string const& field, Json const& value, ErrorChecker& errs);

        /*
            Reads a field of a live component by name.
        */
        std::optional<Json> GetField(Entity e, std::string const& component, std::string const& field, ErrorChecker &errs);

        /*
            Describes a component TYPE. Works without an entity
        */
        std::optional<Json> Schema(std::string const& component) const;

        private:
        // Wrapper that runs CheckingEntity
        bool ApplyComponents(Entity e, Json const& jsonfile, std::string const& path, ErrorChecker& errsvec, bool dryRun);

        //Preserves exact identity ID string read from json file
        bool LoadEntity(std::string const& idStr, Json const& j, std::string const& path, ErrorChecker& errsvec); 

        //Bulk version of LoadEntity
        bool LoadingEntities(Json const& root, std::string const& path, ErrorChecker& errsvec);

        //Shared by GetField and SetField: Finds the components and check if the field exists
        bool LocateField(Entity e, std::string const& component, std::string const& field, std::string const& pos,
        ErrorChecker& errsvec, IoParts const*& parts, Json& currentf);

        Json Snapshot();

        Coordinator& cord;
        std::unordered_map<std::string, IoParts> io{};
       
    };
}

namespace Ecs{
    
    /*
        Registers a component type T into internal handler maps (io) to link load, 
        save, and check functions dynamically
    */
    template <typename T>
    void JsonScene::Register(std::string const& name){
        cord.RegisterComponent<T>();
        
        io[name] = IoParts{ &SaveComponent<T>, &CheckComponent<T>, &AssignComponent<T>, &DefaultComponent<T>};
    }

   
}