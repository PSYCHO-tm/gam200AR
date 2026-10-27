#include "JsonSceneCoordinator.hpp"
#include "DefinitionLoader.hpp"

namespace Ecs{
    //Private Functions

    /*
        Returns the "Components" object of a definition / entity, or nullptr (and an error)
        if it is missing or not an object.
    */
    static Json const* FindComponents(Json const& file, std::string const& location, ErrorChecker& errs)
    {
        auto pos = file.find("Components");
        if (pos == file.end() || !pos->is_object()) {
            errs.push_back(location + ": missing 'Components' object");
            return nullptr;
        }
        return &*pos;
    }

    /*
        Helper function that checks against typo, used for ApplyComponents 
    */
    // static bool ValidateComponentSubKeys(std::string const& compName, Json const& data, std::string const& where, ErrorChecker& errs) {
    //     if (!data.is_object()) return true;

    //     if (compName == "Name") {
    //         return ValidateKnownKeys(data, {"Value"}, where + ".Name", errs);
    //     } 
    //     else if (compName == "Units") {
    //         return ValidateKnownKeys(data, {"Value"}, where + ".Units", errs);
    //     } 
    //     else if (compName == "RecipeRef") {
    //         return ValidateKnownKeys(data, {"Id"}, where + ".RecipeRef", errs);
    //     } 
    //     else if (compName == "Temp") {
    //         return ValidateKnownKeys(data, {"Status"}, where + ".Temp", errs);
    //     } 
    //     else if (compName == "Attributes") {
    //         // Catches typos like "Alowed" instead of "Allowed"
    //         return ValidateKnownKeys(data, {"Allowed"}, where + ".Attributes", errs);
    //     }
    //     return true;
    // }

//=========================================================================================================================
//Private Funcs;
    /*
        Wrapper that runs CheckingEntity
    */
    bool JsonScene::ApplyComponents(Entity e, Json const& jsonfile, std::string const& pos, ErrorChecker& errsvec, bool dryRun){
        
        assert(jsonfile.is_object() && "callers must check with FindComponents first");

        for(auto const& [name, data] : jsonfile.items()){
            auto p = io.find(name);
            if (p == io.end()) {
                errsvec.push_back(pos + ": unknown component [" + name + "]");
                return false;
            }

            // Skip Free-forms like Props
            if (name != "Props" && data.is_object()) {
                Json blank = p->second.bydefault();
                if (blank.is_object()) {
                    for (auto const& [k, v] : data.items()) {
                        if (!blank.contains(k)) {
                            errsvec.push_back(pos + "." + name + ": unknown or typo'd key '" + k + "'");
                            return false;
                        }
                    }
                }
            }

            try { 
                if (dryRun) p->second.check(data);              // testing the conversion
                else        p->second.assign(cord, e, data);    // attach to the entity
            }
            catch (Json::exception const& exep) {
                errsvec.push_back(pos + "." + name + ": " + exep.what());
                return false;
            }
        }

        return true;
    }

    /*
        Functions makes a root json and stores entities that exist into it
    */
    Json JsonScene::Snapshot(){

        //Root structure will follow as 
        Json out = { {"schema_version", 1}, {"entities", Json::object()} };

        for (Entity e : cord.LivingEntities()) {
           
            //Attaches the entity's components under its string ID
            out["entities"][std::to_string(e)] = { {"Components", Describe(e)} };
        }
        return out;
    }

