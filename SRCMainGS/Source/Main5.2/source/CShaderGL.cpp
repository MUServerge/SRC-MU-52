#include "stdafx.h"
#include "CShaderGL.h"
#include "CShaderScene.h"

#ifdef SHADER_VERSION_TEST
#include "Utilities/Log/muConsoleDebug.h"

CShaderGL::CShaderGL()
{
	shader_id = 0;
	m_MaxVertexUniformComponents = 0;
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
	bool hasPrograms = (shader_id != 0);
	for (int i = 0; i < eVBO_Max; ++i)
		hasPrograms = hasPrograms || (m_VBOProgram[i] != 0);

	if (!hasPrograms)
	{
		m_MaxVertexUniformComponents = 0;
		for (int i = 0; i < eVBO_Max; ++i)
			m_VBOBoneCapacity[i] = 0;
		return;
	}

	// The normal shutdown path calls this before KillGLWindow. If a late static
	// destructor reaches it without a current context, invalidate the stale names
	// without issuing context-dependent GL calls.
	const bool canDelete = (wglGetCurrentContext() != NULL && glDeleteProgram != NULL);
	const GLuint boundProgram = GetTrackedProgram();
	bool ownsBoundProgram = (shader_id != 0 && shader_id == boundProgram);
	for (int i = 0; i < eVBO_Max; ++i)
		ownsBoundProgram = ownsBoundProgram || (m_VBOProgram[i] != 0 && m_VBOProgram[i] == boundProgram);
	if (canDelete && ownsBoundProgram)
		RestoreProgram(0);

#ifdef SHADER_PIPELINE
	if (shader_id != 0)
		gShaderScene.ForgetProgram(shader_id);
#endif // SHADER_PIPELINE
	if (canDelete && shader_id != 0)
		RenderProfilerDeleteProgram(shader_id);
	shader_id = 0;

	for (int i = 0; i < eVBO_Max; ++i)
	{
#ifdef SHADER_PIPELINE
		if (m_VBOProgram[i] != 0)
			gShaderScene.ForgetProgram(m_VBOProgram[i]);
#endif // SHADER_PIPELINE
		if (canDelete && m_VBOProgram[i] != 0)
			RenderProfilerDeleteProgram(m_VBOProgram[i]);
		m_VBOProgram[i] = 0;
		m_VBOBoneCapacity[i] = 0;
	}

	m_MaxVertexUniformComponents = 0;
}

void CShaderGL::Init()
{
	// Load the per-material Data\Effect\VBO programs (VBO / GPU-skinning path)
	// first, so it is independent of the legacy single-program load below.
	InitVBOShaders();

	std::string vertex_shader;

	if (!readshader("Shaders\\shader.vs", vertex_shader))
	{
		return;
	}

	std::string frgmen_shader;

	if (!readshader("Shaders\\shader.fs", frgmen_shader))
	{
		return;
	}

#ifdef SHADER_PIPELINE
	shader_id = CShaderScene::BuildProgram(vertex_shader.c_str(), frgmen_shader.c_str(), "legacy shader");
	if (shader_id == 0)
		g_ConsoleDebug->Write(5, "[Shader] legacy program unavailable; fixed-function fallback remains active");
#else
	GLuint shader_vertex = run_shader(vertex_shader.c_str(), GL_VERTEX_SHADER);
	GLuint shader_frgmen = run_shader(frgmen_shader.c_str(), GL_FRAGMENT_SHADER);

	shader_id = RenderProfilerCreateProgram();
	glAttachShader(shader_id, shader_vertex);
	glAttachShader(shader_id, shader_frgmen);
	glLinkProgram(shader_id);

	int success;
	glGetProgramiv(shader_id, GL_LINK_STATUS, &success);
	g_RenderProfiler.RecordProgramLink(success != 0);

	if (!success)
	{
		char infoLog[512];
		glGetProgramInfoLog(shader_id, 512, NULL, infoLog);
		g_ConsoleDebug->Write(5, "Error al enlazar el Shader Program:");
		g_ConsoleDebug->Write(5, infoLog);
	}

	RenderProfilerDeleteShader(shader_vertex);
	RenderProfilerDeleteShader(shader_frgmen);
#endif // SHADER_PIPELINE
}

