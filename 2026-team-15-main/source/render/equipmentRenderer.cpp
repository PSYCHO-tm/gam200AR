#include "render/equipmentRenderer.hpp"

#include <cmath>
#include <cstddef>

#include "render/glApi.hpp"

namespace Render
{
	namespace
	{
		char const* const kVertexSource = KOPI_GLSL_HEADER R"(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat4 uModel;
out vec3 vNormal;
void main()
{
	vNormal     = mat3(uModel) * aNormal; // fine while Scale is uniform, which the JSON allows
	gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

		char const* const kFragmentSource = KOPI_GLSL_HEADER R"(
in vec3 vNormal;
uniform vec3  uColor;
uniform vec3  uTint;
uniform float uTintAmount;
out vec4 oColor;
void main()
{
	vec3 n = normalize(vNormal);
	if (!gl_FrontFacing) n = -n; // placeholder OBJs do not promise a winding order
	float diffuse = max(dot(n, normalize(vec3(0.4, 1.0, 0.6))), 0.0);
	vec3  lit     = uColor * (0.35 + 0.65 * diffuse);
	oColor        = vec4(mix(lit, uTint, uTintAmount), 1.0);
}
)";

		GLuint Build(GLenum type, char const* source, std::string& error)
		{
			GLuint const shader = glCreateShader(type);
			glShaderSource(shader, 1, &source, nullptr);
			glCompileShader(shader);
			GLint ok = GL_FALSE;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
			if (ok == GL_FALSE)
			{
				char log[1024] = {};
				glGetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log);
				error = std::string(type == GL_VERTEX_SHADER ? "vertex" : "fragment") + " shader: " + log;
				glDeleteShader(shader);
				return 0;
			}
			return shader;
		}

