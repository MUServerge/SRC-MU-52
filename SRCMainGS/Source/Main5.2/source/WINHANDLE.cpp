#include "stdafx.h"
#include "resource.h"
#include "ThemidaSDK.h"

#include "UIMng.h"
#include "Nprotect.h"
#include "UIWindows.h"
#include "ScaleForm.h"
#include "wsclientinline.h"
#include "NewUISystem.h"
#include "ConnectVersionHex.h"
#include "WINHANDLE.h"

static RECT GetWindowTargetWorkArea(HWND hWnd)
{
	HMONITOR hMonitor = NULL;

	if (hWnd != NULL)
	{
		hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	}
	else
	{
		POINT point = { GetSystemMetrics(SM_CXSCREEN) / 2, GetSystemMetrics(SM_CYSCREEN) / 2 };
		hMonitor = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
	}

	MONITORINFO monitorInfo = {};
	monitorInfo.cbSize = sizeof(monitorInfo);

	if (hMonitor != NULL && GetMonitorInfo(hMonitor, &monitorInfo))
	{
		return monitorInfo.rcWork;
	}

	RECT rcWork = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
	return rcWork;
}

static POINT GetCenteredWindowPosition(HWND hWnd, int width, int height)
{
	RECT rcWork = GetWindowTargetWorkArea(hWnd);
	int workWidth = rcWork.right - rcWork.left;
	int workHeight = rcWork.bottom - rcWork.top;

	POINT point = {};
	point.x = rcWork.left + ((workWidth > width) ? ((workWidth - width) / 2) : 0);
	point.y = rcWork.top + ((workHeight > height) ? ((workHeight - height) / 2) : 0);

	return point;
}

CWINHANDLE::CWINHANDLE()
{
	hWnd = NULL;
	WndMode = true;
	WndActive = false;
	WndIconic = false;
	hInstance = NULL;
	wndIndex = 0;
	Shell_NotifyIcon(NIM_DELETE, &nid);


	iWinWidth = 1024;
	iWinHight = 768;
}

CWINHANDLE::~CWINHANDLE()
{
	this->Release();
}

void CWINHANDLE::Release()
{
	if (nid.cbSize != 0)
		Shell_NotifyIcon(NIM_DELETE, &nid);

	//if (this->hWnd)
	//{
	//	DestroyWindow(this->hWnd);
	//	this->hWnd = NULL;
	//}
}

HWND CWINHANDLE::Create(HINSTANCE hCurrentInst, mu_uint32 RenderSizeX, mu_uint32 RenderSizeY)
{
	WNDCLASS wc = {};

	const char* windowName = GMProtect->GetWindowName();

	this->SetInstance(hCurrentInst);

	if (!GetClassInfo(hCurrentInst, windowName, &wc))
	{
		wc.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = CWINHANDLE::WndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hCurrentInst;
		wc.hIcon = LoadIcon(hCurrentInst, (LPCTSTR)IDI_ICON1);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = windowName;
		RegisterClass(&wc);
	}

	RECT rc = { 0, 0, RenderSizeX, RenderSizeY };

	if (this->CheckWndMode())
	{
		AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN, NULL);

		rc.right -= rc.left;
		rc.bottom -= rc.top;
		POINT point = GetCenteredWindowPosition(NULL, rc.right, rc.bottom);

		this->hWnd = CreateWindow(
			windowName,
			windowName,
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN,
			point.x,
			point.y,
			rc.right,
			rc.bottom,
			NULL,
			NULL,
			hCurrentInst,
			NULL);
	}
	else
	{
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, NULL);

		this->hWnd = CreateWindow(
			windowName,                    // Nombre de la clase
			windowName,         // Título de la ventana
			WS_POPUP | WS_VISIBLE,        // Estilo de la ventana (sin bordes, ni título)
			0, 0,                         // Posición de la ventana (top-left corner)
			RenderSizeX,
			RenderSizeY,    // Tamaño de la ventana (ancho y alto)
			NULL,                         // Handle de la ventana padre
			NULL,                         // Handle del menú
			hCurrentInst,                    // Handle de la instancia
			NULL                          // Parámetros de creación
		);
	}

	nid.cbSize = sizeof(nid);
	nid.hWnd = this->hWnd;
	nid.uID = TRAY_ID_ICON;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = TRAY_ID_MESSAGE;
	strncpy_s(nid.szTip, sizeof(nid.szTip), windowName, _TRUNCATE);
	nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));

	if (nid.hIcon == NULL)
	{
		nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Icono por defecto
	}

	return this->hWnd;
}

