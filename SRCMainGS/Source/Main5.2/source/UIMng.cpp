//*****************************************************************************
// File: UIMng.cpp
//*****************************************************************************

#include "stdafx.h"
#include "UIMng.h"
#include "Input.h"
#include "Sprite.h"
#include "GaugeBar.h"
#include "ZzzOpenglUtil.h"
#include "Zzzinfomation.h"
#include "ZzzBMD.h"
#include "ZzzObject.h"
#include "ZzzCharacter.h"
#include "ZzzInterface.h"
#include "GameCensorship.h"
#include "UIControls.h"
#include "ServerListManager.h"
#include "ConnectVersionHex.h"
#include "WINHANDLE.h"

#define	DOCK_EXTENT		10

CUIMng::CUIMng()
{
	m_pgbLoding = NULL;
	m_pLoadingScene = NULL;

#ifdef MOVIE_DIRECTSHOW
	m_bMoving = false;
#endif // MOVIE_DIRECTSHOW
}

CUIMng::~CUIMng()
{

}

CUIMng& CUIMng::Instance()
{
	static CUIMng s_UIMng;
	return s_UIMng;
}

// Loading-screen percentage readout: draws each digit from the KMUA2 number
// strip (its own texture slot, loaded/unloaded together with the rest of the
// KMUA2 art -- deliberately NOT the CNewUIRenderNumber singleton), then the
// Porcento ("%") icon right after the last digit.
// x/y come in already scaled to actual window pixels; sx/sy scale the digit
// size the same way, and RenderBitmap is called with Scale/StartScale = false so
// everything stays in raw window pixels (the whole loading screen must fill the
// window at any resolution/ScreenType, so it must NOT go through g_fScreenRate).
static float CRenderNumber(float x, float y, int iNum, float fScale, float sx, float sy)
{
	EnableAlphaTest(true);

	float width, height;

	if (fScale < 0.3f)
	{
		return x;
	}

	width = 12.f * (fScale - 0.3f) * sx;
	height = 16.f * (fScale - 0.3f) * sy;

	char strText[32];
	itoa(iNum, strText, 10);
	int iLength = (int)strlen(strText);

	x -= width * iLength / 2;

	for (int i = 0; i < iLength; ++i)
	{
		float fU = (float)(strText[i] - 48) * 12.f / 128.f;
		RenderBitmap(BITMAP_MUA2_LOADING_BEGIN + 6, x, y, width, height, fU, 0.f, 12.f / 128.f, 14.f / 16.f, 0, 0, 0.0);
		x += width * 0.8f;
	}

	RenderBitmap(BITMAP_MUA2_LOADING_BEGIN + 4, x, y, width + 3.f * sx, height, 0.f, 0.f, 9.f / 16.f, 14.f / 16.f, 0, 0, 0.0);

	return x;
}

