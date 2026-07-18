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

enum eVBOBoneTransport
{
	eVBOBoneTransport_None = 0,
	eVBOBoneTransport_UniformArray,
	eVBOBoneTransport_UniformBuffer,
};

bool IsTranslatedVBOEnabled();

class CShaderGL
{
public:
	CShaderGL();
	virtual ~CShaderGL();

	void Init();
	void Release();

	// --- Data\\Effect\\VBO Model program ---------------------------------------
	void InitVBOShaders();
	bool IsReadyVBO() const
	{
		return m_VBOProgram[eVBO_Model] != 0 &&
			m_VBOBoneCapacity[eVBO_Model] > 0 &&
			m_BoneTransport != eVBOBoneTransport_None;
	}
	GLuint GetVBOProgram(eVBOShader shader) const;
	int GetVBOBoneCapacity(eVBOShader shader) const;
	eVBOBoneTransport GetBoneTransport() const { return m_BoneTransport; }
	bool IsTranslatedVBOEnabled() const { return m_TranslatedVboEnabled; }
	bool UseVBO(eVBOShader shader, GLuint* previousProgram = NULL);
	void RestoreProgram(GLuint program);

	// Upload one validated model palette through the selected transport.
	bool UploadBones(const float* data, int boneCount) const;

	// Uniform setters operate on the currently bound VBO program.
	void vboSetInt(const char* name, int value) const;
	void vboSetVec4(const char* name, float x, float y, float z, float w) const;
	void vboSetMat4(const char* name, const float* m16) const;

	static CShaderGL* Instance();

private:
	GLuint LoadVBOProgram(const char* baseName, const char* vertexDefine = NULL);
	int InspectUniformArrayBoneCapacity(GLuint program, const char* tag) const;
	bool CanUseUniformBuffer() const;
	bool ConfigureUniformBuffer(GLuint program, const char* tag);
	void ReleaseUniformBuffer(bool canDelete);
	GLuint BindTrackedProgram(GLuint program) const;
	GLuint GetTrackedProgram() const;
	GLuint GetBoundVBOProgram() const;
	GLint GetUniformLocation(GLuint program, const char* name) const;

	GLuint m_VBOProgram[eVBO_Max];
	int m_VBOBoneCapacity[eVBO_Max];
	int m_MaxVertexUniformComponents;
	int m_MaxUniformBlockSize;
	GLuint m_BoneUniformBuffer;
	eVBOBoneTransport m_BoneTransport;
	bool m_TranslatedVboEnabled;
};

#define gShaderGL (CShaderGL::Instance())
#endif // SHADER_VERSION_TEST
