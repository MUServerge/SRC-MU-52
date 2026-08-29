// NewUIOptionWindow.cpp: implementation of the CNewUIOptionWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIOptionWindow.h"
#include "NewUISystem.h"
#include "ZzzTexture.h"
#include "DSPlaySound.h"
#include "Input.h"
#include "ZzzOpenData.h"
#include "ZzzInterface.h"
#include "pugixml.hpp"
#include "./ExternalObject/leaf/regkey.h"
#include "TextClien.h"
#include "GameOptions.h"
#include "NewUIStyleFX.h"
using namespace SEASON3B;

namespace
{
	const float OPTION_WIDTH = 320.f;
	const float OPTION_HEIGHT = 300.f;
	const float OPTION_TITLE_HEIGHT = 38.f;
	const float OPTION_TAB_Y = 45.f;
	const float OPTION_TAB_WIDTH = 92.f;
	const float OPTION_TAB_HEIGHT = 24.f;
	const float OPTION_ROW_HEIGHT = 19.f;
	const float OPTION_CHECK_SIZE = 16.f;

	int ClampInt(int iValue, int iMin, int iMax)
	{
		if (iValue < iMin)
			return iMin;
		if (iValue > iMax)
			return iMax;
		return iValue;
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SEASON3B::CNewUIOptionWindow::CNewUIOptionWindow()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;

	m_bAutoAttack = true;
	m_bWhisperSound = false;
	m_bSlideHelp = true;
	m_iVolumeLevel = 0;
	m_iRenderLevel = 4;

	m_RenderEffect = true;
	m_RenderEquipment = true;
	m_RenderTerrain = true;
	m_RenderObjects = true;

	m_iActiveTab = TAB_GAME;
	m_bDragging = false;
	m_DragOffset.x = 0;
	m_DragOffset.y = 0;
}

SEASON3B::CNewUIOptionWindow::~CNewUIOptionWindow()
{
	Release();
}

bool SEASON3B::CNewUIOptionWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if (NULL == pNewUIMng)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_OPTION, this);

	SetPos(x, y);

	LoadImages();

	SetButtonInfo();

	Show(false);

	return true;
}

void SEASON3B::CNewUIOptionWindow::SetButtonInfo()
{
	m_ResolutionDropDown.Create(m_Pos.x + 158.f, m_Pos.y + 94.f, 140.f, 20.f, 7);
	m_FontDropDown.Create(m_Pos.x + 158.f, m_Pos.y + 123.f, 140.f, 20.f, 7);
	m_ResolutionDropDown.SetSkin(SKIN_DROPDOWN, 300.f, 44.f, 18.f, -2, 0.f);
	m_FontDropDown.SetSkin(SKIN_DROPDOWN, 300.f, 44.f, 18.f, -2, 0.f);
	m_ResolutionDropDown.SetPopupSkin(SKIN_DROPDOWN_LIST, 300.f, 220.f,
		SKIN_DROPDOWN_ROW, 300.f, 40.f, SKIN_SCROLL_TRACK, 28.f, 240.f,
		SKIN_SCROLL_THUMB, 120.f, 96.f, SKIN_SCROLL_ARROW, 48.f, 74.f);
	m_FontDropDown.SetPopupSkin(SKIN_DROPDOWN_LIST, 300.f, 220.f,
		SKIN_DROPDOWN_ROW, 300.f, 40.f, SKIN_SCROLL_TRACK, 28.f, 240.f,
		SKIN_SCROLL_THUMB, 120.f, 96.f, SKIN_SCROLL_ARROW, 48.f, 74.f);

	LoadResolution("Data\\Resolutions.xml");
}

