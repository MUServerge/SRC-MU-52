//*****************************************************************************
// File: NewUIQuestInfoHUD.cpp
// Look5 right-side quest tracker, using the project-native NewUI renderer.
//*****************************************************************************

#include "stdafx.h"
#include "NewUIQuestInfoHUD.h"
#include "NewUISystem.h"
#include "wsclientinline.h"
#include "UsefulDef.h"
#include "pugixml.hpp"
#include "MapManager.h"

using namespace SEASON3B;

struct PMSG_CUSTOMNPCQUEST_HUD_RECV
{
	PSBMSG_HEAD header;
	BYTE state;
	BYTE map;
	WORD quest;
	WORD monsterClass;
	DWORD killed;
	DWORD total;
	char monster[32];
};

enum
{
	CUSTOMNPCQUEST_HUD_STATE_NONE = 0,
	CUSTOMNPCQUEST_HUD_STATE_ACTIVE = 1,
};

CNewUIQuestInfoHUD::CNewUIQuestInfoHUD()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;
	m_bCollapsed = false;
	m_bShowCurrentMap = false;
	ClearQuests();
}

CNewUIQuestInfoHUD::~CNewUIQuestInfoHUD()
{
	Release();
}

bool CNewUIQuestInfoHUD::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if (pNewUIMng == NULL)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_QUESTINFO_HUD, this);

	SetPos(x, y);
	LoadImages();
	LoadQuestTexts();
	Show(true);

	return true;
}

void CNewUIQuestInfoHUD::Release()
{
	UnloadImages();
	m_QuestTitles.clear();

	if (m_pNewUIMng != NULL)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}
}

void CNewUIQuestInfoHUD::LoadImages()
{
	LoadBitmap("Interface\\QuestSystem\\QuestSystemMissionView.tga", IMAGE_QIHUD_ATLAS, GL_LINEAR);
	LoadBitmap("Interface\\QuestInfo\\quest_log_hide.tga", IMAGE_QIHUD_FOLD, GL_LINEAR);
	LoadBitmap("Interface\\QuestInfo\\MissionView_IFF.tga", IMAGE_QIHUD_MONSTER_MARKER, GL_LINEAR);
	LoadBitmap("Interface\\HUD\\checkbox.tga", IMAGE_QIHUD_CHECKED, GL_LINEAR);
	LoadBitmap("Interface\\HUD\\uncheckbox.tga", IMAGE_QIHUD_UNCHECKED, GL_LINEAR);
}

void CNewUIQuestInfoHUD::UnloadImages()
{
	DeleteBitmap(IMAGE_QIHUD_ATLAS);
	DeleteBitmap(IMAGE_QIHUD_FOLD);
	DeleteBitmap(IMAGE_QIHUD_MONSTER_MARKER);
	DeleteBitmap(IMAGE_QIHUD_CHECKED);
	DeleteBitmap(IMAGE_QIHUD_UNCHECKED);
}

void CNewUIQuestInfoHUD::LoadQuestTexts()
{
	m_QuestTitles.clear();

	pugi::xml_document document;
	pugi::xml_parse_result result = document.load_file("Data\\Local\\xml\\QuestSystem\\QuestSystemText.xml");
	if (result.status != pugi::status_ok)
		return;

	pugi::xml_node root = document.child("QuestSystemText");
	for (pugi::xml_node node = root.child("Quest"); node; node = node.next_sibling("Quest"))
	{
		int id = node.attribute("Id").as_int(-1);
		const char* title = node.attribute("Title").as_string();
		if (id > 0 && id <= 0xFFFF && title != NULL && title[0] != '\0')
		{
			m_QuestTitles[(WORD)id] = title;
		}
	}
}

const char* CNewUIQuestInfoHUD::GetQuestTitle(WORD quest) const
{
	std::map<WORD, std::string>::const_iterator it = m_QuestTitles.find(quest);
	return (it != m_QuestTitles.end()) ? it->second.c_str() : NULL;
}

void CNewUIQuestInfoHUD::SetPos(int x, int y)
{
	m_Pos.x = x;
	m_Pos.y = y;
}

