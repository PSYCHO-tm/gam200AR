// M1G05 visualizer: a desktop debug tool for the interaction engine.
//
// Shows the kopi equipment around a fake anchor, lets you click to pick (the same ScreenPointToRay + PickNearest
// the phone uses), press the mapped native-style buttons, watch the timed events that would go to the assessment
// core (M1G06), inject errors and see them drawn at the object, and hot-reload Assets/Json/Equipment.json.
//
// On the phone only two things change: the view/projection matrices come from ARCore for the current frame, and
// touches arrive from Android instead of the mouse. Everything under Core/ is shared.

#include <SDL.h>
#include <SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include "GameObject/EquipmentDef.hpp"
#include "GameObject/InteractionEngine.hpp"
#include "GameObject/MeshData.hpp"

namespace
{
	// ---- Minimal GL function loader (core 3.0+ functions are not exported by opengl32.dll on Windows) ----
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
	X(PFNGLUNIFORM1FPROC, Uniform1f)                                                                                   \
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

	// ---- Shader: simple directional light, optional tint and unlit mode for debug lines ----
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
uniform vec3 uTint;
uniform float uTintAmount;
uniform float uUnlit;
out vec4 outColor;
void main()
{
    vec3 n = normalize(vNormal);
    float diffuse = max(dot(n, normalize(vec3(0.4, 1.0, 0.6))), 0.0);
    vec3 lit = uColor * (0.25 + 0.75 * diffuse);
    lit = mix(lit, uTint, uTintAmount);
    outColor = vec4(mix(lit, uColor, uUnlit), 1.0);
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

	// ---- GPU meshes ----
	struct GpuMesh
	{
		GLuint  Vao   = 0;
		GLuint  Vbo   = 0;
		GLsizei Count = 0;
	};

	GpuMesh Upload(float const* data, std::size_t floatCount, GLenum usage)
	{
		GpuMesh mesh;
		gGl.GenVertexArrays(1, &mesh.Vao);
		gGl.GenBuffers(1, &mesh.Vbo);
		gGl.BindVertexArray(mesh.Vao);
		gGl.BindBuffer(GL_ARRAY_BUFFER, mesh.Vbo);
		gGl.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(floatCount * sizeof(float)), data, usage);
		gGl.EnableVertexAttribArray(0);
		gGl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
		gGl.EnableVertexAttribArray(1);
		gGl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
		                        reinterpret_cast<void const*>(3 * sizeof(float)));
		gGl.BindVertexArray(0);
		mesh.Count = static_cast<GLsizei>(floatCount / 6);
		return mesh;
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