void CUIMng::CreateTitleSceneUI()
{
	ReleaseTitleSceneUI();

	g_GameCensorship->SetVisible(true);
	g_GameCensorship->SetState(SEASON3A::CGameCensorship::STATE_LOADING);

	CInput& rInput = CInput::Instance();
	float fScaleX = (float)rInput.GetScreenWidth() / 800.0f;
	float fScaleY = (float)rInput.GetScreenHeight() / 600.0f;

	// KMUA2 boot loading screen art. Loaded here (not in CreateWebzenScene)
	// and released again right after the single render pass in WebzenScene().
	LoadBitmap("Interface\\KMUA2_Back01.jpg", BITMAP_MUA2_LOADING_BEGIN, GL_NEAREST, GL_CLAMP);
	LoadBitmap("Interface\\KMUA2_Back02.jpg", BITMAP_MUA2_LOADING_BEGIN + 1, GL_NEAREST, GL_CLAMP);
	LoadBitmap("Interface\\KMUA2_Bar.jpg", BITMAP_MUA2_LOADING_BEGIN + 2, GL_LINEAR, GL_REPEAT);
	LoadBitmap("Interface\\KMUA2_BarFrame.tga", BITMAP_MUA2_LOADING_BEGIN + 3, GL_LINEAR, GL_REPEAT);
	LoadBitmap("Interface\\KMUA2_Porcento.tga", BITMAP_MUA2_LOADING_BEGIN + 4, GL_LINEAR, GL_REPEAT);
	LoadBitmap("Interface\\KMUA2_Loading.tga", BITMAP_MUA2_LOADING_BEGIN + 5, GL_LINEAR, GL_REPEAT);
	LoadBitmap("Interface\\newui_number1.tga", BITMAP_MUA2_LOADING_BEGIN + 6, GL_LINEAR, GL_CLAMP);

	m_pgbLoding = new CGaugeBar;

	RECT rc = { -36, 4, 656 + 36, 8 };
	m_pgbLoding->Create(4, 15, BITMAP_MUA2_LOADING_BEGIN + 2, &rc, 0, 0, -1, true, fScaleX, fScaleY);

	m_pgbLoding->SetPosition(72, 540);
	m_pgbLoding->Show();
	m_nScene = UIM_SCENE_TITLE;
}

void CUIMng::ReleaseTitleSceneUI()
{
	g_GameCensorship->SetVisible(false);

	SAFE_DELETE(m_pgbLoding);

	m_nScene = UIM_SCENE_NONE;
}

void CUIMng::RenderTitleSceneUI(HDC hDC, DWORD dwNow, DWORD dwTotal)
{
	::BeginOpengl();
	::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	::BeginBitmap();

	// The loading screen is a fullscreen splash: it must cover the ENTIRE window
	// at any resolution AND any ScreenType. RenderBitmap's built-in scaling uses
	// g_fScreenRate, which on ScreenType 1 (aspect-lock) and ScreenType 2 (fixed
	// rate) does NOT map to the full window -- it leaves black margins. So we
	// scale the 640x480-authored art by the ACTUAL window size instead and pass
	// Scale/StartScale = false so RenderBitmap uses these raw pixel coords as-is.
	// (This matches the gauge bar, which likewise keys off the real window size.)
	const float sx = (float)WindowWidth / 640.0f;
	const float sy = (float)WindowHeight / 480.0f;

	// Back01/Back02 are the left/right halves of the scene -> one per window half,
	// so the full artwork shows with no black bars and no arbitrary crop.
	RenderBitmap(BITMAP_MUA2_LOADING_BEGIN, 0.0f, -1.0f * sy, 320.0f * sx, 480.0f * sy, 0, 0, 1.0, 1.0, 0, 0, 0.0);			// Back01 (left half)
	RenderBitmap(BITMAP_MUA2_LOADING_BEGIN + 1, 320.0f * sx, -1.0f * sy, 320.0f * sx, 480.0f * sy, 0, 0, 1.0, 1.0, 0, 0, 0.0);	// Back02 (right half)
	RenderBitmap(BITMAP_MUA2_LOADING_BEGIN + 5, 0.0f, 406.0f * sy, 640.0f * sx, 74.0f * sy, 0, 0, 1.0, 0.94, 0, 0, 0.0);		// Loading strip

	m_pgbLoding->SetValue(dwNow, dwTotal);
	m_pgbLoding->Render();

	DWORD dwSafeTotal = (dwTotal < 1) ? 1 : dwTotal;

	float WH, rate, SizeNew, Porcento;
	double AnimX, AnimY;

	WH = 0.25f;
	int Anim = (int)(timeGetTime() * 0.5f) % 600 / 40;
	AnimX = (double)(Anim % 4) * WH;
	AnimY = (double)(Anim / 4) * WH;

	// SizeNew is in 640x480 reference units (bar is 580 units wide); scaled to
	// actual window pixels by sx/sy below.
	rate = 580.f / (float)dwSafeTotal;
	SizeNew = rate * dwNow;
	RenderBitmap(BITMAP_MUA2_LOADING_BEGIN + 3, (SizeNew - 27.0f) * sx, 391.0f * sy, 81.0f * sx, 81.0f * sy, AnimX, AnimY, WH, WH, 0, 0, 0.0);	// BarFrame

	glColor3f(0.84f, 0.85f, 0.86f);
	rate = 100.f / (float)dwSafeTotal;
	Porcento = rate * dwNow;
	CRenderNumber(590.0f * sx, 454.0f * sy, (int)Porcento, 1.0f, sx, sy);
	glColor3f(1.0f, 1.0f, 1.0f);

	GMConnectHex->data_end();

	::EndBitmap();
	::EndOpengl();
	::glFlush();
	::SwapBuffers(hDC);
}

