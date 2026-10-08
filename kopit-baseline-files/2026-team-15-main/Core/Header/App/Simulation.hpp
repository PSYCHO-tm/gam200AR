// simulation.hpp - the core of the app. No GL, no window, no wall clock, no platform events.
// The shell owns the loop: it calls Update(dt, input) zero or more times per frame.
#pragma once
#include "App/Animation.hpp"
#include "App/InputState.hpp"
#include "Entity Component System/ComponentRegistry.hpp"
#include "Entity Component System/Ecs.hpp"
#include "Entity Component System/SceneData.hpp"
#include "Timing/Timing.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Core
{
	class Simulation
	{
	public:
		// Loads data/objects/*.json and data/scenes/<sceneFile>. Returns false and logs on failure.
		bool TryInitialize(const std::string& dataDir, const std::string& sceneFile = "baseline.json");
		// Reloads data. On failure the previous scene stays active and GetLastError() says why.
		bool TryReload();
		// Test/tool entry: load from text instead of files. Same keep-old-on-failure rule.
		bool TryLoadFromText(const std::vector<std::string>& defTexts, const std::string& sceneText,
		                     const std::vector<std::string>& animationTexts = {});

		void Update(float dt, const InputState& input);

		// Starts a clip by name, or the clip bound to an interaction action (e.g. "pour_water").
		// Return false (and set GetLastError) when the clip, binding or a target entity is missing.
		bool                   TryPlayAnimation(const std::string& clipName);
		bool                   TriggerAction(const std::string& action);
		const AnimationSystem& GetAnimations() const { return Animator; }

		Registry&                GetRegistry() { return World; }
		const Registry&          GetRegistry() const { return World; }
		const ComponentRegistry& GetComponents() const { return Components; }
		TimingRegistry&          GetTimings() { return Timings; }
		const std::string&       GetLastError() const { return LastError; }
		std::uint64_t            GetTickCount() const { return TickCount; }
		const std::string&       GetDataDir() const { return DataDir; }

	private:
		bool Commit(ObjectDefs& newDefs, ClipLibrary& newClips, const std::string& sceneText);

		ComponentRegistry Components;
		ObjectDefs        Defs;
		ClipLibrary       Clips;
		AnimationSystem   Animator;
		Registry          World;
		TimingRegistry    Timings;
		std::string       DataDir;
		std::string       SceneFile;
		std::string       LastError;
		std::uint64_t     TickCount  = 0;
		bool              Registered = false;
	};
}