    /*
        Preserves exact identity ID string read from json file
    */
    bool JsonScene::LoadEntity(std::string const& idStr, Json const& j, std::string const& path, ErrorChecker& errsvec){
        unsigned long raw = 0;

        //Converts json key strings into unsigned int. If invalid, throw
        try { 
            std::size_t used = 0;
            raw = std::stoul(idStr, &used);
            
            if(used != idStr.size()){
              errsvec.push_back(path + ": id contains trailing non-numeric characters [" + idStr + "]");
                return false;  
            }
        }
        catch (...) { 
            errsvec.push_back(path + ": id is not a number"); 
            return false; 
        }

        //If converted ID exceeds engine's maximum entity limit
        if (raw >= MAX_ENTITIES) { 
            errsvec.push_back(path  + ": id out of range"); 
            return false; 
        }
        
        Json const* comps = FindComponents(j, path, errsvec);
        if(!comps) return false;

        //Additional checks for different keys that result in the same id. E.g. 07 and 7
        Entity id = static_cast<Entity>(raw);
        if (cord.isAlive(id)) {
            errsvec.push_back(path + ": duplicate entity id " + std::to_string(id));
            return false;   
        }

        //Forces coordinator to spawn an entity with exact ID
        Entity e = cord.CreateEntityWithId(id);
        return ApplyComponents(e, *comps, path, errsvec, false);
    }

    /*
        Iterates through an entire collection of entities stored within a scene file's root JSON structure
    */
    bool JsonScene::LoadingEntities(Json const& root, std::string const& path, ErrorChecker& errsvec){
        
        //type is of idStr, while def is json function, binding it to the json's key [entities]'s value
        for (auto const& [idStr, def] : root.at("entities").items())
        {
            if (!LoadEntity(idStr, def, path + ": entities." + idStr, errsvec))
            {   
                return false;
            }
        }

        return true;
    }


    /*
        Shared by GetField and SetField: Finds the components and check if the field exists
        [Private Function] located here as needs to be initialized first
    */

    bool JsonScene::LocateField(Entity e, std::string const& component, std::string const& field, std::string const& pos,
    ErrorChecker& errsvec, IoParts const*& parts, Json& currentf){
        auto it = io.find(component);

        if(it == io.end()){
            errsvec.push_back(pos + ": unknown component [" + component + "]");
            return false;
        }

        if (!cord.isAlive(e))
        { 
            errsvec.push_back(pos + ": no such entity"); 
            return false; 
        }

        auto saved = it->second.save(cord, e); // the component as Json              
        if (!saved) 
        { 
            errsvec.push_back(pos + ": entity has no [" + component + "]"); 
            return false; 
        }

        //Segment catches typoes that dont relate to field
        if (!saved->is_object() || !saved->contains(field)) {
            errsvec.push_back(pos + ": unknown field");
            return false;
        }

        parts   = &it->second;
        currentf = std::move(*saved);
        return true;
    }
    //========================================================================================================================
    //Public functions
    /*
        Validates the presence of a "Components" object and instantiates a fresh entity 
        populated with those components.
    */
    std::optional<Entity> JsonScene::Spawn(Json const& jsonfile, std::string const& path, ErrorChecker& errsvec)
    {
        Json const* comps = FindComponents(jsonfile, path, errsvec);
        if (!comps) return std::nullopt;

        //Initial check
        if (cord.NumLiving() >= MAX_ENTITIES) {
            errsvec.push_back(path + ": world is full (MAX_ENTITIES = " + std::to_string(MAX_ENTITIES) + ")");
            return std::nullopt;
        }

        //Create a fresh entity ID 
        Entity e = cord.CreateEntity();
        if (!ApplyComponents(e, *comps , path, errsvec, false)) {
            cord.DestroyEntity(e);
            return std::nullopt;
        }

        return e;
    }

    /*
        Generates a complete world snapshot and serializes it out to a target JSON file path
    */
    bool JsonScene::Saving(std::string const& path, ErrorChecker& errsvec)
    {
        //Initializing root Json schema
        if (!SerializeObj(Snapshot(), path)) 
        {
            errsvec.push_back(path + ": could not write file");
            return false;
        }
        
        return true;
    }