void SEASON3B::CNewUIOptionWindow::Release()
{
	UnloadImages();

	if (m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUIOptionWindow::SetPos(int x, int y)
{
	const float fScreenX = (float)GetWindowsX;
	const float fScreenY = (float)GetWindowsY;

	m_Pos.x = (LONG)((fScreenX - OPTION_WIDTH) * 0.5f + (float)x);
	m_Pos.y = (LONG)((fScreenY - OPTION_HEIGHT) * 0.5f + (float)y);

	if (m_Pos.x < 0) m_Pos.x = 0;
	if (m_Pos.y < 0) m_Pos.y = 0;
	if (m_Pos.x + OPTION_WIDTH > fScreenX) m_Pos.x = (LONG)(fScreenX - OPTION_WIDTH);
	if (m_Pos.y + OPTION_HEIGHT > fScreenY) m_Pos.y = (LONG)(fScreenY - OPTION_HEIGHT);

	UpdateChildPositions();
}

void SEASON3B::CNewUIOptionWindow::UpdateChildPositions()
{
	m_ResolutionDropDown.SetPosition(m_Pos.x + 158.f, m_Pos.y + 94.f);
	m_FontDropDown.SetPosition(m_Pos.x + 158.f, m_Pos.y + 123.f);
}

bool SEASON3B::CNewUIOptionWindow::UpdateMouseEvent()
{
	if (m_iActiveTab == TAB_OPTIONS)
	{
		if (m_ResolutionDropDown.IsOpen())
		{
			if (m_ResolutionDropDown.UpdateMouseEvent())
				change_resolution();
			return false;
		}

		if (m_FontDropDown.IsOpen())
		{
			if (m_FontDropDown.UpdateMouseEvent())
				change_fontsize();
			return false;
		}

		if (m_ResolutionDropDown.UpdateMouseEvent())
			change_resolution();
		if (m_ResolutionDropDown.IsOpen())
			return false;

		if (m_FontDropDown.UpdateMouseEvent())
			change_fontsize();

		if (m_ResolutionDropDown.IsMouseOver() || m_FontDropDown.IsMouseOver())
			return false;
	}

	if (UpdateDragEvent())
		return false;

	if (SEASON3B::IsPress(VK_LBUTTON) &&
		CheckMouseIn(m_Pos.x + 292.f, m_Pos.y + 6.f, 18.f, 18.f))
	{
		g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);
		PlayBuffer(SOUND_CLICK01);
		return false;
	}

	if (UpdateTabMouseEvent() || UpdateCheckboxMouseEvent() ||
		UpdateSliderMouseEvent() || UpdateActionMouseEvent())
	{
		return false;
	}

	return !CheckMouseIn((float)m_Pos.x, (float)m_Pos.y, OPTION_WIDTH, OPTION_HEIGHT);
}

bool SEASON3B::CNewUIOptionWindow::UpdateDragEvent()
{
	const bool bTitle = CheckMouseIn((float)m_Pos.x + 12.f, (float)m_Pos.y + 4.f,
		OPTION_WIDTH - 42.f, OPTION_TITLE_HEIGHT - 4.f) != 0;

	if (!m_bDragging && bTitle && SEASON3B::IsPress(VK_LBUTTON))
	{
		m_bDragging = true;
		m_DragOffset.x = (LONG)MouseX - m_Pos.x;
		m_DragOffset.y = (LONG)MouseY - m_Pos.y;
	}

	if (!m_bDragging)
		return false;

	if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
	{
		m_bDragging = false;
		return true;
	}

	const float fScreenX = (float)GetWindowsX;
	const float fScreenY = (float)GetWindowsY;
	m_Pos.x = (LONG)MouseX - m_DragOffset.x;
	m_Pos.y = (LONG)MouseY - m_DragOffset.y;
	m_Pos.x = ClampInt(m_Pos.x, 0, (int)(fScreenX - OPTION_WIDTH));
	m_Pos.y = ClampInt(m_Pos.y, 0, (int)(fScreenY - OPTION_HEIGHT));
	UpdateChildPositions();
	return true;
}

bool SEASON3B::CNewUIOptionWindow::UpdateTabMouseEvent()
{
	if (!SEASON3B::IsPress(VK_LBUTTON))
		return false;

	for (int i = 0; i < TAB_COUNT; ++i)
	{
		const float x = m_Pos.x + 14.f + (OPTION_TAB_WIDTH + 8.f) * (float)i;
		if (CheckMouseIn(x, m_Pos.y + OPTION_TAB_Y, OPTION_TAB_WIDTH, OPTION_TAB_HEIGHT))
		{
			m_iActiveTab = i;
			m_ResolutionDropDown.Close();
			m_FontDropDown.Close();
			PlayBuffer(SOUND_CLICK01);
			return true;
		}
	}
	return false;
}

bool SEASON3B::CNewUIOptionWindow::UpdateCheckboxMouseEvent()
{
	if (!SEASON3B::IsPress(VK_LBUTTON))
		return false;

	const float x = m_Pos.x + 16.f;
	const float width = OPTION_WIDTH - 32.f;

	if (m_iActiveTab == TAB_GAME)
	{
		const float rows[] = { 104.f, 123.f, 142.f, 188.f, 207.f, 226.f, 245.f };
		for (int i = 0; i < 7; ++i)
		{
			if (!CheckMouseIn(x, m_Pos.y + rows[i], width, OPTION_ROW_HEIGHT))
				continue;

			switch (i)
			{
			case 0: m_bAutoAttack = !m_bAutoAttack; break;
			case 1: m_bWhisperSound = !m_bWhisperSound; break;
			case 2: m_bSlideHelp = !m_bSlideHelp; break;
			case 3: gGameOptions.Toggle(GAMEOPT_SHOW_MY_NAME); break;
			case 4: gGameOptions.Toggle(GAMEOPT_SHOW_PLAYER_NAME); break;
			case 5: gGameOptions.Toggle(GAMEOPT_SHOW_NPC_NAME); break;
			case 6: gGameOptions.Toggle(GAMEOPT_SHOW_MONSTER_NAME); break;
			}
			PlayBuffer(SOUND_CLICK01);
			return true;
		}
	}
	else if (m_iActiveTab == TAB_GRAPHICS)
	{
		const float rows[] = { 104.f, 123.f, 142.f, 161.f, 206.f, 225.f, 244.f };
		for (int i = 0; i < 7; ++i)
		{
			if (!CheckMouseIn(x, m_Pos.y + rows[i], width, OPTION_ROW_HEIGHT))
				continue;

			switch (i)
			{
			case 0: m_RenderEffect = !m_RenderEffect; break;
			case 1: m_RenderEquipment = !m_RenderEquipment; break;
			case 2: m_RenderTerrain = !m_RenderTerrain; break;
			case 3: m_RenderObjects = !m_RenderObjects; break;
			case 4: gGameOptions.Toggle(GAMEOPT_ENABLE_FOG); break;
			case 5: gGameOptions.Toggle(GAMEOPT_ENABLE_WEATHER); break;
			case 6: gGameOptions.Toggle(GAMEOPT_ENABLE_AURA); break;
			}
			PlayBuffer(SOUND_CLICK01);
			return true;
		}
	}
	else if (m_iActiveTab == TAB_OPTIONS)
	{
		if (CheckMouseIn(x, m_Pos.y + 249.f, width, OPTION_ROW_HEIGHT))
		{
			gGameOptions.Toggle(GAMEOPT_BACKGROUND_THROTTLE);
			PlayBuffer(SOUND_CLICK01);
			return true;
		}
	}

	return false;
}

bool SEASON3B::CNewUIOptionWindow::UpdateSliderMouseEvent()
{
	if (m_iActiveTab != TAB_OPTIONS)
		return false;

	const float fVolumeX = m_Pos.x + 158.f;
	const float fVolumeY = m_Pos.y + 174.f;
	const float fVolumeWidth = 140.f;

	if (CheckMouseIn(fVolumeX, fVolumeY - 3.f, fVolumeWidth, 18.f))
	{
		const int iOld = m_iVolumeLevel;
		if (MouseWheel != 0)
		{
			m_iVolumeLevel += (MouseWheel > 0) ? 1 : -1;
			MouseWheel = 0;
		}
		if (SEASON3B::IsRepeat(VK_LBUTTON))
			m_iVolumeLevel = (int)(((MouseX - fVolumeX) * 10.f / fVolumeWidth) + 0.5f);

		m_iVolumeLevel = ClampInt(m_iVolumeLevel, 0, 10);
		if (iOld != m_iVolumeLevel)
			SetEffectVolumeLevel(m_iVolumeLevel);
		return true;
	}

	if (SEASON3B::IsPress(VK_LBUTTON))
	{
		for (int i = 0; i < 5; ++i)
		{
			const float x = m_Pos.x + 158.f + (float)i * 28.f;
			if (CheckMouseIn(x, m_Pos.y + 222.f, 24.f, 20.f))
			{
				m_iRenderLevel = i;
				PlayBuffer(SOUND_CLICK01);
				return true;
			}
		}
	}

	return false;
}

bool SEASON3B::CNewUIOptionWindow::UpdateActionMouseEvent()
{
	if (SEASON3B::IsPress(VK_LBUTTON) &&
		CheckMouseIn(m_Pos.x + 117.f, m_Pos.y + 267.f, 86.f, 22.f))
	{
		gGameOptions.Save();
		g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);
		PlayBuffer(SOUND_CLICK01);
		return true;
	}
	return false;
}

