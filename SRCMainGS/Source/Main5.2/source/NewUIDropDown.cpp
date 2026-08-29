// NewUIDropDown.cpp: implementation of the CNewUIDropDown class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIDropDown.h"
#include "NewUISystem.h"
#include "NewUIStyleFX.h"
#include "DSPlaySound.h"
#include <stdlib.h>

using namespace SEASON3B;

namespace
{
	const float ITEM_HEIGHT = 20.f;
	const float ARROW_WIDTH = 20.f;

	inline DWORD COL_BOX_TOP() { return RGBA(30, 38, 54, 245); }
	inline DWORD COL_BOX_BOTTOM() { return RGBA(16, 21, 32, 245); }
	inline DWORD COL_BORDER() { return RGBA(86, 116, 168, 190); }
	inline DWORD COL_BORDER_HOT() { return RGBA(120, 180, 255, 235); }
	inline DWORD COL_ARROW() { return RGBA(150, 190, 240, 255); }
	inline DWORD COL_ACCENT() { return RGBA(74, 152, 255, 255); }
	inline DWORD COL_TEXT() { return RGBA(214, 221, 234, 255); }
	inline DWORD COL_POPUP_TOP() { return RGBA(20, 26, 38, 250); }
	inline DWORD COL_POPUP_BOTTOM() { return RGBA(11, 14, 22, 252); }
}

SEASON3B::CNewUIDropDown::CNewUIDropDown()
{
	m_Pos.x = 0;
	m_Pos.y = 0;
	m_fWidth = 120.f;
	m_fHeight = 20.f;
	m_iMaxVisible = 6;
	m_iCurrent = -1;
	m_iScroll = 0;
	m_bOpen = false;
	m_iBoxImage = -1;
	m_fBoxSrcWidth = 0.f;
	m_fBoxSrcHeight = 0.f;
	m_fBoxCorner = 0.f;
	m_iArrowImage = -1;
	m_fArrowSrcSize = 0.f;
	m_iListImage = -1;
	m_fListSrcWidth = 0.f;
	m_fListSrcHeight = 0.f;
	m_iRowImage = -1;
	m_fRowSrcWidth = 0.f;
	m_fRowSrcHeight = 0.f;
	m_iScrollTrackImage = -1;
	m_fScrollTrackSrcWidth = 0.f;
	m_fScrollTrackSrcHeight = 0.f;
	m_iScrollThumbImage = -1;
	m_fScrollThumbSrcWidth = 0.f;
	m_fScrollThumbSrcHeight = 0.f;
	m_iScrollArrowImage = -1;
	m_fScrollArrowSrcSize = 0.f;
	m_fPopupWidth = 0.f;
}

void SEASON3B::CNewUIDropDown::SetPopupSkin(int iListImage, float fListSrcWidth, float fListSrcHeight,
	int iRowImage, float fRowSrcWidth, float fRowSrcHeight,
	int iScrollTrackImage, float fScrollTrackSrcWidth, float fScrollTrackSrcHeight,
	int iScrollThumbImage, float fScrollThumbSrcWidth, float fScrollThumbSrcHeight,
	int iScrollArrowImage, float fScrollArrowSrcSize, float fPopupWidth)
{
	m_iListImage = iListImage;
	m_fListSrcWidth = fListSrcWidth;
	m_fListSrcHeight = fListSrcHeight;
	m_iRowImage = iRowImage;
	m_fRowSrcWidth = fRowSrcWidth;
	m_fRowSrcHeight = fRowSrcHeight;
	m_iScrollTrackImage = iScrollTrackImage;
	m_fScrollTrackSrcWidth = fScrollTrackSrcWidth;
	m_fScrollTrackSrcHeight = fScrollTrackSrcHeight;
	m_iScrollThumbImage = iScrollThumbImage;
	m_fScrollThumbSrcWidth = fScrollThumbSrcWidth;
	m_fScrollThumbSrcHeight = fScrollThumbSrcHeight;
	m_iScrollArrowImage = iScrollArrowImage;
	m_fScrollArrowSrcSize = fScrollArrowSrcSize;
	m_fPopupWidth = fPopupWidth;
}

void SEASON3B::CNewUIDropDown::SetSkin(int iBoxImage, float fBoxSrcWidth, float fBoxSrcHeight, float fBoxCorner,
	int iArrowImage, float fArrowSrcSize)
{
	m_iBoxImage = iBoxImage;
	m_fBoxSrcWidth = fBoxSrcWidth;
	m_fBoxSrcHeight = fBoxSrcHeight;
	m_fBoxCorner = fBoxCorner;
	m_iArrowImage = iArrowImage;
	m_fArrowSrcSize = fArrowSrcSize;
}

