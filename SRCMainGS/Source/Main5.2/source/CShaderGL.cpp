#include "stdafx.h"
#include "CShaderGL.h"

#ifdef SHADER_VERSION_TEST
#include "Utilities/Log/muConsoleDebug.h"

CShaderGL::CShaderGL()
{
	shader_id = 0;
	m_CurrentVBOProgram = 0;
	for (int i = 0; i < eVBO_Max; ++i)
		m_VBOProgram[i] = 0;
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
		m_CurrentVBOProgram = 0;
		return;
	}

	// The normal shutdown path calls this before KillGLWindow. If a late static
	// destructor reaches it without a current context, invalidate the stale names
	// without issuing context-dependent GL calls.
	const bool canDelete = (wglGetCurrentContext() != NULL && glDeleteProgram != NULL);
	if (canDelete && shader_id != 0)
		RenderProfilerDeleteProgram(shader_id);
	shader_id = 0;

	for (int i = 0; i < eVBO_Max; ++i)
	{
		if (canDelete && m_VBOProgram[i] != 0)
			RenderProfilerDeleteProgram(m_VBOProgram[i]);
		m_VBOProgram[i] = 0;
	}

	m_CurrentVBOProgram = 0;
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

	GLuint shader_vertex = run_shader(vertex_shader.data(), GL_VERTEX_SHADER);

	GLuint shader_frgmen = run_shader(frgmen_shader.data(), GL_FRAGMENT_SHADER);

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

	// Eliminar los shaders compilados
	RenderProfilerDeleteShader(shader_vertex);
	RenderProfilerDeleteShader(shader_frgmen);
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

	GLuint vs = run_shader(vsSrc.data(), GL_VERTEX_SHADER);
	GLuint fs = run_shader(fsSrc.data(), GL_FRAGMENT_SHADER);

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
}

void CShaderGL::InitVBOShaders()
{
	for (int i = 0; i < eVBO_Max; ++i)
		m_VBOProgram[i] = loadVBOProgram(s_VBOShaderName[i]);
}

GLuint CShaderGL::GetVBOProgram(eVBOShader s) const
{
	if (s < 0 || s >= eVBO_Max)
		return 0;
	return m_VBOProgram[s];
}

bool CShaderGL::UseVBO(eVBOShader s)
{
	GLuint id = GetVBOProgram(s);
	if (id == 0)
		return false;

	RenderProfilerUseProgram(id);
	m_CurrentVBOProgram = id;
	return true;
}

void CShaderGL::vboSetInt(const char* name, int value) const
{
	if (m_CurrentVBOProgram == 0) return;
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	GLint loc = glGetUniformLocation(m_CurrentVBOProgram, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1i(loc, value);
	}
}

void CShaderGL::vboSetVec4(const char* name, float x, float y, float z, float w) const
{
	if (m_CurrentVBOProgram == 0) return;
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	GLint loc = glGetUniformLocation(m_CurrentVBOProgram, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform4f(loc, x, y, z, w);
	}
}

void CShaderGL::vboSetVec4Array(const char* name, const float* data, int vec4Count) const
{
	if (m_CurrentVBOProgram == 0) return;
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	GLint loc = glGetUniformLocation(m_CurrentVBOProgram, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_BONE);
		glUniform4fv(loc, vec4Count, data);
	}
}

void CShaderGL::vboSetMat4(const char* name, const float* m16) const
{
	if (m_CurrentVBOProgram == 0) return;
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	GLint loc = glGetUniformLocation(m_CurrentVBOProgram, name);
	if (loc >= 0)
	{
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATRIX);
		glUniformMatrix4fv(loc, 1, GL_FALSE, m16);
	}
}

void CShaderGL::RenderShader()
{
	if (this->CheckedShader())
	{
		RenderProfilerUseProgram(shader_id);
	}
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
	if (shader_id != 0)
	{
		RenderProfilerUseProgram(shader_id);

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
		g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
		g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
		glUniform1i(glGetUniformLocation(shader_id, "texture1"), 0);

		RenderProfilerUseProgram(0);
	}
}

void CShaderGL::SetPerspective(float Fov, float Aspect, float ZNear, float ZFar)
{
	if (shader_id != 0)
	{
		RenderProfilerUseProgram(shader_id);
		glm::mat4 projection = glm::perspective(glm::radians(Fov), Aspect, ZNear, ZFar);
		this->setMat4("projection", projection);
		RenderProfilerUseProgram(0);
	}
}

// Funciones para establecer uniforms
void CShaderGL::setBool(const char* name, bool value) const
{
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
	glUniform1i(glGetUniformLocation(shader_id, name), (int)value);
}

void CShaderGL::setInt(const char* name, int value) const
{
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
	glUniform1i(glGetUniformLocation(shader_id, name), value);
}

void CShaderGL::setFloat(const char* name, float value) const
{
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
	glUniform1f(glGetUniformLocation(shader_id, name), value);
}

void CShaderGL::setVec2(const char* name, float x, float y) const
{
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
	glUniform2f(glGetUniformLocation(shader_id, name), x, y);
}

void CShaderGL::setVec3(const char* name, float x, float y, float z) const
{
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
	glUniform3f(glGetUniformLocation(shader_id, name), x, y, z);
}

void CShaderGL::setVec4(const char* name, float x, float y, float z, float w) const
{
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATERIAL);
	glUniform4f(glGetUniformLocation(shader_id, name), x, y, z, w);
}

void CShaderGL::setMat4(const char* name, glm::mat4& matrix) const
{
	g_RenderProfiler.AddCounter(RPC_UNIFORM_LOCATION_QUERIES);
	g_RenderProfiler.AddCounter(RPC_UNIFORM_UPLOAD_MATRIX);
	glUniformMatrix4fv(glGetUniformLocation(shader_id, name), 1, GL_FALSE, glm::value_ptr(matrix));
}

CShaderGL* CShaderGL::Instance()
{
	static CShaderGL sInstance;
	return &sInstance;
}
#endif // SHADER_VERSION_TEST



