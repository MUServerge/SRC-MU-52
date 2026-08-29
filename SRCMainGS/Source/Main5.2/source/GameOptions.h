// GameOptions.h: local, per-machine client options.
//
// The account options the server knows about live in a single byte
// (SaveOptions() packs them into options[20] and sends them with the hotkeys),
// and that byte has exactly one free bit left. Everything below is a rendering
// preference that belongs to the machine, not to the account, so it is kept in
// Data\GameOptions.xml instead and never touches the protocol.
//
// Values are held in a flat array indexed by the enum, so the render hot paths
// can read them without a string lookup:
//
//     if (gGameOptions.IsOn(GAMEOPT_SHOW_GUILD_LOGO) == false)
//         return;
//
//////////////////////////////////////////////////////////////////////

#pragma once

enum eGameOption
{
	//-- Nameplates and overhead information
	GAMEOPT_SHOW_PLAYER_NAME = 0,
	GAMEOPT_SHOW_MY_NAME,
	GAMEOPT_SHOW_GUILD_NAME,
	GAMEOPT_SHOW_GUILD_LOGO,
	GAMEOPT_SHOW_NPC_NAME,
	GAMEOPT_SHOW_MONSTER_NAME,
	GAMEOPT_SHOW_HP_BAR,
	GAMEOPT_SHOW_DAMAGE,
	GAMEOPT_SHOW_CHAT_BUBBLE,

	//-- World and character effects
	GAMEOPT_ENABLE_FOG,
	GAMEOPT_ENABLE_WEATHER,
	GAMEOPT_ENABLE_AURA,
	GAMEOPT_ENABLE_ITEM_GLOW,
	GAMEOPT_ENABLE_WING_MOTION,
	GAMEOPT_ENABLE_CHARACTER_SHADOW,

	//-- Load shedding
	GAMEOPT_BACKGROUND_THROTTLE,	//. drop render work while the window is not in front
	GAMEOPT_VIEW_PLAYER_LIMIT,		//. 0 = unlimited
	GAMEOPT_PARTICLE_LIMIT,			//. 0 = unlimited

	GAMEOPT_COUNT
};

class CGameOptions
{
public:
	enum EValueType
	{
		VALUE_BOOL = 0,
		VALUE_INT,
	};

	struct SDefinition
	{
		const char* pszName;
		EValueType	Type;
		int			iDefault;
		int			iMin;
		int			iMax;
	};

	CGameOptions();

	//. Reads Data\GameOptions.xml, filling anything missing with the default.
	//. Writes the file back when it did not exist, so a fresh install ends up
	//. with a complete, editable file.
	bool Load(const char* pszFileName = "Data\\GameOptions.xml");

	//. Only touches the disk when something actually changed.
	bool Save(bool bForce = false);

	bool IsOn(eGameOption Option) const;
	int GetValue(eGameOption Option) const;

	void SetOn(eGameOption Option, bool bOn);
	void SetValue(eGameOption Option, int iValue);
	void Toggle(eGameOption Option);

	void ResetToDefault(eGameOption Option);
	void ResetAllToDefault();

	bool IsDirty() const { return m_bDirty; }

	static const SDefinition* GetDefinition(eGameOption Option);
	static const char* GetName(eGameOption Option);
	static int FindByName(const char* pszName);		//. -1 when unknown

private:
	int Clamp(eGameOption Option, int iValue) const;

	int		m_iValue[GAMEOPT_COUNT];
	bool	m_bDirty;
	char	m_szFileName[260];
};

extern CGameOptions gGameOptions;
