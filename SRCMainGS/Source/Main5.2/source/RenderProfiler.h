#pragma once

enum RenderProfilerSection
{
	RP_MOVE_SCENE = 0,
	RP_RENDER_SCENE,
	RP_PHYSICS_RENDER,
	RP_SWAP_BUFFERS,
	RP_FRAME_SLEEP,
	RP_BMD_TRANSFORM,
	RP_BMD_TRANSFORM_VERTICES,
	RP_BMD_TRANSFORM_NORMALS,
	RP_BMD_RENDER_MESH,
	RP_BMD_RENDER_MESH_VBO,
	RP_BMD_RENDER_MESH_LEGACY,
	RP_SECTION_COUNT
};

enum RenderProfilerCounter
{
	RPC_TOTAL_DRAW_CALLS = 0,
	RPC_IMMEDIATE_DRAW_CALLS,
	RPC_LEGACY_BMD_MESH_DRAWS,
	RPC_VBO_DRAW_ATTEMPTED,
	RPC_VBO_DRAW_SUCCEEDED,
	RPC_VBO_DRAW_REJECTED,
	RPC_VBO_BONE_CAPACITY_REJECTED,
	// First failing condition in the VBO eligibility chain. One reason is counted
	// per rejected mesh without changing the render-path decision.
	RPC_VBO_GATE_SCENE_OFF,
	RPC_VBO_GATE_TRANSLATE,
	RPC_VBO_GATE_BONESCALE,
	RPC_VBO_GATE_OBJSCALE,
	RPC_VBO_GATE_NOT_PLAIN_TEXTURE,
	RPC_VBO_GATE_UNLIT,
	RPC_VBO_GATE_WAVE,
	RPC_VBO_GATE_NO_VAO,
	RPC_VBO_GATE_EXCLUDED_FLAG,
	// Secondary classification for meshes whose first rejection was Translate.
	// These counters expose the next gate without enabling a new render path.
	RPC_VBO_TRANSLATE_PLAIN_CANDIDATE,
	RPC_VBO_TRANSLATE_MATERIAL_BLOCKED,
	RPC_VBO_TRANSLATE_UNLIT_BLOCKED,
	RPC_VBO_TRANSLATE_WAVE_BLOCKED,
	RPC_VBO_TRANSLATE_NO_VAO_BLOCKED,
	RPC_VBO_TRANSLATE_EXCLUDED_BLOCKED,
	RPC_FIXED_UPDATE_STEPS,
	RPC_FIXED_UPDATE_DROPPED,
	RPC_RENDERED_VERTICES_KNOWN,
	RPC_RENDERED_INDICES,
	RPC_RENDERED_TRIANGLES_KNOWN,
	RPC_PROGRAM_BIND_REQUESTS,
	RPC_PROGRAM_SWITCHES,
	RPC_PROGRAM_BIND_REDUNDANT,
	RPC_CURRENT_PROGRAM_QUERIES,
	RPC_UNIFORM_LOCATION_QUERIES,
	RPC_UNIFORM_UPLOAD_MATRIX,
	RPC_UNIFORM_UPLOAD_BONE,
	RPC_BONE_PALETTE_UPLOAD_UBO,
	RPC_UNIFORM_UPLOAD_MATERIAL,
	RPC_VAO_BINDS,
	RPC_ARRAY_BUFFER_BINDS,
	RPC_ELEMENT_BUFFER_BINDS,
	RPC_UNIFORM_BUFFER_BINDS,
	RPC_TEXTURE_BIND_REQUESTS,
	RPC_TEXTURE_BIND_CHANGES,
	RPC_CPU_VERTEX_TRANSFORM_REQUIRED,
	RPC_CPU_VERTEX_TRANSFORM_SKIPPED,
	RPC_CPU_NORMAL_TRANSFORM_REQUIRED,
	RPC_CPU_NORMAL_TRANSFORM_SKIPPED,
	RPC_CPU_TRANSFORM_DEFERRED,
	RPC_CPU_TRANSFORM_DEFERRED_MATERIALIZED,
	RPC_GPU_SKINNED_MESHES,
	RPC_GPU_UPLOADED_BONES,
	RPC_SHADER_CREATED,
	RPC_SHADER_DELETED,
	RPC_PROGRAM_CREATED,
	RPC_PROGRAM_DELETED,
	RPC_VAO_CREATED,
	RPC_VAO_DELETED,
	RPC_BUFFER_CREATED,
	RPC_BUFFER_DELETED,
	RPC_SHADER_COMPILE_SUCCEEDED,
	RPC_SHADER_COMPILE_FAILED,
	RPC_PROGRAM_LINK_SUCCEEDED,
	RPC_PROGRAM_LINK_FAILED,
	RPC_COUNTER_COUNT
};

