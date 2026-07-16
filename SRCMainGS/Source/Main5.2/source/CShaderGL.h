#pragma once

#ifdef SHADER_VERSION_TEST

// Active GPU-skinned BMD program. Chrome/metal/oil and other special materials
// remain on their authoritative legacy render path until they are implemented
// and verified rather than carrying unused GLSL variants.
enum eVBOShader
{
	eVBO_Model = 0,
	eVBO_Max
};

class CShaderGL
{
public:
	CShaderGL();
	virtual ~CShaderGL();

	void Init();
	void Release();

	// --- Data\Effect\VBO Model program ---------------------------------------
	void InitVBOShaders();
	bool IsReadyVBO() const
	{
		return m_VBOProgram[eVBO_Model] != 0 &&
			m_VBOBoneCapacity[eVBO_Model] > 0;
	}
	GLuint GetVBOProgram(eVBOShader shader) const;
	int GetVBOBoneCapacity(eVBOShader shader) const;
	bool UseVBO(eVBOShader shader, GLuint* previousProgram = NULL);
	void RestoreProgram(GLuint program);

	// Uniform setters operate on the currently bound VBO program.
	void vboSetInt(const char* name, int value) const;
	void vboSetVec4(const char* name, float x, float y, float z, float w) const;
	void vboSetVec4Array(const char* name, const float* data, int vec4Count) const;
	void vboSetMat4(const char* name, const float* m16) const;

	static CShaderGL* Instance();

private:
	GLuint LoadVBOProgram(const char* baseName);
	int InspectVBOBoneCapacity(GLuint program, const char* tag) const;
	GLuint BindTrackedProgram(GLuint program) const;
	GLuint GetTrackedProgram() const;
	GLuint GetBoundVBOProgram() const;
	GLint GetUniformLocation(GLuint program, const char* name) const;

	GLuint m_VBOProgram[eVBO_Max];
	int m_VBOBoneCapacity[eVBO_Max];
	int m_MaxVertexUniformComponents;
};

#define gShaderGL (CShaderGL::Instance())
#endif // SHADER_VERSION_TEST
