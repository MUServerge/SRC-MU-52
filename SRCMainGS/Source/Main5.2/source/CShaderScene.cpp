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
		"terrain",       // eShaderS_Terrain
		"character",     // eShaderS_Character
		"terrain_core",  // eShaderS_TerrainCore
		"character_core",// eShaderS_CharacterCore
		"effect_core",   // eShaderS_EffectCore
		"ui_core",       // eShaderS_UICore
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
	ClearUniformCache();
}

CShaderScene::~CShaderScene()
{
	Release();
}

std::string CShaderScene::ReadTextFile(const char* path)
{
	if (path == NULL || path[0] == '\0')
		return std::string();

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
		return std::string();

	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
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
		const std::string source = ReadTextFile(paths[i].c_str());
		if (!source.empty())
			return source;
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

GLuint CShaderScene::BuildProgram(const char* vertexSource, const char* fragmentSource, const char* tag)
{
	if (vertexSource == NULL || fragmentSource == NULL || vertexSource[0] == '\0' || fragmentSource[0] == '\0')
		return 0;

	const char* safeTag = (tag != NULL && tag[0] != '\0') ? tag : "unnamed";
	const GLuint vs = CompileShader(GL_VERTEX_SHADER, std::string(vertexSource), safeTag);
	if (vs == 0)
		return 0;

	const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, std::string(fragmentSource), safeTag);
	if (fs == 0)
	{
		RenderProfilerDeleteShader(vs);
		return 0;
	}

	return LinkProgram(vs, fs, safeTag);
}

GLuint CShaderScene::BuildProgramFromFiles(const char* vertexPath, const char* fragmentPath,
	const char* tag, const char* vertexDefine)
{
	std::string vertexSource = ReadTextFile(vertexPath);
	const std::string fragmentSource = ReadTextFile(fragmentPath);
	if (vertexSource.empty() || fragmentSource.empty())
	{
		g_ErrorReport.Write("> [Shader] Failed to load program files (%s, %s)\r\n",
			vertexPath != NULL ? vertexPath : "<null>",
			fragmentPath != NULL ? fragmentPath : "<null>");
		return 0;
	}

	if (vertexDefine != NULL && vertexDefine[0] != '\0')
	{
		// GLSL requires #version to remain the first directive. Insert the one
		// internal capability define after its complete line instead of prepending it.
		const size_t versionOffset = vertexSource.find("#version");
		const size_t versionEnd = versionOffset == std::string::npos
			? std::string::npos
			: vertexSource.find('\n', versionOffset);
		if (versionOffset == std::string::npos || versionEnd == std::string::npos)
		{
			g_ErrorReport.Write("> [Shader] Cannot specialize '%s': missing #version line.\r\n",
				tag != NULL ? tag : "unnamed");
			return 0;
		}

		const std::string defineLine = std::string("#define ") + vertexDefine + " 1\n";
		vertexSource.insert(versionEnd + 1, defineLine);
	}

	return BuildProgram(vertexSource.c_str(), fragmentSource.c_str(), tag);
}

