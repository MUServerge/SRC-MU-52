///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ZzzOpenglUtil.h"
#include "ZzzTexture.h"
#include "ZzzBMD.h"
#include "ZzzInfomation.h"
#include "zzzObject.h"
#include "zzzcharacter.h"
#include "Zzzinfomation.h"
#include "NewUISystem.h"
#include "CShaderGL.h"
#include "CShaderScene.h"
#include "CameraProjection.h"
#include "RenderMatrix.h"


float Distance;
vec3_t CollisionPosition;
int     OpenglWindowX;
int     OpenglWindowY;
int     OpenglWindowWidth;
int     OpenglWindowHeight;
bool    CameraTopViewEnable = false;
float   CameraViewNear = 20.f;
float   CameraViewFar = 2000.f;
float   CameraFOV = 55.f;
vec3_t  CameraPosition;
vec3_t  CameraAngle;
float   CameraMatrix[3][4];
vec3_t  MousePosition;
vec3_t  MouseTarget;

// Phase 13.2: CPU copy of the current perspective projection, built alongside
// the fixed-function gluPerspective in gluPerspective2 (RenderMatrix backbone).
// The fixed-function matrix stays authoritative; this is a column-major float[16]
// mirror for future shader-fed (uProj) draws. Zero behavior change today.
float   g_ProjectionMatrix[16];

// Phase 13.3: CPU copy of the current camera view (modelview) matrix, built in
// BeginOpengl alongside the fixed-function glRotatef/glTranslatef camera setup.
// Column-major float[16] mirror for future shader-fed (uView) draws. The
// fixed-function MODELVIEW stays authoritative; zero behavior change today.
float   g_ViewMatrix[16];

// Phase 17.2: CPU copy of the UI's screen-space ortho projection, built in
// BeginBitmap alongside gluOrtho2D. Separate from g_ProjectionMatrix on purpose:
// that one is the perspective world camera, and the 2D UI never uses it.
float   g_UIProjectionMatrix[16];

// Phase 13.5: authoritative CPU mirror of the fixed-function MODELVIEW stack.
// Each fixed-function modelview op (glPushMatrix/glPopMatrix/glLoadIdentity/
// glRotatef/glTranslatef) is mirrored onto this stack at its call site so the
// current CPU modelview is available in a Core profile (no GL_MODELVIEW_MATRIX
// readback). Sites are migrated incrementally; the fixed-function stack stays
// authoritative until every site is covered and a consumer swaps to Top().
// g_ViewMatrix is kept as a derived snapshot of the world-camera Top().
RenderMatrix::Stack g_ModelViewStack;
float   g_fCameraCustomDistance = 0.f;

#ifdef SHADER_PIPELINE
// ===========================================================================
// Phase 16.3: shared Core-profile effect draw helper. Effects (ZzzEffect*,
// SideHair, ...) are pure fixed-function today and are scattered across many
// small draws interleaved with other fixed-function work, so - like the Phase
// 15 character path, and unlike the homogeneous terrain pass - each effect draw
// binds effect_core JUST for itself and restores the previous program, never a
// pass-level bind (that would put the following fixed-function draws under an
// explicit-attribute Core program and crash the driver, as the 15.4 shadow
// crash showed).
//
// Alpha BLEND stays fixed-function (glBlendFunc is program-independent), set by
// the caller as today; this helper only moves the geometry emission, the
// texture-env combine (uTexEnvMode) and the alpha-test discard (uAlphaRef) into
// the shader. CPU-side vertex data is uploaded unchanged.
//
// Opt-IN for now ('-gl33effect' or a 'gl33effect.enable' marker file): effects
// are the flicker-prone surface, so this phase stays behind a flag and is
// converted one effect family at a time until each is validated in the Release
// run, before it becomes default.
// ===========================================================================
bool GL33EffectEnabled()
{
	static int cached = -1;
	if (cached < 0)
	{
		const char* cmd = GetCommandLineA();
		bool on = (cmd != NULL && strstr(cmd, "-gl33effect") != NULL);
		if (!on && GetFileAttributesA("gl33effect.enable") != INVALID_FILE_ATTRIBUTES)
			on = true;
		cached = on ? 1 : 0;
	}
	return cached != 0;
}

namespace
{
	GLuint s_effectVao = 0;
	GLuint s_effectVboPos = 0, s_effectVboTex = 0, s_effectVboCol = 0;

	// Phase 16.9 note: see ZzzBMD.cpp - the capacity-orphan + glBufferSubData
	// streaming variant measured SLOWER than exact-size glBufferData on this
	// hardware, so both Core paths keep the simple form.

	bool EffectCoreEnsureBuffers()
	{
		if (s_effectVao != 0)
			return true;
		if (glGenVertexArrays == NULL)
			return false;
		glGenVertexArrays(1, &s_effectVao);
		glBindVertexArray(s_effectVao);
		glGenBuffers(1, &s_effectVboPos);
		glBindBuffer(GL_ARRAY_BUFFER, s_effectVboPos);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(0);
		glGenBuffers(1, &s_effectVboTex);
		glBindBuffer(GL_ARRAY_BUFFER, s_effectVboTex);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(1);
		glGenBuffers(1, &s_effectVboCol);
		glBindBuffer(GL_ARRAY_BUFFER, s_effectVboCol);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(2);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		return true;
	}
}

// Draw one effect primitive through effect_core. 'mode' is a GL primitive
// (GL_TRIANGLE_FAN, GL_TRIANGLES, ...); 'pos' is vec3/vertex, 'tex' vec2, 'col'
// vec4 (always provided - the caller fills it from the fixed-function current
// colour when the legacy draw had no per-vertex colour). texEnvMode is 0
// MODULATE / 1 ADD / 2 REPLACE; alphaRef < 0 disables the discard. uModelView is
// read back from GL_MODELVIEW_MATRIX so the caller's push/translate/rotate is
// honoured. Returns false when the Core effect path is off or unavailable, so
// the caller runs its untouched legacy immediate-mode draw.
bool EffectCoreDrawArrays(unsigned int mode, const float* pos, const float* tex,
	const float* col, int vertexCount, int texEnvMode, bool useTexture, float alphaRef)
{
	if (!GL33EffectEnabled() || vertexCount <= 0 || pos == NULL || tex == NULL || col == NULL)
		return false;
	if (gShaderScene.GetProgram(eShaderS_EffectCore) == 0)
		return false;
	if (!EffectCoreEnsureBuffers())
		return false;
	if (!gShaderScene.Use(eShaderS_EffectCore))
		return false;

	glBindVertexArray(s_effectVao);
	glBindBuffer(GL_ARRAY_BUFFER, s_effectVboPos);
	glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * (int)sizeof(float), pos, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, s_effectVboTex);
	glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * (int)sizeof(float), tex, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, s_effectVboCol);
	glBufferData(GL_ARRAY_BUFFER, vertexCount * 4 * (int)sizeof(float), col, GL_STREAM_DRAW);

	float modelView[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
	gShaderScene.SetMat4("uProj", g_ProjectionMatrix);
	gShaderScene.SetMat4("uModelView", modelView);
	gShaderScene.SetInt("texture1", 0);
	gShaderScene.SetInt("uTexEnvMode", texEnvMode);
	gShaderScene.SetInt("uUseTexture", useTexture ? 1 : 0);
	gShaderScene.SetFloat("uAlphaRef", alphaRef);

	glDrawArrays(mode, 0, vertexCount);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	gShaderScene.Unuse();

	static bool s_logged = false;
	if (!s_logged)
	{
		s_logged = true;
		g_ErrorReport.Write("> [Shader] Core effect path active (effect_core program %u, per-draw bind)\r\n",
			gShaderScene.GetProgram(eShaderS_EffectCore));
	}
	return true;
}

// ===========================================================================
// Phase 17: Core-profile 2D UI draw.
//
// Opt-IN for now ('-gl33ui' or a 'gl33ui.enable' marker file), like every other
// phase, so the UI can be compared against the fixed-function original.
// ===========================================================================
bool GL33UIEnabled()
{
	static int cached = -1;
	if (cached < 0)
	{
		const char* cmd = GetCommandLineA();
		bool on = (cmd != NULL && strstr(cmd, "-gl33ui") != NULL);
		if (!on && GetFileAttributesA("gl33ui.enable") != INVALID_FILE_ATTRIBUTES)
			on = true;
		cached = on ? 1 : 0;
	}
	return cached != 0;
}

namespace
{
	GLuint s_uiVao = 0;
	GLuint s_uiVboPos = 0, s_uiVboTex = 0;

	bool UICoreEnsureBuffers()
	{
		if (s_uiVao != 0)
			return true;
		if (glGenVertexArrays == NULL)
			return false;
		glGenVertexArrays(1, &s_uiVao);
		glBindVertexArray(s_uiVao);
		glGenBuffers(1, &s_uiVboPos);
		glBindBuffer(GL_ARRAY_BUFFER, s_uiVboPos);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(0);
		glGenBuffers(1, &s_uiVboTex);
		glBindBuffer(GL_ARRAY_BUFFER, s_uiVboTex);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(1);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		return true;
	}
}

// Draw one 2D UI primitive through ui_core. 'pos' is vec2 screen space, 'tex'
// vec2 (may be NULL for untextured draws). The colour is NOT per-vertex: the UI
// call sites set the fixed-function current colour once, so it is read back here
// into uColor - which also means this must be called AFTER the caller's
// glColor*. uProj comes from g_UIProjectionMatrix (the ortho mirror built in
// BeginBitmap), never from g_ProjectionMatrix. Blending stays fixed-function.
// Returns false when the Core UI path is off or unavailable, so the caller runs
// its untouched legacy draw.
bool UICoreDrawArrays(unsigned int mode, const float* pos, const float* tex,
	int vertexCount, bool useTexture, float alphaRef)
{
	if (!GL33UIEnabled() || vertexCount <= 0 || pos == NULL)
		return false;
	if (useTexture && tex == NULL)
		return false;
	if (gShaderScene.GetProgram(eShaderS_UICore) == 0)
		return false;
	if (!UICoreEnsureBuffers())
		return false;
	if (!gShaderScene.Use(eShaderS_UICore))
		return false;

	glBindVertexArray(s_uiVao);
	glBindBuffer(GL_ARRAY_BUFFER, s_uiVboPos);
	glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * (int)sizeof(float), pos, GL_STREAM_DRAW);
	if (tex != NULL)
	{
		glBindBuffer(GL_ARRAY_BUFFER, s_uiVboTex);
		glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * (int)sizeof(float), tex, GL_STREAM_DRAW);
	}

	float current[4] = { 1.f, 1.f, 1.f, 1.f };
	glGetFloatv(GL_CURRENT_COLOR, current);
	gShaderScene.SetMat4("uProj", g_UIProjectionMatrix);
	gShaderScene.SetVec4("uColor", current[0], current[1], current[2], current[3]);
	gShaderScene.SetInt("texture1", 0);
	gShaderScene.SetInt("uUseTexture", useTexture ? 1 : 0);
	gShaderScene.SetFloat("uAlphaRef", alphaRef);

	glDrawArrays(mode, 0, vertexCount);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	gShaderScene.Unuse();

	static bool s_logged = false;
	if (!s_logged)
	{
		s_logged = true;
		g_ErrorReport.Write("> [Shader] Core UI path active (ui_core program %u, per-draw bind)\r\n",
			gShaderScene.GetProgram(eShaderS_UICore));
	}
	return true;
}
#endif // SHADER_PIPELINE

