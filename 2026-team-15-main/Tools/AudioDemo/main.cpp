/* ======================================================================================

This code is written by...

Name:           Lee Mingxuan
Course:         RTIS
Digipen ID:     mingxuan.lee

====================================================================================== */

#include "Audio/audioSystem.hpp"

#include <array>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#	include <conio.h> // _getch(): read one key without waiting for ENTER
#else
#	include <termios.h>
#	include <unistd.h>
#endif

#ifndef KOPI_ASSETS_ROOT
#	define KOPI_ASSETS_ROOT "Assets"
#endif

namespace
{
	constexpr int kKeys = core::AudioSystem::kSfxChannels; // keys 1..8

	// ---- Reading single key presses -----------------------------------------------------
	// Normally the console waits for ENTER. "Raw mode" hands us every key the moment it is pressed.
	class KeyReader
	{
	public:
		KeyReader() { SetRaw(true); }
		~KeyReader() { SetRaw(false); }

		// Returns the key, or -1 when there is no more input
		int GetKey()
		{
#ifdef _WIN32
			const int c = _getch();
			if (c == 0 || c == 0xE0) // arrow / function keys send two codes: ignore them
			{
				_getch();
				return 0;
			}
			return c;
#else
			unsigned char c = 0;
			return read(STDIN_FILENO, &c, 1) == 1 ? c : -1;
#endif
		}

		// For typed commands: switch back to normal typing until ENTER
		std::string GetLine()
		{
			SetRaw(false);
			std::string line;
#ifdef _WIN32
			std::getline(std::cin, line);
#else
			unsigned char c = 0;
			while (read(STDIN_FILENO, &c, 1) == 1 && c != '\n')
				line += static_cast<char>(c);
#endif
			SetRaw(true);
			return line;
		}

	private:
		void SetRaw(bool on)
		{
#ifndef _WIN32
			if (!isatty(STDIN_FILENO)) return; // input comes from a file / pipe: nothing to switch
			if (on && !raw)
			{
				tcgetattr(STDIN_FILENO, &saved);
				termios t = saved;
				t.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO)); // no line buffering, no echo
				t.c_cc[VMIN]  = 1;
				t.c_cc[VTIME] = 0;
				tcsetattr(STDIN_FILENO, TCSANOW, &t);
				raw = true;
			}
			else if (!on && raw)
			{
				tcsetattr(STDIN_FILENO, TCSANOW, &saved);
				raw = false;
			}
#else
			(void)on; // _getch() already reads single keys on Windows
#endif
		}

#ifndef _WIN32
		termios saved{};
		bool    raw = false;
