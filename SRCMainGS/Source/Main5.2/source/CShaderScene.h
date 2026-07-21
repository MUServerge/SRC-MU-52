#pragma once

// =============================================================================
// CShaderScene - scene-wide GLSL program manager for the (otherwise
// fixed-function) MU client. Ported from the MU Mobile project.
//
//   Shaders/terrain.vs   + terrain.fs     (eShaderS_Terrain)
//   Shaders/character.vs + character.fs   (eShaderS_Character)
//
// Each pair is loaded from "Shaders/<name>" first, then "Data/Shaders/<name>".
//
// The shaders are authored in "#version 330 compatibility" and read the legacy
// fixed-function vertex inputs (gl_Vertex, gl_Color, gl_MultiTexCoord0, ...),
// so they slot into the existing glBegin/glEnd immediate-mode rendering without
// a VAO/VBO rewrite: just Use() a program before the relevant draws, set the
// per-frame uniforms, and Unuse() afterwards. If a program failed to load,
// Use() returns false and binds nothing, so the caller keeps the original
// fixed-function output untouched.
//
// Requires GLEW to be initialised (glewInit) before Init() is called.
// =============================================================================

#include "Defined_Global.h"

#ifdef SHADER_PIPELINE
#include <gl/glew.h>
#include <string>

enum eShaderSProgram
{
	eShaderS_Terrain = 0,
	eShaderS_Character,
	eShaderS_TerrainCore,   // Phase 14: Core-profile terrain (terrain_core.vs/.fs),
	                        // loaded but not bound for drawing yet.
	eShaderS_MaxValue,
};

class CShaderScene
{
public:
	CShaderScene();
	~CShaderScene();

	// Compile and link the two scene techniques with verified draw call sites.
	bool Init();

	// True if at least one program is usable this run.
	bool IsReady() const { return m_Ready; }

	// Bind / unbind a program. Use() returns false (and binds nothing) when the
	// requested program is unavailable, so callers can fall back to fixed-function.
	bool Use(eShaderSProgram program);
	void Unuse();

	// Authoritative binding boundary for every client GLSL owner. BindProgram
	// verifies GL_CURRENT_PROGRAM, binds only when needed, synchronizes the scene
	// technique view, and returns the exact predecessor for nested restoration.
	GLuint BindProgram(GLuint program);
	GLuint GetTrackedProgram() const { return m_BoundProgram; }
	GLuint GetProgram(eShaderSProgram program) const;

	// Shared active-path loader/compiler/linker for every client GLSL adapter.
	// Exact-path loading is used by VBO programs; scene techniques retain their
	// Shaders/ then Data/Shaders/ compatibility search.
	// vertexDefine is inserted immediately after #version. It lets one owned
	// shader source provide capability-selected variants without duplicated files.
	static GLuint BuildProgramFromFiles(const char* vertexPath, const char* fragmentPath,
		const char* tag, const char* vertexDefine = NULL);
	static GLuint BuildProgram(const char* vertexSource, const char* fragmentSource, const char* tag);

	// Uniform locations are immutable after a successful link. Cache both valid
	// and missing locations per program and invalidate them before deletion.
	GLint GetUniformLocation(GLuint program, const char* name) const;
	void ForgetProgram(GLuint program);

	// Uniform setters operate on the currently bound program.
	void SetInt  (const char* name, int value) const;
	void SetFloat(const char* name, float value) const;
	void SetVec3 (const char* name, float x, float y, float z) const;

	void Release();

private:
	GLuint LoadProgram(const char* baseName);
	static GLuint CompileShader(GLenum type, const std::string& src, const char* tag);
	static GLuint LinkProgram(GLuint vs, GLuint fs, const char* tag);
	static std::string ReadTextFile(const char* path);
	static std::string ReadShaderFile(const char* name);
	void SynchronizeSceneProgram(GLuint program);
	void ClearUniformCache();

	static const int PROGRAM_STACK_CAPACITY = 16;
	static const int PROGRAM_UNIFORM_CACHE_CAPACITY = 24;
	static const int UNIFORMS_PER_PROGRAM = 16;
	static const int UNIFORM_NAME_CAPACITY = 48;

	struct UniformCacheEntry
	{
		char Name[UNIFORM_NAME_CAPACITY];
		GLint Location;
	};

	struct ProgramUniformCache
	{
		GLuint Program;
		int Count;
		UniformCacheEntry Uniforms[UNIFORMS_PER_PROGRAM];
	};

	GLuint m_Program[eShaderS_MaxValue];
	GLint  m_CurrentProgram; // bound scene-program enum, -1 for external/legacy
	GLuint m_BoundProgram;   // synchronized actual GL program, including external VBO programs
	GLuint m_ProgramStack[PROGRAM_STACK_CAPACITY];
	int    m_ProgramStackDepth;
	mutable ProgramUniformCache m_UniformCache[PROGRAM_UNIFORM_CACHE_CAPACITY];
	bool   m_Ready;
};

extern CShaderScene gShaderScene;
#endif // SHADER_PIPELINE
