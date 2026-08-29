// GameOptions.cpp: implementation of CGameOptions.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "GameOptions.h"
#include "pugixml.hpp"
#include <string.h>
#include <stdio.h>

CGameOptions gGameOptions;

namespace
{
	//-- Order must match eGameOption. The name is what ends up in the XML, so
	//-- renaming one silently resets that option for existing players.
	const CGameOptions::SDefinition s_Definition[GAMEOPT_COUNT] =
	{
		{ "ShowPlayerName",			CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "ShowMyName",				CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "ShowGuildName",			CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "ShowGuildLogo",			CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "ShowNpcName",			CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "ShowMonsterName",		CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "ShowHpBar",				CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "ShowDamage",				CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "ShowChatBubble",			CGameOptions::VALUE_BOOL,	1,	0,	1 },

		{ "EnableFog",				CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "EnableWeather",			CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "EnableAura",				CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "EnableItemGlow",			CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "EnableWingMotion",		CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "EnableCharacterShadow",	CGameOptions::VALUE_BOOL,	0,	0,	1 },

		{ "BackgroundThrottle",		CGameOptions::VALUE_BOOL,	1,	0,	1 },
		{ "ViewPlayerLimit",		CGameOptions::VALUE_INT,	0,	0,	400 },
		{ "ParticleLimit",			CGameOptions::VALUE_INT,	0,	0,	3000 },
	};

	bool IsValid(eGameOption Option)
	{
		return (Option >= 0 && Option < GAMEOPT_COUNT);
	}
}

CGameOptions::CGameOptions()
{
	m_bDirty = false;
	m_szFileName[0] = 0;

	ResetAllToDefault();
}

const CGameOptions::SDefinition* CGameOptions::GetDefinition(eGameOption Option)
{
	if (!IsValid(Option))
		return NULL;

	return &s_Definition[Option];
}

const char* CGameOptions::GetName(eGameOption Option)
{
	if (!IsValid(Option))
		return "";

	return s_Definition[Option].pszName;
}

int CGameOptions::FindByName(const char* pszName)
{
	if (pszName == NULL)
		return -1;

	for (int i = 0; i < GAMEOPT_COUNT; ++i)
	{
		if (_stricmp(s_Definition[i].pszName, pszName) == 0)
			return i;
	}

	return -1;
}

int CGameOptions::Clamp(eGameOption Option, int iValue) const
{
	if (!IsValid(Option))
		return 0;

	const SDefinition& def = s_Definition[Option];

	if (iValue < def.iMin)
		return def.iMin;

	if (iValue > def.iMax)
		return def.iMax;

	return iValue;
}

bool CGameOptions::IsOn(eGameOption Option) const
{
	if (!IsValid(Option))
		return false;

	return (m_iValue[Option] != 0);
}

int CGameOptions::GetValue(eGameOption Option) const
{
	if (!IsValid(Option))
		return 0;

	return m_iValue[Option];
}

void CGameOptions::SetOn(eGameOption Option, bool bOn)
{
	SetValue(Option, bOn ? 1 : 0);
}

void CGameOptions::SetValue(eGameOption Option, int iValue)
{
	if (!IsValid(Option))
		return;

	const int iClamped = Clamp(Option, iValue);

	if (m_iValue[Option] == iClamped)
		return;

	m_iValue[Option] = iClamped;
	m_bDirty = true;
}

void CGameOptions::Toggle(eGameOption Option)
{
	SetOn(Option, !IsOn(Option));
}

void CGameOptions::ResetToDefault(eGameOption Option)
{
	if (!IsValid(Option))
		return;

	SetValue(Option, s_Definition[Option].iDefault);
}

void CGameOptions::ResetAllToDefault()
{
	for (int i = 0; i < GAMEOPT_COUNT; ++i)
		m_iValue[i] = s_Definition[i].iDefault;

	m_bDirty = true;
}

bool CGameOptions::Load(const char* pszFileName)
{
	if (pszFileName == NULL || pszFileName[0] == 0)
		return false;

	strncpy(m_szFileName, pszFileName, sizeof(m_szFileName) - 1);
	m_szFileName[sizeof(m_szFileName) - 1] = 0;

	for (int i = 0; i < GAMEOPT_COUNT; ++i)
		m_iValue[i] = s_Definition[i].iDefault;

	pugi::xml_document file;
	pugi::xml_parse_result res = file.load_file(m_szFileName);

	if (res.status != pugi::status_ok)
	{
		//-- First run, or the file was removed: lay a complete one down so it
		//-- can be edited by hand.
		m_bDirty = true;
		Save(true);
		return false;
	}

	pugi::xml_node root = file.child("GameOptions");

	for (pugi::xml_node child = root.child("Option"); child; child = child.next_sibling("Option"))
	{
		//-- Unknown names are ignored rather than dropped, so an older client
		//-- reading a newer file does not fight over the contents.
		const int iOption = FindByName(child.attribute("name").as_string());

		if (iOption < 0)
			continue;

		m_iValue[iOption] = Clamp((eGameOption)iOption, child.attribute("value").as_int(s_Definition[iOption].iDefault));
	}

	m_bDirty = false;

	return true;
}

bool CGameOptions::Save(bool bForce)
{
	if (!bForce && !m_bDirty)
		return true;

	if (m_szFileName[0] == 0)
		strcpy(m_szFileName, "Data\\GameOptions.xml");

	pugi::xml_document file;

	pugi::xml_node decl = file.prepend_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";

	pugi::xml_node root = file.append_child("GameOptions");
	root.append_attribute("version") = 1;

	for (int i = 0; i < GAMEOPT_COUNT; ++i)
	{
		pugi::xml_node child = root.append_child("Option");
		child.append_attribute("name") = s_Definition[i].pszName;
		child.append_attribute("value") = m_iValue[i];
	}

	if (file.save_file(m_szFileName) == false)
		return false;

	m_bDirty = false;

	return true;
}
