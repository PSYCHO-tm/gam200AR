// KopiTwin desktop shell (M1G05 viewer). Creates the window and GL 4.3 core context, turns SDL events into the
// scene's input calls, and owns the fixed-timestep loop. Everything it shows comes from source/ (the shared code).
//
//   Mouse left click   pick equipment (yellow pulse = selected)
//   1 - 9              run the Nth interaction of the selected equipment (e.g. 1 = pour_water on the Kettle)
//   E                  debug: report a fake assessment error on the selected equipment (red for 3 s)
//   Esc                quit
//   --selftest out.ppm no window input: pick the kettle, press its first action, write one frame, exit 0 if all good

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "render/glApi.hpp"
#include "render/kopiScene.hpp"

namespace
{
	constexpr float kTick = 1.0f / 60.0f; // fixed simulation step

	std::optional<std::string> ReadRepoFile(std::string const& path)
	{
		std::ifstream file(std::string(KOPI_REPO_ROOT) + "/" + path, std::ios::binary);
		if (!file.is_open())
		{
			return std::nullopt;
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	void SaveFrame(char const* path, int width, int height)
	{
		std::vector<unsigned char> px(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
		std::ofstream out(path, std::ios::binary);
		out << "P6\n" << width << " " << height << "\n255\n";
		for (int y = height - 1; y >= 0; --y) // GL rows start at the bottom
		{
			for (int x = 0; x < width; ++x)
			{
				out.write(reinterpret_cast<char const*>(&px[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
				                                             static_cast<std::size_t>(x)) * 4]), 3);
			}
		}
	}

	// Pixel where the center of a piece of equipment appears, so the self test can "click" it.
	bool PixelOf(Render::KopiScene const& scene, std::string const& id, int width, int height, float& x, float& y)
	{
		Sim::Mat4 const viewProjection = Sim::Multiply(scene.Projection(), scene.View());
		for (Sim::PickCandidate const& candidate : scene.Engine().BuildCandidates())
		{
			if (candidate.Id == id)
			{
				Sim::Vec3 const mid = (candidate.WorldBounds.Min + candidate.WorldBounds.Max) * 0.5f;
				Sim::Vec3 const ndc = Sim::TransformPoint(viewProjection, mid);
				x                   = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width);
				y                   = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height);
				return true;
			}
		}
		return false;
	}

	// Stand-in for the assessment core (M1G06): print every timed event the engine produced.
	int DrainAndPrint(Render::KopiScene& scene)
	{
		std::vector<Sim::TimedEvent> const events = scene.Engine().DrainEvents();
		for (Sim::TimedEvent const& event : events)
		{
			std::printf("[event] t=%.2f  %s  %s  x%.0f\n", event.Time, event.Object.c_str(), event.Action.c_str(),
			            static_cast<double>(event.Quantity));
		}
		return static_cast<int>(events.size());
	}

	std::string TitleFor(Render::KopiScene const& scene, std::string const& lastMessage)
	{
		std::string title = "KopiTwin | ";
		std::string const& selected = scene.Engine().GetSelected();
		Sim::EquipmentDef const* def = selected.empty() ? nullptr : Sim::FindEquipment(scene.Engine().GetEquipment(), selected);
		if (def == nullptr)
		{
			title += "click a piece of equipment";
		}
		else
		{
			title += def->Name + ":";
			int number = 1;
			for (Sim::InteractionDef const& interaction : def->Interactions)
			{
				title += "  [" + std::to_string(number++) + "] " + interaction.Label;
			}
		}
		if (!lastMessage.empty())
		{
			title += "   >> " + lastMessage;
		}
		return title;
	}
}