bool SEASON3B::CNewUIOptionWindow::UpdateKeyEvent()
{
	if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION) == true)
	{
		if (SEASON3B::IsPress(VK_ESCAPE) == true)
		{
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);
			PlayBuffer(SOUND_CLICK01);
			return false;
		}
	}

	return true;
}

bool SEASON3B::CNewUIOptionWindow::Update()
{
	return true;
}

bool SEASON3B::CNewUIOptionWindow::Render()
{
	EnableAlphaTest();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	RenderFrame();

	RenderContents();

	RenderButtons();

	if (m_iActiveTab == TAB_OPTIONS)
	{
		m_ResolutionDropDown.RenderPopup();
		m_FontDropDown.RenderPopup();
	}

	DisableAlphaBlend();

	return true;
}

float SEASON3B::CNewUIOptionWindow::GetLayerDepth()	//. 10.5f
{
	return 10.5f;
}

float SEASON3B::CNewUIOptionWindow::GetKeyEventOrder()	// 10.f;
{
	return 10.0f;
}

void SEASON3B::CNewUIOptionWindow::OpenningProcess()
{

}

void SEASON3B::CNewUIOptionWindow::ClosingProcess()
{
	gGameOptions.Save();
	SaveOptions();
}

void SEASON3B::CNewUIOptionWindow::SetGameOptions(BYTE GameOption)
{
	if ((GameOption & AUTOATTACK_ON) == AUTOATTACK_ON)
	{
		this->SetAutoAttack(true);
	}
	else
	{
		this->SetAutoAttack(false);
	}

	if ((GameOption & WHISPER_SOUND_ON) == WHISPER_SOUND_ON)
	{
		this->SetWhisperSound(true);
	}
	else
	{
		this->SetWhisperSound(false);
	}

	if ((GameOption & SLIDE_HELP_ON) == SLIDE_HELP_ON)
	{
		this->SetSlideHelp(true);
	}
	else
	{
		this->SetSlideHelp(false);
	}

	if ((GameOption & RENDER_EFFECT_ON) == RENDER_EFFECT_ON)
	{
		this->SetRenderEffect(true);
	}
	else
	{
		this->SetRenderEffect(false);
	}

	if ((GameOption & RENDER_EQUIPMENT_ON) == RENDER_EQUIPMENT_ON)
	{
		this->SetRenderEquipment(true);
	}
	else
	{
		this->SetRenderEquipment(false);
	}

	if ((GameOption & RENDER_TERRAIN_ON) == RENDER_TERRAIN_ON)
	{
		this->SetRenderTerrain(true);
	}
	else
	{
		this->SetRenderTerrain(false);
	}

	if ((GameOption & RENDER_OBJECTS_ON) == RENDER_OBJECTS_ON)
	{
		this->SetRenderObjects(true);
	}
	else
	{
		this->SetRenderObjects(false);
	}
}