void CNewUIQuestInfoHUD::RecvQuestProgress(BYTE* lpMsg, int size)
{
	if (lpMsg == NULL || size != sizeof(PMSG_CUSTOMNPCQUEST_HUD_RECV))
		return;

	PMSG_CUSTOMNPCQUEST_HUD_RECV packet;
	memcpy(&packet, lpMsg, sizeof(packet));
	packet.monster[sizeof(packet.monster) - 1] = '\0';

	if (packet.state == CUSTOMNPCQUEST_HUD_STATE_NONE)
	{
		ClearQuests();
		return;
	}

	if (packet.state != CUSTOMNPCQUEST_HUD_STATE_ACTIVE || packet.quest == 0 || packet.total == 0)
		return;

	if (packet.killed > packet.total)
		packet.killed = packet.total;

	UpsertQuest(packet.quest, packet.map, packet.monsterClass, packet.monster, packet.killed, packet.total);
}

int CNewUIQuestInfoHUD::FindQuest(WORD quest) const
{
	for (int i = 0; i < QIHUD_MAX_QUESTS; ++i)
	{
		if (m_Quest[i].active && m_Quest[i].quest == quest)
			return i;
	}
	return -1;
}

int CNewUIQuestInfoHUD::FindFreeQuest() const
{
	for (int i = 0; i < QIHUD_MAX_QUESTS; ++i)
	{
		if (!m_Quest[i].active)
			return i;
	}
	return -1;
}

void CNewUIQuestInfoHUD::UpsertQuest(WORD quest, BYTE map, WORD monsterClass, const char* objective, DWORD current, DWORD maximum)
{
	int index = FindQuest(quest);
	if (index < 0)
		index = FindFreeQuest();
	if (index < 0)
		return;

	QUEST_TRACK_ENTRY& entry = m_Quest[index];
	bool wasActive = entry.active;
	bool wasMinimized = entry.minimized;
	bool wasHidden = entry.hidden;
	memset(&entry, 0, sizeof(entry));
	entry.active = true;
	entry.minimized = wasActive ? wasMinimized : false;
	entry.hidden = wasActive ? wasHidden : false;
	entry.map = map;
	entry.quest = quest;
	entry.monsterClass = monsterClass;
	entry.current = (current <= maximum) ? current : maximum;
	entry.maximum = maximum;

	const char* title = GetQuestTitle(quest);
	if (title != NULL)
	{
		strncpy(entry.title, title, sizeof(entry.title) - 1);
	}
	else
	{
		wsprintf(entry.title, "Quest %u", (DWORD)quest);
	}
	entry.title[sizeof(entry.title) - 1] = '\0';

	strncpy(entry.objective, (objective != NULL) ? objective : "", sizeof(entry.objective) - 1);
	entry.objective[sizeof(entry.objective) - 1] = '\0';
}

void CNewUIQuestInfoHUD::RemoveQuest(WORD quest)
{
	int index = FindQuest(quest);
	if (index >= 0)
		memset(&m_Quest[index], 0, sizeof(m_Quest[index]));
}

void CNewUIQuestInfoHUD::ClearQuests()
{
	memset(m_Quest, 0, sizeof(m_Quest));
}

bool CNewUIQuestInfoHUD::IsTrackedMonster(WORD monsterClass) const
{
	for (int i = 0; i < QIHUD_MAX_QUESTS; ++i)
	{
		if (m_Quest[i].active && m_Quest[i].monsterClass == monsterClass
			&& (m_Quest[i].maximum == 0 || m_Quest[i].current < m_Quest[i].maximum))
		{
			return true;
		}
	}
	return false;
}

void CNewUIQuestInfoHUD::RenderMonsterMarker(WORD monsterClass, int centerX, int healthBarY) const
{
	if (!IsTrackedMonster(monsterClass))
		return;

	BITMAP_t* bitmap = &Bitmaps[IMAGE_QIHUD_MONSTER_MARKER];
	const float width = 20.f;
	const float height = 20.f;
	const float uWidth = bitmap->output_width / bitmap->Width;
	const float vHeight = bitmap->output_height / bitmap->Height;

	EnableAlphaTest();
	glColor4f(1.f, 1.f, 1.f, 1.f);
	RenderBitmap(IMAGE_QIHUD_MONSTER_MARKER, centerX - width * 0.5f, healthBarY - height - 4.f,
		width, height, 0.f, 0.f, uWidth, vHeight, false, false);
}

int CNewUIQuestInfoHUD::GetActiveQuestCount() const
{
	int count = 0;
	for (int i = 0; i < QIHUD_MAX_QUESTS; ++i)
	{
		if (m_Quest[i].active)
			++count;
	}
	return count;
}

