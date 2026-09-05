#include "stdafx.h"
#include "RenderProfiler.h"
#include "_define.h"
#include "Utilities/Log/ErrorReport.h"
#include <share.h> // _fsopen / _SH_DENYWR for the shared plain-text log

namespace
{
	// Plain-text profiler sink. The reports also go to g_ErrorReport, but
	// MuError.log is XOR-encrypted AND is recreated once it passes 32 KB
	// (ErrorReport.cpp), so a profiling run overruns it and loses its own data.
	// Mirror every report line into Client\RenderProfiler.log, truncated once per
	// launch so the file always holds exactly the current session. Opened with
	// _fsopen/_SH_DENYWR, not fopen_s, so it can be read while the client runs.
	FILE* ProfilerTextFile()
	{
		static FILE* s_file = NULL;
		static bool s_opened = false;
		if (!s_opened)
		{
			s_opened = true;
			s_file = _fsopen("RenderProfiler.log", "wt", _SH_DENYWR);
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
		case RP_INPUT: return "Input";
		case RP_SCENE_MOVE: return "SceneMove";
		case RP_OBJECT_UPDATE: return "ObjectUpdate";
		case RP_CHARACTER_UPDATE: return "CharacterUpdate";
		case RP_EFFECT_UPDATE: return "EffectUpdate";
		case RP_PHYSICS_MOVE: return "PhysicsMove";
		case RP_UI_NOTICES: return "UI/Notices";
		case RP_NETWORK_TAIL: return "NetworkTail";
		case RP_RENDER_WORLD: return "RenderWorld/Terrain";
		case RP_RENDER_MODELS: return "RenderModels/Characters";
		case RP_RENDER_EFFECTS: return "RenderEffects/Particles";
		case RP_RENDER_UI: return "RenderUI";
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
		case RPC_FIXED_ZERO_STEP_FRAMES: return "FixedZeroStepFrames";
		case RPC_FIXED_MULTI_STEP_FRAMES: return "FixedMultiStepFrames";
		case RPC_ANIMATION_KEY_CROSSINGS: return "AnimationKeyCrossings";
		case RPC_VISUAL_EMISSION_CHECKS: return "VisualEmissionChecks";
		case RPC_VISUAL_EMISSION_OPPORTUNITIES: return "VisualEmissionOpportunities";
		case RPC_PHYSICS_NONFINITE: return "PhysicsNonFinite";
		case RPC_PACKETS_SENT: return "PacketsSent";
		case RPC_PACKET_BYTES_SENT: return "PacketBytesSent";
		case RPC_PACKETS_RECEIVED: return "PacketsReceived";
		case RPC_PACKET_BYTES_RECEIVED: return "PacketBytesReceived";
		case RPC_MODEL_DRAW_CALLS: return "ModelsDrawCalls";
		case RPC_MODEL_DRAW_ARRAYS: return "ModelsDrawArrays";
		case RPC_MODEL_DRAW_ELEMENTS: return "ModelsDrawElements";
		case RPC_MODEL_IMMEDIATE_DRAW_CALLS: return "ModelsImmediateDrawCalls";
		case RPC_MODEL_LEGACY_BMD_MESH_DRAWS: return "ModelsLegacyBMDMeshDraws";
		case RPC_MODEL_PROGRAM_SWITCHES: return "ModelsProgramSwitches";
		case RPC_MODEL_TEXTURE_BIND_REQUESTS: return "ModelsTextureBindRequests";
		case RPC_MODEL_TEXTURE_BIND_CHANGES: return "ModelsTextureBindChanges";
		case RPC_MODEL_VAO_BINDS: return "ModelsVAOBinds";
		case RPC_MODEL_ARRAY_BUFFER_BINDS: return "ModelsArrayBufferBinds";
		case RPC_MODEL_ELEMENT_BUFFER_BINDS: return "ModelsElementBufferBinds";
		case RPC_MODEL_UNIFORM_BUFFER_BINDS: return "ModelsUniformBufferBinds";
		case RPC_MODEL_BONE_PALETTE_UPLOADS: return "ModelsBonePaletteUploads";
		case RPC_MODEL_MATERIAL_UNIFORM_UPLOADS: return "ModelsMaterialUniformUploads";
		case RPC_MODEL_VISIBLE_CHARACTERS: return "ModelsVisibleCharacters";
		case RPC_MODEL_VISIBLE_PLAYERS: return "ModelsVisiblePlayers";
		case RPC_MODEL_VISIBLE_MONSTERS_NPCS: return "ModelsVisibleMonstersNPCs";
		case RPC_MODEL_VISIBLE_PETS: return "ModelsVisiblePets";
		case RPC_MODEL_RENDERED_BODIES: return "ModelsRenderedBodies";
		case RPC_MODEL_RENDERED_MESHES: return "ModelsRenderedMeshes";
		case RPC_MODEL_PASS_BODY: return "ModelsPassBody";
		case RPC_MODEL_PASS_DIRECT_OVERLAY: return "ModelsPassDirectOverlay";
		case RPC_MODEL_PASS_SHADOW: return "ModelsPassShadow";
		case RPC_MODEL_PASS_SELECTION: return "ModelsPassSelection";
		case RPC_MODEL_PASS_TEXTURE_SCRIPT_REPEAT: return "ModelsPassTextureScriptRepeat";
		case RPC_MODEL_PASS_ALPHA: return "ModelsPassAlpha";
		case RPC_MODEL_MATERIAL_PLAIN: return "ModelsMaterialPlain";
		case RPC_MODEL_MATERIAL_SPECIAL: return "ModelsMaterialSpecial";
		case RPC_MODEL_LEGACY_MATERIAL_CHROME: return "ModelsLegacyChromeMetalOil";
		case RPC_MODEL_LEGACY_MATERIAL_BRIGHT: return "ModelsLegacyBright";
		case RPC_MODEL_LEGACY_MATERIAL_WAVE: return "ModelsLegacyWave";
		case RPC_MODEL_LEGACY_MATERIAL_SHADOW: return "ModelsLegacyShadow";
		case RPC_MODEL_LEGACY_MATERIAL_COLOR: return "ModelsLegacyColor";
		case RPC_MODEL_LEGACY_MATERIAL_PLAIN: return "ModelsLegacyPlain";
		case RPC_MODEL_LEGACY_MATERIAL_OTHER: return "ModelsLegacyOther";
		case RPC_MODEL_PASS_UNCLASSIFIED: return "ModelsPassUnclassified";
		case RPC_EFFECT_CREATE_ATTEMPTS: return "EffectCreateAttempts";
		case RPC_EFFECT_CREATE_SUCCEEDED: return "EffectCreateSucceeded";
		case RPC_EFFECT_CREATE_NO_FREE_SLOT: return "EffectCreateNoFreeSlot";
		case RPC_PARTICLE_CREATE_ATTEMPTS: return "ParticleCreateAttempts";
		case RPC_PARTICLE_CREATE_SUCCEEDED: return "ParticleCreateSucceeded";
		case RPC_PARTICLE_CREATE_NO_FREE_SLOT: return "ParticleCreateNoFreeSlot";
		case RPC_JOINT_CREATE_ATTEMPTS: return "JointCreateAttempts";
		case RPC_JOINT_CREATE_SUCCEEDED: return "JointCreateSucceeded";
		case RPC_JOINT_CREATE_NO_FREE_SLOT: return "JointCreateNoFreeSlot";
		default: return "UnknownCounter";
		}
	}

