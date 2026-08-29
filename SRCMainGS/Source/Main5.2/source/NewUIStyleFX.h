// NewUIStyleFX.h: a small CSS-like drawing layer for the new-UI windows.
//
// The UI still renders through the fixed-function path, where per-vertex
// colours are free: gradients, rounded panels, glow and soft dividers cost the
// same as the flat quads RenderColor() already draws, and need no texture.
// Colours are DWORDs in the client's usual 0xAABBGGRR layout (RGBA()/ARGB()).
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <windows.h>

namespace SEASON3B
{
	//-- Colour helpers -------------------------------------------------------
	DWORD ColorAlpha(DWORD color, float fAlpha);		//. scales the existing alpha
	DWORD ColorLerp(DWORD from, DWORD to, float fT);
	DWORD ColorScale(DWORD color, float fScale);		//. brightens/darkens RGB

	//-- Fills ----------------------------------------------------------------
	void FillRect(float x, float y, float width, float height, DWORD color);
	void FillGradientV(float x, float y, float width, float height, DWORD top, DWORD bottom);
	void FillGradientH(float x, float y, float width, float height, DWORD left, DWORD right);

	void FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, DWORD color);

	//-- Rounded panels -------------------------------------------------------
	void FillRoundRect(float x, float y, float width, float height, float fRadius, DWORD color);
	void FillRoundRectV(float x, float y, float width, float height, float fRadius, DWORD top, DWORD bottom);
	void StrokeRoundRect(float x, float y, float width, float height, float fRadius, DWORD color, float fThickness = 1.f);

	//-- Effects --------------------------------------------------------------
	//. Additive halo around the rect, fading out over fSpread pixels.
	void DrawGlow(float x, float y, float width, float height, float fRadius, DWORD color, float fSpread);
	//. Divider that fades in from both ends, like a CSS transparent->colour->transparent gradient.
	void DrawSoftDivider(float x, float y, float width, float height, DWORD color);
	//. Drop shadow under a rounded panel.
	void DrawDropShadow(float x, float y, float width, float height, float fRadius, float fAlpha);

	//-- Skin helpers ---------------------------------------------------------
	//. Call before drawing a skin plate. Restores alpha blending, forces
	//. GL_TEXTURE_2D on (the TextureEnable mirror goes stale after text and
	//. untextured shapes) and drops the BindTexture cache so the next bind is
	//. real. Without this a plate can inherit the font texture and draw white.
	void BeginSkinDraw();

	//. Draws a plate whose left/right caps keep their pixel width while the
	//. middle stretches. fSrcV selects the row when the file is a state atlas.
	void RenderStretch3H(GLuint uiImage, float x, float y, float width, float height,
		float fSrcWidth, float fSrcHeight, float fCorner, float fSrcV = 0.f);

	//-- Framerate independent 0..1 transition, for hover/press states ---------
	class CUIFade
	{
		float m_fValue;
	public:
		CUIFade() : m_fValue(0.f) {}

		float Get() const { return m_fValue; }
		void Reset(float fValue = 0.f) { m_fValue = fValue; }

		//. Eases toward 1 while bOn, toward 0 otherwise. fSpeed is per second.
		float Update(bool bOn, float fSpeed = 10.f);
	};
}
