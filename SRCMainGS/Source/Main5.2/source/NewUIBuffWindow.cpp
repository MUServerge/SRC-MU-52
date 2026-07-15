// NewUIBuffWindow.cpp: implementation of the CNewUIBuffWindow class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "NewUIBuffWindow.h"
#include "HudTooltip.h"
#include "ZzzBMD.h"
#include "ZzzCharacter.h"
#include "ZzzTexture.h"
#include "ZzzInventory.h"
#include "UIControls.h"
#include "NewUICommonMessageBox.h"

using namespace SEASON3B;

bool gBuffCollapsed = false; 

namespace
{
	const float BUFF_IMG_WIDTH = 16.0f;
	const float BUFF_IMG_HEIGHT = 20.0f;
	const int BUFF_MAX_LINE_COUNT = 8;
	const int BUFF_IMG_SPACE = 5;
};

SEASON3B::CNewUIBuffWindow::CNewUIBuffWindow()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;
}

SEASON3B::CNewUIBuffWindow::~CNewUIBuffWindow()
{
	Release();
}

bool SEASON3B::CNewUIBuffWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if (NULL == pNewUIMng)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_BUFF_WINDOW, this);

	SetPos(x, y);

	LoadImages();

	Show(true);

	return true;
}

void SEASON3B::CNewUIBuffWindow::Release()
{
	UnloadImages();

	if (m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUIBuffWindow::SetPos(int x, int y)
{
	m_Pos.x = x;
	m_Pos.y = y;
}

void SEASON3B::CNewUIBuffWindow::SetPos(int iScreenWidth)
{
#if MAIN_UPDATE > 303
	if (iScreenWidth == gwinhandle->GetScreenX())
	{
		SetPos(PositionX_In_The_Mid(220), 25);
	}
	else if (iScreenWidth == (gwinhandle->GetScreenX() - 190))
	{
		SetPos(PositionX_In_The_Mid(125), 25);
	}
	else if (iScreenWidth == (gwinhandle->GetScreenX() - 380))
	{
		SetPos(PositionX_In_The_Mid(86), 25);
	}
	else if (iScreenWidth == (gwinhandle->GetScreenX() - 570))
	{
		SetPos(PositionX_In_The_Mid(30), 25);
	}
	else
	{
		SetPos(PositionX_In_The_Mid(220), 25);
	}
#endif
}

void SEASON3B::CNewUIBuffWindow::BuffSort(OBJECT* pHeroObject, std::list<eBuffState>& buffstate)
{
	int iBuffSize = g_CharacterBuffSize(pHeroObject);

	for (int i = 0; i < iBuffSize; ++i)
	{
		eBuffState eBuffType = g_CharacterBuff(pHeroObject, i);

		if (SetDisableRenderBuff(eBuffType))
			continue;

		if (eBuffType != eBuffNone)
		{
			eBuffClass eBuffClassType = g_IsBuffClass(eBuffType);

			if (eBuffClassType == eBuffClass_Buff)
			{
				buffstate.push_front(eBuffType);
			}
			else if (eBuffClassType == eBuffClass_DeBuff)
			{
				buffstate.push_back(eBuffType);
			}
			else {
				assert(!"SetDisableRenderBuff");
			}
		}
	}
}

bool SEASON3B::CNewUIBuffWindow::SetDisableRenderBuff(const eBuffState& _BuffState)
{
	switch (_BuffState)
	{
#ifdef PBG_ADD_PKSYSTEM_INGAMESHOP
	case eDeBuff_MoveCommandWin:
#endif //PBG_ADD_PKSYSTEM_INGAMESHOP
	case eDeBuff_FlameStrikeDamage:
	case eDeBuff_GiganticStormDamage:
	case eDeBuff_LightningShockDamage:
	case eDeBuff_Discharge_Stamina:
		return true;
	default:
		return false;
}
	return false;
}

bool SEASON3B::CNewUIBuffWindow::UpdateMouseEvent()
{
	OBJECT* pHeroObject = &Hero->Object;
	float frameX = (float)PositionX_In_The_Mid(170);
	float frameY = (float)Position_In_The_Down(85.f);
	float x = 0.0f, y = 0.0f;
	int buffwidthcount = 0;

	std::list<eBuffState> buffstate;
	BuffSort(pHeroObject, buffstate);

	std::list<eBuffState>::iterator iter;
	const int totalBuffs = buffstate.size();
	const float spacingX = gBuffCollapsed ? 10.0f : (BUFF_IMG_WIDTH + BUFF_IMG_SPACE);

	for (iter = buffstate.begin(); iter != buffstate.end(); )
	{
		std::list<eBuffState>::iterator tempiter = iter;
		++iter;
		eBuffState buff = (*tempiter);

		x = frameX + (buffwidthcount * spacingX);
		y = frameY;

		if (SEASON3B::CheckMouseIn(x, y, BUFF_IMG_WIDTH, BUFF_IMG_HEIGHT))
		{
			if (buff == eBuff_InfinityArrow)
			{
				if (SEASON3B::IsRelease(VK_RBUTTON))
				{
					SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CInfinityArrowCancelMsgBoxLayout));
				}
			}
			else if (buff == eBuff_SwellOfMagicPower)
			{
				if (SEASON3B::IsRelease(VK_RBUTTON))
				{
					SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CBuffSwellOfMPCancelMsgBoxLayOut));
				}
			}
			else if (buff == EFFECT_MAGIC_CIRCLE_IMPROVED)
			{
				if (SEASON3B::IsRelease(VK_RBUTTON))
				{
					SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CBuffSwellOfMPUpCancelMsgBoxLayOut));
				}
			}
			else if (buff == EFFECT_MAGIC_CIRCLE_ENHANCED)
			{
				if (SEASON3B::IsRelease(VK_RBUTTON))
				{
					SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CBuffSwellOfMPMasteryCancelMsgBoxLayOut));
				}
			}
			return false;
		}

		++buffwidthcount;
	}

	if (totalBuffs > 1)
	{
		const float arrowWidth = 14.0f;
		const float arrowHeight = 10.0f;
		const float arrowX = x + 7.0f;
		const float arrowY = y - 5.0f;

		if (SEASON3B::CheckMouseIn(arrowX, arrowY, arrowWidth, arrowHeight))
		{
			if (SEASON3B::IsRelease(VK_LBUTTON))
			{
				gBuffCollapsed = !gBuffCollapsed;
				PlayBuffer(SOUND_CLICK01);
			}
			return false;
		}
	}
	return true;
}

