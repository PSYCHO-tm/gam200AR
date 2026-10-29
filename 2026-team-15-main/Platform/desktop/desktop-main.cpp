/* ======================================================================================
Main Author
Name:           Zachary Yee
Course:         IMGD
Digipen ID:     zachary.y
====================================================================================== */
#include <cmath>
#include <iostream>
#include <string>
#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Engine.hpp"

namespace TestRender
{
	// ---------------------------------------------------------------------------
	// Vertex data
	// ---------------------------------------------------------------------------

	static const GLfloat gTriangleVertices[] = {
		0.0f,  0.5f,  // top
		-0.5f, -0.5f, // bottom-left
		0.5f,  -0.5f  // bottom-right
	};

	static const GLfloat gTriangleColors[] = {
		1.0f, 0.0f, 0.0f, // top          - red
		0.0f, 1.0f, 0.0f, // bottom-left  - green
		0.0f, 0.0f, 1.0f  // bottom-right - blue
	};

	// ---------------------------------------------------------------------------
	// Shaders
	// ---------------------------------------------------------------------------

	static const char* vertexShader = R"(
#version 450 core
layout(location = 0) in vec2 VertexPosition;
layout(location = 1) in vec3 VertexColor;
uniform float RadianAngle;
out vec3 InterpColor;
void main()
{
    mat2 r = mat2( cos(RadianAngle), sin(RadianAngle),
                  -sin(RadianAngle), cos(RadianAngle));
    gl_Position = vec4(r * VertexPosition, 0.0, 1.0);
    InterpColor  = VertexColor;
}
)";

	static const char* fragmentShader = R"(
#version 450 core
in  vec3 InterpColor;
out vec4 FragColor;
void main()
{
    FragColor = vec4(InterpColor, 1.0);
}
)";

	// ---------------------------------------------------------------------------
	// GL state
	// ---------------------------------------------------------------------------

	static GLuint programID    = 0;
	static GLuint vao          = 0;
	static GLuint vboPos       = 0;
	static GLuint vboColor     = 0;
	static GLint  uRadianAngle = -1;
	static float  degree       = 0.0f;

	// ---------------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------------

	static void CheckGlError(const char* op)
	{
		for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError())
			std::cerr << "after " << op << "() glError: 0x" << std::hex << err << std::dec << '\n';
	}

	static GLuint CompileShader(GLenum type, const char* src)
	{
		GLuint shader = glCreateShader(type);
		if (!shader) return 0;

		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		GLint ok = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			GLint len = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
			std::string log(static_cast<size_t>(len), '\0');
			glGetShaderInfoLog(shader, len, nullptr, log.data());
			std::cerr << "Shader compile error:\n" << log << '\n';
			glDeleteShader(shader);
			return 0;
		}
		return shader;
	}

	static GLuint CreateProgram(const char* vertSrc, const char* fragSrc)
	{
		GLuint vs = CompileShader(GL_VERTEX_SHADER, vertSrc);
		if (!vs) return 0;
		GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragSrc);
		if (!fs)
		{
			glDeleteShader(vs);
			return 0;
		}

		GLuint prog = glCreateProgram();
		glAttachShader(prog, vs);
		glAttachShader(prog, fs);
		glLinkProgram(prog);

		GLint ok = GL_FALSE;
		glGetProgramiv(prog, GL_LINK_STATUS, &ok);
		if (!ok)
		{
			GLint len = 0;
			glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
			std::string log(static_cast<size_t>(len), '\0');
			glGetProgramInfoLog(prog, len, nullptr, log.data());
			std::cerr << "Program link error:\n" << log << '\n';
			glDeleteProgram(prog);
			prog = 0;
		}

		glDetachShader(prog, vs);
		glDeleteShader(vs);
		glDetachShader(prog, fs);
		glDeleteShader(fs);
		return prog;
	}

	// ---------------------------------------------------------------------------
	// Graphics lifecycle
	// ---------------------------------------------------------------------------

	bool GraphicsInit()
	{
		const GLubyte* version = glGetString(GL_VERSION);
		std::cout << "GL Version = " << (version ? reinterpret_cast<const char*>(version) : "Unknown") << '\n';

		programID = CreateProgram(vertexShader, fragmentShader);
		if (!programID)
		{
			std::cerr << "Could not create shader program.\n";
			return false;
		}

		uRadianAngle = glGetUniformLocation(programID, "RadianAngle");

		// DSA: create VAO and both VBOs without binding
		glCreateVertexArrays(1, &vao);

		// Attribute 0: position (vec2) - binding point 0
		glCreateBuffers(1, &vboPos);
		glNamedBufferStorage(vboPos, sizeof(gTriangleVertices), gTriangleVertices, 0);
		glVertexArrayVertexBuffer(vao, 0, vboPos, 0, sizeof(GLfloat) * 2);
		glEnableVertexArrayAttrib(vao, 0);
		glVertexArrayAttribFormat(vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribBinding(vao, 0, 0);

		// Attribute 1: color (vec3) - binding point 1
		glCreateBuffers(1, &vboColor);
		glNamedBufferStorage(vboColor, sizeof(gTriangleColors), gTriangleColors, 0);
		glVertexArrayVertexBuffer(vao, 1, vboColor, 0, sizeof(GLfloat) * 3);
		glEnableVertexArrayAttrib(vao, 1);
		glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribBinding(vao, 1, 1);

		CheckGlError("GraphicsInit");
		return true;
	}

	void GraphicsResize(int width, int height) { glViewport(0, 0, width, height); }

	constexpr float angularSpeed = 1.0471976f;
	void GraphicsRender(F32 dt)
	{
		static F32 angleRad = 0.0f; // current angle in radians

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(programID);

		//float radian = degree++ / 57.2957795f;
		angleRad += angularSpeed * dt;
		// Keep the angle bounded to avoid float precision loss over long runtimes
		constexpr float twoPi = 6.28318530718f;
		if (angleRad >= twoPi) angleRad -= twoPi;

		glUniform1f(uRadianAngle, angleRad);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindVertexArray(0);
	}

	void GraphicsCleanup()
	{
		if (vboColor) glDeleteBuffers(1, &vboColor);
		if (vboPos) glDeleteBuffers(1, &vboPos);
		if (vao) glDeleteVertexArrays(1, &vao);
		if (programID) glDeleteProgram(programID);
		vboColor = vboPos = vao = programID = 0;
	}
}

