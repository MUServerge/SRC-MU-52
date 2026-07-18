#include "stdafx.h"
#include "jpexs.h"
#include "UIBaseDef.h"
#include "UIControls.h"
#include "CGMInvasionManager.h"
#include "HudTooltip.h"
#include "NewUISystem.h"
#include "ZzzInterface.h"

extern int FontHeight;
extern char* getMonsterName(int type);

using namespace SEASON3B;

static_assert(BITMAP_INTERFACE_IBERIA_NOTIFICATION_END < BITMAP_EFFECT_TEXTURE_END,
	"Iberia notification bitmap range overlaps the effect texture boundary");

//-----------------------------------------------------------------------------
// MuDream-style skin (Interface\ActiveInvasion\*.OZT) - layout constants.
// Every rect below is in virtual 640x480 pixels; RenderImageF / CheckMouseIn
// scale by g_fScreenRate under the hood, so nothing here hard-codes real pixels.
//-----------------------------------------------------------------------------
namespace
{
	// Widget footprint. Width MUST stay in sync with SetPos(): the caller passes
	// the center X and SetPos shifts left by (ACTINV_W / 2) to anchor the top-left.
	// The MuDream art is 2x resolution, so everything renders at native size / 2 -
	// no stretching in either direction.
	const float ACTINV_W          = 136.5f;	// background native width 273 / 2
	const float ACTINV_HEADER_H   = 25.f;	// background.png native height 50 / 2
	const float ACTINV_ROW_H      = 16.5f;
	const float ACTINV_BODY_INSET = 8.f;	// skip the transparent ornamental margins of the header art

	// Prev/Next chevron atlas (arrows.png = 96x156, 2 cols x 3 rows, cell = 48x52).
	// Rows: 0=normal, 1=hover, 2=pressed; cols: 0=left chevron, 1=right chevron.
	const float ATLAS_ARROW_W     = 48.f;
	const float ATLAS_ARROW_H     = 52.f;

	// Arrow button on-screen size + hit rect (virtual px), keeping the 48x52 cell aspect.
	const float BTN_ARROW_W       = 11.f;
	const float BTN_ARROW_H       = 12.f;

	// Icon buttons in the top-right corner.
	const float BTN_PM_SIZE       = 7.f;
	const float BTN_ICON_PAD      = 3.f;
	const float BTN_PM_PAD_RIGHT  = 11.5f;
	const float BTN_PM_OFFSET_Y   = -4.f;
	const float TITLE_OFFSET_Y    = -4.f;

	// Expanded body: transparent black layers. Overall dropdown is faint; list rows are darker.
	const float BODY_ALPHA        = 0.28f;
	const float ROW_ALPHA         = 0.44f;
	const float ROW_TEXT_PAD_X    = 6.f;
	const float ROW_TEXT_PAD_Y    = 2.f;

	// Native pixel sizes of the OZT art. The loader pads each image to a
	// power-of-2 canvas, so every draw must crop to the real rect via
	// RenderImageF - plain RenderImage assumes screen size == texture size
	// and samples the wrong region on these high-res MuDream textures.
	const float TEX_BG_W          = 273.f;	// background.png
	const float TEX_BG_H          = 50.f;
	const float TEX_ICON_SIZE     = 10.f;	// close/plus/minus.png (10x10 native)

	// Look-5 notification atlas: 11 cells of 72x72.
	const float NOTIFY_TEX_CELL   = 72.f;
	const int   NOTIFY_CELL_DAILY = 6;
	const float NOTIFY_ICON_SIZE  = 18.f;	// on-screen size (virtual px)
	const float NOTIFY_EFFECT_SIZE = 30.f;
	const DWORD NOTIFY_EFFECT_FRAME_TIME = 70;
	const int   NOTIFY_EFFECT_FRAME_COUNT = 18;
	const char* NOTIFY_EFFECT_FRAME_NAMES[NOTIFY_EFFECT_FRAME_COUNT] =
	{
		"IB4", "IB6", "IB8", "IBA", "IBC", "IBE",
		"IC0", "IC2", "IC4", "IC6", "IC8", "ICA",
		"ID0", "ID4", "ID6", "ID8", "IDA", "IDC",
	};