bool SEASON3B::CNewUIBuffWindow::UpdateKeyEvent()
{
	return true;
}

bool SEASON3B::CNewUIBuffWindow::Update()
{
	return true;
}

bool SEASON3B::CNewUIBuffWindow::Render()
{
	EnableAlphaTest();
	glColor4f(1.f, 1.f, 1.f, 1.f);

	RenderBuffStatus(BUFF_RENDER_ICON);

	RenderBuffStatus(BUFF_RENDER_TOOLTIP);

	DisableAlphaBlend();

	return true;
}

bool IsMousePressed()
{
	static bool previousLButtonState = false;
	bool currentLButtonState = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	bool result = currentLButtonState && !previousLButtonState;
	previousLButtonState = currentLButtonState;
	return result;
}

void SEASON3B::CNewUIBuffWindow::RenderBuffStatus(BUFF_RENDER renderstate)
{
	OBJECT* pHeroObject = &Hero->Object;

	// NOTE: GetCenterX() is a misnomer - it returns the full VIRTUAL width (WindowWidth/rate),
	// NOT the centre, so "GetCenterX()-120" pushed the buffs to the far right / off the HUD.
	// Anchor with the HUD's own resolution-aware macros instead. Tune to sit above the SD orb.
	float frameX = (float)PositionX_In_The_Mid(170);   // horizontal: larger = further right
	float frameY = (float)Position_In_The_Down(85.f); // vertical: larger = higher (above orbs)

	float x = 0.0f, y = 0.0f;

	int buffwidthcount = 0;

	std::list<eBuffState> buffstate;
	BuffSort(pHeroObject, buffstate);

	int totalBuffs = buffstate.size();
	int visibleBuffs = totalBuffs;

	std::list<eBuffState>::iterator iter;
	int IndexTime = 0;

	const float spacingX = gBuffCollapsed ? 10.0f : (BUFF_IMG_WIDTH + BUFF_IMG_SPACE);

	for (iter = buffstate.begin(); iter != buffstate.end(); )
	{
		if (IndexTime >= visibleBuffs)
			break;

		std::list<eBuffState>::iterator tempiter = iter;
		++iter;
		eBuffState buff = (*tempiter);

		// frameX/frameY already anchor the bar to screen-centre / bottom, so do NOT add
		// m_Pos too - m_Pos.x is itself a centred X, and adding both double-centred the bar
		// off the right edge (buff icons vanished).
		x = frameX + (buffwidthcount * spacingX);
		y = frameY;

		if (renderstate == BUFF_RENDER_ICON)
		{
			RenderBuffIcon(buff, x, y, BUFF_IMG_WIDTH, BUFF_IMG_HEIGHT);
		}
		else if (renderstate == BUFF_RENDER_TOOLTIP)
		{
			if (!gBuffCollapsed && SEASON3B::CheckMouseIn(x, y, BUFF_IMG_WIDTH, BUFF_IMG_HEIGHT))
			{
				float fTooltip_x = x + (BUFF_IMG_WIDTH / 2);
				float fTooltip_y = y + (BUFF_IMG_HEIGHT / 2) - 80;
				eBuffClass buffclass = g_IsBuffClass(buff);
				RenderBuffTooltip(buffclass, buff, fTooltip_x, fTooltip_y);
			}
		}

		IndexTime++;
		++buffwidthcount;  
	}


	if (renderstate == BUFF_RENDER_ICON && totalBuffs > 1)
	{
		const float arrowWidth = 14.0f;
		const float arrowHeight = 10.0f;
		float arrowX = x + 7.0f;
		float arrowY = y - 5.0f;

		if (gBuffCollapsed)
		{
			SEASON3B::RenderImageF(IMAGE_BUFF_EFFECTS_HIDE, arrowX, arrowY, arrowWidth, arrowHeight, 0.0f, 0.0f, 29.0f, 21.0f);

			char buffCount[8];
			sprintf_s(buffCount, "%d", totalBuffs);
			g_pRenderText->SetFont(g_hFontBold);
			g_pRenderText->SetBgColor(8, 20, 24, 210);
			g_pRenderText->SetTextColor(160, 255, 255, 255);
			g_pRenderText->RenderText(
				(int)((x + 6.0f) * g_fScreenRate_x),
				(int)((y - 5.0f) * g_fScreenRate_y),
				buffCount,
				(int)(9.0f * g_fScreenRate_x),
				0,
				RT3_WRITE_CENTER);
			g_pRenderText->SetBgColor(0, 0, 0, 0);
		}
		else
		{
			SEASON3B::RenderImageF(IMAGE_BUFF_EFFECTS_HIDE, arrowX, arrowY, arrowWidth, arrowHeight, 29.0f, 0.0f, 29.0f, 21.0f);
		}

		// Click handling lives in UpdateMouseEvent so the UI consumes the click before
		// the world can receive it.
	}
}

