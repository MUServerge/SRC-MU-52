// NewUINameWindow.cpp: implementation of the CNewUINameWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUINameWindow.h"
#include "ZzzBmd.h"
#include "ZzzObject.h"
#include "ZzzCharacter.h"
#include "ZzzInterface.h"
#include "ZzzInventory.h"
#include "UIControls.h"
#include "CSChaosCastle.h"
#include "PersonalShopTitleImp.h"
#include "MatchEvent.h"
#include "MapManager.h"
#include "ScaleForm.h"
#include "CGMHeadChat.h"
#include "CGMItemDropName.h"
#include "jpexs.h"
#include "pugixml.hpp"


using namespace SEASON3B;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SEASON3B::CNewUINameWindow::CNewUINameWindow()
{
	m_pNewUIMng = NULL;
	m_Pos.x = m_Pos.y = 0;

	m_bShowItemName = false;
	m_bNpcIconRendererEnabled = false;
	m_iNextNpcIconSlot = BITMAP_NPCICON_BEGIN;
	m_fNpcNameOffsetY = 35.f;
	m_fNpcIconGap = 3.f;
	m_fNpcEffectOffsetX = -2.f;
	m_fNpcEffectOffsetY = -2.f;
}

SEASON3B::CNewUINameWindow::~CNewUINameWindow()
{
	Release();
}

bool SEASON3B::CNewUINameWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if (NULL == pNewUIMng)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_NAME_WINDOW, this);

	LoadImages();
	LoadNpcIconConfig();

	SetPos(x, y);

	Show(true);

	return true;
}

