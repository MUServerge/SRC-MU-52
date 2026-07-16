#include "stdafx.h"
#include "CShaderGL.h"
#include "CShaderScene.h"

#ifdef SHADER_VERSION_TEST
#include "Utilities/Log/muConsoleDebug.h"

CShaderGL::CShaderGL()
	: m_MaxVertexUniformComponents(0)
{
	for (int i = 0; i < eVBO_Max; ++i)
	{
		m_VBOProgram[i] = 0;
		m_VBOBoneCapacity[i] = 0;
	}
}

CShaderGL::~CShaderGL()
{
	Release();
}

void CShaderGL::Release()
{
	bool hasPrograms = false;
	for (int i = 0; i < eVBO_Max; ++i)
		hasPrograms = hasPrograms || (m_VBOProgram[i] != 0);

	if (!hasPrograms)
	{
		m_MaxVertexUniformComponents = 0;
		for (int i = 0; i < eVBO_Max; ++i)
			m_VBOBoneCapacity[i] = 0;
		return;
	}

	const bool canDelete =
		(wglGetCurrentContext() != NULL && glDeleteProgram != NULL);
	const GLuint boundProgram = GetTrackedProgram();
	bool ownsBoundProgram = false;
	for (int i = 0; i < eVBO_Max; ++i)
		ownsBoundProgram = ownsBoundProgram ||
			(m_VBOProgram[i] != 0 && m_VBOProgram[i] == boundProgram);

	if (canDelete && ownsBoundProgram)
		RestoreProgram(0);

	for (int i = 0; i < eVBO_Max; ++i)
	{
		if (m_VBOProgram[i] != 0)
			gShaderScene.ForgetProgram(m_VBOProgram[i]);
		if (canDelete && m_VBOProgram[i] != 0)
			RenderProfilerDeleteProgram(m_VBOProgram[i]);
		m_VBOProgram[i] = 0;
		m_VBOBoneCapacity[i] = 0;
	}

	m_MaxVertexUniformComponents = 0;
}

void CShaderGL::Init()
{
	InitVBOShaders();
}

GLuint CShaderGL::LoadVBOProgram(const char* baseName)
{
	const std::string vertexPath =
		std::string("Data\\Effect\\VBO\\") + baseName + ".vs";
	const std::string fragmentPath =
		std::string("Data\\Effect\\VBO\\") + baseName + ".fs";

	const GLuint program = CShaderScene::BuildProgramFromFiles(
		vertexPath.c_str(), fragmentPath.c_str(), baseName);
	if (program == 0)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] program unavailable for '%s'; legacy mesh fallback remains active",
			baseName);
	}
	return program;
}

int CShaderGL::InspectVBOBoneCapacity(GLuint program, const char* tag) const
{
	if (program == 0 || glGetProgramiv == NULL || glGetActiveUniform == NULL)
		return 0;

	GLint activeUniformCount = 0;
	glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &activeUniformCount);

	for (GLint uniformIndex = 0; uniformIndex < activeUniformCount; ++uniformIndex)
	{
		char uniformName[128] = { 0 };
		GLsizei uniformNameLength = 0;
		GLint uniformSize = 0;
		GLenum uniformType = 0;
		glGetActiveUniform(program, (GLuint)uniformIndex, sizeof(uniformName) - 1,
			&uniformNameLength, &uniformSize, &uniformType, uniformName);
		uniformName[sizeof(uniformName) - 1] = '\0';

		const bool isBoneArray =
			strcmp(uniformName, "u_Bones") == 0 ||
			strncmp(uniformName, "u_Bones[", 8) == 0;
		if (!isBoneArray)
			continue;

		if (uniformType != GL_FLOAT_VEC4 || uniformSize < 3)
		{
			g_ConsoleDebug->Write(5,
				"[VBO Shader] '%s' has incompatible u_Bones type/size (type=0x%X size=%d)",
				tag, uniformType, uniformSize);
			return 0;
		}

		const int requiredComponents = uniformSize * 4;
		if (m_MaxVertexUniformComponents <= 0 || requiredComponents > m_MaxVertexUniformComponents)
		{
			g_ConsoleDebug->Write(5,
				"[VBO Shader] '%s' u_Bones requires %d components; hardware reports %d",
				tag, requiredComponents, m_MaxVertexUniformComponents);
			return 0;
		}

		const int boneCapacity = uniformSize / 3;
		g_ConsoleDebug->Write(5,
			"[VBO Shader] '%s' verified bone capacity: %d bones (vec4 count=%d, HW components=%d)",
			tag, boneCapacity, uniformSize, m_MaxVertexUniformComponents);
		return boneCapacity;
	}

	g_ConsoleDebug->Write(5, "[VBO Shader] '%s' has no active u_Bones array", tag);
	return 0;
}

