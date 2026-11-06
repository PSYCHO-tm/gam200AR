#include "RegisterComponents.hpp"
#include "sharedlib.hpp"
#include "objLoader.hpp" // ShaderKindFromName, so the shader names checked here are the ones the renderer knows
#include <set>

namespace Ecs{

    // ---------------------------------------------------------------------------
    //  Rules: extra checks run by Load_All (and SpawnInstance) after the type check
    // ---------------------------------------------------------------------------

    // A drink's RecipeRef must point at a real recipe
    void Rules<::RecipeRef>::Check(::RecipeRef const& r, ValidationS const& ctx)
    {
        if (!ctx.db.Find(kRecipes, r.Id)) ctx.Fail("unknown recipe '" + r.Id + "'");
    }

    // Recipes: has ingredients, each exists, Amount > 0, attribute values allowed by the ingredient
    void Rules<RecipeDef>::Check(RecipeDef const& r, ValidationS const& ctx)
    {
        if (r.Ingredients.empty()) ctx.Fail("has no Ingredients");

        for (auto const& [iid, use] : r.Ingredients) {
            ValidationS here = ctx.At("Ingredients").At(iid);

            Json const* ing = ctx.db.Find(kIngredients, iid);
            if (!ing) { here.Fail("unknown ingredient"); continue; }

            if (use.Amount <= 0) here.Fail("Amount must be > 0");

            // Reuse the Attributes component struct instead of reading the JSON by hand
            ::Attributes allowed;
            auto comps = ing->find("Components");
            if (comps != ing->end() && comps->is_object()) {
                auto a = comps->find("Attributes");
                if (a != comps->end()) allowed = a->get<::Attributes>();
            }

            for (auto const& [key, val] : use.Attributes) {
                auto list = allowed.Allowed.find(key);
                bool ok = list != allowed.Allowed.end() &&
                          std::find(list->second.begin(), list->second.end(), val) != list->second.end();
                if (!ok) here.Fail(key + "='" + val + "' is not allowed by the ingredient");
            }
        }
    }

    // Scenarios: grading set exists, every ordered drink exists, counts positive, fits in the world
    void Rules<ScenarioDef>::Check(ScenarioDef const& s, ValidationS const& ctx)
    {
        if (s.TimeLimitSec < 0) ctx.Fail("TimeLimitSec must be >= 0");
        if (!ctx.db.Find(kGrading, s.Grading)) ctx.Fail("unknown grading set '" + s.Grading + "'");

        std::uint64_t total = 0;
        for (auto const& o : s.Orders) {
            if (!ctx.db.Find(kDrinks, o.Drink)) ctx.Fail("orders unknown drink '" + o.Drink + "'");
            if (o.Count <= 0) ctx.Fail("order '" + o.Drink + "' needs Count > 0");
            else total += static_cast<std::uint64_t>(o.Count);
        }

        if (total > MAX_ENTITIES)
            ctx.Fail("requests " + std::to_string(total) + " entities, more than MAX_ENTITIES (" +
                     std::to_string(MAX_ENTITIES) + ")");
    }

    // Grading: at least one tier, highest Score first, last tier is 0 so every score gets a grade
    void Rules<std::vector<GradeTier>>::Check(std::vector<GradeTier> const& tiers, ValidationS const& ctx)
    {
        if (tiers.empty()) { ctx.Fail("has no tiers"); return; }

        for (std::size_t i = 1; i < tiers.size(); ++i)
            if (tiers[i].Score >= tiers[i - 1].Score) { ctx.Fail("tiers must go highest Score first"); break; }

        if (tiers.back().Score != 0) ctx.Fail("last tier should have Score 0");
    }

    // Ingredient Attributes: every allowed list has at least one value, and no value is listed twice
    void Rules<::Attributes>::Check(::Attributes const& a, ValidationS const& ctx)
    {
        for (auto const& [key, values] : a.Allowed) {
            ValidationS here = ctx.At("Allowed").At(key);
            if (values.empty()) here.Fail("needs at least one allowed value");

            std::set<std::string> seen;
            for (auto const& v : values)
                if (!seen.insert(v).second) here.Fail("'" + v + "' is listed twice");
        }
    }

    // MeshRef: a model file is named, the shader exists, and the colour is a real colour (each part 0..1)
    void Rules<::MeshRef>::Check(::MeshRef const& m, ValidationS const& ctx)
    {
        if (m.Path.empty()) ctx.At("Path").Fail("must name the .obj file");
        if (!ShaderKindFromName(m.Shader)) ctx.At("Shader").Fail("unknown shader '" + m.Shader + "' (known: Lit)");
        for (float const c : {m.Colour.x, m.Colour.y, m.Colour.z})
            if (c < 0.0f || c > 1.0f) { ctx.At("Colour").Fail("each value must be between 0 and 1"); break; }
    }