void SEASON3B::CNewUIOptionWindow::LoadImages()
{
	LoadBitmap("Interface\\Iberia\\GameOption\\window.png", SKIN_WINDOW, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\tabs.png", SKIN_TAB, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\close.png", SKIN_CLOSE, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\section-header.png", SKIN_SECTION, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\checkbox.png", SKIN_CHECKBOX, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\dropdown.png", SKIN_DROPDOWN, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\dropdown-list.png", SKIN_DROPDOWN_LIST, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\dropdown-row.png", SKIN_DROPDOWN_ROW, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\effect-selector.png", SKIN_EFFECT_SELECTOR, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\action-button.png", SKIN_ACTION_BUTTON, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\volume-track.png", SKIN_VOLUME_TRACK, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\volume-fill.png", SKIN_VOLUME_FILL, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\volume-thumb.png", SKIN_VOLUME_THUMB, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\scrollbar-track.png", SKIN_SCROLL_TRACK, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\scrollbar-thumb.png", SKIN_SCROLL_THUMB, GL_LINEAR);
	LoadBitmap("Interface\\Iberia\\GameOption\\scrollbar-arrow.png", SKIN_SCROLL_ARROW, GL_LINEAR);
}

void SEASON3B::CNewUIOptionWindow::UnloadImages()
{
	for (int i = SKIN_WINDOW; i <= SKIN_SCROLL_ARROW; ++i)
		DeleteBitmap(i);
}

