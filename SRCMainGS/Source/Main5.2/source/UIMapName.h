//*****************************************************************************
// File: UIMapName.h
//
// producer: Ahn Sang-Kyu (2005. 6. 16)
//*****************************************************************************

#if !defined(AFX_UIMAPNAME_H__6771C771_B81D_4D5C_8484_63D6961ED6C0__INCLUDED_)
#define AFX_UIMAPNAME_H__6771C771_B81D_4D5C_8484_63D6961ED6C0__INCLUDED_

#pragma once

// Map-entry announcement (MuDream principle): ONE shared banner texture
// (Interface\LegendUI\WorldMap_I4C.ozt) + the localized map name rendered as TEXT
// on top of it - no more per-map name images (Local\<lang>\ImgsMapName\*.tga).
class CUIMapName
{
	enum SHOW_STATE { HIDE, FADEIN, SHOW, FADEOUT };

protected:
	bool		m_bBannerLoaded;
	SHOW_STATE	m_eState;
	DWORD		m_dwOldTime;
	DWORD		m_dwDeltaTickSum;
	float		m_fAlpha;
#ifdef ASG_ADD_GENS_SYSTEM
	bool		m_bStrife;
#endif

public:
	CUIMapName();
	virtual ~CUIMapName();

	void Init();
	void ShowMapName();
	void Update();
	void Render();
};

extern std::string g_strSelectedML;

#endif // !defined(AFX_UIMAPNAME_H__6771C771_B81D_4D5C_8484_63D6961ED6C0__INCLUDED_)