	void ClampWidgetPosition(float& x, float& y)
	{
		const float minY = 2.f;
		const float screenW = (float)GetWindowsX;
		const float screenH = (float)GetWindowsY;
		const float maxX = (screenW > ACTINV_W) ? (screenW - ACTINV_W) : 0.f;
		const float maxY = (screenH > ACTINV_HEADER_H + minY)
			? (screenH - ACTINV_HEADER_H)
			: minY;

		// Recover a position corrupted by an old build or a resolution change.
		// NaN fails every normal range comparison, so handle it explicitly.
		if (x != x)
			x = (maxX * 0.5f);
		if (y != y)
			y = minY;

		if (x < 0.f)
			x = 0.f;
		else if (x > maxX)
			x = maxX;

		if (y < minY)
			y = minY;
		else if (y > maxY)
			y = maxY;
	}
	const float NOTIFY_HOVER_GROW = 2.f;
	const float NOTIFY_ICON_GAP_X = 3.f;
	const float NOTIFY_ICON_PAD_X = 8.f;	// inset from the AG gauge's right edge
	const float NOTIFY_ICON_GAP_Y = 2.f;	// gap between icon bottom and AG gauge top

	inline bool ArrowHit(float bx, float by)
	{
		return SEASON3B::CheckMouseIn(bx, by, BTN_ARROW_W, BTN_ARROW_H);
	}
	inline bool IconHit(float ix, float iy, float size)
	{
		return SEASON3B::CheckMouseIn(ix, iy, size, size);
	}

}

CGMInvasionManager::CGMInvasionManager()
{
	m_CountActive = 0;
	is_Opentable = false;
	m_bLoaded = false;
	m_bHidden = false;
	currentInvasion = -1;
	last_time = std::chrono::steady_clock::now();

	m_Pos.x = 0;
	m_Pos.y = 0;
	m_RenderFrameX = 0;
	m_RenderFrameY = 0;
	m_NotifyIconX = -9999.f;
	m_NotifyIconY = -9999.f;
	m_CharacterIconX = -9999.f;
	m_CharacterIconY = -9999.f;
	m_ShieldIconX = -9999.f;
	m_ShieldIconY = -9999.f;
	m_SwordsIconX = -9999.f;
	m_SwordsIconY = -9999.f;
	m_DailyIconX = -9999.f;
	m_DailyIconY = -9999.f;
	m_bDragging = false;
	m_DragOffsetX = 0.f;
	m_DragOffsetY = 0.f;

	InvasionInfo.clear();
}

CGMInvasionManager::~CGMInvasionManager()
{
	UnloadImages();

	for (type_map_invasion::iterator it = InvasionInfo.begin(); it != InvasionInfo.end(); ++it)
	{
		it->second.total_monster.clear();
	}

	InvasionInfo.clear();
}

void CGMInvasionManager::LoadImages()
{
	// LoadBitmap(".tga") really opens the OZT next to it (see GlobalBitmap.cpp::OpenTga -
	// the client's TGA path swaps the extension to OZT). Filenames must match the OZTs the
	// user drops into Client\Data\Interface\ActiveInvasion.
	LoadBitmap("Interface\\ActiveInvasion\\background.tga",     IMAGE_ACTINV_BACKGROUND,     GL_LINEAR);
	LoadBitmap("Interface\\ActiveInvasion\\arrows.tga",         IMAGE_ACTINV_ARROWS,         GL_LINEAR);
	LoadBitmap("Interface\\ActiveInvasion\\plus.tga",           IMAGE_ACTINV_PLUS,           GL_LINEAR);
	LoadBitmap("Interface\\ActiveInvasion\\minus.tga",          IMAGE_ACTINV_MINUS,          GL_LINEAR);
	LoadBitmap("Interface\\HUD\\Look-5\\UI_HUD_NOTIFICATIONS.tga", IMAGE_ACTINV_NOTIFY,      GL_LINEAR);

	const int notificationFiles[] = { 78, 79, 80, 82, 83, 84, 85, 86, 87, 169, 170, 171 };
	char szNotificationPath[MAX_PATH];
	const int notificationFileCount = (int)(sizeof(notificationFiles) / sizeof(notificationFiles[0]));
	for (int i = 0; i < notificationFileCount; ++i)
	{
		sprintf_s(szNotificationPath, "Interface\\Iberia\\UI_Notification\\TournamentMain_I%d.tga",
			notificationFiles[i]);
		LoadBitmap(szNotificationPath, IMAGE_NOTIFY_CHARACTER_NORMAL + i, GL_LINEAR);
	}

	char szEffectPath[MAX_PATH];
	for (int i = 0; i < NOTIFY_EFFECT_FRAME_COUNT; ++i)
	{
		sprintf_s(szEffectPath, "Interface\\Iberia\\LuerysTreasureBox\\LuerysTreasureBox_%s.tga",
			NOTIFY_EFFECT_FRAME_NAMES[i]);
		LoadBitmap(szEffectPath, IMAGE_ACTINV_NOTIFY_EFFECT_BEGIN + i, GL_LINEAR);
	}

	m_bLoaded = true;
}