void SEASON3B::CNewUINameWindow::Release()
{
	for (std::map<std::string, int>::const_iterator it = m_NpcIconSlots.begin(); it != m_NpcIconSlots.end(); ++it)
	{
		if (it->second >= BITMAP_NPCICON_BEGIN && it->second <= BITMAP_NPCICON_END)
			DeleteBitmap(it->second);
	}
	m_NpcIconSlots.clear();
	m_NpcIconConfig.clear();
	m_iNextNpcIconSlot = BITMAP_NPCICON_BEGIN;
	m_bNpcIconRendererEnabled = false;

	if (m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CNewUINameWindow::LoadImages()
{
	LoadBitmap("Interface\\HUD\\TooltipHUD.tga", IMAGE_TOOLTIP_ID, GL_LINEAR, GL_REPEAT);
	LoadBitmap("Interface\\HUD\\hudPlayerName_I1.tga", IMAGE_HUD_PLAYER, GL_LINEAR, GL_REPEAT);
	LoadBitmap("Interface\\HUD\\bg01.tga", IMAGE_HUD_INVASION, GL_LINEAR, GL_REPEAT);
	LoadBitmap("Interface\\HUD\\PersonalShopPurchase_I1.tga", IMAGE_PURCHARSE_CAPTION_1, GL_LINEAR);
	
	LoadBitmap("Interface\\cr_gr.TGA", IMAGE_BACK_FADED_R1);
	LoadBitmap("Interface\\cr_gr2.TGA", IMAGE_BACK_FADED_R2);
}

void SEASON3B::CNewUINameWindow::LoadNpcIconConfig()
{
	m_NpcIconConfig.clear();
	m_bNpcIconRendererEnabled = false;
	m_fNpcNameOffsetY = 35.f;
	m_fNpcIconGap = 3.f;
	m_fNpcEffectOffsetX = -2.f;
	m_fNpcEffectOffsetY = -2.f;

	pugi::xml_document document;
	if (!document.load_file("Data\\Local\\xml\\NpcIconRenderer.xml"))
		return;

	pugi::xml_node root = document.child("NpcIconRenderer");
	if (!root || root.attribute("Enable").as_int(0) == 0)
		return;

	m_fNpcNameOffsetY = max(0.f, root.attribute("NameOffsetY").as_float(m_fNpcNameOffsetY));
	m_fNpcIconGap = max(0.f, root.attribute("IconGap").as_float(m_fNpcIconGap));
	m_fNpcEffectOffsetX = root.attribute("EffectOffsetX").as_float(m_fNpcEffectOffsetX);
	m_fNpcEffectOffsetY = root.attribute("EffectOffsetY").as_float(m_fNpcEffectOffsetY);

	for (pugi::xml_node node = root.child("Npc"); node; node = node.next_sibling("Npc"))
	{
		const int npcId = node.attribute("Id").as_int(-1);
		const char* icon = node.attribute("Icon").as_string();
		if (npcId < 0 || icon == NULL || icon[0] == '\0')
			continue;

		NPC_ICON_RENDER_CONFIG config = {0};
		config.effectSpeed = max(1, node.attribute("EffectSpeed").as_int(60));
		strncpy(config.iconFile, icon, sizeof(config.iconFile) - 1);
		config.iconFile[sizeof(config.iconFile) - 1] = '\0';

		char* extension = strrchr(config.iconFile, '.');
		if (extension != NULL)
			strcpy_s(extension, sizeof(config.iconFile) - (extension - config.iconFile), ".tga");

		m_NpcIconConfig[npcId] = config;
	}

	m_bNpcIconRendererEnabled = true;
}

int SEASON3B::CNewUINameWindow::ResolveNpcIconTexture(const char* iconFile)
{
	if (iconFile == NULL || iconFile[0] == '\0')
		return -1;

	std::map<std::string, int>::iterator found = m_NpcIconSlots.find(iconFile);
	if (found != m_NpcIconSlots.end())
		return found->second;

	if (m_iNextNpcIconSlot > BITMAP_NPCICON_END)
		return -1;

	char path[256] = {0};
	_snprintf_s(path, sizeof(path), _TRUNCATE, "Interface\\NpcIcons\\%s", iconFile);
	if (!LoadBitmap(path, m_iNextNpcIconSlot, GL_LINEAR, GL_CLAMP_TO_EDGE, false))
	{
		m_NpcIconSlots[iconFile] = -1;
		return -1;
	}

	const int slot = m_iNextNpcIconSlot++;
	m_NpcIconSlots[iconFile] = slot;
	return slot;
}

void SEASON3B::CNewUINameWindow::SetPos(int x, int y)
{
	m_Pos.x = x;
	m_Pos.y = y;
}

bool SEASON3B::CNewUINameWindow::UpdateMouseEvent()
{
	return true;
}

bool SEASON3B::CNewUINameWindow::UpdateKeyEvent()
{
	if (SEASON3B::IsPress(VK_MENU) == true)
	{
		m_bShowItemName = !m_bShowItemName;
		//GMItemDropName->SetVisibleInfo(-1, m_bShowItemName);
	}

	return true;
}

bool SEASON3B::CNewUINameWindow::Update()
{
	return true;
}

bool SEASON3B::CNewUINameWindow::Render()
{
	EnableAlphaTest();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	this->RenderName();

	RenderTimes();

	matchEvent::RenderMatchTimes();

	gmHeadChat->RenderBooleans();

	DrawPersonalShopTitleImp();

	DisableAlphaBlend();

	return true;
}

void SEASON3B::CNewUINameWindow::RenderTooltip(float posX, float posY, float RenderSizeX, float RenderSizeY, bool fixed)
{
	if (fixed)
	{
		posX *= g_fScreenRate_x;
		posY *= g_fScreenRate_y;
		RenderSizeX *= g_fScreenRate_x;
		RenderSizeY *= g_fScreenRate_y;
	}

	jpexs::RenderBoxjpexs_dds(IMAGE_HUD_PLAYER, posX, posY, RenderSizeX, RenderSizeY);
}

void SEASON3B::CNewUINameWindow::RenderTooltip_old(float x, float y, float width, float height, bool fixed)
{
	glColor4f(0.0, 0.0, 0.0, 1.f);
	RenderColor(x - 1.f, y - 1.f, width + 2.f, 1.0, 0.0, 0, fixed);
	RenderColor(x - 1.f, y + height, width + 2.f, 1.0, 0.0, 0, fixed);
	RenderColor(x - 1.f, y - 1.f, 1.0, height + 1.f, 0.0, 0, fixed);
	RenderColor(x + width, y - 1.f, 1.0, height + 1.f, 0.0, 0, fixed);

	glColor4f(0.0, 0.0, 0.0, 0.8f);
	RenderColor(x, y, width, height, 0.0, 0, fixed);
	EndRenderColor();
}

void SEASON3B::CNewUINameWindow::RenderName()
{
	//if (g_bGMObservation == true)
	//{
		for (size_t i = 0; i < MAX_CHARACTERS_CLIENT; i++)
		{
			CHARACTER* pCharacter = gmCharacters->GetCharacter(i);
			OBJECT* o = &pCharacter->Object;

			if (gmProtect->LookAndFeel == 5 && o->Live && o->Kind == KIND_NPC && pCharacter->Dead == 0)
			{
				gmHeadChat->CreateChat(pCharacter->ID, "", pCharacter);
				continue;
			}

			if (o->Live && (o->Kind == KIND_PLAYER || o->Kind == KIND_MONSTER) && pCharacter->Dead == 0)
			{
#ifdef PLAYER_INFO_SYSTEM
				if (gmProtect->m_RenderCharacterName)
				{
					gmHeadChat->CreateChat(pCharacter->ID, "", pCharacter);
				}
				else
				{
					if (IsShopTitleVisible(pCharacter) == false)
					{
						gmHeadChat->CreateChat(pCharacter->ID, "", pCharacter);
					}
				}
#else
				if (IsShopTitleVisible(pCharacter) == false)
				{
					gmHeadChat->CreateChat(pCharacter->ID, "", pCharacter);
				}
#endif // PLAYER_INFO_SYSTEM
			}
		}
	//}

#ifndef GUILD_WAR_EVENT
	if (gMapManager->InChaosCastle() == true)
	{
		if (FindText(Hero->ID, "webzen") == false)
		{
			if (SelectedNpc != -1 || SelectedCharacter != -1)
			{
				return;
			}
		}
	}
#endif//GUILD_WAR_EVENT

	if (SelectedItem != -1 || SelectedNpc != -1 || SelectedCharacter != -1)
	{
		if (SelectedNpc != -1)
		{
			CHARACTER* c = gmCharacters->GetCharacter(SelectedNpc);
			OBJECT* o = &c->Object;
			gmHeadChat->CreateChat(c->ID, "", c);
		}
		else if (SelectedCharacter != -1)
		{
			CHARACTER* c = gmCharacters->GetCharacter(SelectedCharacter);

			OBJECT* o = &c->Object;
			if (o->Kind == KIND_MONSTER)
			{
				if (gmProtect->LookAndFeel != 5)
				{
					g_pRenderText->SetTextColor(255, 230, 200, 255);
					g_pRenderText->SetBgColor(100, 0, 0, 255);
					g_pRenderText->RenderText((GetWindowsX / 2), 2, c->ID, 0, 0, RT3_WRITE_CENTER);
				}
			}
			else if (!::IsStrifeMap(World) || Hero->m_byGensInfluence == c->m_byGensInfluence)
			{
				if (IsShopTitleVisible(c) == false)
				{
					gmHeadChat->CreateChat(c->ID, "", c);
				}
			}
		}
		else if (SelectedItem != -1)
		{
			RenderItemName(&Items[SelectedItem], SelectedItem, &Items[SelectedItem].Object, Items[SelectedItem].Item.Level, Items[SelectedItem].Item.Option1, Items[SelectedItem].Item.ExtOption, false);
		}
	}

	if (m_bShowItemName || SEASON3B::IsRepeat(VK_MENU))
	{
		for (int i = 0; i < MAX_ITEMS; i++)
		{
			OBJECT* o = &Items[i].Object;
			if (o->Live)
			{
				if (o->Visible && i != SelectedItem)
				{
					RenderItemName(&Items[i], i, o, Items[i].Item.Level, Items[i].Item.Option1, Items[i].Item.ExtOption, true);
				}
			}
		}
	}

	if (CharacterMachine->PowerLevelUp)
	{
		auto current_time = std::chrono::steady_clock::now();
		double difTime = std::chrono::duration<double>(current_time - CharacterMachine->last_time).count();

		double time_checked = 1.5;

		if (difTime >= time_checked)
		{
			CharacterMachine->PowerLevelUp = false;
			CharacterMachine->last_time = current_time;
		}

		float Alpha = (float)(1.f - (difTime / time_checked));
		float MoveY = (float)(60.f * (1.f - (difTime / time_checked)));

		glColor4f(1.f, 1.f, 1.f, Alpha);

		vec3_t Position;
		int ScreenX = 0, ScreenY = 0;

		OBJECT* pObj = &Hero->Object;
		Vector(pObj->Position[0], pObj->Position[1], pObj->Position[2] + pObj->BoundingBoxMax[2] + 100.f, Position);

		Projection(Position, &ScreenX, &ScreenY);

		ScreenY = ScreenY + (Alpha * 50);

		RenderNumberHQ(ScreenX + 6.f, ScreenY - 8.f, CharacterMachine->ATTKPowerValue, 7.f, 10.f);
		RenderBitmap(BITMAP_TEXT_POWER, ScreenX-5, ScreenY, 60.f, 28.f, 0.0, 0.0, 1.f, 118.f / 128.f, true, true, 0.0);
	}

	RenderNpcIcons();
}

// Over-head NPC icons (MuDream-style). Mapping is data-driven via the encoder
// (Local\CustomNpcIcon.txt -> av-code45.pak -> CGMProtect::GetNpcIcon). Each
// distinct icon file is lazy-loaded once into the BITMAP_NPCICON range and then
// cached by filename and drawn below the engine's NPC name.
void SEASON3B::CNewUINameWindow::RenderNpcIcons()
{
	for (size_t i = 0; i < MAX_CHARACTERS_CLIENT; i++)
	{
		CHARACTER* pCharacter = gmCharacters->GetCharacter(i);
		OBJECT* o = &pCharacter->Object;

		if (!o->Live || o->Kind != KIND_NPC || pCharacter->Dead != 0)
			continue;

		const char* iconFile = NULL;
		int effectSpeed = 60;
		int offsetY = 0;
		float size = 32.f;

		if (m_bNpcIconRendererEnabled)
		{
			std::map<int, NPC_ICON_RENDER_CONFIG>::const_iterator config = m_NpcIconConfig.find(pCharacter->MonsterIndex);
			if (config != m_NpcIconConfig.end())
			{
				iconFile = config->second.iconFile;
				effectSpeed = config->second.effectSpeed;
			}
		}

		if (iconFile == NULL)
		{
			CUSTOM_NPC_ICON* legacyIcon = GMProtect->GetNpcIcon(pCharacter->MonsterIndex, World);
			if (legacyIcon == NULL)
				continue;

			iconFile = legacyIcon->IconFile;
			offsetY = legacyIcon->OffsetY;
			size = (legacyIcon->Scale > 0) ? (float)legacyIcon->Scale : 32.f;
		}

		const int slot = ResolveNpcIconTexture(iconFile);

		if (slot < 0)
			continue;

		// Use the same terrain/model-aware anchor as the name renderer. The icon
		// then stays at a fixed screen-space distance from the name at every zoom
		// and camera pitch.
		int ScreenX = 0, ScreenY = 0;
		if (!gmHeadChat->ProjectCharacterAnchor(pCharacter, &ScreenX, &ScreenY))
			continue;

		const float headX = (float)ScreenX / g_fScreenRate_x;
		const float headY = (float)ScreenY / g_fScreenRate_y;
		const float nameTop = headY - m_fNpcNameOffsetY;
		const float centerX = headX;
		const float centerY = nameTop - m_fNpcIconGap - (float)offsetY - size * 0.5f;
		const float rotation = (float)fmod((double)WorldTime * (double)effectSpeed / 1000.0, 360.0);

		EnableAlphaBlend();
		glColor4f(0.65f, 0.85f, 0.35f, 0.55f);
		RenderBitmapRotate(BITMAP_SHINY + 1,
			centerX + m_fNpcEffectOffsetX, centerY + m_fNpcEffectOffsetY,
			size * 1.25f, size * 1.25f, rotation);

		EnableAlphaTest(true);
		glColor4f(1.f, 1.f, 1.f, 1.f);
		RenderBitmapRotate(slot, centerX, centerY, size, size, 0.f);
		glColor4f(1.f, 1.f, 1.f, 1.f);
	}
}

float SEASON3B::CNewUINameWindow::GetLayerDepth()
{
	return 1.0f;
}