void CWINHANDLE::Destroyer()
{
	DestroyImGuiWindow();

#ifdef SHUTDOWN_SCALEFORM_INFO
	gfxinit->runtime_disconnect();
#endif // SHUTDOWN_SCALEFORM_INFO

	DestroyWindow();

#ifdef MAX_INSTANCE_GAME
	GMProtect->runtime_delete_mutex();
#endif // MAX_INSTANCE_GAME
}

void CWINHANDLE::InitSize(mu_uint32 RenderSizeX, mu_uint32 RenderSizeY)
{
	WindowWidth = RenderSizeX;

	WindowHeight = RenderSizeY;

	if (gmProtect->LookAndFeel == 5)
	{
		const mu_float ScaleX = (mu_float)WindowWidth / (mu_float)LOOK5_DESIGN_WIDTH;
		const mu_float ScaleY = (mu_float)WindowHeight / (mu_float)LOOK5_DESIGN_HEIGHT;
		const mu_float Scale = min(ScaleX, ScaleY);

		g_fScreenRate_x = (Scale > 0.0f) ? Scale : 1.0f;
		g_fScreenRate_y = g_fScreenRate_x;
	}
	else if (gmProtect->ScreenType == 0)
	{
		g_fScreenRate_x = (float)WindowWidth / 640.0;
		g_fScreenRate_y = (float)WindowHeight / 480.0;
	}
	else if (gmProtect->ScreenType == 1)
	{
		if (((float)WindowWidth / 640.0) >= ((float)WindowHeight / 480.0))
		{
			g_fScreenRate_x = (float)WindowHeight / 480.0;
			g_fScreenRate_y = (float)WindowHeight / 480.0;
		}
		else
		{
			g_fScreenRate_x = (float)WindowWidth / 640.0;
			g_fScreenRate_y = (float)WindowWidth / 640.0;
		}
	}
	else
	{
		if (WindowWidth >= 1920)
		{
			g_fScreenRate_x = 2.1f;
			g_fScreenRate_y = 2.1f;
		}
		else
		{
			g_fScreenRate_x = 1.65f;
			g_fScreenRate_y = 1.65f;
		}
	}

	iWinWidth = ((float)WindowWidth / g_fScreenRate_x);

	iWinHight = ((float)WindowHeight / g_fScreenRate_y);
}

void CWINHANDLE::SetFontSize(mu_uint32 FontSize)
{
	if (FontHeight != FontSize || FontHeight == -1)
	{
		int iFontSize = FontSize - 1;

		SAFE_DELETE_OBJET(g_hFont);
		SAFE_DELETE_OBJET(g_hFontBold);
		SAFE_DELETE_OBJET(g_hFontBig);
		SAFE_DELETE_OBJET(g_hFixFont);

		const char* fontname = res_setting.Getfontfamily();

		g_hFont = CreateFont(iFontSize, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontname);

		g_hFontBold = CreateFont(iFontSize, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontname);

		g_hFontBig = CreateFont(iFontSize * 2, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontname);

		g_hFixFont = CreateFont(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontname);

		ResolutionConfig* conf = this->LoadCurrentConfig();

		if (conf->fontsize != FontSize)
		{
			conf->fontsize = FontSize;
		}

		FontHeight = conf->fontsize;
	}
}