	// Startup-only renderer diagnostics deliberately use a separate plain-text
	// file.  MuError.log is shared with account/login diagnostics and is kept
	// encrypted; this sink accepts only explicit renderer capability messages.
	FILE* RendererDiagnosticsFile()
	{
		static FILE* s_file = NULL;
		static bool s_opened = false;
		if (!s_opened)
		{
			s_opened = true;
			s_file = _fsopen("RendererDiagnostics.log", "wt", _SH_DENYWR);
		}
		return s_file;
	}

	const char* GetPoolName(RenderProfilerPool pool)
	{
		switch (pool)
		{
		case RPP_EFFECT: return "EffectPool";
		case RPP_PARTICLE: return "ParticlePool";
		case RPP_JOINT: return "JointPool";
		default: return "UnknownPool";
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
	const bool rendererDiagnosticsEnabled = commandLine != NULL && strstr(commandLine, "-renderdiagnostics") != NULL;
	// Marker-file gate. Launchers and the protection wrapper re-spawn Main.exe and
	// drop BOTH the environment variable and the command line, so neither switch
	// above is reliable in practice - a capture produced zero reports. An empty
	// 'renderprofiler.enable' file in the client directory always works.
	const bool markerEnabled = GetFileAttributesA("renderprofiler.enable") != INVALID_FILE_ATTRIBUTES;
	m_enabled = (enabledLength > 0 && enabledLength < sizeof(enabledValue) && atoi(enabledValue) != 0)
		|| commandLineEnabled || markerEnabled;
	m_rendererDiagnosticsEnabled = rendererDiagnosticsEnabled;
	m_started = false;
	m_loadingFrame = false;
	m_sceneFlag = -1;
	m_currentFps = 0.0f;
	m_lastReportTime = 0;
	m_frameStartCounter = 0;
	m_lastProgram = 0;
	m_lastTextureTarget = 0;
	m_lastTexture = (GLuint)-1;
	m_modelsScopeDepth = 0;

	LARGE_INTEGER frequency;
	m_counterFrequency = QueryPerformanceFrequency(&frequency) ? frequency.QuadPart : 0;

	memset(m_liveResources, 0, sizeof(m_liveResources));
	memset(m_poolCurrent, 0, sizeof(m_poolCurrent));
	memset(m_poolPeak, 0, sizeof(m_poolPeak));
	ResetWindow();
}

void CRenderProfiler::WriteRendererDiagnostic(const char* format, ...) const
{
	if (!m_rendererDiagnosticsEnabled)
		return;

	char line[1024];
	va_list args;
	va_start(args, format);
	_vsnprintf_s(line, sizeof(line), _TRUNCATE, format, args);
	va_end(args);

	FILE* file = RendererDiagnosticsFile();
	if (file != NULL)
	{
		fputs(line, file);
		fflush(file);
	}
}

void CRenderProfiler::ResetWindow()
{
	memset(m_sectionMs, 0, sizeof(m_sectionMs));
	memset(m_sectionCalls, 0, sizeof(m_sectionCalls));
	memset(m_counters, 0, sizeof(m_counters));
	for (int i = 0; i < RPP_POOL_COUNT; ++i)
		m_poolPeak[i] = m_poolCurrent[i];
	m_stableFrameSampleCount = 0;
	m_loadingFrameSampleCount = 0;
	m_timingSampleCount = 0;
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

	if (m_modelsScopeDepth <= 0)
		return;

	RenderProfilerCounter modelsCounter = RPC_COUNTER_COUNT;
	switch (counter)
	{
	case RPC_TOTAL_DRAW_CALLS: modelsCounter = RPC_MODEL_DRAW_CALLS; break;
	case RPC_IMMEDIATE_DRAW_CALLS: modelsCounter = RPC_MODEL_IMMEDIATE_DRAW_CALLS; break;
	case RPC_LEGACY_BMD_MESH_DRAWS: modelsCounter = RPC_MODEL_LEGACY_BMD_MESH_DRAWS; break;
	case RPC_PROGRAM_SWITCHES: modelsCounter = RPC_MODEL_PROGRAM_SWITCHES; break;
	case RPC_TEXTURE_BIND_REQUESTS: modelsCounter = RPC_MODEL_TEXTURE_BIND_REQUESTS; break;
	case RPC_TEXTURE_BIND_CHANGES: modelsCounter = RPC_MODEL_TEXTURE_BIND_CHANGES; break;
	case RPC_VAO_BINDS: modelsCounter = RPC_MODEL_VAO_BINDS; break;
	case RPC_ARRAY_BUFFER_BINDS: modelsCounter = RPC_MODEL_ARRAY_BUFFER_BINDS; break;
	case RPC_ELEMENT_BUFFER_BINDS: modelsCounter = RPC_MODEL_ELEMENT_BUFFER_BINDS; break;
	case RPC_UNIFORM_BUFFER_BINDS: modelsCounter = RPC_MODEL_UNIFORM_BUFFER_BINDS; break;
	case RPC_BONE_PALETTE_UPLOAD_UBO: modelsCounter = RPC_MODEL_BONE_PALETTE_UPLOADS; break;
	case RPC_UNIFORM_UPLOAD_MATERIAL: modelsCounter = RPC_MODEL_MATERIAL_UNIFORM_UPLOADS; break;
	default: break;
	}

	if (modelsCounter != RPC_COUNTER_COUNT)
		m_counters[modelsCounter] += (unsigned long long)amount;
}

void CRenderProfiler::AddModelsCounter(RenderProfilerCounter counter, int amount)
{
	if (!IsModelsScopeActive() || counter < 0 || counter >= RPC_COUNTER_COUNT || amount <= 0)
		return;

	m_counters[counter] += (unsigned long long)amount;
}

void CRenderProfiler::SamplePoolOccupancy(RenderProfilerPool pool, int liveCount)
{
	if (!m_enabled || pool < 0 || pool >= RPP_POOL_COUNT || liveCount < 0)
		return;

	m_poolCurrent[pool] = liveCount;
	if (liveCount > m_poolPeak[pool])
		m_poolPeak[pool] = liveCount;
}

void CRenderProfiler::EnterSection(RenderProfilerSection section)
{
	if (m_enabled && section == RP_RENDER_MODELS)
		m_modelsScopeDepth++;
}

void CRenderProfiler::LeaveSection(RenderProfilerSection section)
{
	if (m_enabled && section == RP_RENDER_MODELS && m_modelsScopeDepth > 0)
		m_modelsScopeDepth--;
}

int CRenderProfiler::RecordPacketSendResult(int result, int knownBytes)
{
	if (m_enabled && result != FALSE && knownBytes > 0)
	{
		AddCounter(RPC_PACKETS_SENT);
		AddCounter(RPC_PACKET_BYTES_SENT, knownBytes);
	}
	return result;
}

void CRenderProfiler::SampleTimingFrame(double rawDeltaMs, double visualDeltaMs, float renderFps,
	int fixedSteps, int droppedSteps, double fixedAccumulatorMs, double interpolationAlpha)
{
	if (!m_enabled)
		return;

	AddCounter(RPC_FIXED_UPDATE_STEPS, fixedSteps);
	AddCounter(RPC_FIXED_UPDATE_DROPPED, droppedSteps);
	if (fixedSteps == 0)
		AddCounter(RPC_FIXED_ZERO_STEP_FRAMES);
	else if (fixedSteps > 1)
		AddCounter(RPC_FIXED_MULTI_STEP_FRAMES);

	if (m_timingSampleCount < FRAME_SAMPLE_CAPACITY)
	{
		m_rawDeltaSamples[m_timingSampleCount] = rawDeltaMs;
		m_visualDeltaSamples[m_timingSampleCount] = visualDeltaMs;
		m_renderFpsSamples[m_timingSampleCount] = renderFps;
		m_fixedAccumulatorSamples[m_timingSampleCount] = fixedAccumulatorMs;
		m_interpolationAlphaSamples[m_timingSampleCount] = interpolationAlpha;
		m_timingSampleCount++;
	}
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
	AddModelsCounter(indexed ? RPC_MODEL_DRAW_ELEMENTS : RPC_MODEL_DRAW_ARRAYS);
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

void CRenderProfiler::ReportFrameSamples(const char* name, const double* samples, int sampleCount, const char* unit) const
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
	ProfilerEmit("  %-22s samples %d, min %.3f, median %.3f, p95 %.3f, p99 %.3f, max %.3f, mean %.3f %s\r\n",
		name, sampleCount, sorted[0], sorted[p50Index], sorted[p95Index], sorted[p99Index],
		sorted[sampleCount - 1], total / (double)sampleCount, unit);
}

void CRenderProfiler::ReportAndReset()
{
	if (!m_enabled || m_frameCount <= 0)
		return;

	ProfilerEmit("[RenderProfiler] scene %d, FPS %.0f, frames %d\r\n", m_sceneFlag, m_currentFps, m_frameCount);
	ReportFrameSamples("FrameTimeStable", m_stableFrameSamples, m_stableFrameSampleCount, "ms");
	ReportFrameSamples("FrameTimeLoading", m_loadingFrameSamples, m_loadingFrameSampleCount, "ms");
	ReportFrameSamples("RawFrameDelta", m_rawDeltaSamples, m_timingSampleCount, "ms");
	ReportFrameSamples("VisualFrameDelta", m_visualDeltaSamples, m_timingSampleCount, "ms");
	ReportFrameSamples("RenderFPS", m_renderFpsSamples, m_timingSampleCount, "FPS");
	ReportFrameSamples("FixedAccumulator", m_fixedAccumulatorSamples, m_timingSampleCount, "ms");
	ReportFrameSamples("InterpolationAlpha", m_interpolationAlphaSamples, m_timingSampleCount, "ratio");

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

	for (int i = 0; i < RPP_POOL_COUNT; ++i)
		ProfilerEmit("  %-22s current %d, peak %d\r\n", GetPoolName((RenderProfilerPool)i),
			m_poolCurrent[i], m_poolPeak[i]);

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
	g_RenderProfiler.EnterSection(section);
	if (!g_RenderProfiler.IsEnabled())
		return;

	LARGE_INTEGER counter;
	if (QueryPerformanceCounter(&counter))
		m_startCounter = counter.QuadPart;
}

CRenderProfilerScope::~CRenderProfilerScope()
{
	if (m_startCounter != 0)
	{
		LARGE_INTEGER counter;
		if (QueryPerformanceCounter(&counter))
			g_RenderProfiler.Add(m_section, g_RenderProfiler.CounterToMilliseconds(counter.QuadPart - m_startCounter));
	}
	g_RenderProfiler.LeaveSection(m_section);
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
