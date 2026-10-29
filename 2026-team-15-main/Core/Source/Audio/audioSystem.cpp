/* ======================================================================================

This code is written by

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

====================================================================================== */

#include "Audio/audioSystem.hpp"

#include "nlohmann/json.hpp"

#include <algorithm>

namespace Core
{
	namespace
	{
		using Json = nlohmann::ordered_json; // ordered = keeps the order the sounds are written in

		float Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

		std::string SdlError() { return SDL_GetError(); }
	}

	AudioSystem::~AudioSystem() { Shutdown(); }

	bool AudioSystem::Fail(const std::string& message)
	{
		lastError = message;
		return false;
	}

	// ======================================================================================
	// START / STOP
	// ======================================================================================

	bool AudioSystem::Init(const std::string& root)
	{
		if (IsReady()) return true; // already running
		assetRoot = root;

		// Turn on SDL's audio part. SDL counts these calls, so it is safe even if the
		// renderer (or anyone else) already called SDL_Init.
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return Fail("SDL audio could not start: " + SdlError());
		ownsSubsystem = true;

		// Open the default speaker. SDL picks the best format for it.
		device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		if (device == 0)
		{
			const std::string why = SdlError();
			CloseDevice(); // keeps any sounds you already loaded
			return Fail("No speaker could be opened: " + why);
		}

		SDL_AudioSpec deviceSpec{};
		SDL_GetAudioDeviceFormat(device, &deviceSpec, nullptr);

		// One "stream" per speaker: 8 for sound effects + 1 for music.
		// A stream is a queue of samples. SDL converts each one to the speaker's format and mixes them all.
		SDL_AudioStream* all[kSfxChannels + 1] = {};
		for (int i = 0; i < kSfxChannels; ++i)
		{
			sfx[i].stream = SDL_CreateAudioStream(&deviceSpec, &deviceSpec);
			all[i]        = sfx[i].stream;
		}
		musicStream       = SDL_CreateAudioStream(&deviceSpec, &deviceSpec);
		all[kSfxChannels] = musicStream;

		for (SDL_AudioStream* stream : all)
		{
			if (stream == nullptr)
			{
				const std::string why = SdlError();
				CloseDevice(); // keeps any sounds you already loaded
				return Fail("Could not create an audio stream: " + why);
			}
		}

		// When the speaker needs more music, SDL calls MusicCallback, which keeps the song looping
		SDL_SetAudioStreamGetCallback(musicStream, MusicCallback, this);

		// Plug all 9 streams into the speaker
		if (!SDL_BindAudioStreams(device, all, kSfxChannels + 1))
		{
			const std::string why = SdlError();
			CloseDevice(); // keeps any sounds you already loaded
			return Fail("Could not connect the audio streams to the speaker: " + why);
		}

		ApplyAllGains();
		lastError.clear();
		return true;
	}

	void AudioSystem::Shutdown()
	{
		manifestLoaded = false; manifestTries = 0;
		CloseDevice();
		for (auto& [name, sound] : sounds)
			SDL_free(sound.data);
		sounds.clear();
	}

	void AudioSystem::CloseDevice()
	{
		// Streams first, so the audio thread stops using our data before anything is freed
		if (musicStream != nullptr) SDL_DestroyAudioStream(musicStream);
		musicStream = nullptr;
		musicName.clear();
		musicPaused = false;
		musicData   = nullptr;
		musicLength = 0;
		musicPos    = 0;

		for (SfxChannel& channel : sfx)
		{
			if (channel.stream != nullptr) SDL_DestroyAudioStream(channel.stream);
			channel = SfxChannel{};
		}

		if (device != 0) SDL_CloseAudioDevice(device);
		device = 0;

		if (ownsSubsystem) SDL_QuitSubSystem(SDL_INIT_AUDIO);
		ownsSubsystem = false;
	}

	// ======================================================================================
	// ADDING / REMOVING SOUNDS
	// ======================================================================================

	std::string AudioSystem::FullPath(const std::string& file) const
	{
		const bool absolute =
			!file.empty() && (file[0] == '/' || file[0] == '\\' || (file.size() > 1 && file[1] == ':'));
		if (absolute || assetRoot.empty()) return file; // Android: relative paths are read from the APK assets

		const char last = assetRoot.back();
		return (last == '/' || last == '\\') ? assetRoot + file : assetRoot + "/" + file;
	}

