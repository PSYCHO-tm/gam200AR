#include "RegisterSystem.hpp"

namespace Ecs{

    double ScoringCup(RecipeDef const& recipe, Contents const& cup)
    {
        double total = 0;

        for (auto const& [id, want] : recipe.Ingredients) {
            auto curr = cup.Items.find(id);
            if (curr == cup.Items.end()) continue;                    // missing: this line scores 0

            // Quantity: 1 when exact, 0 when off by the whole wanted amount or more
            double amountScore = 1.0;
            if (want.Amount > 0) {
                double off = std::abs(curr->second.Amount - want.Amount) / want.Amount;
                amountScore = std::max(0.0, 1.0 - off);
            }

            // Type / state: share of the recipe's attributes that the cup matches
            double attrScore = 1.0;
            if (!want.Attributes.empty()) {
                int matched = 0;
                for (auto const& [key, value] : want.Attributes) {
                    auto a = curr->second.Attributes.find(key);
                    if (a != curr->second.Attributes.end() && a->second == value) ++matched;
                }
                attrScore = static_cast<double>(matched) / static_cast<double>(want.Attributes.size());
            }

            total += amountScore * attrScore;
        }

        // Ingredients that are not in the recipe count against the cup
        size_t extras = 0;
        for (auto const& [id, item] : cup.Items)
            if (!recipe.Ingredients.count(id)) ++extras;

        size_t lines = recipe.Ingredients.size() + extras;
        if (lines == 0) return 0;
        return 100.0 * total / static_cast<double>(lines);
    }

    std::string GradeFor(double score, std::vector<GradeTier> const& tiers)
    {
        for (auto const& t : tiers)                                   // highest Score first
            if (score >= t.Score) return t.Grade;
        return tiers.empty() ? std::string{} : tiers.back().Grade;
    }


    // ------------------------------------------------------------------
    //  DisplaySystem
    // ------------------------------------------------------------------
    void DisplaySystem::Show() const
    {
        auto list = entityVec;                                        // copy so we can sort
        std::sort(list.begin(), list.end());

        std::cout << list.size() << " drink(s)\n";
        for (Entity e : list)
            std::cout << "  #" << e << "  " << world->GetComponent<Name>(e).Value
                      << "  (" << world->GetComponent<Temp>(e).Status << ")\n";
    }


    // ------------------------------------------------------------------
    //  RecipeCheckSystem
    // ------------------------------------------------------------------
    bool RecipeCheckSystem::Evaluate(std::string const& gradingSet, ErrorChecker& errs)
    {
        auto tiers = db->grading.find(gradingSet);
        if (tiers == db->grading.end()) {
            errs.push_back("unknown grading set '" + gradingSet + "'");
            return false;
        }

        bool ok = true;
        auto list = entityVec;               // copy: adding a Result component must not disturb the loop

        for (Entity e : list) {
            std::string recipeId = world->GetComponent<RecipeRef>(e).Id;
            auto recipe = db->recipes.find(recipeId);
            if (recipe == db->recipes.end()) {
                errs.push_back("entity " + std::to_string(e) + ": unknown recipe '" + recipeId + "'");
                ok = false;
                continue;
            }

            Result r;
            r.Score = ScoringCup(recipe->second, world->GetComponent<Contents>(e));
            r.Grade = GradeFor(r.Score, tiers->second);

            if (world->HasComponent<Result>(e)) world->GetComponent<Result>(e) = r;
            else                                world->AddComponent<Result>(e, r);
        }
        return ok;
    }


    // ------------------------------------------------------------------
    //  ScenarioRunner
    // ------------------------------------------------------------------
   
    /*
        Destroy entities in orders
    */
    void ScenarioRunner::ClearOrders()
    {
        for (Entity e : orders)
            if (world.isAlive(e) && world.HasComponent<Contents>(e))  // guard: the id may have been reused
                world.DestroyEntity(e);
        orders.clear();
    }

   /*
            Spawns every order in the scenario (an empty cup each). Replaces any earlier run.
        */
    bool ScenarioRunner::Start(std::string const& scenarioName, ErrorChecker& errs)
    {
        auto sc = db.scenarios.find(scenarioName);
        if (sc == db.scenarios.end()) {
            errs.push_back("unknown scenario '" + scenarioName + "'");
            return false;
        }

        ClearOrders();
        current = scenarioName;

        for (auto const& o : sc->second.Orders) {
            for (int i = 0; i < o.Count; ++i) {
                auto e = SpawnInstance(scene, db, kDrinks, o.Drink, Json::object(), errs);
                if (!e) {    // undo this run completely
                    ClearOrders();
                    current.clear();
                    return false;
                }
                world.AddComponent<Contents>(*e, Contents{}); // empty cup
                orders.push_back(*e);
            }
        }
        return true;
    }

    /*
        Puts some of an ingredient into a drink's cup. Amounts add up if you call it twice.
    */
    bool ScenarioRunner::Add(Entity drink, std::string const& ingredient, double amount, std::map<std::string, std::string> 
        const& attributes, ErrorChecker& errs)
    {
        if (!world.isAlive(drink) || !world.HasComponent<Contents>(drink)) {
            errs.push_back("entity " + std::to_string(drink) + " is not a drink being made");
            return false;
        }

        if (!db.ingredients.count(ingredient)) {
            errs.push_back("unknown ingredient '" + ingredient + "'");
            return false;
        }

        if (amount <= 0) {
            errs.push_back("amount must be above 0");
            return false;
        }

        auto& item = world.GetComponent<Contents>(drink).Items[ingredient];
        item.Amount += amount;
        for (auto const& [key, value] : attributes) item.Attributes[key] = value;
        return true;
    }

    /*
        Scores every order and fills the report.
    */
    bool ScenarioRunner::Finish(ScenarioReport& out, ErrorChecker& errs)
    {
        auto sc = db.scenarios.find(current);
        if (current.empty() || sc == db.scenarios.end()) {
            errs.push_back("no scenario is running");
            return false;
        }

        if (!check.Evaluate(sc->second.Grading, errs)) return false;

        out = ScenarioReport{};
        out.scenario = current;

        double sum = 0;
        for (Entity e : orders) {
            if (!world.isAlive(e) || !world.HasComponent<Result>(e)) continue;
            Result const& r = world.GetComponent<Result>(e);
            out.drinks.push_back({ e, world.GetComponent<Name>(e).Value, r.Score, r.Grade });
            sum += r.Score;
        }

        out.average = out.drinks.empty() ? 0.0 : sum / static_cast<double>(out.drinks.size());
        out.grade   = GradeFor(out.average, db.grading.at(sc->second.Grading));
        return true;
    }
}