void CUIMng::Create()
{
	m_bCursorOnUI = false;
	m_bBlockCharMove = false;
	m_bWinActive = false;
	m_nScene = UIM_SCENE_NONE;

	return;
}

void CUIMng::RemoveWinList()
{
	CWin* pWin;
	while (m_WinList.GetCount())
	{
		pWin = (CWin*)m_WinList.RemoveHead();
		pWin->Release();
	}
}

void CUIMng::Release()
{
	RemoveWinList();

	m_CharInfoBalloonMng.Release();

	m_nScene = UIM_SCENE_NONE;
}

void CUIMng::CreateLoginScene()
{
	g_GameCensorship->SetVisible(false);

	RemoveWinList();

	m_CharInfoBalloonMng.Release();

	CInput& rInput = CInput::Instance();

	m_MsgWin.Create();
	m_WinList.AddHead(&m_MsgWin);
	m_MsgWin.SetPosition((rInput.GetScreenWidth() - 352) / 2,
		(rInput.GetScreenHeight() - 113) / 2);

	m_SysMenuWin.Create();
	m_WinList.AddHead(&m_SysMenuWin);

	m_OptionWin.Create();
	m_WinList.AddHead(&m_OptionWin);

	m_LoginMainWin.Create();
	m_WinList.AddHead(&m_LoginMainWin);

	int nBaseY = int(567.0f / 600.0f * (float)rInput.GetScreenHeight());
	m_LoginMainWin.SetPosition(30, nBaseY - m_LoginMainWin.GetHeight() - 11);

	m_ServerSelWin.Create();
	m_WinList.AddHead(&m_ServerSelWin);
	m_ServerSelWin.SetPosition(
		(rInput.GetScreenWidth() - m_ServerSelWin.GetWidth()) / 2,
		(rInput.GetScreenHeight() - m_ServerSelWin.GetHeight()) / 2);

	m_LoginWin.Create();
	m_WinList.AddHead(&m_LoginWin);
	m_LoginWin.SetPosition(
		(rInput.GetScreenWidth() - m_LoginWin.GetWidth()) / 2,
		(rInput.GetScreenHeight() - m_LoginWin.GetHeight()) * 2 / 3);

	m_CreditWin.Create();
	m_WinList.AddHead(&m_CreditWin);

	m_bSysMenuWinShow = false;
	m_nScene = UIM_SCENE_LOGIN;
}