void SEASON3B::CNewUIOptionWindow::RenderFrame()
{
	SEASON3B::BeginSkinDraw();
	RenderImageF(SKIN_WINDOW, (float)m_Pos.x, (float)m_Pos.y, OPTION_WIDTH, OPTION_HEIGHT,
		0.f, 0.f, 640.f, 600.f);

	RenderTabs();

	int iCloseState = 0;
	if (CheckMouseIn(m_Pos.x + 292.f, m_Pos.y + 6.f, 18.f, 18.f))
		iCloseState = SEASON3B::IsRepeat(VK_LBUTTON) ? 2 : 1;
	RenderImageF(SKIN_CLOSE, m_Pos.x + 292.f, m_Pos.y + 6.f, 18.f, 18.f,
		56.f * (float)iCloseState, 0.f, 56.f, 56.f);
}

void SEASON3B::CNewUIOptionWindow::RenderContents()
{
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(224, 203, 126, 255);
	g_pRenderText->RenderText(m_Pos.x, m_Pos.y + 14, "Game Options", (int)OPTION_WIDTH, 0, 3);

	switch (m_iActiveTab)
	{
	case TAB_GAME: RenderGameTab(); break;
	case TAB_GRAPHICS: RenderGraphicsTab(); break;
	default: RenderOptionsTab(); break;
	}
}

void SEASON3B::CNewUIOptionWindow::RenderChecked(float RenderFrameX, float RenderFrameY, bool bEnable)
{
	const int iState = bEnable ? 2 : 0;
	RenderImageF(SKIN_CHECKBOX, RenderFrameX, RenderFrameY, OPTION_CHECK_SIZE, OPTION_CHECK_SIZE,
		40.f * (float)iState, 0.f, 40.f, 40.f);
}

void SEASON3B::CNewUIOptionWindow::RenderButtons()
{
	RenderActionButton(m_Pos.x + 117.f, m_Pos.y + 267.f, 86.f, "Close");
}

void SEASON3B::CNewUIOptionWindow::RenderTabs()
{
	static const char* s_TabText[TAB_COUNT] = { "Game", "Graphics", "Options" };

	for (int i = 0; i < TAB_COUNT; ++i)
	{
		const float x = m_Pos.x + 14.f + (OPTION_TAB_WIDTH + 8.f) * (float)i;
		const bool bHover = CheckMouseIn(x, m_Pos.y + OPTION_TAB_Y, OPTION_TAB_WIDTH, OPTION_TAB_HEIGHT) != 0;
		int iState = (i == m_iActiveTab) ? 2 : (bHover ? 1 : 0);
		if (bHover && SEASON3B::IsRepeat(VK_LBUTTON)) iState = 3;

		RenderImageF(SKIN_TAB, x, m_Pos.y + OPTION_TAB_Y, OPTION_TAB_WIDTH, OPTION_TAB_HEIGHT,
			200.f * (float)iState, 0.f, 200.f, 64.f);

		g_pRenderText->SetFont((i == m_iActiveTab) ? g_hFontBold : g_hFont);
		g_pRenderText->SetBgColor(0);
		g_pRenderText->SetTextColor((i == m_iActiveTab) ? RGBA(210, 255, 160, 255) : RGBA(210, 205, 188, 255));
		g_pRenderText->RenderText((int)x, m_Pos.y + 51, s_TabText[i], (int)OPTION_TAB_WIDTH, 0, 3);
	}
}

void SEASON3B::CNewUIOptionWindow::RenderSection(float y, const char* pszText)
{
	RenderImageF(SKIN_SECTION, m_Pos.x + 14.f, m_Pos.y + y, OPTION_WIDTH - 28.f, 18.f,
		0.f, 0.f, 560.f, 44.f);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(215, 196, 126, 255);
	g_pRenderText->RenderText(m_Pos.x + 32, m_Pos.y + (int)y + 2, pszText);
}

void SEASON3B::CNewUIOptionWindow::RenderOptionRow(float y, const char* pszText, bool bChecked, bool bEnabled)
{
	const bool bHover = bEnabled && CheckMouseIn(m_Pos.x + 16.f, m_Pos.y + y, OPTION_WIDTH - 32.f, OPTION_ROW_HEIGHT);
	int iState = bChecked ? 2 : (bHover ? 1 : 0);
	if (!bEnabled) iState = 3;

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(bEnabled ? (bHover ? RGBA(235, 245, 225, 255) : RGBA(210, 210, 205, 255)) : RGBA(120, 120, 120, 255));
	g_pRenderText->RenderText(m_Pos.x + 22, m_Pos.y + (int)y + 2, pszText);

	RenderImageF(SKIN_CHECKBOX, m_Pos.x + 281.f, m_Pos.y + y + 1.f, OPTION_CHECK_SIZE, OPTION_CHECK_SIZE,
		40.f * (float)iState, 0.f, 40.f, 40.f);
}

