#pragma once

namespace MonsterHealthBar
{
	enum IMAGE_LIST
	{
		IMAGE_MONSTER_BACKGROUND = BITMAP_INTERFACE_HEALTHBAR_BEGIN,
		IMAGE_MONSTER_DIVISOR,
		IMAGE_MONSTER_DIVISOR_ORIGINAL,
		IMAGE_CHARACTER_FILL,
		IMAGE_MONSTER_FILL_GREEN,
		IMAGE_MONSTER_FILL_ORANGE,
		IMAGE_MONSTER_FILL_ORANGE_ORIGINAL,
		IMAGE_MONSTER_FILL_RED,
		IMAGE_MONSTER_TIER,
		IMAGE_BOSS_ATLAS,
	};

	void Load();
	void Unload();

	// Returns true when the monster is configured as a boss. Type 1 follows its
	// projected monster anchor; the larger boss types use the top-center HUD.
	bool RenderMonster(int centerX, int screenY, const char* name,
		unsigned int monsterClass, unsigned int level,
		DWORD currentLife, DWORD maximumLife, bool selected);
}
