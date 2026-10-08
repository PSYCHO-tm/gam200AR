#pragma once

// Data-driven animation (M1G05: pouring, straining).
// A clip is a list of tracks. A track animates one property (position, rotation, scale or color) of the entity whose
// Name matches 'Target', using keyframes. Clips and the action->clip bindings live in Assets/Animations/*.json.
// Everything advances by the fixed timestep only (no wall clock), so animations are deterministic and testable.

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "Entity Component System/Ecs.hpp"
#include "Math/Vec3.hpp"

namespace Core
{
	enum class ANIM_PROPERTY
	{
		POSITION,
		ROTATION,
		SCALE,
		COLOR
	};

	enum class ANIM_EASE
	{
		LINEAR,
		SMOOTH
	};

	struct Keyframe
	{
		float Time = 0.0f;
		Vec3  Value;
	};

	struct Track
	{
		std::string           Target; // Name component of the entity to animate
		ANIM_PROPERTY         Property = ANIM_PROPERTY::POSITION;
		ANIM_EASE             Ease     = ANIM_EASE::SMOOTH;
		bool                  Relative = true; // position/rotation: offset from the value when the clip started
		std::vector<Keyframe> Keys;
	};

	struct Clip
	{
		std::string        Name;
		float              Duration = 1.0f;
		bool               Loop     = false;
		std::vector<Track> Tracks;
	};

	struct ClipLibrary
	{
		std::map<std::string, Clip>        Clips;
		std::map<std::string, std::string> Bindings; // interaction action (e.g. "pour_water") -> clip name
	};

	// A file holds either one clip ({"name", "duration", "tracks"}) or bindings ({"bindings": {...}}).
	bool ParseAnimationFile(std::string const& text, std::string const& sourceName, ClipLibrary& library,
	                        std::string& error);
	// A missing folder is fine (no animations). Any bad file is an error naming the file and field.
	bool LoadAnimationsFromDir(std::string const& dir, ClipLibrary& library, std::string& error);

	Vec3 SampleTrack(Track const& track, float time);

	class AnimationSystem
	{
	public:
		// Starts a clip. Playing a clip that is already playing does nothing. Fails if a target entity is missing.
		bool        Play(Clip const& clip, Registry& registry, std::string& error);
		void        Update(Registry& registry, float dt);
		bool        IsPlaying(std::string const& clipName) const;
		std::size_t ActiveCount() const { return Active.size(); }
		void        StopAll() { Active.clear(); }

	private:
		struct BoundTrack
		{
			Entity Target = 0;
			Vec3   Base;
		};

		struct Instance
		{
			Clip                    ClipData;
			float                   Time = 0.0f;
			std::vector<BoundTrack> Bound;
		};

		static void Apply(Instance const& instance, Registry& registry, float time);

		std::vector<Instance> Active;
	};
}