		// Per-piece base colors, handed out in library order (placeholder art until the real models exist).
		constexpr float kPalette[3][3] = {{0.90f, 0.55f, 0.25f}, {0.72f, 0.78f, 0.86f}, {0.85f, 0.72f, 0.48f}};
		constexpr float kTableColor[3] = {0.30f, 0.22f, 0.17f};
		constexpr float kSelectTint[3] = {1.00f, 0.85f, 0.20f};
		constexpr float kErrorTint[3]  = {0.95f, 0.15f, 0.15f};
		constexpr float kNoTint[3]     = {0.0f, 0.0f, 0.0f};
	}

	bool EquipmentRenderer::Init(std::string& error)
	{
		Meshes.clear();
		Table   = {};
		Program = 0;

		GLuint const vertex = Build(GL_VERTEX_SHADER, kVertexSource, error);
		if (vertex == 0)
		{
			return false;
		}
		GLuint const fragment = Build(GL_FRAGMENT_SHADER, kFragmentSource, error);
		if (fragment == 0)
		{
			glDeleteShader(vertex);
			return false;
		}
		GLuint const program = glCreateProgram();
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glLinkProgram(program);
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		GLint linked = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE)
		{
			char log[1024] = {};
			glGetProgramInfoLog(program, sizeof(log) - 1, nullptr, log);
			error = std::string("program link: ") + log;
			glDeleteProgram(program);
			return false;
		}
		Program = program;
		UMvp    = glGetUniformLocation(Program, "uMvp");
		UModel  = glGetUniformLocation(Program, "uModel");
		UColor  = glGetUniformLocation(Program, "uColor");
		UTint   = glGetUniformLocation(Program, "uTint");
		UAmount = glGetUniformLocation(Program, "uTintAmount");

		// Table: a 0.7 m square just under the anchor plane so the kit has something to stand on.
		Sim::MeshData table;
		float const   h = 0.35f;
		table.Vertices  = {{{-h, -0.002f, -h}, {0, 1, 0}, {}},
		                   {{h, -0.002f, -h}, {0, 1, 0}, {}},
		                   {{h, -0.002f, h}, {0, 1, 0}, {}},
		                   {{-h, -0.002f, h}, {0, 1, 0}, {}}};
		table.Indices   = {0, 2, 1, 0, 3, 2};
		AddMesh("__table", table);
		auto const found = Meshes.find("__table");
		if (found != Meshes.end())
		{
			Table = found->second;
			Meshes.erase(found);
		}
		return true;
	}

	bool EquipmentRenderer::AddMesh(std::string const& id, Sim::MeshData const& mesh)
	{
		if (Program == 0 || mesh.Vertices.empty() || mesh.Indices.empty())
		{
			return false;
		}
		GpuMesh gpu;
		glGenVertexArrays(1, &gpu.Vao);
		glGenBuffers(1, &gpu.Vbo);
		glGenBuffers(1, &gpu.Ibo);

		glBindVertexArray(gpu.Vao);
		glBindBuffer(GL_ARRAY_BUFFER, gpu.Vbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.Vertices.size() * sizeof(Sim::Vertex)),
		             mesh.Vertices.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Sim::Vertex),
		                      reinterpret_cast<void const*>(offsetof(Sim::Vertex, Pos)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Sim::Vertex),
		                      reinterpret_cast<void const*>(offsetof(Sim::Vertex, Normal)));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.Ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.Indices.size() * sizeof(std::uint32_t)),
		             mesh.Indices.data(), GL_STATIC_DRAW);
		glBindVertexArray(0);

		gpu.IndexCount = static_cast<int>(mesh.Indices.size());
		Meshes[id]     = gpu;
		return true;
	}

	void EquipmentRenderer::DrawMesh(GpuMesh const& mesh, Sim::Mat4 const& model, Sim::Mat4 const& viewProjection,
	                                 float const color[3], float const tint[3], float tintAmount) const
	{
		Sim::Mat4 const mvp = Sim::Multiply(viewProjection, model);
		glUniformMatrix4fv(UMvp, 1, GL_FALSE, mvp.Values);
		glUniformMatrix4fv(UModel, 1, GL_FALSE, model.Values);
		glUniform3fv(UColor, 1, color);
		glUniform3fv(UTint, 1, tint);
		glUniform1f(UAmount, tintAmount);
		glBindVertexArray(mesh.Vao);
		glDrawElements(GL_TRIANGLES, mesh.IndexCount, GL_UNSIGNED_INT, nullptr);
	}

	void EquipmentRenderer::Draw(Sim::InteractionEngine const& engine, Sim::Mat4 const& view,
	                             Sim::Mat4 const& projection) const
	{
		glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (Program == 0)
		{
			return;
		}
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDisable(GL_CULL_FACE);
		glUseProgram(Program);

		Sim::Mat4 const viewProjection = Sim::Multiply(projection, view);
		if (Table.IndexCount > 0)
		{
			DrawMesh(Table, engine.GetAnchorPose(), viewProjection, kTableColor, kNoTint, 0.0f);
		}

		float const pulse    = 0.30f + 0.12f * std::sin(static_cast<float>(engine.GetTime()) * 6.0f);
		std::size_t index    = 0;
		for (Sim::EquipmentDef const& def : engine.GetEquipment().Items)
		{
			float const* const color = kPalette[index++ % 3];
			auto const         found = Meshes.find(def.Id);
			if (found == Meshes.end())
			{
				continue;
			}
			bool hasError = false;
			for (Sim::ActiveError const& active : engine.GetActiveErrors())
			{
				hasError = hasError || active.Error.Object == def.Id;
			}
			float const* tint   = kNoTint;
			float        amount = 0.0f;
			if (hasError)
			{
				tint   = kErrorTint;
				amount = 0.55f;
			}
			else if (engine.GetSelected() == def.Id)
			{
				tint   = kSelectTint;
				amount = pulse;
			}
			DrawMesh(found->second, engine.GetModelMatrix(def.Id), viewProjection, color, tint, amount);
		}
		glBindVertexArray(0);
	}
}
