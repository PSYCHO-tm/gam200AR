// Tests for the coffee simulation (core::Simulation, M1G06).
//
// Everything here is headless and deterministic: simulated time only moves when a test calls
// Update(dt), nothing sleeps, nothing reads the keyboard, and every shift uses a fixed seed.
//
// Most tests use a tiny set of JSON fixtures (below), so they do not break when someone edits
// Assets/Json. Two tests load the real Assets/Json to prove the shipped data works end to end.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Simulation/simulation.hpp"
#include "log.hpp"

#ifndef KOPI_ASSETS_DIR
#error "Tests/CMakeLists.txt must define KOPI_ASSETS_DIR (the Assets/Json folder)"
#endif

using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using namespace core;

// Show "WrongQuantity" instead of a number when an ErrorType check fails
namespace Catch
{
    template <>
    struct StringMaker<core::ErrorType>
    {
        static std::string convert(core::ErrorType type) { return core::ErrorName(type); }
    };
}

namespace
{
    // ------------------------------------------------------------------------------------------
    // Fixture data: Kopi = beans, water, sugar. Teh = water, sugar, ice.
    // ------------------------------------------------------------------------------------------
    constexpr const char* kDrinks = R"json({ "DRINKS": {
        "Kopi": { "Components": { "Name": {"Value":"Kopi"}, "RecipeRef": {"Id":"Kopi_recipe"}, "Temp": {"Status":"Hot"}  } },
        "Teh":  { "Components": { "Name": {"Value":"Teh"},  "RecipeRef": {"Id":"Teh_recipe"},  "Temp": {"Status":"Iced"} } }
    }})json";

    constexpr const char* kIngredients = R"json({ "INGREDIENTS": {
        "coffee_beans": { "Components": { "Name": {"Value":"Coffee Beans"}, "Units": {"Value":"g"}  } },
        "water":        { "Components": { "Name": {"Value":"Water"},        "Units": {"Value":"ml"} } },
        "sugar":        { "Components": { "Name": {"Value":"Sugar"},        "Units": {"Value":"g"}  } },
        "ice":          { "Components": { "Name": {"Value":"Ice"},          "Units": {"Value":"g"}  } }
    }})json";

    constexpr const char* kRecipes = R"json({ "RECIPES": {
        "Kopi_recipe": { "Ingredients": {
            "coffee_beans": { "Amount": 18,  "Attributes": {"State":"ground"} },
            "water":        { "Amount": 200, "Attributes": {"State":"hot"} },
            "sugar":        { "Amount": 10 } } },
        "Teh_recipe": { "Ingredients": {
            "water": { "Amount": 150, "Attributes": {"State":"hot"} },
            "sugar": { "Amount": 5 },
            "ice":   { "Amount": 20 } } }
    }})json";

    constexpr const char* kScenarios = R"json({ "SCENARIO": {
        "Single": { "Order": 0, "TimeLimitSec": 0,  "Grading": "G", "Orders": [ {"Drink":"Kopi","Count":1} ] },
        "Duo":    { "Order": 1, "TimeLimitSec": 0,  "Grading": "G", "Orders": [ {"Drink":"Kopi","Count":2}, {"Drink":"Teh","Count":2} ] },
        "Timed":  { "Order": 2, "TimeLimitSec": 10, "Grading": "G", "Orders": [ {"Drink":"Kopi","Count":5} ] },
        "Many":   { "Order": 3, "TimeLimitSec": 0,  "Grading": "G", "Orders": [ {"Drink":"Kopi","Count":30} ] }
    }})json";

    constexpr const char* kGrading =
        R"json({ "GRADING": { "G": [ {"Grade":"A","Score":90}, {"Grade":"C","Score":50}, {"Grade":"F","Score":0} ] } })json";

    SimData MakeData()
    {
        SimData data;
        REQUIRE(data.LoadDrinks(kDrinks));
        REQUIRE(data.LoadIngredients(kIngredients));
        REQUIRE(data.LoadRecipes(kRecipes));
        REQUIRE(data.LoadScenarios(kScenarios));
        REQUIRE(data.LoadGrading(kGrading));
        REQUIRE(data.Validate());

        data.config.seed          = 1;     // fixed customers
        data.config.initialDelay  = 1.0f;  // first customer after 1 s
        data.config.spawnInterval = 5.0f;
        data.config.orderLifetime = 20.0f;
        data.config.maxQueueSize  = 50;

        // Generous shelves, so a test only meets an empty shelf when it asks for one
        for (const IngredientDef& item : data.ingredients)
        {
            StockRule rule;
            rule.start                  = 1000.0f;
            rule.refillAmount           = 1000.0f;
            rule.lowThreshold           = 100.0f;
            data.config.stock[item.id] = rule;
        }
        return data;
    }

    // A simulation that is already started. "tweak" can change the data before Init (stock, patience...).
    struct Shop
    {
        SimData    data;
        Simulation sim;

        explicit Shop(const char* scenario = "Single", bool retryOnMistake = true,
                      const std::function<void(SimData&)>& tweak = nullptr)
            : data(MakeData())
        {
            if (tweak) tweak(data);
            sim.SetRetryOnMistake(retryOnMistake);
            REQUIRE(sim.Init(data, scenario));
        }
    };

    // ------------------------------------------------------------------------------------------
    // Helpers: move time forward and act like a barista
    // ------------------------------------------------------------------------------------------
    void Run(Simulation& sim, float seconds) // 0.1 s steps, the same as the console bot
    {
        const int steps = static_cast<int>(seconds * 10.0f + 0.5f);
        for (int i = 0; i < steps && !sim.IsFinished(); ++i)
            sim.Update(0.1f);
    }

    // Let time pass until a customer is waiting. Returns their ticket number (0 = nobody came).
    int WaitForCustomer(Simulation& sim, int maxSteps = 600)
    {
        for (int i = 0; i < maxSteps && !sim.IsFinished(); ++i)
        {
            if (const Order* front = sim.Orders().GetFront()) return front->id;
            sim.Update(0.1f);
        }
        const Order* front = sim.Orders().GetFront();
        return front ? front->id : 0;
    }

    Verdict Pour(Simulation& sim, const std::string& ingredient, float quantity, const AttributeMap& attributes = {})
    {
        InteractionEvent add;
        add.action     = "add";
        add.object     = ingredient;
        add.quantity   = quantity;
        add.attributes = attributes;
        return sim.OnInteraction(add);
    }

    Verdict PourStep(Simulation& sim, const RecipeStep& step, float factor = 1.0f)
    {
        return Pour(sim, step.ingredientId, step.amount * factor, step.attributes);
    }

    Verdict Refill(Simulation& sim, const std::string& ingredient)
    {
        InteractionEvent refill;
        refill.action = "refill";
        refill.object = ingredient;
        return sim.OnInteraction(refill);
    }

    Verdict Serve(Simulation& sim, const std::string& drinkId)
    {
        InteractionEvent serve;
        serve.action = "serve";
        serve.object = drinkId;
        return sim.OnInteraction(serve);
    }

    // Follow the recipe exactly (refilling the shelf first when it is too low), then serve.
    Verdict MakePerfectly(Simulation& sim, int orderId)
    {
        const Order* order = sim.Orders().FindOrder(orderId);
        REQUIRE(order != nullptr);
        const DrinkDef   drink  = sim.Orders().GetDrink(*order); // a copy: the queue changes while we work
        const RecipeDef* recipe = sim.Data().FindRecipe(drink.recipeId);
        REQUIRE(recipe != nullptr);
        REQUIRE(sim.BeginOrder(orderId));

        for (const RecipeStep& step : recipe->steps)
        {
            for (int guard = 0; guard < 10 && sim.Stock().Level(step.ingredientId) < step.amount; ++guard)
                Refill(sim, step.ingredientId);
            const Verdict poured = PourStep(sim, step);
            INFO("order #" << orderId << " " << drink.id << ", step " << step.ingredientId << ": " << poured.message);
            REQUIRE(poured.correct);
        }
        return Serve(sim, drink.id);
    }

    // Serve every customer perfectly and instantly. Time only passes while waiting for the next customer.
    void PlayPerfectly(Simulation& sim)
    {
        for (int guard = 0; guard < 100000 && !sim.IsFinished(); ++guard)
        {
            const Order* front = sim.Orders().GetFront();
            if (front == nullptr)
            {
                sim.Update(0.1f);
                continue;
            }
            const int     orderId = front->id;
            const Verdict served  = MakePerfectly(sim, orderId);
            REQUIRE(served.correct); // stop at the first bad drink (a REQUIRE inside CHECK(...) would be swallowed)
        }
    }

    void AddRushHour(SimData& data)
    {
        Condition rush;
        rush.name                = "Rush";
        rush.startSec            = 0.0f;
        rush.durationSec         = 100.0f;
        rush.orderRateMultiplier = 2.0f;
        rush.patienceMultiplier  = 0.5f;
        data.config.conditions.push_back(rush);
    }
}

