#include <cmath>
#include <string>

#include "sim/interaction/interactionEngine.hpp"
#include "testCheck.hpp"

using namespace Sim;

namespace
{
	int ViewW() { return 1280; }
	int ViewH() { return 720; }

	// Where the center of a piece of equipment appears on screen, so tests can "touch" it.
	InteractionInput TouchAt(InteractionEngine const& engine, std::string const& id, Mat4 const& view, Mat4 const& proj)
	{
		for (PickCandidate const& candidate : engine.BuildCandidates())
		{
			if (candidate.Id == id)
			{
				Vec3 const       center = (candidate.WorldBounds.Min + candidate.WorldBounds.Max) * 0.5f;
				Vec3 const       ndc    = TransformPoint(Multiply(proj, view), center);
				InteractionInput input;
				input.Kind      = INPUT_KIND::TOUCH;
				input.X         = (ndc.x * 0.5f + 0.5f) * static_cast<float>(ViewW());
				input.Y         = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(ViewH());
				input.ViewportW = ViewW();
				input.ViewportH = ViewH();
				return input;
			}
		}
		return {};
	}

	InteractionInput Press(char const* action)
	{
		InteractionInput input;
		input.Kind   = INPUT_KIND::BUTTON;
		input.Action = action;
		return input;
	}
}

int main()
{
	EquipmentLibrary library;
	std::string      error;
	TEST_CHECK(LoadEquipmentFile(RepoRoot() + "/Assets/Json/Equipment.json", library, error));
	TEST_CHECK(AttachMeshBounds(library, RepoRoot(), error));

	InteractionEngine engine;
	engine.SetEquipment(library);
	Mat4 const view = LookAt({0.0f, 0.35f, 0.7f}, {0.0f, 0.05f, 0.0f}, {0, 1, 0});
	Mat4 const proj = Perspective(60.0f, static_cast<float>(ViewW()) / static_cast<float>(ViewH()), 0.01f, 10.0f);

	// Criterion: a touch selects the object it hits, and touching empty space clears the selection.
	TEST_CHECK(engine.Handle(TouchAt(engine, "Kettle", view, proj), view, proj).Selected == "Kettle");
	TEST_CHECK(engine.Handle(TouchAt(engine, "SockFilter", view, proj), view, proj).Selected == "SockFilter");
	TEST_CHECK(engine.Handle(TouchAt(engine, "CondensedMilkTin", view, proj), view, proj).Selected ==
	           "CondensedMilkTin");
	InteractionInput corner;
	corner.X         = 3;
	corner.Y         = 3;
	corner.ViewportW = ViewW();
	corner.ViewportH = ViewH();
	TEST_CHECK(engine.Handle(corner, view, proj).Selected.empty());

	// A button with nothing selected, or one the selected equipment does not offer, is rejected without an event.
	TEST_CHECK(!engine.Handle(Press("pour_water"), view, proj).Handled);
	engine.Handle(TouchAt(engine, "Kettle", view, proj), view, proj);
	InteractionResult const wrong = engine.Handle(Press("serve"), view, proj);
	TEST_CHECK(!wrong.Handled && !wrong.EventEmitted);
	TEST_CHECK(engine.DrainEvents().empty());

	// Criterion: a press becomes a timed event (object, action, quantity) with simulation time.
	engine.Update(0.5f);
	InteractionResult const poured = engine.Handle(Press("pour_water"), view, proj);
	TEST_CHECK(poured.Handled && poured.EventEmitted);
	std::vector<TimedEvent> events = engine.DrainEvents();
	TEST_CHECK(events.size() == 1);
	if (events.size() == 1)
	{
		TEST_CHECK(events[0].Object == "Kettle" && events[0].Action == "pour_water");
		TEST_CHECK(events[0].Quantity == 220.0f && std::fabs(events[0].Time - 0.5) < 1e-9);
	}
	TEST_CHECK(engine.DrainEvents().empty());
	TEST_CHECK(engine.GetHistory().size() == 1);

	// Criterion: an error is visible right after it is reported (the next frame can draw it), then expires.
	engine.ReportError({"Kettle", "wrong_quantity", "Too much water"}, 2.0f);
	TEST_CHECK(engine.GetActiveErrors().size() == 1 && engine.GetActiveErrors()[0].Error.Object == "Kettle");
	engine.Update(1.0f);
	TEST_CHECK(engine.GetActiveErrors().size() == 1);
	engine.Update(1.5f);
	TEST_CHECK(engine.GetActiveErrors().empty());

	// Moving the anchor moves the pick targets with it: the old screen spot now misses, and a camera that
	// follows the anchor hits the kettle again.
	InteractionInput const oldSpot = TouchAt(engine, "Kettle", view, proj);
	TEST_CHECK(engine.Handle(oldSpot, view, proj).Selected == "Kettle");
	engine.SetAnchorPose(Translate({2.0f, 0.0f, 0.0f}));
	TEST_CHECK(engine.Handle(oldSpot, view, proj).Selected.empty());
	Mat4 const followView = LookAt({2.0f, 0.35f, 0.7f}, {2.0f, 0.05f, 0.0f}, {0, 1, 0});
	TEST_CHECK(engine.Handle(TouchAt(engine, "Kettle", followView, proj), followView, proj).Selected == "Kettle");
	return TestFinish("test_interaction");
}
