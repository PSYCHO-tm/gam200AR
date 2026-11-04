#include "RegisterComponents.hpp"
#include "sharedlib.hpp"

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