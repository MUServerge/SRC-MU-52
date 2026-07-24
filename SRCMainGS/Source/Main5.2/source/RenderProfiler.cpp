#include "stdafx.h"
#include "RenderProfiler.h"
#include "_define.h"
#include "Utilities/Log/ErrorReport.h"

namespace
{
	// Plain-text profiler sink. The reports also go to g_ErrorReport, but
	// MuError.log is XOR-encrypted AND is recreated once it passes 32 KB
	// (ErrorReport.cpp), so a profiling run overruns it and loses the data.
	// Mirror every report line into Client\RenderProfiler.log, truncated once
	// per launch so the file always holds exactly the current session.
	FILE* ProfilerTextFile()
	{
		static FILE* s_file = NULL;
		static bool s_opened = false;
		if (!s_opened)
		{
			s_opened = true;
			if (fopen_s(&s_file, "RenderProfiler.log", "wt") != 0)
				s_file = NULL;
		}
		return s_file;
	}

	void ProfilerEmit(const char* format, ...)
	{
		char line[1024];
		va_list args;
		va_start(args, format);
		_vsnprintf_s(line, sizeof(line), _TRUNCATE, format, args);
		va_end(args);

		g_ErrorReport.Write("%s", line);

		FILE* file = ProfilerTextFile();
		if (file != NULL)
		{
			fputs(line, file);
			fflush(file);
		}
	}

	const char* GetSectionName(RenderProfilerSection section)
	{
		switch (section)
		{
		case RP_MOVE_SCENE: return "Move";
		case RP_RENDER_SCENE: return "Render";
		case RP_PHYSICS_RENDER: return "PhysicsRender";
		case RP_SWAP_BUFFERS: return "Swap";
		case RP_FRAME_SLEEP: return "Sleep";
		case RP_BMD_TRANSFORM: return "BMD::Transform";
		case RP_BMD_TRANSFORM_VERTICES: return "BMD::TransformVertices";
		case RP_BMD_TRANSFORM_NORMALS: return "BMD::TransformNormals";
		case RP_BMD_RENDER_MESH: return "BMD::RenderMesh";
		case RP_BMD_RENDER_MESH_VBO: return "BMD::RenderMeshVBO";
		case RP_BMD_RENDER_MESH_LEGACY: return "BMD::RenderMeshLegacy";
		default: return "Unknown";
		}
	}

