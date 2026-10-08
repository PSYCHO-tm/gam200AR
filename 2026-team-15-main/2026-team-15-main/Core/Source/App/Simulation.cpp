#include "App/Simulation.hpp"

#include "App/Systems.hpp"
#include "Log/Log.hpp"

namespace Core
{
	bool Simulation::TryInitialize(const std::string& dataDir, const std::string& sceneFile)
	{
		DataDir   = dataDir;
		SceneFile = sceneFile;
		if (!Registered)
		{
			RegisterBuiltinComponents(Components);
			Registered = true;
		}
		return TryReload();
	}

	bool Simulation::TryReload()
	{
		ScopedTimer timer(Timings, "Data.Load");
		ObjectDefs  newDefs;
		ClipLibrary newClips;
		std::string text;
		if (!LoadObjectDefsFromDir(DataDir + "/Objects", newDefs, LastError) ||
		    !ReadTextFile(DataDir + "/Scenes/" + SceneFile, text))
		{
			if (LastError.empty())
				LastError = "cannot read scene file " + DataDir + "/Scenes/" + SceneFile;
			LogError("data load failed: " + LastError);
			return false;
		}
		if (!LoadAnimationsFromDir(DataDir + "/Animations", newClips, LastError))
		{
			LogError("data load failed: " + LastError);
			return false;
		}
		return Commit(newDefs, newClips, text);
	}

	bool Simulation::TryLoadFromText(const std::vector<std::string>& defTexts, const std::string& sceneText,
	                                 const std::vector<std::string>& animationTexts)
	{
		if (!Registered)
		{
			RegisterBuiltinComponents(Components);
			Registered = true;
		}
		ObjectDefs newDefs;
		for (std::size_t i = 0; i < defTexts.size(); ++i)
		{
			if (!ParseObjectDef(defTexts[i], "def[" + std::to_string(i) + "]", newDefs, LastError))
			{
				LogError("data load failed: " + LastError);
				return false;
			}
		}
		ClipLibrary newClips;
		for (std::size_t i = 0; i < animationTexts.size(); ++i)
		{
			if (!ParseAnimationFile(animationTexts[i], "anim[" + std::to_string(i) + "]", newClips, LastError))
			{
				LogError("data load failed: " + LastError);
				return false;
			}
		}
		return Commit(newDefs, newClips, sceneText);
	}

	bool Simulation::Commit(ObjectDefs& newDefs, ClipLibrary& newClips, const std::string& sceneText)
	{
		Registry    newWorld;
		std::string error;
		if (!InstantiateSceneText(sceneText, newDefs, Components, newWorld, error))
		{
			LastError = error;
			LogError("data load failed: " + LastError);
			return false;
		}
		for (const auto& binding : newClips.Bindings)
		{
			if (newClips.Clips.count(binding.second) == 0)
			{
				LastError = "animation binding '" + binding.first + "' refers to unknown clip '" + binding.second + "'";
				LogError("data load failed: " + LastError);
				return false;
			}
		}
		Defs  = std::move(newDefs);
		Clips = std::move(newClips);
		Animator.StopAll();
		World = std::move(newWorld);
		LastError.clear();
		LogInfo("data loaded: " + std::to_string(World.EntityCount()) + " entities, " + std::to_string(Defs.size()) +
		        " object types");
		return true;
	}

	void Simulation::Update(float dt, const InputState& input)
	{
		ScopedTimer timer(Timings, "Simulation.Update");
		SpinSystem(World, dt, input);
		{
			ScopedTimer animTimer(Timings, "Animation.Update");
			Animator.Update(World, dt);
		}
		++TickCount;
	}

	bool Simulation::TryPlayAnimation(const std::string& clipName)
	{
		const auto found = Clips.Clips.find(clipName);
		if (found == Clips.Clips.end())
		{
			LastError = "unknown animation clip '" + clipName + "'";
			return false;
		}
		if (!Animator.Play(found->second, World, LastError))
		{
			LogError(LastError);
			return false;
		}
		return true;
	}

	bool Simulation::TriggerAction(const std::string& action)
	{
		const auto binding = Clips.Bindings.find(action);
		if (binding == Clips.Bindings.end())
		{
			LastError = "no animation bound to action '" + action + "'";
			return false;
		}
		return TryPlayAnimation(binding->second);
	}
}
