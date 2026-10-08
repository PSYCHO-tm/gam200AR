#include <cmath>
#include <string>

#include "App/Animation.hpp"
#include "App/Simulation.hpp"
#include "Entity Component System/Components.hpp"
#include "TestFramework.hpp"

using namespace Core;

namespace
{
	std::string BoxDef()
	{
		return R"({"type":"box","components":{"Transform":{"position":[0,1,0]},"Material":{"color":[1,1,1]}}})";
	}

	std::string BoxScene() { return R"({"instances":[{"type":"box","name":"box"}]})"; }

	std::string MoveClip()
	{
		return R"({"name":"move","duration":1.0,"tracks":[
			{"target":"box","property":"position","ease":"linear","keys":[{"t":0,"value":[0,0,0]},{"t":1,"value":[0,2,0]}]}]})";
	}

	Entity FindByName(Registry& reg, std::string const& name)
	{
		Entity found = 0;
		reg.ForEach<Name>(
			[&](Entity entity, Name& value)
			{
				if (value.Value == name)
				{
					found = entity;
				}
			});
		return found;
	}

	void Run(Simulation& sim, float seconds)
	{
		int const ticks = static_cast<int>(std::lround(seconds * 60.0f));
		for (int i = 0; i < ticks; ++i)
		{
			sim.Update(1.0f / 60.0f, InputState{});
		}
	}
}

KOPIT_TEST(AnimSampleEasingAndClamping)
{
	Track track;
	track.Keys = {{0.0f, Vec3{0, 0, 0}}, {1.0f, Vec3{10, 0, 0}}};
	track.Ease = ANIM_EASE::LINEAR;
	KOPIT_CHECK(std::fabs(SampleTrack(track, 0.5f).x - 5.0f) < 1e-5f);
	track.Ease = ANIM_EASE::SMOOTH;
	KOPIT_CHECK(std::fabs(SampleTrack(track, 0.5f).x - 5.0f) < 1e-5f);
	KOPIT_CHECK(std::fabs(SampleTrack(track, 0.25f).x - 1.5625f) < 1e-4f);
	KOPIT_CHECK(SampleTrack(track, -1.0f).x == 0.0f);
	KOPIT_CHECK(SampleTrack(track, 5.0f).x == 10.0f);
}

KOPIT_TEST(AnimRelativePositionPlaysAndFinishes)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryLoadFromText({BoxDef()}, BoxScene(), {MoveClip()}));
	Entity const box = FindByName(sim.GetRegistry(), "box");
	KOPIT_CHECK(sim.TryPlayAnimation("move"));
	KOPIT_CHECK(sim.GetAnimations().IsPlaying("move"));
	Run(sim, 0.5f);
	KOPIT_CHECK(std::fabs(sim.GetRegistry().TryGet<Transform>(box)->Position.y - 2.0f) < 0.02f);
	Run(sim, 0.6f);
	KOPIT_CHECK(sim.GetAnimations().ActiveCount() == 0);
	KOPIT_CHECK(std::fabs(sim.GetRegistry().TryGet<Transform>(box)->Position.y - 3.0f) < 1e-4f);
}

KOPIT_TEST(AnimPlayingTwiceDoesNotDuplicateAndLoopKeepsGoing)
{
	Simulation        sim;
	std::string const loopClip = R"({"name":"spin","duration":1.0,"loop":true,"tracks":[
		{"target":"box","property":"rotationDeg","keys":[{"t":0,"value":[0,0,0]},{"t":1,"value":[0,360,0]}]}]})";
	KOPIT_CHECK(sim.TryLoadFromText({BoxDef()}, BoxScene(), {loopClip}));
	KOPIT_CHECK(sim.TryPlayAnimation("spin"));
	KOPIT_CHECK(sim.TryPlayAnimation("spin"));
	KOPIT_CHECK(sim.GetAnimations().ActiveCount() == 1);
	Run(sim, 2.5f);
	KOPIT_CHECK(sim.GetAnimations().ActiveCount() == 1);
}

KOPIT_TEST(AnimAbsoluteScaleAndColor)
{
	Simulation        sim;
	std::string const clip = R"({"name":"grow","duration":1.0,"tracks":[
		{"target":"box","property":"scale","keys":[{"t":0,"value":[1,1,1]},{"t":1,"value":[2,2,2]}]},
		{"target":"box","property":"color","keys":[{"t":0,"value":[1,1,1]},{"t":1,"value":[0,0.5,0]}]}]})";
	KOPIT_CHECK(sim.TryLoadFromText({BoxDef()}, BoxScene(), {clip}));
	Entity const box = FindByName(sim.GetRegistry(), "box");
	KOPIT_CHECK(sim.TryPlayAnimation("grow"));
	Run(sim, 1.1f);
	KOPIT_CHECK(sim.GetRegistry().TryGet<Transform>(box)->Scale.x == 2.0f);
	KOPIT_CHECK(sim.GetRegistry().TryGet<Material>(box)->Color.y == 0.5f);
}