void CGMInvasionManager::UnloadImages()
{
	if (!m_bLoaded)
		return;

	DeleteBitmap(IMAGE_ACTINV_BACKGROUND);
	DeleteBitmap(IMAGE_ACTINV_ARROWS);
	DeleteBitmap(IMAGE_ACTINV_PLUS);
	DeleteBitmap(IMAGE_ACTINV_MINUS);
	DeleteBitmap(IMAGE_ACTINV_NOTIFY);

	for (int i = IMAGE_NOTIFY_CHARACTER_NORMAL; i <= IMAGE_NOTIFY_INVASION_DISABLED; ++i)
	{
		DeleteBitmap(i);
	}

	for (int i = IMAGE_ACTINV_NOTIFY_EFFECT_BEGIN; i <= IMAGE_ACTINV_NOTIFY_EFFECT_END; ++i)
	{
		DeleteBitmap(i);
	}

	m_bLoaded = false;
}

void CGMInvasionManager::SetInvasion(int Index, DWORD TimeRemaing, char* Name)
{
	if (Index != -1 && Index >= 0 && Index < MAX_INVASION)
	{
		INVASION_GLOBAL_INFO& invasion = InvasionInfo[Index];

		invasion.index = Index;
		invasion.Name = Name;
		invasion.TimeRemaing = TimeRemaing;
		invasion.total_monster.clear();

		// Fresh invasion should surface the widget again if the user closed it earlier.
		if (TimeRemaing > 0)
			m_bHidden = false;

		RefreshActiveState();
	}
}

void CGMInvasionManager::SetMonsterKill(int Index, int MonsterIndex, int Monster_Kill, int MonsterCount)
{
	if (Index != -1 && Index >= 0 && Index < MAX_INVASION)
	{
		INVASION_GLOBAL_INFO* invasion = FindInvasion(Index);

		if (invasion != NULL)
		{
			size_t length = invasion->total_monster.size();

			for (size_t i = 0; i < length; i++)
			{
				INVASION_MONSTER_INFO* info = &invasion->total_monster[i];

				if (info->MonsterIndex == MonsterIndex)
				{
					info->Monster_Kill = Monster_Kill;

					info->MonsterCount = MonsterCount;
				}
			}
		}
	}
}

void CGMInvasionManager::SetMonsterCount(int Index, int MonsterIndex, int Monster_Kill, int MonsterCount)
{
	if (Index != -1 && Index >= 0 && Index < MAX_INVASION)
	{
		INVASION_GLOBAL_INFO* invasion = FindInvasion(Index);

		if (invasion != NULL)
		{
			invasion->total_monster.push_back({ MonsterIndex, Monster_Kill, MonsterCount });
		}
	}
}

