#include "stdafx.h"
#include "ScaleForm.h"
#include "Utilities/Log/muConsoleDebug.h"
#include "CGMProtect.h"
#include "UIControls.h"

#ifdef SHUTDOWN_SCALEFORM_INFO
PLACEMENT_NEW placementObj_new = NULL;
PLACEMENT_NEW placementMov_new = NULL;
extern int FontHeight;

namespace
{
	struct LOOK5_LONG_NOTICE
	{
		std::string Text;
		std::chrono::steady_clock::time_point StartTime;
		int Duration;
		int FontSize;
		bool Active;

		LOOK5_LONG_NOTICE()
			: Duration(0), FontSize(0), Active(false)
		{
		}
	};

	LOOK5_LONG_NOTICE g_Look5LongNotice;

	void ShowLook5LongNotice(const char* text, int fontSize, int duration)
	{
		if (text == NULL || text[0] == '\0' || duration <= 0)
		{
			g_Look5LongNotice.Active = false;
			g_Look5LongNotice.Text.clear();
			return;
		}

		g_Look5LongNotice.Text = text;
		g_Look5LongNotice.StartTime = std::chrono::steady_clock::now();
		g_Look5LongNotice.Duration = duration;
		g_Look5LongNotice.FontSize = fontSize;
		g_Look5LongNotice.Active = true;
	}

	void RenderLook5LongNotice()
	{
		if (!g_Look5LongNotice.Active || gmProtect->LookAndFeel != 5)
			return;

		const long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - g_Look5LongNotice.StartTime).count();

		if (elapsed >= g_Look5LongNotice.Duration)
		{
			g_Look5LongNotice.Active = false;
			g_Look5LongNotice.Text.clear();
			return;
		}

		const int fadeInDuration = 150;
		const int fadeOutDuration = 250;
		float alpha = 1.0f;

		if (elapsed < fadeInDuration)
			alpha = (float)elapsed / (float)fadeInDuration;

		const int remaining = g_Look5LongNotice.Duration - (int)elapsed;
		if (remaining < fadeOutDuration)
			alpha = min(alpha, (float)remaining / (float)fadeOutDuration);

		alpha = max(0.0f, min(1.0f, alpha));

		// Physical-pixel layout derived from viewport proportions. Keeping the
		// background and text in the same coordinate space makes the banner span
		// and center correctly on every aspect ratio and resolution.
		const float bannerY = (float)WindowHeight * 0.085f;
		const float bannerHeight = max(24.0f, (float)WindowHeight * 0.036f);

		EnableAlphaTest(false);
		glColor4f(0.015f, 0.025f, 0.035f, 0.72f * alpha);
		RenderColor(0.0f, bannerY, (float)WindowWidth, bannerHeight, 0.0f, 0, false);
		EndRenderColor();

		const HFONT oldFont = g_pRenderText->GetFont();
		const DWORD oldTextColor = g_pRenderText->GetTextColor();
		const DWORD oldBgColor = g_pRenderText->GetBgColor();
		const BYTE textAlpha = (BYTE)(255.0f * alpha + 0.5f);
		const float virtualBannerHeight = bannerHeight / g_fScreenRate_y;
		const int textY = (int)((bannerY + (bannerHeight - (float)FontHeight) * 0.5f) / g_fScreenRate_y);

		g_pRenderText->SetFont(g_Look5LongNotice.FontSize >= 20 ? g_hFontBold : g_hFont);
		g_pRenderText->SetBgColor(0, 0, 0, 0);
		g_pRenderText->SetTextColor(255, 204, 25, textAlpha);
		g_pRenderText->RenderText(0, textY, g_Look5LongNotice.Text.c_str(),
			GetWindowsX, (int)virtualBannerHeight, RT3_WRITE_CENTER);

		g_pRenderText->SetFont(oldFont);
		g_pRenderText->SetTextColor(oldTextColor);
		g_pRenderText->SetBgColor(oldBgColor);
		EnableAlphaTest(true);
		glColor4f(1.f, 1.f, 1.f, 1.f);
	}
}

SCALEFORM::SCALEFORM()
{
	hModule = NULL;
}

SCALEFORM::~SCALEFORM()
{
	Release();
}

void SCALEFORM::Release()
{
	if (hModule != NULL)
	{
		hldList.clear();

		void (*ExitProc)() = (void(*)())GetProcAddress(hModule, "runtime_kill");

		if (ExitProc) ExitProc();

		FreeLibrary(hModule);
	}
}

SCALEFORM* SCALEFORM::Instance()
{
	static SCALEFORM s_Instance;
	return &s_Instance;
}

void SCALEFORM::runtime_connection()
{
	hModule = LoadLibrary("ScaleForm.dll");

	if (hModule != NULL)
	{
		void (*EntryProc)() = (void(*)())GetProcAddress(hModule, "EntryProc");

		if (EntryProc != 0)
		{
			placementMov_new = ((PLACEMENT_NEW)GetProcAddress(hModule, "InitializeMovieLoader"));

			placementObj_new = ((PLACEMENT_NEW)GetProcAddress(hModule, "InitializeContainerType"));

			EntryProc();
		}

		if (placementMov_new == NULL || placementObj_new == NULL)
		{
			FreeLibrary(hModule);
		}
	}
}

