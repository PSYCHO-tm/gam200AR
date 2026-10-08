#include "App/Animation.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "Entity Component System/Components.hpp"
#include "Entity Component System/SceneData.hpp"

namespace Core
{
	using Json = nlohmann::json;

	namespace
	{
		bool ReadVec3Value(Json const& json, Vec3& out)
		{
			if (!json.is_array() || json.size() != 3 || !json[0].is_number() || !json[1].is_number() ||
			    !json[2].is_number())
			{
				return false;
			}
			out = Vec3{json[0].get<float>(), json[1].get<float>(), json[2].get<float>()};
			return true;
		}

		bool ParseTrack(Json const& json, std::string const& context, Track& track, float duration, std::string& error)
		{
			if (!json.is_object() || !json.contains("target") || !json["target"].is_string())
			{
				error = context + ": 'target' is required and must be a string";
				return false;
			}
			track.Target = json["target"].get<std::string>();

			std::string const property = json.value("property", std::string{});
			if (property == "position")
			{
				track.Property = ANIM_PROPERTY::POSITION;
			}
			else if (property == "rotationDeg")
			{
				track.Property = ANIM_PROPERTY::ROTATION;
			}
			else if (property == "scale")
			{
				track.Property = ANIM_PROPERTY::SCALE;
			}
			else if (property == "color")
			{
				track.Property = ANIM_PROPERTY::COLOR;
			}
			else
			{
				error = context + ": 'property' must be position, rotationDeg, scale or color";
				return false;
			}

			bool const isOffsetProperty =
				track.Property == ANIM_PROPERTY::POSITION || track.Property == ANIM_PROPERTY::ROTATION;
			track.Relative = json.value("relative", isOffsetProperty);

			std::string const ease = json.value("ease", std::string{"smooth"});
			if (ease == "linear")
			{
				track.Ease = ANIM_EASE::LINEAR;
			}
			else if (ease == "smooth")
			{
				track.Ease = ANIM_EASE::SMOOTH;
			}
			else
			{
				error = context + ": 'ease' must be linear or smooth";
				return false;
			}

			if (!json.contains("keys") || !json["keys"].is_array() || json["keys"].empty())
			{
				error = context + ": 'keys' must be a non-empty array";
				return false;
			}
			float previousTime = -1.0f;
			for (Json const& keyJson : json["keys"])
			{
				Keyframe key;
				if (!keyJson.is_object() || !keyJson.contains("t") || !keyJson["t"].is_number() ||
				    !keyJson.contains("value") || !ReadVec3Value(keyJson["value"], key.Value))
				{
					error = context + ": each key needs 't' (number) and 'value' (array of 3 numbers)";
					return false;
				}
				key.Time = keyJson["t"].get<float>();
				if (key.Time < previousTime || key.Time > duration)
				{
					error = context + ": key times must increase and stay within the clip duration";
					return false;
				}
				previousTime = key.Time;
				track.Keys.push_back(key);
			}
			return true;
		}

		float ApplyEase(ANIM_EASE ease, float u) { return ease == ANIM_EASE::SMOOTH ? u * u * (3.0f - 2.0f * u) : u; }
	}

	bool ParseAnimationFile(std::string const& text, std::string const& sourceName, ClipLibrary& library,
	                        std::string& error)
	{
		Json const json = Json::parse(text, nullptr, false);
		if (json.is_discarded() || !json.is_object())
		{
			error = sourceName + ": invalid JSON";
			return false;
		}

		if (json.contains("bindings"))
		{
			if (!json["bindings"].is_object())
			{
				error = sourceName + ": 'bindings' must be an object";
				return false;
			}
			for (auto const& item : json["bindings"].items())
			{
				if (!item.value().is_string())
				{
					error = sourceName + ": binding '" + item.key() + "' must map to a clip name (string)";
					return false;
				}
				library.Bindings[item.key()] = item.value().get<std::string>();
			}
			return true;
		}

		Clip clip;
		if (!json.contains("name") || !json["name"].is_string() || json["name"].get<std::string>().empty())
		{
			error = sourceName + ": 'name' is required";
			return false;
		}
		clip.Name = json["name"].get<std::string>();
		if (!json.contains("duration") || !json["duration"].is_number() || json["duration"].get<float>() <= 0.0f)
		{
			error = sourceName + ": 'duration' must be a number greater than 0";
			return false;
		}
		clip.Duration = json["duration"].get<float>();
		clip.Loop     = json.value("loop", false);

		if (!json.contains("tracks") || !json["tracks"].is_array() || json["tracks"].empty())
		{
			error = sourceName + ": 'tracks' must be a non-empty array";
			return false;
		}
		std::size_t index = 0;
		for (Json const& trackJson : json["tracks"])
		{
			Track             track;
			std::string const context = sourceName + " (" + clip.Name + ") tracks[" + std::to_string(index++) + "]";
			if (!ParseTrack(trackJson, context, track, clip.Duration, error))
			{
				return false;
			}
			clip.Tracks.push_back(std::move(track));
		}
		if (library.Clips.count(clip.Name) != 0)
		{
			error = sourceName + ": duplicate clip name '" + clip.Name + "'";
			return false;
		}
		library.Clips[clip.Name] = std::move(clip);
		return true;
	}

