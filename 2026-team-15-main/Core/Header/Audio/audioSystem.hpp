/* ======================================================================================

This code is written by

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

====================================================================================== */

#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace Core
{
	class AudioSystem
	{
	public:
		static constexpr int kSfxChannels = 8; // how many sound effects can play at the same time

		AudioSystem() = default;
		~AudioSystem();
		AudioSystem(const AudioSystem&)            = delete; // owns SDL objects, so it must not be copied
		AudioSystem& operator=(const AudioSystem&) = delete;

		// ---- Start / stop ------------------------------------------------------------------

		// Opens the default speaker. assetRoot is put in front of every relative file path
		// (desktop: ".../Assets", Android: "" because SDL reads relative paths from the APK assets).
		bool Init(const std::string& assetRoot = "");
		void Shutdown(); // stops everything and frees all sounds (also called by the destructor)
		bool IsReady() const { return device != 0; }

		// ---- Adding / removing sounds --------------------------------------------------------

		// Reads a list of sounds from JSON (path relative to assetRoot, e.g. "Json/Audio.json").
		// Call it again after editing the file: new sounds are loaded, deleted ones are removed,
		// changed ones are reloaded. Sounds you added with Load() in code are left alone.
		bool LoadManifest(const std::string& jsonPath);

		bool EnsureLoaded();
		const std::string& GetManifestError() const { return manifestError; }
// private
		bool manifestLoaded = false;
		int  manifestTries  = 0;
		std::string manifestError;

		// Adds one sound under a name. If the name already exists, the old sound is replaced.
		// volume (0..1) is this sound's own loudness, multiplied with the other volumes.
		bool Load(const std::string& name, const std::string& file, float volume = 1.0f);
		bool Unload(const std::string& name); // stops it if it is playing, then frees it
		void UnloadAll();

		bool                     IsLoaded(const std::string& name) const;
		std::vector<std::string> GetLoadedNames() const;                         // sorted A-Z
		float                    GetSoundSeconds(const std::string& name) const; // length, 0 if not loaded

		// The names under "Sfx" / "Music" in the manifest, in the order they are written in the file
		const std::vector<std::string>& GetManifestSfx() const { return manifestSfx; }
		const std::vector<std::string>& GetManifestMusic() const { return manifestMusic; }

		// ---- Sound effects (up to 8 at the same time) ----------------------------------------

		// Plays on a FREE channel, on top of whatever is already playing (nothing gets cut off).
		// Returns the channel (0..7), or -1 if it could not play (all 8 busy, unknown name, ...).
		int  PlaySfx(const std::string& name, float volume = 1.0f);
		void StopSfx(int channel);
		void StopAllSfx();
		bool IsSfxChannelBusy(int channel) const;
		int  GetBusySfxCount() const;
		// Name of the sound on that channel ("" if the channel is free)
		std::string GetSfxChannelSound(int channel) const;

		// ---- Background music (1 at a time) --------------------------------------------------

		bool               PlayMusic(const std::string& name, bool loop = true); // replaces the current music
		void               StopMusic();
		void               PauseMusic();
		void               ResumeMusic();
		bool               IsMusicPlaying() const; // true while music is audible (not paused, not finished)
		const std::string& GetMusicName() const { return musicName; }

		// ---- Volume (0 = silent, 1 = full) ---------------------------------------------------

		void  SetMasterVolume(float volume);
		void  SetSfxVolume(float volume);
		void  SetMusicVolume(float volume);
		float GetMasterVolume() const { return masterVolume; }
		float GetSfxVolume() const { return sfxVolume; }
		float GetMusicVolume() const { return musicVolume; }
		void  SetMuted(bool mute);
		bool  IsMuted() const { return muted; }

		// ---- Whole speaker (e.g. the Android app goes to the background) ---------------------

		void PauseAll();
		void ResumeAll();

		const std::string& GetLastError() const { return lastError; }

	private:
		struct Sound
		{
			SDL_AudioSpec spec{};           // sample format, channels and rate of the file
			Uint8*        data   = nullptr; // raw samples (owned, freed with SDL_free)
			Uint32        length = 0;       // bytes
			float         volume = 1.0f;
			std::string   file; // as written in Load() / the manifest
			bool          fromManifest = false;
		};

		struct SfxChannel
		{
			SDL_AudioStream* stream = nullptr;
			std::string      soundName;
			float            playVolume = 1.0f; // the volume passed to PlaySfx
		};

		// Music feeding runs on SDL's audio thread. SDL locks the music stream before calling it.
		static void SDLCALL MusicCallback(void* userdata, SDL_AudioStream* stream, int additional, int total);
		void                FeedMusic(SDL_AudioStream* stream, int additional);

		void        CloseDevice(); // closes the speaker but keeps the loaded sounds
		bool        LoadInternal(const std::string& name, const std::string& file, float volume, bool fromManifest);
		std::string FullPath(const std::string& file) const;
		float       SoundVolume(const std::string& name) const;
		void        ApplySfxGain(int channel);
		void        ApplyMusicGain();
		void        ApplyAllGains();
		bool        Fail(const std::string& message); // stores the message, returns false

		SDL_AudioDeviceID                      device = 0;
		std::string                            assetRoot;
		std::unordered_map<std::string, Sound> sounds;
		std::array<SfxChannel, kSfxChannels>   sfx{};
		std::vector<std::string>               manifestSfx; // names in file order
		std::vector<std::string>               manifestMusic;

		SDL_AudioStream* musicStream = nullptr;
		std::string      musicName;
		bool             musicPaused = false;
		// Copied from the playing Sound, only changed while the music stream is locked
		const Uint8* musicData   = nullptr;
		Uint32       musicLength = 0;
		Uint32       musicPos    = 0;
		int          musicFrame  = 1; // bytes per sample frame, so we never cut a sample in half
		bool         musicLoop   = true;

		float masterVolume  = 1.0f;
		float sfxVolume     = 1.0f;
		float musicVolume   = 1.0f;
		bool  muted         = false;
		bool  ownsSubsystem = false; // we called SDL_InitSubSystem(SDL_INIT_AUDIO) and must quit it

		std::string lastError;
	};
}