	const char* GetCounterName(RenderProfilerCounter counter)
	{
		switch (counter)
		{
		case RPC_TOTAL_DRAW_CALLS: return "DrawCalls";
		case RPC_IMMEDIATE_DRAW_CALLS: return "ImmediateDrawCalls";
		case RPC_LEGACY_BMD_MESH_DRAWS: return "LegacyBMDMeshDraws";
		case RPC_VBO_DRAW_ATTEMPTED: return "VBODrawAttempted";
		case RPC_VBO_DRAW_SUCCEEDED: return "VBODrawSucceeded";
		case RPC_VBO_DRAW_REJECTED: return "VBODrawRejected";
		case RPC_VBO_BONE_CAPACITY_REJECTED: return "VBOBoneCapacityRejected";
		case RPC_VBO_TRANSLATED_DRAW_ATTEMPTED: return "VBOTranslatedDrawAttempted";
		case RPC_VBO_TRANSLATED_DRAW_SUCCEEDED: return "VBOTranslatedDrawSucceeded";
		case RPC_VBO_TRANSLATED_DRAW_REJECTED: return "VBOTranslatedDrawRejected";
		case RPC_VBO_GATE_SCENE_OFF: return "VBOGateSceneOff";
		case RPC_VBO_GATE_TRANSLATE: return "VBOGateTranslate";
		case RPC_VBO_GATE_BONESCALE: return "VBOGateBoneScale";
		case RPC_VBO_GATE_OBJSCALE: return "VBOGateObjScale";
		case RPC_VBO_GATE_NOT_PLAIN_TEXTURE: return "VBOGateNotPlainTexture";
		case RPC_VBO_GATE_UNLIT: return "VBOGateUnlit";
		case RPC_VBO_GATE_WAVE: return "VBOGateWave";
		case RPC_VBO_GATE_NO_VAO: return "VBOGateNoVAO";
		case RPC_VBO_GATE_EXCLUDED_FLAG: return "VBOGateExcludedFlag";
		case RPC_VBO_TRANSLATE_PLAIN_CANDIDATE: return "VBOTranslatePlainCandidate";
		case RPC_VBO_TRANSLATE_MATERIAL_BLOCKED: return "VBOTranslateMaterialBlocked";
		case RPC_VBO_TRANSLATE_UNLIT_BLOCKED: return "VBOTranslateUnlitBlocked";
		case RPC_VBO_TRANSLATE_WAVE_BLOCKED: return "VBOTranslateWaveBlocked";
		case RPC_VBO_TRANSLATE_NO_VAO_BLOCKED: return "VBOTranslateNoVAOBlocked";
		case RPC_VBO_TRANSLATE_EXCLUDED_BLOCKED: return "VBOTranslateExcludedBlocked";
		case RPC_FIXED_UPDATE_STEPS: return "FixedUpdateSteps";
		case RPC_FIXED_UPDATE_DROPPED: return "FixedUpdateStepsDropped";
		case RPC_RENDERED_VERTICES_KNOWN: return "SubmittedVerticesKnown";
		case RPC_RENDERED_INDICES: return "SubmittedIndices";
		case RPC_RENDERED_TRIANGLES_KNOWN: return "SubmittedTrianglesKnown";
		case RPC_PROGRAM_BIND_REQUESTS: return "ProgramBindRequests";
		case RPC_PROGRAM_SWITCHES: return "ProgramSwitches";
		case RPC_PROGRAM_BIND_REDUNDANT: return "ProgramBindRedundant";
		case RPC_CURRENT_PROGRAM_QUERIES: return "CurrentProgramQueries";
		case RPC_UNIFORM_LOCATION_QUERIES: return "UniformLocationQueries";
		case RPC_UNIFORM_UPLOAD_MATRIX: return "UniformUploadsMatrix";
		case RPC_UNIFORM_UPLOAD_BONE: return "UniformArrayBoneUploads";
		case RPC_BONE_PALETTE_UPLOAD_UBO: return "UBOBonePaletteUploads";
		case RPC_UNIFORM_UPLOAD_MATERIAL: return "UniformUploadsMaterial";
		case RPC_VAO_BINDS: return "VAOBinds";
		case RPC_ARRAY_BUFFER_BINDS: return "ArrayBufferBinds";
		case RPC_ELEMENT_BUFFER_BINDS: return "ElementBufferBinds";
		case RPC_UNIFORM_BUFFER_BINDS: return "UniformBufferBinds";
		case RPC_TEXTURE_BIND_REQUESTS: return "TextureBindRequests";
		case RPC_TEXTURE_BIND_CHANGES: return "TextureBindChanges";
		case RPC_CPU_VERTEX_TRANSFORM_REQUIRED: return "CPUVertexTransformRequired";
		case RPC_CPU_VERTEX_TRANSFORM_SKIPPED: return "CPUVertexTransformSkipped";
		case RPC_CPU_NORMAL_TRANSFORM_REQUIRED: return "CPUNormalTransformRequired";
		case RPC_CPU_NORMAL_TRANSFORM_SKIPPED: return "CPUNormalTransformSkipped";
		case RPC_CPU_TRANSFORM_DEFERRED: return "CPUTransformDeferred";
		case RPC_CPU_TRANSFORM_DEFERRED_MATERIALIZED: return "CPUTransformDeferredMaterialized";
		case RPC_GPU_SKINNED_MESHES: return "GPUSkinnedMeshes";
		case RPC_GPU_UPLOADED_BONES: return "GPUUploadedBones";
		case RPC_SHADER_CREATED: return "ShaderCreated";
		case RPC_SHADER_DELETED: return "ShaderDeleted";
		case RPC_PROGRAM_CREATED: return "ProgramCreated";
		case RPC_PROGRAM_DELETED: return "ProgramDeleted";
		case RPC_VAO_CREATED: return "VAOCreated";
		case RPC_VAO_DELETED: return "VAODeleted";
		case RPC_BUFFER_CREATED: return "BufferCreated";
		case RPC_BUFFER_DELETED: return "BufferDeleted";
		case RPC_SHADER_COMPILE_SUCCEEDED: return "ShaderCompileSucceeded";
		case RPC_SHADER_COMPILE_FAILED: return "ShaderCompileFailed";
		case RPC_PROGRAM_LINK_SUCCEEDED: return "ProgramLinkSucceeded";
		case RPC_PROGRAM_LINK_FAILED: return "ProgramLinkFailed";
		default: return "UnknownCounter";
		}
	}