SEASON3B::CNewUIDropDown::~CNewUIDropDown()
{
	m_TextList.clear();
}

void SEASON3B::CNewUIDropDown::Create(float x, float y, float width, float height, int iMaxVisible)
{
	m_Pos.x = (LONG)x;
	m_Pos.y = (LONG)y;
	m_fWidth = width;
	m_fHeight = height;
	m_iMaxVisible = (iMaxVisible > 0) ? iMaxVisible : 1;
	m_bOpen = false;
}

void SEASON3B::CNewUIDropDown::SetPosition(float x, float y)
{
	m_Pos.x = (LONG)x;
	m_Pos.y = (LONG)y;
}

void SEASON3B::CNewUIDropDown::Clear()
{
	m_TextList.clear();
	m_iCurrent = -1;
	m_iScroll = 0;
}

void SEASON3B::CNewUIDropDown::PushBack(const std::string& strText)
{
	m_TextList.push_back(strText);
}

void SEASON3B::CNewUIDropDown::SetCurrent(int iIndex)
{
	if (iIndex >= 0 && iIndex < (int)m_TextList.size())
		m_iCurrent = iIndex;
	else
		m_iCurrent = -1;
}

std::string SEASON3B::CNewUIDropDown::AsString() const
{
	if (m_iCurrent >= 0 && m_iCurrent < (int)m_TextList.size())
		return m_TextList[m_iCurrent];

	return "";
}

int SEASON3B::CNewUIDropDown::AsInteger() const
{
	if (m_iCurrent >= 0 && m_iCurrent < (int)m_TextList.size())
		return atoi(m_TextList[m_iCurrent].c_str());

	return 0;
}

void SEASON3B::CNewUIDropDown::Close()
{
	m_bOpen = false;
}

float SEASON3B::CNewUIDropDown::GetPopupHeight() const
{
	int iVisible = (int)m_TextList.size();
	if (iVisible > m_iMaxVisible)
		iVisible = m_iMaxVisible;

	return (float)iVisible * ITEM_HEIGHT + 4.f;
}

float SEASON3B::CNewUIDropDown::GetPopupWidth() const
{
	return (m_fPopupWidth > 0.f) ? m_fPopupWidth : m_fWidth;
}

float SEASON3B::CNewUIDropDown::GetPopupX() const
{
	return (float)m_Pos.x + (m_fWidth - GetPopupWidth()) * 0.5f;
}

void SEASON3B::CNewUIDropDown::ClampScroll()
{
	const int iMax = (int)m_TextList.size() - m_iMaxVisible;

	if (iMax <= 0)
		m_iScroll = 0;
	else if (m_iScroll < 0)
		m_iScroll = 0;
	else if (m_iScroll > iMax)
		m_iScroll = iMax;
}

int SEASON3B::CNewUIDropDown::GetHoverIndex() const
{
	if (!m_bOpen)
		return -1;

	const float fTop = (float)m_Pos.y + m_fHeight + 2.f;

	int iVisible = (int)m_TextList.size();
	if (iVisible > m_iMaxVisible)
		iVisible = m_iMaxVisible;

	for (int i = 0; i < iVisible; ++i)
	{
		if (SEASON3B::CheckMouseIn(GetPopupX(), fTop + 2.f + (float)i * ITEM_HEIGHT, GetPopupWidth(), ITEM_HEIGHT))
			return m_iScroll + i;
	}

	return -1;
}

bool SEASON3B::CNewUIDropDown::IsMouseOver() const
{
	if (SEASON3B::CheckMouseIn((float)m_Pos.x, (float)m_Pos.y, m_fWidth, m_fHeight))
		return true;

	if (m_bOpen && SEASON3B::CheckMouseIn(GetPopupX(), (float)m_Pos.y + m_fHeight + 2.f, GetPopupWidth(), GetPopupHeight()))
		return true;

	return false;
}