void CWINHANDLE::Resize(mu_uint32 RenderSizeX, mu_uint32 RenderSizeY)
{
	if (!this->GethWnd())
		return;

	RECT rc = { 0, 0, RenderSizeX, RenderSizeY };

	if (this->CheckWndMode())
	{
		AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN, FALSE);
	}

	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;

	if (this->CheckWndMode())
	{
		POINT point = GetCenteredWindowPosition(this->GethWnd(), width, height);
		SetWindowPos(this->GethWnd(), NULL, point.x, point.y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}
	else
	{
		SetWindowPos(this->GethWnd(), NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

MSG CWINHANDLE::winLoop()
{
	MSG msg = {};

	while (msg.message != WM_QUIT)
	{
		this->UpdateWndActive();

		if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage(&msg, NULL, 0, 0))
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (this->CheckWndMode() || this->CheckWndActive())
			{
				Scene(g_hDC);
			}
			else if (!this->CheckWndMode())
			{
				if (GetForegroundWindow() != this->GethWnd())
				{
					SetForegroundWindow(this->GethWnd());
					SetFocus(this->GethWnd());
				}

				if (g_iInactiveWarning < 1)
				{
					g_iInactiveWarning++;
					g_bMinimizedEnabled = TRUE;
					
					if (IsIconic(this->GethWnd()))
						ShowWindow(this->GethWnd(), SW_RESTORE);
					else
						ShowWindow(this->GethWnd(), SW_MINIMIZE);

					g_bMinimizedEnabled = FALSE;
				}
				else
				{
					SetTimer(this->GethWnd(), WINDOWMINIMIZED_TIMER, 1 * 1000, NULL);
					this->SendPostMessage(WM_DESTROY, 0, 0);
				}
			}
		}
		ProtocolCompiler();
		g_pChatRoomSocketList->ProtocolCompile();
	}

	return msg;
}

HWND CWINHANDLE::GethWnd()
{
	return hWnd;
}

void CWINHANDLE::SetInstance(HINSTANCE hInst)
{
	hInstance = hInst;
}

HINSTANCE CWINHANDLE::GetInstance()
{
	return hInstance;
}

void CWINHANDLE::SetWndActive(mu_boolean bActive)
{
	WndActive = bActive;
}

mu_boolean CWINHANDLE::CheckWndActive()
{
	return WndActive;
}

void CWINHANDLE::UpdateWndActive()
{
	WndActive = (GetForegroundWindow() == this->GethWnd());
}

void CWINHANDLE::SetWndMode(mu_boolean bActive)
{
	WndMode = bActive;
}

mu_boolean CWINHANDLE::CheckWndMode()
{
	return WndMode;
}

void CWINHANDLE::SetDisplayIndex(mu_uint8 index, mu_boolean Fontload)
{
	wndIndex = index;

	ResolutionConfig* conf = this->LoadCurrentConfig();

	this->InitSize(conf->width, conf->height);

	this->SetFontSize(conf->fontsize);


	if (Fontload == false)
	{
		this->Resize(conf->width, conf->height);
	}
}

mu_uint8 CWINHANDLE::GetDisplayIndex()
{
	return wndIndex;
}

mu_uint8 CWINHANDLE::GetDisplayIndex(const std::string text_name)
{
	return res_setting.FindInfoByIndex(text_name);
}

mu_float CWINHANDLE::GetScreenX()
{
	return iWinWidth;
}

mu_float CWINHANDLE::GetScreenY()
{
	return iWinHight;
}

mu_float CWINHANDLE::GetLook5Scale()
{
	return (g_fScreenRate_x > 0.0f) ? g_fScreenRate_x : 1.0f;
}

mu_float CWINHANDLE::GetLook5SafeOffsetX()
{
	return ((mu_float)WindowWidth - ((mu_float)LOOK5_DESIGN_WIDTH * GetLook5Scale())) * 0.5f;
}

mu_float CWINHANDLE::GetLook5SafeOffsetY()
{
	return ((mu_float)WindowHeight - ((mu_float)LOOK5_DESIGN_HEIGHT * GetLook5Scale())) * 0.5f;
}

mu_float CWINHANDLE::Look5DesignToScreenX(mu_float DesignX)
{
	return GetLook5SafeOffsetX() + (DesignX * GetLook5Scale());
}

mu_float CWINHANDLE::Look5DesignToScreenY(mu_float DesignY)
{
	return GetLook5SafeOffsetY() + (DesignY * GetLook5Scale());
}

mu_float CWINHANDLE::Look5ScreenToDesignX(mu_float ScreenX)
{
	return (ScreenX - GetLook5SafeOffsetX()) / GetLook5Scale();
}

mu_float CWINHANDLE::Look5ScreenToDesignY(mu_float ScreenY)
{
	return (ScreenY - GetLook5SafeOffsetY()) / GetLook5Scale();
}

ResolutionConfig* CWINHANDLE::LoadCurrentConfig()
{
	return res_setting.FindInfoByIndex(wndIndex);
}

void CWINHANDLE::Check_State()
{
	if (this->CheckWndMode())
	{
		this->Change_State(IsWindowVisible(hWnd));
	}
}

void CWINHANDLE::Change_State(mu_boolean bActive)
{
	if (this->CheckWndMode() && nid.cbSize != 0)
	{
		WndIconic = bActive;
		Shell_NotifyIcon((bActive ? NIM_ADD : NIM_DELETE), &nid);
		ShowWindow(hWnd, (bActive ? SW_HIDE : SW_SHOW));
	}
}

mu_boolean CWINHANDLE::CheckWndIconic()
{
	return WndIconic && !IsWindowVisible(hWnd);
}

mu_boolean CWINHANDLE::CheckPerformance()
{
	return !this->CheckWndIconic();
}

void CWINHANDLE::SendWindowMessage(const char* format, bool destroyAfter, ...)
{
	va_list args;
	char szMessage[512];
	va_start(args, destroyAfter);
	vsprintf(szMessage, format, args);
	va_end(args);

	g_ErrorReport.Write(szMessage);

	MessageBox(this->GethWnd(), szMessage, NULL, MB_OK);

	if (destroyAfter)
	{
		this->SendPostMessage(WM_DESTROY, 0, 0);
	}
}

void CWINHANDLE::SendNowMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	SendMessage(this->GethWnd(), Msg, wParam, lParam);
}

