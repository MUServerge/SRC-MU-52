// NewUIStyleFX.cpp: implementation of the CSS-like drawing layer.
//
// Every primitive mirrors RenderColor() (ZzzOpenglUtil.cpp): screen units are
// converted with ConvertX/ConvertY and Y is flipped against WindowHeight, so
// these mix freely with the existing RenderImage/RenderColor calls.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIStyleFX.h"
#include "ZzzOpenglUtil.h"
#include "NewUICommon.h"
#include "steady_clock.h"
#include <math.h>

using namespace SEASON3B;

namespace
{
	const int ARC_SEGMENTS = 5;		//. per corner; 5 is smooth at UI radii and cheap

	inline BYTE ColorR(DWORD c) { return (BYTE)(c & 0xff); }
	inline BYTE ColorG(DWORD c) { return (BYTE)((c >> 8) & 0xff); }
	inline BYTE ColorB(DWORD c) { return (BYTE)((c >> 16) & 0xff); }
	inline BYTE ColorA(DWORD c) { return (BYTE)((c >> 24) & 0xff); }

	inline void ApplyColor(DWORD c)
	{
		glColor4ub(ColorR(c), ColorG(c), ColorB(c), ColorA(c));
	}

	inline float Clamp01(float f)
	{
		if (f < 0.f) return 0.f;
		if (f > 1.f) return 1.f;
		return f;
	}

	//-- Screen rect -> GL quad corners, same convention as RenderColor().
	struct SQuad
	{
		float left, right, top, bottom;
	};

	SQuad ToGL(float x, float y, float width, float height)
	{
		SQuad q;
		q.left = ConvertX(x);
		q.top = WindowHeight - ConvertY(y);
		q.right = q.left + ConvertX(width);
		q.bottom = q.top - ConvertY(height);
		return q;
	}

	void BeginShape()
	{
		DisableTexture();

		//. EndRenderColor() (and a few other sites) re-enable GL_TEXTURE_2D with a
		//. raw glEnable without updating the TextureEnable mirror, so DisableTexture
		//. can believe texturing is already off while it is not. Two untextured
		//. shapes in a row would then be modulated by whatever texture was bound.
		glDisable(GL_TEXTURE_2D);
	}

	void EndShape()
	{
		EndRenderColor();
	}

	//-- Outline of a rounded rect in GL space, walked clockwise from the top
	//-- left corner. Returns the number of points written.
	int BuildRoundRect(float x, float y, float width, float height, float fRadius, float(*pOut)[2])
	{
		if (fRadius < 0.f)
			fRadius = 0.f;

		const float fMax = ((width < height) ? width : height) * 0.5f;
		if (fRadius > fMax)
			fRadius = fMax;

		const SQuad q = ToGL(x, y, width, height);
		const float rx = ConvertX(fRadius);
		const float ry = ConvertY(fRadius);

		//. Corner centres walked in perimeter order - top-left, bottom-left,
		//. bottom-right, top-right - each arc starting where the previous one
		//. ended. Any other order makes the outline cross itself and the fan
		//. renders as a bow tie.
		const float cx[4] = { q.left + rx, q.left + rx, q.right - rx, q.right - rx };
		const float cy[4] = { q.top - ry, q.bottom + ry, q.bottom + ry, q.top - ry };
		const float fStart[4] = { 90.f, 180.f, 270.f, 0.f };

		int iCount = 0;
		for (int iCorner = 0; iCorner < 4; ++iCorner)
		{
			for (int i = 0; i <= ARC_SEGMENTS; ++i)
			{
				const float fAngle = (fStart[iCorner] + (90.f * i) / (float)ARC_SEGMENTS) * 3.14159265f / 180.f;
				pOut[iCount][0] = cx[iCorner] + cosf(fAngle) * rx;
				pOut[iCount][1] = cy[iCorner] + sinf(fAngle) * ry;
				++iCount;
			}
		}
		return iCount;
	}

	//-- Vertical gradient lookup for a point inside the shape.
	inline DWORD GradientAt(float fY, float fTop, float fBottom, DWORD top, DWORD bottom)
	{
		if (fTop == fBottom)
			return top;

		return SEASON3B::ColorLerp(top, bottom, Clamp01((fTop - fY) / (fTop - fBottom)));
	}
}

DWORD SEASON3B::ColorAlpha(DWORD color, float fAlpha)
{
	const float fA = (float)ColorA(color) * Clamp01(fAlpha);
	return (color & 0x00ffffff) | ((DWORD)(fA + 0.5f) << 24);
}

