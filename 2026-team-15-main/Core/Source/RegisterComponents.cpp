#include "RegisterComponents.hpp"
#include "sharedlib.hpp"

namespace Ecs{

     /*
        [Factory]
        Builds ONE entity from a definition in the Database.
        kind      = kDrinks or kIngredients
        id        = the definition's key, e.g. "Kopi"
        overrides = changes to apply on top, keyed by component name, e.g.

        The new entity always gets a DefRef {Kind, Id, Overrule}.
        On any problem: returns nullopt, writes a message to errs, destroys
    */
    std::optional<Entity> SpawnInstance(Ecs::JsonScene& jc, Ecs::Database const& db, std::string const& kind, 
    std::string const& id, Json const& overrides, ErrorChecker& errs)
    {
       //Add on here when other keys are needed to be updated
        Template<Json> const* table = nullptr;
        if (kind == kDrinks){
            table = &db.drinks;
        }
        else if (kind == kIngredients) {
            table = &db.ingredients;
        }
        else {
            errs.push_back("SpawnInstance: unknown definition kind '" + kind + "'");
            return std::nullopt;
        }
 
        //Finding the definition
        std::string pos = kind + "." + id;

        auto tmp = table->find(id);

        if (tmp  == table->end()) {
            errs.push_back(pos + ": no such definition");
            return std::nullopt;
        }
 
        // Work on a copy, so that the Database itself is never changed
        Json def = tmp ->second;
        if (!def.is_object() || !def.contains("Components") || !def["Components"].is_object()) {
            errs.push_back(pos + ": definition has no 'Components' object");
            return std::nullopt;
        }
 
        // Apply the overrides on top of the copy.
        // null means "no overrides". Anything else must be an object.
        Json patch = Json::object();
        if (!overrides.is_null()) {
            if (!overrides.is_object()) {
                errs.push_back(pos + ": overrides must be an object");
                return std::nullopt;
            }
            patch = overrides;
        }

        // merge_patch: matching keys are replaced, objects merge key by key,
        // arrays are replaced whole, and a null value deletes that key.
        def["Components"].merge_patch(patch);
 
        // Targetting the inner key of Json file "DefRef"
        // If exists, overrides it with a fresh Json values
        def["Components"]["DefRef"] = Json{ {"Kind", kind}, {"Id", id}, {"Overrides", patch} };
 
        //Spawns the entitiy, performing cleanup if necessary
        return jc.Spawn(def, pos, errs);
    }
 
 
    /*
        Reads a scene file and spawns every instance listed in it.
        Adds to the world (it does not clear it): *Call world.DestroyAll() first if you
        want to replace the current scene
        
        If ANY instance fails, the ones already created are destroyed again,
        and nullopt is returned. On success returns the new entities, in file order.
    */
    std::optional<std::vector<Entity>> LoadScene(Coordinator& world, JsonScene& jc, Database const& db, 
    std::string const& path, ErrorChecker& errs)
    {
        auto file = DeserializePath(path);
        if (!file) {
            errs.push_back(path + ": unreadable or invalid JSON");
            return std::nullopt;
        }

        auto list = file->find("instances");
        if (list == file->end() || !list->is_array()) {
            errs.push_back(path + ": missing 'instances' array");
            return std::nullopt;
        }
 
        std::vector<Entity> made;
 
        // If anything goes wrong, destroy what was made
        auto rollback = [&]() {
            for (Entity e : made) world.DestroyEntity(e);
        };
 
        //Parsing individual instances
        for (size_t i = 0; i < list->size(); ++i) {
            Json const& item = (*list)[i];
            std::string where = path + ": instances[" + std::to_string(i) + "]";
 
            // Each instance needs a string "kind" and a string "def"
            auto kind = item.find("kind");
            auto def  = item.find("def");
            if (kind == item.end() || !kind->is_string() ||
                def  == item.end() || !def->is_string()) {
                errs.push_back(where + ": needs a string 'kind' and a string 'def'");
                rollback();
                return std::nullopt;
            }
 
            // "overrides" is optional, if not found, defaults to an empty Json
            auto over = item.find("overrides");
            Json overrides = (over == item.end()) ? Json::object() : *over;
 
            //offloads to SpawnInstance to instantiate entity
            auto e = SpawnInstance(jc, db, kind->get<std::string>(), def->get<std::string>(),
                                   overrides, errs);
            if (!e) {
                errs.push_back(where + ": could not spawn, scene load cancelled");
                rollback();
                return std::nullopt;
            }
            made.push_back(*e);
        }

        return made;
    }
}