void SCALEFORM::runtime_disconnect()
{
	Release();
}

void commandInternal(void* pClass, const char* pName, const char* pCommand)
{
}

void commandExternal(void* pClass, const char* pName, const void* gfxValue, UInt numArgs)
{
}

void SCALEFORM::LoadMovie()
{
	this->LoadMovie(0, "hudNotice.gfx", commandInternal, commandExternal, 5);
}

void SCALEFORM::LoadMovie(UInt index, const char* pfilename, FUNC_INTERNAL pCommand, FUNC_EXTERNAL pExternal, UInt align)
{
	if (placementMov_new)
	{
		auto gfxFile = std::make_shared<CONTAINER_HANDLE>();

		if (gfxFile)
		{
			gfxFile->Live = false;
			gfxFile->index = index;
			gfxFile->shared.AddMovie(pCommand, pExternal, pfilename, align);
			gfxFile->shared.OnCreateDevice(WindowWidth, WindowHeight);
			hldList.push_back(gfxFile);
		}
	}
}

void SCALEFORM::CreateObject(UInt index, SCALEFORMOBJECT* gfxValue)
{
	SCALEFORMHANDLE* shared = GetSharedHandleByIndex(index);

	if (shared)
	{
		shared->CreateObject(gfxValue);
	}
}

void SCALEFORM::CreateArray(UInt index, SCALEFORMOBJECT* gfxValue, UInt Size)
{
	SCALEFORMHANDLE* shared = GetSharedHandleByIndex(index);

	if (shared)
	{
		shared->CreateArray(gfxValue, Size);
	}
}

void SCALEFORM::Render(UInt index)
{
	SCALEFORMHANDLE* shared = GetSharedHandleByIndex(index);

	if (shared)
	{
		EndBitmap();
		shared->Render();
		BeginBitmap();
	}

	RenderLook5LongNotice();
}

void SCALEFORM::Update(UInt index)
{
	SCALEFORMHANDLE* shared = GetSharedHandleByIndex(index);

	if (shared)
	{
		shared->Update();
	}
}

void SCALEFORM::Invoke(UInt index, const char* pmethodName, const char* pargFmt, ...)
{
	SCALEFORMHANDLE* shared = GetSharedHandleByIndex(index);

	if (shared)
	{
		va_list args;
		va_start(args, pargFmt);
		shared->Invoke(pmethodName, pargFmt, args);
		va_end(args);
	}
}

void SCALEFORM::Invoke(UInt index, const char* pmethodName, SCALEFORMOBJECT* pargFmt)
{
	SCALEFORMHANDLE* shared = GetSharedHandleByIndex(index);

	if (shared)
	{
		shared->Invoke(pmethodName, pargFmt);
	}
}

void SCALEFORM::Invoke(UInt index, const char* pmethodName, SCALEFORMOBJECT* pargFmt, int Size)
{
	SCALEFORMHANDLE* shared = GetSharedHandleByIndex(index);

	if (shared)
	{
		shared->Invoke(pmethodName, pargFmt);
	}
}

bool SCALEFORM::CallBack(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	for (size_t i = 0; i < hldList.size(); i++)
	{
		if (hldList[i]->Live)
		{
			hldList[i]->shared.runtime_event(uMsg, wParam, lParam);
		}
	}

	return false;
}

SCALEFORMHANDLE* SCALEFORM::GetSharedHandleByIndex(int searchIndex)
{
	SCALEFORMHANDLE* shared = NULL;

	auto it = std::find_if(hldList.begin(), hldList.end(), [searchIndex](const std::shared_ptr<CONTAINER_HANDLE>& handle) {
		return handle->index == searchIndex;
		});

	if (it != hldList.end())
	{
		shared = &(*it)->shared;
	}

	return shared;
}

SCALEFORMOBJECT::SCALEFORMOBJECT()
{
	memset(inheritance, 0, sizeof(inheritance));
	placementObj_new(this, 0x00);
}

SCALEFORMOBJECT::SCALEFORMOBJECT(UInt Type)
{
	memset(inheritance, 0, sizeof(inheritance));
	placementObj_new(this, Type);
}

bool SCALEFORMOBJECT::AddMember(const char* name, bool to_member)
{
	return (*(bool(__thiscall**)(void*, const char*, int))(*(DWORD*)this + 4))(this, name, to_member);
}

bool SCALEFORMOBJECT::AddMember(const char* name, int to_member)
{
	return (*(bool(__thiscall**)(void*, const char*, double))(*(DWORD*)this + 8))(this, name, static_cast<double>(to_member));
}

bool SCALEFORMOBJECT::AddMember(const char* name, double to_member)
{
	return (*(bool(__thiscall**)(void*, const char*, double))(*(DWORD*)this + 8))(this, name, to_member);
}