	bool AudioSystem::Load(const std::string& name, const std::string& file, float volume)
	{
		return LoadInternal(name, file, volume, false);
	}

	bool AudioSystem::LoadInternal(const std::string& name, const std::string& file, float volume, bool fromManifest)
	{
		if (name.empty()) return Fail("A sound needs a name");

		Sound sound;
		sound.file         = file;
		sound.volume       = Clamp01(volume);
		sound.fromManifest = fromManifest;

		// SDL_LoadWAV reads the whole file into memory (on Android it also finds files inside the APK)
		if (!SDL_LoadWAV(FullPath(file).c_str(), &sound.spec, &sound.data, &sound.length))
			return Fail("Could not load \"" + name + "\" from " + FullPath(file) + ": " + SdlError());

		const Uint32 frame = static_cast<Uint32>(SDL_AUDIO_FRAMESIZE(sound.spec));
		if (frame == 0 || sound.length < frame)
		{
			SDL_free(sound.data);
			return Fail("\"" + name + "\" (" + file + ") has no sound in it");
		}
		sound.length -= sound.length % frame; // never end in the middle of a sample

		// Only now (the new file loaded fine) remove the old sound with the same name
		if (IsLoaded(name)) Unload(name);
		sounds[name] = sound;
		return true;
	}

	bool AudioSystem::EnsureLoaded()
	{
		if (manifestLoaded) return true;
		if (!IsReady() || manifestTries >= 3) return false; // needs Init first; stop retrying after 3 attempts
		++manifestTries;
		manifestLoaded = LoadManifest("Json/Audio.json");
		manifestError  = manifestLoaded ? "" : lastError;   // keep the real reason
		return manifestLoaded;
	}

	bool AudioSystem::Unload(const std::string& name)
	{
		const auto it = sounds.find(name);
		if (it == sounds.end()) return Fail("There is no sound called \"" + name + "\"");

		for (int i = 0; i < kSfxChannels; ++i)
			if (sfx[i].soundName == name) StopSfx(i);
		if (musicName == name) StopMusic(); // the music feeder must stop reading this data first

		SDL_free(it->second.data);
		sounds.erase(it);
		return true;
	}

	void AudioSystem::UnloadAll()
	{
		StopAllSfx();
		StopMusic();
		for (auto& [name, sound] : sounds)
			SDL_free(sound.data);
		sounds.clear();
	}

	bool AudioSystem::IsLoaded(const std::string& name) const { return sounds.find(name) != sounds.end(); }

	std::vector<std::string> AudioSystem::GetLoadedNames() const
	{
		std::vector<std::string> names;
		names.reserve(sounds.size());
		for (const auto& [name, sound] : sounds)
			names.push_back(name);
		std::sort(names.begin(), names.end());
		return names;
	}

	float AudioSystem::GetSoundSeconds(const std::string& name) const
	{
		const auto it = sounds.find(name);
		if (it == sounds.end()) return 0.0f;
		const Sound& s     = it->second;
		const int    frame = SDL_AUDIO_FRAMESIZE(s.spec);
		if (frame == 0 || s.spec.freq == 0) return 0.0f;
		return static_cast<float>(s.length / static_cast<Uint32>(frame)) / static_cast<float>(s.spec.freq);
	}