void SEASON3B::CNewUIBuffWindow::RenderBuffIcon(eBuffState& eBuffType, float x, float y, float width, float height)
{
	float u, v;

	if (eBuffType > eBuffNone && eBuffType < eBuff_Count) // eBuff_Berserker
	{
		int skillindex = ((int)eBuffType - 1) % 80;

		u = (double)(skillindex % 10) * 20.f / 256.0;
		v = (double)(skillindex / 10) * 28.f / 256.0;
		int imgindex = ((int)eBuffType - 1) / 80 + IMAGE_BUFF_STATUS;
		RenderBitmap(imgindex, x, y, width, height, u, v, 20.f / 256.f, 28.f / 256.f);
	}
}

void SEASON3B::CNewUIBuffWindow::RenderBuffTooltip(eBuffClass& eBuffClassType, eBuffState& eBuffType, float x, float y)
{
	(void)eBuffClassType;
	std::list<std::string> tooltipinfo;
	g_BuffToolTipString(tooltipinfo, eBuffType);
	std::vector<HudTooltip::Line> lines;
	lines.reserve(tooltipinfo.size() + 2);

	std::size_t lineIndex = 0;
	for (std::list<std::string>::const_iterator iter = tooltipinfo.begin(); iter != tooltipinfo.end(); ++iter, ++lineIndex)
	{
		const bool title = (lineIndex == 0);
		lines.push_back({
			iter->c_str(),
			title ? RGBA(190, 218, 242, 255) : RGBA(232, 232, 232, 255),
			title
		});
	}

	std::string bufftime;
	g_BuffStringTime(eBuffType, bufftime);
	char durationText[256] = { 0 };

	if (!bufftime.empty())
	{
		sprintf_s(durationText, GlobalText[2533], bufftime.c_str());
		lines.push_back({ "", RGBA(232, 232, 232, 255), false });
		lines.push_back({ durationText, RGBA(218, 184, 232, 255), false });
	}

	if (!lines.empty())
		HudTooltip::RenderCentered(x, y, lines.data(), lines.size());
}