// Phase 16.6: tracked mirror of the fixed-function texture-environment combine.
// A Core profile has no glTexEnv, so effect_core reproduces the combine in the
// fragment shader (uTexEnvMode 0=MODULATE / 1=ADD / 2=REPLACE). Every effect
// glTexEnvi site goes through this setter, so a Core effect draw knows which
// combine the legacy path would have used without a per-draw glGet readback
// (particles issue hundreds of draws a frame). The fixed-function call is still
// made here, so the legacy (Core-effect-off) path is byte-for-byte unchanged.
int g_EffectTexEnvMode = 0;

void SetEffectTexEnvMode(int mode)
{
	GLint glMode = GL_MODULATE;
	if (mode == 1)
		glMode = GL_ADD;
	else if (mode == 2)
		glMode = GL_REPLACE;
	else
		mode = 0;
	g_EffectTexEnvMode = mode;
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, glMode);
}

bool    FogEnable = false;
GLfloat FogDensity = 0.0004f;


int  CachTexture = -1;
bool TextureEnable;
bool DepthTestEnable;
bool CullFaceEnable;
bool DepthMaskEnable;
bool AlphaTestEnable;
int  AlphaBlendType;


unsigned int WindowWidth = 1024;
unsigned int WindowHeight = 768;
float MouseX = WindowWidth / 2;
float MouseY = WindowHeight / 2;
float BackMouseX = MouseX;
float BackMouseY = MouseY;
float MouseRenderX = WindowWidth / 2;
float MouseRenderY = WindowHeight / 2;

bool  MouseLButton;
bool  MouseLButtonPop;
bool  MouseLButtonPush;
bool  MouseRButton;
bool  MouseRButtonPop;
bool  MouseRButtonPush;
bool  MouseLButtonDBClick;
bool  MouseMButton;
bool  MouseMButtonPop;
bool  MouseMButtonPush;
int   MouseWheel;
DWORD MouseRButtonPress = 0;
GLfloat FogColor[4] = { 30 / 256.f, 20 / 256.f, 10 / 256.f, 256.f / 256.f };

//bool    showShoppingMall = false;

void OpenExploper(char* Name, char* para)
{
	ShellExecute(NULL, "open", Name, para, "", SW_SHOW);
}

bool CheckID_HistoryDay(char* Name, WORD day)
{
	typedef struct  __day_history__
	{
		char ID[MAX_ID_SIZE + 1];
		WORD date;
	}dayHistory;

	FILE* fp;
	dayHistory days[100] = {0,};
	int   count = 0;
	WORD  num = 0;
	bool  sameName = false;
	bool  update = true;

	if ((fp = fopen("dconfig.ini", "rb")) != NULL)
	{
		fread(&num, sizeof(WORD), 1, fp);

		if (num > 100)
		{
			num = 0;
		}
		else
		{
			for (int i = 0; i < num; ++i)
			{
				fread(days[i].ID, sizeof(char), MAX_ID_SIZE + 1, fp);
				fread(&days[i].date, sizeof(WORD), 1, fp);

				if (!strcmp(days[i].ID, Name))
				{
					sameName = true;
					if (days[i].date == day)
					{
						update = false;
						break;
					}
					days[i].date = day;
				}
				count++;
			}
		}
		fclose(fp);
	}

	if (update)
	{
		if (!sameName)
		{
			memcpy(days[num].ID, Name, (MAX_ID_SIZE + 1) * sizeof(char));
			days[num].date = day;

			num++;
		}

		fp = fopen("dconfig.ini", "wb");

		fwrite(&num, sizeof(WORD), 1, fp);
		for (int i = 0; i < num; ++i)
		{
			fwrite(days[i].ID, sizeof(char), MAX_ID_SIZE + 1, fp);
			fwrite(&days[i].date, sizeof(WORD), 1, fp);
		}

		fclose(fp);
	}

	//    showShoppingMall = update;

	return  update;
}

bool GrabEnable = false;
char GrabFileName[MAX_PATH];
int  GrabScreen = 0;
bool GrabFirst = false;

void SaveScreen()
{
	GrabFirst = true;

	const int w = (int)WindowWidth;
	const int h = (int)WindowHeight;
	if (w <= 0 || h <= 0)
		return;

	// Reset pixel-pack state that other render paths (Scaleform/ImGui/shaders) may
	// have left dirty. If a PBO is bound to GL_PIXEL_PACK_BUFFER, glReadPixels treats
	// the client pointer as an offset into that buffer and the driver crashes writing
	// into VRAM (access violation inside nvoglv32.dll). Unbinding it and forcing tight
	// packing makes the read safe for any resolution.
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glReadBuffer(GL_BACK);

	unsigned char* Buffer = new unsigned char[w * h * 3];
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, Buffer);
	WriteJpeg(GrabFileName, w, h, Buffer, 100);

	SAFE_DELETE_ARRAY(Buffer);

	GrabScreen++;
	GrabScreen %= 10000;
}

float PerspectiveX;
float PerspectiveY;
int   ScreenCenterX;
int   ScreenCenterY;
int   ScreenCenterYFlip;

void GetOpenGLMatrix(float Matrix[3][4])
{
	float OpenGLMatrix[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, OpenGLMatrix);
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			Matrix[i][j] = OpenGLMatrix[j * 4 + i];
		}
	}
}

void gluPerspective2(float Fov, float Aspect, float ZNear, float ZFar)
{
	gluPerspective(Fov, Aspect, ZNear, ZFar);

	// Phase 13.2: build a CPU copy of the same projection into g_ProjectionMatrix.
	// The fixed-function gluPerspective call above stays authoritative; this is an
	// additive mirror (column-major float[16]) for future shader-fed draws. No
	// existing behavior changes.
	RenderMatrix::Perspective(g_ProjectionMatrix, Fov, Aspect, ZNear, ZFar);

	ScreenCenterX = OpenglWindowX + OpenglWindowWidth / 2;
	ScreenCenterY = OpenglWindowY + OpenglWindowHeight / 2;
	ScreenCenterYFlip = WindowWidth - ScreenCenterY;

	float AspectY = (float)(WindowHeight) / (float)(OpenglWindowHeight);
	PerspectiveX = tanf(Fov * 0.5f * Q_PI / 180.f) / (float)(OpenglWindowWidth / 2) * Aspect;
	PerspectiveY = tanf(Fov * 0.5f * Q_PI / 180.f) / (float)(OpenglWindowHeight / 2) * AspectY;
}

void CreateScreenVector(int sx, int sy, vec3_t Target, bool bFixView, bool bFixScreen)
{
	vec3_t p1, p2;

	if (bFixScreen)
	{
		sx = ConvertNoX(sx);
		sy = ConvertNoY(sy);
	}
	else
	{
		sx = sx * g_fScreenRate_x;
		sy = sy * g_fScreenRate_y;
	}

	if (bFixView)
	{
		p1[0] = (float)(sx - ScreenCenterX) * CameraViewFar * PerspectiveX;
		p1[1] = -(float)(sy - ScreenCenterY) * CameraViewFar * PerspectiveY;
		p1[2] = -CameraViewFar;
	}
	else
	{
		p1[0] = (float)(sx - ScreenCenterX) * RENDER_ITEMVIEW_FAR * PerspectiveX;
		p1[1] = -(float)(sy - ScreenCenterY) * RENDER_ITEMVIEW_FAR * PerspectiveY;
		p1[2] = -RENDER_ITEMVIEW_FAR;
	}

	p2[0] = -CameraMatrix[0][3];
	p2[1] = -CameraMatrix[1][3];
	p2[2] = -CameraMatrix[2][3];
	VectorIRotate(p2, CameraMatrix, MousePosition);
	VectorIRotate(p1, CameraMatrix, p2);
	VectorAdd(MousePosition, p2, Target);
}

void Projection(vec3_t Position, int* sx, int* sy)
{
	vec3_t TrasformPosition;
	VectorTransform(Position, CameraMatrix, TrasformPosition);
	*sx = ScreenCenterX - (int)(TrasformPosition[0] / PerspectiveX / TrasformPosition[2]);
	*sy = ScreenCenterY + (int)(TrasformPosition[1] / PerspectiveY / TrasformPosition[2]);
	*sx = *sx / g_fScreenRate_x;
	*sy = *sy / g_fScreenRate_y;
}

void Projection2(vec3_t Position, int* sx, int* sy)
{
	if (!CameraProjection::WorldToScreen(Position, sx, sy))
	{
		if (sx != NULL)
			*sx = -10000;
		if (sy != NULL)
			*sy = -10000;
	}
}

void TransformPosition(vec3_t Position, vec3_t WorldPosition, int* x, int* y)
{
	vec3_t Temp;
	VectorSubtract(Position, CameraPosition, Temp);
	VectorRotate(Temp, CameraMatrix, WorldPosition);

	*x = (int)(WorldPosition[0] / PerspectiveX / -WorldPosition[2]) + (ScreenCenterX);
	*y = (int)(WorldPosition[1] / PerspectiveY / -WorldPosition[2]) + (ScreenCenterYFlip);
}

bool TestDepthBuffer(vec3_t Position)
{
	vec3_t WorldPosition;
	int x, y;
	TransformPosition(Position, WorldPosition, &x, &y);
	if (x < OpenglWindowX ||
		y < OpenglWindowY ||
		x >= (int)OpenglWindowX + OpenglWindowWidth ||
		y >= (int)OpenglWindowY + OpenglWindowHeight) return false;

	GLfloat key[3];
	glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, key);

	float z = 1.f - CameraViewNear / -WorldPosition[2] + CameraViewNear / CameraViewFar;
	if (key[0] >= z) return true;
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// opengl render util
///////////////////////////////////////////////////////////////////////////////

void BindTexture(int tex)
{
	if (CachTexture != tex)
	{
		CachTexture = tex;
		if (tex >= 0)
		{
			BITMAP_t* b = &Bitmaps[tex];
#ifdef SHADER_VERSION_TEST
			glActiveTexture(GL_TEXTURE0);  // Activamos la unidad de textura despu�s
#endif // SHADER_VERSION_TEST

			glBindTexture(GL_TEXTURE_2D, b->TextureNumber);
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, -1 * tex);
		}
	}
}

bool TextureStream = false;

extern  int test;

void BindTextureStream(int tex)
{
	if (CachTexture != tex)
	{
		CachTexture = tex;
		if (TextureStream)
			glEnd();
		BITMAP_t* b = &Bitmaps[tex];
		glBindTexture(GL_TEXTURE_2D, b->TextureNumber);

		glBegin(GL_TRIANGLES);
		TextureStream = true;
	}
}

void EndTextureStream()
{
	if (TextureStream)
		glEnd();
	TextureStream = false;
}

void EnableDepthTest()
{
	if (!DepthTestEnable)
	{
		DepthTestEnable = true;
		glEnable(GL_DEPTH_TEST);
	}
}

void DisableDepthTest()
{
	if (DepthTestEnable)
	{
		DepthTestEnable = false;
		glDisable(GL_DEPTH_TEST);
	}
}

void EnableDepthMask()
{
	if (!DepthMaskEnable)
	{
		DepthMaskEnable = true;
		glDepthMask(true);
	}
}

void DisableDepthMask()
{
	if (DepthMaskEnable)
	{
		DepthMaskEnable = false;
		glDepthMask(false);
	}
}

void EnableCullFace()
{
	if (!CullFaceEnable)
	{
		CullFaceEnable = true;
		glEnable(GL_CULL_FACE);
	}
}