	// Audio.json looks like this (both ways of writing a sound work):
	//   {
	//     "Volume": { "Master": 1.0, "Music": 0.6, "Sfx": 1.0 },
	//     "Music":  { "cafe_theme": { "File": "Audio/Music/cafe_theme.wav", "Volume": 0.8 } },
	//     "Sfx":    { "pour": "Audio/Sfx/pour.wav",
	//                 "bell": { "File": "Audio/Sfx/bell.wav", "Volume": 0.5 } }
	//   }
	bool AudioSystem::LoadManifest(const std::string& jsonPath)
	{
		// SDL_LoadFile also works for files inside the Android APK
		std::size_t size = 0;
		void*       raw  = SDL_LoadFile(FullPath(jsonPath).c_str(), &size);
		if (raw == nullptr) return Fail("Could not read " + FullPath(jsonPath) + ": " + SdlError());
		const std::string text(static_cast<const char*>(raw), size);
		SDL_free(raw);

		const Json root = Json::parse(text, nullptr, false); // false = do not throw on broken JSON
		if (root.is_discarded() || !root.is_object())
			return Fail(jsonPath + ": not valid JSON (check commas, quotes and brackets)");

		std::vector<std::string> errors;

		// ---- 1. Read the list of wanted sounds ----
		struct Wanted
		{
			std::string name;
			std::string file;
			float       volume = 1.0f;
		};
		std::vector<Wanted> wanted;
		manifestSfx.clear();
		manifestMusic.clear();

		for (const char* section : {"Music", "Sfx"})
		{
			const auto it = root.find(section);
			if (it == root.end()) continue;
			if (!it->is_object())
			{
				errors.push_back(std::string(section) + " must be an object like { \"name\": \"file.wav\" }");
				continue;
			}

			for (const auto& [name, entry] : it->items())
			{
				Wanted w;
				w.name = name;
				if (entry.is_string()) // "pour": "Audio/Sfx/pour.wav"
				{
					w.file = entry.get<std::string>();
				}
				else if (entry.is_object() && entry.contains("File") && entry["File"].is_string())
				{
					w.file = entry["File"].get<std::string>();
					if (entry.contains("Volume"))
					{
						if (entry["Volume"].is_number()) w.volume = entry["Volume"].get<float>();
						else
							errors.push_back(name + ": \"Volume\" must be a number");
					}
				}
				else
				{
					errors.push_back(name + ": needs a file, e.g. \"" + name + "\": \"Audio/Sfx/" + name + ".wav\"");
					continue;
				}

				const bool duplicate = std::any_of(wanted.begin(), wanted.end(),
				                                   [&](const Wanted& other) { return other.name == w.name; });
				if (duplicate)
				{
					errors.push_back(name + ": the same name is used twice");
					continue;
				}
				wanted.push_back(w);
				// Remember the order they are written in (the demo uses it for keys 1-8 and 9)
				(std::string(section) == "Music" ? manifestMusic : manifestSfx).push_back(w.name);
			}
		}

		// ---- 2. Remove sounds that came from the manifest but are no longer in it ----
		std::vector<std::string> removed;
		for (const auto& entry : sounds)
		{
			const std::string& soundName = entry.first;
			const bool         stillWanted =
				std::any_of(wanted.begin(), wanted.end(), [&](const Wanted& other) { return other.name == soundName; });
			if (entry.second.fromManifest && !stillWanted) removed.push_back(soundName);
		}
		for (const std::string& name : removed)
			Unload(name);

		// ---- 3. Add new sounds, reload changed ones ----
		for (const Wanted& w : wanted)
		{
			const auto it = sounds.find(w.name);
			if (it != sounds.end() && it->second.file == w.file)
			{
				// Same file as before: keep it (music keeps playing), just take the new volume
				it->second.volume       = Clamp01(w.volume);
				it->second.fromManifest = true;
				continue;
			}
			if (!LoadInternal(w.name, w.file, w.volume, true)) errors.push_back(lastError);
		}

		// ---- 4. Optional group volumes ----
		const auto volume = root.find("Volume");
		if (volume != root.end() && volume->is_object())
		{
			if (volume->contains("Master") && (*volume)["Master"].is_number())
				masterVolume = Clamp01((*volume)["Master"].get<float>());
			if (volume->contains("Music") && (*volume)["Music"].is_number())
				musicVolume = Clamp01((*volume)["Music"].get<float>());
			if (volume->contains("Sfx") && (*volume)["Sfx"].is_number())
				sfxVolume = Clamp01((*volume)["Sfx"].get<float>());
		}
		ApplyAllGains();

		if (!errors.empty())
		{
			std::string all = jsonPath + " has problems:";
			for (const std::string& e : errors)
				all += "\n  - " + e;
			return Fail(all); // the sounds that DID load are still usable
		}
		lastError.clear();
		return true;
	}

	// ======================================================================================
	// SOUND EFFECTS
	// ======================================================================================

