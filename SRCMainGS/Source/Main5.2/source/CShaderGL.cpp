#include "stdafx.h"
#include "CShaderGL.h"
#include "CShaderScene.h"

#ifdef SHADER_VERSION_TEST
#include "Utilities/Log/muConsoleDebug.h"
#include "Utilities/Log/ErrorReport.h"

namespace
{
	const int kBoneCapacity = 200;
	const int kVec4PerBone = 3;
	const int kBoneVec4Capacity = kBoneCapacity * kVec4PerBone;
	const int kBonePaletteBytes = kBoneVec4Capacity * 4 * sizeof(float);
	const GLuint kBoneBlockBindingPoint = 0;
	const char* const kBoneBlockName = "BoneBlock";
	const char* const kBoneUboDefine = "MU_USE_BONE_UBO";
	const GLuint kBoneStorageBindingPoint = 1;
	const char* const kBoneStorageBlockName = "BoneStorage";
	const char* const kBoneSsboDefine = "MU_USE_BONE_SSBO";
	const int kBoneStoragePaletteCapacity = 512;
	const int kBoneStorageVec4Capacity = kBoneStoragePaletteCapacity * kBoneVec4Capacity;
	const int kBoneStorageBytes = kBoneStorageVec4Capacity * 4 * sizeof(float);
	const GLuint kBlessOriginalBoneStorageBindingPoint = 0;
	const char* const kBlessOriginalBoneStorageBlockName = "BoneMatricesBuffer";
	const int kBlessOriginalBoneMatrixFloatCount = 16;
	const int kBlessOriginalBoneStorageBytes =
		kBoneCapacity * kBlessOriginalBoneMatrixFloatCount * sizeof(float);
}

CShaderGL::CShaderGL()
	: m_MaxVertexUniformComponents(0)
	, m_MaxUniformBlockSize(0)
	, m_BoneUniformBuffer(0)
	, m_BoneStorageBuffer(0)
	, m_BoneStorageCursorVec4(0)
	, m_BoneTransport(eVBOBoneTransport_None)
	, m_TranslatedVboEnabled(false)
	, m_BlessModelShaderEnabled(false)
	, m_BlessOriginalModelSyncEnabled(false)
{
	for (int i = 0; i < eVBO_Max; ++i)
	{
		m_VBOProgram[i] = 0;
		m_VBOBoneCapacity[i] = 0;
	}
}

void CShaderGL::ReleaseShaderStorageBuffer(bool canDelete)
{
	if (m_BoneStorageBuffer == 0)
		return;

	if (canDelete && glDeleteBuffers != NULL)
	{
		if (glBindBufferBase != NULL)
		{
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, kBoneStorageBindingPoint, 0);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, kBlessOriginalBoneStorageBindingPoint, 0);
		}
		g_RenderProfiler.ResourceDeleted(RPR_BUFFER);
		glDeleteBuffers(1, &m_BoneStorageBuffer);
	}

	m_BoneStorageBuffer = 0;
	m_BoneStorageCursorVec4 = 0;
}

CShaderGL::~CShaderGL()
{
	Release();
}

bool IsTranslatedVBOEnabled()
{
	return gShaderGL->IsTranslatedVBOEnabled();
}

