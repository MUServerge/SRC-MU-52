//*****************************************************************************
// File: NewUIQuestInfoHUD.h
// Look5 quest tracker anchored to the right side of the game viewport.
//*****************************************************************************
#pragma once

#include "NewUIBase.h"
#include "_TextureIndex.h"

#include <map>
#include <string>

namespace SEASON3B
{
	class CNewUIQuestInfoHUD : public CNewUIObj
	{
	public:
		enum IMAGE_LIST
		{
			IMAGE_QIHUD_ATLAS = BITMAP_INTERFACE_QUESTINFO_HUD_BEGIN,
			IMAGE_QIHUD_FOLD = BITMAP_INTERFACE_QUESTINFO_HUD_BEGIN + 1,
			IMAGE_QIHUD_MONSTER_MARKER = BITMAP_INTERFACE_QUESTINFO_HUD_BEGIN + 2,
			IMAGE_QIHUD_CHECKED = BITMAP_INTERFACE_QUESTINFO_HUD_BEGIN + 3,
			IMAGE_QIHUD_UNCHECKED = BITMAP_INTERFACE_QUESTINFO_HUD_BEGIN + 4,
		};

		// Exact named rectangles from QuestSystemMissionView.json (705x696).
		enum eAtlasRect
		{
			ATLAS_W = 705,
			ATLAS_H = 696,

			SRC_HEADER_X = 297,
			SRC_HEADER_Y = 227,
			SRC_HEADER_W = 324,
			SRC_HEADER_H = 28,

			SRC_QUEST_MAIN_X = 297,
			SRC_QUEST_MAIN_Y = 182,
			SRC_QUEST_MAIN_W = 296,
			SRC_QUEST_MAIN_H = 44,

			SRC_MISSION_MINI_X = 0,
			SRC_MISSION_MINI_Y = 272,
			SRC_MISSION_MINI_W = 254,
			SRC_MISSION_MINI_H = 30,

			SRC_FOLD_SHOW_X = 594,
			SRC_FOLD_SHOW_Y = 182,
			SRC_FOLD_SHOW_W = 32,
			SRC_FOLD_SHOW_H = 28,

			SRC_FOLD_HIDE_X = 627,
			SRC_FOLD_HIDE_Y = 182,
			SRC_FOLD_HIDE_W = 32,
			SRC_FOLD_HIDE_H = 28,

			SRC_ICON_ACTIVE_MAIN_X = 195,
			SRC_ICON_ACTIVE_MAIN_Y = 462,
			SRC_ICON_ACTIVE_MAIN_W = 20,
			SRC_ICON_ACTIVE_MAIN_H = 20,

			SRC_SHOW_NORMAL_X = 655,
			SRC_SHOW_NORMAL_Y = 227,
			SRC_SHOW_NORMAL_W = 20,
			SRC_SHOW_NORMAL_H = 24,
			SRC_SHOW_HOVER_X = 685,
			SRC_SHOW_HOVER_Y = 98,
			SRC_SHOW_HOVER_W = 20,
			SRC_SHOW_HOVER_H = 24,

			SRC_HIDE_NORMAL_X = 229,
			SRC_HIDE_NORMAL_Y = 328,
			SRC_HIDE_NORMAL_W = 20,
			SRC_HIDE_NORMAL_H = 24,

			SRC_HIDE_HOVER_X = 195,
			SRC_HIDE_HOVER_Y = 397,
			SRC_HIDE_HOVER_W = 20,
			SRC_HIDE_HOVER_H = 24,

			SRC_CLOSE_NORMAL_X = 195,
			SRC_CLOSE_NORMAL_Y = 372,
			SRC_CLOSE_NORMAL_W = 20,
			SRC_CLOSE_NORMAL_H = 24,

			SRC_CLOSE_HOVER_X = 676,
			SRC_CLOSE_HOVER_Y = 227,
			SRC_CLOSE_HOVER_W = 20,
			SRC_CLOSE_HOVER_H = 24,

		};

		// Virtual UI coordinates. Look5 converts these to the real backbuffer.
		enum eLayout
		{
			QIHUD_MAX_QUESTS = 5,
			QIHUD_WIDTH = 180,
			QIHUD_MARGIN_RIGHT = 0,
			QIHUD_TOP = 218,
			QIHUD_MIN_TOP = 72,
			QIHUD_BOTTOM_RESERVE = 74,

			QIHUD_HEADER_H = 16,
			QIHUD_TITLE_H = 25,
			QIHUD_OBJECTIVE_H = 17,
			QIHUD_CARD_GAP = 3,

			QIHUD_TITLE_W = 180,
			QIHUD_OBJECTIVE_W = 141,
			QIHUD_OBJECTIVE_LEFT = 25,
			QIHUD_TEXT_LEFT = 25,
			QIHUD_OBJECTIVE_TEXT_LEFT = 30,
			QIHUD_PAD = 4,

			QIHUD_HEADER_CONTROL_W = 18,
			QIHUD_HEADER_CONTROL_H = 16,
			QIHUD_CHECK_W = 10,
			QIHUD_CHECK_H = 10,
			QIHUD_CARD_CONTROL_W = 11,
			QIHUD_CARD_CONTROL_H = 13,
		};

		struct QUEST_TRACK_ENTRY
		{
			bool active;
			bool minimized;
			bool hidden;
			BYTE map;
			WORD quest;
			WORD monsterClass;
			char title[64];
			char objective[64];
			DWORD current;
			DWORD maximum;
		};

	private:
		CNewUIManager*	m_pNewUIMng;
		POINT			m_Pos;
		bool			m_bCollapsed;
		bool			m_bShowCurrentMap;
		QUEST_TRACK_ENTRY m_Quest[QIHUD_MAX_QUESTS];
		std::map<WORD, std::string> m_QuestTitles;

	public:
		CNewUIQuestInfoHUD();
		virtual ~CNewUIQuestInfoHUD();

		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		void Release();

		void SetPos(int x, int y);

		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();
		float GetLayerDepth();

		void RecvQuestProgress(BYTE* lpMsg, int size);
		void UpsertQuest(WORD quest, BYTE map, WORD monsterClass, const char* objective, DWORD current, DWORD maximum);
		void RemoveQuest(WORD quest);
		void ClearQuests();
		bool IsTrackedMonster(WORD monsterClass) const;
		void RenderMonsterMarker(WORD monsterClass, int centerX, int healthBarY) const;

	private:
		void UpdateAnchor();
		int GetActiveQuestCount() const;
		bool IsQuestVisible(const QUEST_TRACK_ENTRY& quest) const;
		int GetQuestHeight(const QUEST_TRACK_ENTRY& quest) const;
		int GetTotalHeight() const;
		int FindQuest(WORD quest) const;
		int FindFreeQuest() const;
		void GetHeaderFoldRect(int& x, int& y) const;
		void GetHeaderCheckRect(int& x, int& y) const;
		void GetQuestCollapseRect(int questY, int& x, int& y) const;
		void GetQuestCloseRect(int questY, int& x, int& y) const;
		void RenderAtlas(int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh, bool hover, float opacity = 1.0f) const;
		void RenderCheckBox(int x, int y, bool checked, bool hover) const;
		void RenderFoldButton(int x, int y, bool hover) const;
		void LoadQuestTexts();
		const char* GetQuestTitle(WORD quest) const;
		void LoadImages();
		void UnloadImages();
	};
}