#endif
	};

	// ---- Which key plays which sound ----------------------------------------------------
	struct KeyMap
	{
		std::array<std::string, kKeys> sfx;   // sfx[0] = key 1
		std::string                    music; // key 9

		// Keys follow the order in Audio.json
		void FromManifest(const core::AudioSystem& audio)
		{
			sfx.fill("");
			const auto& list = audio.GetManifestSfx();
			for (std::size_t i = 0; i < list.size() && i < sfx.size(); ++i)
				sfx[i] = list[i];
			music = audio.GetManifestMusic().empty() ? "" : audio.GetManifestMusic().front();
		}

		// A sound added while running gets the first key that is empty or whose sound was removed
		int Assign(const core::AudioSystem& audio, const std::string& name)
		{
			for (int i = 0; i < kKeys; ++i)
				if (sfx[i] == name) return i;
			for (int i = 0; i < kKeys; ++i)
			{
				if (sfx[i].empty() || !audio.IsLoaded(sfx[i]))
				{
					sfx[i] = name;
					return i;
				}
			}
			return -1;
		}
	};

	void PrintKeys(const core::AudioSystem& audio, const KeyMap& keys)
	{
		std::printf("\n---------------------------------------------------------------\n");
		for (int i = 0; i < kKeys; ++i)
		{
			const std::string& name = keys.sfx[i];
			std::printf("  [%d] %-12s%s\n", i + 1, name.empty() ? "-" : name.c_str(),
			            name.empty() || audio.IsLoaded(name) ? "" : "  (not loaded)");
		}
		std::printf("  [9] %-12s music on/off\n\n", keys.music.empty() ? "-" : keys.music.c_str());
		std::printf("  [0] stop all sfx   [p] pause/resume music   [+/-] volume   [m] mute\n"
		            "  [l] list channels  [r] reload Audio.json    [c] type a command (load/unload)\n"
		            "  [q] quit\n"
		            "---------------------------------------------------------------\n");
	}

	void PrintChannels(const core::AudioSystem& audio)
	{
		std::printf("Channels (%d/%d playing):", audio.GetBusySfxCount(), kKeys);
		for (int i = 0; i < kKeys; ++i)
		{
			const std::string sound = audio.GetSfxChannelSound(i);
			std::printf(" [%d:%s]", i + 1, sound.empty() ? "-" : sound.c_str());
		}
		std::printf("\nMusic: %s%s   Volume: master %.1f, music %.1f, sfx %.1f%s\n",
		            audio.GetMusicName().empty() ? "(off)" : audio.GetMusicName().c_str(),
		            audio.GetMusicName().empty() ? "" : (audio.IsMusicPlaying() ? " (playing)" : " (paused)"),
		            audio.GetMasterVolume(), audio.GetMusicVolume(), audio.GetSfxVolume(),
		            audio.IsMuted() ? "  MUTED" : "");
	}

	// Typed commands after pressing c
	void RunCommand(core::AudioSystem& audio, KeyMap& keys, const std::string& line)
	{
		std::istringstream in(line);
		std::string        cmd;
		in >> cmd;

		if (cmd == "load")
		{
			std::string name, file;
			float       volume = 1.0f;
			in >> name >> file;
			if (!(in >> volume)) volume = 1.0f;
			if (!audio.Load(name, file, volume))
			{
				std::printf("ERROR: %s\n", audio.GetLastError().c_str());
				return;
			}
			const int key = keys.Assign(audio, name);
			if (key >= 0) std::printf("Added \"%s\" -> key %d\n", name.c_str(), key + 1);
			else
				std::printf("Added \"%s\" (all 8 keys are used: free one with unload first)\n", name.c_str());
		}
		else if (cmd == "unload")
		{
			std::string name;
			in >> name;
			if (audio.Unload(name)) std::printf("Removed \"%s\"\n", name.c_str());
			else
				std::printf("ERROR: %s\n", audio.GetLastError().c_str());
		}
		else if (cmd == "music")
		{
			std::string name;
			in >> name;
			if (audio.PlayMusic(name))
			{
				keys.music = name;
				std::printf("Music \"%s\" is playing (key 9 now controls it)\n", name.c_str());
			}
			else
				std::printf("ERROR: %s\n", audio.GetLastError().c_str());
		}
		else if (cmd == "volume")
		{
			std::string which;
			float       value = 1.0f;
			in >> which >> value;
			if (which == "master") audio.SetMasterVolume(value);
			else if (which == "music")
				audio.SetMusicVolume(value);
			else if (which == "sfx")
				audio.SetSfxVolume(value);
			else
				std::printf("Use: volume master|music|sfx <0..1>\n");
		}
		else if (cmd == "wait") // for scripted tests
		{
			float seconds = 1.0f;
			in >> seconds;
			SDL_Delay(static_cast<Uint32>(seconds * 1000.0f));
		}
		else if (!cmd.empty())
		{
			std::printf("Commands: load <name> <file> [volume] | unload <name> | music <name> | "
			            "volume master|music|sfx <0..1>\n");
		}
	}
}