// One .vs/.fs pair per material, matching the Data\Effect\VBO folder layout.
// Index order == eVBOShader.
static const char* const s_VBOShaderName[eVBO_Max] =
{
	"Model", "BlendMesh", "Metal", "Oil",
	"Chrome1", "Chrome2", "Chrome3", "Chrome4", "Chrome5", "Chrome6", "Chrome7",
};

GLuint CShaderGL::loadVBOProgram(const char* baseName)
{
	std::string vsSrc, fsSrc;

	std::string vsPath = std::string("Data\\Effect\\VBO\\") + baseName + ".vs";
	std::string fsPath = std::string("Data\\Effect\\VBO\\") + baseName + ".fs";

	if (!readshader(vsPath.c_str(), vsSrc) || !readshader(fsPath.c_str(), fsSrc))
	{
		g_ConsoleDebug->Write(5, "[VBO Shader] missing file for '%s'", baseName);
		return 0;
	}

#ifdef SHADER_PIPELINE
	const GLuint program = CShaderScene::BuildProgram(vsSrc.c_str(), fsSrc.c_str(), baseName);
	if (program == 0)
		g_ConsoleDebug->Write(5, "[VBO Shader] program unavailable for '%s'; legacy mesh fallback remains active", baseName);
	return program;
#else
	GLuint vs = run_shader(vsSrc.c_str(), GL_VERTEX_SHADER);
	GLuint fs = run_shader(fsSrc.c_str(), GL_FRAGMENT_SHADER);

	GLuint program = RenderProfilerCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	RenderProfilerDeleteShader(vs);
	RenderProfilerDeleteShader(fs);

	int success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	g_RenderProfiler.RecordProgramLink(success != 0);
	if (!success)
	{
		char infoLog[512] = { 0 };
		glGetProgramInfoLog(program, sizeof(infoLog) - 1, NULL, infoLog);
		g_ConsoleDebug->Write(5, "[VBO Shader] link error (%s):", baseName);
		g_ConsoleDebug->Write(5, infoLog);
		RenderProfilerDeleteProgram(program);
		return 0;
	}

	return program;
#endif // SHADER_PIPELINE
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

	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &m_MaxVertexUniformComponents);
	if (m_MaxVertexUniformComponents <= 0)
	{
		g_ConsoleDebug->Write(5,
			"[VBO Shader] invalid GL_MAX_VERTEX_UNIFORM_COMPONENTS=%d; GPU skinning disabled",
			m_MaxVertexUniformComponents);
		return;
	}

	for (int i = 0; i < eVBO_Max; ++i)
	{
		m_VBOProgram[i] = loadVBOProgram(s_VBOShaderName[i]);
		if (m_VBOProgram[i] == 0)
			continue;

		m_VBOBoneCapacity[i] = InspectVBOBoneCapacity(m_VBOProgram[i], s_VBOShaderName[i]);
		if (m_VBOBoneCapacity[i] > 0)
			continue;

#ifdef SHADER_PIPELINE
		gShaderScene.ForgetProgram(m_VBOProgram[i]);
#endif // SHADER_PIPELINE
		RenderProfilerDeleteProgram(m_VBOProgram[i]);
		m_VBOProgram[i] = 0;
	}
}

GLuint CShaderGL::GetVBOProgram(eVBOShader s) const
{
	if (s < 0 || s >= eVBO_Max)
		return 0;
	return m_VBOProgram[s];
}

