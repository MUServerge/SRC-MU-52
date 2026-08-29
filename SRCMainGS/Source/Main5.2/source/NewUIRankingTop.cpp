#include "stdafx.h"
#include "NewUISystem.h"
#include "NewUIRankingTop.h"
#include "ZzzTexture.h"
#include "wsclientinline.h"
#include "CharacterManager.h"
#include "DSPlaySound.h"

using namespace SEASON3B;

namespace
{
	//-- Original frame: two 183px plates, the right one carrying the close
	//-- button. Nothing here may grow it.
	const float RANK_WIDTH = 366.f;
	const float RANK_HEIGHT = 230.f;
	const float RANK_TITLE_HEIGHT = 25.f;
	const float RANK_TITLE_OFFSET = 30.f;		//. caption bar sits above m_Pos.y
	const float BACK_TILE_WIDTH = 183.f;

	//-- List area, unchanged from the original window.
	const float LIST_LEFT = 162.f;
	const float LIST_WIDTH = 184.f;				//. the scroll bar lives at +350
	const float LIST_TOP = 50.f;
	const float ROW_HEIGHT = 16.f;

	const float PAGE_BAR_Y = 12.f;
	const float PAGE_BAR_HEIGHT = 16.f;
	const float PAGE_ARROW_WIDTH = 14.f;
	const float HEADER_Y = 32.f;
	const float HEADER_HEIGHT = 16.f;

	//-- Merged table: # | Class | Name | Master Reset | Reset | Level
	const float COL_RANK = 18.f;
	const float COL_CLASS = 58.f;
	const float COL_NAME = 46.f;
	const float COL_BOARD = 20.f;

	//-- Event page: # | Name | Class | Score
	const float EVT_RANK = 20.f;
	const float EVT_NAME = 60.f;
	const float EVT_CLASS = 62.f;
	const float EVT_SCORE = 42.f;

	const float REQUEST_TIMEOUT = 5000.f;		//. WorldTime is in milliseconds
}

SEASON3B::CNewUIRankingTop::CNewUIRankingTop()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;

	m_iBoardCount = 0;
	m_iPage = 0;
	m_iSelectRow = -1;
	m_iHoverRow = -1;
	m_iPendingBoard = -1;
	m_fRequestTime = 0.f;

	m_bDragging = false;
	m_fDragOffsetX = 0.f;
	m_fDragOffsetY = 0.f;
}

SEASON3B::CNewUIRankingTop::~CNewUIRankingTop()
{
	Release();
}

bool SEASON3B::CNewUIRankingTop::Create(CNewUIManager* pNewUIMng, float x, float y)
{
	if (pNewUIMng == NULL)
		return false;

	m_pNewUIMng = pNewUIMng;

	m_pNewUIMng->AddUIObj(INTERFACE_RANKING_TOP, this);

	this->LoadImages();

	this->SetPos(x, y);

	this->SetInfo();

	this->Show(false);

	return true;
}