	int AudioSystem::PlaySfx(const std::string& name, float volume)
	{
		EnsureLoaded();
		if (!IsReady())
		{
			Fail("Audio is not started (call Init first)");
			return -1;
		}
		const auto it = sounds.find(name);
		if (it == sounds.end())
		{
			Fail("There is no sound called \"" + name + "\"");
			return -1;
		}

		// Pick a FREE speaker. A sound that is already playing is never cut off:
		// pressing 1 then 2 plays both on top of each other. Only if all 8 are busy is the new sound skipped.
		int chosen = -1;
		for (int i = 0; i < kSfxChannels && chosen < 0; ++i)
			if (!IsSfxChannelBusy(i)) chosen = i;
		if (chosen < 0)
		{
			Fail("All " + std::to_string(kSfxChannels) + " sound effect channels are busy, so \"" + name +
			     "\" was skipped (nothing playing gets cut off)");
			return -1;
		}

		SfxChannel&  channel = sfx[chosen];
		const Sound& sound   = it->second;

		SDL_ClearAudioStream(channel.stream);                           // the channel is free, start clean
		SDL_SetAudioStreamFormat(channel.stream, &sound.spec, nullptr); // tell SDL what format this file is in
		channel.soundName  = name;
		channel.playVolume = Clamp01(volume);
		ApplySfxGain(chosen);

		SDL_PutAudioStreamData(channel.stream, sound.data, static_cast<int>(sound.length)); // SDL copies the samples
		SDL_FlushAudioStream(channel.stream); // "that is the whole sound", so the very end is played too
		return chosen;
	}

	void AudioSystem::StopSfx(int channel)
	{
		if (channel < 0 || channel >= kSfxChannels || sfx[channel].stream == nullptr) return;
		SDL_ClearAudioStream(sfx[channel].stream);
		sfx[channel].soundName.clear();
	}

	void AudioSystem::StopAllSfx()
	{
		for (int i = 0; i < kSfxChannels; ++i)
			StopSfx(i);
	}

	bool AudioSystem::IsSfxChannelBusy(int channel) const
	{
		if (channel < 0 || channel >= kSfxChannels || sfx[channel].stream == nullptr) return false;
		SDL_AudioStream* stream = sfx[channel].stream;
		return SDL_GetAudioStreamQueued(stream) > 0 || SDL_GetAudioStreamAvailable(stream) > 0;
	}

	int AudioSystem::GetBusySfxCount() const
	{
		int count = 0;
		for (int i = 0; i < kSfxChannels; ++i)
			if (IsSfxChannelBusy(i)) ++count;
		return count;
	}

	std::string AudioSystem::GetSfxChannelSound(int channel) const
	{
		return IsSfxChannelBusy(channel) ? sfx[channel].soundName : std::string();
	}

	// ======================================================================================
	// MUSIC
	// ======================================================================================

	bool AudioSystem::PlayMusic(const std::string& name, bool loop)
	{
		EnsureLoaded();
		if (!IsReady()) return Fail("Audio is not started (call Init first)");
		const auto it = sounds.find(name);
		if (it == sounds.end()) return Fail("There is no sound called \"" + name + "\"");
		const Sound& sound = it->second;

		// Lock = "audio thread, wait a moment" while we swap the song
		SDL_LockAudioStream(musicStream);
		SDL_ClearAudioStream(musicStream);
		SDL_SetAudioStreamFormat(musicStream, &sound.spec, nullptr);
		musicData   = sound.data;
		musicLength = sound.length;
		musicPos    = 0;
		musicFrame  = SDL_AUDIO_FRAMESIZE(sound.spec);
		musicLoop   = loop;
		SDL_UnlockAudioStream(musicStream);

		musicName = name;
		if (musicPaused) ResumeMusic();
		ApplyMusicGain();
		return true;
	}

	void AudioSystem::StopMusic()
	{
		if (musicStream == nullptr) return;
		SDL_LockAudioStream(musicStream);
		SDL_ClearAudioStream(musicStream);
		musicData   = nullptr;
		musicLength = 0;
		musicPos    = 0;
		SDL_UnlockAudioStream(musicStream);

		musicName.clear();
		if (musicPaused) ResumeMusic(); // plug the (now empty) stream back in, ready for the next song
	}

