// KopiTwin scene viewer: renders the ECS scene (Assets/Scenes/baseline.json) and plays the animation clips
// (Assets/Animations/*.json) so you can see and tune them. Desktop only, never part of the phone build.
//
// It drives the same Core::Simulation the phone will use: fixed timestep, no wall clock inside Core.
// Buttons call simulation.TriggerAction(action), exactly what the interaction engine will do on a real interaction.
// Edit any JSON under Assets/, save, and the scene reloads (running animations stop on reload, by design).

#include <SDL.h>
#include <SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include "App/DataWatcher.hpp"
#include "App/Simulation.hpp"
#include "Entity Component System/Components.hpp"
#include "GameObject/MeshData.hpp"
#include "Math/Mat4.hpp"

namespace
{
	// ---- Minimal GL function loader (opengl32.dll on Windows only exports GL 1.1) ----
#define GL_FUNCS(X)                                                                                                    \
	X(PFNGLCREATESHADERPROC, CreateShader)                                                                             \
	X(PFNGLSHADERSOURCEPROC, ShaderSource)                                                                             \
	X(PFNGLCOMPILESHADERPROC, CompileShader)                                                                           \
	X(PFNGLGETSHADERIVPROC, GetShaderiv)                                                                               \
	X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog)                                                                     \
	X(PFNGLCREATEPROGRAMPROC, CreateProgram)                                                                           \
	X(PFNGLATTACHSHADERPROC, AttachShader)                                                                             \
	X(PFNGLLINKPROGRAMPROC, LinkProgram)                                                                               \
	X(PFNGLGETPROGRAMIVPROC, GetProgramiv)                                                                             \
	X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog)                                                                   \
	X(PFNGLDELETESHADERPROC, DeleteShader)                                                                             \
	X(PFNGLUSEPROGRAMPROC, UseProgram)                                                                                 \
	X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation)                                                                 \
	X(PFNGLUNIFORMMATRIX4FVPROC, UniformMatrix4fv)                                                                     \
	X(PFNGLUNIFORM3FPROC, Uniform3f)                                                                                   \
	X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays)                                                                       \
	X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays)                                                                 \
	X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray)                                                                       \
	X(PFNGLGENBUFFERSPROC, GenBuffers)                                                                                 \
	X(PFNGLDELETEBUFFERSPROC, DeleteBuffers)                                                                           \
	X(PFNGLBINDBUFFERPROC, BindBuffer)                                                                                 \
	X(PFNGLBUFFERDATAPROC, BufferData)                                                                                 \
	X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray)                                                       \
	X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer)

	struct GlApi
	{
#define X(type, name) type name = nullptr;
		GL_FUNCS(X)
#undef X
	};
	GlApi gGl;

	bool LoadGl()
	{
#define X(type, name)                                                                                                  \
	gGl.name = reinterpret_cast<type>(SDL_GL_GetProcAddress("gl" #name));                                              \
	if (!gGl.name)                                                                                                     \
	{                                                                                                                  \
		std::fprintf(stderr, "Missing GL function gl%s\n", #name);                                                     \
		return false;                                                                                                  \
	}
		GL_FUNCS(X)
#undef X
		return true;
	}

	char const* const gVertexSource = R"(#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat4 uModel;
out vec3 vNormal;
void main()
{
    vNormal = mat3(uModel) * aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
})";

	char const* const gFragmentSource = R"(#version 430 core
in vec3 vNormal;
uniform vec3 uColor;
out vec4 outColor;
void main()
{
    vec3 n = normalize(vNormal);
    float diffuse = max(dot(n, normalize(vec3(0.4, 1.0, 0.6))), 0.0);
    outColor = vec4(uColor * (0.25 + 0.75 * diffuse), 1.0);
})";

	GLuint CompileProgram()
	{
		auto compile = [](GLenum type, char const* source) -> GLuint
		{
			GLuint shader = gGl.CreateShader(type);
			gGl.ShaderSource(shader, 1, &source, nullptr);
			gGl.CompileShader(shader);
			GLint ok = 0;
			gGl.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
			if (!ok)
			{
				char log[1024];
				gGl.GetShaderInfoLog(shader, sizeof(log), nullptr, log);
				std::fprintf(stderr, "Shader compile error: %s\n", log);
				return 0;
			}
			return shader;
		};
		GLuint const vs = compile(GL_VERTEX_SHADER, gVertexSource);
		GLuint const fs = compile(GL_FRAGMENT_SHADER, gFragmentSource);
		if (!vs || !fs)
		{
			return 0;
		}
		GLuint program = gGl.CreateProgram();
		gGl.AttachShader(program, vs);
		gGl.AttachShader(program, fs);
		gGl.LinkProgram(program);
		gGl.DeleteShader(vs);
		gGl.DeleteShader(fs);
		GLint ok = 0;
		gGl.GetProgramiv(program, GL_LINK_STATUS, &ok);
		if (!ok)
		{
			char log[1024];
			gGl.GetProgramInfoLog(program, sizeof(log), nullptr, log);
			std::fprintf(stderr, "Program link error: %s\n", log);
			return 0;
		}
		return program;
	}

	struct GpuMesh
	{
		GLuint  Vao   = 0;
		GLuint  Vbo   = 0;
		GLsizei Count = 0;
	};

	GpuMesh Upload(Core::MeshData const& mesh)
	{
		GpuMesh gpu;
		gGl.GenVertexArrays(1, &gpu.Vao);
		gGl.GenBuffers(1, &gpu.Vbo);
		gGl.BindVertexArray(gpu.Vao);
		gGl.BindBuffer(GL_ARRAY_BUFFER, gpu.Vbo);
		gGl.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.Vertices.size() * sizeof(float)),
		               mesh.Vertices.data(), GL_STATIC_DRAW);
		gGl.EnableVertexAttribArray(0);
		gGl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
		gGl.EnableVertexAttribArray(1);
		gGl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
		                        reinterpret_cast<void const*>(3 * sizeof(float)));
		gGl.BindVertexArray(0);
		gpu.Count = static_cast<GLsizei>(mesh.VertexCount());
		return gpu;
	}

	void Destroy(GpuMesh& mesh)
	{
		if (mesh.Vao)
		{
			gGl.DeleteVertexArrays(1, &mesh.Vao);
			gGl.DeleteBuffers(1, &mesh.Vbo);
		}
		mesh = GpuMesh{};
	}

	// ---- Matrices for ECS transforms (rotation degrees, applied Y then X then Z, as Components.hpp says) ----
	constexpr float gDegToRad = 3.14159265358979f / 180.0f;

	Core::Mat4 RotateXMatrix(float radians)
	{
		Core::Mat4  m;
		float const c = std::cos(radians), s = std::sin(radians);
		m.Values[5]  = c;
		m.Values[6]  = s;
		m.Values[9]  = -s;
		m.Values[10] = c;
		return m;
	}

	Core::Mat4 RotateZMatrix(float radians)
	{
		Core::Mat4  m;
		float const c = std::cos(radians), s = std::sin(radians);
		m.Values[0] = c;
		m.Values[1] = s;
		m.Values[4] = -s;
		m.Values[5] = c;
		return m;
	}

	Core::Mat4 ScaleMatrix(Core::Vec3 s)
	{
		Core::Mat4 m;
		m.Values[0]  = s.x;
		m.Values[5]  = s.y;
		m.Values[10] = s.z;
		return m;
	}

	Core::Vec3 Lerp(Core::Vec3 a, Core::Vec3 b, float t) { return a + (b - a) * t; }

	Core::Mat4 ModelMatrix(Core::Transform const& t, float alpha)
	{
		Core::Vec3 const position = Lerp(t.PrevPosition, t.Position, alpha);
		Core::Vec3 const rotation = Lerp(t.PrevRotation, t.Rotation, alpha);
		Core::Vec3 const scale    = Lerp(t.PrevScale, t.Scale, alpha);
		Core::Mat4 const rot      = Core::Multiply(
            RotateZMatrix(rotation.z * gDegToRad),
            Core::Multiply(RotateXMatrix(rotation.x * gDegToRad), Core::RotateY(rotation.y * gDegToRad)));
		return Core::Multiply(Core::Translate(position), Core::Multiply(rot, ScaleMatrix(scale)));
	}

	// ---- Files ----
	std::string ReadFile(std::filesystem::path const& path)
	{
		std::ifstream     file(path, std::ios::binary);
		std::stringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	// The data folder is the repo's Assets/. Pass it as the first argument, or let the viewer search upward for it.
	std::filesystem::path FindAssets(int argc, char** argv)
	{
		if (argc > 1)
		{
			return argv[1];
		}
		std::filesystem::path dir = std::filesystem::current_path();
		for (int i = 0; i < 8; ++i)
		{
			if (std::filesystem::exists(dir / "Assets" / "Scenes" / "baseline.json"))
			{
				return dir / "Assets";
			}
			if (!dir.has_parent_path() || dir.parent_path() == dir)
			{
				break;
			}
			dir = dir.parent_path();
		}
		return std::filesystem::current_path() / "Assets";
	}

	struct Binding
	{
		std::string Action;
		std::string Clip;
	};

	std::vector<Binding> LoadBindings(std::filesystem::path const& assets)
	{
		std::vector<Binding> result;
		nlohmann::json json = nlohmann::json::parse(ReadFile(assets / "Animations" / "Bindings.json"), nullptr, false);
		if (json.is_object() && json.contains("bindings") && json["bindings"].is_object())
		{
			for (auto const& item : json["bindings"].items())
			{
				if (item.value().is_string())
				{
					result.push_back({item.key(), item.value().get<std::string>()});
				}
			}
		}
		return result;
	}

	// Meshes are cached by the path in MeshRef (relative to Assets/). A mesh that fails to load is remembered once.
	struct MeshCache
	{
		std::map<std::string, GpuMesh>     Meshes;
		std::map<std::string, std::string> Errors;

		GpuMesh const* Get(std::filesystem::path const& assets, std::string const& path)
		{
			auto found = Meshes.find(path);
			if (found != Meshes.end())
			{
				return found->second.Count > 0 ? &found->second : nullptr;
			}
			GpuMesh        gpu;
			Core::MeshData data;
			std::string    error;
			std::string    text = ReadFile(assets / path);
			if (text.empty())
			{
				Errors[path] = "cannot read " + path;
			}
			else if (!Core::ParseObj(text, data, error))
			{
				Errors[path] = path + ": " + error;
			}
			else
			{
				gpu = Upload(data);
			}
			auto inserted = Meshes.emplace(path, gpu);
			return inserted.first->second.Count > 0 ? &inserted.first->second : nullptr;
		}

		void Clear()
		{
			for (auto& entry : Meshes)
			{
				Destroy(entry.second);
			}
			Meshes.clear();
			Errors.clear();
		}
	};
}