void CUIMng::CreateCharacterScene()
{
	g_GameCensorship->SetState(g_ServerListManager->GetCensorshipIndex());

	RemoveWinList();

	m_CharInfoBalloonMng.Create();

	CInput& rInput = CInput::Instance();

	m_MsgWin.Create();
	m_WinList.AddHead(&m_MsgWin);
	m_MsgWin.SetPosition((rInput.GetScreenWidth() - 352) / 2,
		(rInput.GetScreenHeight() - 113) / 2);

	m_ServerMsgWin.Create();
	m_WinList.AddHead(&m_ServerMsgWin);
	int nBaseY = int(31.0f / 600.0f * (float)rInput.GetScreenHeight());
	m_ServerMsgWin.SetPosition(10, nBaseY + 10);

	m_SysMenuWin.Create();
	m_WinList.AddHead(&m_SysMenuWin);

	m_OptionWin.Create();
	m_WinList.AddHead(&m_OptionWin);

	m_CharSelMainWin.Create();
	m_WinList.AddHead(&m_CharSelMainWin);
	nBaseY = int(567.0f / 600.0f * (float)rInput.GetScreenHeight());
	m_CharSelMainWin.SetPosition(22, nBaseY - m_CharSelMainWin.GetHeight() - 11);

	m_CharMakeWin.Create();
	m_WinList.AddHead(&m_CharMakeWin);

	m_CharMakeWin.SetPosition((rInput.GetScreenWidth() - 454) / 2, (rInput.GetScreenHeight() - 406) / 2);

	m_CharSelMainWin.UpdateDisplay();
	m_CharInfoBalloonMng.UpdateDisplay();

	ShowWin(&m_CharSelMainWin);

	m_bSysMenuWinShow = false;
	m_nScene = UIM_SCENE_CHARACTER;
}

void CUIMng::CreateMainScene()
{
	RemoveWinList();

	m_CharInfoBalloonMng.Release();

	m_nScene = UIM_SCENE_MAIN;
}

CWin* CUIMng::SetActiveWin(CWin* pWin)
{
	CWin* pBeforeActWin = (CWin*)m_WinList.GetHead();

	if (pBeforeActWin == NULL)
		return NULL;

	if (pBeforeActWin->IsActive())
		pBeforeActWin->Active(FALSE);
	else
		pBeforeActWin = NULL;

	if (pWin->IsShow())
	{
		if (!m_WinList.RemoveAt(m_WinList.Find(pWin)))
			return NULL;

		m_bWinActive = true;
		m_WinList.AddHead(pWin);
	}

	return pBeforeActWin;
}

void CUIMng::ShowWin(CWin* pWin)
{
	pWin->Show(TRUE);
	SetActiveWin(pWin);
}

void CUIMng::HideWin(CWin* pWin)
{
	if (!m_WinList.RemoveAt(m_WinList.Find(pWin)))
		return;

	pWin->Show(FALSE);
	pWin->Active(FALSE);
	m_WinList.AddTail(pWin);

	pWin = (CWin*)m_WinList.GetHead();
	if (pWin->IsShow())
		m_bWinActive = true;
}

