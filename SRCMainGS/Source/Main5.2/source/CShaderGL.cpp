#include "stdafx.h"
#include "CShaderGL.h"
#include "CShaderScene.h"

#ifdef SHADER_VERSION_TEST
#include "Utilities/Log/muConsoleDebug.h"

namespace
{
	const int kBoneCapacity = 200;
	const int kVec4PerBone = 3;
	const int kBoneVec4Capacity = kBoneCapacity * kVec4PerBone;
	const int kBonePaletteBytes = kBoneVec4Capacity * 4 * sizeof(float);
	const GLuint kBoneBlockBindingPoint = 0;
	const char* const kBoneBlockName = "BoneBlock";
	const char* const kBoneUboDefine = "MU_USE_BONE_UBO";
}

CShaderGL::CShaderGL()
	: m_MaxVertexUniformComponents(0)
	, m_MaxUniformBlockSize(0)
	, m_BoneUniformBuffer(0)
	, m_BoneTransport(eVBOBoneTransport_None)
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

void CShaderGL::ReleaseUniformBuffer(bool canDelete)
{
	if (m_BoneUniformBuffer == 0)
		return;

	if (canDelete && glDeleteBuffers != NULL)
	{
		if (glBindBufferBase != NULL)
			glBindBufferBase(GL_UNIFORM_BUFFER, kBoneBlockBindingPoint, 0);
		g_RenderProfiler.ResourceDeleted(RPR_BUFFER);
		glDeleteBuffers(1, &m_BoneUniformBuffer);
	}

	m_BoneUniformBuffer = 0;
}

void CShaderGL::Release()
{
	bool hasPrograms = false;
	for (int i = 0; i < eVBO_Max; ++i)
		hasPrograms = hasPrograms || (m_VBOProgram[i] != 0);

	const bool hasResources = hasPrograms || m_BoneUniformBuffer != 0;
	if (!hasResources)
	{
		m_MaxVertexUniformComponents = 0;
		m_MaxUniformBlockSize = 0;
		m_BoneTransport = eVBOBoneTransport_None;
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

	ReleaseUniformBuffer(canDelete);

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
	m_MaxUniformBlockSize = 0;
	m_BoneTransport = eVBOBoneTransport_None;
}

void CShaderGL::Init()
{
	InitVBOShaders();
}

GLuint CShaderGL::LoadVBOProgram(const char* baseName, const char* vertexDefine)
{
	const std::string vertexPath =
		std::string("Data\\Effect\\VBO\\") + baseName + ".vs";
	const std::string fragmentPath =
		std::string("Data\\Effect\\VBO\\") + baseName + ".fs";
	const char* tag = vertexDefine != NULL ? "Model UBO" : "Model UniformArray";

	const GLuint program = CShaderScene::BuildProgramFromFiles(
		vertexPath.c_str(), fragmentPath.c_str(), tag, vertexDefine);
	if (program == 0)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] program unavailable for '%s'; another bone transport or legacy fallback will be tried",
			tag);
	}
	return program;
}

int CShaderGL::InspectUniformArrayBoneCapacity(GLuint program, const char* tag) const
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

		if (uniformType != GL_FLOAT_VEC4 || uniformSize < kVec4PerBone)
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

		const int boneCapacity = uniformSize / kVec4PerBone;
		g_ConsoleDebug->Write(5,
			"[VBO Shader] '%s' verified uniform-array capacity: %d bones",
			tag, boneCapacity);
		return boneCapacity;
	}

	g_ConsoleDebug->Write(5, "[VBO Shader] '%s' has no active u_Bones array", tag);
	return 0;
}

bool CShaderGL::CanUseUniformBuffer() const
{
	const bool capability = GLEW_VERSION_3_1 || GLEW_ARB_uniform_buffer_object;
	return capability &&
		glGetUniformBlockIndex != NULL &&
		glGetActiveUniformBlockiv != NULL &&
		glUniformBlockBinding != NULL &&
		glBindBufferBase != NULL &&
		glGenBuffers != NULL &&
		glBindBuffer != NULL &&
		glBufferData != NULL &&
		glBufferSubData != NULL &&
		glGetBufferParameteriv != NULL &&
		glDeleteBuffers != NULL;
}