int main(int argc, char** argv)
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	SDL_Window* window = SDL_CreateWindow("KopiTwin Scene Viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280,
	                                      720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (!window)
	{
		std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_GLContext gl = SDL_GL_CreateContext(window);
	if (!gl || !LoadGl())
	{
		std::fprintf(stderr, "Could not create an OpenGL 4.3 core context: %s\n", SDL_GetError());
		return 1;
	}
	SDL_GL_SetSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr; // do not drop an imgui.ini into whatever folder we were started from
	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(window, gl);
	ImGui_ImplOpenGL3_Init("#version 430");

	GLuint const program = CompileProgram();
	if (!program)
	{
		return 1;
	}
	GLint const locMvp   = gGl.GetUniformLocation(program, "uMvp");
	GLint const locModel = gGl.GetUniformLocation(program, "uModel");
	GLint const locColor = gGl.GetUniformLocation(program, "uColor");

	std::filesystem::path const assets = FindAssets(argc, argv);
	Core::Simulation            sim;
	bool const                  loaded = sim.TryInitialize(assets.string(), "baseline.json");
	Core::DataWatcher           watcher(assets.string());
	std::vector<Binding>        bindings = LoadBindings(assets);
	MeshCache                   meshes;
	std::string                 message = loaded ? "" : sim.GetLastError();

	float camYaw = 0.6f, camPitch = 0.5f, camDistance = 0.75f;
	float timeScale    = 1.0f;
	bool  paused       = false;
	bool  autoReload   = true;
	int   stepRequests = 0;

	constexpr float fixedDt     = 1.0f / 60.0f;
	float           accumulator = 0.0f;
	float           pollTimer   = 0.0f;
	Uint64          previous    = SDL_GetPerformanceCounter();
	bool            running     = true;

	auto reload = [&]()
	{
		if (sim.TryReload())
		{
			message.clear();
		}
		else
		{
			message = sim.GetLastError(); // the previous scene stays loaded
		}
		meshes.Clear();
		bindings = LoadBindings(assets);
	};
	auto trigger = [&](std::string const& action)
	{
		if (!sim.TriggerAction(action))
		{
			message = sim.GetLastError();
		}
		else
		{
			message.clear();
		}
	};

	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
			{
				running = false;
			}
		}

		Uint64 const now    = SDL_GetPerformanceCounter();
		float const  realDt = std::min(static_cast<float>(static_cast<double>(now - previous) /
		                                                  static_cast<double>(SDL_GetPerformanceFrequency())),
		                               0.1f);
		previous            = now;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		ImGuiIO& io = ImGui::GetIO();

		// ---- Hot reload ----
		pollTimer += realDt;
		if (autoReload && pollTimer > 0.5f)
		{
			pollTimer = 0.0f;
			if (watcher.Poll())
			{
				reload();
			}
		}

		// ---- Keyboard ----
		if (!io.WantTextInput)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
			{
				reload();
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
			{
				paused = !paused;
			}
			for (std::size_t i = 0; i < bindings.size() && i < 9; ++i)
			{
				if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + static_cast<int>(i)), false))
				{
					trigger(bindings[i].Action);
				}
			}
		}

		// ---- Camera ----
		if (!io.WantCaptureMouse)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				camYaw -= io.MouseDelta.x * 0.008f;
				camPitch = std::clamp(camPitch + io.MouseDelta.y * 0.008f, 0.05f, 1.5f);
			}
			camDistance = std::clamp(camDistance - io.MouseWheel * 0.05f, 0.2f, 3.0f);
		}

		// ---- Fixed-timestep simulation (render interpolates between ticks) ----
		Core::InputState const input;
		if (!paused)
		{
			accumulator += realDt * timeScale;
		}
		while (accumulator >= fixedDt)
		{
			sim.Update(fixedDt, input);
			accumulator -= fixedDt;
		}
		while (stepRequests > 0)
		{
			sim.Update(fixedDt, input);
			--stepRequests;
		}
		float const alpha = paused ? 1.0f : accumulator / fixedDt;

		// ---- UI ----
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(380, 600), ImGuiCond_FirstUseEver);
		ImGui::Begin("KopiTwin animation viewer");
		ImGui::TextWrapped("Data: %s", assets.string().c_str());
		if (message.empty())
		{
			ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "OK  (%zu entities)", sim.GetRegistry().EntityCount());
		}
		else
		{
			ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Problem:");
			ImGui::TextWrapped("%s", message.c_str());
		}
		if (ImGui::Button("Reload (F5)"))
		{
			reload();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto-reload on save", &autoReload);

		if (ImGui::CollapsingHeader("Play an interaction animation", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextDisabled("Keys 1-%zu trigger these. Edit Assets/Animations/*.json, save, replay.",
			                    bindings.size());
			for (std::size_t i = 0; i < bindings.size(); ++i)
			{
				std::string const label = std::to_string(i + 1) + "  " + bindings[i].Action;
				if (ImGui::Button(label.c_str()))
				{
					trigger(bindings[i].Action);
				}
				if (sim.GetAnimations().IsPlaying(bindings[i].Clip))
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1, 0.85f, 0.2f, 1), "playing %s", bindings[i].Clip.c_str());
				}
			}
			ImGui::Text("Active clips: %zu", sim.GetAnimations().ActiveCount());
		}

		if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("Speed", &timeScale, 0.05f, 2.0f, "%.2fx");
			if (ImGui::Button(paused ? "Resume (Space)" : "Pause (Space)"))
			{
				paused = !paused;
			}
			ImGui::SameLine();
			if (ImGui::Button("Step 1 tick"))
			{
				paused       = true;
				stepRequests = 1;
			}
			ImGui::Text("Tick %llu (%.2f s of simulation)", static_cast<unsigned long long>(sim.GetTickCount()),
			            static_cast<double>(sim.GetTickCount()) * static_cast<double>(fixedDt));
		}

		if (ImGui::CollapsingHeader("Entities"))
		{
			if (ImGui::BeginTable("entities", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Position");
				ImGui::TableSetupColumn("Rot");
				ImGui::TableSetupColumn("Scale");
				ImGui::TableHeadersRow();
				sim.GetRegistry().ForEach<Core::Name, Core::Transform>(
					[&](Core::Entity, Core::Name const& name, Core::Transform const& t)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(name.Value.c_str());
						ImGui::TableNextColumn();
						ImGui::Text("%.2f %.2f %.2f", static_cast<double>(t.Position.x),
					                static_cast<double>(t.Position.y), static_cast<double>(t.Position.z));
						ImGui::TableNextColumn();
						ImGui::Text("%.0f %.0f %.0f", static_cast<double>(t.Rotation.x),
					                static_cast<double>(t.Rotation.y), static_cast<double>(t.Rotation.z));
						ImGui::TableNextColumn();
						ImGui::Text("%.3f %.3f %.3f", static_cast<double>(t.Scale.x), static_cast<double>(t.Scale.y),
					                static_cast<double>(t.Scale.z));
					});
				ImGui::EndTable();
			}
		}

		if (ImGui::CollapsingHeader("System timings (ms)"))
		{
			for (std::string const& name : sim.GetTimings().Names())
			{
				ImGui::Text("%-20s last %.3f  avg %.3f", name.c_str(), sim.GetTimings().LastMs(name),
				            sim.GetTimings().AvgMs(name));
			}
		}
		for (auto const& problem : meshes.Errors)
		{
			ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Mesh: %s", problem.second.c_str());
		}
		ImGui::TextDisabled("Drag: orbit   Wheel: zoom   %.0f FPS", static_cast<double>(io.Framerate));
		ImGui::End();

		// ---- Render ----
		ImGui::Render();
		Core::Vec3 const target    = {0.0f, 0.04f, 0.0f};
		Core::Vec3 const eye       = {target.x + camDistance * std::cos(camPitch) * std::sin(camYaw),
		                              target.y + camDistance * std::sin(camPitch),
		                              target.z + camDistance * std::cos(camPitch) * std::cos(camYaw)};
		int              drawableW = 0, drawableH = 0;
		SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
		Core::Mat4 const view = Core::LookAt(eye, target, {0, 1, 0});
		Core::Mat4 const proj = Core::Perspective(
			1.0f, static_cast<float>(drawableW) / static_cast<float>(std::max(drawableH, 1)), 0.01f, 20.0f);
		Core::Mat4 const viewProj = Core::Multiply(proj, view);

		glViewport(0, 0, drawableW, drawableH);
		glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		gGl.UseProgram(program);

		sim.GetRegistry().ForEach<Core::Transform, Core::MeshRef, Core::Material>(
			[&](Core::Entity, Core::Transform const& t, Core::MeshRef const& meshRef, Core::Material const& material)
			{
				GpuMesh const* mesh = meshes.Get(assets, meshRef.Path);
				if (!mesh)
				{
					return;
				}
				Core::Mat4 const model = ModelMatrix(t, alpha);
				Core::Mat4 const mvp   = Core::Multiply(viewProj, model);
				gGl.UniformMatrix4fv(locMvp, 1, GL_FALSE, mvp.Values);
				gGl.UniformMatrix4fv(locModel, 1, GL_FALSE, model.Values);
				gGl.Uniform3f(locColor, material.Color.x, material.Color.y, material.Color.z);
				gGl.BindVertexArray(mesh->Vao);
				glDrawArrays(GL_TRIANGLES, 0, mesh->Count);
			});
		gGl.BindVertexArray(0);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}

	meshes.Clear();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	SDL_GL_DeleteContext(gl);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
