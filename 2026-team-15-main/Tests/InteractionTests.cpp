// Host tests for M1G05: no window, no graphics context, no Android.
// Exits non-zero if any CHECK fails, so a script or CI server can rely on the exit code.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "Collider/Picking.hpp"
#include "GameObject/EquipmentDef.hpp"
#include "GameObject/InteractionEngine.hpp"
#include "GameObject/MeshData.hpp"

namespace
{
	int gFailures = 0;

#define CHECK(cond)                                                                                                    \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(cond))                                                                                                   \
		{                                                                                                              \
			std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                              \
			++gFailures;                                                                                               \
		}                                                                                                              \
	} while (0)

	bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

	std::string ReadFile(std::string const& path)
	{
		std::ifstream     file(path, std::ios::binary);
		std::stringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	Core::Aabb Box(float cx, float cy, float cz, float half)
	{
		return {{cx - half, cy - half, cz - half}, {cx + half, cy + half, cz + half}};
	}

	// Camera 1 m back on +z looking at the origin, 800x600 viewport.
	struct Camera
	{
		Core::Mat4 View = Core::LookAt({0, 0.1f, 1.0f}, {0, 0.05f, 0}, {0, 1, 0});
		Core::Mat4 Proj = Core::Perspective(1.0f, 800.0f / 600.0f, 0.01f, 10.0f);
	};

	Core::EquipmentDef MakeDef(std::string const& id, Core::Vec3 position, float half)
	{
		Core::EquipmentDef def;
		def.Id       = id;
		def.Position = position;
		def.Bounds   = Box(0, half, 0, half);
		def.Interactions.push_back({"pour_water", "pour", 220.0f, "ml"});
		def.Interactions.push_back({"serve", "serve", 1.0f, "cup"});
		return def;
	}

	void TestRayAabb()
	{
		std::printf("ray vs aabb\n");
		Core::Aabb const box = Box(0, 0, -2, 0.5f);
		float            t   = -1;
		CHECK(Core::IntersectRayAabb({{0, 0, 0}, {0, 0, -1}}, box, t));
		CHECK(Near(t, 1.5f));
		CHECK(!Core::IntersectRayAabb({{0, 0, 0}, {0, 1, 0}}, box, t));  // misses
		CHECK(!Core::IntersectRayAabb({{0, 0, 0}, {0, 0, 1}}, box, t));  // box is behind
		CHECK(!Core::IntersectRayAabb({{2, 0, 0}, {0, 0, -1}}, box, t)); // parallel, outside slab
		CHECK(Core::IntersectRayAabb({{0, 0, -2}, {0, 0, -1}}, box, t)); // starts inside
		CHECK(Near(t, 0.0f));
	}

	void TestPickNearest()
	{
		std::printf("pick nearest\n");
		std::vector<Core::Aabb> const boxes = {Box(0, 0, -5, 0.5f), Box(0, 0, -2, 0.5f), Box(3, 0, -2, 0.5f)};
		Core::PickResult const        pick  = Core::PickNearest({{0, 0, 0}, {0, 0, -1}}, boxes);
		CHECK(pick.Index == 1); // the closer of the two boxes on the ray
		CHECK(Near(pick.Distance, 1.5f));
		CHECK(Core::PickNearest({{0, 5, 0}, {0, 0, -1}}, boxes).Index == -1);
	}

	void TestScreenRay()
	{
		std::printf("screen point to ray\n");
		Camera const    cam;
		Core::Ray const centre = Core::ScreenPointToRay(400, 300, 800, 600, cam.View, cam.Proj);
		CHECK(centre.Direction.z < -0.9f);
		CHECK(Near(centre.Direction.x, 0.0f, 0.02f));

		Core::Ray const left  = Core::ScreenPointToRay(100, 300, 800, 600, cam.View, cam.Proj);
		Core::Ray const right = Core::ScreenPointToRay(700, 300, 800, 600, cam.View, cam.Proj);
		CHECK(left.Direction.x < 0.0f);
		CHECK(right.Direction.x > 0.0f);

		Core::Ray const top    = Core::ScreenPointToRay(400, 50, 800, 600, cam.View, cam.Proj);
		Core::Ray const bottom = Core::ScreenPointToRay(400, 550, 800, 600, cam.View, cam.Proj);
		CHECK(top.Direction.y > bottom.Direction.y); // screen y grows downward
	}

	void TestObj()
	{
		std::printf("obj parser\n");
		std::string const cube = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 1\nv 1 0 1\nv 1 1 1\nv 0 1 1\n"
								 "f 1 2 3 4\nf 5 6 7 8\nf 1 2 6 5\nf 2 3 7 6\nf 3 4 8 7\nf 4 1 5 8\n";
		Core::MeshData    mesh;
		std::string       error;
		CHECK(Core::ParseObj(cube, mesh, error));
		CHECK(mesh.VertexCount() == 36); // 6 quads -> 12 triangles
		CHECK(Near(mesh.Bounds.Min.x, 0) && Near(mesh.Bounds.Max.x, 1) && Near(mesh.Bounds.Max.z, 1));

		CHECK(!Core::ParseObj("v 0 0 0\nf 1 2 3\n", mesh, error)); // index out of range
		CHECK(error.find("line 2") != std::string::npos);
		CHECK(!Core::ParseObj("# nothing here\n", mesh, error));
	}

	void TestLoaderErrors()
	{
		std::printf("equipment loader\n");
		std::vector<Core::EquipmentDef> defs;
		std::string                     error;

		CHECK(Core::LoadEquipmentDefs(R"({"EQUIPMENT":{"Kettle":{"Mesh":"k.obj","Position":[1,2,3],"Scale":2}}})", defs,
		                              error));
		CHECK(defs.size() == 1 && defs[0].Id == "Kettle" && Near(defs[0].Position.y, 2) && Near(defs[0].Scale, 2));

		CHECK(!Core::LoadEquipmentDefs(R"({"EQUIPMENT":{"Kettle":{"Scale":1}}})", defs, error));
		CHECK(error.find("Kettle") != std::string::npos && error.find("Mesh") != std::string::npos);

		CHECK(!Core::LoadEquipmentDefs(R"({"EQUIPMENT":{"A":{"Mesh":"a.obj","Position":[1,2]}}})", defs, error));
		CHECK(error.find("Position") != std::string::npos);

		CHECK(!Core::LoadEquipmentDefs(R"({"EQUIPMENT":{"A":{"Mesh":"a.obj","Interactions":[{"Action":"x"}]}}})", defs,
		                               error));
		CHECK(error.find("Button") != std::string::npos);

		CHECK(!Core::LoadEquipmentDefs("{ not json", defs, error));
		CHECK(!error.empty());
		CHECK(!Core::LoadEquipmentDefs(R"({"WRONG":{}})", defs, error));
	}

	void TestShippedData()
	{
		std::printf("shipped Equipment.json and meshes\n");
		std::string const               root = M1G05_PROJECT_ROOT;
		std::vector<Core::EquipmentDef> defs;
		std::string                     error;
		CHECK(Core::LoadEquipmentDefs(ReadFile(root + "/Assets/Json/Equipment.json"), defs, error));
		if (!error.empty())
		{
			std::printf("  loader said: %s\n", error.c_str());
		}
		CHECK(defs.size() >= 3);
		for (Core::EquipmentDef const& def : defs)
		{
			CHECK(!def.Interactions.empty());
			Core::MeshData mesh;
			CHECK(Core::ParseObj(ReadFile(root + "/" + def.MeshPath), mesh, error));
			CHECK(mesh.VertexCount() > 0);
			CHECK(mesh.Bounds.Max.y > mesh.Bounds.Min.y);
		}
	}

	void TestEngineFlow()
	{
		std::printf("engine: touch select, button event, errors\n");
		Camera const            cam;
		Core::InteractionEngine engine;
		// Two items side by side, 0.1 m boxes.
		engine.SetEquipment({MakeDef("Kettle", {-0.2f, 0, 0}, 0.1f), MakeDef("SockFilter", {0.2f, 0, 0}, 0.1f)});

		auto touchAt = [&](Core::Vec3 world)
		{
			Core::Mat4 const vp = Core::Multiply(cam.Proj, cam.View);
			float const cx = vp.Values[0] * world.x + vp.Values[4] * world.y + vp.Values[8] * world.z + vp.Values[12];
			float const cy = vp.Values[1] * world.x + vp.Values[5] * world.y + vp.Values[9] * world.z + vp.Values[13];
			float const cw = vp.Values[3] * world.x + vp.Values[7] * world.y + vp.Values[11] * world.z + vp.Values[15];
			Core::InteractionInput in;
			in.Kind    = Core::InputKind::TOUCH_DOWN;
			in.ScreenX = (cx / cw * 0.5f + 0.5f) * 800.0f;
			in.ScreenY = (1.0f - (cy / cw * 0.5f + 0.5f)) * 600.0f;
			engine.HandleInput(in, cam.View, cam.Proj, 800, 600);
		};
		auto press = [&](char const* button)
		{
			Core::InteractionInput in;
			in.Kind   = Core::InputKind::BUTTON_PRESS;
			in.Button = button;
			engine.HandleInput(in, cam.View, cam.Proj, 800, 600);
		};

		press("pour"); // nothing selected yet
		CHECK(engine.DrainEvents().empty());

		touchAt({-0.2f, 0.1f, 0}); // the kettle
		CHECK(engine.GetSelected() == 0);

		engine.Update(0.5f);
		engine.Update(0.25f);
		press("pour");
		auto events = engine.DrainEvents();
		CHECK(events.size() == 1);
		if (events.size() == 1)
		{
			CHECK(events[0].Object == "Kettle");
			CHECK(events[0].Action == "pour_water");
			CHECK(Near(events[0].Quantity, 220.0f));
			CHECK(Near(static_cast<float>(events[0].Time), 0.75f)); // simulation time, not wall clock
		}
		CHECK(engine.DrainEvents().empty()); // drained once

		press("unknown_button");
		CHECK(engine.DrainEvents().empty());

		touchAt({0.2f, 0.1f, 0}); // the sock filter
		CHECK(engine.GetSelected() == 1);
		press("serve");
		events = engine.DrainEvents();
		CHECK(events.size() == 1 && events[0].Object == "SockFilter" && events[0].Action == "serve");

		touchAt({0.0f, 0.1f, 0}); // empty table between the two
		CHECK(engine.GetSelected() == -1);

		engine.ShowError("Kettle", "Wrong quantity", 1.0f);
		CHECK(engine.GetErrors().size() == 1); // visible immediately, so the very next frame draws it
		engine.Update(0.5f);
		CHECK(engine.GetErrors().size() == 1);
		engine.Update(0.6f);
		CHECK(engine.GetErrors().empty());
	}

	void TestAnchorMovesPicking()
	{
		std::printf("engine: moving the anchor moves the pick bounds\n");
		Camera const            cam;
		Core::InteractionEngine engine;
		engine.SetEquipment({MakeDef("Kettle", {0, 0, 0}, 0.1f)});

		Core::InteractionInput in;
		in.Kind    = Core::InputKind::TOUCH_DOWN;
		in.ScreenX = 400;
		in.ScreenY = 300;
		engine.HandleInput(in, cam.View, cam.Proj, 800, 600);
		CHECK(engine.GetSelected() == 0); // centre of screen is over the kettle

		engine.SetAnchorPose(Core::Translate({0.5f, 0, 0}));
		engine.HandleInput(in, cam.View, cam.Proj, 800, 600);
		CHECK(engine.GetSelected() == -1); // kettle moved away from the centre
	}
}

int main()
{
	TestRayAabb();
	TestPickNearest();
	TestScreenRay();
	TestObj();
	TestLoaderErrors();
	TestShippedData();
	TestEngineFlow();
	TestAnchorMovesPicking();

	if (gFailures == 0)
	{
		std::printf("ALL TESTS PASSED\n");
		return 0;
	}
	std::printf("%d CHECK(S) FAILED\n", gFailures);
	return 1;
}