void SEASON3B::CNewUIOptionWindow::RenderGameTab()
{
	RenderSection(80.f, "Gameplay");
	RenderOptionRow(104.f, "Automatic Attack", m_bAutoAttack);
	RenderOptionRow(123.f, "Whisper Sound", m_bWhisperSound);
	RenderOptionRow(142.f, "Slide Help", m_bSlideHelp);

	RenderSection(164.f, "Names and Interface");
	RenderOptionRow(188.f, "Show My Name", gGameOptions.IsOn(GAMEOPT_SHOW_MY_NAME));
	RenderOptionRow(207.f, "Show Player Names", gGameOptions.IsOn(GAMEOPT_SHOW_PLAYER_NAME));
	RenderOptionRow(226.f, "Show NPC Names", gGameOptions.IsOn(GAMEOPT_SHOW_NPC_NAME));
	RenderOptionRow(245.f, "Show Monster Names", gGameOptions.IsOn(GAMEOPT_SHOW_MONSTER_NAME));
}

void SEASON3B::CNewUIOptionWindow::RenderGraphicsTab()
{
	RenderSection(80.f, "World Rendering");
	RenderOptionRow(104.f, "Visual Effects", m_RenderEffect);
	RenderOptionRow(123.f, "Equipment Effects", m_RenderEquipment);
	RenderOptionRow(142.f, "Terrain", m_RenderTerrain);
	RenderOptionRow(161.f, "World Objects", m_RenderObjects);

	RenderSection(182.f, "Atmosphere");
	RenderOptionRow(206.f, "Fog", gGameOptions.IsOn(GAMEOPT_ENABLE_FOG));
	RenderOptionRow(225.f, "Weather", gGameOptions.IsOn(GAMEOPT_ENABLE_WEATHER));
	RenderOptionRow(244.f, "Character Aura", gGameOptions.IsOn(GAMEOPT_ENABLE_AURA));
}

void SEASON3B::CNewUIOptionWindow::RenderOptionsTab()
{
	RenderSection(76.f, "Display");
	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(210, 210, 205, 255);
	g_pRenderText->RenderText(m_Pos.x + 22, m_Pos.y + 98, "Resolution");
	g_pRenderText->RenderText(m_Pos.x + 22, m_Pos.y + 127, "Font Size");
	m_ResolutionDropDown.Render();
	m_FontDropDown.Render();

	RenderSection(151.f, "Audio");
	g_pRenderText->RenderText(m_Pos.x + 22, m_Pos.y + 175, "Volume");
	const float fVolume = (float)ClampInt(m_iVolumeLevel, 0, 10) / 10.f;
	RenderImageF(SKIN_VOLUME_TRACK, m_Pos.x + 158.f, m_Pos.y + 177.f, 140.f, 8.f, 0.f, 0.f, 600.f, 32.f);
	if (fVolume > 0.f)
		RenderImageF(SKIN_VOLUME_FILL, m_Pos.x + 158.f, m_Pos.y + 179.f, 140.f * fVolume, 6.f,
			0.f, 0.f, 600.f * fVolume, 24.f);
	const float fThumbX = m_Pos.x + 158.f + 140.f * fVolume - 6.f;
	const bool bVolumeHover = CheckMouseIn(m_Pos.x + 158.f, m_Pos.y + 171.f, 140.f, 20.f) != 0;
	const int iThumbState = bVolumeHover ? (SEASON3B::IsRepeat(VK_LBUTTON) ? 2 : 1) : 0;
	RenderImageF(SKIN_VOLUME_THUMB, fThumbX, m_Pos.y + 171.f, 12.f, 16.f,
		48.f * (float)iThumbState, 0.f, 48.f, 64.f);

	RenderSection(199.f, "Performance");
	g_pRenderText->RenderText(m_Pos.x + 22, m_Pos.y + 224, "Effect Limit");
	static const int s_EffectValue[5] = { 5, 7, 9, 11, 13 };
	for (int i = 0; i < 5; ++i)
	{
		const float x = m_Pos.x + 158.f + (float)i * 28.f;
		const bool bHover = CheckMouseIn(x, m_Pos.y + 222.f, 24.f, 20.f) != 0;
		const int iState = (m_iRenderLevel == i) ? 2 : (bHover ? 1 : 0);
		RenderImageF(SKIN_EFFECT_SELECTOR, x, m_Pos.y + 222.f, 24.f, 20.f,
			72.f * (float)iState, 0.f, 72.f, 56.f);
		char szValue[8];
		sprintf_s(szValue, "%d", s_EffectValue[i]);
		g_pRenderText->RenderText((int)x, m_Pos.y + 226, szValue, 24, 0, 3);
	}
	RenderOptionRow(249.f, "Throttle In Background", gGameOptions.IsOn(GAMEOPT_BACKGROUND_THROTTLE));
}