void CWINHANDLE::SendPostMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	PostMessage(this->GethWnd(), Msg, wParam, lParam);
}

CWINHANDLE* CWINHANDLE::Instance()
{
	static CWINHANDLE s_Instance;
	return &s_Instance;
}

LONG CWINHANDLE::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
#ifdef IMPLEMENT_IMGUI_WIN32
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
#endif // IMPLEMENT_IMGUI_WIN32

#ifdef SHUTDOWN_SCALEFORM_INFO
	if (SceneFlag == LOG_IN_SCENE || SceneFlag == CHARACTER_SCENE || SceneFlag == MAIN_SCENE)
	{
		gfxinit->CallBack(hwnd, msg, wParam, lParam);
	}
#endif // SHUTDOWN_SCALEFORM_INFO

	switch (msg)
	{
	case WM_SYSKEYDOWN:
	{
		return 0;
	}
	break;
	case WM_SYSCOMMAND:
	{
		if (wParam == SC_KEYMENU || wParam == SC_SCREENSAVE)
		{
			return 0;
		}
	}
	break;
	case WM_KEYDOWN:
		if (wParam == VK_F12)
		{
			gwinhandle->Check_State();
		}
		break;
	case TRAY_ID_MESSAGE:
		if (wParam == TRAY_ID_ICON)
		{
			switch (LOWORD(lParam))
			{
			case WM_LBUTTONDOWN:
				gwinhandle->Change_State(false);
				break;
			case WM_RBUTTONDOWN:
				// Mostrar menú contextual si lo necesitas
				break;
			}
		}
		break;
	case WM_ACTIVATE:
		gwinhandle->SetWndActive(LOWORD(wParam) != WA_INACTIVE);

		if (LOWORD(wParam) == WA_INACTIVE)
		{
			if (gwinhandle->CheckWndMode())
			{
				MouseLButton = false;
				MouseLButtonPop = false;
				MouseRButton = false;
				MouseRButtonPop = false;
				MouseRButtonPush = false;
				MouseLButtonDBClick = false;
				MouseMButton = false;
				MouseMButtonPop = false;
				MouseMButtonPush = false;
				MouseWheel = false;
			}
		}
		break;
	case WM_TIMER:
		switch (wParam)
		{
		case HACK_TIMER:
			VM_START;
			CheckHack();
			VM_END;
			break;
		case WINDOWMINIMIZED_TIMER:
			gwinhandle->SendPostMessage(WM_DESTROY, 0, 0);
			break;
		case CHATCONNECT_TIMER:
			g_pFriendMenu->SendChatRoomConnectCheck();
			break;
		case SLIDEHELP_TIMER:
			if (gwinhandle->CheckWndMode())
			{
				if (g_pSlideHelpMgr)
					g_pSlideHelpMgr->CreateSlideText();
			}
			break;
		}
		break;
	case WM_USER_MEMORYHACK:
		KillGLWindow();
		break;
	case WM_NPROTECT_EXIT_TWO:
		SendHackingChecked(0x04, 0);
		SetTimer(gwinhandle->GethWnd(), WINDOWMINIMIZED_TIMER, 1 * 1000, NULL);
		MessageBox(NULL, GlobalText[16], "Error", MB_OK);
		break;
	case WM_ASYNCSELECTMSG:
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_CONNECT:
			break;
		case FD_READ:
			SocketClient.nRecv();
			break;
		case FD_WRITE:
			SocketClient.FDWriteSend();
			break;
		case FD_CLOSE:
			g_pChatListBox->AddText("", GlobalText[3], SEASON3B::TYPE_SYSTEM_MESSAGE);
			SocketClient.Close();
			CUIMng::Instance().PopUpMsgWin(MESSAGE_SERVER_LOST);
			break;
		}
		break;
	case WM_CTLCOLOREDIT:
		SetBkColor((HDC)wParam, RGB(0, 0, 0));
		SetTextColor((HDC)wParam, RGB(255, 255, 255));
		return (LRESULT)GetStockObject(BLACK_BRUSH);
		break;
	case WM_ERASEBKGND:
		return TRUE;
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
	}
	return 0;
	break;
	case WM_CLOSE:
	case WM_DESTROY:
	{
		Destroy = true;
		SocketClient.Close();
		DestroySound();
		KillGLWindow();
		CloseMainExe();
		GMConnectHex->OpenUpdater();
		PostQuitMessage(0);
	}
	break;
	case WM_SETCURSOR:
		ShowCursor(false);
		break;
	case WM_SIZE:
		if (SIZE_MINIMIZED == wParam)
		{
			if (!gwinhandle->CheckWndMode())
			{
				if (!g_bMinimizedEnabled)
				{
					DWORD dwMess[SIZE_ENCRYPTION_KEY];
					for (int i = 0; i < SIZE_ENCRYPTION_KEY; ++i)
					{
						dwMess[i] = GetTickCount();
					}
					g_SimpleModulusCS.LoadKeyFromBuffer((BYTE*)dwMess, FALSE, FALSE, FALSE, TRUE);
				}
			}
			else
			{
				gwinhandle->Change_State(false);
			}
		}
		else if (gmProtect->LookAndFeel == 5)
		{
			const mu_uint32 RenderSizeX = LOWORD(lParam);
			const mu_uint32 RenderSizeY = HIWORD(lParam);

			if (RenderSizeX > 0 && RenderSizeY > 0
				&& (RenderSizeX != WindowWidth || RenderSizeY != WindowHeight))
			{
				const mu_float PreviousScreenX = gwinhandle->GetScreenX();
				const mu_float PreviousScreenY = gwinhandle->GetScreenY();

				gwinhandle->InitSize(RenderSizeX, RenderSizeY);
				MouseX = MouseRenderX / g_fScreenRate_x;
				MouseY = MouseRenderY / g_fScreenRate_y;

				if (g_pNewUISystem->GetNewUIManager() != NULL)
				{
					g_pNewUISystem->RenderFrameUpdate(PreviousScreenX, PreviousScreenY);

					if (g_pNewUI3DRenderMng != NULL)
					{
						g_pNewUI3DRenderMng->Reload3DEffectObject(WindowWidth, WindowHeight);
					}
				}
			}
		}
		break;
	default:
		if (msg >= WM_CHATROOMMSG_BEGIN && msg < WM_CHATROOMMSG_END)
			g_pChatRoomSocketList->ProcessSocketMessage(msg - WM_CHATROOMMSG_BEGIN, WSAGETSELECTEVENT(lParam));
		break;
	}

	MouseLButtonDBClick = false;
	if (MouseLButtonPop == true && (g_iMousePopPosition_x != MouseX || g_iMousePopPosition_y != MouseY))
		MouseLButtonPop = false;
	switch (msg)
	{
	case WM_MOUSEMOVE:
	{
		MouseRenderX = (float)LOWORD(lParam);
		MouseRenderY = (float)HIWORD(lParam);

		if (MouseRenderX < 0)
			MouseRenderX = 0;

		if (MouseRenderX > WindowWidth)
			MouseRenderX = WindowWidth;

		if (MouseRenderY < 0)
			MouseRenderY = 0;

		if (MouseRenderY > WindowHeight)
			MouseRenderY = WindowHeight;

		MouseX = (MouseRenderX / g_fScreenRate_x);
		MouseY = (MouseRenderY / g_fScreenRate_y);
	}
	break;
	case WM_LBUTTONDOWN:
		MouseLButtonPop = false;
		if (!MouseLButton)
			MouseLButtonPush = true;
		MouseLButton = true;
		SetCapture(gwinhandle->GethWnd());
		break;
	case WM_LBUTTONUP:
		MouseLButtonPush = false;
		MouseLButtonPop = true;
		MouseLButton = false;
		g_iMousePopPosition_x = MouseX;
		g_iMousePopPosition_y = MouseY;
		ReleaseCapture();
		break;
	case WM_RBUTTONDOWN:
		MouseRButtonPop = false;
		if (!MouseRButton)
			MouseRButtonPush = true;
		MouseRButton = true;
		SetCapture(gwinhandle->GethWnd());
		break;
	case WM_RBUTTONUP:
		MouseRButtonPush = false;
		if (MouseRButton)
			MouseRButtonPop = true;
		MouseRButton = false;
		ReleaseCapture();
		break;
	case WM_LBUTTONDBLCLK:
		MouseLButtonDBClick = true;
		break;
	case WM_MBUTTONDOWN:
		MouseMButtonPop = false;
		if (!MouseMButton)
			MouseMButtonPush = true;
		MouseMButton = true;
		SetCapture(gwinhandle->GethWnd());
		break;
	case WM_MBUTTONUP:
		MouseMButtonPush = false;
		if (MouseMButton)
			MouseMButtonPop = true;
		MouseRButton = false;
		ReleaseCapture();
		break;
	case WM_MOUSEWHEEL:
	{
		MouseWheel = (short)HIWORD(wParam) / WHEEL_DELTA;
	}
	break;
	case WM_IME_NOTIFY:
		break;
	case WM_CHAR:
	{
		switch (wParam)
		{
		case VK_RETURN:
			SetEnterPressed(true);
			break;
		}
	}
	break;
	}

	if (g_BuffSystem)
	{
		LRESULT result;
		TheBuffStateSystem().HandleWindowMessage(msg, wParam, lParam, result);
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}