void DisableCullFace()
{
	if (CullFaceEnable)
	{
		CullFaceEnable = false;
		glDisable(GL_CULL_FACE);
	}
}

// Phase 17: authoritative toggle for GL_TEXTURE_2D.
//
// 'TextureEnable' is meant to mirror the fixed-function texture enable, but 21
// call sites across the UI (UIControls, UIWindows, NewUIMessageBox, Sprite,
// CameraMove, ZzzInterface) called glEnable/glDisable(GL_TEXTURE_2D) directly
// and bypassed it, so the mirror could not be trusted. That is not a cosmetic
// problem: the Core UI draw has to know whether the legacy draw would have
// sampled a texture or emitted a flat colour, and a stale mirror turns name
// and chat backplates black or white depending on which way it is wrong.
//
// Deliberately NOT DisableTexture(): that one also forces the depth mask on and
// toggles the alpha test, so substituting it at these sites would change
// behaviour. This does exactly what the raw call did and keeps the mirror in
// step - the redundant-call skip is the only difference.
//
// Phase 18 removes GL_TEXTURE_2D entirely (it does not exist in Core), so
// routing every site through one function is a prerequisite either way.
void SetTextureEnabled(bool enable)
{
	if (TextureEnable == enable)
		return;
	TextureEnable = enable;
	if (enable)
		glEnable(GL_TEXTURE_2D);
	else
		glDisable(GL_TEXTURE_2D);
}

void DisableTexture(bool AlphaTest)
{
	EnableDepthMask();
	if (AlphaTest == true)
	{
		if (!AlphaTestEnable)
		{
			AlphaTestEnable = true;
			glEnable(GL_ALPHA_TEST);
		}
	}
	else
	{
		if (AlphaTestEnable)
		{
			AlphaTestEnable = false;
			glDisable(GL_ALPHA_TEST);
		}
	}
	if (TextureEnable)
	{
		TextureEnable = false;
		glDisable(GL_TEXTURE_2D);
	}
}

void DisableAlphaBlend()
{
	if (AlphaBlendType != 0)
	{
		AlphaBlendType = 0;
		glDisable(GL_BLEND);
	}
	EnableCullFace();
	EnableDepthMask();
	if (AlphaTestEnable)
	{
		AlphaTestEnable = false;
		glDisable(GL_ALPHA_TEST);
	}
	if (!TextureEnable)
	{
		TextureEnable = true;
		glEnable(GL_TEXTURE_2D);
	}
	if (FogEnable)
		glEnable(GL_FOG);
}

void EnableAlphaTest(bool DepthMask)
{
	if (AlphaBlendType != 2)
	{
		AlphaBlendType = 2;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	DisableCullFace();

	if (DepthMask)
		EnableDepthMask();

	if (!AlphaTestEnable)
	{
		AlphaTestEnable = true;
		glEnable(GL_ALPHA_TEST);
	}
	if (!TextureEnable)
	{
		TextureEnable = true;
		glEnable(GL_TEXTURE_2D);
	}
	if (FogEnable)
		glEnable(GL_FOG);
}

void EnableAlphaBlend()
{
	if (AlphaBlendType != 3)
	{
		AlphaBlendType = 3;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
	}
	DisableCullFace();
	DisableDepthMask();
	if (AlphaTestEnable)
	{
		AlphaTestEnable = false;
		glDisable(GL_ALPHA_TEST);
	}
	if (!TextureEnable)
	{
		TextureEnable = true;
		glEnable(GL_TEXTURE_2D);
	}
	if (FogEnable)
		glDisable(GL_FOG);
}

void EnableAlphaBlendMinus()
{
	if (AlphaBlendType != 4)
	{
		AlphaBlendType = 4;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	}
	DisableCullFace();
	DisableDepthMask();
	if (AlphaTestEnable)
	{
		AlphaTestEnable = false;
		glDisable(GL_ALPHA_TEST);
	}
	if (!TextureEnable)
	{
		TextureEnable = true;
		glEnable(GL_TEXTURE_2D);
	}
	if (FogEnable)
		glEnable(GL_FOG);
}

void EnableAlphaBlend2()
{
	if (AlphaBlendType != 5)
	{
		AlphaBlendType = 5;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE_MINUS_SRC_COLOR, GL_ONE);
	}
	DisableCullFace();
	DisableDepthMask();
	if (AlphaTestEnable)
	{
		AlphaTestEnable = false;
		glDisable(GL_ALPHA_TEST);
	}
	if (!TextureEnable)
	{
		TextureEnable = true;
		glEnable(GL_TEXTURE_2D);
	}
	if (FogEnable)
		glEnable(GL_FOG);
}

