#include "stdafx.h"
#include "CGMProtect.h"
#include "NewUISystem.h"
#include "NewUICommon.h"
#include "DSPlaySound.h"
#include "CGMFrame.h"
#include "CGMResetManager.h"
#include "CustomEventTime.h"
#include "NewUIMenuUser.h"

using namespace SEASON3B;

namespace
{
	const float BG_ASPECT = 548.f / 752.f;
	const float MENU_WIDTH = 248.f;
	const float MENU_HEIGHT = MENU_WIDTH / BG_ASPECT;
	const float TITLE_H = MENU_WIDTH * (72.f / 548.f);
	const int GRID_COLS = 4;
	const float CELL_W = 57.f;
	const float CELL_H = 68.f;
	const float GRID_TOP = TITLE_H + 6.f;
	const float GRID_LEFT = (MENU_WIDTH - GRID_COLS * CELL_W) * 0.5f;
	const float TILE_SZ = 48.f;
	const float ICON_SZ = 32.f;
	const float CLOSE_SZ = 16.f;
	const float BOTTOM_W = 100.f;
	const float BOTTOM_H = 14.f;

	const char* s_szItemLabel[16] =
	{
		"Game Option", "Character", "Inventory", "Command",
		"Guild Info", "Rankings", "Title System", "Reset System",
		"Jewel Bank", "Event Time", "Market", "Skill List",
		"Withdraw", "Relife System", "Command Game", "Vip Training",
	};

	const char* s_szTexFile[24] =
	{
		"TournamentMenu_I13", "TournamentMenu_I5", "TournamentMenu_I9", "TournamentMenu_I14",
		"TournamentMenu_I8", "TournamentMenu_I4", "TournamentMenu_I2", "TournamentMenu_I11",
		"TournamentMenu_I10", "TournamentMenu_I7", "TournamentMenu_I3", "TournamentMenu_I12",
		"TournamentMenu_I19", "TournamentMenu_I25", "TournamentMenu_I24", "TournamentMenu_I23",
		"TournamentMenu_I17", "TournamentMenu_I21", "TournamentMenu_I16", "TournamentMenu_I26",
		"TournamentMenu_I28", "TournamentMenu_I29", "TournamentMenu_I30", "TournamentMenu_I27",
	};
}

SEASON3B::CNewUIMenuUser::CNewUIMenuUser()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;
	m_iHoverItem = -1;
	m_bHoverClose = false;
	m_bHoverBottom = false;
}

SEASON3B::CNewUIMenuUser::~CNewUIMenuUser()
{
	Release();
}

bool SEASON3B::CNewUIMenuUser::Create(CNewUIManager* pNewUIMng, float x, float y)
{
	if (pNewUIMng == NULL)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(INTERFACE_CUSTOM_MENU, this);
	LoadImages();
	SetPos(x, y);
	Show(false);
	return true;
}