void CUIMng::CheckDockWin()
{
	NODE* position = m_WinList.GetHeadPosition();
	if (NULL == position)
		return;

	CWin* pMovWin = (CWin*)m_WinList.GetNext(position);

	if (pMovWin->GetState() != WS_MOVE)
		return;

	pMovWin->SetDocking(false);

	RECT rcMovWin = { pMovWin->GetTempXPos(), pMovWin->GetTempYPos(),
		pMovWin->GetTempXPos() + pMovWin->GetWidth(),
		pMovWin->GetTempYPos() + pMovWin->GetHeight() };

	RECT rcDock[4] =
	{
		{ rcMovWin.left - DOCK_EXTENT, rcMovWin.top - DOCK_EXTENT,
			rcMovWin.left + DOCK_EXTENT, rcMovWin.top + DOCK_EXTENT },
		{ rcMovWin.right - DOCK_EXTENT, rcMovWin.top - DOCK_EXTENT,
			rcMovWin.right + DOCK_EXTENT, rcMovWin.top + DOCK_EXTENT },
		{ rcMovWin.left - DOCK_EXTENT, rcMovWin.bottom - DOCK_EXTENT,
			rcMovWin.left + DOCK_EXTENT, rcMovWin.bottom + DOCK_EXTENT },
		{ rcMovWin.right - DOCK_EXTENT, rcMovWin.bottom - DOCK_EXTENT,
			rcMovWin.right + DOCK_EXTENT, rcMovWin.bottom + DOCK_EXTENT }
	};

	CInput& rInput = CInput::Instance();

	POINT pt[4] = { { 0, 0 }, { rInput.GetScreenWidth(), 0 },
		{ 0, rInput.GetScreenHeight() },
		{ rInput.GetScreenWidth(), rInput.GetScreenHeight() } };

	if (::PtInRect(&rcDock[0], pt[0]))
	{
		pMovWin->SetPosition(pt[0].x, pt[0].y);
		pMovWin->SetDocking(true);
	}
	else if (::PtInRect(&rcDock[1], pt[1]))
	{
		pMovWin->SetPosition(pt[1].x - pMovWin->GetWidth(), pt[1].y);
		pMovWin->SetDocking(true);
	}
	else if (::PtInRect(&rcDock[2], pt[2]))
	{
		pMovWin->SetPosition(pt[2].x, pt[2].y - pMovWin->GetHeight());
		pMovWin->SetDocking(true);
	}
	else if (::PtInRect(&rcDock[3], pt[3]))
	{
		pMovWin->SetPosition(pt[3].x - pMovWin->GetWidth(),
			pt[3].y - pMovWin->GetHeight());
		pMovWin->SetDocking(true);
	}
	else if (rcDock[0].top < 0 && rcDock[0].bottom > 0)
	{
		pMovWin->SetPosition(rcMovWin.left, 0);
		pMovWin->SetDocking(true);
	}
	else if (rcDock[2].top < pt[2].y && rcDock[2].bottom > pt[2].y)
	{
		pMovWin->SetPosition(rcMovWin.left, pt[2].y - pMovWin->GetHeight());
		pMovWin->SetDocking(true);
	}
	else if (rcDock[0].left < 0 && rcDock[0].right > 0)
	{
		pMovWin->SetPosition(0, rcMovWin.top);
		pMovWin->SetDocking(true);
	}
	else if (rcDock[1].left < pt[1].x && rcDock[1].right > pt[1].x)
	{
		pMovWin->SetPosition(pt[1].x - pMovWin->GetWidth(), rcMovWin.top);
		pMovWin->SetDocking(true);
	}

	BOOL bEdgeDocking = FALSE;
	int i, j, nXCoord, nYCoord;
	CWin* pWin;

	while (position)
	{
		pWin = (CWin*)m_WinList.GetNext(position);
		if (!pWin->IsShow())
			continue;

		pt[0].x = pWin->GetXPos();
		pt[0].y = pWin->GetYPos();
		pt[1].x = pWin->GetXPos() + pWin->GetWidth();
		pt[1].y = pt[0].y;
		pt[2].x = pt[0].x;
		pt[2].y = pWin->GetYPos() + pWin->GetHeight();
		pt[3].x = pt[1].x;
		pt[3].y = pt[2].y;

		for (i = 0; i < 4; i++)
		{
			for (j = 0; j < 4; j++)
			{
				if (i != j && ::PtInRect(&rcDock[i], pt[j]))
				{
					bEdgeDocking = TRUE;
					goto DOCKING;
				}
			}
		}

		if (pt[0].x < rcDock[1].left && pt[1].x > rcDock[0].right)
		{
			nXCoord = rcMovWin.left;
			if (pt[2].y > rcDock[0].top && pt[2].y < rcDock[0].bottom)
			{
				if (SetDockWinPosition(pMovWin, nXCoord, pt[2].y))
					continue;
			}
			else if (pt[0].y > rcDock[2].top && pt[0].y < rcDock[2].bottom)
			{
				if (SetDockWinPosition(pMovWin,
					nXCoord, pt[0].y - pMovWin->GetHeight()))
					continue;
			}
		}
		else if (pt[0].y < rcDock[2].top && pt[2].y > rcDock[0].bottom)
		{
			nYCoord = rcMovWin.top;
			if (pt[1].x > rcDock[0].left && pt[1].x < rcDock[0].right)
			{
				if (SetDockWinPosition(pMovWin, pt[1].x, nYCoord))
					continue;
			}
			else if (pt[0].x > rcDock[1].left && pt[0].x < rcDock[1].right)
			{
				if (SetDockWinPosition(pMovWin,
					pt[0].x - pMovWin->GetWidth(), nYCoord))
					continue;
			}
		}
	}

DOCKING:
	if (bEdgeDocking)
	{
		switch (j)
		{
		case 0:
			switch (i)
			{
			case 1:
				nXCoord = pWin->GetXPos() - pMovWin->GetWidth();
				nYCoord = pWin->GetYPos();
				break;
			case 2:
				nXCoord = pWin->GetXPos();
				nYCoord = pWin->GetYPos() - pMovWin->GetHeight();
				break;
			case 3:
				nXCoord = pWin->GetXPos() - pMovWin->GetWidth();
				nYCoord = pWin->GetYPos() - pMovWin->GetHeight();
			}
			break;

		case 1:
			switch (i)
			{
			case 0:
				nXCoord = pWin->GetXPos() + pWin->GetWidth();
				nYCoord = pWin->GetYPos();
				break;
			case 2:
				nXCoord = pWin->GetXPos() + pWin->GetWidth();
				nYCoord = pWin->GetYPos() - pMovWin->GetHeight();
				break;
			case 3:
				nXCoord = pWin->GetXPos() + pWin->GetWidth()
					- pMovWin->GetWidth();
				nYCoord = pWin->GetYPos() - pMovWin->GetHeight();
			}
			break;

		case 2:
			switch (i)
			{
			case 0:
				nXCoord = pWin->GetXPos();
				nYCoord = pWin->GetYPos() + pWin->GetHeight();
				break;
			case 1:
				nXCoord = pWin->GetXPos() - pMovWin->GetWidth();
				nYCoord = pWin->GetYPos() + pWin->GetHeight();
				break;
			case 3:
				nXCoord = pWin->GetXPos() - pMovWin->GetWidth();
				nYCoord = pWin->GetYPos() + pWin->GetHeight()
					- pMovWin->GetHeight();
			}
			break;

		case 3:
			switch (i)
			{
			case 0:
				nXCoord = pWin->GetXPos() + pWin->GetWidth();
				nYCoord = pWin->GetYPos() + pWin->GetHeight();
				break;
			case 1:
				nXCoord = pWin->GetXPos() + pWin->GetWidth()
					- pMovWin->GetWidth();
				nYCoord = pWin->GetYPos() + pWin->GetHeight();
				break;
			case 2:
				nXCoord = pWin->GetXPos() + pWin->GetWidth();
				nYCoord = pWin->GetYPos() + pWin->GetHeight()
					- pMovWin->GetHeight();
			}
		}
		SetDockWinPosition(pMovWin, nXCoord, nYCoord);
	}
}