void EnableAlphaBlend3()
{
	if (AlphaBlendType != 6)
	{
		AlphaBlendType = 6;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	DisableCullFace();
	DisableDepthMask();
	if (AlphaTestEnable)
	{
		AlphaTestEnable = false;
		glDisable(GL_ALPHA_TEST);
	}
	if (!TextureEnable)
	{
		TextureEnable = true;
		glEnable(GL_TEXTURE_2D);
	}
	if (FogEnable)
		glEnable(GL_FOG);
}

void EnableAlphaBlend4()
{
	if (AlphaBlendType != 7)
	{
		AlphaBlendType = 7;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	}
	DisableCullFace();
	DisableDepthMask();
	if (AlphaTestEnable)
	{
		AlphaTestEnable = false;
		glDisable(GL_ALPHA_TEST);
	}
	if (!TextureEnable)
	{
		TextureEnable = true;
		glEnable(GL_TEXTURE_2D);
	}
	if (FogEnable)
		glEnable(GL_FOG);
}

void EnableLightMap()
{
	if (AlphaBlendType != 1)
	{
		AlphaBlendType = 1;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ZERO, GL_SRC_COLOR);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	EnableCullFace();
	EnableDepthMask();
	if (AlphaTestEnable)
	{
		AlphaTestEnable = false;
		glDisable(GL_ALPHA_TEST);
	}
	if (!TextureEnable)
	{
		TextureEnable = true;
		glEnable(GL_TEXTURE_2D);
	}
	if (FogEnable)
		glEnable(GL_FOG);
}

void glViewport2(int x, int y, int Width, int Height)
{
	OpenglWindowX = x;
	OpenglWindowY = y;
	OpenglWindowWidth = Width;
	OpenglWindowHeight = Height;
	glViewport(x, WindowHeight - (y + Height), Width, Height);
}

float ConvertX(float x)
{
	return x * g_fScreenRate_x;
}

float ConvertY(float y)
{
	return y * g_fScreenRate_y;
}

float ConvertNoX(float x)
{
	return (float)((double)WindowWidth * x / 640.0);
}

float ConvertNoY(float y)
{
	return (float)((double)WindowHeight * y / 480.0);
}

void BeginOpengl(int x, int y, int Width, int Height, bool Screen)
{
	if (Screen)
	{
		x = ConvertX(x);
		y = ConvertY(y);
		Width = ConvertX(Width);
		Height = ConvertY(Height);
	}
	else
	{
		x = ConvertNoX(x);
		y = ConvertNoY(y);
		Width = ConvertNoX(Width);
		Height = ConvertNoY(Height);
	}

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glViewport2(x, y, Width, Height);

	gluPerspective2(CameraFOV, (float)Width / (float)Height, CameraViewNear, CameraViewFar * 1.4f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glRotatef(CameraAngle[1], 0.f, 1.f, 0.f);
	if (CameraTopViewEnable == false)
		glRotatef(CameraAngle[0], 1.f, 0.f, 0.f);
	glRotatef(CameraAngle[2], 0.f, 0.f, 1.f);
	glTranslatef(-CameraPosition[0], -CameraPosition[1], -CameraPosition[2]);

	// Phase 13.5: mirror the same MODELVIEW ops onto the CPU stack, 1:1 with the
	// fixed-function calls above (glPushMatrix -> Push, glLoadIdentity -> Load-
	// Identity, glRotatef/glTranslatef -> Rotate/Translate, GL post-multiply
	// semantics). EndOpengl mirrors the matching glPopMatrix. g_ViewMatrix is kept
	// as a derived snapshot of the resulting world-camera view. The fixed-function
	// stack stays authoritative; additive, no behavior change (no consumer yet).
	g_ModelViewStack.Push();
	g_ModelViewStack.LoadIdentity();
	g_ModelViewStack.Rotate(CameraAngle[1], 0.f, 1.f, 0.f);
	if (CameraTopViewEnable == false)
		g_ModelViewStack.Rotate(CameraAngle[0], 1.f, 0.f, 0.f);
	g_ModelViewStack.Rotate(CameraAngle[2], 0.f, 0.f, 1.f);
	g_ModelViewStack.Translate(-CameraPosition[0], -CameraPosition[1], -CameraPosition[2]);
	RenderMatrix::Copy(g_ViewMatrix, g_ModelViewStack.Top());

	glDisable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthMask(true);
	AlphaTestEnable = false;
	TextureEnable = true;
	DepthTestEnable = true;
	CullFaceEnable = true;
	DepthMaskEnable = true;
	glDepthFunc(GL_LEQUAL);
	glAlphaFunc(GL_GREATER, 0.25f);

	if (FogEnable)
	{
		float color[] = { 20 / 256.f, 20 / 256.f, 20 / 256.f, 256.f / 256.f };

		glEnable(GL_FOG);
		glFogfv(GL_FOG_COLOR, color);
		glFogf(GL_FOG_DENSITY, FogDensity);

		glFogf(GL_FOG_MODE, GL_LINEAR);
		glFogf(GL_FOG_START, 2000.f);
		glFogf(GL_FOG_END, 2700.f);
	}
	else
	{
		glDisable(GL_FOG);
	}

	GetOpenGLMatrix(CameraMatrix);
}

void EndOpengl()
{
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	// Phase 13.5: mirror the MODELVIEW glPopMatrix above onto the CPU stack so it
	// stays balanced with the BeginOpengl Push (projection is not CPU-mirrored).
	g_ModelViewStack.Pop();
}

void UpdateMousePositionn()
{
	vec3_t vPos;

	glLoadIdentity();
	glTranslatef(-CameraPosition[0], -CameraPosition[1], -CameraPosition[2]);
	GetOpenGLMatrix(CameraMatrix);

	Vector(-CameraMatrix[0][3], -CameraMatrix[1][3], -CameraMatrix[2][3], vPos);
	VectorIRotate(vPos, CameraMatrix, MousePosition);
}

#ifdef LDS_ADD_MULTISAMPLEANTIALIASING
BOOL IsGLExtensionSupported(const char* extension)
{
	const size_t extlen = strlen(extension);
	const char* supported = NULL;

	// Try To Use wglGetExtensionStringARB On Current DC, If Possible
	PROC wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");

	if (wglGetExtString)
		supported = ((char* (__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC());

	// If That Failed, Try Standard Opengl Extensions String
	if (supported == NULL)
		supported = (char*)glGetString(GL_EXTENSIONS);

	// If That Failed Too, Must Be No Extensions Supported
	if (supported == NULL)
		return FALSE;

	// Begin Examination At Start Of String, Increment By 1 On False Match
	for (const char* p = supported; ; p++)
	{
		// Advance p Up To The Next Possible Match
		p = strstr(p, extension);

		if (p == NULL)
			return FALSE;															// No Match

		if ((p == supported || p[-1] == ' ') && (p[extlen] == '\0' || p[extlen] == ' '))
			return TRUE;															// Match
	}
}

BOOL InitGLMultisample(HINSTANCE hInstance, HWND hWnd, PIXELFORMATDESCRIPTOR pfd, int iRequestMSAAValue, int& OutiPixelFormat)
{
	BOOL bIsGLMultisampleSupported = FALSE;

#if defined(_DEBUG)
	CheckGLError(__FILE__, __LINE__);
#endif // defined(_DEBUG)

	// See If The String Exists In WGL!
	if (!IsGLExtensionSupported("WGL_ARB_multisample"))
	{
		bIsGLMultisampleSupported = FALSE;
		return FALSE;
	}

#if defined(_DEBUG)
	CheckGLError(__FILE__, __LINE__);
#endif // defined(_DEBUG)
	// Get Our Pixel Format
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	if (!wglChoosePixelFormatARB)
	{
		bIsGLMultisampleSupported = FALSE;
		return FALSE;
	}

#if defined(_DEBUG)
	CheckGLError(__FILE__, __LINE__);
#endif // defined(_DEBUG)

	// Get Our Current Device Context
	HDC hDC = GetDC(hWnd);

	int		valid;
	UINT	numFormats;
	float	fAttributes[] = { 0,0 };

	// These Attributes Are The Bits We Want To Test For In Our Sample
	// Everything Is Pretty Standard, The Only One We Want To 
	// Really Focus On Is The SAMPLE BUFFERS ARB And WGL SAMPLES
	// These Two Are Going To Do The Main Testing For Whether Or Not
	// We Support Multisampling On This Hardware.
	int iAttributes[] =
	{
		WGL_DRAW_TO_WINDOW_ARB,GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB,GL_TRUE,
			WGL_ACCELERATION_ARB,WGL_FULL_ACCELERATION_ARB,
			WGL_COLOR_BITS_ARB,24,
			WGL_ALPHA_BITS_ARB,8,
			WGL_DEPTH_BITS_ARB,16,
			WGL_STENCIL_BITS_ARB,0,
			WGL_DOUBLE_BUFFER_ARB,GL_TRUE,
			WGL_SAMPLE_BUFFERS_ARB,GL_TRUE,
			WGL_SAMPLES_ARB, iRequestMSAAValue,					// xN MultiSampling (N=4,2,1)
			0,0
	};

#if defined(_DEBUG)
	CheckGLError(__FILE__, __LINE__);
#endif // defined(_DEBUG)

	// First We Check To See If We Can Get A Pixel Format For 4 Samples
	valid = wglChoosePixelFormatARB(hDC, iAttributes, fAttributes, 1, &OutiPixelFormat, &numFormats);

#if defined(_DEBUG)
	CheckGLError(__FILE__, __LINE__);
#endif // defined(_DEBUG)

	// If We Returned True, And Our Format Count Is Greater Than 1
	if (valid && numFormats >= 1)
	{
		bIsGLMultisampleSupported = TRUE;
		return bIsGLMultisampleSupported;
	}

#if defined(_DEBUG)
	CheckGLError(__FILE__, __LINE__);
#endif // defined(_DEBUG)

	// Our Pixel Format With 4 Samples Failed, Test For 2 Samples
	iAttributes[19] = 2;
	valid = wglChoosePixelFormatARB(hDC, iAttributes, fAttributes, 1, &OutiPixelFormat, &numFormats);
	if (valid && numFormats >= 1)
	{
		bIsGLMultisampleSupported = TRUE;
		return bIsGLMultisampleSupported;
	}

#if defined(_DEBUG)
	CheckGLError(__FILE__, __LINE__);
#endif // defined(_DEBUG)

	// Return The Valid Format
	return  bIsGLMultisampleSupported;
}

void SetEnableMultisample()
{
	if (TRUE == g_bSupportedMSAA)
	{
		glEnable(GL_MULTISAMPLE_ARB);							// Enable Multisampling
	}

#if defined(_DEBUG)
	CheckGLError(__FILE__, __LINE__);
#endif // defined(_DEBUG)
}

void SetDisableMultisample()
{
	if (TRUE == g_bSupportedMSAA)
	{
		glDisable(GL_MULTISAMPLE_ARB);							// Enable Multisampling
	}

#if defined(_DEBUG)
	CheckGLError(__FILE__, __LINE__);
#endif // defined(_DEBUG)
}

#endif // LDS_ADD_MULTISAMPLEANTIALIASING

///////////////////////////////////////////////////////////////////////////////
// render util
///////////////////////////////////////////////////////////////////////////////

inline void TEXCOORD(float* c, float u, float v)
{
	c[0] = u;
	c[1] = v;
}

void RenderBox(float Matrix[3][4])
{
	vec3_t BoundingBoxMin;
	vec3_t BoundingBoxMax;
	Vector(-10.f, -30.f, -10.f, BoundingBoxMin);
	Vector(10.f, 0.f, 10.f, BoundingBoxMax);

	vec3_t BoundingVertices[8];
	Vector(BoundingBoxMax[0], BoundingBoxMax[1], BoundingBoxMax[2], BoundingVertices[0]);
	Vector(BoundingBoxMax[0], BoundingBoxMax[1], BoundingBoxMin[2], BoundingVertices[1]);
	Vector(BoundingBoxMax[0], BoundingBoxMin[1], BoundingBoxMax[2], BoundingVertices[2]);
	Vector(BoundingBoxMax[0], BoundingBoxMin[1], BoundingBoxMin[2], BoundingVertices[3]);
	Vector(BoundingBoxMin[0], BoundingBoxMax[1], BoundingBoxMax[2], BoundingVertices[4]);
	Vector(BoundingBoxMin[0], BoundingBoxMax[1], BoundingBoxMin[2], BoundingVertices[5]);
	Vector(BoundingBoxMin[0], BoundingBoxMin[1], BoundingBoxMax[2], BoundingVertices[6]);
	Vector(BoundingBoxMin[0], BoundingBoxMin[1], BoundingBoxMin[2], BoundingVertices[7]);

	vec3_t TransformVertices[8];
	for (int j = 0; j < 8; j++)
	{
		VectorTransform(BoundingVertices[j], Matrix, TransformVertices[j]);
	}

	glBegin(GL_QUADS);
	//glBegin(GL_LINES);
	glColor3f(0.2f, 0.2f, 0.2f);
	glTexCoord2f(1.0F, 1.0F);
	glVertex3fv(TransformVertices[7]);
	glTexCoord2f(1.0F, 0.0F);
	glVertex3fv(TransformVertices[6]);
	glTexCoord2f(0.0F, 0.0F);
	glVertex3fv(TransformVertices[4]);
	glTexCoord2f(0.0F, 1.0F);
	glVertex3fv(TransformVertices[5]);

	glColor3f(0.2f, 0.2f, 0.2f);
	glTexCoord2f(0.0F, 1.0F);
	glVertex3fv(TransformVertices[0]);
	glTexCoord2f(1.0F, 1.0F);
	glVertex3fv(TransformVertices[2]);
	glTexCoord2f(1.0F, 0.0F);
	glVertex3fv(TransformVertices[3]);
	glTexCoord2f(0.0F, 0.0F);
	glVertex3fv(TransformVertices[1]);

	glColor3f(0.6f, 0.6f, 0.6f);
	glTexCoord2f(1.0F, 1.0F);
	glVertex3fv(TransformVertices[7]);
	glTexCoord2f(1.0F, 0.0F);
	glVertex3fv(TransformVertices[3]);
	glTexCoord2f(0.0F, 0.0F);
	glVertex3fv(TransformVertices[2]);
	glTexCoord2f(0.0F, 1.0F);
	glVertex3fv(TransformVertices[6]);

	glColor3f(0.6f, 0.6f, 0.6f);
	glTexCoord2f(0.0F, 1.0F);
	glVertex3fv(TransformVertices[0]);
	glTexCoord2f(1.0F, 1.0F);
	glVertex3fv(TransformVertices[1]);
	glTexCoord2f(1.0F, 0.0F);
	glVertex3fv(TransformVertices[5]);
	glTexCoord2f(0.0F, 0.0F);
	glVertex3fv(TransformVertices[4]);

	glColor3f(0.4f, 0.4f, 0.4f);
	glTexCoord2f(1.0F, 1.0F);
	glVertex3fv(TransformVertices[7]);
	glTexCoord2f(1.0F, 0.0F);
	glVertex3fv(TransformVertices[5]);
	glTexCoord2f(0.0F, 0.0F);
	glVertex3fv(TransformVertices[1]);
	glTexCoord2f(0.0F, 1.0F);
	glVertex3fv(TransformVertices[3]);

	glColor3f(0.4f, 0.4f, 0.4f);
	glTexCoord2f(0.0F, 1.0F);
	glVertex3fv(TransformVertices[0]);
	glTexCoord2f(1.0F, 1.0F);
	glVertex3fv(TransformVertices[4]);
	glTexCoord2f(1.0F, 0.0F);
	glVertex3fv(TransformVertices[6]);
	glTexCoord2f(0.0F, 0.0F);
	glVertex3fv(TransformVertices[2]);
	glEnd();
}

void RenderPlane3D(float Width, float Height, float Matrix[3][4])
{
	vec3_t BoundingVertices[4];
	Vector(-Width, -Width, Height, BoundingVertices[3]);
	Vector(Width, Width, Height, BoundingVertices[2]);
	Vector(Width, Width, -Height, BoundingVertices[1]);
	Vector(-Width, -Width, -Height, BoundingVertices[0]);

	vec3_t TransformVertices[4];
	for (int j = 0; j < 4; j++)
	{
		VectorTransform(BoundingVertices[j], Matrix, TransformVertices[j]);
	}

	glBegin(GL_QUADS);
	glTexCoord2f(0.f, 1.f);
	glVertex3fv(TransformVertices[0]);
	glTexCoord2f(1.f, 1.f);
	glVertex3fv(TransformVertices[1]);
	glTexCoord2f(1.f, 0.f);
	glVertex3fv(TransformVertices[2]);
	glTexCoord2f(0.f, 0.f);
	glVertex3fv(TransformVertices[3]);
	glEnd();
}

void BeginSprite()
{
	glPushMatrix();
	glLoadIdentity();
}

void EndSprite()
{
	glPopMatrix();
}

void RenderSprite(int Texture, vec3_t Position, float Width, float Height, vec3_t Light, float Rotation, float u, float v, float uWidth, float vHeight)
{
	BindTexture(Texture);

	vec3_t p2;
	VectorTransform(Position, CameraMatrix, p2);
	//VectorCopy(Position,p2);
	float x = p2[0];
	float y = p2[1];
	float z = p2[2];

	Width *= 0.5f;
	Height *= 0.5f;

	vec3_t p[4];
	if (Rotation == 0)
	{
		Vector(x - Width, y - Height, z, p[0]);
		Vector(x + Width, y - Height, z, p[1]);
		Vector(x + Width, y + Height, z, p[2]);
		Vector(x - Width, y + Height, z, p[3]);
	}
	else
	{
		vec3_t p2[4];
		Vector(-Width, -Height, z, p2[0]);
		Vector(Width, -Height, z, p2[1]);
		Vector(Width, Height, z, p2[2]);
		Vector(-Width, Height, z, p2[3]);
		vec3_t Angle;
		Vector(0.f, 0.f, Rotation, Angle);
		float Matrix[3][4];
		AngleMatrix(Angle, Matrix);

		for (int i = 0; i < 4; i++)
		{
			VectorRotate(p2[i], Matrix, p[i]);
			p[i][0] += x;
			p[i][1] += y;
		}
	}

	float c[4][2];
	TEXCOORD(c[3], u, v);
	TEXCOORD(c[2], u + uWidth, v);
	TEXCOORD(c[1], u + uWidth, v + vHeight);
	TEXCOORD(c[0], u, v + vHeight);

	vec4_t colors[4];

	for (int i = 0; i < 4; i++)
	{
		VectorCopy(Light, colors[i]);
		if (Bitmaps[Texture].Components == 3)
		{
			colors[i][3] = 1.f;
		}
		else
		{
			if (Texture == BITMAP_BLOOD + 1 || Texture == BITMAP_FONT_HIT)
				colors[i][3] = 1.f;
			else
				colors[i][3] = Light[0];
		}
	}

#ifdef SHADER_PIPELINE
	// Phase 16.6: route the particle/sprite billboard through effect_core. This
	// one function is the draw for the whole particle family (skills, aura, fire,
	// sparks) plus the other sprite billboards. The legacy draw below is a
	// client-array GL_QUADS over exactly these 4 vertices; GL_TRIANGLE_FAN over
	// the same 4 in the same order triangulates identically (0,1,2 + 0,2,3), and
	// is Core-legal. uTexEnvMode carries the tracked glTexEnvi combine (this is
	// the first user of the GL_ADD additive path), uUseTexture mirrors the
	// fixed-function GL_TEXTURE_2D enable, and the alpha test / blend mode stay
	// fixed-function state owned by the caller.
	if (EffectCoreDrawArrays(GL_TRIANGLE_FAN, (const float*)p, (const float*)c,
		(const float*)colors, 4, g_EffectTexEnvMode, TextureEnable, -1.f))
		return;
#endif // SHADER_PIPELINE

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(3, GL_FLOAT, 0, p);
	glTexCoordPointer(2, GL_FLOAT, 0, c);
	glColorPointer(4, GL_FLOAT, 0, colors);

	glDrawArrays(GL_QUADS, 0, 4);  // 4 vertices en total

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);



	//glBegin(GL_QUADS);
	//if (Bitmaps[Texture].Components == 3)
	//{
	//	glColor3fv(Light);
	//}
	//else
	//{
	//	if (Texture == BITMAP_BLOOD + 1 || Texture == BITMAP_FONT_HIT)
	//		glColor4f(Light[0], Light[1], Light[2], 1.f);
	//	else
	//		glColor4f(Light[0], Light[1], Light[2], Light[0]);
	//}
	//for (int i = 0; i < 4; i++)
	//{
	//	glTexCoord2f(c[i][0], c[i][1]);
	//	glVertex3fv(p[i]);
	//}
	//glEnd();
}

void RenderSpriteUV(int Texture, vec3_t Position, float Width, float Height, float(*UV)[2], vec3_t Light[4], float Alpha)
{
	BindTexture(Texture);

	vec3_t p2;
	VectorTransform(Position, CameraMatrix, p2);
	float x = p2[0];
	float y = p2[1];
	float z = p2[2];

	Width *= 0.5f;
	Height *= 0.5f;
	vec3_t p[4];
	Vector(x - Width, y - Height, z, p[0]);
	Vector(x + Width, y - Height, z, p[1]);
	Vector(x + Width, y + Height, z, p[2]);
	Vector(x - Width, y + Height, z, p[3]);

#ifdef SHADER_PIPELINE
	// Phase 16.8: route the per-corner-lit world billboard (the floating damage /
	// experience numbers, via RenderNumber) through effect_core. Same 4 vertices
	// in the same order, so GL_TRIANGLE_FAN triangulates identically to the
	// GL_QUADS below (0,1,2 + 0,2,3) and is Core-legal.
	{
		float col[4 * 4];
		for (int i = 0; i < 4; i++)
		{
			col[i * 4 + 0] = Light[i][0];
			col[i * 4 + 1] = Light[i][1];
			col[i * 4 + 2] = Light[i][2];
			col[i * 4 + 3] = Alpha;
		}
		if (EffectCoreDrawArrays(GL_TRIANGLE_FAN, (const float*)p, (const float*)UV,
			col, 4, g_EffectTexEnvMode, TextureEnable, -1.f))
		{
			// The legacy loop below leaves the fixed-function current colour at the
			// last vertex colour, and later draws inherit it. The Core path sets no
			// glColor, so reproduce that trailing state explicitly.
			glColor4f(Light[3][0], Light[3][1], Light[3][2], Alpha);
			return;
		}
	}
#endif // SHADER_PIPELINE

	glBegin(GL_QUADS);
	for (int i = 0; i < 4; i++)
	{
		glColor4f(Light[i][0], Light[i][1], Light[i][2], Alpha);
		glTexCoord2f(UV[i][0], UV[i][1]);
		glVertex3fv(p[i]);
	}
	glEnd();
}

void RotateAngleNumber(float& X, float& Y, float Scale)
{
	const float Rad = 0.01745329f;
	float sinTh = sin((double)Rad * CameraAngle[2]);
	float cosTh = cos((double)Rad * CameraAngle[2]);

	X += Scale / 0.7071067f * cosTh / 2;
	Y -= Scale / 0.7071067f * sinTh / 2;
}

void RenderNumber(vec3_t Position, int Num, vec3_t Color, float Alpha, float Scale)
{
	vec3_t p;
	VectorCopy(Position, p);
	vec3_t Light[4];
	VectorCopy(Color, Light[0]);
	VectorCopy(Color, Light[1]);
	VectorCopy(Color, Light[2]);
	VectorCopy(Color, Light[3]);

	if (Num == -1)
	{
		float UV[4][2];
		TEXCOORD(UV[0], 0.f, 32.f / 32.f);
		TEXCOORD(UV[1], 32.f / 256.f, 32.f / 32.f);
		TEXCOORD(UV[2], 32.f / 256.f, 17.f / 32.f);
		TEXCOORD(UV[3], 0.f, 17.f / 32.f);
		RenderSpriteUV(BITMAP_FONT + 1, p, 45, 20, UV, Light, Alpha);
	}
	else if (Num == -2)
	{
		RenderSprite(BITMAP_FONT_HIT, p, 32 * Scale, 20 * Scale, Light[0], 0.f, 0.f, 0.f, 27.f / 32.f, 15.f / 16.f);
	}
	else
	{
		char Text[32];
		itoa(Num, Text, 10);
		p[0] -= strlen(Text) * 5.f;
		unsigned int Length = strlen(Text);
		p[0] -= Length * Scale * 0.125f;
		p[1] -= Length * Scale * 0.125f;
		for (unsigned int i = 0; i < Length; i++)
		{
			float UV[4][2];
			float u = (float)(Text[i] - 48) * 16.f / 256.f;
			TEXCOORD(UV[0], u, 16.f / 32.f);
			TEXCOORD(UV[1], u + 16.f / 256.f, 16.f / 32.f);
			TEXCOORD(UV[2], u + 16.f / 256.f, 0.f);
			TEXCOORD(UV[3], u, 0.f);
			RenderSpriteUV(BITMAP_FONT + 1, p, Scale, Scale, UV, Light, Alpha);
			RotateAngleNumber(p[0], p[1], Scale);
		}
	}
}

float RenderNumber2D(float x, float y, int Num, float Width, float Height)
{
	char Text[32];
	itoa(Num, Text, 10);
	int Length = (int)strlen(Text);
	x -= Width * Length / 2;
	for (int i = 0; i < Length; i++)
	{
		float u = (float)(Text[i] - 48) * 16.f / 256.f;
		//glColor3fv(Color);
		RenderBitmap(BITMAP_FONT + 1, x, y, Width, Height, u, 0.f, 16.f / 256.f, 16.f / 32.f);
		x += Width * 0.7f;
	}
	return x;
}

float RenderNumberHQ(float x, float y, int Num, float Width, float Height)
{
	char Text[32];
	memset(Text, 0, sizeof(Text));

	itoa(Num, Text, 10);

	for (int i = 0; i < (int)strlen(Text); i++)
	{
		float u = (float)(Text[i] - 48) * 36.f / 512.f;
		RenderBitmap(BITMAP_FONT_POWER, x, y, Width, Height, u, 0.f, 36.f / 512.f, 58.f / 64.f, true, true, 0.0);
		x += Width * 0.75f;
	}
	return x;
}

void BeginBitmap()
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	float aspectRatio = static_cast<float>(WindowWidth) / WindowHeight;

	glViewport(0, 0, WindowWidth, WindowHeight);
	gluPerspective(CameraFOV, aspectRatio, CameraViewNear, CameraViewFar);

	glLoadIdentity();
	gluOrtho2D(0, WindowWidth, 0, WindowHeight);

	// Phase 17.2: CPU mirror of the UI's ortho projection, built alongside the
	// fixed-function gluOrtho2D exactly as g_ProjectionMatrix mirrors
	// gluPerspective (Phase 13.2). The UI is screen-space, so a Core UI draw
	// must feed uProj from THIS, never from g_ProjectionMatrix - that one holds
	// the perspective camera and would place every widget off-screen. The
	// fixed-function matrix stays authoritative; nothing consumes this yet.
	RenderMatrix::Ortho(g_UIProjectionMatrix, 0.f, (float)WindowWidth,
		0.f, (float)WindowHeight, -1.f, 1.f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glLoadIdentity();
	DisableDepthTest();
}

void EndBitmap()
{
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

void RenderColor(float x, float y, float Width, float Height, float Alpha, int Flag, bool Scale)
{
	DisableTexture();

	if (Scale)
	{
		x = ConvertX(x);
		y = ConvertY(y);
		Width = ConvertX(Width);
		Height = ConvertY(Height);
	}

	float p[4][2];
	y = WindowHeight - y;

	p[0][0] = x; p[0][1] = y;
	p[1][0] = x; p[1][1] = y - Height;
	p[2][0] = x + Width; p[2][1] = y - Height;
	p[3][0] = x + Width; p[3][1] = y;

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		if (Alpha > 0.f)
		{
			if (Flag == 0)
				glColor4f(1.f, 1.f, 1.f, Alpha);
			else if (Flag == 1)
				glColor4f(0.f, 0.f, 0.f, Alpha);
		}
		glVertex2f(p[i][0], p[i][1]);
		if (Alpha > 0.f)
		{
			glColor4f(1.f, 1.f, 1.f, 1.f);
		}
	}
	glEnd();
}

void RenderNoColor(float x, float y, float Width, float Height, float Alpha, int Flag)
{
	DisableTexture();

	x = ConvertNoX(x);
	y = ConvertNoY(y);
	Width = ConvertNoX(Width);
	Height = ConvertNoY(Height);

	float p[4][2];
	y = WindowHeight - y;

	p[0][0] = x; p[0][1] = y;
	p[1][0] = x; p[1][1] = y - Height;
	p[2][0] = x + Width; p[2][1] = y - Height;
	p[3][0] = x + Width; p[3][1] = y;

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		if (Alpha > 0.f)
		{
			if (Flag == 0)
				glColor4f(1.f, 1.f, 1.f, Alpha);
			else if (Flag == 1)
				glColor4f(0.f, 0.f, 0.f, Alpha);
		}
		glVertex2f(p[i][0], p[i][1]);
		if (Alpha > 0.f)
		{
			glColor4f(1.f, 1.f, 1.f, 1.f);
		}
	}
	glEnd();
}

void EndRenderColor()
{
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
}

void RenderColorBitmap(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, unsigned int color)
{
	x = ConvertX(x);
	y = ConvertY(y);

	Width = ConvertX(Width);
	Height = ConvertY(Height);

	BindTexture(Texture);

	float p[4][2];

	y = WindowHeight - y;

	p[0][0] = x; p[0][1] = y;
	p[1][0] = x; p[1][1] = y - Height;
	p[2][0] = x + Width; p[2][1] = y - Height;
	p[3][0] = x + Width; p[3][1] = y;

	float c[4][2];
	TEXCOORD(c[0], u, v);
	TEXCOORD(c[3], u + uWidth, v);
	TEXCOORD(c[2], u + uWidth, v + vHeight);
	TEXCOORD(c[1], u, v + vHeight);

	/*glBegin(GL_TRIANGLE_FAN);

	for (int i = 0; i < 4; i++)
	{
		glColor4ub(static_cast<GLubyte>((color & 0xff)),         //Rad
			static_cast<GLubyte>((color >> 8) & 0xff),      //Green
			static_cast<GLubyte>((color >> 16) & 0xff),     //Blue
			static_cast<GLubyte>((color >> 24) & 0xff));   //Alpha

		glTexCoord2f(c[i][0], c[i][1]);
		glVertex2f(p[i][0], p[i][1]);

		glColor4f(1.f, 1.f, 1.f, 1.f);
	}
	glEnd();*/

	// Habilitar el uso de punteros de v�rtices y coordenadas de textura
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	// Pasar los datos de los v�rtices y las coordenadas de textura a OpenGL
	glVertexPointer(2, GL_FLOAT, 0, p);  // 2 componentes por v�rtice (x, y)
	glTexCoordPointer(2, GL_FLOAT, 0, c);  // 2 componentes por coordenada de textura (u, v)

	glColor4ub(static_cast<GLubyte>((color & 0xff)),         //Rad
			static_cast<GLubyte>((color >> 8) & 0xff),      //Green
			static_cast<GLubyte>((color >> 16) & 0xff),     //Blue
			static_cast<GLubyte>((color >> 24) & 0xff));   //Alpha

	// Dibujar los v�rtices como un cuadrado utilizando un tri�ngulo en abanico
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);  // 4 v�rtices en total

	// Restaurar el color si fue cambiado
	glColor4f(1.f, 1.f, 1.f, 1.f);  // Restaurar color blanco sin transparencia

	// Deshabilitar los estados de cliente
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RenderBitmap(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale, float Alpha)
{
	if (StartScale)
	{
		x = ConvertX(x);
		y = ConvertY(y);
	}
	if (Scale)
	{
		Width = ConvertX(Width);
		Height = ConvertY(Height);
	}

	BindTexture(Texture);

	float p[4][2];
	float c[4][2];

	y = WindowHeight - y;

	p[0][0] = x; p[0][1] = y;
	p[1][0] = x; p[1][1] = y - Height;
	p[2][0] = x + Width; p[2][1] = y - Height;
	p[3][0] = x + Width; p[3][1] = y;

	TEXCOORD(c[0], u, v);
	TEXCOORD(c[3], u + uWidth, v);
	TEXCOORD(c[2], u + uWidth, v + vHeight);
	TEXCOORD(c[1], u, v + vHeight);

	// Habilitar el uso de punteros de v�rtices y coordenadas de textura
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	// Pasar los datos de los v�rtices y las coordenadas de textura a OpenGL
	glVertexPointer(2, GL_FLOAT, 0, p);  // 2 componentes por v�rtice (x, y)
	glTexCoordPointer(2, GL_FLOAT, 0, c);  // 2 componentes por coordenada de textura (u, v)

	// Si hay transparencia (Alpha), habilita la mezcla de color
	if (Alpha > 0.f)
	{
		glColor4f(1.f, 1.f, 1.f, Alpha);  // Configurar color con Alpha
	}

	// Dibujar los v�rtices como un cuadrado utilizando un tri�ngulo en abanico
#ifdef SHADER_PIPELINE
	// Phase 17.4: first Core UI consumer. Same primitive, same 4 vertices, same
	// order - only the submission changes. Placed AFTER the glColor4f above,
	// because the helper reads the fixed-function current colour into uColor.
	// The client-array state enabled above is left as the legacy path left it
	// and is disabled below either way, so the fallback stays byte-identical.
	// useTexture mirrors the fixed-function GL_TEXTURE_2D enable, NOT a constant:
	// callers reach here after DisableTexture() for untextured widgets (the name
	// and chat backplates), where the legacy draw emits the flat current colour.
	// Sampling the bound texture there painted them black.
	if (!UICoreDrawArrays(GL_TRIANGLE_FAN, (const float*)p, (const float*)c, 4, TextureEnable, -1.f))
#endif // SHADER_PIPELINE
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);  // 4 v�rtices en total

	// Restaurar el color si fue cambiado
	if (Alpha > 0.f)
	{
		glColor4f(1.f, 1.f, 1.f, 1.f);  // Restaurar color blanco sin transparencia
	}

	// Deshabilitar los estados de cliente
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RenderNoBitmap(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight, bool Scale, bool StartScale, float Alpha)
{
	if (StartScale)
	{
		x = ConvertNoX(x);
		y = ConvertNoY(y);
	}
	if (Scale)
	{
		Width = ConvertNoX(Width);
		Height = ConvertNoY(Height);
	}

	BindTexture(Texture);

	float p[4][2];

	y = WindowHeight - y;

	p[0][0] = x; p[0][1] = y;
	p[1][0] = x; p[1][1] = y - Height;
	p[2][0] = x + Width; p[2][1] = y - Height;
	p[3][0] = x + Width; p[3][1] = y;

	float c[4][2];
	TEXCOORD(c[0], u, v);
	TEXCOORD(c[3], u + uWidth, v);
	TEXCOORD(c[2], u + uWidth, v + vHeight);
	TEXCOORD(c[1], u, v + vHeight);

	/*glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		if (Alpha > 0.f)
		{
			glColor4f(1.f, 1.f, 1.f, Alpha);
		}
		glTexCoord2f(c[i][0], c[i][1]);
		glVertex2f(p[i][0], p[i][1]);
		if (Alpha > 0.f)
		{
			glColor4f(1.f, 1.f, 1.f, 1.f);
		}
	}
	glEnd();*/

	// Habilitar el uso de punteros de v�rtices y coordenadas de textura
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	// Pasar los datos de los v�rtices y las coordenadas de textura a OpenGL
	glVertexPointer(2, GL_FLOAT, 0, p);  // 2 componentes por v�rtice (x, y)
	glTexCoordPointer(2, GL_FLOAT, 0, c);  // 2 componentes por coordenada de textura (u, v)

	// Si hay transparencia (Alpha), habilita la mezcla de color
	if (Alpha > 0.f)
	{
		glColor4f(1.f, 1.f, 1.f, Alpha);  // Configurar color con Alpha
	}

	// Dibujar los v�rtices como un cuadrado utilizando un tri�ngulo en abanico
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);  // 4 v�rtices en total

	// Restaurar el color si fue cambiado
	if (Alpha > 0.f)
	{
		glColor4f(1.f, 1.f, 1.f, 1.f);  // Restaurar color blanco sin transparencia
	}

	// Deshabilitar los estados de cliente
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RenderBitmapRotate(int Texture, float x, float y, float Width, float Height, float Rotate, float u, float v, float uWidth, float vHeight, bool Scale)
{
	if (Scale)
	{
		x = ConvertX(x);
		y = ConvertY(y);
		Width = ConvertX(Width);
		Height = ConvertY(Height);
	}

	BindTexture(Texture);

	vec3_t p[4], p2[4];

	y = WindowHeight - y;

	Vector(-Width * 0.5f, Height * 0.5f, 0.f, p[0]);
	Vector(-Width * 0.5f, -Height * 0.5f, 0.f, p[1]);
	Vector(Width * 0.5f, -Height * 0.5f, 0.f, p[2]);
	Vector(Width * 0.5f, Height * 0.5f, 0.f, p[3]);

	vec3_t Angle;
	Vector(0.f, 0.f, Rotate, Angle);
	float Matrix[3][4];
	AngleMatrix(Angle, Matrix);

	float c[4][2];
	TEXCOORD(c[0], u, v);
	TEXCOORD(c[3], u + uWidth, v);
	TEXCOORD(c[2], u + uWidth, v + vHeight);
	TEXCOORD(c[1], u, v + vHeight);

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		glTexCoord2f(c[i][0], c[i][1]);
		VectorRotate(p[i], Matrix, p2[i]);
		glVertex2f(p2[i][0] + x, p2[i][1] + y);
	}
	glEnd();
}

void RenderBitRotate(int Texture, float x, float y, float Width, float Height, float Rotate)
{
	x = ConvertX(x);
	y = ConvertY(y);
	Width = ConvertX(Width);
	Height = ConvertY(Height);

	BindTexture(Texture);

	vec3_t p[4], p2[4];

	y = Height - y;

	float cx = (Width / 2.f) - (Width - x);
	float cy = (Height / 2.f) - (Height - y);

	float ax = (-Width * 0.5f) + cx;
	float bx = (Width * 0.5f) + cx;
	float ay = (-Height * 0.5f) + cy;
	float by = (Height * 0.5f) + cy;

	Vector(ax, by, 0.f, p[0]);
	Vector(ax, ay, 0.f, p[1]);
	Vector(bx, ay, 0.f, p[2]);
	Vector(bx, by, 0.f, p[3]);

	vec3_t Angle;
	Vector(0.f, 0.f, Rotate, Angle);
	float Matrix[3][4];
	AngleMatrix(Angle, Matrix);

	float c[4][2];
	TEXCOORD(c[0], 0.f, 0.f);
	TEXCOORD(c[3], 1.f, 0.f);
	TEXCOORD(c[2], 1.f, 1.f);
	TEXCOORD(c[1], 0.f, 1.f);

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		glTexCoord2f(c[i][0], c[i][1]);
		VectorRotate(p[i], Matrix, p2[i]);
		glVertex2f(p2[i][0] + (WindowWidth / 2.f), p2[i][1] + (WindowHeight / 2.f));
	}
	glEnd();
}