void CShaderGL::InitVBOShaders()
{
	m_MaxVertexUniformComponents = 0;
	for (int i = 0; i < eVBO_Max; ++i)
	{
		m_VBOProgram[i] = 0;
		m_VBOBoneCapacity[i] = 0;
	}

	if (!GLEW_VERSION_2_0 || glGetActiveUniform == NULL)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] uniform introspection unavailable; GPU skinning disabled, legacy fallback active");
		return;
	}

	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,
		&m_MaxVertexUniformComponents);
	if (m_MaxVertexUniformComponents <= 0)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] invalid GL_MAX_VERTEX_UNIFORM_COMPONENTS=%d; GPU skinning disabled",
			m_MaxVertexUniformComponents);
		return;
	}

	m_VBOProgram[eVBO_Model] = LoadVBOProgram("Model");
	if (m_VBOProgram[eVBO_Model] == 0)
		return;

	m_VBOBoneCapacity[eVBO_Model] =
		InspectVBOBoneCapacity(m_VBOProgram[eVBO_Model], "Model");
	if (m_VBOBoneCapacity[eVBO_Model] > 0)
		return;

	gShaderScene.ForgetProgram(m_VBOProgram[eVBO_Model]);
	RenderProfilerDeleteProgram(m_VBOProgram[eVBO_Model]);
	m_VBOProgram[eVBO_Model] = 0;
}

GLuint CShaderGL::GetVBOProgram(eVBOShader shader) const
{
	if (shader < 0 || shader >= eVBO_Max)
		return 0;
	return m_VBOProgram[shader];
}

int CShaderGL::GetVBOBoneCapacity(eVBOShader shader) const
{
	if (shader < 0 || shader >= eVBO_Max)
		return 0;
	return m_VBOBoneCapacity[shader];
}

GLuint CShaderGL::BindTrackedProgram(GLuint program) const
{
#ifdef SHADER_PIPELINE
	return gShaderScene.BindProgram(program);
#else
	GLint previousProgram = 0;
	g_RenderProfiler.AddCounter(RPC_CURRENT_PROGRAM_QUERIES);
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	RenderProfilerUseProgram(program);
	return (GLuint)previousProgram;
#endif // SHADER_PIPELINE
}

GLuint CShaderGL::GetTrackedProgram() const
{
#ifdef SHADER_PIPELINE
	return gShaderScene.GetTrackedProgram();
#else
	GLint currentProgram = 0;
	g_RenderProfiler.AddCounter(RPC_CURRENT_PROGRAM_QUERIES);
	glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
	return (GLuint)currentProgram;
#endif // SHADER_PIPELINE
}

GLuint CShaderGL::GetBoundVBOProgram() const
{
	const GLuint currentProgram = GetTrackedProgram();
	for (int i = 0; i < eVBO_Max; ++i)
	{
		if (m_VBOProgram[i] != 0 && m_VBOProgram[i] == currentProgram)
			return currentProgram;
	}
	return 0;
}

GLint CShaderGL::GetUniformLocation(GLuint program, const char* name) const
{
#ifdef SHADER_PIPELINE
	return gShaderScene.GetUniformLocation(program, name);
#else
	if (program == 0 || name == NULL || name[0] == '\0' || glGetUniformLocation == NULL)
		return -1;
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	return glGetUniformLocation(program, name);
#endif // SHADER_PIPELINE
}

bool CShaderGL::UseVBO(eVBOShader shader, GLuint* previousProgram)
{
	const GLuint program = GetVBOProgram(shader);
	if (program == 0)
		return false;

	const GLuint previous = BindTrackedProgram(program);
	if (GetTrackedProgram() != program)
		return false;
	if (previousProgram != NULL)
		*previousProgram = previous;
	return true;
}

void CShaderGL::RestoreProgram(GLuint program)
{
	BindTrackedProgram(program);
}

void CShaderGL::vboSetInt(const char* name, int value) const
{
	const GLuint program = GetBoundVBOProgram();
	if (program == 0) return;
	const GLint loc = GetUniformLocation(program, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1i(loc, value);
	}
}

void CShaderGL::vboSetVec4(const char* name, float x, float y, float z, float w) const
{
	const GLuint program = GetBoundVBOProgram();
	if (program == 0) return;
	const GLint loc = GetUniformLocation(program, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform4f(loc, x, y, z, w);
	}
}

void CShaderGL::vboSetVec4Array(const char* name, const float* data, int vec4Count) const
{
	const GLuint program = GetBoundVBOProgram();
	if (program == 0) return;
	const GLint loc = GetUniformLocation(program, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_BONE);
		glUniform4fv(loc, vec4Count, data);
	}
}

void CShaderGL::vboSetMat4(const char* name, const float* m16) const
{
	const GLuint program = GetBoundVBOProgram();
	if (program == 0) return;
	const GLint loc = GetUniformLocation(program, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATRIX);
		glUniformMatrix4fv(loc, 1, GL_FALSE, m16);
	}
}

CShaderGL* CShaderGL::Instance()
{
	static CShaderGL instance;
	return &instance;
}
#endif // SHADER_VERSION_TEST