	const char* GetResourceName(RenderProfilerResource resource)
	{
		switch (resource)
		{
		case RPR_SHADER: return "Shader";
		case RPR_PROGRAM: return "Program";
		case RPR_VAO: return "VAO";
		case RPR_BUFFER: return "Buffer";
		default: return "UnknownResource";
		}
	}

	RenderProfilerCounter GetCreatedCounter(RenderProfilerResource resource)
	{
		switch (resource)
		{
		case RPR_SHADER: return RPC_SHADER_CREATED;
		case RPR_PROGRAM: return RPC_PROGRAM_CREATED;
		case RPR_VAO: return RPC_VAO_CREATED;
		default: return RPC_BUFFER_CREATED;
		}
	}

	RenderProfilerCounter GetDeletedCounter(RenderProfilerResource resource)
	{
		switch (resource)
		{
		case RPR_SHADER: return RPC_SHADER_DELETED;
		case RPR_PROGRAM: return RPC_PROGRAM_DELETED;
		case RPR_VAO: return RPC_VAO_DELETED;
		default: return RPC_BUFFER_DELETED;
		}
	}
}

CRenderProfiler g_RenderProfiler;

CRenderProfiler::CRenderProfiler()
{
	char enabledValue[16] = { 0 };
	const DWORD enabledLength = GetEnvironmentVariableA("MU_RENDER_PROFILER", enabledValue, sizeof(enabledValue));
	const char* commandLine = GetCommandLineA();
	const bool commandLineEnabled = commandLine != NULL && strstr(commandLine, "-renderprofiler") != NULL;
	// Marker-file enable, same pattern as the gl33* gates: launchers and
	// protection wrappers re-spawn Main.exe and drop both the environment
	// variable and the command line, so neither of the two switches above is
	// reliable in practice. An empty 'renderprofiler.enable' file in the client
	// working directory always works.
	const bool markerEnabled = GetFileAttributesA("renderprofiler.enable") != INVALID_FILE_ATTRIBUTES;
	m_enabled = (enabledLength > 0 && enabledLength < sizeof(enabledValue) && atoi(enabledValue) != 0)
		|| commandLineEnabled || markerEnabled;
	m_started = false;
	m_loadingFrame = false;
	m_sceneFlag = -1;
	m_currentFps = 0.0f;
	m_lastReportTime = 0;
	m_frameStartCounter = 0;
	m_lastProgram = 0;
	m_lastTextureTarget = 0;
	m_lastTexture = (GLuint)-1;

	LARGE_INTEGER frequency;
	m_counterFrequency = QueryPerformanceFrequency(&frequency) ? frequency.QuadPart : 0;

	memset(m_liveResources, 0, sizeof(m_liveResources));
	ResetWindow();
}

void CRenderProfiler::ResetWindow()
{
	memset(m_sectionMs, 0, sizeof(m_sectionMs));
	memset(m_sectionCalls, 0, sizeof(m_sectionCalls));
	memset(m_counters, 0, sizeof(m_counters));
	m_stableFrameSampleCount = 0;
	m_loadingFrameSampleCount = 0;
	m_frameCount = 0;
}