// ==============================================================================================
// Loading data
// ==============================================================================================

TEST_CASE("Simulation: fixture data loads and validates")
{
    const SimData data = MakeData(); // MakeData() REQUIREs that every file loads and Validate() passes

    CHECK(data.drinks.size() == 2u);
    CHECK(data.ingredients.size() == 4u);
    CHECK(data.recipes.size() == 2u);
    CHECK(data.scenarios.size() == 4u);

    // The ORDER of the ingredients in the JSON is the order of the steps
    const RecipeDef* kopi = data.FindRecipe("Kopi_recipe");
    REQUIRE(kopi != nullptr);
    REQUIRE(kopi->steps.size() == 3u);
    CHECK(kopi->steps[0].ingredientId == "coffee_beans");
    CHECK(kopi->steps[0].attributes.at("State") == "ground");
    CHECK(kopi->steps[2].ingredientId == "sugar");
    CHECK(data.scenarios.at("Timed").timeLimitSec == Approx(10.0f));
}

TEST_CASE("Simulation: bad data is rejected with a clear message")
{
    SimData data;

    SECTION("broken JSON")
    {
        CHECK_FALSE(data.LoadDrinks("{ not json"));
        CHECK_THAT(data.GetLastError(), ContainsSubstring("Drinks.json"));
        CHECK_THAT(data.GetLastError(), ContainsSubstring("not valid JSON"));
    }

    SECTION("a recipe amount of zero")
    {
        CHECK_FALSE(data.LoadRecipes(R"json({"RECIPES":{"R":{"Ingredients":{"sugar":{"Amount":0}}}}})json"));
        CHECK_THAT(data.GetLastError(), ContainsSubstring("Amount"));
    }

    SECTION("a drink that points at a missing recipe")
    {
        REQUIRE(data.LoadDrinks(kDrinks));
        REQUIRE(data.LoadIngredients(kIngredients));
        REQUIRE(data.LoadRecipes(R"json({"RECIPES":{"Other":{"Ingredients":{"sugar":{"Amount":1}}}}})json"));
        CHECK_FALSE(data.Validate());
        CHECK_THAT(data.GetLastError(), ContainsSubstring("Kopi_recipe"));
    }

    SECTION("a recipe that uses an ingredient nobody stocks")
    {
        REQUIRE(data.LoadDrinks(kDrinks));
        REQUIRE(data.LoadIngredients(kIngredients));
        REQUIRE(data.LoadRecipes(R"json({"RECIPES":{
            "Kopi_recipe":{"Ingredients":{"unobtainium":{"Amount":1}}},
            "Teh_recipe":{"Ingredients":{"sugar":{"Amount":1}}}}})json"));
        CHECK_FALSE(data.Validate());
        CHECK_THAT(data.GetLastError(), ContainsSubstring("unobtainium"));
    }

    SECTION("a folder that does not exist")
    {
        CHECK_FALSE(data.LoadFromDirectory("no_such_folder"));
        CHECK_THAT(data.GetLastError(), ContainsSubstring("Could not open"));
    }
}