KOPIT_TEST(AnimErrorsAreClear)
{
	Simulation        sim;
	std::string const ghostClip = R"({"name":"g","duration":1.0,"tracks":[
		{"target":"ghost","property":"position","keys":[{"t":0,"value":[0,0,0]}]}]})";
	KOPIT_CHECK(sim.TryLoadFromText({BoxDef()}, BoxScene(), {ghostClip}));
	KOPIT_CHECK(!sim.TryPlayAnimation("g"));
	KOPIT_CHECK(sim.GetLastError().find("no entity named 'ghost'") != std::string::npos);
	KOPIT_CHECK(!sim.TryPlayAnimation("nope"));
	KOPIT_CHECK(sim.GetLastError().find("unknown animation clip 'nope'") != std::string::npos);
	KOPIT_CHECK(!sim.TriggerAction("no_such_action"));

	std::string const badProperty = R"({"name":"b","duration":1.0,"tracks":[
		{"target":"box","property":"wobble","keys":[{"t":0,"value":[0,0,0]}]}]})";
	KOPIT_CHECK(!sim.TryLoadFromText({BoxDef()}, BoxScene(), {badProperty}));
	KOPIT_CHECK(sim.GetLastError().find("'property' must be") != std::string::npos);

	std::string const lateKey = R"({"name":"l","duration":1.0,"tracks":[
		{"target":"box","property":"position","keys":[{"t":0,"value":[0,0,0]},{"t":5,"value":[0,0,0]}]}]})";
	KOPIT_CHECK(!sim.TryLoadFromText({BoxDef()}, BoxScene(), {lateKey}));
	KOPIT_CHECK(sim.GetLastError().find("within the clip duration") != std::string::npos);

	std::string const badBinding = R"({"bindings":{"pour_water":"MissingClip"}})";
	KOPIT_CHECK(!sim.TryLoadFromText({BoxDef()}, BoxScene(), {badBinding}));
	KOPIT_CHECK(sim.GetLastError().find("unknown clip 'MissingClip'") != std::string::npos);
}

KOPIT_TEST(AnimReloadStopsRunningAnimations)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryLoadFromText({BoxDef()}, BoxScene(), {MoveClip()}));
	KOPIT_CHECK(sim.TryPlayAnimation("move"));
	KOPIT_CHECK(sim.GetAnimations().ActiveCount() == 1);
	KOPIT_CHECK(sim.TryLoadFromText({BoxDef()}, BoxScene(), {MoveClip()}));
	KOPIT_CHECK(sim.GetAnimations().ActiveCount() == 0);
}

KOPIT_TEST(AnimPourWaterFromAssetsEndToEnd)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryInitialize(KOPIT_DATA_DIR));
	Registry&    reg    = sim.GetRegistry();
	Entity const kettle = FindByName(reg, "kettle");
	Entity const stream = FindByName(reg, "waterStream");
	Entity const filter = FindByName(reg, "filter");
	KOPIT_CHECK(kettle != 0 && stream != 0 && filter != 0);

	Vec3 const kettleStart = reg.TryGet<Transform>(kettle)->Position;
	Vec3 const filterStart = reg.TryGet<Material>(filter)->Color;

	KOPIT_CHECK(sim.TriggerAction("pour_water"));
	Run(sim, 1.7f); // mid-pour: kettle tilted over the filter, stream flowing
	KOPIT_CHECK(std::fabs(reg.TryGet<Transform>(kettle)->Rotation.z + 50.0f) < 0.5f);
	KOPIT_CHECK(reg.TryGet<Transform>(kettle)->Position.x > kettleStart.x + 0.2f);
	KOPIT_CHECK(reg.TryGet<Transform>(stream)->Scale.y > 0.06f);

	Run(sim, 1.5f); // finished: kettle back home, stream gone, filter stained
	KOPIT_CHECK(sim.GetAnimations().ActiveCount() == 0);
	KOPIT_CHECK(std::fabs(reg.TryGet<Transform>(kettle)->Position.x - kettleStart.x) < 1e-3f);
	KOPIT_CHECK(std::fabs(reg.TryGet<Transform>(kettle)->Rotation.z) < 1e-3f);
	KOPIT_CHECK(reg.TryGet<Transform>(stream)->Scale.y < 0.001f);
	KOPIT_CHECK(reg.TryGet<Material>(filter)->Color.x < filterStart.x - 0.2f);
}

KOPIT_TEST(AnimAllBoundActionsPlayFromAssets)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryInitialize(KOPIT_DATA_DIR));
	char const* actions[] = {"pour_water", "add_grounds", "pour_condensed_milk", "serve"};
	for (char const* action : actions)
	{
		KOPIT_CHECK(sim.TriggerAction(action));
	}
	KOPIT_CHECK(sim.GetAnimations().ActiveCount() == 4);
	Run(sim, 3.5f);
	KOPIT_CHECK(sim.GetAnimations().ActiveCount() == 0);
	Entity const level = FindByName(sim.GetRegistry(), "coffeeLevel");
	KOPIT_CHECK(sim.GetRegistry().TryGet<Transform>(level)->Scale.y > 0.019f);
}