    /*
        [Factory]
        Builds ONE entity from a definition in the Database.
        kind      = kDrinks or kIngredients
        id        = the definition's key, e.g. "Kopi"
        overrides = changes to apply on top, keyed by component name, e.g.

        The new entity always gets a DefRef {Kind, Id, Overrule}.
        On any problem: returns nullopt, writes a message to errs, destroys

        But this function is split into 2 versions, 1 for SpawnInstance (normal)
        and the other is for Hot reloading(Reapply)
    */
    std::optional<Json> BuildInstance(JsonScene& jc, Database const& db, std::string const& kind,
                                      std::string const& id, Json const& overrides, ErrorChecker& errs){

        //Add on here when other keys are needed to be updated
        std::string pos = kind + "." + id;

        Json const* found = db.Find(kind, id);
        if (!found) {
            errs.push_back(pos + ": no such definition (unknown kind or id)");
            return std::nullopt;
        }

        // Work on a copy, so that the Database itself is never changed
        Json def = *found;

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


        ValidationS ctx{ db, errs, pos };
        if (!jc.CheckDefinition(def, ctx)) return std::nullopt;

        //Returns the definition to be used seperately
        return def;
    }


    /*
        Function Spawns an instance
     */
    std::optional<Entity> SpawnInstance(Ecs::JsonScene& jc, Ecs::Database const& db, std::string const& kind, 
    std::string const& id, Json const& overrides, ErrorChecker& errs)
    {
        auto definition = BuildInstance(jc, db, kind, id, overrides, errs);
        if(!definition) return std::nullopt;
        //Spawns the entitiy, performing cleanup if necessary
        return jc.Spawn(*definition, kind + "." + id, errs);
    }

    /*
      Hot reload: rebuilds one live entity from the (re)loaded Database, using its DefRef.
      Returns false for entities without DefRef or whose definition is gone/invalid (left unchanged).
  */
    bool ReapplyDefinition(JsonScene& jc, Coordinator& world, Database const& db, Entity e, ErrorChecker& errs){
        if (!world.HasComponent<DefRef>(e)) return false;

        // This part copies the original, before reapply below overwrites DefRef
        DefRef const ref = world.GetComponent<DefRef>(e);

        auto def = BuildInstance(jc, db, ref.Kind, ref.Id, ref.Overrides, errs);

        if (!def) return false;
        return jc.Reapply(e, *def, ref.Kind + "." + ref.Id, errs);
    }

    /*
        For HotReload. It reads every definition file listed in the manifest, validates them,
        and if they all pass, rebuilds each live entity from its DefRef (runtime-only components
        such as Contents are kept).
    */
    ReloadResult HotReload(JsonScene& jc, Coordinator& world, Database& db, std::string const& manifest,
                           DefChecker check, TableRules const* tables)
    {
        ReloadResult rr;
        if (!Load_All(manifest, db, rr.errs, std::move(check), tables)) return rr;   // db untouched on failure
        rr.ok = true;

        // Every entity as JSON before it is rebuilt (reflection: covers every registered component)
        std::map<Entity, Json> before;
        for (Entity e : world.LivingEntities()) before[e] = jc.Describe(e);

        for (Entity e : world.LivingEntities())
            if (ReapplyDefinition(jc, world, db, e, rr.errs)) ++rr.refreshed;

        // Compare old vs new and record each field that changed
        for (auto const& [e, old] : before)
        {
            if (!world.isAlive(e)) continue;
            Json const patch = Json::diff(old, jc.Describe(e));   // a list of { op, path, value }
            if (patch.empty()) continue;

            std::string source = "?";
            if (world.HasComponent<DefRef>(e)) {
                auto const& ref = world.GetComponent<DefRef>(e);
                source = ref.Kind + "." + ref.Id;
            }

            for (auto const& op : patch)
            {
                std::string const ptr  = op["path"].get<std::string>();   // "/Temp/Status"
                std::string const kind = op["op"].get<std::string>();     // replace / add / remove

                FieldChange c;
                c.entity = e;
                c.source = source;
                c.field  = ptr.substr(1);                                  // "Temp/Status" -> "Temp.Status"
                std::replace(c.field.begin(), c.field.end(), '/', '.');
                if (kind != "add")    c.before = old.at(Json::json_pointer(ptr));
                if (kind != "remove") c.after  = op["value"];
                rr.changes.push_back(std::move(c));
            }
        }
        return rr;
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