TEST_CASE("Simulation: an unknown scenario is rejected")
{
    const SimData data = MakeData();
    Simulation    sim;

    CHECK_FALSE(sim.Init(data, "Nope"));
    CHECK_THAT(sim.GetLastError(), ContainsSubstring("Unknown scenario"));
    CHECK_THAT(sim.GetLastError(), ContainsSubstring("Duo")); // it lists the real names
}

TEST_CASE("Simulation: real data loads and validates")
{
    SimData    data;
    const bool loaded = data.LoadFromDirectory(KOPI_ASSETS_DIR);
    INFO(data.GetLastError()); // names the broken file, key or reference
    REQUIRE(loaded);

    CHECK_FALSE(data.drinks.empty());
    CHECK_FALSE(data.ingredients.empty());
    CHECK_FALSE(data.recipes.empty());
    CHECK_FALSE(data.scenarios.empty());
    CHECK(data.scenarios.count("Normal") == 1u); // the console demo starts this one by default

    for (const auto& entry : data.scenarios)
    {
        INFO("scenario " << entry.first);
        Simulation sim;
        CHECK(sim.Init(data, entry.first));
    }
}

// ==============================================================================================
// Grading one drink
// ==============================================================================================

TEST_CASE("Simulation: a correct pour advances the recipe")
{
    Shop        shop;
    Simulation& sim = shop.sim;
    const int   id  = WaitForCustomer(sim);
    REQUIRE(id != 0);
    REQUIRE(sim.BeginOrder(id));

    const RecipeStep first  = *sim.Prep().GetExpectedStep();
    const float      before = sim.Stock().Level(first.ingredientId);
    const Verdict    verdict = PourStep(sim, first);

    CHECK(verdict.correct);
    CHECK(verdict.stepAdvanced);
    CHECK_FALSE(verdict.hasError);
    CHECK(sim.Prep().GetNextStepIndex() == 1u);
    CHECK(sim.Stock().Level(first.ingredientId) == Approx(before - first.amount));
}