bool SEASON3B::CNewUIDropDown::UpdateMouseEvent()
{
	const bool bOverBox = SEASON3B::CheckMouseIn((float)m_Pos.x, (float)m_Pos.y, m_fWidth, m_fHeight) != 0;

	m_BoxFade.Update(bOverBox || m_bOpen);

	if (m_bOpen)
	{
		const bool bOverList = SEASON3B::CheckMouseIn(GetPopupX(), (float)m_Pos.y + m_fHeight + 2.f,
			GetPopupWidth(), GetPopupHeight()) != 0;

		if (bOverList && MouseWheel != 0)
		{
			m_iScroll += (MouseWheel > 0) ? -1 : 1;
			MouseWheel = 0;
			ClampScroll();
		}

		if ((int)m_TextList.size() > m_iMaxVisible && SEASON3B::IsPress(VK_LBUTTON))
		{
			const float fListTop = (float)m_Pos.y + m_fHeight + 2.f;
			const float fArrowX = GetPopupX() + GetPopupWidth() - 14.f;
			if (SEASON3B::CheckMouseIn(fArrowX, fListTop + 2.f, 12.f, 12.f))
			{
				--m_iScroll;
				ClampScroll();
				PlayBuffer(SOUND_CLICK01);
				return false;
			}
			if (SEASON3B::CheckMouseIn(fArrowX, fListTop + GetPopupHeight() - 14.f, 12.f, 12.f))
			{
				++m_iScroll;
				ClampScroll();
				PlayBuffer(SOUND_CLICK01);
				return false;
			}
		}

		if (SEASON3B::IsPress(VK_LBUTTON))
		{
			if (bOverBox)
			{
				m_bOpen = false;
				PlayBuffer(SOUND_CLICK01);
				return false;
			}

			if (!bOverList)
			{
				m_bOpen = false;
				return false;
			}

			const int iHover = GetHoverIndex();
			if (iHover >= 0 && iHover < (int)m_TextList.size())
			{
				const bool bChanged = (iHover != m_iCurrent);
				m_iCurrent = iHover;
				m_bOpen = false;
				PlayBuffer(SOUND_CLICK01);
				return bChanged;
			}
		}

		return false;
	}

	if (bOverBox && SEASON3B::IsPress(VK_LBUTTON))
	{
		m_bOpen = true;
		m_iScroll = (m_iCurrent > 0) ? (m_iCurrent - m_iMaxVisible / 2) : 0;
		ClampScroll();
		PlayBuffer(SOUND_CLICK01);
	}

	return false;
}

void SEASON3B::CNewUIDropDown::Render()
{
	const float x = (float)m_Pos.x;
	const float y = (float)m_Pos.y;
	const float fHot = m_BoxFade.Get();

	const float fArrowX = x + m_fWidth - ARROW_WIDTH;

	if (m_iBoxImage >= 0)
	{
		SEASON3B::BeginSkinDraw();
		SEASON3B::RenderStretch3H(m_iBoxImage, x, y, m_fWidth, m_fHeight,
			m_fBoxSrcWidth, m_fBoxSrcHeight, m_fBoxCorner);

	}
	else if (m_iArrowImage == -1)
	{
		SEASON3B::FillRoundRectV(x, y, m_fWidth, m_fHeight, 3.f, COL_BOX_TOP(), COL_BOX_BOTTOM());
		SEASON3B::StrokeRoundRect(x, y, m_fWidth, m_fHeight, 3.f,
			SEASON3B::ColorLerp(COL_BORDER(), COL_BORDER_HOT(), fHot), 1.f);
	}

	//-- Arrow cell on the right, like the reference client.
	if (m_iArrowImage >= 0)
	{
		const float fSize = m_fHeight - 4.f;

		SEASON3B::BeginSkinDraw();
		SEASON3B::RenderImageF(m_iArrowImage, fArrowX + (ARROW_WIDTH - fSize) * 0.5f, y + 2.f, fSize, fSize,
			0.f, (m_bOpen ? 2.f : (fHot > 0.5f ? 1.f : 0.f)) * m_fArrowSrcSize,
			m_fArrowSrcSize, m_fArrowSrcSize);
	}
	else if (m_iArrowImage == -1)
	{
		const float fCx = fArrowX + (ARROW_WIDTH - 1.f) * 0.5f;
		const float fCy = y + m_fHeight * 0.5f;

		if (m_bOpen)
			SEASON3B::FillTriangle(fCx - 4.f, fCy + 2.f, fCx + 4.f, fCy + 2.f, fCx, fCy - 3.f,
				SEASON3B::ColorLerp(COL_ARROW(), RGBA(255, 255, 255, 255), fHot));
		else
			SEASON3B::FillTriangle(fCx - 4.f, fCy - 2.f, fCx + 4.f, fCy - 2.f, fCx, fCy + 3.f,
				SEASON3B::ColorLerp(COL_ARROW(), RGBA(255, 255, 255, 255), fHot));
	}

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(SEASON3B::ColorLerp(RGBA(224, 211, 174, 255), RGBA(238, 255, 220, 255), fHot));
	g_pRenderText->RenderText((int)(x + 6.f), (int)(y + 3.f), AsString().c_str(), (int)(m_fWidth - 28.f), 0, 3);
}