	// ---- Scene loading (data -> engine + GPU) ----
	std::string ReadFile(std::filesystem::path const& path)
	{
		std::ifstream     file(path, std::ios::binary);
		std::stringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	struct Scene
	{
		std::vector<GpuMesh> Meshes;
		std::string          Status = "not loaded";
		bool                 Ok     = false;
	};

	// Builds everything into temporaries first, so a bad edit leaves the previous working scene on screen.
	bool LoadScene(std::filesystem::path const& root, Core::InteractionEngine& engine, Scene& scene)
	{
		std::filesystem::path const jsonPath = root / "Assets" / "Json" / "Equipment.json";
		if (!std::filesystem::exists(jsonPath))
		{
			scene.Status = "Cannot find " + jsonPath.string();
			scene.Ok     = false;
			return false;
		}

		std::vector<Core::EquipmentDef> defs;
		std::string                     error;
		if (!Core::LoadEquipmentDefs(ReadFile(jsonPath), defs, error))
		{
			scene.Status = error;
			scene.Ok     = false;
			return false;
		}

		std::vector<Core::MeshData> meshes;
		for (Core::EquipmentDef& def : defs)
		{
			Core::MeshData mesh;
			std::string    meshText = ReadFile(root / def.MeshPath);
			if (meshText.empty())
			{
				scene.Status = "Equipment '" + def.Id + "': cannot read mesh " + def.MeshPath;
				scene.Ok     = false;
				return false;
			}
			if (!Core::ParseObj(meshText, mesh, error))
			{
				scene.Status = "Equipment '" + def.Id + "' (" + def.MeshPath + "): " + error;
				scene.Ok     = false;
				return false;
			}
			def.Bounds = mesh.Bounds;
			meshes.push_back(std::move(mesh));
		}

		for (GpuMesh& old : scene.Meshes)
		{
			Destroy(old);
		}
		scene.Meshes.clear();
		for (Core::MeshData const& mesh : meshes)
		{
			scene.Meshes.push_back(Upload(mesh.Vertices.data(), mesh.Vertices.size(), GL_STATIC_DRAW));
		}
		engine.SetEquipment(std::move(defs));
		scene.Status = "loaded " + std::to_string(scene.Meshes.size()) + " equipment";
		scene.Ok     = true;
		return true;
	}

	std::filesystem::path FindProjectRoot(int argc, char** argv)
	{
		if (argc > 1)
		{
			return argv[1];
		}
		std::filesystem::path dir = std::filesystem::current_path();
		for (int i = 0; i < 6; ++i)
		{
			if (std::filesystem::exists(dir / "Assets" / "Json" / "Equipment.json"))
			{
				return dir;
			}
			if (!dir.has_parent_path() || dir.parent_path() == dir)
			{
				break;
			}
			dir = dir.parent_path();
		}
		return std::filesystem::current_path();
	}

	// ---- Debug drawing ----
	void PushLine(std::vector<float>& out, Core::Vec3 a, Core::Vec3 b)
	{
		out.insert(out.end(), {a.x, a.y, a.z, 0, 1, 0, b.x, b.y, b.z, 0, 1, 0});
	}

	void PushBox(std::vector<float>& out, Core::Aabb const& box)
	{
		Core::Vec3 const c[8]     = {{box.Min.x, box.Min.y, box.Min.z}, {box.Max.x, box.Min.y, box.Min.z},
		                             {box.Max.x, box.Max.y, box.Min.z}, {box.Min.x, box.Max.y, box.Min.z},
		                             {box.Min.x, box.Min.y, box.Max.z}, {box.Max.x, box.Min.y, box.Max.z},
		                             {box.Max.x, box.Max.y, box.Max.z}, {box.Min.x, box.Max.y, box.Max.z}};
		int const        e[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
		                             {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
		for (auto const& edge : e)
		{
			PushLine(out, c[edge[0]], c[edge[1]]);
		}
	}

	bool WorldToScreen(Core::Mat4 const& viewProj, Core::Vec3 p, float width, float height, ImVec2& out)
	{
		float const* v = viewProj.Values;
		float const  x = v[0] * p.x + v[4] * p.y + v[8] * p.z + v[12];
		float const  y = v[1] * p.x + v[5] * p.y + v[9] * p.z + v[13];
		float const  w = v[3] * p.x + v[7] * p.y + v[11] * p.z + v[15];
		if (w <= 1e-5f)
		{
			return false;
		}
		out = ImVec2((x / w * 0.5f + 0.5f) * width, (1.0f - (y / w * 0.5f + 0.5f)) * height);
		return true;
	}
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

	SDL_Window* window =
		SDL_CreateWindow("KopiTwin M1G05 Visualizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
	                     SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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
	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(window, gl);
	ImGui_ImplOpenGL3_Init("#version 430");

	GLuint const program = CompileProgram();
	if (!program)
	{
		return 1;
	}
	GLint const locMvp        = gGl.GetUniformLocation(program, "uMvp");
	GLint const locModel      = gGl.GetUniformLocation(program, "uModel");
	GLint const locColor      = gGl.GetUniformLocation(program, "uColor");
	GLint const locTint       = gGl.GetUniformLocation(program, "uTint");
	GLint const locTintAmount = gGl.GetUniformLocation(program, "uTintAmount");
	GLint const locUnlit      = gGl.GetUniformLocation(program, "uUnlit");

	Core::InteractionEngine     engine;
	Scene                       scene;
	std::filesystem::path const root     = FindProjectRoot(argc, argv);
	std::filesystem::path const jsonPath = root / "Assets" / "Json" / "Equipment.json";
	LoadScene(root, engine, scene);
	std::filesystem::file_time_type lastWrite = std::filesystem::exists(jsonPath)
	                                                ? std::filesystem::last_write_time(jsonPath)
	                                                : std::filesystem::file_time_type{};

	// Orbit camera around the table centre.
	float camYaw = 0.5f, camPitch = 0.55f, camDistance = 0.9f;
	// Fake anchor pose (what ARCore would give us on the phone).
	float anchorPos[3] = {0.0f, 0.0f, 0.0f};
	float anchorYawDeg = 0.0f;

	bool                                autoReload  = true;
	bool                                hasRay      = false;
	float                               reloadTimer = 0.0f;
	std::vector<Core::InteractionEvent> eventLog;
	char                                errorText[96] = "Wrong quantity";
	float                               errorSeconds  = 3.0f;
	int                                 errorTarget   = 0;
	Core::OrderView                     order;
	order.OrderName    = "Kopi";
	order.StepName     = "Pour hot water";
	order.StepIndex    = 1;
	order.StepCount    = 4;
	order.StepTimeLeft = 20.0f;
	engine.SetOrderView(order);

	GpuMesh lineMesh = Upload(nullptr, 0, GL_DYNAMIC_DRAW);
	bool    running  = true;
	Uint64  previous = SDL_GetPerformanceCounter();

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

		Uint64 const now = SDL_GetPerformanceCounter();
		float const  dt  = static_cast<float>(static_cast<double>(now - previous) / SDL_GetPerformanceFrequency());
		previous         = now;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		ImGuiIO& io = ImGui::GetIO();

		// ---- Camera ----
		if (!io.WantCaptureMouse)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
			{
				camYaw -= io.MouseDelta.x * 0.008f;
				camPitch = std::clamp(camPitch + io.MouseDelta.y * 0.008f, 0.05f, 1.5f);
			}
			camDistance = std::clamp(camDistance - io.MouseWheel * 0.06f, 0.25f, 3.0f);
		}
		Core::Vec3 const target   = {0.0f, 0.05f, 0.0f};
		Core::Vec3 const eye      = {target.x + camDistance * std::cos(camPitch) * std::sin(camYaw),
		                             target.y + camDistance * std::sin(camPitch),
		                             target.z + camDistance * std::cos(camPitch) * std::cos(camYaw)};
		float const      width    = io.DisplaySize.x;
		float const      height   = io.DisplaySize.y;
		Core::Mat4 const view     = Core::LookAt(eye, target, {0, 1, 0});
		Core::Mat4 const proj     = Core::Perspective(1.0f, width / std::max(height, 1.0f), 0.01f, 20.0f);
		Core::Mat4 const viewProj = Core::Multiply(proj, view);

		// ---- Hot reload ----
		reloadTimer += dt;
		bool reloadNow = false;
		if (autoReload && reloadTimer > 0.5f)
		{
			reloadTimer = 0.0f;
			std::error_code ec;
			auto const      stamp = std::filesystem::last_write_time(jsonPath, ec);
			if (!ec && stamp != lastWrite)
			{
				lastWrite = stamp;
				reloadNow = true;
			}
		}
		if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
		{
			reloadNow = true;
		}
		if (reloadNow)
		{
			LoadScene(root, engine, scene);
		}

		// ---- Anchor pose ----
		engine.SetAnchorPose(Core::Multiply(Core::Translate({anchorPos[0], anchorPos[1], anchorPos[2]}),
		                                    Core::RotateY(anchorYawDeg * 3.14159265f / 180.0f)));
		engine.Update(dt);

		// ---- Mouse click = touch ----
		if (!io.WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			Core::InteractionInput touch;
			touch.Kind    = Core::InputKind::TOUCH_DOWN;
			touch.ScreenX = io.MousePos.x;
			touch.ScreenY = io.MousePos.y;
			engine.HandleInput(touch, view, proj, width, height);
			hasRay = true;
		}

		// ---- UI ----
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(380, 560), ImGuiCond_FirstUseEver);
		ImGui::Begin("M1G05 Interaction Engine");
		ImGui::TextColored(scene.Ok ? ImVec4(0.5f, 1, 0.5f, 1) : ImVec4(1, 0.4f, 0.4f, 1), "Data: %s",
		                   scene.Status.c_str());
		ImGui::TextWrapped("%s", jsonPath.string().c_str());
		if (ImGui::Button("Reload (F5)"))
		{
			LoadScene(root, engine, scene);
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto-reload on save", &autoReload);
		ImGui::Text("%.1f FPS  |  sim time %.2fs", io.Framerate, engine.GetTime());
		ImGui::TextDisabled("Left click: touch/pick   Right drag: orbit   Wheel: zoom");

		if (ImGui::CollapsingHeader("Fake anchor pose", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat3("Position (m)", anchorPos, -0.5f, 0.5f);
			ImGui::SliderFloat("Yaw (deg)", &anchorYawDeg, -180.0f, 180.0f);
		}

		auto const& equipment = engine.GetEquipment();
		if (ImGui::CollapsingHeader("Equipment", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (std::size_t i = 0; i < equipment.size(); ++i)
			{
				bool const selected = static_cast<int>(i) == engine.GetSelected();
				ImGui::TextColored(selected ? ImVec4(1, 0.85f, 0.2f, 1) : ImVec4(0.8f, 0.8f, 0.8f, 1), "%s %s",
				                   selected ? ">" : " ", equipment[i].Id.c_str());
			}
			ImGui::Separator();
			if (engine.GetSelected() < 0)
			{
				ImGui::TextDisabled("Nothing selected. Click a piece of equipment.");
			}
			else
			{
				Core::EquipmentDef const& def = equipment[static_cast<std::size_t>(engine.GetSelected())];
				ImGui::Text("Selected: %s  (stands in for native Android buttons)", def.Id.c_str());
				for (Core::InteractionDef const& interaction : def.Interactions)
				{
					std::string const label = "[" + interaction.Button + "]  " + interaction.Action + "  " +
					                          std::to_string(static_cast<int>(interaction.Quantity)) + " " +
					                          interaction.Unit;
					if (ImGui::Button(label.c_str()))
					{
						Core::InteractionInput press;
						press.Kind   = Core::InputKind::BUTTON_PRESS;
						press.Button = interaction.Button;
						engine.HandleInput(press, view, proj, width, height);
					}
				}
			}
		}

		for (Core::InteractionEvent& drained : engine.DrainEvents())
		{
			eventLog.push_back(drained);
		}
		if (ImGui::CollapsingHeader("Timed events -> M1G06", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("Clear log"))
			{
				eventLog.clear();
			}
			if (ImGui::BeginTable("events", 4,
			                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
			                      ImVec2(0, 130)))
			{
				ImGui::TableSetupColumn("Time");
				ImGui::TableSetupColumn("Object");
				ImGui::TableSetupColumn("Action");
				ImGui::TableSetupColumn("Qty");
				ImGui::TableHeadersRow();
				for (auto it = eventLog.rbegin(); it != eventLog.rend(); ++it)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%.2f", it->Time);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(it->Object.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(it->Action.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%.0f", it->Quantity);
				}
				ImGui::EndTable();
			}
		}

		if (ImGui::CollapsingHeader("Inject assessment error", ImGuiTreeNodeFlags_DefaultOpen) && !equipment.empty())
		{
			errorTarget = std::clamp(errorTarget, 0, static_cast<int>(equipment.size()) - 1);
			if (ImGui::BeginCombo("Object", equipment[static_cast<std::size_t>(errorTarget)].Id.c_str()))
			{
				for (std::size_t i = 0; i < equipment.size(); ++i)
				{
					if (ImGui::Selectable(equipment[i].Id.c_str(), static_cast<int>(i) == errorTarget))
					{
						errorTarget = static_cast<int>(i);
					}
				}
				ImGui::EndCombo();
			}
			ImGui::InputText("Message", errorText, sizeof(errorText));
			ImGui::SliderFloat("Seconds", &errorSeconds, 0.5f, 10.0f);
			if (ImGui::Button("Show error at object"))
			{
				engine.ShowError(equipment[static_cast<std::size_t>(errorTarget)].Id, errorText, errorSeconds);
			}
		}

		if (ImGui::CollapsingHeader("Current order (-> Android UI via JNI)"))
		{
			Core::OrderView edit = engine.GetOrderView();
			char            orderName[64], stepName[64];
			std::snprintf(orderName, sizeof(orderName), "%s", edit.OrderName.c_str());
			std::snprintf(stepName, sizeof(stepName), "%s", edit.StepName.c_str());
			bool changed = ImGui::InputText("Order", orderName, sizeof(orderName));
			changed |= ImGui::InputText("Step", stepName, sizeof(stepName));
			changed |= ImGui::SliderInt("Step #", &edit.StepIndex, 0, 10);
			changed |= ImGui::SliderFloat("Step time left", &edit.StepTimeLeft, 0.0f, 60.0f);
			if (changed)
			{
				edit.OrderName = orderName;
				edit.StepName  = stepName;
				engine.SetOrderView(edit);
			}
		}
		ImGui::End();

		// World-anchored labels: error text drawn at the object, plus a name tag on the selected item.
		// (On the phone these become FreeType labels from M1G04.)
		ImDrawList* overlay = ImGui::GetBackgroundDrawList();
		auto const& bounds  = engine.GetWorldBounds();
		for (Core::ErrorMarker const& marker : engine.GetErrors())
		{
			for (std::size_t i = 0; i < equipment.size(); ++i)
			{
				if (equipment[i].Id != marker.Object)
				{
					continue;
				}
				Core::Vec3 const top = {(bounds[i].Min.x + bounds[i].Max.x) * 0.5f, bounds[i].Max.y + 0.02f,
				                        (bounds[i].Min.z + bounds[i].Max.z) * 0.5f};
				ImVec2           screen;
				if (WorldToScreen(viewProj, top, width, height, screen))
				{
					ImVec2 const size = ImGui::CalcTextSize(marker.Message.c_str());
					overlay->AddRectFilled(ImVec2(screen.x - size.x * 0.5f - 6, screen.y - size.y - 10),
					                       ImVec2(screen.x + size.x * 0.5f + 6, screen.y - 2),
					                       IM_COL32(190, 30, 30, 230), 4.0f);
					overlay->AddText(ImVec2(screen.x - size.x * 0.5f, screen.y - size.y - 6), IM_COL32_WHITE,
					                 marker.Message.c_str());
				}
			}
		}
		if (engine.GetSelected() >= 0)
		{
			std::size_t const i = static_cast<std::size_t>(engine.GetSelected());
			ImVec2            screen;
			Core::Vec3 const  base = {(bounds[i].Min.x + bounds[i].Max.x) * 0.5f, bounds[i].Min.y,
			                          (bounds[i].Min.z + bounds[i].Max.z) * 0.5f};
			if (WorldToScreen(viewProj, base, width, height, screen))
			{
				overlay->AddText(ImVec2(screen.x - 20, screen.y + 4), IM_COL32(255, 220, 60, 255),
				                 equipment[i].Id.c_str());
			}
		}

		// Order HUD, top-right.
		{
			Core::OrderView const& hud = engine.GetOrderView();
			char                   text[160];
			std::snprintf(text, sizeof(text), "Order: %s   Step %d/%d: %s   %.0fs", hud.OrderName.c_str(),
			              hud.StepIndex, hud.StepCount, hud.StepName.c_str(), hud.StepTimeLeft);
			ImVec2 const size = ImGui::CalcTextSize(text);
			overlay->AddText(ImVec2(width - size.x - 14, 12), IM_COL32_WHITE, text);
		}

		// ---- 3D render ----
		ImGui::Render();
		int drawableW = 0, drawableH = 0;
		SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
		glViewport(0, 0, drawableW, drawableH);
		glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);

		gGl.UseProgram(program);
		gGl.Uniform3f(locTint, 1.0f, 0.85f, 0.2f);
		for (std::size_t i = 0; i < scene.Meshes.size() && i < equipment.size(); ++i)
		{
			Core::Mat4 const model = engine.GetModelMatrix(i);
			Core::Mat4 const mvp   = Core::Multiply(viewProj, model);
			gGl.UniformMatrix4fv(locMvp, 1, GL_FALSE, mvp.Values);
			gGl.UniformMatrix4fv(locModel, 1, GL_FALSE, model.Values);
			gGl.Uniform3f(locColor, equipment[i].Color.x, equipment[i].Color.y, equipment[i].Color.z);
			bool hasError = false;
			for (Core::ErrorMarker const& marker : engine.GetErrors())
			{
				hasError |= marker.Object == equipment[i].Id;
			}
			if (hasError)
			{
				gGl.Uniform3f(locTint, 1.0f, 0.2f, 0.2f);
				gGl.Uniform1f(locTintAmount, 0.55f);
			}
			else
			{
				gGl.Uniform3f(locTint, 1.0f, 0.85f, 0.2f);
				gGl.Uniform1f(locTintAmount, static_cast<int>(i) == engine.GetSelected() ? 0.45f : 0.0f);
			}
			gGl.Uniform1f(locUnlit, 0.0f);
			gGl.BindVertexArray(scene.Meshes[i].Vao);
			glDrawArrays(GL_TRIANGLES, 0, scene.Meshes[i].Count);
		}

		// Debug lines: table grid, anchor axes, world bounds, last pick ray.
		auto drawLines = [&](std::vector<float> const& lines, Core::Vec3 color)
		{
			if (lines.empty())
			{
				return;
			}
			Destroy(lineMesh);
			lineMesh = Upload(lines.data(), lines.size(), GL_DYNAMIC_DRAW);
			Core::Mat4 const identity;
			gGl.UniformMatrix4fv(locMvp, 1, GL_FALSE, viewProj.Values);
			gGl.UniformMatrix4fv(locModel, 1, GL_FALSE, identity.Values);
			gGl.Uniform3f(locColor, color.x, color.y, color.z);
			gGl.Uniform1f(locTintAmount, 0.0f);
			gGl.Uniform1f(locUnlit, 1.0f);
			gGl.BindVertexArray(lineMesh.Vao);
			glDrawArrays(GL_LINES, 0, lineMesh.Count);
		};

		std::vector<float> grid;
		for (int i = -5; i <= 5; ++i)
		{
			float const t = static_cast<float>(i) * 0.1f;
			PushLine(grid, {t, 0, -0.5f}, {t, 0, 0.5f});
			PushLine(grid, {-0.5f, 0, t}, {0.5f, 0, t});
		}
		drawLines(grid, {0.28f, 0.30f, 0.34f});

		std::vector<float> boxes;
		std::vector<float> selectedBox;
		for (std::size_t i = 0; i < bounds.size(); ++i)
		{
			PushBox(static_cast<int>(i) == engine.GetSelected() ? selectedBox : boxes, bounds[i]);
		}
		drawLines(boxes, {0.35f, 0.75f, 0.95f});
		drawLines(selectedBox, {1.0f, 0.85f, 0.2f});

		Core::Vec3 const   anchorOrigin = Core::TransformPoint(engine.GetAnchorPose(), {0, 0, 0});
		std::vector<float> axisX, axisY, axisZ;
		PushLine(axisX, anchorOrigin, Core::TransformPoint(engine.GetAnchorPose(), {0.1f, 0, 0}));
		PushLine(axisY, anchorOrigin, Core::TransformPoint(engine.GetAnchorPose(), {0, 0.1f, 0}));
		PushLine(axisZ, anchorOrigin, Core::TransformPoint(engine.GetAnchorPose(), {0, 0, 0.1f}));
		drawLines(axisX, {1, 0.2f, 0.2f});
		drawLines(axisY, {0.2f, 1, 0.2f});
		drawLines(axisZ, {0.3f, 0.5f, 1});

		if (hasRay)
		{
			Core::Ray const    ray = engine.GetLastRay();
			std::vector<float> rayLine;
			PushLine(rayLine, ray.Origin, ray.Origin + ray.Direction * 2.0f);
			drawLines(rayLine, {1, 0.4f, 0.9f});
		}
		gGl.BindVertexArray(0);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}

	for (GpuMesh& mesh : scene.Meshes)
	{
		Destroy(mesh);
	}
	Destroy(lineMesh);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	SDL_GL_DeleteContext(gl);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