void RenderPointRotate(int Texture, float ix, float iy, float iWidth, float iHeight, float x, float y, float Width, float Height, float Rotate, float Rotate_Loc, float uWidth, float vHeight, int Num)
{
	int i = 0;
	vec3_t p, p2[4], p3, p4[4], Angle;
	float c[4][2], Matrix[3][4];

	ix = ConvertX(ix);
	iy = ConvertY(iy);
	x = ConvertX(x);
	y = ConvertY(y);
	Width = ConvertX(Width);
	Height = ConvertY(Height);

	BindTexture(Texture);

	y = Height - y;
	iy = Height - iy;

	Vector((ix - (Width * 0.5f)) + ((Width / 2.f) - (Width - x)), (iy - (Height * 0.5f)) + ((Height / 2.f) - (Height - y)), 0.f, p);

	Vector(0.f, 0.f, Rotate, Angle);
	AngleMatrix(Angle, Matrix);

	VectorRotate(p, Matrix, p3);

	Vector(-(iWidth * 0.5f), (iHeight * 0.5f), 0.f, p2[0]);
	Vector(-(iWidth * 0.5f), -(iHeight * 0.5f), 0.f, p2[1]);
	Vector((iWidth * 0.5f), -(iHeight * 0.5f), 0.f, p2[2]);
	Vector((iWidth * 0.5f), (iHeight * 0.5f), 0.f, p2[3]);

	Vector(0.f, 0.f, Rotate_Loc, Angle);
	AngleMatrix(Angle, Matrix);

	TEXCOORD(c[0], 0.f, 0.f);
	TEXCOORD(c[3], uWidth, 0.f);
	TEXCOORD(c[2], uWidth, vHeight);
	TEXCOORD(c[1], 0.f, vHeight);

	glBegin(GL_TRIANGLE_FAN);
	for (i = 0; i < 4; i++)
	{
		glTexCoord2f(c[i][0], c[i][1]);

		Matrix[0][3] = p3[0] + 25;
		Matrix[1][3] = p3[1];
		VectorTransform(p2[i], Matrix, p4[i]);

		glVertex2f(p4[i][0] + (WindowWidth / 2.f), p4[i][1] + (WindowHeight / 2.f));
	}
	glEnd();

	if (Num > -1)
	{
		float dx, dy;
		dx = p4[0][0] + (WindowWidth / 2.f);
		dy = p4[0][1] + (WindowHeight / 2.f);
		dx = dx / g_fScreenRate_x;
		dy = dy / g_fScreenRate_y;
		g_pNewUIMiniMap->SetBtnPos(Num, dx, GetWindowsY - dy, iWidth / 2, iHeight / 2);
		if (Num >= 100)
			g_pNewUIMiniMap->SetBtnPos(Num - 100, dx - (iWidth / 2), (GetWindowsY - dy) - (iHeight / 2), iWidth, iHeight);
		else
			g_pNewUIMiniMap->SetBtnPos(Num, dx, GetWindowsY - dy, iWidth / 2, iHeight / 2);
	}
}