	void AudioSystem::PauseMusic()
	{
		if (!IsReady() || musicPaused || musicName.empty()) return;
		SDL_UnbindAudioStream(musicStream); // unplugged: the song stays queued exactly where it was
		musicPaused = true;
	}

	void AudioSystem::ResumeMusic()
	{
		if (!IsReady() || !musicPaused) return;
		SDL_BindAudioStream(device, musicStream);
		musicPaused = false;
	}

	bool AudioSystem::IsMusicPlaying() const
	{
		if (!IsReady() || musicName.empty() || musicPaused) return false;

		SDL_LockAudioStream(musicStream);
		const bool moreToFeed = musicData != nullptr && (musicLoop || musicPos < musicLength);
		SDL_UnlockAudioStream(musicStream);
		return moreToFeed || SDL_GetAudioStreamQueued(musicStream) > 0 || SDL_GetAudioStreamAvailable(musicStream) > 0;
	}

	void SDLCALL AudioSystem::MusicCallback(void* userdata, SDL_AudioStream* stream, int additional, int /*total*/)
	{
		static_cast<AudioSystem*>(userdata)->FeedMusic(stream, additional);
	}

	// Runs on SDL's audio thread with the music stream already locked.
	// It gives SDL the next piece of the song, and jumps back to the start when the song ends.
	void AudioSystem::FeedMusic(SDL_AudioStream* stream, int additional)
	{
		if (musicData == nullptr || additional <= 0 || musicFrame <= 0) return;

		const Uint32 frame = static_cast<Uint32>(musicFrame);
		Uint32       need  = (static_cast<Uint32>(additional) + frame - 1) / frame * frame; // whole samples only

		while (need > 0)
		{
			if (musicPos >= musicLength)
			{
				if (!musicLoop)
				{
					SDL_FlushAudioStream(stream); // song is over: let the last bit play out
					return;
				}
				musicPos = 0; // loop back to the start
			}
			const Uint32 chunk = std::min(need, musicLength - musicPos);
			SDL_PutAudioStreamData(stream, musicData + musicPos, static_cast<int>(chunk));
			musicPos += chunk;
			need -= chunk;
		}
	}

	// ======================================================================================
	// VOLUME
	// ======================================================================================

	float AudioSystem::SoundVolume(const std::string& name) const
	{
		const auto it = sounds.find(name);
		return it == sounds.end() ? 1.0f : it->second.volume;
	}

	// Final loudness = master x group (sfx or music) x the sound's own volume (x the PlaySfx volume)
	void AudioSystem::ApplySfxGain(int channel)
	{
		SfxChannel& ch = sfx[channel];
		if (ch.stream == nullptr) return;
		const float gain = muted ? 0.0f : masterVolume * sfxVolume * SoundVolume(ch.soundName) * ch.playVolume;
		SDL_SetAudioStreamGain(ch.stream, gain);
	}

	void AudioSystem::ApplyMusicGain()
	{
		if (musicStream == nullptr) return;
		const float gain = muted ? 0.0f : masterVolume * musicVolume * SoundVolume(musicName);
		SDL_SetAudioStreamGain(musicStream, gain);
	}

	void AudioSystem::ApplyAllGains()
	{
		for (int i = 0; i < kSfxChannels; ++i)
			ApplySfxGain(i);
		ApplyMusicGain();
	}

	void AudioSystem::SetMasterVolume(float volume)
	{
		masterVolume = Clamp01(volume);
		ApplyAllGains();
	}

	void AudioSystem::SetSfxVolume(float volume)
	{
		sfxVolume = Clamp01(volume);
		ApplyAllGains();
	}

	void AudioSystem::SetMusicVolume(float volume)
	{
		musicVolume = Clamp01(volume);
		ApplyMusicGain();
	}

	void AudioSystem::SetMuted(bool mute)
	{
		muted = mute;
		ApplyAllGains();
	}

	// ======================================================================================
	// WHOLE SPEAKER
	// ======================================================================================

	void AudioSystem::PauseAll()
	{
		if (IsReady()) SDL_PauseAudioDevice(device);
	}

	void AudioSystem::ResumeAll()
	{
		if (IsReady()) SDL_ResumeAudioDevice(device);
	}
}