void CRenderProfiler::BeginFrame(int sceneFlag, float currentFps, bool loadingFrame)
{
	if (!m_enabled)
		return;

	if (!m_started)
	{
		m_started = true;
		m_lastReportTime = timeGetTime();
		g_ErrorReport.Write("[RenderProfiler] enabled; fixed sample capacity %d\r\n", FRAME_SAMPLE_CAPACITY);
	}

	if (m_sceneFlag >= 0 && m_sceneFlag != sceneFlag && m_frameCount > 0)
		ReportAndReset();

	m_sceneFlag = sceneFlag;
	m_currentFps = currentFps;
	m_loadingFrame = loadingFrame;
	m_frameCount++;

	LARGE_INTEGER counter;
	m_frameStartCounter = QueryPerformanceCounter(&counter) ? counter.QuadPart : 0;
}

void CRenderProfiler::Add(RenderProfilerSection section, double milliseconds)
{
	if (!m_enabled || section < 0 || section >= RP_SECTION_COUNT)
		return;

	m_sectionMs[section] += milliseconds;
	m_sectionCalls[section]++;
}

void CRenderProfiler::AddCounter(RenderProfilerCounter counter, int amount)
{
	if (!m_enabled || counter < 0 || counter >= RPC_COUNTER_COUNT || amount <= 0)
		return;

	m_counters[counter] += (unsigned long long)amount;
}

unsigned long long CRenderProfiler::TriangleCount(GLenum mode, GLsizei count)
{
	if (count <= 0)
		return 0;

	switch (mode)
	{
	case GL_TRIANGLES: return (unsigned long long)count / 3;
	case GL_TRIANGLE_STRIP:
	case GL_TRIANGLE_FAN: return count >= 3 ? (unsigned long long)(count - 2) : 0;
	case GL_QUADS: return ((unsigned long long)count / 4) * 2;
	case GL_QUAD_STRIP: return count >= 4 ? ((unsigned long long)count / 2 - 1) * 2 : 0;
	case GL_POLYGON: return count >= 3 ? (unsigned long long)(count - 2) : 0;
	default: return 0;
	}
}

void CRenderProfiler::RecordDraw(GLenum mode, GLsizei count, bool indexed)
{
	if (!m_enabled)
		return;

	AddCounter(RPC_TOTAL_DRAW_CALLS);
	AddCounter(RPC_RENDERED_VERTICES_KNOWN, count);
	AddCounter(RPC_RENDERED_TRIANGLES_KNOWN, (int)TriangleCount(mode, count));
	if (indexed)
		AddCounter(RPC_RENDERED_INDICES, count);
}

void CRenderProfiler::RecordImmediateDraw(GLenum mode)
{
	if (!m_enabled)
		return;

	AddCounter(RPC_TOTAL_DRAW_CALLS);
	AddCounter(RPC_IMMEDIATE_DRAW_CALLS);
	(void)mode;
}

void CRenderProfiler::RecordProgramBind(GLuint program)
{
	if (!m_enabled)
		return;

	AddCounter(RPC_PROGRAM_BIND_REQUESTS);
	if (program == m_lastProgram)
		AddCounter(RPC_PROGRAM_BIND_REDUNDANT);
	else
	{
		AddCounter(RPC_PROGRAM_SWITCHES);
		m_lastProgram = program;
	}
}

void CRenderProfiler::RecordBufferBind(GLenum target)
{
	if (!m_enabled)
		return;

	if (target == GL_ARRAY_BUFFER)
		AddCounter(RPC_ARRAY_BUFFER_BINDS);
	else if (target == GL_ELEMENT_ARRAY_BUFFER)
		AddCounter(RPC_ELEMENT_BUFFER_BINDS);
	else if (target == GL_UNIFORM_BUFFER)
		AddCounter(RPC_UNIFORM_BUFFER_BINDS);
}