bool IsBlessOriginalModelSyncEnabled()
{
	return gShaderGL->IsBlessOriginalModelSyncEnabled();
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

	const bool hasResources = hasPrograms || m_BoneUniformBuffer != 0 || m_BoneStorageBuffer != 0;
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
	ReleaseShaderStorageBuffer(canDelete);

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
	const char* commandLine = GetCommandLineA();
	m_TranslatedVboEnabled =
		commandLine != NULL && strstr(commandLine, "-vbotranslate") != NULL;

	char blessShaderOption[8] = { 0 };
	const DWORD blessShaderOptionLength = GetEnvironmentVariableA(
		"MU_BLESS_MODEL_SHADER", blessShaderOption, sizeof(blessShaderOption));
	m_BlessModelShaderEnabled =
		(commandLine != NULL && strstr(commandLine, "-blessmodelshader") != NULL) ||
		(blessShaderOptionLength > 0 && blessShaderOptionLength < sizeof(blessShaderOption) &&
			 strcmp(blessShaderOption, "1") == 0);
	m_BlessOriginalModelSyncEnabled =
		commandLine != NULL && strstr(commandLine, "-blessoriginalmodelsync") != NULL;
	if (m_BlessOriginalModelSyncEnabled)
		m_BlessModelShaderEnabled = false;

	InitVBOShaders();

	const char* transport = "none (legacy mesh fallback)";
	if (m_BoneTransport == eVBOBoneTransport_UniformArray)
		transport = "uniform array";
	else if (m_BoneTransport == eVBOBoneTransport_UniformBuffer)
		transport = "uniform buffer (UBO)";
	else if (m_BoneTransport == eVBOBoneTransport_ShaderStorageBuffer)
		transport = "shared shader storage buffer (SSBO)";

	g_ErrorReport.Write("<Renderer VBO model program>\r\n");
	g_ErrorReport.Write("Model program\t\t: %u\r\n", m_VBOProgram[eVBO_Model]);
	g_ErrorReport.Write("Bone transport\t\t: %s\r\n", transport);
	g_ErrorReport.Write("Bone capacity\t\t: %d\r\n", m_VBOBoneCapacity[eVBO_Model]);
	g_ErrorReport.Write("GPU skinning ready\t: %s\r\n", IsReadyVBO() ? "yes" : "no");
	g_ErrorReport.Write("Translated plain VBO\t: %s\r\n",
		m_TranslatedVboEnabled ? "enabled (opt-in)" : "disabled (legacy default)");
	g_ErrorReport.Write("Bless model shader\t: %s\r\n",
		m_BlessModelShaderEnabled ? "enabled (experimental)" : "disabled (legacy default)");
	g_ErrorReport.Write("Bless original model_sync\t: %s\r\n",
		m_BlessOriginalModelSyncEnabled ? "enabled (experimental)" : "disabled (legacy default)");
	g_RenderProfiler.WriteRendererDiagnostic("Model_program: %u\r\n", m_VBOProgram[eVBO_Model]);
	g_RenderProfiler.WriteRendererDiagnostic("Bone_transport: %s\r\n", transport);
	g_RenderProfiler.WriteRendererDiagnostic("Bless_model_shader: %s\r\n",
		m_BlessModelShaderEnabled ? "enabled (experimental)" : "disabled (legacy default)");
	g_RenderProfiler.WriteRendererDiagnostic("Bless_original_model_sync: %s\r\n",
		m_BlessOriginalModelSyncEnabled ? "enabled (experimental)" : "disabled (legacy default)");
	g_ErrorReport.AddSeparator();
}

GLuint CShaderGL::LoadVBOProgram(const char* baseName, const char* vertexDefine)
{
	if (m_BlessOriginalModelSyncEnabled && strcmp(baseName, "Model") == 0)
	{
		return CShaderScene::BuildProgramFromFiles(
			"Bless Shader\\model_sync.vert", "Bless Shader\\model_sync.frag",
			"Bless original model_sync");
	}
	const char* selectedBaseName = baseName;
	if (m_BlessModelShaderEnabled && strcmp(baseName, "Model") == 0)
		selectedBaseName = "BlessModel";

	const std::string vertexPath =
		std::string("Data\\Effect\\VBO\\") + selectedBaseName + ".vs";
	const std::string fragmentPath =
		std::string("Data\\Effect\\VBO\\") + selectedBaseName + ".fs";
	const char* tag = m_BlessModelShaderEnabled ? "BlessModel experimental" :
		(vertexDefine != NULL ? "Model UBO" : "Model UniformArray");

	GLuint program = CShaderScene::BuildProgramFromFiles(
		vertexPath.c_str(), fragmentPath.c_str(), tag, vertexDefine);
	if (program == 0 && m_BlessModelShaderEnabled && strcmp(baseName, "Model") == 0)
	{
		g_RenderProfiler.WriteRendererDiagnostic("BlessModel: unavailable; established Model fallback requested\r\n");
		g_ConsoleDebug->Write(5,
			"[VBO Shader] experimental BlessModel unavailable; retrying established Model program");
		const std::string fallbackVertexPath =
			std::string("Data\\Effect\\VBO\\") + baseName + ".vs";
		const std::string fallbackFragmentPath =
			std::string("Data\\Effect\\VBO\\") + baseName + ".fs";
		program = CShaderScene::BuildProgramFromFiles(
			fallbackVertexPath.c_str(), fallbackFragmentPath.c_str(), "Model fallback", vertexDefine);
	}
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

bool CShaderGL::ConfigureShaderStorageBuffer(GLuint program, const char* tag)
{
	const bool capability = (GLEW_VERSION_4_3 || GLEW_ARB_shader_storage_buffer_object) &&
		glGetProgramResourceIndex != NULL && glShaderStorageBlockBinding != NULL &&
		glBindBufferBase != NULL && glGenBuffers != NULL && glBindBuffer != NULL &&
		glBufferData != NULL && glGetBufferParameteriv != NULL && glDeleteBuffers != NULL;
	if (program == 0 || !capability)
		return false;

	const GLuint blockIndex = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,
		kBoneStorageBlockName);
	if (blockIndex == GL_INVALID_INDEX)
		return false;

	GLuint buffer = 0;
	RenderProfilerGenBuffers(1, &buffer);
	if (buffer == 0)
		return false;

	GLint previousBuffer = 0;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &previousBuffer);
	RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, kBoneStorageBytes, NULL, GL_STREAM_DRAW);
	GLint allocatedSize = 0;
	glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &allocatedSize);
	if (allocatedSize < kBoneStorageBytes)
	{
		RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)previousBuffer);
		g_RenderProfiler.ResourceDeleted(RPR_BUFFER);
		glDeleteBuffers(1, &buffer);
		return false;
	}

	glShaderStorageBlockBinding(program, blockIndex, kBoneStorageBindingPoint);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, kBoneStorageBindingPoint, buffer);
	RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)previousBuffer);
	m_BoneStorageBuffer = buffer;
	m_BoneStorageCursorVec4 = 0;
	m_BoneTransport = eVBOBoneTransport_ShaderStorageBuffer;
	m_VBOBoneCapacity[eVBO_Model] = kBoneCapacity;
	g_ConsoleDebug->Write(5, "[VBO Shader] '%s' selected SSBO bone transport: %d palettes, binding %u",
		tag, kBoneStoragePaletteCapacity, kBoneStorageBindingPoint);
	return true;
}