bool CNewUIQuestInfoHUD::IsQuestVisible(const QUEST_TRACK_ENTRY& quest) const
{
	if (!quest.active || quest.hidden)
		return false;

	return (!m_bShowCurrentMap || quest.map == 0xFF || quest.map == (BYTE)World);
}

int CNewUIQuestInfoHUD::GetQuestHeight(const QUEST_TRACK_ENTRY& quest) const
{
	return QIHUD_TITLE_H + (quest.minimized ? 0 : QIHUD_OBJECTIVE_H) + QIHUD_CARD_GAP;
}

int CNewUIQuestInfoHUD::GetTotalHeight() const
{
	int height = QIHUD_HEADER_H;
	if (!m_bCollapsed)
	{
		for (int i = 0; i < QIHUD_MAX_QUESTS; ++i)
		{
			if (IsQuestVisible(m_Quest[i]))
				height += GetQuestHeight(m_Quest[i]);
		}
	}
	return height;
}

void CNewUIQuestInfoHUD::UpdateAnchor()
{
	m_Pos.x = max(0, (int)GetWindowsX - QIHUD_WIDTH - QIHUD_MARGIN_RIGHT);

	int maximumY = (int)GetWindowsY - QIHUD_BOTTOM_RESERVE - GetTotalHeight();
	if (maximumY < QIHUD_MIN_TOP)
		maximumY = QIHUD_MIN_TOP;
	m_Pos.y = min(QIHUD_TOP, maximumY);
}

void CNewUIQuestInfoHUD::GetHeaderFoldRect(int& x, int& y) const
{
	x = m_Pos.x + QIHUD_WIDTH - QIHUD_HEADER_CONTROL_W;
	y = m_Pos.y;
}

void CNewUIQuestInfoHUD::GetHeaderCheckRect(int& x, int& y) const
{
	x = m_Pos.x + QIHUD_PAD;
	y = m_Pos.y + 3;
}

void CNewUIQuestInfoHUD::GetQuestCollapseRect(int questY, int& x, int& y) const
{
	x = m_Pos.x + QIHUD_WIDTH - (QIHUD_CARD_CONTROL_W * 2) - 1;
	y = questY + 6;
}

void CNewUIQuestInfoHUD::GetQuestCloseRect(int questY, int& x, int& y) const
{
	x = m_Pos.x + QIHUD_WIDTH - QIHUD_CARD_CONTROL_W;
	y = questY + 6;
}

bool CNewUIQuestInfoHUD::Update()
{
	return true;
}

bool CNewUIQuestInfoHUD::UpdateMouseEvent()
{
	if (GetActiveQuestCount() == 0)
		return true;

	UpdateAnchor();

	int controlX = 0;
	int controlY = 0;
	GetHeaderFoldRect(controlX, controlY);
	if (CheckMouseIn(controlX, controlY, QIHUD_HEADER_CONTROL_W, QIHUD_HEADER_CONTROL_H))
	{
		if (IsPress(VK_LBUTTON))
		{
			m_bCollapsed = !m_bCollapsed;
			PlayBuffer(SOUND_CLICK01);
		}
		return false;
	}

	if (m_bCollapsed)
		return true;

	GetHeaderCheckRect(controlX, controlY);
	if (CheckMouseIn(controlX, controlY, QIHUD_CHECK_W, QIHUD_CHECK_H))
	{
		if (IsPress(VK_LBUTTON))
		{
			m_bShowCurrentMap = !m_bShowCurrentMap;
			PlayBuffer(SOUND_CLICK01);
		}
		return false;
	}

	if (!m_bCollapsed)
	{
		int questY = m_Pos.y + QIHUD_HEADER_H;
		for (int i = 0; i < QIHUD_MAX_QUESTS; ++i)
		{
			if (!IsQuestVisible(m_Quest[i]))
				continue;

			GetQuestCollapseRect(questY, controlX, controlY);
			if (CheckMouseIn(controlX, controlY, QIHUD_CARD_CONTROL_W, QIHUD_CARD_CONTROL_H))
			{
				if (IsPress(VK_LBUTTON))
				{
					m_Quest[i].minimized = !m_Quest[i].minimized;
					PlayBuffer(SOUND_CLICK01);
				}
				return false;
			}

			GetQuestCloseRect(questY, controlX, controlY);
			if (CheckMouseIn(controlX, controlY, QIHUD_CARD_CONTROL_W, QIHUD_CARD_CONTROL_H))
			{
				if (IsPress(VK_LBUTTON))
				{
					m_Quest[i].hidden = true;
					PlayBuffer(SOUND_CLICK01);
				}
				return false;
			}

			questY += GetQuestHeight(m_Quest[i]);
		}
	}

	if (CheckMouseIn(m_Pos.x, m_Pos.y, QIHUD_WIDTH, GetTotalHeight()))
		return false;

	return true;
}