void CRenderProfiler::RecordTextureBind(GLenum target, GLuint texture)
{
	if (!m_enabled)
		return;

	AddCounter(RPC_TEXTURE_BIND_REQUESTS);
	if (target != m_lastTextureTarget || texture != m_lastTexture)
	{
		AddCounter(RPC_TEXTURE_BIND_CHANGES);
		m_lastTextureTarget = target;
		m_lastTexture = texture;
	}
}

void CRenderProfiler::ResourceCreated(RenderProfilerResource resource, int amount)
{
	if (!m_enabled || resource < 0 || resource >= RPR_RESOURCE_COUNT || amount <= 0)
		return;

	m_liveResources[resource] += amount;
	AddCounter(GetCreatedCounter(resource), amount);
}

void CRenderProfiler::ResourceDeleted(RenderProfilerResource resource, int amount)
{
	if (!m_enabled || resource < 0 || resource >= RPR_RESOURCE_COUNT || amount <= 0)
		return;

	m_liveResources[resource] -= amount;
	AddCounter(GetDeletedCounter(resource), amount);
}

void CRenderProfiler::RecordShaderCompile(bool succeeded)
{
	AddCounter(succeeded ? RPC_SHADER_COMPILE_SUCCEEDED : RPC_SHADER_COMPILE_FAILED);
}

void CRenderProfiler::RecordProgramLink(bool succeeded)
{
	AddCounter(succeeded ? RPC_PROGRAM_LINK_SUCCEEDED : RPC_PROGRAM_LINK_FAILED);
}

double CRenderProfiler::CounterToMilliseconds(long long elapsedCounter) const
{
	if (m_counterFrequency <= 0)
		return 0.0;
	return ((double)elapsedCounter * 1000.0) / (double)m_counterFrequency;
}

void CRenderProfiler::ReportFrameSamples(const char* name, const double* samples, int sampleCount) const
{
	if (sampleCount <= 0)
		return;

	double sorted[FRAME_SAMPLE_CAPACITY];
	double total = 0.0;
	for (int i = 0; i < sampleCount; ++i)
	{
		sorted[i] = samples[i];
		total += samples[i];
	}
	std::sort(sorted, sorted + sampleCount);

	const int p50Index = (sampleCount - 1) * 50 / 100;
	const int p95Index = (sampleCount - 1) * 95 / 100;
	const int p99Index = (sampleCount - 1) * 99 / 100;
	ProfilerEmit("  %-22s samples %d, min %.3f, median %.3f, p95 %.3f, p99 %.3f, max %.3f, mean %.3f ms\r\n",
		name, sampleCount, sorted[0], sorted[p50Index], sorted[p95Index], sorted[p99Index],
		sorted[sampleCount - 1], total / (double)sampleCount);
}

void CRenderProfiler::ReportAndReset()
{
	if (!m_enabled || m_frameCount <= 0)
		return;

	ProfilerEmit("[RenderProfiler] scene %d, FPS %.0f, frames %d\r\n", m_sceneFlag, m_currentFps, m_frameCount);
	ReportFrameSamples("FrameTimeStable", m_stableFrameSamples, m_stableFrameSampleCount);
	ReportFrameSamples("FrameTimeLoading", m_loadingFrameSamples, m_loadingFrameSampleCount);

	for (int i = 0; i < RP_SECTION_COUNT; ++i)
	{
		if (m_sectionCalls[i] == 0)
			continue;

		const double avgFrameMs = m_sectionMs[i] / (double)m_frameCount;
		const double avgCallMs = m_sectionMs[i] / (double)m_sectionCalls[i];
		ProfilerEmit("  %-22s frame %.3f ms, call %.4f ms, calls/frame %.1f\r\n",
			GetSectionName((RenderProfilerSection)i), avgFrameMs, avgCallMs,
			(double)m_sectionCalls[i] / (double)m_frameCount);
	}

	for (int i = 0; i < RPC_COUNTER_COUNT; ++i)
	{
		if (m_counters[i] == 0)
			continue;

		ProfilerEmit("  %-22s count %llu, per frame %.1f\r\n",
			GetCounterName((RenderProfilerCounter)i), m_counters[i],
			(double)m_counters[i] / (double)m_frameCount);
	}

	for (int i = 0; i < RPR_RESOURCE_COUNT; ++i)
		ProfilerEmit("  %-22s live %lld\r\n", GetResourceName((RenderProfilerResource)i), m_liveResources[i]);

	ResetWindow();
}