GLuint CShaderScene::LoadProgram(const char* baseName)
{
	const std::string vsSrc = ReadShaderFile((std::string(baseName) + ".vs").c_str());
	const std::string fsSrc = ReadShaderFile((std::string(baseName) + ".fs").c_str());
	if (vsSrc.empty() || fsSrc.empty())
		return 0;

	return BuildProgram(vsSrc.c_str(), fsSrc.c_str(), baseName);
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
	for (int i = 0; i < eShaderS_MaxValue; ++i)
	{
		m_Program[i] = LoadProgram(s_ShaderBaseName[i]);
		if (m_Program[i] == 0)
		{
			allOk = false;
			g_ErrorReport.Write("> [Shader] Technique '%s' unavailable; fixed-function fallback active\r\n",
				s_ShaderBaseName[i]);
		}
		else
		{
			m_Ready = true;
			g_ErrorReport.Write("> [Shader] Loaded '%s' (program %u)\r\n",
				s_ShaderBaseName[i], m_Program[i]);
		}
	}

	g_ErrorReport.Write("> [Shader] Init %s\r\n",
		allOk ? "OK" : "completed with errors (fixed-function fallback active)");

	// Phase 16.2: eShaderS_EffectCore ('effect_core') is loaded and kept the same
	// way, for the upcoming Core effect/sprite/hair draw path. Not bound yet.
	//
	// Phase 15.2: eShaderS_CharacterCore ('character_core') is loaded and kept the
	// same way as terrain_core, for the upcoming Core BMD draw path. It is not
	// bound for rendering yet, so the scene is unchanged.
	//
	// Phase 14.3: eShaderS_TerrainCore ('terrain_core') is now a normally loaded
	// program (handled by the loop above), kept in m_Program for a future draw
	// path. It is not bound for rendering yet, so the scene is unchanged. Its
	// load status is logged like the others; a failure only zeroes its own slot
	// (Use()/GetProgram fall back), leaving terrain/character untouched, and
	// Init()'s return value is not consumed by the caller (Winmain).
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

	const GLuint previousProgram = m_BoundProgram;

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


void CShaderScene::ClearUniformCache()
{
	for (int i = 0; i < PROGRAM_UNIFORM_CACHE_CAPACITY; ++i)
	{
		m_UniformCache[i].Program = 0;
		m_UniformCache[i].Count = 0;
		for (int j = 0; j < UNIFORMS_PER_PROGRAM; ++j)
		{
			m_UniformCache[i].Uniforms[j].Name[0] = '\0';
			m_UniformCache[i].Uniforms[j].Location = -1;
			m_UniformCache[i].Uniforms[j].ValueCount = 0;
		}
	}
}

void CShaderScene::ForgetProgram(GLuint program)
{
	if (program == 0)
		return;

	for (int i = 0; i < PROGRAM_UNIFORM_CACHE_CAPACITY; ++i)
	{
		if (m_UniformCache[i].Program != program)
			continue;

		m_UniformCache[i].Program = 0;
		m_UniformCache[i].Count = 0;
		for (int j = 0; j < UNIFORMS_PER_PROGRAM; ++j)
		{
			m_UniformCache[i].Uniforms[j].Name[0] = '\0';
			m_UniformCache[i].Uniforms[j].Location = -1;
			m_UniformCache[i].Uniforms[j].ValueCount = 0;
		}
		return;
	}
}

GLint CShaderScene::GetUniformLocation(GLuint program, const char* name) const
{
	if (program == 0 || name == NULL || name[0] == '\0' || glGetUniformLocation == NULL)
		return -1;

	ProgramUniformCache* programCache = NULL;
	ProgramUniformCache* emptyCache = NULL;
	for (int i = 0; i < PROGRAM_UNIFORM_CACHE_CAPACITY; ++i)
	{
		if (m_UniformCache[i].Program == program)
		{
			programCache = &m_UniformCache[i];
			break;
		}
		if (emptyCache == NULL && m_UniformCache[i].Program == 0)
			emptyCache = &m_UniformCache[i];
	}

	if (programCache == NULL)
	{
		programCache = emptyCache;
		if (programCache != NULL)
		{
			programCache->Program = program;
			programCache->Count = 0;
		}
	}

	if (programCache != NULL)
	{
		for (int i = 0; i < programCache->Count; ++i)
		{
			if (strcmp(programCache->Uniforms[i].Name, name) == 0)
				return programCache->Uniforms[i].Location;
		}
	}

	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	const GLint location = glGetUniformLocation(program, name);

	const size_t nameLength = strlen(name);
	if (programCache != NULL &&
		programCache->Count < UNIFORMS_PER_PROGRAM &&
		nameLength < UNIFORM_NAME_CAPACITY)
	{
		UniformCacheEntry& entry = programCache->Uniforms[programCache->Count++];
		strcpy_s(entry.Name, UNIFORM_NAME_CAPACITY, name);
		entry.Location = location;
		entry.ValueCount = 0;
	}

	return location;
}

bool CShaderScene::IsUniformValueCached(const char* name, const float* values, int valueCount, GLint& outLocation) const
{
	// Resolving through GetUniformLocation also creates the cache entry, so the
	// value slot below exists from the first upload onwards.
	const GLuint program = m_Program[m_CurrentProgram];
	outLocation = GetUniformLocation(program, name);
	if (outLocation < 0)
		return true; // uniform not present in this program: nothing to upload, ever
	if (values == NULL || valueCount <= 0 || valueCount > 16)
		return false;

	for (int i = 0; i < PROGRAM_UNIFORM_CACHE_CAPACITY; ++i)
	{
		if (m_UniformCache[i].Program != program)
			continue;

		ProgramUniformCache& cache = m_UniformCache[i];
		for (int j = 0; j < cache.Count; ++j)
		{
			if (strcmp(cache.Uniforms[j].Name, name) != 0)
				continue;

			UniformCacheEntry& entry = cache.Uniforms[j];
			if (entry.ValueCount == valueCount &&
				memcmp(entry.Value, values, valueCount * sizeof(float)) == 0)
				return true;

			entry.ValueCount = valueCount;
			memcpy(entry.Value, values, valueCount * sizeof(float));
			return false;
		}
		break;
	}

	return false; // uncacheable (cache full): always upload
}

void CShaderScene::SetInt(const char* name, int value) const
{
	if (m_CurrentProgram < 0 || m_BoundProgram != m_Program[m_CurrentProgram]) return;
	const float cachedValue[1] = { (float)value };
	GLint loc = -1;
	if (IsUniformValueCached(name, cachedValue, 1, loc))
		return;
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1i(loc, value);
	}
}