void SEASON3B::CNewUIMenuUser::Release()
{
	if (m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		UnloadImages();
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUIMenuUser::SetPos(float x, float y)
{
	m_Pos.x = (LONG)PositionX_The_Mid(MENU_WIDTH);
	m_Pos.y = (LONG)PositionY_In_The_Mid((480.f - MENU_HEIGHT) * 0.5f);
}

void SEASON3B::CNewUIMenuUser::LoadImages()
{
	char szPath[128];
	for (int i = 0; i < 24; ++i)
	{
		sprintf(szPath, "Interface\\Iberia_Menu\\%s.tga", s_szTexFile[i]);
		LoadBitmap(szPath, BITMAP_INTERFACE_MAINMENU_BEGIN + i, GL_LINEAR);
	}
}

void SEASON3B::CNewUIMenuUser::UnloadImages()
{
	for (int i = 0; i < 24; ++i)
		DeleteBitmap(BITMAP_INTERFACE_MAINMENU_BEGIN + i);
}

void SEASON3B::CNewUIMenuUser::GetItemRect(int index, float& x, float& y, float& w, float& h)
{
	int col = index % GRID_COLS;
	int row = index / GRID_COLS;
	x = m_Pos.x + GRID_LEFT + col * CELL_W;
	y = m_Pos.y + GRID_TOP + row * CELL_H;
	w = CELL_W;
	h = CELL_H;
}

void SEASON3B::CNewUIMenuUser::GetCloseRect(float& x, float& y, float& w, float& h)
{
	w = CLOSE_SZ;
	h = CLOSE_SZ;
	x = m_Pos.x + MENU_WIDTH - w - 8.f;
	y = m_Pos.y + (TITLE_H - h) * 0.5f;
}

void SEASON3B::CNewUIMenuUser::GetBottomRect(float& x, float& y, float& w, float& h)
{
	w = BOTTOM_W;
	h = BOTTOM_H;
	x = m_Pos.x + (MENU_WIDTH - w) * 0.5f;
	y = m_Pos.y + MENU_HEIGHT - h - 5.f;
}

bool SEASON3B::CNewUIMenuUser::IsItemEnabled(int index) const
{
	switch (index)
	{
	case 5: return gmProtect->MenuButtonRankTop && gmProtect->WindowsRankTop;
	case 6: return false;
	case 7: return GMResetManager->IsWindowAvailable(0);
	case 8: return gmProtect->WindowsJewelInventory;
	case 9: return gmProtect->MenuButtonEventTime;
	case 10: return false;
	case 12: return false;
	case 13: return GMResetManager->IsWindowAvailable(1);
	case 14: return gmProtect->MenuButtonCommand && gmProtect->WindowsCommand;
	case 15: return gmProtect->MenuButtonVipShop && gmProtect->WindowsVipShop;
	}
	return true;
}

bool SEASON3B::CNewUIMenuUser::ActivateItem(int index)
{
	if (!IsItemEnabled(index))
		return false;

	switch (index)
	{
	case 0: g_pNewUISystem->Show(INTERFACE_OPTION); break;
	case 1: g_pNewUISystem->Toggle(INTERFACE_CHARACTER); break;
	case 2: g_pNewUISystem->Toggle(INTERFACE_INVENTORY); break;
	case 3: g_pNewUISystem->Toggle(INTERFACE_COMMAND); break;
	case 4: g_pNewUISystem->Show(INTERFACE_GUILDINFO); break;
	case 5: g_pNewUISystem->Show(INTERFACE_RANKING_TOP); break;
	case 7: return GMResetManager->OpenWindow(0);
	case 8: g_pNewUISystem->Toggle(INTERFACE_INVENTORY_JEWEL); break;
	case 9: g_CustomEventTime->OnOffWindow(); break;
	case 11: g_pNewUISystem->Toggle(INTERFACE_SKILL_LIST); break;
	case 13: return GMResetManager->OpenWindow(1);
	case 14: g_pNewUISystem->Show(INTERFACE_COMMAND_LIST); break;
	case 15: g_pNewUISystem->Show(INTERFACE_SHOW_VIP); break;
	default: return false;
	}
	return true;
}

bool SEASON3B::CNewUIMenuUser::UpdateKeyEvent()
{
	if (IsVisible() && SEASON3B::IsPress(VK_ESCAPE))
	{
		g_pNewUISystem->Hide(INTERFACE_CUSTOM_MENU);
		return false;
	}
	return true;
}

bool SEASON3B::CNewUIMenuUser::UpdateMouseEvent()
{
	float x, y, w, h;
	m_iHoverItem = -1;

	for (int i = 0; i < MENU_ITEM_COUNT; ++i)
	{
		GetItemRect(i, x, y, w, h);
		if (SEASON3B::CheckMouseIn(x, y, w, h))
		{
			m_iHoverItem = i;
			break;
		}
	}

	GetCloseRect(x, y, w, h);
	m_bHoverClose = SEASON3B::CheckMouseIn(x, y, w, h) != 0;
	if (m_bHoverClose && SEASON3B::IsRelease(VK_LBUTTON))
	{
		g_pNewUISystem->Hide(INTERFACE_CUSTOM_MENU);
		PlayBuffer(SOUND_CLICK01);
		return false;
	}

	GetBottomRect(x, y, w, h);
	m_bHoverBottom = SEASON3B::CheckMouseIn(x, y, w, h) != 0;
	if (m_bHoverBottom && SEASON3B::IsRelease(VK_LBUTTON))
	{
		g_pNewUISystem->Hide(INTERFACE_CUSTOM_MENU);
		PlayBuffer(SOUND_CLICK01);
		return false;
	}

	if (m_iHoverItem >= 0 && SEASON3B::IsRelease(VK_LBUTTON))
	{
		if (ActivateItem(m_iHoverItem))
		{
			g_pNewUISystem->Hide(INTERFACE_CUSTOM_MENU);
			PlayBuffer(SOUND_CLICK01);
		}
		return false;
	}

	return !SEASON3B::CheckMouseIn(m_Pos.x, m_Pos.y, MENU_WIDTH, MENU_HEIGHT);
}

bool SEASON3B::CNewUIMenuUser::Render()
{
	EnableAlphaTest(true);
	glColor4f(1.f, 1.f, 1.f, 1.f);
	RenderFrame();
	RenderItems();
	RenderCloseButton();
	RenderBottomButton();
	DisableAlphaBlend();
	return true;
}

void SEASON3B::CNewUIMenuUser::RenderFrame()
{
	glColor4f(1.f, 1.f, 1.f, 1.f);
	SEASON3B::RenderImageF(IMG_BG, (float)m_Pos.x, (float)m_Pos.y, MENU_WIDTH, MENU_HEIGHT);
	SEASON3B::RenderImageF(IMG_TITLE, (float)m_Pos.x, (float)m_Pos.y, MENU_WIDTH, TITLE_H);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(255, 220, 150, 255);
	g_pRenderText->RenderText(m_Pos.x, (int)(m_Pos.y + 9.f), "Menu Game [F5]", (int)MENU_WIDTH, 0, RT3_SORT_CENTER);
}

void SEASON3B::CNewUIMenuUser::RenderItems()
{
	for (int i = 0; i < MENU_ITEM_COUNT; ++i)
	{
		float x, y, w, h;
		GetItemRect(i, x, y, w, h);
		float tx = x + (w - TILE_SZ) * 0.5f;
		float ty = y + 1.f;
		bool enabled = IsItemEnabled(i);

		glColor4f(enabled ? 1.f : 0.45f, enabled ? 1.f : 0.45f, enabled ? 1.f : 0.45f, 1.f);
		SEASON3B::RenderImageF(IMG_TILE, tx, ty, TILE_SZ, TILE_SZ);
		if (enabled && m_iHoverItem == i)
		{
			glColor4f(1.f, 1.f, 1.f, MouseLButton ? 1.f : 0.55f);
			SEASON3B::RenderImageF(IMG_TILE_PRESS, tx, ty, TILE_SZ, TILE_SZ);
		}
		glColor4f(enabled ? 1.f : 0.4f, enabled ? 1.f : 0.4f, enabled ? 1.f : 0.4f, enabled ? 1.f : 0.65f);
		SEASON3B::RenderImageF(IMG_ICON_BEGIN + i, x + (w - ICON_SZ) * 0.5f, y + 1.f + (TILE_SZ - ICON_SZ) * 0.5f, ICON_SZ, ICON_SZ);
	}

	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetFont(g_hFont);
	for (int i = 0; i < MENU_ITEM_COUNT; ++i)
	{
		float x, y, w, h;
		GetItemRect(i, x, y, w, h);
		if (IsItemEnabled(i))
			g_pRenderText->SetTextColor(230, 230, 235, 255);
		else
			g_pRenderText->SetTextColor(105, 105, 115, 255);
		g_pRenderText->RenderText((int)x, (int)(y + 1.f + TILE_SZ), s_szItemLabel[i], (int)w, 0, RT3_SORT_CENTER);
	}
}

void SEASON3B::CNewUIMenuUser::RenderCloseButton()
{
	float x, y, w, h;
	GetCloseRect(x, y, w, h);
	int img = IMG_CLOSE;
	if (m_bHoverClose)
		img = MouseLButton ? IMG_CLOSE_DOWN : IMG_CLOSE_OVER;
	glColor4f(1.f, 1.f, 1.f, 1.f);
	SEASON3B::RenderImageF(img, x, y, w, h);
}

void SEASON3B::CNewUIMenuUser::RenderBottomButton()
{
	float x, y, w, h;
	GetBottomRect(x, y, w, h);
	glColor4f(m_bHoverBottom ? 1.f : 0.8f, m_bHoverBottom ? 1.f : 0.8f, m_bHoverBottom ? 1.f : 0.8f, 1.f);
	SEASON3B::RenderImageF(IMG_BTN, x, y, w, h);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(225, 225, 230, 255);
	g_pRenderText->RenderText((int)x, (int)(y + 2.f), "Close", (int)w, 0, RT3_SORT_CENTER);
}

bool SEASON3B::CNewUIMenuUser::Update()
{
	return true;
}

float SEASON3B::CNewUIMenuUser::GetLayerDepth()
{
	return 10.0f;
}

void SEASON3B::CNewUIMenuUser::OpenningProcess()
{
	SetPos(0.f, 0.f);
	m_iHoverItem = -1;
	m_bHoverClose = false;
	m_bHoverBottom = false;
}

void SEASON3B::CNewUIMenuUser::ClosingProcess()
{
}
