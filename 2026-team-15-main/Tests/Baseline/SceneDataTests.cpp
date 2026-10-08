#include <string>

#include "App/Simulation.hpp"
#include "Entity Component System/Components.hpp"
#include "Entity Component System/SceneData.hpp"
#include "TestFramework.hpp"

using namespace Core;

namespace
{
	std::string const goodDef =
		R"({"type":"crate","components":{"Transform":{"position":[1,2,3]},"Spinner":{"degreesPerSecond":45}}})";
	std::string const goodScene = R"({"instances":[{"type":"crate","name":"a"}]})";

	bool ErrorContains(Simulation const& sim, char const* text)
	{
		return sim.GetLastError().find(text) != std::string::npos;
	}
}

KOPIT_TEST(SceneRejectsWrongFieldTypeWithClearError)
{
	Simulation        sim;
	std::string const bad = R"({"type":"crate","components":{"Transform":{"position":"oops"}}})";
	KOPIT_CHECK(!sim.TryLoadFromText({bad}, goodScene));
	KOPIT_CHECK(ErrorContains(sim, "Transform.position"));
}

KOPIT_TEST(SceneRejectsUnknownComponentAndField)
{
	Simulation        sim;
	std::string const unknownComp = R"({"type":"crate","components":{"Warp":{}}})";
	KOPIT_CHECK(!sim.TryLoadFromText({unknownComp}, goodScene));
	KOPIT_CHECK(ErrorContains(sim, "unknown component 'Warp'"));
	std::string const unknownField = R"({"type":"crate","components":{"Spinner":{"speed":5}}})";
	KOPIT_CHECK(!sim.TryLoadFromText({unknownField}, goodScene));
	KOPIT_CHECK(ErrorContains(sim, "unknown field 'speed'"));
}

KOPIT_TEST(SceneRejectsBadJsonAndMissingType)
{
	Simulation sim;
	KOPIT_CHECK(!sim.TryLoadFromText({"{ not json"}, goodScene));
	KOPIT_CHECK(ErrorContains(sim, "invalid JSON"));
	KOPIT_CHECK(!sim.TryLoadFromText({R"({"components":{}})"}, goodScene));
	KOPIT_CHECK(!sim.TryLoadFromText({goodDef}, R"({"instances":[{"type":"nope"}]})"));
	KOPIT_CHECK(ErrorContains(sim, "unknown object type 'nope'"));
}

KOPIT_TEST(FailedReloadKeepsPreviousScene)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryLoadFromText({goodDef}, goodScene));
	KOPIT_CHECK(sim.GetRegistry().EntityCount() == 1);
	KOPIT_CHECK(!sim.TryLoadFromText({goodDef}, R"({"instances":[{"type":"missing"}]})"));
	KOPIT_CHECK(sim.GetRegistry().EntityCount() == 1);
	KOPIT_CHECK(!sim.GetLastError().empty());
}

KOPIT_TEST(SaveThenReloadReproducesEntities)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryLoadFromText(
		{goodDef},
		R"({"instances":[{"type":"crate","name":"a"},{"type":"crate","name":"b","overrides":{"Transform":{"position":[9,8,7]}}}]})"));
	nlohmann::json const saved = SerializeScene(sim.GetRegistry(), sim.GetComponents());

	Registry    reloaded;
	std::string error;
	KOPIT_CHECK(InstantiateScene(saved, ObjectDefs{}, sim.GetComponents(), reloaded, error));
	KOPIT_CHECK(reloaded.EntityCount() == 2);
	KOPIT_CHECK(reloaded.TryGet<Transform>(2)->Position.x == 9.0f);
	KOPIT_CHECK(reloaded.TryGet<Name>(1)->Value == "a");
	KOPIT_CHECK(reloaded.TryGet<Spinner>(2)->DegreesPerSecond == 45.0f);
	// Saving again gives identical text (deterministic output).
	KOPIT_CHECK(SerializeScene(reloaded, sim.GetComponents()).dump() == saved.dump());
}

KOPIT_TEST(AssetsLoadFromDataFolder)
{
	Simulation sim;
	KOPIT_CHECK(sim.TryInitialize(KOPIT_DATA_DIR));
	KOPIT_CHECK(sim.GetRegistry().EntityCount() >= 3);
}