void SEASON3B::CNewUIOptionWindow::RenderActionButton(float x, float y, float width, const char* pszText, bool bEnabled)
{
	const bool bHover = bEnabled && CheckMouseIn(x, y, width, 22.f) != 0;
	int iState = bEnabled ? (bHover ? 1 : 0) : 3;
	if (bHover && SEASON3B::IsRepeat(VK_LBUTTON)) iState = 2;
	RenderImageF(SKIN_ACTION_BUTTON, x, y, width, 22.f, 200.f * (float)iState, 0.f, 200.f, 56.f);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(bEnabled ? RGBA(226, 211, 164, 255) : RGBA(120, 120, 120, 255));
	g_pRenderText->RenderText((int)x, (int)y + 4, pszText, (int)width, 0, 3);
}

void SEASON3B::CNewUIOptionWindow::RenderTable(float x, float y, float width, float height)
{
	EnableAlphaTest();

	glColor4f(0.0, 0.0, 0.0, 0.40000001);

	RenderColor((x + 3), (y + 2), (width - 7), (height - 7), 0.0, 0);

	EndRenderColor();

#if MAIN_UPDATE <= 603
	RenderImage(IMAGE_MAIN_TABLE_TOP_LEFT, x, y, 14.0, 14.0);

	RenderImageF(IMAGE_MAIN_TABLE_TOP_RIGHT, (x + width - 14), y, 14.0, 14.0, 0.f, 0.f, 56.f, 56.f);

	RenderImageF(IMAGE_MAIN_TABLE_BOTTOM_LEFT, x, (y + height - 14), 14.0, 14.0, 0.f, 0.f, 56.f, 56.f);

	RenderImageF(IMAGE_MAIN_TABLE_BOTTOM_RIGHT, (x + width - 14), (y + height - 14), 14.0, 14.0, 0.f, 0.f, 56.f, 56.f);

	RenderImageF(IMAGE_MAIN_TABLE_TOP_PIXEL, (x + 6), y, (width - 12), 14.0, 0.f, 0.f, (float)(((width - 12)) * 4.f), 56.f);

	RenderImageF(IMAGE_MAIN_TABLE_RIGHT_PIXEL, (x + width - 14), (y + 6), 14.0, (height - 14), 0.f, 0.f, 56.f, (float)(((height - 14)) * 4.f));

	RenderImageF(IMAGE_MAIN_TABLE_BOTTOM_PIXEL, (x + 6), (y + height - 14), (width - 12), 14.0, 0.f, 0.f, (float)(((width - 12)) * 4.f), 56.f);

	RenderImageF(IMAGE_MAIN_TABLE_LEFT_PIXEL, x, (y + 6), 14.0, (height - 14), 0.f, 0.f, 56.f, (float)(((height - 14)) * 4.f));
#endif
}

void SEASON3B::CNewUIOptionWindow::SetAutoAttack(bool bAuto)
{
	m_bAutoAttack = bAuto;
}

bool SEASON3B::CNewUIOptionWindow::IsAutoAttack()
{
	return m_bAutoAttack;
}

void SEASON3B::CNewUIOptionWindow::SetWhisperSound(bool bSound)
{
	m_bWhisperSound = bSound;
}

bool SEASON3B::CNewUIOptionWindow::IsWhisperSound()
{
	return m_bWhisperSound;
}