bool CShaderGL::ConfigureBlessOriginalBoneStorageBuffer(GLuint program, const char* tag)
{
	const bool capability = (GLEW_VERSION_4_3 || GLEW_ARB_shader_storage_buffer_object) &&
		glGetProgramResourceIndex != NULL && glBindBufferBase != NULL &&
		glGenBuffers != NULL && glBindBuffer != NULL && glBufferData != NULL &&
		glGetBufferParameteriv != NULL && glDeleteBuffers != NULL;
	if (program == 0 || !capability)
		return false;

	const GLuint blockIndex = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,
		kBlessOriginalBoneStorageBlockName);
	if (blockIndex == GL_INVALID_INDEX)
		return false;

	GLuint buffer = 0;
	RenderProfilerGenBuffers(1, &buffer);
	if (buffer == 0)
		return false;

	GLint previousBuffer = 0;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &previousBuffer);
	RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, kBlessOriginalBoneStorageBytes, NULL, GL_STREAM_DRAW);
	GLint allocatedSize = 0;
	glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &allocatedSize);
	if (allocatedSize < kBlessOriginalBoneStorageBytes)
	{
		RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)previousBuffer);
		g_RenderProfiler.ResourceDeleted(RPR_BUFFER);
		glDeleteBuffers(1, &buffer);
		return false;
	}

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, kBlessOriginalBoneStorageBindingPoint, buffer);
	RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)previousBuffer);
	m_BoneStorageBuffer = buffer;
	m_BoneStorageCursorVec4 = 0;
	m_BoneTransport = eVBOBoneTransport_ShaderStorageBuffer;
	m_VBOBoneCapacity[eVBO_Model] = kBoneCapacity;
	g_ConsoleDebug->Write(5,
		"[VBO Shader] '%s' selected original std430 bone storage: %d bones, binding %u",
		tag, kBoneCapacity, kBlessOriginalBoneStorageBindingPoint);
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

	char requestedTransport[16] = { 0 };
	const DWORD requestedLength = GetEnvironmentVariableA(
		"MU_BONE_TRANSPORT", requestedTransport, sizeof(requestedTransport));
	const bool hasTransportOverride =
		requestedLength > 0 && requestedLength < sizeof(requestedTransport);
	const bool forceLegacy = hasTransportOverride &&
		_stricmp(requestedTransport, "legacy") == 0;
	const bool forceUniformArray = hasTransportOverride &&
		_stricmp(requestedTransport, "uniform") == 0;
	const bool forceUniformBuffer = hasTransportOverride &&
		_stricmp(requestedTransport, "ubo") == 0;
	const bool forceShaderStorage = hasTransportOverride &&
		_stricmp(requestedTransport, "ssbo") == 0;

	if (forceLegacy)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] MU_BONE_TRANSPORT=legacy; GPU BMD draw disabled for validation");
		return;
	}

	if (m_BlessOriginalModelSyncEnabled)
	{
		m_VBOProgram[eVBO_Model] = LoadVBOProgram("Model");
		if (m_VBOProgram[eVBO_Model] != 0 &&
			ConfigureBlessOriginalBoneStorageBuffer(m_VBOProgram[eVBO_Model], "Bless original model_sync"))
			return;

		if (m_VBOProgram[eVBO_Model] != 0)
		{
			gShaderScene.ForgetProgram(m_VBOProgram[eVBO_Model]);
			RenderProfilerDeleteProgram(m_VBOProgram[eVBO_Model]);
			m_VBOProgram[eVBO_Model] = 0;
		}
		ReleaseShaderStorageBuffer(true);
		m_BlessOriginalModelSyncEnabled = false;
		g_RenderProfiler.WriteRendererDiagnostic(
			"Bless_original_model_sync: unavailable; established Model fallback requested\r\n");
	}

	if (!forceUniformArray && (m_BlessModelShaderEnabled || forceShaderStorage))
	{
		m_VBOProgram[eVBO_Model] = LoadVBOProgram("Model", kBoneSsboDefine);
		if (m_VBOProgram[eVBO_Model] != 0 &&
			ConfigureShaderStorageBuffer(m_VBOProgram[eVBO_Model], "BlessModel SSBO"))
			return;
		if (m_VBOProgram[eVBO_Model] != 0)
		{
			gShaderScene.ForgetProgram(m_VBOProgram[eVBO_Model]);
			RenderProfilerDeleteProgram(m_VBOProgram[eVBO_Model]);
			m_VBOProgram[eVBO_Model] = 0;
		}
		ReleaseShaderStorageBuffer(true);
	}

	// Prefer the portable std140 block. The same source is recompiled without
	// the define when UBO capability, linking, block introspection or allocation
	// fails, so no duplicate shader asset or manager is introduced.
	if (!forceUniformArray && CanUseUniformBuffer())
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
			"[VBO Shader] UBO transport unavailable or bypassed; trying uniform-array fallback");
	}

	if (forceUniformBuffer || forceShaderStorage)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] MU_BONE_TRANSPORT=ubo failed; legacy mesh fallback active");
		return;
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
		g_ConsoleDebug->Write(5,
			"[VBO Shader] selected uniform-array bone transport fallback");
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

