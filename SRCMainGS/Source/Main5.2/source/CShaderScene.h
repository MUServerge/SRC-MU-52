#pragma once

// =============================================================================
// CShaderScene - scene-wide GLSL program manager for the (otherwise
// fixed-function) MU client. Ported from the MU Mobile project.
//
//   Shaders/shader.vs    + shader.fs      (eShaderS_Default)
//   Shaders/terrain.vs   + terrain.fs     (eShaderS_Terrain)
//   Shaders/glow.vs      + glow.fs        (eShaderS_Glow)
//   Shaders/character.vs + character.fs   (eShaderS_Character)
//   Shaders/colorize.vs  + colorize.fs    (eShaderS_Colorize)
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
	eShaderS_Default = 0,
	eShaderS_Terrain,
	eShaderS_Glow,
	eShaderS_Character,
	eShaderS_Colorize,
	eShaderS_MaxValue,
};

class CShaderScene
{
public:
	CShaderScene();
	~CShaderScene();

	// Compile + link every shader pair. Safe to call once after glewInit().
	// Returns true only if every program linked successfully.
	bool Init();

	// True if at least one program is usable this run.
	bool IsReady() const { return m_Ready; }

	// Bind / unbind a program. Use() returns false (and binds nothing) when the
	// requested program is unavailable, so callers can fall back to fixed-function.
	bool Use(eShaderSProgram program);
	void Unuse();

	GLuint GetProgram(eShaderSProgram program) const;

	// Uniform setters operate on the currently bound program.
	void SetInt  (const char* name, int value) const;
	void SetFloat(const char* name, float value) const;
	void SetVec3 (const char* name, float x, float y, float z) const;

	void Release();

private:
	GLuint LoadProgram(const char* baseName);
	static GLuint CompileShader(GLenum type, const std::string& src, const char* tag);
	static GLuint LinkProgram(GLuint vs, GLuint fs, const char* tag);
	static std::string ReadShaderFile(const char* name);

	GLuint m_Program[eShaderS_MaxValue];
	GLint  m_CurrentProgram; // currently bound program enum, -1 = none
	bool   m_Ready;
};

extern CShaderScene gShaderScene;
#endif // SHADER_PIPELINE