bool CNewUIQuestInfoHUD::UpdateKeyEvent()
{
	return true;
}

void CNewUIQuestInfoHUD::RenderAtlas(int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh, bool hover, float opacity) const
{
	glColor4f(hover ? 1.0f : 0.90f, hover ? 1.0f : 0.90f, hover ? 1.0f : 0.90f, opacity);
	SEASON3B::RenderImageF(IMAGE_QIHUD_ATLAS, (float)dx, (float)dy, (float)dw, (float)dh,
		(float)sx, (float)sy, (float)sw, (float)sh);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void CNewUIQuestInfoHUD::RenderCheckBox(int x, int y, bool checked, bool hover) const
{
	const int image = checked ? IMAGE_QIHUD_CHECKED : IMAGE_QIHUD_UNCHECKED;
	const float brightness = hover ? 1.0f : 0.82f;
	glColor4f(brightness, brightness, brightness, 1.f);
	SEASON3B::RenderImage(image, (float)x, (float)y,
		(float)QIHUD_CHECK_W, (float)QIHUD_CHECK_H, 0.f, 0.f, 0.75f, 0.75f);
	glColor4f(1.f, 1.f, 1.f, 1.f);
}

void CNewUIQuestInfoHUD::RenderFoldButton(int x, int y, bool hover) const
{
	const float sourceX = hover ? 32.f : 0.f;
	const float sourceY = m_bCollapsed ? 28.f : 0.f;
	SEASON3B::RenderImageF(IMAGE_QIHUD_FOLD, (float)x, (float)y,
		(float)QIHUD_HEADER_CONTROL_W, (float)QIHUD_HEADER_CONTROL_H,
		sourceX, sourceY, 32.f, 28.f);
}

bool CNewUIQuestInfoHUD::Render()
{
	if (GetActiveQuestCount() == 0)
		return true;

	UpdateAnchor();

	const int x = m_Pos.x;
	const int y = m_Pos.y;

	::EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetBgColor(0);

	int foldX = 0;
	int foldY = 0;
	GetHeaderFoldRect(foldX, foldY);
	bool foldHover = CheckMouseIn(foldX, foldY, QIHUD_HEADER_CONTROL_W, QIHUD_HEADER_CONTROL_H);
	if (m_bCollapsed)
	{
		RenderFoldButton(foldX, foldY, foldHover);
		::DisableAlphaBlend();
		return true;
	}

	RenderAtlas(x, y, QIHUD_WIDTH, QIHUD_HEADER_H,
		SRC_HEADER_X, SRC_HEADER_Y, SRC_HEADER_W, SRC_HEADER_H, true);

	int checkX = 0;
	int checkY = 0;
	GetHeaderCheckRect(checkX, checkY);
	bool checkHover = CheckMouseIn(checkX, checkY, QIHUD_CHECK_W, QIHUD_CHECK_H);
	RenderCheckBox(checkX, checkY, m_bShowCurrentMap, checkHover);

	g_pRenderText->SetTextColor(235, 238, 245, 255);
	g_pRenderText->RenderText(x + 16, y + 4, "Show quests in this map");

	RenderFoldButton(foldX, foldY, foldHover);

	if (!m_bCollapsed)
	{
		int questY = y + QIHUD_HEADER_H;
		for (int i = 0; i < QIHUD_MAX_QUESTS; ++i)
		{
			QUEST_TRACK_ENTRY& entry = m_Quest[i];
			if (!IsQuestVisible(entry))
				continue;

			RenderAtlas(x, questY, QIHUD_TITLE_W, QIHUD_TITLE_H,
				SRC_QUEST_MAIN_X, SRC_QUEST_MAIN_Y, SRC_QUEST_MAIN_W, SRC_QUEST_MAIN_H, true);
			RenderAtlas(x + 7, questY + 6, 12, 12,
				SRC_ICON_ACTIVE_MAIN_X, SRC_ICON_ACTIVE_MAIN_Y,
				SRC_ICON_ACTIVE_MAIN_W, SRC_ICON_ACTIVE_MAIN_H, true);

			g_pRenderText->SetTextColor(253, 194, 67, 255);
			g_pRenderText->RenderText(x + QIHUD_TEXT_LEFT, questY + 8, entry.title,
				QIHUD_TITLE_W - QIHUD_TEXT_LEFT - (QIHUD_CARD_CONTROL_W * 2) - 3, 0, RT3_SORT_LEFT);

			int collapseX = 0;
			int collapseY = 0;
			int closeX = 0;
			int closeY = 0;
			GetQuestCollapseRect(questY, collapseX, collapseY);
			GetQuestCloseRect(questY, closeX, closeY);
			bool cardHover = CheckMouseIn(x, questY, QIHUD_WIDTH, GetQuestHeight(entry));
			if (cardHover)
			{
				bool collapseHover = CheckMouseIn(collapseX, collapseY, QIHUD_CARD_CONTROL_W, QIHUD_CARD_CONTROL_H);
				RenderAtlas(collapseX, collapseY, QIHUD_CARD_CONTROL_W, QIHUD_CARD_CONTROL_H,
					entry.minimized ? (collapseHover ? SRC_SHOW_HOVER_X : SRC_SHOW_NORMAL_X) : (collapseHover ? SRC_HIDE_HOVER_X : SRC_HIDE_NORMAL_X),
					entry.minimized ? (collapseHover ? SRC_SHOW_HOVER_Y : SRC_SHOW_NORMAL_Y) : (collapseHover ? SRC_HIDE_HOVER_Y : SRC_HIDE_NORMAL_Y),
					entry.minimized ? SRC_SHOW_NORMAL_W : SRC_HIDE_NORMAL_W,
					entry.minimized ? SRC_SHOW_NORMAL_H : SRC_HIDE_NORMAL_H, collapseHover);

				bool closeHover = CheckMouseIn(closeX, closeY, QIHUD_CARD_CONTROL_W, QIHUD_CARD_CONTROL_H);
				RenderAtlas(closeX, closeY, QIHUD_CARD_CONTROL_W, QIHUD_CARD_CONTROL_H,
					closeHover ? SRC_CLOSE_HOVER_X : SRC_CLOSE_NORMAL_X,
					closeHover ? SRC_CLOSE_HOVER_Y : SRC_CLOSE_NORMAL_Y,
					SRC_CLOSE_NORMAL_W, SRC_CLOSE_NORMAL_H, closeHover);
			}

			if (!entry.minimized)
			{
				int objectiveY = questY + QIHUD_TITLE_H;
				RenderAtlas(x + QIHUD_OBJECTIVE_LEFT, objectiveY, QIHUD_OBJECTIVE_W, QIHUD_OBJECTIVE_H,
					SRC_MISSION_MINI_X, SRC_MISSION_MINI_Y, SRC_MISSION_MINI_W, SRC_MISSION_MINI_H, true, 0.72f);

				bool complete = (entry.maximum > 0 && entry.current >= entry.maximum);
				g_pRenderText->SetTextColor(complete ? RGBA(160, 160, 160, 255) : RGBA(235, 238, 245, 255));
				g_pRenderText->RenderText(x + QIHUD_OBJECTIVE_TEXT_LEFT, objectiveY + 5, entry.objective,
					QIHUD_OBJECTIVE_W - 44, 0, RT3_SORT_LEFT);

				char countText[32];
				wsprintf(countText, "%u/%u", entry.current, entry.maximum);
				g_pRenderText->SetTextColor(complete ? RGBA(160, 160, 160, 255) : RGBA(255, 255, 255, 255));
				g_pRenderText->RenderText(x + QIHUD_OBJECTIVE_LEFT + QIHUD_OBJECTIVE_W - QIHUD_PAD,
					objectiveY + 5, countText, 0, 0, RT3_WRITE_RIGHT_TO_LEFT);
			}

			questY += GetQuestHeight(entry);
		}
	}

	::DisableAlphaBlend();
	return true;
}

float CNewUIQuestInfoHUD::GetLayerDepth()
{
	return 3.0f;
}
