//*****************************************************************************
// File: UIMapName.cpp
// Map-entry announcement: one shared gold banner (LegendUI\WorldMap_I4C.ozt, 224x60)
// with the localized map name (gMapManager->GetMapName) rendered as text on top.
//*****************************************************************************

#include "stdafx.h"
#include "UIMapName.h"
#include "MapManager.h"
#include "ZzzOpenglUtil.h"
#include "ZzzTexture.h"
#include "NewUICommon.h"
#include "UIControls.h"

#include "UIWindows.h"
#ifdef ASG_ADD_GENS_SYSTEM
#include "ZzzInventory.h"
#endif	// ASG_ADD_GENS_SYSTEM

#define	UIMN_SHOW_TIME				5000
#define	UIMN_ALPHA_VARIATION		0.015f

// Banner size — fixed, same across all resolutions.
#define	UIMN_BANNER_WIDTH			150.0f
#define	UIMN_BANNER_HEIGHT			40.5f
#define	UIMN_BANNER_POS_Y_RATIO		0.52f
#define	UIMN_TEXT_HEIGHT			16.0f
#ifdef ASG_ADD_GENS_SYSTEM
#define UIMN_STRIFE_HEIGHT			24.0f
// MapNameAddStrife.tga is 256x32 with the art in the top-left 166x28 region.
#define UIMN_STRIFE_SRC_W			166.0f
#define UIMN_STRIFE_SRC_H			28.0f
#endif	// ASG_ADD_GENS_SYSTEM

CUIMapName::CUIMapName()
{
	m_bBannerLoaded = false;
}

CUIMapName::~CUIMapName()
{
}

void CUIMapName::Init()
{
	m_eState = HIDE;
	m_dwOldTime = ::timeGetTime();
	m_dwDeltaTickSum = 0;
	m_fAlpha = 1.0f;
#ifdef ASG_ADD_GENS_SYSTEM
	m_bStrife = false;
#endif	// ASG_ADD_GENS_SYSTEM
}

void CUIMapName::ShowMapName()
{
	if (gMapManager->currentMap == WD_40AREA_FOR_GM)
	{
		m_eState = HIDE;
		return;
	}

	m_eState = FADEIN;
	m_fAlpha = 0.2f;
	m_dwDeltaTickSum = 0;

	// One shared banner for every map - loaded once, never swapped per map.
	if (!m_bBannerLoaded)
	{
		LoadBitmap("Interface\\LegendUI\\WorldMap_I4C.tga", BITMAP_INTERFACE_EX + 45, GL_LINEAR);
		m_bBannerLoaded = true;
	}

#ifdef ASG_ADD_GENS_SYSTEM
	m_bStrife = ::IsStrifeMap(gMapManager->currentMap);
#endif	// ASG_ADD_GENS_SYSTEM
}

void CUIMapName::Update()
{
	DWORD dwNowTime = ::timeGetTime();
	DWORD dwDeltaTick = dwNowTime - m_dwOldTime;

	switch (m_eState)
	{
	case FADEIN:
		m_fAlpha += UIMN_ALPHA_VARIATION;
		if (1.0f <= m_fAlpha)
		{
			m_eState = SHOW;
			m_fAlpha = 1.0f;
		}
		break;

	case SHOW:
		m_dwDeltaTickSum += dwDeltaTick;
		if (m_dwDeltaTickSum > UIMN_SHOW_TIME)
		{
			m_eState = FADEOUT;
			m_dwDeltaTickSum = 0;
		}
		break;

	case FADEOUT:
		m_fAlpha -= UIMN_ALPHA_VARIATION;
		if (0.0f >= m_fAlpha)
		{
			m_eState = HIDE;
			m_fAlpha = 0.0f;
		}
		break;
	}

	m_dwOldTime = dwNowTime;
}

void CUIMapName::Render()
{
	Update();

	if (HIDE == m_eState)
		return;

	float fVirtW = (float)GetWindowsX;
	float fVirtH = (float)GetWindowsY;

	float fPosX = (fVirtW - UIMN_BANNER_WIDTH) / 2.0f;
	float fPosY = fVirtH * UIMN_BANNER_POS_Y_RATIO;

	::EnableAlphaTest();
	::glColor4f(1.0f, 1.0f, 1.0f, m_fAlpha);

#ifdef ASG_ADD_GENS_SYSTEM
	if (m_bStrife)
		SEASON3B::RenderImageF(BITMAP_INTERFACE_EX + 47, fPosX, fPosY - UIMN_STRIFE_HEIGHT, UIMN_BANNER_WIDTH, UIMN_STRIFE_HEIGHT, 0.0f, 0.0f, UIMN_STRIFE_SRC_W, UIMN_STRIFE_SRC_H);
#endif	// ASG_ADD_GENS_SYSTEM

	SEASON3B::RenderImageF(BITMAP_INTERFACE_EX + 45, fPosX, fPosY, UIMN_BANNER_WIDTH, UIMN_BANNER_HEIGHT);

	g_pRenderText->SetFont(g_hFontBig);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(255, 204, 100, (BYTE)(m_fAlpha * 255.0f));
	g_pRenderText->RenderFont((int)fPosX, (int)(fPosY + (UIMN_BANNER_HEIGHT - UIMN_TEXT_HEIGHT) / 2.0f), gMapManager->GetMapName(), (int)UIMN_BANNER_WIDTH, (int)UIMN_TEXT_HEIGHT, RT3_SORT_CENTER);

	::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	::DisableAlphaBlend();
}