void SEASON3B::CNewUIOptionWindow::SetSlideHelp(bool bHelp)
{
	m_bSlideHelp = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::IsSlideHelp()
{
	return m_bSlideHelp;
}

void SEASON3B::CNewUIOptionWindow::SetVolumeLevel(int iVolume)
{
	m_iVolumeLevel = iVolume;
}

int SEASON3B::CNewUIOptionWindow::GetVolumeLevel()
{
	return m_iVolumeLevel;
}

void SEASON3B::CNewUIOptionWindow::SetRenderLevel(int iRender)
{
	m_iRenderLevel = iRender;
}

int SEASON3B::CNewUIOptionWindow::GetRenderLevel()
{
	return m_iRenderLevel;
}

void SEASON3B::CNewUIOptionWindow::SetRenderEffect(bool bHelp)
{
	m_RenderEffect = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderEffect()
{
	return m_RenderEffect;
}

void SEASON3B::CNewUIOptionWindow::SetRenderEquipment(bool bHelp)
{
	m_RenderEquipment = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderEquipment()
{
	return m_RenderEquipment;
}

void SEASON3B::CNewUIOptionWindow::SetRenderTerrain(bool bHelp)
{
	m_RenderTerrain = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderTerrain()
{
	return m_RenderTerrain;
}

void SEASON3B::CNewUIOptionWindow::SetRenderObjects(bool bHelp)
{
	m_RenderObjects = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderObjects()
{
	return m_RenderObjects;
}

void SEASON3B::CNewUIOptionWindow::change_resolution()
{
	int index = gwinhandle->GetDisplayIndex(m_ResolutionDropDown.AsString());
	if (index < 0)
		return;

	if (m_Resolution != index)
	{
		leaf::CRegKey regkey;

		regkey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu2\\Config");

		if (regkey.WriteDword("Resolution", index))
		{
			m_Resolution = index;

			double backupWidth = gwinhandle->GetScreenX();

			double backupHight = gwinhandle->GetScreenY();

			gwinhandle->SetDisplayIndex(index, false);

			CameraFactorPtr->Init();

			OpenFont();

			ClearInput(TRUE);

			CInput& rInput = CInput::Instance();

			rInput.Create(gwinhandle->GethWnd(), WindowWidth, WindowHeight);

			g_pMoveCommandWindow->SetPos(1, 1);

			this->SetPos(0, 0);

			g_pNewUISystem->RenderFrameUpdate(backupWidth, backupHight);

			g_pNewUI3DRenderMng->Reload3DEffectObject(WindowWidth, WindowHeight);
		}
	}
}

void SEASON3B::CNewUIOptionWindow::change_fontsize()
{
	int fontsize = m_FontDropDown.AsInteger();

	if (fontsize != FontHeight)
	{
		gwinhandle->SetFontSize(fontsize);

		pugi::xml_document file;
		pugi::xml_parse_result res = file.load_file("Data\\Resolutions.xml");

		if (res.status == pugi::status_ok)
		{
			pugi::xml_node root = file.child("ResolutionsInfo");

			for (pugi::xml_node child = root.child("Resolution"); child; child = child.next_sibling())
			{
				if (gwinhandle->GetDisplayIndex() == child.attribute("index").as_int())
				{
					child.attribute("font_size").set_value(fontsize);

					file.save_file("Data\\Resolutions.xml");
					return;
				}
			}
		}
	}
}

void SEASON3B::CNewUIOptionWindow::LoadResolution(const char* filename)
{
	pugi::xml_document file;
	pugi::xml_parse_result res = file.load_file(filename);

	if (res.status != pugi::status_ok)
	{
		return;
	}

	m_ResolutionDropDown.Clear();
	m_FontDropDown.Clear();

	int fontSize = 17;
	int fontindex = 0;
	int resolutionPosition = 0;
	int selectedResolutionPosition = 0;

	pugi::xml_node root = file.child("ResolutionsInfo");

	for (pugi::xml_node child_io = root.child("Resolution"); child_io;
		child_io = child_io.next_sibling(), resolutionPosition++)
	{
		std::string text_name = child_io.attribute("name").as_string();

		m_ResolutionDropDown.PushBack(text_name);

		if (gwinhandle->GetDisplayIndex() == child_io.attribute("index").as_int())
		{
			fontSize = child_io.attribute("font_size").as_int();
			selectedResolutionPosition = resolutionPosition;
		}
	}

	root = file.child("FontSize");

	int i = 0;

	for (pugi::xml_node child_io = root.child("size"); child_io; child_io = child_io.next_sibling(), i++)
	{
		std::string text_name = child_io.attribute("fontsize").as_string();

		m_FontDropDown.PushBack(text_name);

		if (fontSize == child_io.attribute("fontsize").as_int())
		{
			fontindex = i;
		}
	}

	m_FontDropDown.SetCurrent(fontindex);

	// The list stores positions while the registry/XML stores explicit indices.
	// Keep them separate so gaps or reordered XML entries cannot select past the
	// end of the list (the previous 15 -> 16 gap exposed this at 2560x1440).
	m_ResolutionDropDown.SetCurrent(selectedResolutionPosition);
}