bool SCALEFORMOBJECT::AddMember(const char* name, const char* to_member)
{
	return (*(bool(__thiscall**)(void*, const char*, const char*))(*(DWORD*)this + 12))(this, name, to_member);
}

bool SCALEFORMOBJECT::AddElement(UInt idx, void* to_element)
{
	return (*(bool(__thiscall**)(void*, UInt, void*))(*(DWORD*)this + 16))(this, idx, to_element);
}

bool SCALEFORMOBJECT::SetArraySize(UInt Size)
{
	return (*(bool(__thiscall**)(void*, UInt))(*(DWORD*)this + 20))(this, Size);
}

SCALEFORMHANDLE::SCALEFORMHANDLE()
{
	memset(this, 0, sizeof(SCALEFORMHANDLE));
	placementMov_new(this, 0x01);
}

bool SCALEFORMHANDLE::AddMovie(FUNC_INTERNAL pCommand, FUNC_EXTERNAL pExternalCommand, const char* pfilename, UInt AlignType)
{
	return (*(bool(__thiscall**)(void*, FUNC_INTERNAL, FUNC_EXTERNAL, const char*, UInt))(*(DWORD*)this + 4))(this,
		pCommand, pExternalCommand, pfilename, AlignType);
}

bool SCALEFORMHANDLE::Update()
{
	return (*(bool(__thiscall**)(void*))(*(DWORD*)this + 8))(this);
}

bool SCALEFORMHANDLE::Render()
{
	return (*(bool(__thiscall**)(void*))(*(DWORD*)this + 12))(this);
}

void SCALEFORMHANDLE::CreateObject(SCALEFORMOBJECT* gfxValue)
{
	(*(bool(__thiscall**)(void*, void*))(*(DWORD*)this + 16))(this, gfxValue);
}

void SCALEFORMHANDLE::CreateArray(SCALEFORMOBJECT* gfxValue, UInt sz)
{
	(*(bool(__thiscall**)(void*, void*, UInt))(*(DWORD*)this + 20))(this, gfxValue, sz);
}

void SCALEFORMHANDLE::Invoke(const char* methodName, SCALEFORMOBJECT* gfxValue)
{
	(*(void(__thiscall**)(void*, const char*, void*))(*(DWORD*)this + 28))(this, methodName, gfxValue);
}

void SCALEFORMHANDLE::Invoke(const char* methodName, const char* pargFmt, va_list args)
{
	(*(void(__thiscall**)(void*, const char*, const char*, va_list))(*(DWORD*)this + 24))(this, methodName, pargFmt, args);
}

void SCALEFORMHANDLE::OnResetDevice(SInt bufw, SInt bufh)
{
	(*(void(__thiscall**)(void*, SInt, SInt))(*(DWORD*)this + 32))(this, bufw, bufh);
}

bool SCALEFORMHANDLE::OnCreateDevice(SInt bufw, SInt bufh)
{
	return (*(bool(__thiscall**)(void*, SInt, SInt))(*(DWORD*)this + 36))(this, bufw, bufh);
}

void SCALEFORMHANDLE::runtime_event(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	(*(void(__thiscall**)(void*, UInt, WPARAM, LPARAM))(*(DWORD*)this + 40))(this, uMsg, wParam, lParam);
}


void gfxNotice::addNoticeText(char* Text, DWORD Style)
{
	DWORD HexColor = RGBA(0xff, 0xff, 0xff, 0xff);

	switch (Style)
	{
	case 0:
		HexColor = RGBA(0xff, 0xc8, 0x32, 0xff);
		break;
	case 1:
		HexColor = RGBA(0xff, 0xff, 0xff, 0xff);
		break;
	case 2:
		HexColor = RGBA(0x32, 0xe2, 0x86, 0xff);
		break;
	}

	const char* HTML = SEASON3B::FontHTML(Text, HexColor, (int)(FontHeight-1), true, false, 3);

	gfxinit->Invoke(0, "addText", "%s", HTML);
}

void gfxNotice::addEventMapText(char* Text)
{
	const char* HTML = SEASON3B::FontHTML(Text, RGBA(0xff, 0xcc, 0x19u, 0xffu), 0, false, true);
	gfxinit->Invoke(0, "addEventMapText", "%s", HTML);
}

void gfxNotice::addLongMovementText(const char* Text, int Size, int YPos, int Duration)
{
	if (gmProtect->LookAndFeel == 5)
	{
		ShowLook5LongNotice(Text, Size, Duration);
		return;
	}

	const char* textHTML = SEASON3B::FontHTML(Text, RGBA(0xff, 0xcc, 0x19u, 0xffu), Size, false, true);

	SCALEFORMOBJECT gfxvalue(6);

	gfxinit->CreateObject(0, &gfxvalue);
	gfxvalue.AddMember("Duration", (double)Duration);
	gfxvalue.AddMember("YPos", (double)(YPos / g_fScreenRate_y));
	gfxvalue.AddMember("Text", textHTML);
	gfxinit->Invoke(0, "addLongMovementText", &gfxvalue);
}
#endif // SHUTDOWN_SCALEFORM_INFO