DWORD SEASON3B::ColorLerp(DWORD from, DWORD to, float fT)
{
	fT = Clamp01(fT);

	const DWORD r = (DWORD)(ColorR(from) + (ColorR(to) - ColorR(from)) * fT);
	const DWORD g = (DWORD)(ColorG(from) + (ColorG(to) - ColorG(from)) * fT);
	const DWORD b = (DWORD)(ColorB(from) + (ColorB(to) - ColorB(from)) * fT);
	const DWORD a = (DWORD)(ColorA(from) + (ColorA(to) - ColorA(from)) * fT);

	return r | (g << 8) | (b << 16) | (a << 24);
}

DWORD SEASON3B::ColorScale(DWORD color, float fScale)
{
	if (fScale < 0.f)
		fScale = 0.f;

	float r = (float)ColorR(color) * fScale;
	float g = (float)ColorG(color) * fScale;
	float b = (float)ColorB(color) * fScale;

	if (r > 255.f) r = 255.f;
	if (g > 255.f) g = 255.f;
	if (b > 255.f) b = 255.f;

	return (DWORD)r | ((DWORD)g << 8) | ((DWORD)b << 16) | ((DWORD)ColorA(color) << 24);
}

void SEASON3B::FillRect(float x, float y, float width, float height, DWORD color)
{
	FillGradientV(x, y, width, height, color, color);
}

void SEASON3B::FillGradientV(float x, float y, float width, float height, DWORD top, DWORD bottom)
{
	const SQuad q = ToGL(x, y, width, height);

	BeginShape();

	glBegin(GL_TRIANGLE_FAN);
	ApplyColor(top);
	glVertex2f(q.left, q.top);
	ApplyColor(bottom);
	glVertex2f(q.left, q.bottom);
	ApplyColor(bottom);
	glVertex2f(q.right, q.bottom);
	ApplyColor(top);
	glVertex2f(q.right, q.top);
	glEnd();

	EndShape();
}

void SEASON3B::FillGradientH(float x, float y, float width, float height, DWORD left, DWORD right)
{
	const SQuad q = ToGL(x, y, width, height);

	BeginShape();

	glBegin(GL_TRIANGLE_FAN);
	ApplyColor(left);
	glVertex2f(q.left, q.top);
	ApplyColor(left);
	glVertex2f(q.left, q.bottom);
	ApplyColor(right);
	glVertex2f(q.right, q.bottom);
	ApplyColor(right);
	glVertex2f(q.right, q.top);
	glEnd();

	EndShape();
}

void SEASON3B::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, DWORD color)
{
	BeginShape();

	glBegin(GL_TRIANGLES);
	ApplyColor(color);
	glVertex2f(ConvertX(x1), WindowHeight - ConvertY(y1));
	glVertex2f(ConvertX(x2), WindowHeight - ConvertY(y2));
	glVertex2f(ConvertX(x3), WindowHeight - ConvertY(y3));
	glEnd();

	EndShape();
}

void SEASON3B::FillRoundRect(float x, float y, float width, float height, float fRadius, DWORD color)
{
	FillRoundRectV(x, y, width, height, fRadius, color, color);
}

void SEASON3B::FillRoundRectV(float x, float y, float width, float height, float fRadius, DWORD top, DWORD bottom)
{
	float p[4 * (ARC_SEGMENTS + 1)][2];
	const int iCount = BuildRoundRect(x, y, width, height, fRadius, p);

	const SQuad q = ToGL(x, y, width, height);
	const float fMidY = (q.top + q.bottom) * 0.5f;

	BeginShape();

	glBegin(GL_TRIANGLE_FAN);
	ApplyColor(ColorLerp(top, bottom, 0.5f));
	glVertex2f((q.left + q.right) * 0.5f, fMidY);
	for (int i = 0; i < iCount; ++i)
	{
		ApplyColor(GradientAt(p[i][1], q.top, q.bottom, top, bottom));
		glVertex2f(p[i][0], p[i][1]);
	}
	ApplyColor(GradientAt(p[0][1], q.top, q.bottom, top, bottom));
	glVertex2f(p[0][0], p[0][1]);
	glEnd();

	EndShape();
}