void CShaderGL::BeginBoneFrame()
{
	if (m_BoneTransport != eVBOBoneTransport_ShaderStorageBuffer || m_BoneStorageBuffer == 0)
		return;
	if (m_BlessOriginalModelSyncEnabled)
		return;

	GLint previousBuffer = 0;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &previousBuffer);
	RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BoneStorageBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, kBoneStorageBytes, NULL, GL_STREAM_DRAW);
	RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)previousBuffer);
	m_BoneStorageCursorVec4 = 0;
}

bool CShaderGL::UploadBones(const float* data, int boneCount, int* baseVec4)
{
	const GLuint program = GetBoundVBOProgram();
	if (program == 0 || data == NULL || boneCount <= 0 ||
		boneCount > m_VBOBoneCapacity[eVBO_Model])
	{
		return false;
	}

	const int vec4Count = boneCount * kVec4PerBone;
	if (m_BoneTransport == eVBOBoneTransport_ShaderStorageBuffer)
	{
		if (m_BlessOriginalModelSyncEnabled)
		{
			if (m_BoneStorageBuffer == 0 || glBufferSubData == NULL)
				return false;

			float matrices[kBoneCapacity][kBlessOriginalBoneMatrixFloatCount];
			for (int bone = 0; bone < boneCount; ++bone)
			{
				memcpy(matrices[bone], data + bone * kVec4PerBone * 4,
					kVec4PerBone * 4 * sizeof(float));
				matrices[bone][12] = 0.f;
				matrices[bone][13] = 0.f;
				matrices[bone][14] = 0.f;
				matrices[bone][15] = 1.f;
			}

			GLint previousBuffer = 0;
			glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &previousBuffer);
			RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BoneStorageBuffer);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
				boneCount * kBlessOriginalBoneMatrixFloatCount * sizeof(float), matrices);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
				kBlessOriginalBoneStorageBindingPoint, m_BoneStorageBuffer);
			RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)previousBuffer);
			if (baseVec4 != NULL)
				*baseVec4 = 0;
			return true;
		}
		if (m_BoneStorageBuffer == 0 || glBufferSubData == NULL ||
			m_BoneStorageCursorVec4 + vec4Count > kBoneStorageVec4Capacity)
			return false;
		GLint previousBuffer = 0;
		glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &previousBuffer);
		RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BoneStorageBuffer);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, m_BoneStorageCursorVec4 * 4 * sizeof(float),
			vec4Count * 4 * sizeof(float), data);
		RenderProfilerBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)previousBuffer);
		if (baseVec4 != NULL)
			*baseVec4 = m_BoneStorageCursorVec4;
		m_BoneStorageCursorVec4 += vec4Count;
		return true;
	}
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

void CShaderGL::vboSetFloat(const char* name, float value) const
{
	const GLuint program = GetBoundVBOProgram();
	if (program == 0) return;
	const GLint loc = GetUniformLocation(program, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1f(loc, value);
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