bool CUIMng::SetDockWinPosition(CWin* pMoveWin, int nDockX, int nDockY)
{
	CInput& rInput = CInput::Instance();
	RECT rcDummy;
	RECT rcScreen = { 0, 0, rInput.GetScreenWidth(), rInput.GetScreenHeight() };
	RECT rcMoveWin = { nDockX, nDockY,
		nDockX + pMoveWin->GetWidth(), nDockY + pMoveWin->GetHeight() };

	if (::IntersectRect(&rcDummy, &rcScreen, &rcMoveWin))
	{
		pMoveWin->SetPosition(nDockX, nDockY);
		pMoveWin->SetDocking(true);
		return true;
	}

	return false;
}

void CUIMng::Update(double dDeltaTick)
{
	if (UIM_SCENE_NONE == m_nScene || m_WinList.IsEmpty())
		return;

	if (m_bWinActive)
	{
		CWin* pWin = (CWin*)m_WinList.GetHead();
		if (pWin->IsShow())
		{
			pWin->Active(true);
			m_bWinActive = false;
		}
	}

	CInput& rInput = CInput::Instance();
	if (rInput.IsKeyDown(VK_ESCAPE))
	{
		m_bSysMenuWinShow = !m_bSysMenuWinShow;
	}

	CWin* pWin;
	NODE* position;

	m_bCursorOnUI = false;

	if (rInput.IsLBtnDn())
	{
		bool bWinClick = false;
		position = m_WinList.GetHeadPosition();
		while (position)
		{
			pWin = (CWin*)m_WinList.GetNext(position);

			if (pWin->CursorInWin(WA_ALL))
			{
				SetActiveWin(pWin);
				bWinClick = true;
				break;
			}
		}

		if (!bWinClick)
		{
			pWin = (CWin*)m_WinList.GetHead();
			pWin->Active(false);
		}
	}
	else if (rInput.IsLBtnUp())
	{
		m_bBlockCharMove = false;
	}
	int nlist = m_WinList.GetCount();
	CWin** apTempWin = new (CWin * [nlist]);

	position = m_WinList.GetHeadPosition();
	for (int i = 0; i < nlist; ++i)
	{
		apTempWin[i] = (CWin*)m_WinList.GetNext(position);
		apTempWin[i]->ActiveBtns(false);
	}

	position = m_WinList.GetHeadPosition();
	while (position)
	{
		pWin = (CWin*)m_WinList.GetNext(position);
		if (pWin->CursorInWin(WA_ALL))
		{
			pWin->ActiveBtns(true);
			break;
		}
	}

	for (int i = 0; i < nlist; ++i)
	{
		apTempWin[i]->Update(dDeltaTick);
	}

	SAFE_DELETE_ARRAY(apTempWin);

	CheckDockWin();

	position = m_WinList.GetHeadPosition();
	while (position)
	{
		pWin = (CWin*)m_WinList.GetNext(position);

		switch (pWin->GetState())
		{
		case WS_ETC:
			m_bCursorOnUI = true;
			break;
		case WS_MOVE:
			m_bCursorOnUI = true;
			break;
		case WS_EXTEND_UP:
			m_bCursorOnUI = true;
			break;
		case WS_EXTEND_DN:
			m_bCursorOnUI = true;
			break;
		}

		if (m_bCursorOnUI)
			break;

		if (pWin->CursorInWin(WA_ALL))
		{
			m_bCursorOnUI = true;
			break;
		}
	}
}