void SEASON3B::StrokeRoundRect(float x, float y, float width, float height, float fRadius, DWORD color, float fThickness)
{
	float pOuter[4 * (ARC_SEGMENTS + 1)][2];
	float pInner[4 * (ARC_SEGMENTS + 1)][2];

	const int iCount = BuildRoundRect(x, y, width, height, fRadius, pOuter);
	BuildRoundRect(x + fThickness, y + fThickness,
		width - fThickness * 2.f, height - fThickness * 2.f,
		fRadius - fThickness, pInner);

	BeginShape();

	glBegin(GL_TRIANGLE_STRIP);
	ApplyColor(color);
	for (int i = 0; i < iCount; ++i)
	{
		glVertex2f(pOuter[i][0], pOuter[i][1]);
		glVertex2f(pInner[i][0], pInner[i][1]);
	}
	glVertex2f(pOuter[0][0], pOuter[0][1]);
	glVertex2f(pInner[0][0], pInner[0][1]);
	glEnd();

	EndShape();
}

void SEASON3B::DrawGlow(float x, float y, float width, float height, float fRadius, DWORD color, float fSpread)
{
	if (fSpread <= 0.f)
		return;

	EnableAlphaBlend();		//. additive, so the halo adds light instead of covering

	const int iRings = 5;
	for (int i = iRings; i >= 1; --i)
	{
		const float fT = (float)i / (float)iRings;
		const float fGrow = fSpread * fT;
		//. quadratic falloff reads closer to a real blur than a linear ramp
		const float fAlpha = (1.f - fT) * (1.f - fT) * 0.55f;

		StrokeRoundRect(x - fGrow, y - fGrow,
			width + fGrow * 2.f, height + fGrow * 2.f,
			fRadius + fGrow, ColorAlpha(color, fAlpha),
			fSpread / (float)iRings + 1.f);
	}

	EnableAlphaTest();
}

void SEASON3B::DrawSoftDivider(float x, float y, float width, float height, DWORD color)
{
	const DWORD clear = color & 0x00ffffff;
	const float fHalf = width * 0.5f;

	FillGradientH(x, y, fHalf, height, clear, color);
	FillGradientH(x + fHalf, y, fHalf, height, color, clear);
}

void SEASON3B::DrawDropShadow(float x, float y, float width, float height, float fRadius, float fAlpha)
{
	const int iRings = 4;
	for (int i = iRings; i >= 1; --i)
	{
		const float fGrow = (float)i * 2.f;
		const float fT = (float)i / (float)iRings;

		FillRoundRect(x - fGrow, y - fGrow + 2.f,
			width + fGrow * 2.f, height + fGrow * 2.f,
			fRadius + fGrow,
			((DWORD)((1.f - fT) * fAlpha * 255.f) << 24));
	}
}

void SEASON3B::BeginSkinDraw()
{
	EnableAlphaTest();

	TextureEnable = true;
	glEnable(GL_TEXTURE_2D);

	InvalidateTextureCache();

	glColor4f(1.f, 1.f, 1.f, 1.f);
}

void SEASON3B::RenderStretch3H(GLuint uiImage, float x, float y, float width, float height,
	float fSrcWidth, float fSrcHeight, float fCorner, float fSrcV)
{
	SEASON3B::BeginSkinDraw();

	if (fCorner * 2.f >= width || fCorner * 2.f >= fSrcWidth)
	{
		SEASON3B::RenderImageF(uiImage, x, y, width, height, 0.f, fSrcV, fSrcWidth, fSrcHeight);
		return;
	}

	const float fSrcMid = fSrcWidth - fCorner * 2.f;
	const float fMid = width - fCorner * 2.f;

	SEASON3B::RenderImageF(uiImage, x, y, fCorner, height,
		0.f, fSrcV, fCorner, fSrcHeight);

	SEASON3B::RenderImageF(uiImage, x + fCorner, y, fMid, height,
		fCorner, fSrcV, fSrcMid, fSrcHeight);

	SEASON3B::RenderImageF(uiImage, x + width - fCorner, y, fCorner, height,
		fSrcWidth - fCorner, fSrcV, fCorner, fSrcHeight);
}

float SEASON3B::CUIFade::Update(bool bOn, float fSpeed)
{
	const float fDelta = (float)gsteady_clock->GetFrameDeltaSeconds();
	const float fStep = Clamp01(fSpeed * fDelta);
	const float fTarget = bOn ? 1.f : 0.f;

	m_fValue += (fTarget - m_fValue) * fStep;

	if (m_fValue < 0.001f) m_fValue = 0.f;
	else if (m_fValue > 0.999f) m_fValue = 1.f;

	return m_fValue;
}