namespace TestAudio
{
	void PrintAudio(InputSnapshot const& s)
	{
		auto const& keys = s.Is.Keys.Pressed;

		for (int i = 0; i < 8; ++i)
		{
            if (keys.test(SDL_SCANCODE_1 + i))
                std::cout << '[' << i + 1 << "] Sound effect requested\n";
		}

		if (keys.test(SDL_SCANCODE_0))
            std::cout << "[0] All sound effects stopped\n";
	}
}

int main(int, char**)
{
	SDL_Window* window = nullptr;
	SDL_GLContext glContext = nullptr;
	// Init validation checks
	{
		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			std::cerr << "Failed to initialize SDL: " << SDL_GetError() << '\n';
			return -1;
		}

		// Request GL 4.3 core (the desktop equivalent of the ES 3.0 target)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		window = SDL_CreateWindow("KopiTwin Windows Shell", 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
		if (!window)
		{
			std::cerr << "Failed to create SDL window: " << SDL_GetError() << '\n';
			SDL_Quit();
			return -1;
		}

		glContext = SDL_GL_CreateContext(window);
		if (!glContext)
		{
			std::cerr << "Failed to create GL context: " << SDL_GetError() << '\n';
			SDL_DestroyWindow(window);
			SDL_Quit();
			return -1;
		}
		SDL_GL_MakeCurrent(window, glContext);
		SDL_GL_SetSwapInterval(1); // vsync

		glewExperimental  = GL_TRUE;
		GLenum glewResult = glewInit();
		if (glewResult != GLEW_OK)
		{
			std::cerr << "Failed to initialize GLEW: " << reinterpret_cast<const char*>(glewGetErrorString(glewResult))
					  << '\n';
			SDL_GL_DestroyContext(glContext);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return -1;
		}
		glGetError(); // glewInit can leave a spurious GL_INVALID_ENUM on core profiles

		int w = 0, h = 0;
		SDL_GetWindowSizeInPixels(window, &w, &h);
		TestRender::GraphicsResize(w, h);

		if (!TestRender::GraphicsInit())
		{
			TestRender::GraphicsCleanup();
			SDL_GL_DestroyContext(glContext);
			SDL_DestroyWindow(window);
			SDL_Quit();
			return -1;
		}
	}
	
	// Code reaches here means edge cases checks passed. Windows shell loop begins.
	Core::Init();
	bool running = true;
	while (running)
	{
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
				case SDL_EVENT_QUIT:
					running = false;
					break;
				case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
					TestRender::GraphicsResize(e.window.data1, e.window.data2);
					break;
				case SDL_EVENT_KEY_DOWN:
				case SDL_EVENT_KEY_UP:
					if (e.key.down && e.key.key == SDLK_ESCAPE) running = false;
					Core::mInput.SetKey(static_cast<KeyId>(e.key.scancode), e.key.down);
					break;
				case SDL_EVENT_WINDOW_FOCUS_LOST:
					Core::mInput.Reset();
					break;
				default:
					break;
			}
		}

		Core::Update();
		TestAudio::PrintAudio(Core::mInput.GetSnapshot());
		TestRender::GraphicsRender(Core::dt);
		SDL_GL_SwapWindow(window);
	}

	// Exit code when app terminates.
	TestRender::GraphicsCleanup();
	Core::Exit();
	SDL_GL_DestroyContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
    return 0;
}