void CShaderScene::SetFloat(const char* name, float value) const
{
	if (m_CurrentProgram < 0 || m_BoundProgram != m_Program[m_CurrentProgram]) return;
	const float cachedValue[1] = { value };
	GLint loc = -1;
	if (IsUniformValueCached(name, cachedValue, 1, loc))
		return;
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1f(loc, value);
	}
}

void CShaderScene::SetVec3(const char* name, float x, float y, float z) const
{
	if (m_CurrentProgram < 0 || m_BoundProgram != m_Program[m_CurrentProgram]) return;
	const float cachedValue[3] = { x, y, z };
	GLint loc = -1;
	if (IsUniformValueCached(name, cachedValue, 3, loc))
		return;
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform3f(loc, x, y, z);
	}
}

void CShaderScene::SetVec4(const char* name, float x, float y, float z, float w) const
{
	if (m_CurrentProgram < 0 || m_BoundProgram != m_Program[m_CurrentProgram]) return;
	const float cachedValue[4] = { x, y, z, w };
	GLint loc = -1;
	if (IsUniformValueCached(name, cachedValue, 4, loc))
		return;
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform4f(loc, x, y, z, w);
	}
}

void CShaderScene::SetMat4(const char* name, const float* m16) const
{
	if (m_CurrentProgram < 0 || m_BoundProgram != m_Program[m_CurrentProgram]) return;
	GLint loc = -1;
	if (IsUniformValueCached(name, m16, 16, loc))
		return;
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATRIX);
		glUniformMatrix4fv(loc, 1, GL_FALSE, m16);
	}
}

void CShaderScene::SetMat3(const char* name, const float* m9) const
{
	if (m_CurrentProgram < 0 || m_BoundProgram != m_Program[m_CurrentProgram]) return;
	GLint loc = -1;
	if (IsUniformValueCached(name, m9, 9, loc))
		return;
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATRIX);
		glUniformMatrix3fv(loc, 1, GL_FALSE, m9);
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
		if (m_Program[i] != 0)
			ForgetProgram(m_Program[i]);
		if (canDelete && m_Program[i] != 0)
			RenderProfilerDeleteProgram(m_Program[i]);
		m_Program[i] = 0;
	}

	ClearUniformCache();
	m_CurrentProgram = -1;
	if (!canDelete)
		m_BoundProgram = 0;
	m_ProgramStackDepth = 0;
	m_Ready = false;
}
#endif // SHADER_PIPELINE