int CShaderGL::GetVBOBoneCapacity(eVBOShader s) const
{
	if (s < 0 || s >= eVBO_Max)
		return 0;
	return m_VBOBoneCapacity[s];
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

bool CShaderGL::UseVBO(eVBOShader s, GLuint* previousProgram)
{
	const GLuint id = GetVBOProgram(s);
	if (id == 0)
		return false;

	const GLuint previous = BindTrackedProgram(id);
	if (GetTrackedProgram() != id)
		return false;
	if (previousProgram != NULL)
		*previousProgram = previous;
	return true;
}

bool CShaderGL::UseLegacy(GLuint* previousProgram)
{
	if (shader_id == 0)
		return false;

	const GLuint previous = BindTrackedProgram(shader_id);
	if (GetTrackedProgram() != shader_id)
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

void CShaderGL::RenderShader()
{
	UseLegacy();
}

bool CShaderGL::CheckedShader()
{
	return (shader_id != 0);
}

GLuint CShaderGL::GetShaderId()
{
	return shader_id;
}

bool CShaderGL::readshader(const char* filename, std::string& shader_text)
{
	FILE* compressedFile = fopen(filename, "rb");

	if (compressedFile)
	{
		fseek(compressedFile, 0, SEEK_END);
		long fileSize = ftell(compressedFile);
		fseek(compressedFile, 0, SEEK_SET);

		shader_text.resize(fileSize, 0);
		fread(shader_text.data(), 1, fileSize, compressedFile);
		fclose(compressedFile);

		return true;
	}

	return false;
}

GLuint CShaderGL::run_shader(const char* shader_text, GLenum type)
{
	GLuint shader = RenderProfilerCreateShader(type);
	glShaderSource(shader, 1, &shader_text, NULL);
	glCompileShader(shader);

	// Verificar errores de compilaci�n
	int success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	g_RenderProfiler.RecordShaderCompile(success != 0);

	if (!success)
	{
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		g_ConsoleDebug->Write(5, "Error al compilar shader:");
		g_ConsoleDebug->Write(5, infoLog);
	}

	return shader;
}

void CShaderGL::run_projection()
{
	GLuint previousProgram = 0;
	if (UseLegacy(&previousProgram))
	{

		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 model = glm::mat4(1.0f);

		view = glm::rotate(view, glm::radians(CameraAngle[1]), glm::vec3(0.0f, 1.0f, 0.0f));
		if (CameraTopViewEnable == false)
			view = glm::rotate(view, glm::radians(CameraAngle[0]), glm::vec3(1.0f, 0.0f, 0.0f));
		view = glm::rotate(view, glm::radians(CameraAngle[2]), glm::vec3(0.0f, 0.0f, 1.0f));

		view = glm::translate(view, glm::vec3(-CameraPosition[0], -CameraPosition[1], -CameraPosition[2]));


		this->setMat4("view", view);
		this->setMat4("model", model);

		// texture sampler must be set while the program is still bound (core profile).
		const GLint textureLocation = GetUniformLocation(shader_id, "texture1");
		if (textureLocation >= 0)
		{
			g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
			glUniform1i(textureLocation, 0);
		}

		RestoreProgram(previousProgram);
	}
}

void CShaderGL::SetPerspective(float Fov, float Aspect, float ZNear, float ZFar)
{
	GLuint previousProgram = 0;
	if (UseLegacy(&previousProgram))
	{
		glm::mat4 projection = glm::perspective(glm::radians(Fov), Aspect, ZNear, ZFar);
		this->setMat4("projection", projection);
		RestoreProgram(previousProgram);
	}
}

// Funciones para establecer uniforms
void CShaderGL::setBool(const char* name, bool value) const
{
	if (shader_id == 0 || GetTrackedProgram() != shader_id) return;
	const GLint loc = GetUniformLocation(shader_id, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1i(loc, (int)value);
	}
}

void CShaderGL::setInt(const char* name, int value) const
{
	if (shader_id == 0 || GetTrackedProgram() != shader_id) return;
	const GLint loc = GetUniformLocation(shader_id, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1i(loc, value);
	}
}

void CShaderGL::setFloat(const char* name, float value) const
{
	if (shader_id == 0 || GetTrackedProgram() != shader_id) return;
	const GLint loc = GetUniformLocation(shader_id, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1f(loc, value);
	}
}

void CShaderGL::setVec2(const char* name, float x, float y) const
{
	if (shader_id == 0 || GetTrackedProgram() != shader_id) return;
	const GLint loc = GetUniformLocation(shader_id, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform2f(loc, x, y);
	}
}

void CShaderGL::setVec3(const char* name, float x, float y, float z) const
{
	if (shader_id == 0 || GetTrackedProgram() != shader_id) return;
	const GLint loc = GetUniformLocation(shader_id, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform3f(loc, x, y, z);
	}
}

void CShaderGL::setVec4(const char* name, float x, float y, float z, float w) const
{
	if (shader_id == 0 || GetTrackedProgram() != shader_id) return;
	const GLint loc = GetUniformLocation(shader_id, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform4f(loc, x, y, z, w);
	}
}

void CShaderGL::setMat4(const char* name, glm::mat4& matrix) const
{
	if (shader_id == 0 || GetTrackedProgram() != shader_id) return;
	const GLint loc = GetUniformLocation(shader_id, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATRIX);
		glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(matrix));
	}
}

CShaderGL* CShaderGL::Instance()
{
	static CShaderGL sInstance;
	return &sInstance;
}
#endif // SHADER_VERSION_TEST