void CUIMng::Render()
{
	if (UIM_SCENE_NONE == m_nScene)
		return;

	m_CharInfoBalloonMng.Render();

	CWin* pWin;
	NODE* position = m_WinList.GetTailPosition();
	while (position)
	{
		pWin = (CWin*)m_WinList.GetPrev(position);
		pWin->Render();
	}
}

void CUIMng::PopUpMsgWin(int nMsgCode, char* pszMsg)
{
	if (UIM_SCENE_NONE == m_nScene || UIM_SCENE_TITLE == m_nScene || UIM_SCENE_LOADING == m_nScene)
		return;

	if (UIM_SCENE_MAIN == m_nScene)
		return;

	m_MsgWin.PopUp(nMsgCode, pszMsg);
}

void CUIMng::AddServerMsg(char* pszMsg)
{
	if (UIM_SCENE_CHARACTER != m_nScene)
		return;

	m_ServerMsgWin.AddMsg(pszMsg);
}


bool CUIMng::IsInterfaceLogin()
{
	if (!(m_MsgWin.IsShow() || m_LoginWin.IsShow() || m_SysMenuWin.IsShow() || m_OptionWin.IsShow() || m_CreditWin.IsShow()) && m_LoginMainWin.IsShow() && m_ServerSelWin.IsShow() && IsSysMenuWinShow())
	{
		return true;
	}
	return false;
}

bool CUIMng::IsInterfaceCharacter()
{
	if (m_MsgWin.IsShow() || m_CharMakeWin.IsShow() || m_SysMenuWin.IsShow() || m_OptionWin.IsShow())
	{
		return true;
	}
	return false;
}