void CGMInvasionManager::Update()
{
	auto current_time = std::chrono::steady_clock::now();
	double difTime = std::chrono::duration<double>(current_time - last_time).count();

	if (difTime >= 1.0)
	{
		last_time = current_time;

		for (type_map_invasion::iterator it = InvasionInfo.begin(); it != InvasionInfo.end(); ++it)
		{
			INVASION_GLOBAL_INFO* invasion = &it->second;

			if (invasion->TimeRemaing > 0)
				invasion->TimeRemaing -= 1;
		}

		RefreshActiveState();
	}
}

void CGMInvasionManager::RefreshActiveState()
{
	m_CountActive = 0;
	int firstActive = -1;
	bool currentIsActive = false;

	for (type_map_invasion::iterator it = InvasionInfo.begin(); it != InvasionInfo.end(); ++it)
	{
		if (it->second.TimeRemaing == 0)
			continue;

		if (firstActive == -1)
			firstActive = it->first;

		if (it->first == currentInvasion)
			currentIsActive = true;

		++m_CountActive;
	}

	if (!currentIsActive)
		currentInvasion = firstActive;
}

void CGMInvasionManager::SetPos(float RenderFrameX, float RenderFrameY)
{
	// Caller passes CENTER X; we shift left by half-width so m_RenderFrame is the top-left corner.
	m_RenderFrameX = RenderFrameX - (ACTINV_W / 2.f);
	m_RenderFrameY = RenderFrameY;
	ClampWidgetPosition(m_RenderFrameX, m_RenderFrameY);
}

bool CGMInvasionManager::UpdateMouseEvent()
{
	if (m_bDragging)
	{
		if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
		{
			m_bDragging = false;
			return true;
		}
		else
		{
			m_RenderFrameX = (float)MouseX - m_DragOffsetX;
			m_RenderFrameY = (float)MouseY - m_DragOffsetY;
			ClampWidgetPosition(m_RenderFrameX, m_RenderFrameY);

			return true;
		}
	}

	if (SEASON3B::CheckMouseIn(m_CharacterIconX, m_CharacterIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE))
	{
		if (SEASON3B::IsRelease(VK_LBUTTON))
		{
			g_pNewUISystem->Toggle(SEASON3B::INTERFACE_CHARACTER);
			PlayBuffer(SOUND_CLICK01);
		}

		return true;
	}

	// Reserved notification icons have no action yet, but own their HUD hit areas.
	if (SEASON3B::CheckMouseIn(m_ShieldIconX, m_ShieldIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE)
		|| SEASON3B::CheckMouseIn(m_SwordsIconX, m_SwordsIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE))
	{
		return true;
	}

	// Daily Reward is visual-only for now, but still owns its HUD hit area.
	if (SEASON3B::CheckMouseIn(m_DailyIconX, m_DailyIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE))
		return true;

	// HUD notify icon first: it stays clickable while the widget itself is closed, so the user
	// can bring the widget back after hiding it with X. Its rect is only valid on frames the
	// HUD actually drew it (RenderFrame parks it off-screen otherwise).
	if (SEASON3B::CheckMouseIn(m_NotifyIconX, m_NotifyIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE))
	{
		if (m_CountActive > 0 && currentInvasion != -1 && SEASON3B::IsRelease(VK_LBUTTON))
			m_bHidden = !m_bHidden;

		return true;
	}

	// Same visibility gate as RenderFrame - stay in perfect lockstep so we can't click something
	// the user cannot see.
	if (m_bHidden || m_CountActive <= 0 || currentInvasion == -1)
		return false;

	ClampWidgetPosition(m_RenderFrameX, m_RenderFrameY);

	if (g_pNewUISystem->IsVisible(INTERFACE_MINI_MAP)
		|| g_pNewUISystem->IsVisible(INTERFACE_INGAMESHOP)
		|| g_pNewUISystem->IsVisible(INTERFACE_MASTER_LEVEL))
	{
		return false;
	}

	const float x = m_RenderFrameX;
	const float y = m_RenderFrameY;

	if (!SEASON3B::CheckMouseIn(x, y, ACTINV_W, ACTINV_HEADER_H))
		return false;

	// Only this button expands/collapses the list; the remaining header is the drag handle.
	const float bx_pm = x + ACTINV_W - BTN_PM_PAD_RIGHT - BTN_PM_SIZE;
	const float by_pm = y + (ACTINV_HEADER_H - BTN_PM_SIZE) / 2.f + BTN_PM_OFFSET_Y;

	if (IconHit(bx_pm, by_pm, BTN_PM_SIZE))
	{
		if (SEASON3B::IsRelease(VK_LBUTTON))
		{
			is_Opentable = !is_Opentable;
			PlayBuffer(SOUND_CLICK01);
		}
		return true;
	}

	// Prev/next arrows, guarded by having >1 active invasions (single invasion = no navigation).
	if (m_CountActive > 1)
	{
		const float ax_prev = x + BTN_ICON_PAD;
		const float ax_next = x + ACTINV_W - BTN_ICON_PAD - BTN_ARROW_W;
		const float ay      = y + (ACTINV_HEADER_H - BTN_ARROW_H) / 2.f;

		if (ArrowHit(ax_prev, ay))
		{
			if (SEASON3B::IsRelease(VK_LBUTTON))
				PreviousInvasion();
			return true;
		}
		if (ArrowHit(ax_next, ay))
		{
			if (SEASON3B::IsRelease(VK_LBUTTON))
				NextInvasion();
			return true;
		}
	}

	if (SEASON3B::IsPress(VK_LBUTTON))
	{
		m_bDragging = true;
		m_DragOffsetX = (float)MouseX - x;
		m_DragOffsetY = (float)MouseY - y;
		return true;
	}

	// Consume the full header so clicks never reach the world. It no longer toggles the body.
	return true;
}