	bool LoadAnimationsFromDir(std::string const& dir, ClipLibrary& library, std::string& error)
	{
		namespace Fs = std::filesystem;
		std::error_code code;
		if (!Fs::is_directory(dir, code))
		{
			return true;
		}
		std::vector<Fs::path> files;
		for (auto const& entry : Fs::directory_iterator(dir, code))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".json")
			{
				files.push_back(entry.path());
			}
		}
		std::sort(files.begin(), files.end());
		for (Fs::path const& file : files)
		{
			std::string text;
			if (!ReadTextFile(file.string(), text))
			{
				error = "cannot read " + file.string();
				return false;
			}
			if (!ParseAnimationFile(text, file.filename().string(), library, error))
			{
				return false;
			}
		}
		return true;
	}

	Vec3 SampleTrack(Track const& track, float time)
	{
		std::vector<Keyframe> const& keys = track.Keys;
		if (time <= keys.front().Time)
		{
			return keys.front().Value;
		}
		if (time >= keys.back().Time)
		{
			return keys.back().Value;
		}
		for (std::size_t i = 1; i < keys.size(); ++i)
		{
			if (time <= keys[i].Time)
			{
				Keyframe const& a    = keys[i - 1];
				Keyframe const& b    = keys[i];
				float const     span = b.Time - a.Time;
				float const     u    = span > 1e-8f ? ApplyEase(track.Ease, (time - a.Time) / span) : 1.0f;
				return a.Value + (b.Value - a.Value) * u;
			}
		}
		return keys.back().Value;
	}

	bool AnimationSystem::IsPlaying(std::string const& clipName) const
	{
		return std::any_of(Active.begin(), Active.end(),
		                   [&](Instance const& instance) { return instance.ClipData.Name == clipName; });
	}

	bool AnimationSystem::Play(Clip const& clip, Registry& registry, std::string& error)
	{
		if (IsPlaying(clip.Name))
		{
			return true;
		}
		Instance instance;
		instance.ClipData = clip;
		for (Track const& track : clip.Tracks)
		{
			BoundTrack bound;
			registry.ForEach<Name>(
				[&](Entity entity, Name& name)
				{
					if (bound.Target == 0 && name.Value == track.Target)
					{
						bound.Target = entity;
					}
				});
			if (bound.Target == 0)
			{
				error = "animation '" + clip.Name + "': no entity named '" + track.Target + "'";
				return false;
			}
			Transform const* transform = registry.TryGet<Transform>(bound.Target);
			if (track.Property == ANIM_PROPERTY::POSITION && transform != nullptr)
			{
				bound.Base = transform->Position;
			}
			else if (track.Property == ANIM_PROPERTY::ROTATION && transform != nullptr)
			{
				bound.Base = transform->Rotation;
			}
			else if ((track.Property == ANIM_PROPERTY::SCALE || track.Property == ANIM_PROPERTY::POSITION ||
			          track.Property == ANIM_PROPERTY::ROTATION) &&
			         transform == nullptr)
			{
				error = "animation '" + clip.Name + "': entity '" + track.Target + "' has no Transform";
				return false;
			}
			else if (track.Property == ANIM_PROPERTY::COLOR && !registry.Has<Material>(bound.Target))
			{
				error = "animation '" + clip.Name + "': entity '" + track.Target + "' has no Material";
				return false;
			}
			instance.Bound.push_back(bound);
		}
		Active.push_back(std::move(instance));
		return true;
	}

	void AnimationSystem::Apply(Instance const& instance, Registry& registry, float time)
	{
		for (std::size_t i = 0; i < instance.ClipData.Tracks.size(); ++i)
		{
			Track const&      track     = instance.ClipData.Tracks[i];
			BoundTrack const& bound     = instance.Bound[i];
			Vec3 const        value     = SampleTrack(track, time);
			Transform*        transform = registry.TryGet<Transform>(bound.Target);
			switch (track.Property)
			{
				case ANIM_PROPERTY::POSITION:
					transform->PrevPosition = transform->Position;
					transform->Position     = track.Relative ? bound.Base + value : value;
					break;
				case ANIM_PROPERTY::ROTATION:
					transform->PrevRotation = transform->Rotation;
					transform->Rotation     = track.Relative ? bound.Base + value : value;
					break;
				case ANIM_PROPERTY::SCALE:
					transform->PrevScale = transform->Scale;
					transform->Scale     = value;
					break;
				case ANIM_PROPERTY::COLOR:
					registry.TryGet<Material>(bound.Target)->Color = value;
					break;
			}
		}
	}

	void AnimationSystem::Update(Registry& registry, float dt)
	{
		for (Instance& instance : Active)
		{
			instance.Time += dt;
			float time = instance.Time;
			if (instance.ClipData.Loop)
			{
				time = std::fmod(time, instance.ClipData.Duration);
			}
			else
			{
				time = std::min(time, instance.ClipData.Duration);
			}
			Apply(instance, registry, time);
		}
		Active.erase(std::remove_if(Active.begin(), Active.end(), [](Instance const& instance)
		                            { return !instance.ClipData.Loop && instance.Time >= instance.ClipData.Duration; }),
		             Active.end());
	}
}