enum RenderProfilerResource
{
	RPR_SHADER = 0,
	RPR_PROGRAM,
	RPR_VAO,
	RPR_BUFFER,
	RPR_RESOURCE_COUNT
};

class CRenderProfiler
{
public:
	CRenderProfiler();

	bool IsEnabled() const { return m_enabled; }
	void BeginFrame(int sceneFlag, float currentFps, bool loadingFrame = false);
	void Add(RenderProfilerSection section, double milliseconds);
	void AddCounter(RenderProfilerCounter counter, int amount = 1);
	void EndFrame();

	void RecordDraw(GLenum mode, GLsizei count, bool indexed);
	void RecordImmediateDraw(GLenum mode);
	void RecordProgramBind(GLuint program);
	void RecordBufferBind(GLenum target);
	void RecordTextureBind(GLenum target, GLuint texture);
	void ResourceCreated(RenderProfilerResource resource, int amount = 1);
	void ResourceDeleted(RenderProfilerResource resource, int amount = 1);
	void RecordShaderCompile(bool succeeded);
	void RecordProgramLink(bool succeeded);
	double CounterToMilliseconds(long long elapsedCounter) const;

private:
	static const int FRAME_SAMPLE_CAPACITY = 512;

	void ReportFrameSamples(const char* name, const double* samples, int sampleCount) const;
	void ReportAndReset();
	void ResetWindow();
	static unsigned long long TriangleCount(GLenum mode, GLsizei count);

	bool m_enabled;
	bool m_started;
	bool m_loadingFrame;
	double m_sectionMs[RP_SECTION_COUNT];
	unsigned long long m_sectionCalls[RP_SECTION_COUNT];
	unsigned long long m_counters[RPC_COUNTER_COUNT];
	long long m_liveResources[RPR_RESOURCE_COUNT];
	double m_stableFrameSamples[FRAME_SAMPLE_CAPACITY];
	double m_loadingFrameSamples[FRAME_SAMPLE_CAPACITY];
	int m_stableFrameSampleCount;
	int m_loadingFrameSampleCount;
	int m_frameCount;
	int m_sceneFlag;
	float m_currentFps;
	unsigned int m_lastReportTime;
	long long m_counterFrequency;
	long long m_frameStartCounter;
	GLuint m_lastProgram;
	GLenum m_lastTextureTarget;
	GLuint m_lastTexture;
};

class CRenderProfilerScope
{
public:
	explicit CRenderProfilerScope(RenderProfilerSection section);
	~CRenderProfilerScope();

private:
	RenderProfilerSection m_section;
	long long m_startCounter;
};

extern CRenderProfiler g_RenderProfiler;

void RenderProfilerUseProgram(GLuint program);
void RenderProfilerBindVertexArray(GLuint array);
void RenderProfilerBindBuffer(GLenum target, GLuint buffer);
GLuint RenderProfilerCreateShader(GLenum type);
void RenderProfilerDeleteShader(GLuint shader);
GLuint RenderProfilerCreateProgram();
void RenderProfilerDeleteProgram(GLuint program);
void RenderProfilerGenVertexArrays(GLsizei count, GLuint* arrays);
void RenderProfilerGenBuffers(GLsizei count, GLuint* buffers);

inline void RenderProfilerGLBegin(GLenum mode)
{
	g_RenderProfiler.RecordImmediateDraw(mode);
	glBegin(mode);
}

inline void RenderProfilerGLDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	g_RenderProfiler.RecordDraw(mode, count, false);
	glDrawArrays(mode, first, count);
}

inline void RenderProfilerGLDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
	g_RenderProfiler.RecordDraw(mode, count, true);
	glDrawElements(mode, count, type, indices);
}

inline void RenderProfilerGLBindTexture(GLenum target, GLuint texture)
{
	g_RenderProfiler.RecordTextureBind(target, texture);
	glBindTexture(target, texture);
}

// These four core drawing calls are wrapped after the OpenGL declarations have
// been included by StdAfx.h. The wrappers preserve argument order and call the
// original function exactly once. When MU_RENDER_PROFILER is absent, recording
// returns immediately and the renderer follows the same GL call sequence.
#define glBegin(mode) RenderProfilerGLBegin(mode)
#define glDrawArrays(mode, first, count) RenderProfilerGLDrawArrays(mode, first, count)
#define glDrawElements(mode, count, type, indices) RenderProfilerGLDrawElements(mode, count, type, indices)
#define glBindTexture(target, texture) RenderProfilerGLBindTexture(target, texture)