    /*
        Parses a scene file and saves old details in a snapshot, attempts to load the new entities,
        and automatically restores the snapshot if validation or parsing fails
    */
    bool JsonScene::Loading(std::string const& path, ErrorChecker& errsvec){
        
        //Attempts to parse
        auto j = DeserializePath(path);
        if(!j) { 
            errsvec.push_back(path + ": unreadable or invalid JSON");
            return false; 
        }

        //Checks if json root has a top level "entities"
        if (!j->contains("entities") || !(*j)["entities"].is_object()) {
            errsvec.push_back(path + ": missing 'entities' object");
            return false;
        }

        //Snapshot and destroy all Current Active entities
        Json backup = Snapshot();
        cord.DestroyAll();

       
        if(!LoadingEntities(*j, path, errsvec)){
            cord.DestroyAll();

            //Creates a dummy error log vec
            ErrorChecker ignorevec;

            //Restores the scene via snapshot
            if(!LoadingEntities(backup, "backup" , ignorevec)){
                errsvec.push_back("backup restore failed: scene might be incomplete");
            }
            else
            {
                errsvec.push_back(path + ": load failed, previous scene restored");
            }
            return false;
        }

        return true;
    }

    /*
        Ensures that a template / blueprint is correctly formated before use
    */
    bool JsonScene::CheckDefinition(Json const& def, std::string const& where, ErrorChecker& errs){
        
        Json const* comps = FindComponents(def, where, errs);

        return comps && ApplyComponents(0, *comps, where, errs, true);   
    }

    /*
        Returns the sorted list of registered component names
        Meant for an editor / console later on
    */
    std::vector<std::string> JsonScene::ComponentNames() const
    {
        std::vector<std::string> names;
        for (auto const& [name, parts] : io)
        { 
            names.push_back(name);
        }
        
        std::sort(names.begin(), names.end());
        
        return names;
    }
    
    /*
        Returns one live entity's component as Json
    */
    Json JsonScene::Describe(Entity e)
    {
        Json comps = Json::object();
        for (auto const& [name, parts] : io)
        {
            if (auto j = parts.save(cord, e)){

                comps[name] = *j;
            }
        }
        
        return comps;
    }


   
    /*
        Changes a field of a live component.
        new Json is converted to new struct BEFORE anything is overwritten
    */
    bool JsonScene::SetField(Entity e, std::string const& comp, std::string const& field, Json const& value, ErrorChecker& errs)
    {
        std::string loc = "entity " + std::to_string(e) + "." + comp + "." + field;

        IoParts const* p = nullptr;
        Json curr;

        if(!LocateField(e, comp, field, loc, errs, p, curr)) return false;

        curr[field] = value;
        try {

            // converts first, so a bad value changes nothing
            p->assign(cord, e, curr); 
        }         
        catch (Json::exception const& e) { 
            errs.push_back(loc + ": " + e.what()); 
            return false; 
        }

        return true;
    }

     /*
        Reads a field of a live component by name.
    */
    std::optional<Json> JsonScene::GetField(Entity e, std::string const& component, std::string const& field, ErrorChecker &errs){

        std::string loc = "entity " + std::to_string(e) + "." + component + "." + field;
        IoParts const* p = nullptr;
        Json curr;

        if(!LocateField(e, component, field, loc, errs, p, curr)) return std::nullopt;

        return curr.at(field);
    }

    /*
        Describes a component TYPE. Works without an entity
    */
    std::optional<Json> JsonScene::Schema(std::string const& component) const{

        auto it = io.find(component);
        if(it == io.end()) return std::nullopt;

        Json defconstruct = it->second.bydefault();
        Json out = Json::object();

        if(defconstruct.is_object()){
            for(auto const& [field, val] : defconstruct.items()){

                /*
                    Int and float is seperated as can't be differentiated
                    by nlohmann. Would put both under "number"
                */
                if(val.is_number_integer()) out[field] = "integer";
                
                else if(val.is_number_float()) out[field] = "float";
                
                else out[field] = val.type_name();
            }
        }

        return out;
   }

   
}