TEST_CASE("Simulation: a wrong quantity is graded and has to be redone")
{
    Shop        shop; // retry on: a wrong pour does not advance the step
    Simulation& sim = shop.sim;
    REQUIRE(sim.BeginOrder(WaitForCustomer(sim)));
    const RecipeStep step = *sim.Prep().GetExpectedStep(); // 18 g of coffee beans

    const Verdict bad = PourStep(sim, step, 1.5f); // 27 g
    CHECK(bad.hasError);
    CHECK(bad.errorType == ErrorType::WrongQuantity);
    CHECK_FALSE(bad.stepAdvanced);
    CHECK(sim.Prep().GetNextStepIndex() == 0u); // the same step again

    CHECK(PourStep(sim, step).stepAdvanced); // the redo is accepted
    while (const RecipeStep* next = sim.Prep().GetExpectedStep())
    {
        const RecipeStep copy = *next;
        REQUIRE(PourStep(sim, copy).stepAdvanced);
    }
    CHECK_FALSE(Serve(sim, "Kopi").correct); // the earlier mistake still counts

    const SessionReport report = sim.BuildReport();
    CHECK(report.errorCounts.at("WrongQuantity") == 1);
    CHECK(report.waste.at("coffee_beans") == Approx(9.0f)); // 27 poured - 18 needed
    CHECK(report.stepsCorrect == 2);                        // 3 steps, the first one had a mistake
}