float SEASON3B::CNewUIBuffWindow::GetLayerDepth()	//. 5.3f
{
	return 0.95f;
}

void SEASON3B::CNewUIBuffWindow::OpenningProcess()
{

}

void SEASON3B::CNewUIBuffWindow::ClosingProcess()
{

}

#if MAIN_UPDATE <= 303
void SEASON3B::CNewUIBuffWindow::RenderBuffStatus(float x, float y, OBJECT* pHeroObject)
{
	float RenderFrameX = 0.0;
	float RenderFrameY = 0.0;

	int buffwidthcount = 0, buffheightcount = 1;

	std::list<eBuffState> buffstate;

	BuffSort(pHeroObject, buffstate);

	float RenderSizeX = 16.f;
	float RenderSizeY = 20.f;

	for (std::list<eBuffState>::iterator iter = buffstate.begin(); iter != buffstate.end(); )
	{
		std::list<eBuffState>::iterator tempiter = iter;
		++iter;
		eBuffState buff = (*tempiter);

		if (buffwidthcount < 4)
		{
			RenderFrameX = x - (buffwidthcount * RenderSizeX); // Renderiza a la izquierda
		}
		else
		{
			RenderFrameX = x + ((buffwidthcount - 4) * RenderSizeX) + RenderSizeX; // Renderiza a la derecha
		}

		RenderFrameY = y - (buffheightcount * (RenderSizeY + 2));

		RenderBuffIcon(buff, RenderFrameX, RenderFrameY, RenderSizeX, RenderSizeY);

		if (++buffwidthcount >= BUFF_MAX_LINE_COUNT)
		{
			buffwidthcount = 0;
			++buffheightcount;
		}
	}
}
#endif

void SEASON3B::CNewUIBuffWindow::LoadImages()
{
	LoadBitmap("Interface\\newui_statusicon.jpg", IMAGE_BUFF_STATUS, GL_LINEAR);
	LoadBitmap("Interface\\newui_statusicon2.jpg", IMAGE_BUFF_STATUS2, GL_LINEAR);
	LoadBitmap("Interface\\newui_statusicon3.jpg", IMAGE_BUFF_STATUS3, GL_LINEAR);
    LoadBitmap("Interface\\newui_statusicon2.jpg", IMAGE_BUFF_STATUS2, GL_LINEAR);
	LoadBitmap("Interface\\Custom\\Pegasus_BUFFICON.tga", IMAGE_BUFF_arrow, GL_LINEAR);
	LoadBitmap("Interface\\Custom\\Pegasus_BUFFICON_01.tga", IMAGE_BUFF_arrow_1, GL_LINEAR);
	LoadBitmap("Interface\\HUD\\Look-5\\EffectsHide.tga", IMAGE_BUFF_EFFECTS_HIDE, GL_LINEAR);
}

void SEASON3B::CNewUIBuffWindow::UnloadImages()
{
	DeleteBitmap(IMAGE_BUFF_STATUS);
	DeleteBitmap(IMAGE_BUFF_STATUS2);
	DeleteBitmap(IMAGE_BUFF_STATUS3); 
	DeleteBitmap(IMAGE_BUFF_arrow);
	DeleteBitmap(IMAGE_BUFF_arrow_1);
	DeleteBitmap(IMAGE_BUFF_EFFECTS_HIDE);
}