bool CGMInvasionManager::IsHudNotificationHovered() const
{
	return SEASON3B::CheckMouseIn(m_CharacterIconX, m_CharacterIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE)
		|| SEASON3B::CheckMouseIn(m_ShieldIconX, m_ShieldIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE)
		|| SEASON3B::CheckMouseIn(m_SwordsIconX, m_SwordsIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE)
		|| SEASON3B::CheckMouseIn(m_DailyIconX, m_DailyIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE)
		|| SEASON3B::CheckMouseIn(m_NotifyIconX, m_NotifyIconY, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE);
}

void CGMInvasionManager::RenderFrame(float /*RenderFrameX*/, float /*RenderFrameY*/)
{
	// Rendering can run before the one-second Update tick after a fresh packet.
	// Rebuild the lightweight active index here so the widget appears immediately.
	RefreshActiveState();

	// Park the notify-icon rect off-screen; RenderNotifyIcon (called later in the frame by the
	// Look-5 HUD) re-arms it, so the icon hit test never fires on frames it was not drawn.
	m_NotifyIconX = -9999.f;
	m_NotifyIconY = -9999.f;
	m_CharacterIconX = -9999.f;
	m_CharacterIconY = -9999.f;
	m_ShieldIconX = -9999.f;
	m_ShieldIconY = -9999.f;
	m_SwordsIconX = -9999.f;
	m_SwordsIconY = -9999.f;
	m_DailyIconX = -9999.f;
	m_DailyIconY = -9999.f;

	if (m_bHidden || m_CountActive <= 0 || currentInvasion == -1)
		return;

	ClampWidgetPosition(m_RenderFrameX, m_RenderFrameY);

	if (g_pNewUISystem->IsVisible(INTERFACE_MINI_MAP)
		|| g_pNewUISystem->IsVisible(INTERFACE_INGAMESHOP)
		|| g_pNewUISystem->IsVisible(INTERFACE_MASTER_LEVEL))
	{
		return;
	}

	if (!m_bLoaded)
		LoadImages();

	INVASION_GLOBAL_INFO* invasion = FindInvasion(currentInvasion);

	const float x = m_RenderFrameX;
	const float y = m_RenderFrameY;

	::EnableAlphaTest();
	glColor4f(1.f, 1.f, 1.f, 1.f);

	// ---- HEADER (ornate title bar) ----
	SEASON3B::RenderImageF(IMAGE_ACTINV_BACKGROUND, x, y, ACTINV_W, ACTINV_HEADER_H,
		0.f, 0.f, TEX_BG_W, TEX_BG_H);

	// Prev/next chevron buttons when there's more than one invasion to page through.
	if (m_CountActive > 1)
	{
		const float ax_prev = x + BTN_ICON_PAD;
		const float ax_next = x + ACTINV_W - BTN_ICON_PAD - BTN_ARROW_W;
		const float ay      = y + (ACTINV_HEADER_H - BTN_ARROW_H) / 2.f;

		const bool bHovPrev = ArrowHit(ax_prev, ay);
		const bool bHovNext = ArrowHit(ax_next, ay);

		// Row 0 = normal, row 1 = hover; keep pressed (row 2) available if needed later.
		const float src_prev_v = bHovPrev ? ATLAS_ARROW_H : 0.f;
		const float src_next_v = bHovNext ? ATLAS_ARROW_H : 0.f;

		SEASON3B::RenderImageF(IMAGE_ACTINV_ARROWS, ax_prev, ay, BTN_ARROW_W, BTN_ARROW_H,
			0.f, src_prev_v, ATLAS_ARROW_W, ATLAS_ARROW_H);
		SEASON3B::RenderImageF(IMAGE_ACTINV_ARROWS, ax_next, ay, BTN_ARROW_W, BTN_ARROW_H,
			ATLAS_ARROW_W, src_next_v, ATLAS_ARROW_W, ATLAS_ARROW_H);
	}

	// The notify icon hides/shows the widget, so only the compact expand control belongs here.
	const float bx_pm = x + ACTINV_W - BTN_PM_PAD_RIGHT - BTN_PM_SIZE;
	const float by_pm = y + (ACTINV_HEADER_H - BTN_PM_SIZE) / 2.f + BTN_PM_OFFSET_Y;

	SEASON3B::RenderImageF(is_Opentable ? IMAGE_ACTINV_MINUS : IMAGE_ACTINV_PLUS,
		bx_pm, by_pm, BTN_PM_SIZE, BTN_PM_SIZE, 0.f, 0.f, TEX_ICON_SIZE, TEX_ICON_SIZE);

	// Header title (invasion name, centered on the full bar like MuDream).
	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(CLRDW_GOLD);

	const char* pszName = (invasion != NULL && !invasion->Name.empty()) ? invasion->Name.c_str() : "Invasion";
	// RenderText expects backbuffer pixels; convert virtual->screen once here.
	const float titleX = x * g_fScreenRate_x;
	const float titleY = (y + (ACTINV_HEADER_H - (float)FontHeight) / 2.f + TITLE_OFFSET_Y) * g_fScreenRate_y;
	const float titleW = ACTINV_W * g_fScreenRate_x;
	const float titleH = (float)FontHeight * g_fScreenRate_y;
	g_pRenderText->RenderTextClipped(titleX, titleY, pszName, titleW, titleH, RT3_SORT_CENTER);

	// ---- BODY (monster list) ----
	if (is_Opentable && invasion != NULL && invasion->TimeRemaing > 0)
	{
		const size_t monster_count = invasion->total_monster.size();
		if (monster_count == 0)
			return;

		char pszText[128];
		float rowTop = y + ACTINV_HEADER_H;
		const float bodyH = ACTINV_ROW_H * (float)monster_count;
		const float bodyX = x + ACTINV_BODY_INSET;
		const float bodyW = ACTINV_W - ACTINV_BODY_INSET * 2.f;

		RenderColor(bodyX, rowTop, bodyW, bodyH, BODY_ALPHA, 1);

		for (size_t i = 0; i < monster_count; i++)
		{
			RenderColor(bodyX, rowTop, bodyW, ACTINV_ROW_H, ROW_ALPHA, 1);
			rowTop += ACTINV_ROW_H;
		}

		EndRenderColor();

		// Monster rows: name left in gold, kill count right in white.
		g_pRenderText->SetFont(g_hFont);
		rowTop = y + ACTINV_HEADER_H;

		for (size_t i = 0; i < monster_count; i++)
		{
			INVASION_MONSTER_INFO* Data = &invasion->total_monster[i];

			const float textY = (rowTop + ROW_TEXT_PAD_Y) * g_fScreenRate_y;
			const float textH = (ACTINV_ROW_H - ROW_TEXT_PAD_Y) * g_fScreenRate_y;

			g_pRenderText->SetTextColor(CLRDW_GOLD);
			g_pRenderText->RenderTextClipped((bodyX + ROW_TEXT_PAD_X) * g_fScreenRate_x, textY,
				getMonsterName(Data->MonsterIndex), 0, textH, RT3_SORT_LEFT);

			sprintf_s(pszText, "%d / %d", Data->Monster_Kill, Data->MonsterCount);
			g_pRenderText->SetTextColor(CLRDW_WHITE3);
			g_pRenderText->RenderTextClipped((bodyX + ROW_TEXT_PAD_X) * g_fScreenRate_x, textY,
				pszText, (bodyW - ROW_TEXT_PAD_X * 2.f) * g_fScreenRate_x, textH, RT3_SORT_RIGHT);

			rowTop += ACTINV_ROW_H;
		}
	}

	// Deliberately leave alpha-test ON: the caller (CNewUIMainFrameWindow::Render) enables it
	// once for the whole HUD pass, and everything drawn after us (Look-5 frame, gauges, skill
	// bar) still needs it - a DisableAlphaBlend() here painted the rest of the HUD opaque black.
}