TEST_CASE("Simulation: the quantity tolerance is +/-10 percent")
{
    Shop        shop;
    Simulation& sim = shop.sim;
    REQUIRE(sim.BeginOrder(WaitForCustomer(sim)));
    const RecipeStep step = *sim.Prep().GetExpectedStep(); // 18 g, so 16.2 g to 19.8 g is fine

    SECTION("just above the limit is accepted") { CHECK(PourStep(sim, step, 1.09f).correct); }
    SECTION("just below the limit is accepted") { CHECK(PourStep(sim, step, 0.91f).correct); }
    SECTION("too much is rejected")
    {
        const Verdict verdict = PourStep(sim, step, 1.11f);
        CHECK(verdict.errorType == ErrorType::WrongQuantity);
        CHECK_FALSE(verdict.stepAdvanced);
    }
    SECTION("too little is rejected")
    {
        const Verdict verdict = PourStep(sim, step, 0.89f);
        CHECK(verdict.errorType == ErrorType::WrongQuantity);
        CHECK_FALSE(verdict.stepAdvanced);
    }
}

TEST_CASE("Simulation: every kind of mistake is graded")
{
    Shop        shop;
    Simulation& sim = shop.sim;
    REQUIRE(sim.BeginOrder(WaitForCustomer(sim)));
    const RecipeStep first = *sim.Prep().GetExpectedStep(); // coffee beans, State=ground

    SECTION("wrong state")
    {
        const Verdict verdict = Pour(sim, first.ingredientId, first.amount, {{"State", "whole"}});
        CHECK(verdict.errorType == ErrorType::WrongState);
        CHECK_FALSE(verdict.stepAdvanced);
    }
    SECTION("missing state")
    {
        const Verdict verdict = Pour(sim, first.ingredientId, first.amount);
        CHECK(verdict.errorType == ErrorType::WrongState);
        CHECK_FALSE(verdict.stepAdvanced);
    }
    SECTION("wrong sequence: water is needed, but later")
    {
        const Verdict verdict = Pour(sim, "water", 200.0f, {{"State", "hot"}});
        CHECK(verdict.errorType == ErrorType::WrongSequence);
        CHECK_FALSE(verdict.stepAdvanced);
    }
    SECTION("wrong ingredient: ice is not in a Kopi")
    {
        const Verdict verdict = Pour(sim, "ice", 5.0f);
        CHECK(verdict.errorType == ErrorType::WrongIngredient);
        CHECK_FALSE(verdict.stepAdvanced);
    }
    SECTION("wrong drink")
    {
        const Verdict verdict = Serve(sim, "Teh");
        CHECK(verdict.errorType == ErrorType::WrongDrink);
        CHECK(sim.Prep().IsActive()); // the customer did not take it
    }
}

TEST_CASE("Simulation: serving early records every skipped step")
{
    Shop        shop;
    Simulation& sim = shop.sim;
    REQUIRE(sim.BeginOrder(WaitForCustomer(sim)));
    const RecipeStep first = *sim.Prep().GetExpectedStep();
    REQUIRE(PourStep(sim, first).stepAdvanced);

    const Verdict served = Serve(sim, "Kopi"); // 1 of 3 steps done
    CHECK(served.stepAdvanced);
    CHECK_FALSE(served.correct);

    const SessionReport report = sim.BuildReport();
    CHECK(report.errorCounts.at("SkippedStep") == 2);
    CHECK(report.ordersServed == 1);
    CHECK(sim.IsFinished()); // the only customer has been served
}

// ==============================================================================================
// The shelf
// ==============================================================================================

