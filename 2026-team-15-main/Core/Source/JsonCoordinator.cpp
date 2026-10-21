#include "JsonCoordinator.hpp"

namespace Ecs{
    //Private Functions
    bool JsonCoord::Apply(Entity e, Json const& jsonfile, std::string const& pos, ErrorChecker& errsvec){
        if(!jsonfile.is_object()){
            errsvec.push_back( pos + ": Components must be an object");
            return false;
        }

        for(auto const& [name,data] : jsonfile.items()){
            auto p = io.find(name);
            if (p == io.end()) {
                errsvec.push_back( pos + ": unknown component [" + name + "]");
                return false;
            }

            try { p->second.load(cord, e, data); }
            catch (Json::exception const& ex) {
                errsvec.push_back(pos + "." + name + ": " + ex.what());
                return false;
            }
        }
        return true;
    }

    
    //Public functions
    std::optional<Entity> JsonCoord::Spawn(Json const& jsonfile, std::string const& path, ErrorChecker& errsvec)
    {
        //Initial check
        if (!jsonfile.contains("Components")) {
            errsvec.push_back(path + ": missing 'Components'");
            return std::nullopt;
        }

        //Create a fresh entity ID 
        Entity e = cord.CreateEntity();
        if (!Apply(e, jsonfile["Components"], path, errsvec)) {
            cord.DestroyEntity(e);
            return std::nullopt;
        }

        return e;
    }

    /*
        Attemps to serialize via Snapshot (json obj) and path
    */
    bool JsonCoord::Saving(std::string const& path, ErrorChecker& errsvec)
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
        Parses file via filename. Includes a Rollback to Safety mechanism in case
        of corrupted JSON files
    */
    bool JsonCoord::Loading(std::string const& path, ErrorChecker& errsvec){
        
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


    //Private Funcs;
    /*
        Functions makes a root json and stores entities that exist into it
    */
    Json JsonCoord::Snapshot(){

        //Root structure will follow as 
        Json out = { {"schema_version", 1}, {"entities", Json::object()} };

        for (Entity e : cord.LivingEntities()) {
            Json comps = Json::object();
            //for each entity, check if it holds a component. If it does it gets attached to the json
            for (auto const& [name, parts] : io){
                
                //.save is under IoParts.
                if (auto j = parts.save(cord, e)) comps[name] = *j;
            }

            //Attaches the entity's components under its string ID
            out["entities"][std::to_string(e)] = { {"Components", comps} };
        }
        return out;
    }

    /*
        Preserves exact identity ID string read from json file
    */
    bool JsonCoord::LoadEntity(std::string const& idStr, Json const& j, std::string const& path, ErrorChecker& errsvec){
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
        if (raw >= MAX_ENTITIES) { errsvec.push_back(path  + ": id out of range"); return false; }
        if (!j.contains("Components")) { errsvec.push_back(path  + ": missing 'Components'"); return false; }

        //Additional checks for different keys that result in the same id. E.g. 07 and 7
        Entity id = static_cast<Entity>(raw);
        if (cord.isAlive(id)) {
            errsvec.push_back(path + ": duplicate entity id " + std::to_string(id));
            return false;
        }

        //Forces coordinator to spawn an entity with exact ID
        Entity e = cord.CreateEntityWithId(static_cast<Entity>(raw));
        return Apply(e, j["Components"], path, errsvec);
    }

    /*
        Loads entities with the term 
    */
    bool JsonCoord::LoadingEntities(Json const& root, std::string const& path, ErrorChecker& errsvec){

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

}