bool CShaderGL::ConfigureUniformBuffer(GLuint program, const char* tag)
{
	if (program == 0 || !CanUseUniformBuffer())
		return false;

	GLint maxBindings = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxBindings);
	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &m_MaxUniformBlockSize);
	if (maxBindings <= (GLint)kBoneBlockBindingPoint ||
		m_MaxUniformBlockSize < kBonePaletteBytes)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] '%s' UBO limits insufficient (bindings=%d block=%d required=%d)",
			tag, maxBindings, m_MaxUniformBlockSize, kBonePaletteBytes);
		return false;
	}

	const GLuint blockIndex = glGetUniformBlockIndex(program, kBoneBlockName);
	if (blockIndex == GL_INVALID_INDEX)
	{
		g_ConsoleDebug->Write(5, "[VBO Shader] '%s' has no active %s block", tag, kBoneBlockName);
		return false;
	}

	GLint blockSize = 0;
	glGetActiveUniformBlockiv(program, blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
	if (blockSize < kBonePaletteBytes || blockSize > m_MaxUniformBlockSize)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] '%s' incompatible BoneBlock size=%d required=%d max=%d",
			tag, blockSize, kBonePaletteBytes, m_MaxUniformBlockSize);
		return false;
	}

	GLuint buffer = 0;
	RenderProfilerGenBuffers(1, &buffer);
	if (buffer == 0)
		return false;

	GLint previousBuffer = 0;
	glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previousBuffer);
	RenderProfilerBindBuffer(GL_UNIFORM_BUFFER, buffer);
	glBufferData(GL_UNIFORM_BUFFER, blockSize, NULL, GL_STREAM_DRAW);

	GLint allocatedSize = 0;
	glGetBufferParameteriv(GL_UNIFORM_BUFFER, GL_BUFFER_SIZE, &allocatedSize);
	if (allocatedSize < blockSize)
	{
		RenderProfilerBindBuffer(GL_UNIFORM_BUFFER, (GLuint)previousBuffer);
		g_RenderProfiler.ResourceDeleted(RPR_BUFFER);
		glDeleteBuffers(1, &buffer);
		g_ConsoleDebug->Write(5,
			"[VBO Shader] '%s' failed to allocate BoneBlock buffer (%d/%d bytes)",
			tag, allocatedSize, blockSize);
		return false;
	}

	glUniformBlockBinding(program, blockIndex, kBoneBlockBindingPoint);
	glBindBufferBase(GL_UNIFORM_BUFFER, kBoneBlockBindingPoint, buffer);
	RenderProfilerBindBuffer(GL_UNIFORM_BUFFER, (GLuint)previousBuffer);

	m_BoneUniformBuffer = buffer;
	m_BoneTransport = eVBOBoneTransport_UniformBuffer;
	m_VBOBoneCapacity[eVBO_Model] = kBoneCapacity;
	g_ConsoleDebug->Write(5,
		"[VBO Shader] '%s' selected UBO bone transport: %d bones, %d-byte block, binding %u",
		tag, kBoneCapacity, blockSize, kBoneBlockBindingPoint);
	return true;
}

void CShaderGL::InitVBOShaders()
{
	m_MaxVertexUniformComponents = 0;
	m_MaxUniformBlockSize = 0;
	m_BoneTransport = eVBOBoneTransport_None;
	for (int i = 0; i < eVBO_Max; ++i)
	{
		m_VBOProgram[i] = 0;
		m_VBOBoneCapacity[i] = 0;
	}

	// Prefer the portable std140 block. The same source is recompiled without
	// the define when UBO capability, linking, block introspection or allocation
	// fails, so no duplicate shader asset or manager is introduced.
	if (CanUseUniformBuffer())
	{
		m_VBOProgram[eVBO_Model] = LoadVBOProgram("Model", kBoneUboDefine);
		if (m_VBOProgram[eVBO_Model] != 0 &&
			ConfigureUniformBuffer(m_VBOProgram[eVBO_Model], "Model UBO"))
		{
			return;
		}

		if (m_VBOProgram[eVBO_Model] != 0)
		{
			gShaderScene.ForgetProgram(m_VBOProgram[eVBO_Model]);
			RenderProfilerDeleteProgram(m_VBOProgram[eVBO_Model]);
			m_VBOProgram[eVBO_Model] = 0;
		}
		ReleaseUniformBuffer(true);
	}
	else
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] UBO transport unavailable; trying uniform-array fallback");
	}

	if (!GLEW_VERSION_2_0 || glGetActiveUniform == NULL)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] uniform-array introspection unavailable; legacy mesh fallback active");
		return;
	}

	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,
		&m_MaxVertexUniformComponents);
	if (m_MaxVertexUniformComponents <= 0)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] invalid GL_MAX_VERTEX_UNIFORM_COMPONENTS=%d; legacy mesh fallback active",
			m_MaxVertexUniformComponents);
		return;
	}

	m_VBOProgram[eVBO_Model] = LoadVBOProgram("Model");
	if (m_VBOProgram[eVBO_Model] == 0)
		return;

	m_VBOBoneCapacity[eVBO_Model] =
		InspectUniformArrayBoneCapacity(m_VBOProgram[eVBO_Model], "Model UniformArray");
	if (m_VBOBoneCapacity[eVBO_Model] > 0)
	{
		m_BoneTransport = eVBOBoneTransport_UniformArray;
		return;
	}

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
	if (program == 0 || m_BoneTransport == eVBOBoneTransport_None)
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

bool CShaderGL::UploadBones(const float* data, int boneCount) const
{
	const GLuint program = GetBoundVBOProgram();
	if (program == 0 || data == NULL || boneCount <= 0 ||
		boneCount > m_VBOBoneCapacity[eVBO_Model])
	{
		return false;
	}

	const int vec4Count = boneCount * kVec4PerBone;
	if (m_BoneTransport == eVBOBoneTransport_UniformBuffer)
	{
		if (m_BoneUniformBuffer == 0 || glBufferSubData == NULL)
			return false;

		GLint previousBuffer = 0;
		glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previousBuffer);
		RenderProfilerBindBuffer(GL_UNIFORM_BUFFER, m_BoneUniformBuffer);
		glBufferSubData(GL_UNIFORM_BUFFER, 0,
			vec4Count * 4 * sizeof(float), data);
		RenderProfilerBindBuffer(GL_UNIFORM_BUFFER, (GLuint)previousBuffer);
		g_RenderProfiler.AddCounter(RPC_BONE_PALETTE_UPLOAD_UBO);
		return true;
	}

	if (m_BoneTransport == eVBOBoneTransport_UniformArray)
	{
		const GLint location = GetUniformLocation(program, "u_Bones");
		if (location < 0)
			return false;
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_BONE);
		glUniform4fv(location, vec4Count, data);
		return true;
	}

	return false;
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