void SEASON3B::CNewUIDropDown::RenderPopup()
{
	if (!m_bOpen)
		return;

	const float x = GetPopupX();
	const float y = (float)m_Pos.y + m_fHeight + 2.f;
	const float fHeight = GetPopupHeight();
	const float fPopupWidth = GetPopupWidth();

	SEASON3B::DrawDropShadow(x, y, fPopupWidth, fHeight, 4.f, 0.5f);

	if (m_iListImage >= 0)
	{
		SEASON3B::BeginSkinDraw();
		SEASON3B::RenderImageF(m_iListImage, x, y, fPopupWidth, fHeight,
			0.f, 0.f, m_fListSrcWidth, m_fListSrcHeight);
	}
	else
	{
		SEASON3B::FillRoundRectV(x, y, fPopupWidth, fHeight, 4.f, COL_POPUP_TOP(), COL_POPUP_BOTTOM());
	}

	int iVisible = (int)m_TextList.size();
	if (iVisible > m_iMaxVisible)
		iVisible = m_iMaxVisible;

	const int iHover = GetHoverIndex();

	for (int i = 0; i < iVisible; ++i)
	{
		const int iIndex = m_iScroll + i;
		if (iIndex < 0 || iIndex >= (int)m_TextList.size())
			continue;

		const float iy = y + 2.f + (float)i * ITEM_HEIGHT;
		int iState = 0;
		if (iIndex == m_iCurrent)
			iState = 2;
		else if (iIndex == iHover)
			iState = 1;

		if (iState != 0)
		{
			SEASON3B::FillRoundRectV(x + 4.f, iy + 1.f, fPopupWidth - 10.f, ITEM_HEIGHT - 2.f, 1.f,
				SEASON3B::ColorAlpha(RGBA(62, 132, 70, 255), iState == 2 ? 0.42f : 0.20f),
				SEASON3B::ColorAlpha(RGBA(25, 62, 42, 255), iState == 2 ? 0.32f : 0.10f));
		}

		g_pRenderText->SetFont(g_hFont);
		g_pRenderText->SetBgColor(0);
		g_pRenderText->SetTextColor((iIndex == m_iCurrent || iIndex == iHover)
			? RGBA(230, 255, 190, 255) : RGBA(224, 211, 174, 255));
		g_pRenderText->RenderText((int)(x + 4.f), (int)(iy + 3.f), m_TextList[iIndex].c_str(),
			(int)(fPopupWidth - 12.f), 0, 3);
	}

	//-- Scroll indicator, only when the list is longer than the window.
	if ((int)m_TextList.size() > m_iMaxVisible)
	{
		const float fArrowSize = 12.f;
		const float fArrowX = x + fPopupWidth - 14.f;
		const float fTrackX = x + fPopupWidth - 8.f;
		const float fTrackY = y + fArrowSize + 4.f;
		const float fTrackH = fHeight - fArrowSize * 2.f - 8.f;
		const float fThumbH = fTrackH * (float)m_iMaxVisible / (float)m_TextList.size();
		const float fThumbY = fTrackY + (fTrackH - fThumbH) * (float)m_iScroll / (float)((int)m_TextList.size() - m_iMaxVisible);

		if (m_iScrollArrowImage >= 0)
		{
			SEASON3B::BeginSkinDraw();
			SEASON3B::RenderImageF(m_iScrollArrowImage, fArrowX, y + 2.f, fArrowSize, fArrowSize,
				0.f, 0.f, m_fScrollArrowSrcSize, m_fScrollArrowSrcSize);
			SEASON3B::RenderImageF(m_iScrollArrowImage, fArrowX, y + fHeight - 14.f, fArrowSize, fArrowSize,
				m_fScrollArrowSrcSize, 0.f, m_fScrollArrowSrcSize, m_fScrollArrowSrcSize);
		}

		if (m_iScrollTrackImage >= 0 && m_iScrollThumbImage >= 0)
		{
			SEASON3B::BeginSkinDraw();
			SEASON3B::RenderImageF(m_iScrollTrackImage, fTrackX - 2.f, fTrackY, 5.f, fTrackH,
				0.f, 0.f, m_fScrollTrackSrcWidth, m_fScrollTrackSrcHeight);
			SEASON3B::RenderImageF(m_iScrollThumbImage, fTrackX - 3.f, fThumbY, 7.f, fThumbH,
				0.f, 0.f, m_fScrollThumbSrcWidth, m_fScrollThumbSrcHeight);
		}
		else
		{
			SEASON3B::FillRoundRect(fTrackX, fTrackY, 3.f, fTrackH, 1.5f, RGBA(255, 255, 255, 28));
			SEASON3B::FillRoundRect(fTrackX, fThumbY, 3.f, fThumbH, 1.5f, SEASON3B::ColorAlpha(COL_ACCENT(), 0.85f));
		}
	}
}
