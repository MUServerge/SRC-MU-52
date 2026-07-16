#include "stdafx.h"
#include "CShaderScene.h"

#ifdef SHADER_PIPELINE
#include "Utilities/Log/ErrorReport.h"
#include <fstream>
#include <sstream>

CShaderScene gShaderScene;

namespace
{
	// File base names per program, matching the package layout.
	const char* const s_ShaderBaseName[eShaderS_MaxValue] =
	{
		"shader",     // eShaderS_Default
		"terrain",    // eShaderS_Terrain
		"glow",       // eShaderS_Glow
		"character",  // eShaderS_Character
		"colorize",   // eShaderS_Colorize
	};
}

CShaderScene::CShaderScene()
	: m_CurrentProgram(-1)
	, m_BoundProgram(0)
	, m_ProgramStackDepth(0)
	, m_Ready(false)
{
	for (int i = 0; i < eShaderS_MaxValue; ++i)
		m_Program[i] = 0;
	for (int i = 0; i < PROGRAM_STACK_CAPACITY; ++i)
		m_ProgramStack[i] = 0;
}

CShaderScene::~CShaderScene()
{
	Release();
}

std::string CShaderScene::ReadShaderFile(const char* name)
{
	// Try "Shaders/<name>" first, then "Data/Shaders/<name>".
	const std::string paths[2] =
	{
		std::string("Shaders/") + name,
		std::string("Data/Shaders/") + name,
	};

	for (int i = 0; i < 2; ++i)
	{
		std::ifstream file(paths[i].c_str(), std::ios::binary);
		if (!file.is_open())
			continue;

		std::stringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	g_ErrorReport.Write("> [Shader] Failed to open shader file: %s (tried Shaders/ and Data/Shaders/)\r\n", name);
	return std::string();
}

GLuint CShaderScene::CompileShader(GLenum type, const std::string& src, const char* tag)
{
	if (src.empty())
		return 0;

	const GLuint shader = RenderProfilerCreateShader(type);
	const char* srcPtr = src.c_str();
	glShaderSource(shader, 1, &srcPtr, NULL);
	glCompileShader(shader);

	GLint ok = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	g_RenderProfiler.RecordShaderCompile(ok == GL_TRUE);
	if (ok != GL_TRUE)
	{
		char log[1024] = { 0 };
		glGetShaderInfoLog(shader, sizeof(log) - 1, NULL, log);
		g_ErrorReport.Write("> [Shader] Compile error (%s): %s\r\n", tag, log);
		RenderProfilerDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint CShaderScene::LinkProgram(GLuint vs, GLuint fs, const char* tag)
{
	const GLuint program = RenderProfilerCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	// Shaders are no longer needed once linked.
	RenderProfilerDeleteShader(vs);
	RenderProfilerDeleteShader(fs);

	GLint ok = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	g_RenderProfiler.RecordProgramLink(ok == GL_TRUE);
	if (ok != GL_TRUE)
	{
		char log[1024] = { 0 };
		glGetProgramInfoLog(program, sizeof(log) - 1, NULL, log);
		g_ErrorReport.Write("> [Shader] Link error (%s): %s\r\n", tag, log);
		RenderProfilerDeleteProgram(program);
		return 0;
	}
	return program;
}

GLuint CShaderScene::LoadProgram(const char* baseName)
{
	const std::string vsSrc = ReadShaderFile((std::string(baseName) + ".vs").c_str());
	const std::string fsSrc = ReadShaderFile((std::string(baseName) + ".fs").c_str());
	if (vsSrc.empty() || fsSrc.empty())
		return 0;

	const GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc, baseName);
	const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc, baseName);
	if (vs == 0 || fs == 0)
	{
		if (vs) RenderProfilerDeleteShader(vs);
		if (fs) RenderProfilerDeleteShader(fs);
		return 0;
	}

	return LinkProgram(vs, fs, baseName);
}

bool CShaderScene::Init()
{
	// glewInit must have already succeeded.
	if (glCreateShader == NULL || glCreateProgram == NULL)
	{
		g_ErrorReport.Write("> [Shader] GL shader entry points unavailable (GLEW not initialised?).\r\n");
		m_Ready = false;
		return false;
	}

	bool allOk = true;
	bool anyOk = false;
	for (int i = 0; i < eShaderS_MaxValue; ++i)
	{
		m_Program[i] = LoadProgram(s_ShaderBaseName[i]);
		if (m_Program[i] == 0)
			allOk = false;
		else
		{
			anyOk = true;
			g_ErrorReport.Write("> [Shader] Loaded '%s' (program %u)\r\n", s_ShaderBaseName[i], m_Program[i]);
		}
	}

	m_Ready = anyOk;
	g_ErrorReport.Write("> [Shader] Init %s\r\n", allOk ? "OK" : "completed with errors (shaders disabled where missing)");
	return allOk;
}

GLuint CShaderScene::GetProgram(eShaderSProgram program) const
{
	if (program < 0 || program >= eShaderS_MaxValue)
		return 0;
	return m_Program[program];
}

void CShaderScene::SynchronizeSceneProgram(GLuint program)
{
	m_CurrentProgram = -1;
	for (int i = 0; i < eShaderS_MaxValue; ++i)
	{
		if (m_Program[i] == program && program != 0)
		{
			m_CurrentProgram = i;
			break;
		}
	}
}

GLuint CShaderScene::BindProgram(GLuint program)
{
	if (wglGetCurrentContext() == NULL || glUseProgram == NULL)
	{
		m_BoundProgram = 0;
		SynchronizeSceneProgram(0);
		return 0;
	}

	GLint actualProgram = 0;
	g_RenderProfiler.AddCounter(RPC_CURRENT_PROGRAM_QUERIES);
	glGetIntegerv(GL_CURRENT_PROGRAM, &actualProgram);
	const GLuint previousProgram = (GLuint)actualProgram;

	if (previousProgram != program)
		RenderProfilerUseProgram(program);
	else
		g_RenderProfiler.RecordProgramBind(program);

	m_BoundProgram = program;
	SynchronizeSceneProgram(program);
	return previousProgram;
}

bool CShaderScene::Use(eShaderSProgram program)
{
	const GLuint id = GetProgram(program);
	if (id == 0 || m_ProgramStackDepth >= PROGRAM_STACK_CAPACITY)
	{
		if (m_ProgramStackDepth >= PROGRAM_STACK_CAPACITY)
			g_ErrorReport.Write("> [Shader] Program binding stack overflow.\r\n");
		return false;
	}

	const GLuint previousProgram = BindProgram(id);
	if (m_BoundProgram != id)
		return false;

	m_ProgramStack[m_ProgramStackDepth++] = previousProgram;
	return true;
}

void CShaderScene::Unuse()
{
	GLuint previousProgram = 0;
	if (m_ProgramStackDepth > 0)
		previousProgram = m_ProgramStack[--m_ProgramStackDepth];
	BindProgram(previousProgram);
}

void CShaderScene::SetInt(const char* name, int value) const
{
	if (m_CurrentProgram < 0 || m_BoundProgram != m_Program[m_CurrentProgram]) return;
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	const GLint loc = glGetUniformLocation(m_Program[m_CurrentProgram], name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1i(loc, value);
	}
}

void CShaderScene::SetFloat(const char* name, float value) const
{
	if (m_CurrentProgram < 0 || m_BoundProgram != m_Program[m_CurrentProgram]) return;
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	const GLint loc = glGetUniformLocation(m_Program[m_CurrentProgram], name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1f(loc, value);
	}
}

void CShaderScene::SetVec3(const char* name, float x, float y, float z) const
{
	if (m_CurrentProgram < 0 || m_BoundProgram != m_Program[m_CurrentProgram]) return;
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	const GLint loc = glGetUniformLocation(m_Program[m_CurrentProgram], name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform3f(loc, x, y, z);
	}
}

void CShaderScene::Release()
{
	const bool canDelete = (wglGetCurrentContext() != NULL && glDeleteProgram != NULL);
	bool ownsBoundProgram = false;
	for (int i = 0; i < eShaderS_MaxValue; ++i)
		ownsBoundProgram = ownsBoundProgram || (m_Program[i] != 0 && m_Program[i] == m_BoundProgram);

	if (canDelete && ownsBoundProgram)
		BindProgram(0);

	for (int i = 0; i < eShaderS_MaxValue; ++i)
	{
		if (canDelete && m_Program[i] != 0)
			RenderProfilerDeleteProgram(m_Program[i]);
		m_Program[i] = 0;
	}

	m_CurrentProgram = -1;
	if (!canDelete)
		m_BoundProgram = 0;
	m_ProgramStackDepth = 0;
	m_Ready = false;
}
#endif // SHADER_PIPELINE