int main(int argc, char** argv)
{
	char const* selftestPath = nullptr;
	for (int i = 1; i + 1 < argc; ++i)
	{
		if (std::strcmp(argv[i], "--selftest") == 0)
		{
			selftestPath = argv[i + 1];
		}
	}

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_Window* window = SDL_CreateWindow("KopiTwin", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
	                                      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (window == nullptr)
	{
		std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (context == nullptr)
	{
		std::fprintf(stderr, "GL 4.3 core context failed: %s\n", SDL_GetError());
		return 1;
	}
	if (gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) == 0)
	{
		std::fprintf(stderr, "gladLoadGL failed\n");
		return 1;
	}
	SDL_GL_SetSwapInterval(1);
	std::printf("GL %s | %s\n", reinterpret_cast<char const*>(glGetString(GL_VERSION)),
	            reinterpret_cast<char const*>(glGetString(GL_RENDERER)));

	Render::KopiScene scene;
	std::string       error;
	if (!scene.Init(ReadRepoFile, error))
	{
		std::fprintf(stderr, "scene init failed: %s\n", error.c_str());
		return 1;
	}

	int drawW = 1;
	int drawH = 1;
	SDL_GL_GetDrawableSize(window, &drawW, &drawH);
	scene.Resize(drawW, drawH);

	if (selftestPath != nullptr)
	{
		float x = 0.0f;
		float y = 0.0f;
		bool  ok = PixelOf(scene, "Kettle", drawW, drawH, x, y);
		for (int i = 0; i < 10; ++i)
		{
			scene.Update(kTick);
		}
		std::printf("kettle is at pixel (%.0f, %.0f) of %d x %d\n", static_cast<double>(x), static_cast<double>(y), drawW, drawH);
		ok = ok && scene.Touch(0, x, y) == "selected Kettle";
		ok = ok && scene.Press("serve").find("cannot") != std::string::npos; // kettle cannot serve
		ok = ok && scene.Press("pour_water") == "Kettle: pour_water";
		ok = ok && DrainAndPrint(scene) == 1;
		scene.Update(kTick);
		scene.Draw();
		SaveFrame(selftestPath, drawW, drawH);
		std::printf("selftest %s (glGetError=0x%x)\n", ok ? "PASSED" : "FAILED", glGetError());
		SDL_GL_DeleteContext(context);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return ok ? 0 : 1;
	}

	std::string   lastMessage;
	std::string   shownTitle;
	bool          running     = true;
	Uint64 const  frequency   = SDL_GetPerformanceFrequency();
	Uint64        previous    = SDL_GetPerformanceCounter();
	float         accumulator = 0.0f;

	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				running = false;
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				{
					SDL_GL_GetDrawableSize(window, &drawW, &drawH);
					scene.Resize(drawW, drawH);
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_LEFT)
				{
					// Window units -> drawable pixels (differs on high-DPI screens). Pixel (0,0) is top-left, like the engine.
					int winW = 1;
					int winH = 1;
					SDL_GetWindowSize(window, &winW, &winH);
					float const x = static_cast<float>(event.button.x) * static_cast<float>(drawW) / static_cast<float>(winW);
					float const y = static_cast<float>(event.button.y) * static_cast<float>(drawH) / static_cast<float>(winH);
					lastMessage   = scene.Touch(0, x, y);
				}
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					running = false;
				}
				else if (event.key.keysym.sym >= SDLK_1 && event.key.keysym.sym <= SDLK_9)
				{
					std::string const&       selected = scene.Engine().GetSelected();
					Sim::EquipmentDef const* def =
						selected.empty() ? nullptr : Sim::FindEquipment(scene.Engine().GetEquipment(), selected);
					std::size_t const index = static_cast<std::size_t>(event.key.keysym.sym - SDLK_1);
					lastMessage = (def != nullptr && index < def->Interactions.size())
					                  ? scene.Press(def->Interactions[index].Action)
					                  : "nothing to press";
					DrainAndPrint(scene);
				}
				else if (event.key.keysym.sym == SDLK_e && !scene.Engine().GetSelected().empty())
				{
					scene.Engine().ReportError({scene.Engine().GetSelected(), "wrong_quantity", "debug error"}, 3.0f);
					lastMessage = "debug error on " + scene.Engine().GetSelected();
				}
				break;
			default:
				break;
			}
		}

		// Fixed timestep: Update may run several times per frame, Draw runs once.
		Uint64 const now = SDL_GetPerformanceCounter();
		accumulator += static_cast<float>(static_cast<double>(now - previous) / static_cast<double>(frequency));
		previous = now;
		if (accumulator > 0.25f)
		{
			accumulator = 0.25f; // after a stall, do not try to catch up
		}
		while (accumulator >= kTick)
		{
			scene.Update(kTick);
			accumulator -= kTick;
		}

		scene.Draw();
		SDL_GL_SwapWindow(window);

		std::string const title = TitleFor(scene, lastMessage);
		if (title != shownTitle)
		{
			SDL_SetWindowTitle(window, title.c_str());
			shownTitle = title;
		}
	}

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