void RenderBitmapLocalRotate(int Texture, float x, float y, float Width, float Height, float Rotate, float u, float v, float uWidth, float vHeight)
{
	BindTexture(Texture);

	vec3_t p[4];
	x = ConvertX(x);
	y = ConvertY(y);
	y = WindowHeight - y;
	Width = ConvertX(Width);
	Height = ConvertY(Height);

	vec3_t vCenter, vDir;
	Vector(x, y, 0, vCenter);
	Vector(Width * 0.5f, -Height * 0.5f, 0, vDir);
	p[0][0] = vCenter[0] + (vDir[0]) * cosf(Rotate);
	p[0][1] = vCenter[1] + (vDir[1]) * sinf(Rotate);
	p[1][0] = vCenter[0] + (vDir[0]) * sinf(Rotate);
	p[1][1] = vCenter[1] - (vDir[1]) * cosf(Rotate);
	p[2][0] = vCenter[0] - (vDir[0]) * cosf(Rotate);
	p[2][1] = vCenter[1] - (vDir[1]) * sinf(Rotate);
	p[3][0] = vCenter[0] - (vDir[0]) * sinf(Rotate);
	p[3][1] = vCenter[1] + (vDir[1]) * cosf(Rotate);

	float c[4][2];
	TEXCOORD(c[0], u, v);
	TEXCOORD(c[3], u + uWidth, v);
	TEXCOORD(c[2], u + uWidth, v + vHeight);
	TEXCOORD(c[1], u, v + vHeight);

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		glTexCoord2f(c[i][0], c[i][1]);
		glVertex2f(p[i][0], p[i][1]);
	}
	glEnd();
}