void SEASON3B::CNewUIRankingTop::Release()
{
	if (m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);

		this->UnloadImages();

		for (int i = 0; i < MAX_RANKING_BOARD; ++i)
			m_Board[i].Entries.clear();

		m_Table.clear();

		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUIRankingTop::SetInfo()
{
	m_RenderCharacter.Init(0);
	m_RenderCharacter.SetArrangeType(1, 119, 30);
	m_RenderCharacter.SetSize(141, 184);
	m_RenderCharacter.CopyPlayer();
	m_RenderCharacter.SetAutoupdatePlayer(TRUE);
	m_RenderCharacter.SetAnimation(AT_STAND1);
	m_RenderCharacter.SetAngle(90.f);
	m_RenderCharacter.SetZoom(0.80f);

	m_pScrollBar.Create(m_Pos.x + 350, m_Pos.y + 51, 169);
	m_pScrollBar.SetPercent(0.0);
}

void SEASON3B::CNewUIRankingTop::SetPos(float x, float y)
{
	m_Pos.x = (LONG)x;
	m_Pos.y = (LONG)y;

	UpdateChildPositions();
}

void SEASON3B::CNewUIRankingTop::UpdateChildPositions()
{
	m_RenderCharacter.SetPosition((float)m_Pos.x + 12.f, (float)m_Pos.y + 34.f);

	m_pScrollBar.SetPos(m_Pos.x + 350, m_Pos.y + 51);
}

void SEASON3B::CNewUIRankingTop::ClampPosition()
{
	//-- The caption bar is drawn above m_Pos.y, so the window cannot go higher
	//-- than its own title.
	const float fTop = RANK_TITLE_OFFSET;
	const float maxX = ((float)GetWindowsX > RANK_WIDTH) ? ((float)GetWindowsX - RANK_WIDTH) : 0.f;
	const float maxY = ((float)GetWindowsY > RANK_HEIGHT) ? ((float)GetWindowsY - RANK_HEIGHT) : fTop;

	if ((float)m_Pos.x < 0.f)
		m_Pos.x = 0;
	else if ((float)m_Pos.x > maxX)
		m_Pos.x = (LONG)maxX;

	if ((float)m_Pos.y < fTop)
		m_Pos.y = (LONG)fTop;
	else if ((float)m_Pos.y > maxY)
		m_Pos.y = (LONG)maxY;

	UpdateChildPositions();
}

void SEASON3B::CNewUIRankingTop::LoadImages()
{
	LoadBitmap("Interface\\HUD\\top_back_1.tga", IMAGE_TOP_BACK1, GL_LINEAR);
	LoadBitmap("Interface\\HUD\\top_back_2.tga", IMAGE_TOP_BACK2, GL_LINEAR);
	LoadBitmap("Interface\\HUD\\VipLevel1.tga", IMAGE_TOP_LEVEL1, GL_LINEAR, GL_CLAMP_TO_EDGE, true, false);
	LoadBitmap("Interface\\HUD\\VipLevel2.tga", IMAGE_TOP_LEVEL2, GL_LINEAR, GL_CLAMP_TO_EDGE, true, false);
	LoadBitmap("Interface\\HUD\\VipLevel3.tga", IMAGE_TOP_LEVEL3, GL_LINEAR, GL_CLAMP_TO_EDGE, true, false);
}

void SEASON3B::CNewUIRankingTop::UnloadImages()
{
	DeleteBitmap(IMAGE_TOP_BACK1);
	DeleteBitmap(IMAGE_TOP_BACK2);
	DeleteBitmap(IMAGE_TOP_LEVEL1);
	DeleteBitmap(IMAGE_TOP_LEVEL2);
	DeleteBitmap(IMAGE_TOP_LEVEL3);
}

//////////////////////////////////////////////////////////////////////
// Layout - drawing and hit testing share these
//////////////////////////////////////////////////////////////////////

void SEASON3B::CNewUIRankingTop::GetListRect(float& x, float& y, float& width, float& height) const
{
	x = (float)m_Pos.x + LIST_LEFT;
	y = (float)m_Pos.y + LIST_TOP;
	width = LIST_WIDTH;
	height = ROW_HEIGHT * (float)ROWS_VISIBLE;
}

//-- The visible SLOT, not the absolute row: the original hit-tested with the
//-- absolute index while it drew with the slot, so once the list was scrolled
//-- every click landed on the wrong row.
void SEASON3B::CNewUIRankingTop::GetRowRect(int iVisibleSlot, float& x, float& y, float& width, float& height) const
{
	x = (float)m_Pos.x + LIST_LEFT;
	y = (float)m_Pos.y + LIST_TOP + ROW_HEIGHT * (float)iVisibleSlot;
	width = LIST_WIDTH;
	height = ROW_HEIGHT;
}

void SEASON3B::CNewUIRankingTop::GetPageArrowRect(bool bRight, float& x, float& y, float& width, float& height) const
{
	width = PAGE_ARROW_WIDTH;
	height = PAGE_BAR_HEIGHT;
	y = (float)m_Pos.y + PAGE_BAR_Y;
	x = bRight
		? ((float)m_Pos.x + LIST_LEFT + LIST_WIDTH - width)
		: ((float)m_Pos.x + LIST_LEFT);
}

void SEASON3B::CNewUIRankingTop::GetTitleRect(float& x, float& y, float& width, float& height) const
{
	x = (float)m_Pos.x;
	y = (float)m_Pos.y - RANK_TITLE_OFFSET;
	width = RANK_WIDTH;
	height = RANK_TITLE_HEIGHT;
}

void SEASON3B::CNewUIRankingTop::GetCloseRect(float& x, float& y, float& width, float& height) const
{
	x = (float)m_Pos.x + 344.f;
	y = (float)m_Pos.y + 3.5f;
	width = 19.f;
	height = 19.f;
}

//////////////////////////////////////////////////////////////////////
// Pages
//////////////////////////////////////////////////////////////////////

int SEASON3B::CNewUIRankingTop::GetPageCount() const
{
	const int iEvents = (m_iBoardCount > RANK_MERGED_BOARDS) ? (m_iBoardCount - RANK_MERGED_BOARDS) : 0;

	return 1 + iEvents;
}

int SEASON3B::CNewUIRankingTop::GetPageBoard() const
{
	if (m_iPage <= 0)
		return -1;

	return RANK_MERGED_BOARDS + (m_iPage - 1);
}

int SEASON3B::CNewUIRankingTop::GetRowCount() const
{
	const int iBoard = GetPageBoard();

	if (iBoard < 0)
		return (int)m_Table.size();

	if (iBoard >= MAX_RANKING_BOARD)
		return 0;

	return (int)m_Board[iBoard].Entries.size();
}

int SEASON3B::CNewUIRankingTop::GetFirstVisibleRow()
{
	const int iCount = GetRowCount();

	if (iCount <= ROWS_VISIBLE)
		return 0;

	return (int)((double)(iCount - ROWS_VISIBLE) * m_pScrollBar.GetPercent());
}

void SEASON3B::CNewUIRankingTop::StepPage(int iDelta)
{
	const int iCount = GetPageCount();

	if (iCount <= 1)
		return;

	m_iPage = (m_iPage + iDelta) % iCount;

	if (m_iPage < 0)
		m_iPage += iCount;

	m_iSelectRow = -1;
	m_iHoverRow = -1;

	m_pScrollBar.SetPercent(0.0);

	PlayBuffer(SOUND_CLICK01);
}

//////////////////////////////////////////////////////////////////////
// Data
//////////////////////////////////////////////////////////////////////

//-- Folds the merged boards into one row per player. Built when a board
//-- arrives, never per frame.
void SEASON3B::CNewUIRankingTop::RebuildTable()
{
	m_Table.clear();

	for (int iBoard = 0; iBoard < RANK_MERGED_BOARDS && iBoard < m_iBoardCount; ++iBoard)
	{
		const SRankingBoard& Board = m_Board[iBoard];

		for (int iEntry = 0; iEntry < (int)Board.Entries.size(); ++iEntry)
		{
			const SRankingEntry& Entry = Board.Entries[iEntry];

			int iFound = -1;

			for (int i = 0; i < (int)m_Table.size(); ++i)
			{
				if (_stricmp(m_Table[i].szName, Entry.szName) == 0)
				{
					iFound = i;
					break;
				}
			}

			if (iFound < 0)
			{
				SRankingMerged Row;

				memset(&Row, 0, sizeof(Row));

				strncpy(Row.szName, Entry.szName, sizeof(Row.szName) - 1);
				strncpy(Row.szClass, Entry.szClass, sizeof(Row.szClass) - 1);

				for (int i = 0; i < RANK_MERGED_BOARDS; ++i)
					strcpy_s(Row.szScore[i], "-");

				Row.sBoard = (short)iBoard;
				Row.sEntry = (short)iEntry;

				m_Table.push_back(Row);

				iFound = (int)m_Table.size() - 1;
			}

			m_Table[iFound].iScore[iBoard] = Entry.iScore;
			sprintf_s(m_Table[iFound].szScore[iBoard], "%d", Entry.iScore);
		}
	}

	//-- Master resets first, then resets, then level.
	std::sort(m_Table.begin(), m_Table.end(), [](const SRankingMerged& a, const SRankingMerged& b)
	{
		if (a.iScore[0] != b.iScore[0]) return a.iScore[0] > b.iScore[0];
		if (a.iScore[1] != b.iScore[1]) return a.iScore[1] > b.iScore[1];
		return a.iScore[2] > b.iScore[2];
	});

	for (int i = 0; i < (int)m_Table.size(); ++i)
		sprintf_s(m_Table[i].szRank, "%02d", i + 1);

	if (m_iSelectRow >= (int)m_Table.size())
		m_iSelectRow = -1;
}

//////////////////////////////////////////////////////////////////////
// Requests
//////////////////////////////////////////////////////////////////////

void SEASON3B::CNewUIRankingTop::RequestServerRankingInfo(int iBoard)
{
	if (iBoard < 0 || iBoard >= m_iBoardCount || iBoard >= MAX_RANKING_BOARD)
		return;

	if (m_iPendingBoard >= 0)
		return;

	m_iPendingBoard = iBoard;
	m_fRequestTime = WorldTime;

	SendRequestRankingInfo((BYTE)iBoard);
}

void SEASON3B::CNewUIRankingTop::PumpRequests()
{
	if (m_iPendingBoard >= 0)
	{
		if (WorldTime - m_fRequestTime < REQUEST_TIMEOUT)
			return;

		//-- Dropped by the server (board switched off, bad type). Skip it and
		//-- carry on: the original flag was only cleared by a reply, so one
		//-- unanswered request wedged the window for good.
		m_iPendingBoard = -1;
	}

	for (int iBoard = 0; iBoard < m_iBoardCount && iBoard < MAX_RANKING_BOARD; ++iBoard)
	{
		if (m_Board[iBoard].bLoaded == false)
		{
			RequestServerRankingInfo(iBoard);
			return;
		}
	}
}

//-- Shows the picked player in the preview.
void SEASON3B::CNewUIRankingTop::SelectRow(int iRow)
{
	const SRankingEntry* pEntry = NULL;

	const int iBoard = GetPageBoard();

	if (iBoard < 0)
	{
		if (iRow < 0 || iRow >= (int)m_Table.size())
			return;

		const SRankingMerged& Row = m_Table[iRow];

		if (Row.sBoard >= 0 && Row.sBoard < MAX_RANKING_BOARD
			&& Row.sEntry >= 0 && Row.sEntry < (int)m_Board[Row.sBoard].Entries.size())
		{
			pEntry = &m_Board[Row.sBoard].Entries[Row.sEntry];
		}
	}
	else
	{
		if (iBoard >= MAX_RANKING_BOARD || iRow < 0 || iRow >= (int)m_Board[iBoard].Entries.size())
			return;

		pEntry = &m_Board[iBoard].Entries[iRow];
	}

	if (pEntry == NULL)
		return;

	m_iSelectRow = iRow;

	//-- Rebuild the preview from scratch. A mount does not only add a helper
	//-- object, it also puts the character into a riding pose, and that state
	//-- lives on the CHARACTER - clearing the helper by hand left the next
	//-- player still sitting on the previous one's Fenrir.
	m_RenderCharacter.Init(0);
	m_RenderCharacter.SetArrangeType(1, 119, 30);
	m_RenderCharacter.SetSize(141, 184);
	m_RenderCharacter.SetPosition((float)m_Pos.x + 12.f, (float)m_Pos.y + 34.f);
	m_RenderCharacter.SetAutoupdatePlayer(FALSE);
	m_RenderCharacter.SetAngle(90.f);
	m_RenderCharacter.SetZoom(0.80f);

	//-- Class first: SetEquipmentPacket() dresses whatever body is set.
	m_RenderCharacter.SetClass(pEntry->byClass);
	m_RenderCharacter.SetEquipmentPacket(const_cast<DWORD*>(pEntry->dwEquipment));
	m_RenderCharacter.SetID(pEntry->szName);
	m_RenderCharacter.SetAnimation(AT_STAND1);

	PlayBuffer(SOUND_CLICK01);
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

bool SEASON3B::CNewUIRankingTop::UpdateDragEvent()
{
	if (m_bDragging)
	{
		if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
		{
			m_bDragging = false;
		}
		else
		{
			m_Pos.x = (LONG)((float)MouseX - m_fDragOffsetX);
			m_Pos.y = (LONG)((float)MouseY - m_fDragOffsetY);
			ClampPosition();
		}
		return true;
	}

	float x, y, width, height;

	GetCloseRect(x, y, width, height);
	if (SEASON3B::CheckMouseIn(x, y, width, height))
		return false;

	GetTitleRect(x, y, width, height);
	if (SEASON3B::CheckMouseIn(x, y, width, height) && SEASON3B::IsPress(VK_LBUTTON))
	{
		m_bDragging = true;
		m_fDragOffsetX = (float)MouseX - (float)m_Pos.x;
		m_fDragOffsetY = (float)MouseY - (float)m_Pos.y;
		return true;
	}

	return false;
}

bool SEASON3B::CNewUIRankingTop::UpdateKeyEvent()
{
	if (IsVisible() == true)
	{
		if (SEASON3B::IsPress(VK_ESCAPE))
		{
			g_pNewUISystem->Hide(INTERFACE_RANKING_TOP);
			return false;
		}

		if (SEASON3B::IsRelease(VK_LEFT))
		{
			StepPage(-1);
			return false;
		}

		if (SEASON3B::IsRelease(VK_RIGHT))
		{
			StepPage(1);
			return false;
		}
	}

	return true;
}

bool SEASON3B::CNewUIRankingTop::UpdateMouseEvent()
{
	if (UpdateDragEvent())
		return false;

	const bool bInside = SEASON3B::CheckMouseIn((float)m_Pos.x, (float)m_Pos.y, RANK_WIDTH, RANK_HEIGHT) != 0;

	float x, y, width, height;

	GetCloseRect(x, y, width, height);
	if (SEASON3B::CheckMouseIn(x, y, width, height) && SEASON3B::IsRelease(VK_LBUTTON))
	{
		g_pNewUISystem->Hide(INTERFACE_RANKING_TOP);
		return false;
	}

	m_iHoverRow = -1;

	if (bInside == false)
		return true;

	GetPageArrowRect(false, x, y, width, height);
	if (SEASON3B::CheckMouseIn(x, y, width, height) && SEASON3B::IsRelease(VK_LBUTTON))
	{
		StepPage(-1);
		return false;
	}

	GetPageArrowRect(true, x, y, width, height);
	if (SEASON3B::CheckMouseIn(x, y, width, height) && SEASON3B::IsRelease(VK_LBUTTON))
	{
		StepPage(1);
		return false;
	}

	const int iCount = GetRowCount();
	const int iFirst = GetFirstVisibleRow();

	GetListRect(x, y, width, height);

	if (iCount > ROWS_VISIBLE)
	{
		if (SEASON3B::CheckMouseIn(x, y, width, height) && MouseWheel != 0)
		{
			double prev = m_pScrollBar.GetPercent();

			//-- One row per notch.
			prev += ((MouseWheel > 0) ? -1.0 : 1.0) / (double)(iCount - ROWS_VISIBLE);

			if (prev < 0.0) prev = 0.0;
			else if (prev > 1.0) prev = 1.0;

			MouseWheel = 0;
			m_pScrollBar.SetPercent(prev);
		}

		m_pScrollBar.UpdateMouseEvent();
	}

	for (int iSlot = 0; iSlot < ROWS_VISIBLE; ++iSlot)
	{
		const int iRow = iFirst + iSlot;

		if (iRow >= iCount)
			break;

		GetRowRect(iSlot, x, y, width, height);

		if (SEASON3B::CheckMouseIn(x, y, width, height))
		{
			m_iHoverRow = iRow;

			if (SEASON3B::IsRelease(VK_LBUTTON))
				SelectRow(iRow);

			break;
		}
	}

	m_RenderCharacter.DoMouseAction();

	return false;
}

//////////////////////////////////////////////////////////////////////
// Frame
//////////////////////////////////////////////////////////////////////

bool SEASON3B::CNewUIRankingTop::Update()
{
	if (IsVisible())
	{
		m_pScrollBar.Update();

		PumpRequests();
	}

	return true;
}

bool SEASON3B::CNewUIRankingTop::Render()
{
	EnableAlphaTest(true);

	glColor4f(1.f, 1.f, 1.f, 1.f);

	this->RenderFrame();

	this->RenderTexte();

	DisableAlphaBlend();

	m_RenderCharacter.Render();

	return true;
}

float SEASON3B::CNewUIRankingTop::GetLayerDepth()
{
	return 10.0f;
}

void SEASON3B::CNewUIRankingTop::OpenningProcess()
{
	m_iPendingBoard = -1;
	m_iPage = 0;
	m_iSelectRow = -1;
	m_iHoverRow = -1;
	m_bDragging = false;

	for (int i = 0; i < MAX_RANKING_BOARD; ++i)
	{
		m_Board[i].bLoaded = false;
		m_Board[i].Entries.clear();
	}

	m_Table.clear();

	m_pScrollBar.SetPercent(0.0);

	m_RenderCharacter.SetAutoupdatePlayer(TRUE);
	m_RenderCharacter.CopyPlayer();

	ClampPosition();

	SendRequestMaxRanking();
}

void SEASON3B::CNewUIRankingTop::ClosingProcess()
{
	m_bDragging = false;
}

void SEASON3B::CNewUIRankingTop::RenderFrame()
{
	const float x = (float)m_Pos.x;
	const float y = (float)m_Pos.y;

	RenderImageF(IMAGE_TOP_BACK1, x, y, BACK_TILE_WIDTH, RANK_HEIGHT, 0.0, 0.0, 732.0, 917.0);
	RenderImageF(IMAGE_TOP_BACK2, x + BACK_TILE_WIDTH, y, BACK_TILE_WIDTH, RANK_HEIGHT, 0.0, 0.0, 732.0, 917.0);
}

void SEASON3B::CNewUIRankingTop::RenderHeader()
{
	const int iBoard = GetPageBoard();

	//-- Page bar: which ranking is on screen, with the arrows either side.
	const float bx = (float)m_Pos.x + LIST_LEFT;
	const float by = (float)m_Pos.y + PAGE_BAR_Y;

	EnableAlphaTest(true);
	glColor4ub(38, 44, 56, 255);
	RenderColor(bx, by, LIST_WIDTH, PAGE_BAR_HEIGHT);
	EndRenderColor();

	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(CLRDW_GOLD);
	g_pRenderText->RenderFont((int)bx, (int)by, (iBoard < 0) ? "Ranking" : m_Board[iBoard].szName,
		(int)LIST_WIDTH, (int)PAGE_BAR_HEIGHT, RT3_SORT_CENTER);

	if (GetPageCount() > 1)
	{
		float ax, ay, aw, ah;

		GetPageArrowRect(false, ax, ay, aw, ah);
		g_pRenderText->SetTextColor(SEASON3B::CheckMouseIn(ax, ay, aw, ah) ? CLRDW_WHITE : CLRDW_BR_GRAY);
		g_pRenderText->RenderFont((int)ax, (int)ay, "<", (int)aw, (int)ah, RT3_SORT_CENTER);

		GetPageArrowRect(true, ax, ay, aw, ah);
		g_pRenderText->SetTextColor(SEASON3B::CheckMouseIn(ax, ay, aw, ah) ? CLRDW_WHITE : CLRDW_BR_GRAY);
		g_pRenderText->RenderFont((int)ax, (int)ay, ">", (int)aw, (int)ah, RT3_SORT_CENTER);
	}

	//-- Column headers for whichever page is up.
	static const char* s_pszFallback[RANK_MERGED_BOARDS] = { "MR", "RS", "LV" };

	const float hy = (float)m_Pos.y + HEADER_Y;

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(-1);
	g_pRenderText->SetBgColor(52, 57, 67, 255);

	float x = (float)m_Pos.x + LIST_LEFT;

	if (iBoard < 0)
	{
		g_pRenderText->RenderFont((int)x, (int)hy, "#", (int)COL_RANK, (int)HEADER_HEIGHT, RT3_SORT_CENTER);
		x += COL_RANK;

		g_pRenderText->RenderFont((int)x, (int)hy, GlobalText[1973], (int)COL_CLASS, (int)HEADER_HEIGHT, RT3_SORT_CENTER);
		x += COL_CLASS;

		g_pRenderText->RenderFont((int)x, (int)hy, GlobalText[1389], (int)COL_NAME, (int)HEADER_HEIGHT, RT3_SORT_CENTER);
		x += COL_NAME;

		for (int i = 0; i < RANK_MERGED_BOARDS; ++i)
		{
			g_pRenderText->RenderFont((int)x, (int)hy, s_pszFallback[i], (int)COL_BOARD, (int)HEADER_HEIGHT, RT3_SORT_CENTER);
			x += COL_BOARD;
		}
	}
	else
	{
		g_pRenderText->RenderFont((int)x, (int)hy, "#", (int)EVT_RANK, (int)HEADER_HEIGHT, RT3_SORT_CENTER);
		x += EVT_RANK;

		g_pRenderText->RenderFont((int)x, (int)hy, GlobalText[1389], (int)EVT_NAME, (int)HEADER_HEIGHT, RT3_SORT_CENTER);
		x += EVT_NAME;

		g_pRenderText->RenderFont((int)x, (int)hy, GlobalText[1973], (int)EVT_CLASS, (int)HEADER_HEIGHT, RT3_SORT_CENTER);
		x += EVT_CLASS;

		g_pRenderText->RenderFont((int)x, (int)hy, m_Board[iBoard].szScoreColumn, (int)EVT_SCORE, (int)HEADER_HEIGHT, RT3_SORT_CENTER);
	}

	g_pRenderText->SetBgColor(0);
}

void SEASON3B::CNewUIRankingTop::RenderRows()
{
	const int iBoard = GetPageBoard();
	const int iCount = GetRowCount();
	const int iFirst = GetFirstVisibleRow();

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetBgColor(0);

	for (int iSlot = 0; iSlot < ROWS_VISIBLE; ++iSlot)
	{
		const int iRow = iFirst + iSlot;

		if (iRow >= iCount)
			break;

		float rx, ry, rw, rh;
		GetRowRect(iSlot, rx, ry, rw, rh);

		if (m_iHoverRow == iRow || m_iSelectRow == iRow)
		{
			EnableAlphaTest(true);
			glColor4ub(79, 86, 100, 255);
			RenderColor(rx, ry, rw, rh - 1.f);
			EndRenderColor();
		}

		g_pRenderText->SetTextColor(-1);

		float x = rx;

		if (iBoard < 0)
		{
			const SRankingMerged& Row = m_Table[iRow];

			g_pRenderText->RenderFont((int)x, (int)ry, Row.szRank, (int)COL_RANK, (int)ROW_HEIGHT, RT3_SORT_CENTER);
			x += COL_RANK;

			g_pRenderText->RenderFont((int)x, (int)ry, Row.szClass, (int)COL_CLASS, (int)ROW_HEIGHT, RT3_SORT_CENTER);
			x += COL_CLASS;

			g_pRenderText->RenderFont((int)x, (int)ry, Row.szName, (int)COL_NAME, (int)ROW_HEIGHT, RT3_SORT_CENTER);
			x += COL_NAME;

			for (int i = 0; i < RANK_MERGED_BOARDS; ++i)
			{
				g_pRenderText->RenderFont((int)x, (int)ry, Row.szScore[i], (int)COL_BOARD, (int)ROW_HEIGHT, RT3_SORT_CENTER);
				x += COL_BOARD;
			}
		}
		else
		{
			const SRankingEntry& Entry = m_Board[iBoard].Entries[iRow];

			g_pRenderText->RenderFont((int)x, (int)ry, Entry.szRank, (int)EVT_RANK, (int)ROW_HEIGHT, RT3_SORT_CENTER);
			x += EVT_RANK;

			g_pRenderText->RenderFont((int)x, (int)ry, Entry.szName, (int)EVT_NAME, (int)ROW_HEIGHT, RT3_SORT_CENTER);
			x += EVT_NAME;

			g_pRenderText->RenderFont((int)x, (int)ry, Entry.szClass, (int)EVT_CLASS, (int)ROW_HEIGHT, RT3_SORT_CENTER);
			x += EVT_CLASS;

			g_pRenderText->RenderFont((int)x, (int)ry, Entry.szScore, (int)EVT_SCORE, (int)ROW_HEIGHT, RT3_SORT_CENTER);
		}
	}

	if (iCount > ROWS_VISIBLE)
		m_pScrollBar.Render();
}

void SEASON3B::CNewUIRankingTop::RenderTexte()
{
	//-- Caption bar
	float tx, ty, tw, th;
	GetTitleRect(tx, ty, tw, th);

	glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
	RenderColor(tx, ty, tw, th, 0.0, 0);
	EndRenderColor();

	g_pRenderText->SetBgColor(0);
	g_pRenderText->SetTextColor(CLRDW_GOLD);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->RenderText((int)(tx + tw * 0.5f), (int)ty, GlobalText[3236], 0, (int)th, RT3_WRITE_CENTER);

	RenderHeader();

	RenderRows();
}

//////////////////////////////////////////////////////////////////////
// Protocol
//////////////////////////////////////////////////////////////////////

void SEASON3B::CNewUIRankingTop::ReceiveRankingInfo(BYTE* ReceiveBuffer)
{
	LPPWHEADER_DEFAULT_WORD Data = (LPPWHEADER_DEFAULT_WORD)ReceiveBuffer;

	int iCount = (int)Data->Value;

	if (iCount < 0)
		iCount = 0;
	else if (iCount > MAX_RANKING_BOARD)
		iCount = MAX_RANKING_BOARD;

	m_iBoardCount = iCount;

	if (m_iPage >= GetPageCount())
		m_iPage = 0;

	PumpRequests();
}

void SEASON3B::CNewUIRankingTop::ReceiveRankingListInfo(BYTE* ReceiveBuffer)
{
	LPPHEADER_RANKING_LIST Data = (LPPHEADER_RANKING_LIST)ReceiveBuffer;

	const int iBoard = Data->index;

	if (iBoard < 0 || iBoard >= MAX_RANKING_BOARD)
		return;

	if (m_iBoardCount <= iBoard)
		m_iBoardCount = iBoard + 1;

	SRankingBoard& Board = m_Board[iBoard];

	Board.Entries.clear();

	strncpy(Board.szName, Data->rankname, sizeof(Board.szName) - 1);
	Board.szName[sizeof(Board.szName) - 1] = 0;

	strncpy(Board.szScoreColumn, Data->col2, sizeof(Board.szScoreColumn) - 1);
	Board.szScoreColumn[sizeof(Board.szScoreColumn) - 1] = 0;

	int offset = sizeof(PHEADER_RANKING_LIST);

	const int iEntryCount = (Data->count < 0) ? 0 : Data->count;

	Board.Entries.reserve(iEntryCount);

	for (int n = 0; n < iEntryCount; ++n)
	{
		LPPCREATE_RANKING_INFO Data2 = (LPPCREATE_RANKING_INFO)(ReceiveBuffer + offset);

		SRankingEntry Entry;

		Entry.byClass = gCharacterManager.ChangeServerClassTypeToClientClassType(Data2->PlayerClass);
		Entry.byVip = Data2->LevelVip;
		Entry.iScore = Data2->TotalScore;

		memcpy(Entry.dwEquipment, Data2->Equipment, sizeof(Entry.dwEquipment));

		sprintf_s(Entry.szRank, "%02d", n + 1);

		strncpy(Entry.szName, Data2->Name, sizeof(Entry.szName) - 1);
		Entry.szName[sizeof(Entry.szName) - 1] = 0;

		const char* pszClass = gCharacterManager.GetCharacterClassText(Entry.byClass);
		strncpy(Entry.szClass, (pszClass != NULL) ? pszClass : "", sizeof(Entry.szClass) - 1);
		Entry.szClass[sizeof(Entry.szClass) - 1] = 0;

		sprintf_s(Entry.szScore, "%d", Entry.iScore);

		Board.Entries.push_back(Entry);

		offset += sizeof(PCREATE_RANKING_INFO);
	}

	//-- The stored procedure already returns each board in its own order; the
	//-- original re-sorted by descending score here, which scrambled any board
	//-- whose best score is the lowest one.
	Board.bLoaded = true;

	RebuildTable();

	if (m_iPendingBoard == iBoard)
		m_iPendingBoard = -1;

	//-- Chain the next board here rather than from Update(): the UI manager
	//-- aborts its whole Update loop as soon as one window returns false, so a
	//-- window above this one can stop Update() from ever reaching us.
	PumpRequests();
}