TEST_CASE("Inventory: consuming and refilling a shelf")
{
    std::vector<IngredientDef> items(1);
    items[0].id = "sugar";

    StockRule rule;
    rule.start        = 20.0f;
    rule.refillAmount = 50.0f;
    rule.lowThreshold = 8.0f;
    std::map<std::string, StockRule> rules;
    rules["sugar"] = rule;

    Inventory shelf;
    shelf.Init(items, rules);
    CHECK(shelf.Known("sugar"));
    CHECK_FALSE(shelf.Known("tea"));
    CHECK(shelf.Level("sugar") == Approx(20.0f));

    CHECK_FALSE(shelf.Consume("sugar", 25.0f)); // not enough: nothing changes
    CHECK(shelf.Level("sugar") == Approx(20.0f));
    CHECK_FALSE(shelf.Consume("sugar", -1.0f)); // negative amounts are refused

    CHECK(shelf.Consume("sugar", 15.0f));
    CHECK(shelf.Level("sugar") == Approx(5.0f));
    CHECK(shelf.IsLow("sugar"));

    CHECK(shelf.Refill("sugar") == Approx(50.0f));
    CHECK(shelf.Level("sugar") == Approx(55.0f));
    CHECK_FALSE(shelf.IsLow("sugar"));
    CHECK(shelf.GetRefillCount() == 1);

    CHECK(shelf.Refill("tea") == Approx(0.0f)); // unknown ingredient: nothing happens
}

TEST_CASE("Simulation: an empty shelf blocks a pour until it is refilled")
{
    Shop shop("Single", true,
              [](SimData& data)
              {
                  StockRule rule;
                  rule.start        = 10.0f; // a Kopi needs 18 g of beans
                  rule.refillAmount = 100.0f;
                  rule.lowThreshold = 5.0f;
                  data.config.stock["coffee_beans"] = rule;
              });
    Simulation& sim = shop.sim;
    REQUIRE(sim.BeginOrder(WaitForCustomer(sim)));
    const RecipeStep step = *sim.Prep().GetExpectedStep();

    const Verdict empty = PourStep(sim, step);
    CHECK(empty.errorType == ErrorType::OutOfStock);
    CHECK_FALSE(empty.stepAdvanced);
    CHECK(sim.Stock().Level("coffee_beans") == Approx(10.0f)); // nothing was taken

    CHECK(Refill(sim, "coffee_beans").correct);
    CHECK(sim.Stock().Level("coffee_beans") == Approx(110.0f));

    CHECK(PourStep(sim, step).stepAdvanced);
    CHECK(sim.Stock().Level("coffee_beans") == Approx(92.0f));
    CHECK(sim.BuildReport().refills == 1);
}

// ==============================================================================================
// Customers and time
// ==============================================================================================

TEST_CASE("Simulation: a customer leaves when their patience runs out")
{
    Shop        shop("Single", true, [](SimData& data) { data.config.orderLifetime = 5.0f; });
    Simulation& sim = shop.sim;
    REQUIRE(sim.BeginOrder(WaitForCustomer(sim)));
    REQUIRE(PourStep(sim, *sim.Prep().GetExpectedStep()).stepAdvanced); // drink is half made...

    Run(sim, 7.0f); // ...and takes longer than their patience

    CHECK(sim.Orders().GetExpiredCount() == 1);
    CHECK_FALSE(sim.Prep().IsActive());
    CHECK(sim.IsFinished()); // the only customer is gone

    const SessionReport report = sim.BuildReport();
    CHECK(report.ordersServed == 0);
    CHECK(report.ordersExpired == 1);
    REQUIRE(report.drinks.size() == 1u);
    CHECK_FALSE(report.drinks[0].served);
    CHECK(report.errorCounts.at("CustomerLeft") == 1);
    CHECK(report.grade == "F"); // nothing served: the lowest grade
}

TEST_CASE("Simulation: the time limit ends the shift")
{
    Shop        shop("Timed"); // 10 second limit
    Simulation& sim = shop.sim;

    Run(sim, 9.0f);
    CHECK_FALSE(sim.IsFinished());
    Run(sim, 2.0f);
    CHECK(sim.IsFinished());

    const SessionReport report = sim.BuildReport();
    CHECK(report.timeUp);
    CHECK(report.stillWaiting > 0); // customers were still on the rail

    const double endTime = sim.Time();
    sim.Update(1.0f);
    CHECK(sim.Time() == Approx(endTime)); // a finished shift ignores more time
}

