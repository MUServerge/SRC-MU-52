#pragma once

#ifdef SHADER_VERSION_TEST
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> // Necesario para glm::value_ptr


// -----------------------------------------------------------------------------
// Per-material GLSL programs shipped in Data\Effect\VBO (one .vs/.fs pair each).
// These drive the VBO / GPU-skinning model path (BMD::CreateVertexBuffer /
// BMD::RenderMeshVBO). Order is independent of the legacy RENDER_* flags; the
// caller maps a flag to the right program.
// -----------------------------------------------------------------------------
enum eVBOShader
{
	eVBO_Model = 0,
	eVBO_BlendMesh,
	eVBO_Metal,
	eVBO_Oil,
	eVBO_Chrome1,
	eVBO_Chrome2,
	eVBO_Chrome3,
	eVBO_Chrome4,
	eVBO_Chrome5,
	eVBO_Chrome6,
	eVBO_Chrome7,
	eVBO_Max
};

class CShaderGL
{
public:
	CShaderGL();
	virtual~CShaderGL();

	void Init();
	void Release();
	void RenderShader();
	bool CheckedShader();
	GLuint GetShaderId();

	bool readshader(const char* filename, std::string& shader_text);
	GLuint run_shader(const char* shader_text, GLenum type);

	void run_projection();
	void SetPerspective(float Fov, float Aspect, float ZNear, float ZFar);

	// --- Data\Effect\VBO material programs (VBO / GPU-skinning path) ----------
	// Initialize VBO shader capabilities and preload only the active Model
	// technique. Other material programs load once on first request.
	void InitVBOShaders();
	// True once the base program linked and its active u_Bones contract was verified.
	bool IsReadyVBO() const { return m_VBOProgram[eVBO_Model] != 0 && m_VBOBoneCapacity[eVBO_Model] > 0; }
	GLuint GetVBOProgram(eVBOShader s) const;
	int GetVBOBoneCapacity(eVBOShader s);
	// Bind a program; returns false (binds nothing) when unavailable so the
	// caller can fall back to the legacy immediate-mode draw.
	bool UseVBO(eVBOShader s, GLuint* previousProgram = NULL);
	bool UseLegacy(GLuint* previousProgram = NULL);
	void RestoreProgram(GLuint program);
	// Uniform setters operate on the currently bound VBO program.
	void vboSetInt(const char* name, int value) const;
	void vboSetVec4(const char* name, float x, float y, float z, float w) const;
	void vboSetVec4Array(const char* name, const float* data, int vec4Count) const;
	void vboSetMat4(const char* name, const float* m16) const;

	// Funciones para establecer uniforms
	void setBool(const char* name, bool value) const;
	void setInt(const char* name, int value) const;
	void setFloat(const char* name, float value) const;
	void setVec2(const char* name, float x, float y) const;
	void setVec3(const char* name, float x, float y, float z) const;
	void setVec4(const char* name, float x, float y, float z, float w) const;
	void setMat4(const char* name, glm::mat4& matrix) const;

	static CShaderGL* Instance();
private:
	bool EnsureVBOProgram(eVBOShader shader);
	GLuint loadVBOProgram(const char* baseName);
	int InspectVBOBoneCapacity(GLuint program, const char* tag) const;
	GLuint BindTrackedProgram(GLuint program) const;
	GLuint GetTrackedProgram() const;
	GLuint GetBoundVBOProgram() const;
	GLint GetUniformLocation(GLuint program, const char* name) const;

	GLuint shader_id;
	GLuint m_VBOProgram[eVBO_Max];
	int m_VBOBoneCapacity[eVBO_Max];
	bool m_VBOLoadAttempted[eVBO_Max];
	int m_MaxVertexUniformComponents;
};

#define gShaderGL				(CShaderGL::Instance())
#endif // SHADER_VERSION_TEST
