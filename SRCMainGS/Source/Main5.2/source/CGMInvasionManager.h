#pragma once
#include "NewUIQuestProgress.h"

#define MAX_INVASION		50

typedef struct
{
	int MonsterIndex;
	int Monster_Kill;
	int MonsterCount;
} INVASION_MONSTER_INFO;

typedef struct
{
	int index;
	std::string Name;
	DWORD TimeRemaing;
	std::vector<INVASION_MONSTER_INFO> total_monster;
}INVASION_GLOBAL_INFO;

typedef std::map<int, INVASION_GLOBAL_INFO> type_map_invasion;

namespace SEASON3B
{
	class CGMInvasionManager
	{
		// MuDream-style skin loaded from Interface\ActiveInvasion\*.OZT.
		// Legacy IMAGE_ND_BTN_L/R aliases kept so any old caller reads the same value as before.
		enum IMAGE_LIST
		{
			IMAGE_ACTINV_BACKGROUND     = BITMAP_INTERFACE_ACTINV_BACKGROUND,
			IMAGE_ACTINV_BACKGROUND_NEW = BITMAP_INTERFACE_ACTINV_BACKGROUND_NEW,
			IMAGE_ACTINV_ARROWS         = BITMAP_INTERFACE_ACTINV_ARROWS,
			IMAGE_ACTINV_BTN            = BITMAP_INTERFACE_ACTINV_BTN,
			IMAGE_ACTINV_CLOSE          = BITMAP_INTERFACE_ACTINV_CLOSE,
			IMAGE_ACTINV_PLUS           = BITMAP_INTERFACE_ACTINV_PLUS,
			IMAGE_ACTINV_MINUS          = BITMAP_INTERFACE_ACTINV_MINUS,
			IMAGE_ACTINV_NOTIFY         = BITMAP_INTERFACE_ACTINV_NOTIFY,
			IMAGE_ACTINV_NOTIFY_EFFECT_BEGIN = BITMAP_INTERFACE_LUERYS_NOTIFY_BEGIN,
			IMAGE_ACTINV_NOTIFY_EFFECT_END   = BITMAP_INTERFACE_LUERYS_NOTIFY_END,
			IMAGE_NOTIFY_CHARACTER_NORMAL = BITMAP_INTERFACE_IBERIA_NOTIFICATION_BEGIN,
			IMAGE_NOTIFY_CHARACTER_HOVER,
			IMAGE_NOTIFY_CHARACTER_DISABLED,
			IMAGE_NOTIFY_SHIELD_NORMAL,
			IMAGE_NOTIFY_SHIELD_HOVER,
			IMAGE_NOTIFY_SHIELD_DISABLED,
			IMAGE_NOTIFY_SWORDS_NORMAL,
			IMAGE_NOTIFY_SWORDS_HOVER,
			IMAGE_NOTIFY_SWORDS_DISABLED,
			IMAGE_NOTIFY_INVASION_NORMAL,
			IMAGE_NOTIFY_INVASION_HOVER,
			IMAGE_NOTIFY_INVASION_DISABLED,

			IMAGE_ND_BTN_L = CNewUIQuestProgress::IMAGE_QP_BTN_L,
			IMAGE_ND_BTN_R = CNewUIQuestProgress::IMAGE_QP_BTN_R,
		};
	public:
		CGMInvasionManager();
		virtual~CGMInvasionManager();

		void SetInvasion(int Index, DWORD TimeRemaing, char* Name);
		void SetMonsterKill(int Index, int MonsterIndex, int Monster_Kill, int MonsterCount);
		void SetMonsterCount(int Index, int MonsterIndex, int Monster_Kill, int MonsterCount);
		void SetPos(float RenderFrameX, float RenderFrameY);

		void Update();
		bool UpdateMouseEvent();
		bool IsHudNotificationHovered() const;
		void RenderFrame(float RenderFrameX, float RenderFrameY);

		// Look-5 HUD notification strip above the AG gauge: invasion, character points and daily.
		void RenderNotifyIcon(float fAgX, float fAgY, float fAgW);

		void NextInvasion();
		void PreviousInvasion();

		INVASION_GLOBAL_INFO* FindInvasion(int Index);
	private:
		// Skin textures live at Interface\ActiveInvasion\ and are shared across every invasion row.
		// Loaded lazily on the first RenderFrame (OpenGL must be up), freed in the destructor.
		void LoadImages();
		void UnloadImages();
		void RefreshActiveState();

		int m_CountActive;
		bool is_Opentable;
		bool m_bLoaded;		// skin OZTs bound? (lazy, since Instance() runs before GL is ready)
		bool m_bHidden;		// user clicked the header close (X) - reset when a new invasion arrives
		POINT m_Pos;
		float m_RenderFrameX;
		float m_RenderFrameY;
		// Notify-icon top-left, cached by RenderNotifyIcon for the hit test. RenderFrame parks it
		// off-screen every frame, so the icon is only clickable on frames the HUD actually drew it.
		float m_NotifyIconX;
		float m_NotifyIconY;
		float m_CharacterIconX;
		float m_CharacterIconY;
		float m_ShieldIconX;
		float m_ShieldIconY;
		float m_SwordsIconX;
		float m_SwordsIconY;
		float m_DailyIconX;
		float m_DailyIconY;
		bool m_bDragging;
		float m_DragOffsetX;
		float m_DragOffsetY;
		int currentInvasion;
		std::chrono::steady_clock::time_point last_time;
		type_map_invasion InvasionInfo;
		//INVASION_GLOBAL_INFO [MAX_INVASION];
	public:
		static CGMInvasionManager* Instance() {
			static CGMInvasionManager sInstance;
			return &sInstance;
		};
	};
}

#define GMInvasionManager			(SEASON3B::CGMInvasionManager::Instance())