TEST_CASE("Simulation: Rush Hour brings customers faster and with less patience")
{
    const auto customersAfter30Seconds = [](bool rush)
    {
        Shop shop("Many", true, rush ? std::function<void(SimData&)>(AddRushHour) : nullptr);
        Run(shop.sim, 30.0f);
        return shop.sim.Orders().GetSpawnedCount();
    };
    const int withRush    = customersAfter30Seconds(true);
    const int withoutRush = customersAfter30Seconds(false);
    CHECK(withRush > withoutRush);

    Shop        shop("Many", true, AddRushHour);
    Simulation& sim = shop.sim;
    const int   id  = WaitForCustomer(sim);
    REQUIRE(id != 0);
    CHECK(sim.Orders().FindOrder(id)->lifetime == Approx(10.0f)); // 20 s x 0.5
}

TEST_CASE("Simulation: the same seed gives the same shift")
{
    const auto playDuo = [](unsigned seed)
    {
        Shop shop("Duo", true, [seed](SimData& data) { data.config.seed = seed; });
        PlayPerfectly(shop.sim);
        return shop.sim.FormatReport();
    };
    const std::string firstRun  = playDuo(5);
    const std::string secondRun = playDuo(5);
    CHECK(firstRun == secondRun);

    // ...and the seed really does decide who arrives when
    std::set<std::string> arrivalOrders;
    for (unsigned seed = 1; seed <= 20; ++seed)
    {
        Shop shop("Duo", true, [seed](SimData& data) { data.config.seed = seed; });
        PlayPerfectly(shop.sim);
        std::string order;
        for (const PrepResult& drink : shop.sim.BuildReport().drinks)
            order += drink.drinkId + " ";
        arrivalOrders.insert(order);
    }
    CHECK(arrivalOrders.size() > 1u);
}

// ==============================================================================================
// Whole shifts
// ==============================================================================================

TEST_CASE("Simulation: perfect play on the fixture data scores 100 percent")
{
    Shop        shop("Duo");
    Simulation& sim = shop.sim;
    PlayPerfectly(sim);

    REQUIRE(sim.IsFinished());
    const SessionReport report = sim.BuildReport();
    CHECK(report.customersArrived == 4);
    CHECK(report.ordersServed == 4);
    CHECK(report.ordersExpired == 0);
    CHECK(report.errorCounts.empty());
    CHECK(report.scorePct == Approx(100.0f));
    CHECK(report.grade == "A");
}

TEST_CASE("Simulation: perfect play finishes every real scenario")
{
    SimData    real;
    const bool loaded = real.LoadFromDirectory(KOPI_ASSETS_DIR);
    INFO(real.GetLastError());
    REQUIRE(loaded);
    real.config.seed = 7; // the same customers every run, whatever Simulation.json says

    for (const auto& entry : real.scenarios)
    {
        INFO("scenario " << entry.first);

        Simulation sim;
        sim.onLog = [](const std::string& text) { LOG_INFO("SIM " << text); }; // the whole shift ends up in the log
        REQUIRE(sim.Init(real, entry.first));
        PlayPerfectly(sim);

        const SessionReport report = sim.BuildReport();
        LOG_INFO("SIM " << entry.first << ": " << report.customersArrived << " arrived | " << report.ordersServed
                        << " served | " << report.ordersExpired << " left | score " << std::fixed
                        << std::setprecision(1) << report.scorePct << "%");

        REQUIRE(sim.IsFinished());
        CHECK(report.customersArrived > 0);
        CHECK(report.ordersServed == report.customersArrived);
        CHECK(report.ordersExpired == 0);
        CHECK(report.errorCounts.empty());
        CHECK(report.scorePct == Approx(100.0f));
    }
}
