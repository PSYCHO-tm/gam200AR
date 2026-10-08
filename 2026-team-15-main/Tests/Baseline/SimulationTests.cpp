#include <cmath>
#include <string>
#include <vector>

#include "App/Simulation.hpp"
#include "Entity Component System/Components.hpp"
#include "TestFramework.hpp"

using namespace Core;

namespace
{
	std::string CrateDef()
	{
		return R"({
		"type": "crate",
		"components": {
			"Transform": { "position": [0, 0.05, 0], "scale": [0.1, 0.1, 0.1] },
			"MeshRef": { "path": "meshes/testCube.obj" },
			"Material": { "color": [0.5, 0.5, 0.5] },
			"Spinner": { "degreesPerSecond": 90 }
		}
	})";
	}

	std::string TwoCrates()
	{
		return R"({
		"instances": [
			{ "type": "crate", "name": "crateA" },
			{ "type": "crate", "name": "crateB",
			  "overrides": { "Transform": { "position": [0.5, 0.05, 0] }, "Material": { "color": [1, 0, 0] } } }
		]
	})";
	}

	float RotationY(Simulation& sim, Entity e) { return sim.GetRegistry().TryGet<Transform>(e)->Rotation.y; }
}

KOPIT_TEST(SimLoadsTwoInstancesOfSameTypeWithDifferentValues)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryLoadFromText({CrateDef()}, TwoCrates()));
	KOPIT_CHECK(sim.GetRegistry().EntityCount() == 2);
	Transform const* a = sim.GetRegistry().TryGet<Transform>(1);
	Transform const* b = sim.GetRegistry().TryGet<Transform>(2);
	KOPIT_CHECK(a != nullptr && b != nullptr);
	KOPIT_CHECK(a->Position.x == 0.0f && b->Position.x == 0.5f);
	KOPIT_CHECK(sim.GetRegistry().TryGet<Material>(2)->Color.x == 1.0f);
	KOPIT_CHECK(sim.GetRegistry().TryGet<Material>(1)->Color.x == 0.5f);
	// Override of one field keeps the other fields from the definition.
	KOPIT_CHECK(std::fabs(b->Scale.x - 0.1f) < 1e-6f);
}

KOPIT_TEST(SimUpdateAdvancesByTimestepOnly)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryLoadFromText({CrateDef()}, TwoCrates()));
	InputState input;
	for (int i = 0; i < 60; ++i)
		sim.Update(1.0f / 60.0f, input);
	KOPIT_CHECK(std::fabs(RotationY(sim, 1) - 90.0f) < 0.01f);
	KOPIT_CHECK(sim.GetTickCount() == 60);
}

KOPIT_TEST(SimInputChangesSpeedAndReset)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryLoadFromText({CrateDef()}, TwoCrates()));
	InputState fast;
	fast.SpinAxis = 1.0f;
	for (int i = 0; i < 60; ++i)
		sim.Update(1.0f / 60.0f, fast);
	KOPIT_CHECK(std::fabs(RotationY(sim, 1) - 180.0f) < 0.02f);
	InputState reset;
	reset.Reset = true;
	sim.Update(1.0f / 60.0f, reset);
	KOPIT_CHECK(RotationY(sim, 1) == 0.0f);
}

KOPIT_TEST(SimIsDeterministic)
{
	Simulation a;
	Simulation b;
	KOPIT_CHECK(a.TryLoadFromText({CrateDef()}, TwoCrates()));
	KOPIT_CHECK(b.TryLoadFromText({CrateDef()}, TwoCrates()));
	InputState input;
	input.SpinAxis = 0.5f;
	for (int i = 0; i < 500; ++i)
	{
		a.Update(1.0f / 60.0f, input);
		b.Update(1.0f / 60.0f, input);
	}
	KOPIT_CHECK(RotationY(a, 1) == RotationY(b, 1));
}

KOPIT_TEST(SimRecordsTimings)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryLoadFromText({CrateDef()}, TwoCrates()));
	sim.Update(1.0f / 60.0f, InputState{});
	KOPIT_CHECK(sim.GetTimings().LastMs("Simulation.Update") >= 0.0);
	KOPIT_CHECK(!sim.GetTimings().Names().empty());
}