void RenderNoBitmapLocalRotate(int Texture, float x, float y, float Width, float Height, float Rotate, float u, float v, float uWidth, float vHeight)
{
	BindTexture(Texture);

	vec3_t p[4];
	x = ConvertNoX(x);
	y = ConvertNoY(y);
	y = WindowHeight - y;
	Width = ConvertNoX(Width);
	Height = ConvertNoY(Height);

	vec3_t vCenter, vDir;
	Vector(x, y, 0, vCenter);
	Vector(Width * 0.5f, -Height * 0.5f, 0, vDir);
	p[0][0] = vCenter[0] + (vDir[0]) * cosf(Rotate);
	p[0][1] = vCenter[1] + (vDir[1]) * sinf(Rotate);
	p[1][0] = vCenter[0] + (vDir[0]) * sinf(Rotate);
	p[1][1] = vCenter[1] - (vDir[1]) * cosf(Rotate);
	p[2][0] = vCenter[0] - (vDir[0]) * cosf(Rotate);
	p[2][1] = vCenter[1] - (vDir[1]) * sinf(Rotate);
	p[3][0] = vCenter[0] - (vDir[0]) * sinf(Rotate);
	p[3][1] = vCenter[1] + (vDir[1]) * cosf(Rotate);

	float c[4][2];
	TEXCOORD(c[0], u, v);
	TEXCOORD(c[3], u + uWidth, v);
	TEXCOORD(c[2], u + uWidth, v + vHeight);
	TEXCOORD(c[1], u, v + vHeight);

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		glTexCoord2f(c[i][0], c[i][1]);
		glVertex2f(p[i][0], p[i][1]);
	}
	glEnd();
}

void RenderBitmapLocalRotate(int Texture, float x, float y, float Width, float Height, float Rotate, float u, float v, float uWidth, float vHeight, bool Scale)
{
	float Matrix[3][4];
	vec3_t Angle, p[4], p2[4];
	if (Scale)
	{
		x = ConvertX(x);
		y = ConvertY(y);
		Width = ConvertX(Width);
		Height = ConvertY(Height);
	}
	y = WindowHeight - y;

	BindTexture(Texture);

	float vertex[4][2];
	vertex[0][0] = x; vertex[0][1] = y;
	vertex[1][0] = x; vertex[1][1] = y - Height;
	vertex[2][0] = x + Width; vertex[2][1] = y - Height;
	vertex[3][0] = x + Width; vertex[3][1] = y;

	float textureWidth = uWidth * 0.500;
	float textureHeight = vHeight * 0.5;

	float PosX = u + textureWidth;
	float PosY = v + textureHeight;

	Vector(-textureWidth, -textureHeight, 0.f, p[0]);
	Vector(textureWidth, -textureHeight, 0.f, p[3]);
	Vector(textureWidth, textureHeight, 0.f, p[2]);
	Vector(-textureWidth, textureHeight, 0.f, p[1]);

	Vector(0, 0, Rotate, Angle);
	AngleMatrix(Angle, Matrix);

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		VectorRotate(p[i], Matrix, p2[i]);
		glTexCoord2f(p2[i][0] + PosX, p2[i][1] + PosY);
		glVertex2f(vertex[i][0], vertex[i][1]);
	}
	glEnd();
}

void RenderBitmapLocalRotate2(int Texture, float x, float y, float Width, float Height, float Rotate, float u, float v, float uWidth, float vHeight, bool Scale)
{
	float Matrix[3][4];
	vec3_t Angle, p[4], p2[4];

	if (Scale)
	{
		x = ConvertX(x);
		y = ConvertY(y);
		Width = ConvertX(Width);
		Height = ConvertY(Height);
	}
	y = WindowHeight - y;

	BindTexture(Texture);

	Vector(-Width * 0.5f, Height * 0.5f, 0.f, p[0]);
	Vector(-Width * 0.5f, -Height * 0.5f, 0.f, p[1]);
	Vector(Width * 0.5f, -Height * 0.5f, 0.f, p[2]);
	Vector(Width * 0.5f, Height * 0.5f, 0.f, p[3]);

	Vector(0, 0, Rotate, Angle);
	AngleMatrix(Angle, Matrix);

	float c[4][2];
	TEXCOORD(c[0], u, v);
	TEXCOORD(c[3], u + uWidth, v);
	TEXCOORD(c[2], u + uWidth, v + vHeight);
	TEXCOORD(c[1], u, v + vHeight);

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		glTexCoord2f(c[i][0], c[i][1]);
		VectorRotate(p[i], Matrix, p2[i]);
		glVertex2f(p2[i][0] + x, p2[i][1] + y);
	}
	glEnd();
}


void RenderBitmapLocalProjection(int Texture, float x, float y, float w, float h, vec3_t Angle, float su, float sv, float uw, float uh, bool Scale)
{
	float Matrix[3][4];
	vec3_t sp[4], vertex[4];

	if (Scale)
	{
		x = ConvertX(x);
		y = ConvertY(y);
		w = ConvertX(w);
		h = ConvertY(h);
	}

	y = WindowHeight - y;

	Vector(0.0, 0.0, 0.f, sp[0]);
	Vector(0.0, -h , 0.0, sp[1]);
	Vector(w  , -h , 0.0, sp[2]);
	Vector(w  , 0.0, 0.0, sp[3]);

	float coord2[4][2];
	coord2[0][0] = su;      coord2[0][1] = sv;
	coord2[1][0] = su;      coord2[1][1] = sv + uh;
	coord2[2][0] = su + uw; coord2[2][1] = sv + uh;
	coord2[3][0] = su + uw; coord2[3][1] = sv;

	AngleMatrix(Angle, Matrix);

	BindTexture(Texture);

	glBegin(GL_TRIANGLE_FAN);

	for (int n = 0; n < 4; n++)
	{
		VectorRotate(sp[n], Matrix, vertex[n]);
		glTexCoord2fv(coord2[n]);
		glVertex2f(vertex[n][0] + x, vertex[n][1] + y);
	}
	glEnd();
}

void RenderBitmapAlpha(int Texture, float sx, float sy, float Width, float Height)
{
	EnableAlphaTest();
	BindTexture(Texture);

	sy = WindowHeight - sy;
	for (int y = 0; y < 4; y++)
	{
		for (int x = 0; x < 4; x++)
		{
			float p[4][2];
			p[0][0] = sx + ((x)*Width) * 0.25f; p[0][1] = sy - ((y)*Height) * 0.25f;
			p[1][0] = sx + ((x)*Width) * 0.25f; p[1][1] = sy - ((y + 1) * Height) * 0.25f;
			p[2][0] = sx + ((x + 1) * Width) * 0.25f; p[2][1] = sy - ((y + 1) * Height) * 0.25f;
			p[3][0] = sx + ((x + 1) * Width) * 0.25f; p[3][1] = sy - ((y)*Height) * 0.25f;

			float c[4][2];
			TEXCOORD(c[0], (x) * 0.25f, (y) * 0.25f);
			TEXCOORD(c[1], (x) * 0.25f, (y + 1) * 0.25f);
			TEXCOORD(c[2], (x + 1) * 0.25f, (y + 1) * 0.25f);
			TEXCOORD(c[3], (x + 1) * 0.25f, (y) * 0.25f);

			float Alpha[4] = { 1.f,1.f,1.f,1.f };
			if (x == 0) { Alpha[0] = 0.f; Alpha[1] = 0.f; }
			if (x == 3) { Alpha[2] = 0.f; Alpha[3] = 0.f; }
			if (y == 0) { Alpha[0] = 0.f; Alpha[3] = 0.f; }
			if (y == 3) { Alpha[1] = 0.f; Alpha[2] = 0.f; }

			glBegin(GL_TRIANGLE_FAN);
			for (int i = 0; i < 4; i++)
			{
				glColor4f(1.f, 1.f, 1.f, Alpha[i]);
				glTexCoord2f(c[i][0], c[i][1]);
				glVertex2f(p[i][0], p[i][1]);
			}
			glEnd();
		}
	}
}