int main(int argc, char** argv)
{
	std::string assets   = KOPI_ASSETS_ROOT;
	std::string manifest = "Json/Audio.json";

	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--assets" && i + 1 < argc) assets = argv[++i];
		else if (arg == "--manifest" && i + 1 < argc)
			manifest = argv[++i];
		else
		{
			std::printf("Usage: audio_demo [--assets FOLDER] [--manifest Json/Audio.json]\n");
			return 0;
		}
	}

	core::AudioSystem audio;
	if (!audio.Init(assets))
	{
		std::printf("ERROR: %s\n", audio.GetLastError().c_str());
		return 1;
	}
	std::printf("Audio started (SDL driver: %s). Assets folder: %s\n", SDL_GetCurrentAudioDriver(), assets.c_str());
	if (!audio.LoadManifest(manifest)) std::printf("WARNING: %s\n", audio.GetLastError().c_str());

	KeyMap keys;
	keys.FromManifest(audio);
	PrintKeys(audio, keys);

	KeyReader reader;
	while (true)
	{
		const int key = reader.GetKey();
		if (key < 0 || key == 'q' || key == 'Q' || key == 27) break; // 27 = ESC

		if (key >= '1' && key <= '8')
		{
			const int          index = key - '1';
			const std::string& name  = keys.sfx[index];
			if (name.empty())
			{
				std::printf("[%c] has no sound. Add one to Audio.json or press c and type: load <name> <file>\n", key);
				continue;
			}
			const int channel = audio.PlaySfx(name);
			if (channel >= 0)
				std::printf("[%c] %-10s -> channel %d   (%d/%d playing)\n", key, name.c_str(), channel + 1,
				            audio.GetBusySfxCount(), kKeys);
			else
				std::printf("[%c] %s\n", key, audio.GetLastError().c_str());
		}
		else if (key == '9')
		{
			if (keys.music.empty()) std::printf("[9] No music in Audio.json\n");
			else if (audio.GetMusicName() == keys.music && audio.IsMusicPlaying())
			{
				audio.StopMusic();
				std::printf("[9] Music off\n");
			}
			else if (audio.PlayMusic(keys.music))
				std::printf("[9] Music on: %s (loops)\n", keys.music.c_str());
			else
				std::printf("[9] %s\n", audio.GetLastError().c_str());
		}
		else if (key == '0')
		{
			audio.StopAllSfx();
			std::printf("[0] All sound effects stopped\n");
		}
		else if (key == 'p' || key == 'P')
		{
			if (audio.GetMusicName().empty()) std::printf("[p] No music is on (press 9)\n");
			else if (audio.IsMusicPlaying())
			{
				audio.PauseMusic();
				std::printf("[p] Music paused\n");
			}
			else
			{
				audio.ResumeMusic();
				std::printf("[p] Music resumed\n");
			}
		}
		else if (key == '+' || key == '=' || key == '-' || key == '_')
		{
			const float step = (key == '-' || key == '_') ? -0.1f : 0.1f;
			audio.SetMasterVolume(audio.GetMasterVolume() + step);
			std::printf("[%c] Master volume %.1f\n", key, audio.GetMasterVolume());
		}
		else if (key == 'm' || key == 'M')
		{
			audio.SetMuted(!audio.IsMuted());
			std::printf("[m] %s\n", audio.IsMuted() ? "Muted" : "Unmuted");
		}
		else if (key == 'l' || key == 'L')
			PrintChannels(audio);
		else if (key == 'r' || key == 'R')
		{
			if (audio.LoadManifest(manifest)) std::printf("[r] Re-read %s\n", manifest.c_str());
			else
				std::printf("[r] WARNING: %s\n", audio.GetLastError().c_str());
			keys.FromManifest(audio);
			PrintKeys(audio, keys);
		}
		else if (key == 'c' || key == 'C')
		{
			std::printf("command> ");
			std::fflush(stdout);
			RunCommand(audio, keys, reader.GetLine());
		}
		std::fflush(stdout);
	}

	audio.Shutdown();
	std::printf("End of Music\n");
	return 0;
}
