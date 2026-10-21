#include "EquipmentRenderer.hpp"

#include <GLES3/gl3.h>

#include <cstdio>
#include <string>

#if defined(__ANDROID__)
#	include <android/log.h>
#	define RENDER_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "kopitwin", __VA_ARGS__)
#else
#	define RENDER_LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace KopiInteraction
{
	namespace
	{
		char const* const gVertexSource = R"(#version 300 es
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

		char const* const gFragmentSource = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
uniform vec3 uColor;
uniform vec3 uTint;
uniform float uTintAmount;
out vec4 outColor;
void main()
{
    vec3 n = normalize(vNormal);
    float diffuse = max(dot(n, normalize(vec3(0.4, 1.0, 0.6))), 0.0);
    vec3 lit = uColor * (0.3 + 0.7 * diffuse);
    outColor = vec4(mix(lit, uTint, uTintAmount), 1.0);
})";

		unsigned int Compile(GLenum type, char const* source)
		{
			GLuint shader = glCreateShader(type);
			glShaderSource(shader, 1, &source, nullptr);
			glCompileShader(shader);
			GLint ok = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
			if (!ok)
			{
				char log[1024] = {};
				glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
				RENDER_LOGE("Equipment shader compile failed: %s\n", log);
				glDeleteShader(shader);
				return 0;
			}
			return shader;
		}

		// Equipment has no color in the data yet, so each piece gets one from a small palette.
		void ColorFor(std::size_t index, float color[3])
		{
			static float const palette[3][3] = {{0.92f, 0.88f, 0.70f}, {0.70f, 0.72f, 0.78f}, {0.60f, 0.40f, 0.24f}};
			for (int i = 0; i < 3; ++i)
			{
				color[i] = palette[index % 3][i];
			}
		}
	}

	bool EquipmentRenderer::BuildProgram()
	{
		unsigned int vertex   = Compile(GL_VERTEX_SHADER, gVertexSource);
		unsigned int fragment = Compile(GL_FRAGMENT_SHADER, gFragmentSource);
		if (vertex == 0 || fragment == 0)
		{
			return false;
		}
		Program = glCreateProgram();
		glAttachShader(Program, vertex);
		glAttachShader(Program, fragment);
		glLinkProgram(Program);
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		GLint ok = 0;
		glGetProgramiv(Program, GL_LINK_STATUS, &ok);
		if (!ok)
		{
			char log[1024] = {};
			glGetProgramInfoLog(Program, sizeof(log), nullptr, log);
			RENDER_LOGE("Equipment program link failed: %s\n", log);
			glDeleteProgram(Program);
			Program = 0;
			return false;
		}
		MvpLocation        = glGetUniformLocation(Program, "uMvp");
		ModelLocation      = glGetUniformLocation(Program, "uModel");
		ColorLocation      = glGetUniformLocation(Program, "uColor");
		TintLocation       = glGetUniformLocation(Program, "uTint");
		TintAmountLocation = glGetUniformLocation(Program, "uTintAmount");
		return true;
	}

	void EquipmentRenderer::FreeMeshes()
	{
		for (GpuMesh& mesh : Gpu)
		{
			if (mesh.Vao != 0)
			{
				glDeleteVertexArrays(1, &mesh.Vao);
				glDeleteBuffers(1, &mesh.Vbo);
				glDeleteBuffers(1, &mesh.Ebo);
			}
		}
		Gpu.clear();
	}

	void EquipmentRenderer::Upload(std::vector<Sim::MeshData> const& meshes)
	{
		FreeMeshes();
		for (Sim::MeshData const& mesh : meshes)
		{
			std::vector<float> interleaved;
			interleaved.reserve(mesh.Vertices.size() * 6);
			for (Sim::Vertex const& vertex : mesh.Vertices)
			{
				interleaved.insert(interleaved.end(), {vertex.Pos.x, vertex.Pos.y, vertex.Pos.z, vertex.Normal.x,
				                                       vertex.Normal.y, vertex.Normal.z});
			}

			GpuMesh gpu;
			glGenVertexArrays(1, &gpu.Vao);
			glGenBuffers(1, &gpu.Vbo);
			glGenBuffers(1, &gpu.Ebo);
			glBindVertexArray(gpu.Vao);
			glBindBuffer(GL_ARRAY_BUFFER, gpu.Vbo);
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
			             interleaved.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.Ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.Indices.size() * sizeof(std::uint32_t)),
			             mesh.Indices.data(), GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
			                      reinterpret_cast<void const*>(3 * sizeof(float)));
			glBindVertexArray(0);
			gpu.IndexCount = static_cast<int>(mesh.Indices.size());
			Gpu.push_back(gpu);
		}
	}

	void EquipmentRenderer::ContextLost()
	{
		Program = 0;
		Gpu.clear(); // handles died with the context
		HasUpload = false;
	}

	void EquipmentRenderer::Shutdown()
	{
		FreeMeshes();
		if (Program != 0)
		{
			glDeleteProgram(Program);
		}
		ContextLost();
	}

	void EquipmentRenderer::Draw(Sim::InteractionEngine const& engine, std::vector<Sim::MeshData> const& meshes,
	                             std::uint32_t generation, Sim::Mat4 const& view, Sim::Mat4 const& projection)
	{
		if (Program == 0 && !BuildProgram())
		{
			return;
		}
		if (!HasUpload || generation != UploadedGeneration)
		{
			Upload(meshes);
			UploadedGeneration = generation;
			HasUpload          = true;
		}

		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glUseProgram(Program);
		Sim::Mat4 const viewProjection = Sim::Multiply(projection, view);

		std::vector<Sim::EquipmentDef> const& items = engine.GetEquipment().Items;
		for (std::size_t i = 0; i < items.size() && i < Gpu.size(); ++i)
		{
			Sim::EquipmentDef const& item  = items[i];
			Sim::Mat4 const          model = engine.GetModelMatrix(item.Id);
			Sim::Mat4 const          mvp   = Sim::Multiply(viewProjection, model);

			bool hasError = false;
			for (Sim::ActiveError const& active : engine.GetActiveErrors())
			{
				hasError = hasError || active.Error.Object == item.Id;
			}
			bool const selected = engine.GetSelected() == item.Id;

			float color[3];
			ColorFor(i, color);
			glUniformMatrix4fv(MvpLocation, 1, GL_FALSE, mvp.Values);
			glUniformMatrix4fv(ModelLocation, 1, GL_FALSE, model.Values);
			glUniform3fv(ColorLocation, 1, color);
			if (hasError)
			{
				glUniform3f(TintLocation, 1.0f, 0.2f, 0.2f);
				glUniform1f(TintAmountLocation, 0.55f);
			}
			else
			{
				glUniform3f(TintLocation, 1.0f, 0.85f, 0.2f);
				glUniform1f(TintAmountLocation, selected ? 0.45f : 0.0f);
			}
			glBindVertexArray(Gpu[i].Vao);
			glDrawElements(GL_TRIANGLES, Gpu[i].IndexCount, GL_UNSIGNED_INT, nullptr);
		}
		glBindVertexArray(0);
		glUseProgram(0);
		glDisable(GL_DEPTH_TEST);
	}
}