void RenderBitmapUV(int Texture, float x, float y, float Width, float Height, float u, float v, float uWidth, float vHeight)
{
	x = ConvertX(x);
	y = ConvertY(y);
	Width = ConvertX(Width);
	Height = ConvertY(Height);
	BindTexture(Texture);

	float p[4][2];
	y = WindowHeight - y;
	p[0][0] = x; p[0][1] = y;
	p[1][0] = x; p[1][1] = y - Height;
	p[2][0] = x + Width; p[2][1] = y - Height;
	p[3][0] = x + Width; p[3][1] = y;

	float c[4][2];
	TEXCOORD(c[0], u, v + vHeight * 0.25f);
	TEXCOORD(c[3], u + uWidth, v);
	TEXCOORD(c[2], u + uWidth, v + vHeight);
	TEXCOORD(c[1], u, v + vHeight - vHeight * 0.25f);

	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < 4; i++)
	{
		glTexCoord2f(c[i][0], c[i][1]);
		glVertex2f(p[i][0], p[i][1]);
	}
	glEnd();
}

///////////////////////////////////////////////////////////////////////////////
// collision detect util
///////////////////////////////////////////////////////////////////////////////

float absf(float a)
{
	if (a < 0.f) return -a;
	return a;
}

float minf(float a, float b)
{
	if (a > b)
		return b;
	return a;
}

float maxf(float a, float b)
{
	if (a > b) return a;
	return b;
}

int InsideTest(float x, float y, float z, int n, float* v1, float* v2, float* v3, float* v4, int flag, float type)
{
	if (type > 0.f)
		flag <<= 3;

	int i;
	vec3_t* vtx[4];
	vtx[0] = (vec3_t*)v1;
	vtx[1] = (vec3_t*)v2;
	vtx[2] = (vec3_t*)v3;
	vtx[3] = (vec3_t*)v4;

	int j = n - 1;
	switch (flag)
	{
	case 1:
		for (i = 0; i < n; j = i, i++)
		{
			float d = ((*vtx[i])[1] - y) * ((*vtx[j])[2] - z) - ((*vtx[j])[1] - y) * ((*vtx[i])[2] - z);
			if (d <= 0.f)
				return false;
		}
		break;
	case 2:
		for (i = 0; i < n; j = i, i++)
		{
			float d = ((*vtx[i])[2] - z) * ((*vtx[j])[0] - x) - ((*vtx[j])[2] - z) * ((*vtx[i])[0] - x);
			if (d <= 0.f)
				return false;
		}
		break;
	case 4:
		for (i = 0; i < n; j = i, i++)
		{
			float d = ((*vtx[i])[0] - x) * ((*vtx[j])[1] - y) - ((*vtx[j])[0] - x) * ((*vtx[i])[1] - y);
			if (d <= 0.f)
				return false;
		}
		break;
	case 8:
		for (i = 0; i < n; j = i, i++)
		{
			float d = ((*vtx[i])[1] - y) * ((*vtx[j])[2] - z) - ((*vtx[j])[1] - y) * ((*vtx[i])[2] - z);
			if (d >= 0.f)
				return false;
		}
		break;
	case 16:
		for (i = 0; i < n; j = i, i++)
		{
			float d = ((*vtx[i])[2] - z) * ((*vtx[j])[0] - x) - ((*vtx[j])[2] - z) * ((*vtx[i])[0] - x);
			if (d >= 0.f)
				return false;
		}
		break;
	case 32:
		for (i = 0; i < n; j = i, i++)
		{
			float d = ((*vtx[i])[0] - x) * ((*vtx[j])[1] - y) - ((*vtx[j])[0] - x) * ((*vtx[i])[1] - y);
			if (d >= 0.f)
				return false;
		}
		break;
	}

	return true;
}

void InitCollisionDetectLineToFace()
{
	Distance = 9999999.f;
}

bool CollisionDetectLineToFace(vec3_t Position, vec3_t Target, int Polygon, float* v1, float* v2, float* v3, float* v4, vec3_t Normal, bool Collision)
{
	vec3_t Direction;
	VectorSubtract(Target, Position, Direction);
	float a = DotProduct(Direction, Normal);
	if (a >= 0.f) return false;
	float b = DotProduct(Position, Normal) - DotProduct(v1, Normal);
	float t = -b / a;
	if (t >= 0.f && t <= Distance)
	{
		float X = Direction[0] * t + Position[0];
		float Y = Direction[1] * t + Position[1];
		float Z = Direction[2] * t + Position[2];
		int Count = 0;
		float MIN = minf(minf(absf(Direction[0]), absf(Direction[1])), absf(Direction[2]));
		if (MIN == absf(Direction[0]))
		{
			if ((Y >= minf(Position[1], Target[1]) && Y <= maxf(Position[1], Target[1])) &&
				(Z >= minf(Position[2], Target[2]) && Z <= maxf(Position[2], Target[2]))) Count++;
		}
		else if (MIN == absf(Direction[1]))
		{
			if ((Z >= minf(Position[2], Target[2]) && Z <= maxf(Position[2], Target[2])) &&
				(X >= minf(Position[0], Target[0]) && X <= maxf(Position[0], Target[0]))) Count++;
		}
		else
		{
			if ((X >= minf(Position[0], Target[0]) && X <= maxf(Position[0], Target[0])) &&
				(Y >= minf(Position[1], Target[1]) && Y <= maxf(Position[1], Target[1]))) Count++;
		}
		if (Count == 0) return false;
		Count = 0;
		if (Normal[0] <= -0.5f || Normal[0] >= 0.5f)
		{
			Count += InsideTest(X, Y, Z, Polygon, v1, v2, v3, v4, 1, Normal[0]);
		}
		else if (Normal[1] <= -0.5f || Normal[1] >= 0.5f)
		{
			Count += InsideTest(X, Y, Z, Polygon, v1, v2, v3, v4, 2, Normal[1]);
		}
		else
		{
			Count += InsideTest(X, Y, Z, Polygon, v1, v2, v3, v4, 4, Normal[2]);
		}
		if (Count == 0) return false;
		if (Collision)
		{
			Distance = t;
			Vector(X, Y, Z, CollisionPosition);
		}
		return true;
	}
	return false;
}

bool ProjectLineBox(vec3_t ax, vec3_t p1, vec3_t p2, OBB_t obb)
{
	float P1 = DotProduct(ax, p1);
	float P2 = DotProduct(ax, p2);

	float mx1 = maxf(P1, P2);
	float mn1 = minf(P1, P2);

	float ST = DotProduct(ax, obb.StartPos);
	float Q1 = DotProduct(ax, obb.XAxis);
	float Q2 = DotProduct(ax, obb.YAxis);
	float Q3 = DotProduct(ax, obb.ZAxis);

	float mx2 = ST;
	float mn2 = ST;

	if (Q1 > 0)	mx2 += Q1; else mn2 += Q1;
	if (Q2 > 0)	mx2 += Q2; else mn2 += Q2;
	if (Q3 > 0) mx2 += Q3; else mn2 += Q3;

	if (mn1 > mx2) return false;
	if (mn2 > mx1) return false;

	return true;
}

bool CollisionDetectLineToOBB(vec3_t p1, vec3_t p2, OBB_t obb)
{
	vec3_t e1;
	vec3_t eq11, eq12, eq13;

	VectorSubtract(p2, p1, e1);

	CrossProduct(e1, obb.XAxis, eq11);
	CrossProduct(e1, obb.YAxis, eq12);
	CrossProduct(e1, obb.ZAxis, eq13);

	if (!ProjectLineBox(eq11, p1, p2, obb)) return false;
	if (!ProjectLineBox(eq12, p1, p2, obb)) return false;
	if (!ProjectLineBox(eq13, p1, p2, obb)) return false;

	if (!ProjectLineBox(obb.XAxis, p1, p2, obb)) return false;
	if (!ProjectLineBox(obb.YAxis, p1, p2, obb)) return false;
	if (!ProjectLineBox(obb.ZAxis, p1, p2, obb)) return false;

	return true;
}

// Funci�n para desrotar un punto alrededor de otro
void CollisionDetectRotate(float centerX, float centerY, float angle, float& x, float& y)
{
	static float DEG_TO_RAD = (Q_PI / 180.0f);

	float rad = angle * DEG_TO_RAD;
	float cosA = cos(rad);
	float sinA = sin(rad);

	float translatedX = x - centerX;
	float translatedY = y - centerY;

	float rotatedX = translatedX * cosA + translatedY * sinA;
	float rotatedY = -translatedX * sinA + translatedY * cosA;

	x = rotatedX + centerX;
	y = rotatedY + centerY;
}

#ifdef V_SYNCRONIZE

#include "wglext.h"

bool _isVSyncEnabled = false;
bool _isVSyncAvailable = false;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;

namespace
{
	bool IsValidWGLProcAddress(PROC address)
	{
		const INT_PTR value = reinterpret_cast<INT_PTR>(address);
		return value != 0 && value != 1 && value != 2 && value != 3 && value != -1;
	}

	bool HasWGLExtension(const char* extensions, const char* extensionName)
	{
		if (extensions == nullptr || extensionName == nullptr || extensionName[0] == '\0' || strchr(extensionName, ' ') != nullptr)
		{
			return false;
		}

		const size_t extensionLength = strlen(extensionName);
		const char* match = extensions;
		while ((match = strstr(match, extensionName)) != nullptr)
		{
			const bool validStart = match == extensions || match[-1] == ' ';
			const char endCharacter = match[extensionLength];
			const bool validEnd = endCharacter == '\0' || endCharacter == ' ';
			if (validStart && validEnd)
			{
				return true;
			}
			match += extensionLength;
		}

		return false;
	}
}

bool WGLExtensionSupported(const char* extensionName)
{
	const char* extensions = nullptr;

	const PROC arbAddress = wglGetProcAddress("wglGetExtensionsStringARB");
	if (IsValidWGLProcAddress(arbAddress))
	{
		const PFNWGLGETEXTENSIONSSTRINGARBPROC getExtensionsStringARB =
			reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(arbAddress);
		extensions = getExtensionsStringARB(wglGetCurrentDC());
	}

	if (extensions == nullptr)
	{
		const PROC extAddress = wglGetProcAddress("wglGetExtensionsStringEXT");
		if (IsValidWGLProcAddress(extAddress))
		{
			const PFNWGLGETEXTENSIONSSTRINGEXTPROC getExtensionsStringEXT =
				reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGEXTPROC>(extAddress);
			extensions = getExtensionsStringEXT();
		}
	}

	return HasWGLExtension(extensions, extensionName);
}

void InitVSync()
{
	_isVSyncEnabled = false;
	_isVSyncAvailable = false;
	wglSwapIntervalEXT = nullptr;

	if (!WGLExtensionSupported("WGL_EXT_swap_control"))
	{
		return;
	}

	const PROC swapIntervalAddress = wglGetProcAddress("wglSwapIntervalEXT");
	if (!IsValidWGLProcAddress(swapIntervalAddress))
	{
		return;
	}

	wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(swapIntervalAddress);
	_isVSyncAvailable = true;
}

bool IsVSyncAvailable()
{
	return _isVSyncAvailable;
}

bool IsVSyncEnabled()
{
	return _isVSyncEnabled;
}

void EnableVSync()
{
	if (!_isVSyncAvailable || wglSwapIntervalEXT == nullptr)
	{
		_isVSyncEnabled = false;
		return;
	}

	_isVSyncEnabled = wglSwapIntervalEXT(1) == TRUE;
}

void DisableVSync()
{
	if (!_isVSyncAvailable || wglSwapIntervalEXT == nullptr)
	{
		_isVSyncEnabled = false;
		return;
	}

	const bool disabled = wglSwapIntervalEXT(0) == TRUE;
	_isVSyncEnabled = !disabled;
}

int GetFPSLimit()
{
	return g_hDC != nullptr ? GetDeviceCaps(g_hDC, VREFRESH) : 0;
}

#endif // V_SYNCRONIZE