void CRenderProfiler::EndFrame()
{
	if (!m_enabled)
		return;

	if (m_frameStartCounter != 0)
	{
		LARGE_INTEGER counter;
		if (QueryPerformanceCounter(&counter))
		{
			const double elapsedMs = CounterToMilliseconds(counter.QuadPart - m_frameStartCounter);
			if (m_loadingFrame)
			{
				if (m_loadingFrameSampleCount < FRAME_SAMPLE_CAPACITY)
					m_loadingFrameSamples[m_loadingFrameSampleCount++] = elapsedMs;
			}
			else if (m_stableFrameSampleCount < FRAME_SAMPLE_CAPACITY)
				m_stableFrameSamples[m_stableFrameSampleCount++] = elapsedMs;
		}
	}

	const unsigned int now = timeGetTime();
	if (now - m_lastReportTime >= 2000)
	{
		m_lastReportTime = now;
		ReportAndReset();
	}
}

CRenderProfilerScope::CRenderProfilerScope(RenderProfilerSection section)
	: m_section(section)
	, m_startCounter(0)
{
	if (!g_RenderProfiler.IsEnabled())
		return;

	LARGE_INTEGER counter;
	if (QueryPerformanceCounter(&counter))
		m_startCounter = counter.QuadPart;
}

CRenderProfilerScope::~CRenderProfilerScope()
{
	if (m_startCounter == 0)
		return;

	LARGE_INTEGER counter;
	if (QueryPerformanceCounter(&counter))
		g_RenderProfiler.Add(m_section, g_RenderProfiler.CounterToMilliseconds(counter.QuadPart - m_startCounter));
}

void RenderProfilerUseProgram(GLuint program)
{
	g_RenderProfiler.RecordProgramBind(program);
	glUseProgram(program);
}

void RenderProfilerBindVertexArray(GLuint array)
{
	g_RenderProfiler.AddCounter(RPC_VAO_BINDS);
	glBindVertexArray(array);
}

void RenderProfilerBindBuffer(GLenum target, GLuint buffer)
{
	g_RenderProfiler.RecordBufferBind(target);
	glBindBuffer(target, buffer);
}

GLuint RenderProfilerCreateShader(GLenum type)
{
	const GLuint shader = glCreateShader(type);
	if (shader != 0)
		g_RenderProfiler.ResourceCreated(RPR_SHADER);
	return shader;
}

void RenderProfilerDeleteShader(GLuint shader)
{
	if (shader != 0)
		g_RenderProfiler.ResourceDeleted(RPR_SHADER);
	glDeleteShader(shader);
}

GLuint RenderProfilerCreateProgram()
{
	const GLuint program = glCreateProgram();
	if (program != 0)
		g_RenderProfiler.ResourceCreated(RPR_PROGRAM);
	return program;
}

void RenderProfilerDeleteProgram(GLuint program)
{
	if (program != 0)
		g_RenderProfiler.ResourceDeleted(RPR_PROGRAM);
	glDeleteProgram(program);
}

void RenderProfilerGenVertexArrays(GLsizei count, GLuint* arrays)
{
	glGenVertexArrays(count, arrays);
	for (GLsizei i = 0; i < count; ++i)
	{
		if (arrays[i] != 0)
			g_RenderProfiler.ResourceCreated(RPR_VAO);
	}
}

void RenderProfilerGenBuffers(GLsizei count, GLuint* buffers)
{
	glGenBuffers(count, buffers);
	for (GLsizei i = 0; i < count; ++i)
	{
		if (buffers[i] != 0)
			g_RenderProfiler.ResourceCreated(RPR_BUFFER);
	}
}
