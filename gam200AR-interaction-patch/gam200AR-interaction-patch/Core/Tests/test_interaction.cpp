// Host test for the interaction stack. No GL, no Android, no phone.
//   usage: test_interaction <path to the Assets folder>
// Covers: equipment from data, touch ray-cast picking, button -> timed event -> assessment core,
// errors pinned to the equipment, the order/step text for the screen.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "InteractionSession.hpp"
#include "objLoader.hpp"
#include "picking.hpp"

namespace
{
    int gChecks   = 0;
    int gFailures = 0;

#define CHECK(condition)                                                                                      \
    do                                                                                                        \
    {                                                                                                         \
        ++gChecks;                                                                                            \
        if (!(condition))                                                                                     \
        {                                                                                                     \
            ++gFailures;                                                                                      \
            std::printf("  FAIL line %d: %s\n", __LINE__, #condition);                                        \
        }                                                                                                     \
    } while (0)

    std::string gAssets;

    std::string ReadFile(std::string const& path)
    {
        std::ifstream in(path, std::ios::binary);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    // The library exactly as the app builds it: JSON parsed, every mesh loaded, local bounds filled in.
    bool LoadLibrary(EquipmentLibrary& library, std::string& error)
    {
        if (!library.Parse(ReadFile(gAssets + "/Json/Equipment.json"), "Equipment.json", error))
        {
            return false;
        }
        for (EquipmentDef& def : library.items)
        {
            std::string const path = gAssets + "/" + def.meshPath.substr(std::string("Assets/").size());
            auto mesh = ParseObj(ReadFile(path));
            if (!mesh)
            {
                error = "cannot read mesh " + path;
                return false;
            }
            def.localBounds = Collider{};
            for (Vertex const& vertex : mesh->vertices)
            {
                def.localBounds.Include(vertex.pos);
            }
        }
        return true;
    }

    bool LoadSimData(core::SimData& data, std::string& error)
    {
        if (!data.LoadFromDirectory(gAssets + "/Json"))
        {
            error = data.GetLastError();
            return false;
        }
        return true;
    }

    // A camera that looks down -z from 'eye' (no turn), with a 60 degree lens. Row-vector matrices, as in Matrix.hpp.
    struct Camera
    {
        Matrix4x4 view;
        Matrix4x4 projection;
        int       w = 1080;
        int       h = 2000;
    };

    Camera MakeCamera(Vector3D eye)
    {
        Camera camera;
        camera.view          = Matrix4x4::Translate(-eye.x, -eye.y, -eye.z);
        float const f        = 1.0f / std::tan(30.0f * 3.14159265f / 180.0f);
        camera.projection.m[0][0] = f / (static_cast<float>(camera.w) / static_cast<float>(camera.h));
        camera.projection.m[1][1] = f;
        return camera;
    }

    // Where a world point lands on the screen, in pixels (0,0 = top-left).
    void ToPixel(Camera const& camera, Vector3D const& world, float& px, float& py)
    {
        Vector3D const v    = camera.view.TransformPoint(world);
        float const    ndcX = v.x * camera.projection.m[0][0] / -v.z;
        float const    ndcY = v.y * camera.projection.m[1][1] / -v.z;
        px                  = (ndcX + 1.0f) * 0.5f * static_cast<float>(camera.w);
        py                  = (1.0f - ndcY) * 0.5f * static_cast<float>(camera.h);
    }

    // The middle of the top face, just inside the box. From a camera above the table nothing stands in front of it.
    // (The middle of a whole box can be hidden behind a neighbour: with the anchor turned, the tin's box really is
    // in front of the filter's middle, and "nearest hit wins" is then the right answer.)
    Vector3D TopCentre(Collider const& box)
    {
        return Vector3D{(box.minPoint.x + box.maxPoint.x) * 0.5f, box.maxPoint.y - 0.002f,
                        (box.minPoint.z + box.maxPoint.z) * 0.5f};
    }

    // ------------------------------------------------------------------------------------------------

    void TestEquipmentFromData()
    {
        std::printf("equipment loads from data\n");
        EquipmentLibrary library;
        std::string      error;
        CHECK(LoadLibrary(library, error));
        if (!error.empty())
        {
            std::printf("  %s\n", error.c_str());
        }
        CHECK(library.items.size() >= 3);
        for (EquipmentDef const& def : library.items)
        {
            CHECK(def.localBounds.hasBounds);
            CHECK(!def.interactions.empty()); // every piece has its interaction mapping from the data
        }
        EquipmentDef const* kettle = library.Find("Kettle");
        CHECK(kettle != nullptr);
        if (kettle != nullptr)
        {
            CHECK(kettle->interactions.size() == 1);
            CHECK(kettle->interactions[0].action == "pour_water");
            CHECK(kettle->interactions[0].ingredient == "water");
            CHECK(kettle->interactions[0].quantity == 220.0f);
            auto const state = kettle->interactions[0].attributes.find("State");
            CHECK(state != kettle->interactions[0].attributes.end() && state->second == "hot");
        }
        EquipmentDef const* filter = library.Find("SockFilter");
        CHECK(filter != nullptr && filter->interactions.size() == 2);
    }

    void TestBadData()
    {
        std::printf("bad equipment data is rejected with a useful message\n");
        EquipmentLibrary library;
        std::string      error;
        CHECK(!library.Parse(R"({"EQUIPMENT":{"X":{"Mesh":"m.obj","Interactions":{"a":{"Attributes":{"State":3}}}}}})",
                             "Equipment.json", error));
        CHECK(error.find("X") != std::string::npos && error.find("Attributes") != std::string::npos);
    }

    void TestPicking()
    {
        std::printf("touch ray-cast picks the piece it hits\n");
        EquipmentLibrary library;
        std::string      error;
        CHECK(LoadLibrary(library, error));

        // The anchor is turned and moved, like ARCore's. Models are built the way EquipmentScene builds them.
        Matrix4x4 const anchor = Matrix4x4::SRT(Vector3D{1, 1, 1}, Vector3D{0, 30, 0}, Vector3D{0.2f, 0.0f, -0.8f});
        std::vector<Matrix4x4> models;
        for (EquipmentDef const& def : library.items)
        {
            models.push_back(EquipmentLocalMatrix(def) * anchor);
        }

        Camera const camera = MakeCamera(Vector3D{0.2f, 0.45f, 0.4f});

        for (std::size_t i = 0; i < library.items.size(); ++i)
        {
            Vector3D const centre = TopCentre(library.items[i].localBounds.Transformed(models[i]));
            float          px = 0, py = 0;
            ToPixel(camera, centre, px, py);
            Ray const ray = ScreenPointToRay(px, py, camera.w, camera.h, camera.view, camera.projection);
            auto const hit = PickEquipment(ray, library, models);
            CHECK(hit.has_value());
            if (hit)
            {
                if (hit->index != i)
                {
                    std::printf("  touch on %s selected %s\n", library.items[i].id.c_str(),
                                library.items[hit->index].id.c_str());
                }
                CHECK(hit->index == i);
            }
        }

        // A touch on empty screen selects nothing.
        Ray const sky = ScreenPointToRay(20.0f, 20.0f, camera.w, camera.h, camera.view, camera.projection);
        CHECK(!PickEquipment(sky, library, models).has_value());
    }

    void TestNearestWins()
    {
        std::printf("when two boxes are on the ray, the nearer one wins\n");
        Collider near, far;
        near.Include({-0.1f, -0.1f, -1.0f});
        near.Include({0.1f, 0.1f, -0.8f});
        far.Include({-0.1f, -0.1f, -2.0f});
        far.Include({0.1f, 0.1f, -1.8f});
        Ray const ray{Vector3D{0, 0, 0}, Vector3D{0, 0, -1}};

        auto a = PickNearest(ray, {far, near});
        auto b = PickNearest(ray, {near, far});
        CHECK(a && a->index == 1);
        CHECK(b && b->index == 0);
        CHECK(a && std::fabs(a->distance - 0.8f) < 1e-4f);
        CHECK(!PickNearest(Ray{Vector3D{0, 0, 0}, Vector3D{0, 0, 1}}, {near, far}).has_value()); // behind the camera
    }

    // ------------------------------------------------------------------------------------------------

    struct Fixture
    {
        InteractionSession session;
        bool               ok = false;

        Fixture()
        {
            EquipmentLibrary library;
            core::SimData    data;
            std::string      error;
            ok = LoadLibrary(library, error) && LoadSimData(data, error) &&
                 session.Init(std::move(library), data, "Tutorial", error); // Tutorial = exactly one Kopi
            if (!ok)
            {
                std::printf("  fixture: %s\n", error.c_str());
            }
        }
    };

    void TestOrderFlowsToScreen()
    {
        std::printf("the current order and step come from the core\n");
        Fixture f;
        CHECK(f.ok);
        CHECK(Sim::FormatOrderText(f.session.Order()) == "No active order"); // nobody has arrived yet

        f.session.Update(1.5f);
        Sim::OrderView order = f.session.Order();
        CHECK(order.OrderName == "Kopi");
        CHECK(order.StepIndex == 1 && order.StepCount == 3);
        CHECK(order.StepName.find("Coffee Beans") != std::string::npos);
        std::string const text = Sim::FormatOrderText(order);
        CHECK(text.rfind("Order: Kopi | Step 1/3: Add 18", 0) == 0);

        f.session.Select("SockFilter");
        CHECK(f.session.Press("add_grounds").empty());
        order = f.session.Order();
        CHECK(order.StepIndex == 2); // the step moved on
        CHECK(order.StepName.find("Condensed Milk") != std::string::npos);
    }

    void TestButtonBecomesTimedEvent()
    {
        std::printf("a button press is a timed event (equipment, object, action, quantity)\n");
        Fixture f;
        f.session.Update(1.5f);

        CHECK(f.session.Press("add_grounds") == "no equipment selected");
        CHECK(f.session.History().empty());

        f.session.Select("SockFilter");
        auto buttons = f.session.Buttons();
        CHECK(buttons.size() == 2 && buttons[0].action == "add_grounds");

        CHECK(f.session.Press("add_grounds").empty());
        f.session.Update(0.5f);
        f.session.Select("CondensedMilkTin");
        CHECK(f.session.Press("pour_condensed_milk").empty());

        auto const events = f.session.History();
        CHECK(events.size() == 2);
        if (events.size() == 2)
        {
            CHECK(events[0].equipment == "SockFilter" && events[0].object == "coffee_beans");
            CHECK(events[0].action == "add" && events[0].quantity == 18.0f);
            CHECK(std::fabs(events[0].time - 1.5) < 1e-3);
            CHECK(events[1].equipment == "CondensedMilkTin" && events[1].object == "condensed_milk");
            CHECK(events[1].action == "add" && events[1].quantity == 2.0f);
            CHECK(events[1].time > events[0].time);
        }
        CHECK(f.session.Errors().empty()); // both were right

        // The data decides what a piece can do: a kettle cannot serve, and nothing is recorded for it.
        f.session.Select("Kettle");
        CHECK(f.session.Press("serve").find("cannot") != std::string::npos);
        CHECK(f.session.History().size() == 2);
    }

    void TestErrorShowsAtTheObject()
    {
        std::printf("an error found by the assessment core shows at that object from the next frame\n");
        Fixture f;
        f.session.Update(1.5f);
        f.session.Select("SockFilter");
        CHECK(f.session.Press("add_grounds").empty());

        // Next step is condensed milk; pouring water now is out of order.
        f.session.Select("Kettle");
        CHECK(f.session.Press("pour_water").empty()); // accepted as an interaction...
        CHECK(f.session.HasError("Kettle"));          // ...and the mistake is already on the kettle, before any Update
        CHECK(!f.session.HasError("SockFilter"));
        auto errors = f.session.Errors();
        CHECK(errors.size() == 1 && errors[0].kind == "WrongSequence");
        CHECK(!errors.empty() && errors[0].equipment == "Kettle" && !errors[0].message.empty());

        f.session.Update(1.0f / 60.0f); // the next frame still shows it
        CHECK(f.session.HasError("Kettle"));
        f.session.Update(3.5f); // and it goes away on its own
        CHECK(!f.session.HasError("Kettle"));
    }

    void TestHeldPourQuantity()
    {
        std::printf("a held pour can set the quantity; a wrong amount is graded\n");
        Fixture f;
        f.session.Update(1.5f);
        f.session.Select("SockFilter");
        CHECK(f.session.Press("add_grounds", 30.0f).empty()); // recipe wants 18 g
        CHECK(f.session.HasError("SockFilter"));
        auto errors = f.session.Errors();
        CHECK(!errors.empty() && errors[0].kind == "WrongQuantity");
        auto events = f.session.History();
        CHECK(!events.empty() && events[0].quantity == 30.0f);
    }

    void TestFullDrinkAndServe()
    {
        std::printf("a perfect Kopi from start to serve\n");
        Fixture f;
        f.session.Update(1.5f);
        f.session.Select("SockFilter");
        CHECK(f.session.Press("add_grounds").empty());
        f.session.Update(2.0f);
        f.session.Select("CondensedMilkTin");
        CHECK(f.session.Press("pour_condensed_milk").empty());
        f.session.Update(2.0f);
        f.session.Select("Kettle");
        CHECK(f.session.Press("pour_water").empty());
        CHECK(f.session.Order().StepName == "Serve the drink");
        f.session.Select("SockFilter");
        CHECK(f.session.Press("serve").empty());

        CHECK(f.session.Errors().empty());
        CHECK(f.session.LastMessage().find("SERVED") != std::string::npos);
        CHECK(f.session.LastMessage().find("perfect") != std::string::npos);
        auto const events = f.session.History();
        CHECK(events.size() == 4 && events.back().action == "serve");
        for (std::size_t i = 1; i < events.size(); ++i)
        {
            CHECK(events[i].time >= events[i - 1].time);
        }
        CHECK(f.session.Simulation().Orders().GetServedCount() == 1);
    }

    void TestPressAfterTheShiftEnds()
    {
        std::printf("after the shift ends nothing is accepted or recorded\n");
        Fixture f;
        f.session.Update(1.5f);
        f.session.Update(60.0f); // the customer's patience (45 s) runs out, and the tutorial has only one customer
        CHECK(f.session.Simulation().IsFinished());
        f.session.Select("Kettle");
        CHECK(f.session.Press("pour_water") == "the shift is over");
        CHECK(f.session.History().empty());
    }

    void TestSelectionRules()
    {
        std::printf("selection\n");
        Fixture f;
        f.session.Select("Kettle");
        CHECK(f.session.Selected() == "Kettle");
        f.session.Select("NoSuchThing");
        CHECK(f.session.Selected().empty());
        f.session.Select("");
        CHECK(f.session.Buttons().empty());
    }
}

int main(int argc, char** argv)
{
    gAssets = argc > 1 ? argv[1] : "Assets";
    TestEquipmentFromData();
    TestBadData();
    TestPicking();
    TestNearestWins();
    TestOrderFlowsToScreen();
    TestButtonBecomesTimedEvent();
    TestErrorShowsAtTheObject();
    TestHeldPourQuantity();
    TestFullDrinkAndServe();
    TestPressAfterTheShiftEnds();
    TestSelectionRules();
    std::printf("\n%d checks, %d failed\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