void CGMInvasionManager::RenderNotifyIcon(float fAgX, float fAgY, float fAgW)
{
	RefreshActiveState();

	if (!m_bLoaded)
		LoadImages();

	const float rightX = fAgX + fAgW - NOTIFY_ICON_SIZE - NOTIFY_ICON_PAD_X;
	const float iconY = fAgY - NOTIFY_ICON_SIZE - NOTIFY_ICON_GAP_Y - 1.0f;

	m_DailyIconX = rightX;
	m_DailyIconY = iconY;
	m_SwordsIconX = rightX - NOTIFY_ICON_SIZE - NOTIFY_ICON_GAP_X;
	m_SwordsIconY = iconY;
	m_ShieldIconX = m_SwordsIconX - NOTIFY_ICON_SIZE - NOTIFY_ICON_GAP_X;
	m_ShieldIconY = iconY;
	m_CharacterIconX = m_ShieldIconX - NOTIFY_ICON_SIZE - NOTIFY_ICON_GAP_X;
	m_CharacterIconY = iconY;
	m_NotifyIconX = m_CharacterIconX - NOTIFY_ICON_SIZE - NOTIFY_ICON_GAP_X;
	m_NotifyIconY = iconY;

	struct HUD_ICON_DRAW
	{
		float x;
		float y;
		int imageNormal;
		int imageHover;
		int imageDisabled;
		float sourceWidth;
		float sourceHeight;
		int cell;
		const char* tip;
		bool enabled;
		bool atlas;
	};

	HUD_ICON_DRAW icons[] =
	{
		{ m_NotifyIconX,    m_NotifyIconY,    IMAGE_NOTIFY_INVASION_NORMAL,  IMAGE_NOTIFY_INVASION_HOVER,  IMAGE_NOTIFY_INVASION_DISABLED,  72.f,  72.f,  0,                 "Active Invasion ON / OFF", m_CountActive > 0 && currentInvasion != -1, false },
		{ m_CharacterIconX, m_CharacterIconY, IMAGE_NOTIFY_CHARACTER_NORMAL, IMAGE_NOTIFY_CHARACTER_HOVER, IMAGE_NOTIFY_CHARACTER_DISABLED, 56.f,  56.f,  0,                 "Free Level-up Points",      true,                                       false },
		{ m_ShieldIconX,    m_ShieldIconY,    IMAGE_NOTIFY_SHIELD_NORMAL,    IMAGE_NOTIFY_SHIELD_HOVER,    IMAGE_NOTIFY_SHIELD_DISABLED,    108.f, 120.f, 0,                 NULL,                        true,                                       false },
		{ m_SwordsIconX,    m_SwordsIconY,    IMAGE_NOTIFY_SWORDS_NORMAL,    IMAGE_NOTIFY_SWORDS_HOVER,    IMAGE_NOTIFY_SWORDS_DISABLED,    108.f, 120.f, 0,                 NULL,                        true,                                       false },
		{ m_DailyIconX,     m_DailyIconY,     IMAGE_ACTINV_NOTIFY,           IMAGE_ACTINV_NOTIFY,           IMAGE_ACTINV_NOTIFY,             72.f,  72.f,  NOTIFY_CELL_DAILY, "Daily Reward",              true,                                       true  },
	};

	glColor4f(1.f, 1.f, 1.f, 1.f);

	// The server-fed active invasion state already controls icon visibility. Reuse that
	// same authoritative state for the glow and keep the effect centered behind the icon.
	if (icons[0].enabled)
	{
		const int effectFrame = (GetTickCount() / NOTIFY_EFFECT_FRAME_TIME) % NOTIFY_EFFECT_FRAME_COUNT;
		const float effectX = m_NotifyIconX + (NOTIFY_ICON_SIZE - NOTIFY_EFFECT_SIZE) * 0.5f;
		const float effectY = m_NotifyIconY + (NOTIFY_ICON_SIZE - NOTIFY_EFFECT_SIZE) * 0.5f;
		const float sourceSize = (effectFrame == 0) ? 168.f : 232.f;

		SEASON3B::RenderImageF(IMAGE_ACTINV_NOTIFY_EFFECT_BEGIN + effectFrame,
			effectX, effectY, NOTIFY_EFFECT_SIZE, NOTIFY_EFFECT_SIZE,
			0.f, 0.f, sourceSize, sourceSize);
	}

	for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); ++i)
	{
		const bool hovered = SEASON3B::CheckMouseIn(icons[i].x, icons[i].y, NOTIFY_ICON_SIZE, NOTIFY_ICON_SIZE);
		const float grow = hovered ? NOTIFY_HOVER_GROW : 0.f;
		const float drawX = icons[i].x - grow * 0.5f;
		const float drawY = icons[i].y - grow * 0.5f;
		const float drawSize = NOTIFY_ICON_SIZE + grow;

		if (icons[i].atlas)
		{
			SEASON3B::RenderImageF(icons[i].imageNormal,
				drawX, drawY, drawSize, drawSize,
				icons[i].cell * NOTIFY_TEX_CELL, 0.f, NOTIFY_TEX_CELL, NOTIFY_TEX_CELL);
		}
		else
		{
			const int image = !icons[i].enabled ? icons[i].imageDisabled
				: (hovered ? icons[i].imageHover : icons[i].imageNormal);
			const float imageWidth = drawSize * icons[i].sourceWidth / icons[i].sourceHeight;
			const float imageX = drawX + (drawSize - imageWidth) * 0.5f;

			SEASON3B::RenderImageF(image,
				imageX, drawY, imageWidth, drawSize,
				0.f, 0.f, icons[i].sourceWidth, icons[i].sourceHeight);
		}

		if (hovered && icons[i].tip != NULL)
		{
			HudTooltip::RenderCenteredText(
				icons[i].x + NOTIFY_ICON_SIZE * 0.5f, drawY - 16.f, icons[i].tip);
		}
	}
}

void CGMInvasionManager::NextInvasion()
{
	auto it = InvasionInfo.find(currentInvasion);

LABEL_REPEAT:
	if (it == InvasionInfo.end() || std::next(it) == InvasionInfo.end())
	{
		return;
	}

	++it;

	if (it->second.TimeRemaing == 0)
	{
		goto LABEL_REPEAT;
	}

	currentInvasion = it->first;
}

void CGMInvasionManager::PreviousInvasion()
{
	auto it = InvasionInfo.find(currentInvasion);

LABEL_REPEAT:
	if (it == InvasionInfo.end() || it == InvasionInfo.begin())
	{
		return;
	}

	--it;

	if (it->second.TimeRemaing == 0)
	{
		goto LABEL_REPEAT;
	}

	currentInvasion = it->first;
}

INVASION_GLOBAL_INFO* CGMInvasionManager::FindInvasion(int Index)
{
	auto it = InvasionInfo.find(Index);

	if (it != InvasionInfo.end())
	{
		return &(it->second);
	}

	return NULL;
}
