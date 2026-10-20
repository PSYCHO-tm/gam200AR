#include <cmath>

#include "sim/collider/picking.hpp"
#include "testCheck.hpp"

using namespace Sim;

int main()
{
	Aabb const box{{-1, -1, -1}, {1, 1, 1}};
	float      distance = -1.0f;

	// Ray from +z toward the box hits the front face at distance 4.
	TEST_CHECK(RayIntersectsAabb({{0, 0, 5}, {0, 0, -1}}, box, distance));
	TEST_CHECK(std::fabs(distance - 4.0f) < 1e-4f);
	// Misses: off to the side, pointing away, and parallel outside a slab.
	TEST_CHECK(!RayIntersectsAabb({{3, 0, 5}, {0, 0, -1}}, box, distance));
	TEST_CHECK(!RayIntersectsAabb({{0, 0, 5}, {0, 0, 1}}, box, distance));
	TEST_CHECK(!RayIntersectsAabb({{0, 5, 5}, {0, 0, -1}}, box, distance));
	// Origin inside the box counts as a hit at distance 0.
	TEST_CHECK(RayIntersectsAabb({{0, 0, 0}, {0, 0, -1}}, box, distance) && distance == 0.0f);

	// A rotated box grows to enclose its corners: 45 degrees about Y makes a 1x1 box about 1.414 wide.
	Aabb const turned = TransformAabb({{-0.5f, 0, -0.5f}, {0.5f, 1, 0.5f}}, RotateYDeg(45));
	TEST_CHECK(std::fabs(turned.Max.x - 0.7071f) < 1e-3f && std::fabs(turned.Min.x + 0.7071f) < 1e-3f);

	// Nearest wins when two boxes line up.
	std::vector<PickCandidate> const both = {{"far", {{-1, -1, -6}, {1, 1, -4}}}, {"near", {{-1, -1, -1}, {1, 1, 1}}}};
	PickResult const                 hit  = PickNearest({{0, 0, 5}, {0, 0, -1}}, both);
	TEST_CHECK(hit.Hit && hit.Id == "near");
	TEST_CHECK(!PickNearest({{50, 50, 5}, {0, 0, -1}}, both).Hit);

	// Screen point to ray: the screen center looks straight at the target, a corner does not.
	Mat4 const view   = LookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
	Mat4 const proj   = Perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
	Ray const  center = ScreenPointToRay(640, 360, 1280, 720, view, proj);
	TEST_CHECK(std::fabs(center.Direction.z + 1.0f) < 1e-3f && std::fabs(center.Direction.x) < 1e-3f);
	TEST_CHECK(PickNearest(center, both).Id == "near");
	Ray const corner = ScreenPointToRay(5, 5, 1280, 720, view, proj);
	TEST_CHECK(!PickNearest(corner, both).Hit);
	// Pixel y grows downward: a point above the center points upward in the world.
	TEST_CHECK(ScreenPointToRay(640, 100, 1280, 720, view, proj).Direction.y > 0.0f);
	return TestFinish("test_picking");
}
