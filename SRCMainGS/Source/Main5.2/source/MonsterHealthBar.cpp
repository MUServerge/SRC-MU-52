#include "stdafx.h"
#include "MonsterHealthBar.h"
#include "pugixml.hpp"

namespace
{
	// World projection and CGMHeadChat use physical pixels. Keep the overhead
	// bar in that coordinate space so camera zoom can move only its anchor, not
	// resize the UI. These values are the MuDream texture's intended display size.
	const float MONSTER_BAR_WIDTH = 165.f;
	const float MONSTER_BAR_HEIGHT = 18.f;
	const float MONSTER_FILL_X = 5.5f;
	const float MONSTER_FILL_Y = 4.f;
	const float MONSTER_FILL_WIDTH = 154.f;
	const float MONSTER_FILL_HEIGHT = 10.f;
	const float MONSTER_DIVISOR_Y = 2.f;
	const float MONSTER_DIVISOR_WIDTH = 4.5f;
	const float MONSTER_DIVISOR_HEIGHT = 14.f;
	const float BOSS_HUD_Y = 50.f;
	const BYTE NAME_COLOR_RED = 255;
	const BYTE NAME_COLOR_GREEN = 204;
	const BYTE NAME_COLOR_BLUE = 25;

	struct BossConfig
	{
		int Type;
		int PageCount;
	};

	struct BossAtlasLayout
	{
		float FrameSourceY;
		float FrameWidth;
		float FrameHeight;
		float FillSourceY;
		float FillSourceWidth;
		float FillSourceHeight;
		float FillX;
		float FillY;
		float LevelCenterX;
	};

	// Source rectangles inside boss_health_bar.tga. Destination dimensions are
	// kept at native atlas pixels through RenderImage2F (no Look5/camera scaling).
	const BossAtlasLayout BOSS_LAYOUTS[4] =
	{
		{   0.f, 280.f, 35.f, 235.f, 176.f, 12.f,  57.f, 11.f,  29.f },
		{  88.f, 530.f, 67.f, 555.f, 383.f, 20.f,  76.f, 23.f,  39.f },
		{  35.f, 410.f, 53.f, 355.f, 300.f, 20.f,  66.f, 17.f,  32.f },
		{ 155.f, 690.f, 80.f, 755.f, 460.f, 24.f, 136.f, 30.f, 103.f },
	};

	std::unordered_map<unsigned int, BossConfig> s_Bosses;
	bool s_Loaded = false;
	bool s_NormalTexturesLoaded = false;
	bool s_BossTextureLoaded = false;

	float ClampLife(DWORD currentLife, DWORD maximumLife)
	{
		if (maximumLife == 0)
			return 0.f;

		return max(0.f, min(1.f, currentLife / static_cast<float>(maximumLife)));
	}

	void LoadBossConfiguration()
	{
		s_Bosses.clear();

		pugi::xml_document document;
		if (!document.load_file("Data\\Local\\xml\\BossHealthBar.xml"))
			return;

		for (pugi::xml_node node = document.child("BossHealthBar").child("boss");
			node; node = node.next_sibling("boss"))
		{
			const unsigned int monsterClass = node.attribute("id").as_uint();
			BossConfig config;
			config.Type = max(0, min(3, node.attribute("type").as_int()));
			config.PageCount = max(1, node.attribute("page_count").as_int(1));
			s_Bosses[monsterClass] = config;
		}
	}

	void RestoreTextState(HFONT font, DWORD textColor, DWORD bgColor)
	{
		g_pRenderText->SetFont(font);
		g_pRenderText->SetTextColor(textColor);
		g_pRenderText->SetBgColor(bgColor);
		glColor4f(1.f, 1.f, 1.f, 1.f);
	}

	int ToLogicalX(float physicalX)
	{
		return static_cast<int>(physicalX / max(0.01f, g_fScreenRate_x));
	}

	int ToLogicalY(float physicalY)
	{
		return static_cast<int>(physicalY / max(0.01f, g_fScreenRate_y));
	}

	void RenderNormalMonster(int centerX, int screenY, const char* name,
		unsigned int level, DWORD currentLife, DWORD maximumLife, bool selected)
	{
		if (!s_NormalTexturesLoaded)
			return;

		const float life = ClampLife(currentLife, maximumLife);
		const float x = static_cast<float>(centerX) - MONSTER_BAR_WIDTH * 0.5f;
		const float y = static_cast<float>(screenY);

		if (x + MONSTER_BAR_WIDTH < 0.f || x >= WindowWidth
			|| y + MONSTER_BAR_HEIGHT < 0.f || y >= WindowHeight)
		{
			return;
		}

		const bool hovered = selected
			|| (MouseRenderX >= x && MouseRenderX < x + MONSTER_BAR_WIDTH
				&& MouseRenderY >= y - 22.f && MouseRenderY < y + MONSTER_BAR_HEIGHT + 5.f);

		EnableAlphaTest(true);
		glColor4f(1.f, 1.f, 1.f, 1.f);

		SEASON3B::RenderImage2F(MonsterHealthBar::IMAGE_MONSTER_BACKGROUND,
			x, y, MONSTER_BAR_WIDTH, MONSTER_BAR_HEIGHT, 0.f, 0.f, 420.f, 36.f);

		if (life > 0.f)
		{
			SEASON3B::RenderImage2F(MonsterHealthBar::IMAGE_MONSTER_FILL_RED,
				x + MONSTER_FILL_X, y + MONSTER_FILL_Y,
				MONSTER_FILL_WIDTH * life, MONSTER_FILL_HEIGHT,
				0.f, 0.f, 392.f * life, 20.f);
		}

		// Ten stable HP sections use the provided divisor texture. They are drawn
		// independently from the fill so depletion never stretches the separators.
		for (int section = 1; section < 10; ++section)
		{
			const float divisorX = x + MONSTER_FILL_X
				+ (MONSTER_FILL_WIDTH * section / 10.f) - MONSTER_DIVISOR_WIDTH * 0.5f;
			SEASON3B::RenderImage2F(MonsterHealthBar::IMAGE_MONSTER_DIVISOR,
				divisorX, y + MONSTER_DIVISOR_Y, MONSTER_DIVISOR_WIDTH, MONSTER_DIVISOR_HEIGHT,
				0.f, 0.f, 12.f, 28.f);
		}

		const HFONT oldFont = g_pRenderText->GetFont();
		const DWORD oldTextColor = g_pRenderText->GetTextColor();
		const DWORD oldBgColor = g_pRenderText->GetBgColor();
		g_pRenderText->SetFont(g_hFontBold);
		g_pRenderText->SetBgColor(0, 0, 0, 0);

		char lifeText[48];
		sprintf_s(lifeText, "%u / %u",
			static_cast<unsigned int>(currentLife), static_cast<unsigned int>(maximumLife));
		g_pRenderText->SetTextColor(255, 255, 255, 255);
		g_pRenderText->RenderText(ToLogicalX(static_cast<float>(centerX)),
			ToLogicalY(y + 1.f), lifeText, 0, 0, RT3_WRITE_CENTER, NULL, true);

		if (hovered && name != NULL && name[0] != '\0')
		{
			char title[96];
			sprintf_s(title, "%s lvl: %u", name, level);

			g_pRenderText->SetTextColor(
				NAME_COLOR_RED, NAME_COLOR_GREEN, NAME_COLOR_BLUE, 255);
			g_pRenderText->RenderText(ToLogicalX(static_cast<float>(centerX)),
				ToLogicalY(y - 15.f), title, 0, 0, RT3_WRITE_CENTER, NULL, true);
		}

		RestoreTextState(oldFont, oldTextColor, oldBgColor);
	}

	void RenderBoss(const BossConfig& config, int centerX, int screenY,
		const char* name, unsigned int level, DWORD currentLife, DWORD maximumLife)
	{
		if (!s_BossTextureLoaded)
			return;

		const BossAtlasLayout& layout = BOSS_LAYOUTS[config.Type];
		const float life = ClampLife(currentLife, maximumLife);
		const float scaledLife = life * config.PageCount;
		int remainingPages = static_cast<int>(ceilf(scaledLife));
		remainingPages = max(0, min(config.PageCount, remainingPages));

		float pageLife = 0.f;
		if (remainingPages > 0)
		{
			pageLife = scaledLife - floorf(scaledLife);
			if (pageLife <= 0.f)
				pageLife = 1.f;
		}

		const int pageIndex = min(9, max(0, config.PageCount - remainingPages));
		const bool followsMonster = (config.Type == 1);
		const float frameX = followsMonster
			? static_cast<float>(centerX) - layout.FrameWidth * 0.5f
			: (WindowWidth - layout.FrameWidth) * 0.5f;
		const float frameY = followsMonster
			? static_cast<float>(screenY)
			: ((config.Type == 2 || config.Type == 3) ? BOSS_HUD_Y : 22.f);

		if (followsMonster
			&& (frameX + layout.FrameWidth < 0.f || frameX >= WindowWidth
				|| frameY + layout.FrameHeight < 0.f || frameY >= WindowHeight))
		{
			return;
		}

		EnableAlphaTest(true);
		glColor4f(1.f, 1.f, 1.f, 1.f);
		SEASON3B::RenderImage2F(MonsterHealthBar::IMAGE_BOSS_ATLAS,
			frameX, frameY, layout.FrameWidth, layout.FrameHeight,
			0.f, layout.FrameSourceY, layout.FrameWidth, layout.FrameHeight);

		if (pageLife > 0.f)
		{
			SEASON3B::RenderImage2F(MonsterHealthBar::IMAGE_BOSS_ATLAS,
				frameX + layout.FillX, frameY + layout.FillY,
				layout.FillSourceWidth * pageLife, layout.FillSourceHeight,
				0.f, layout.FillSourceY + pageIndex * layout.FillSourceHeight,
				layout.FillSourceWidth * pageLife, layout.FillSourceHeight);
		}

		const HFONT oldFont = g_pRenderText->GetFont();
		const DWORD oldTextColor = g_pRenderText->GetTextColor();
		const DWORD oldBgColor = g_pRenderText->GetBgColor();
		g_pRenderText->SetFont(g_hFontBold);
		g_pRenderText->SetBgColor(0, 0, 0, 0);

		if (name != NULL && name[0] != '\0')
		{
			g_pRenderText->SetTextColor(
				NAME_COLOR_RED, NAME_COLOR_GREEN, NAME_COLOR_BLUE, 255);
			g_pRenderText->RenderText(ToLogicalX(frameX + layout.FrameWidth * 0.5f),
				ToLogicalY(frameY - 13.f), name, 0, 0, RT3_WRITE_CENTER, NULL, true);
		}

		g_pRenderText->SetTextColor(230, 230, 230, 255);
		char text[32];
		sprintf_s(text, "%u", level);
		g_pRenderText->RenderText(ToLogicalX(frameX + layout.LevelCenterX),
			ToLogicalY(frameY + layout.FillY + 1.f), text, 0, 0, RT3_WRITE_CENTER, NULL, true);

		sprintf_s(text, "x%d", remainingPages);
		g_pRenderText->RenderText(ToLogicalX(frameX + layout.FillX + layout.FillSourceWidth - 10.f),
			ToLogicalY(frameY + layout.FillY + 2.f), text, 0, 0, RT3_WRITE_CENTER, NULL, true);

		RestoreTextState(oldFont, oldTextColor, oldBgColor);
	}
}

void MonsterHealthBar::Load()
{
	if (s_Loaded)
		return;

	// The runtime loader resolves these source TGA names to converted OZT files.
	// All resources are owned by Interface\HealthBar; Quest System is unrelated.
	const bool backgroundLoaded = LoadBitmap("Interface\\HealthBar\\background.tga",
		IMAGE_MONSTER_BACKGROUND, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	const bool divisorLoaded = LoadBitmap("Interface\\HealthBar\\divisor.tga",
		IMAGE_MONSTER_DIVISOR, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	LoadBitmap("Interface\\HealthBar\\divisor_original.tga",
		IMAGE_MONSTER_DIVISOR_ORIGINAL, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	LoadBitmap("Interface\\HealthBar\\fill_character.tga",
		IMAGE_CHARACTER_FILL, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	LoadBitmap("Interface\\HealthBar\\fill_green.tga",
		IMAGE_MONSTER_FILL_GREEN, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	LoadBitmap("Interface\\HealthBar\\fill_orange.tga",
		IMAGE_MONSTER_FILL_ORANGE, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	LoadBitmap("Interface\\HealthBar\\fill_orange_ori.tga",
		IMAGE_MONSTER_FILL_ORANGE_ORIGINAL, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	const bool redLoaded = LoadBitmap("Interface\\HealthBar\\fill_red.tga",
		IMAGE_MONSTER_FILL_RED, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	LoadBitmap("Interface\\HealthBar\\monster_tier.tga",
		IMAGE_MONSTER_TIER, GL_LINEAR, GL_CLAMP_TO_EDGE, false);
	s_BossTextureLoaded = LoadBitmap("Interface\\HealthBar\\boss_health_bar.tga",
		IMAGE_BOSS_ATLAS, GL_LINEAR, GL_CLAMP_TO_EDGE, false);

	s_NormalTexturesLoaded = backgroundLoaded && divisorLoaded && redLoaded;
	LoadBossConfiguration();
	s_Loaded = true;
}

void MonsterHealthBar::Unload()
{
	if (!s_Loaded)
		return;

	for (int image = IMAGE_MONSTER_BACKGROUND; image <= IMAGE_BOSS_ATLAS; ++image)
		DeleteBitmap(image);

	s_Bosses.clear();
	s_NormalTexturesLoaded = false;
	s_BossTextureLoaded = false;
	s_Loaded = false;
}

bool MonsterHealthBar::RenderMonster(int centerX, int screenY, const char* name,
	unsigned int monsterClass, unsigned int level,
	DWORD currentLife, DWORD maximumLife, bool selected)
{
	const std::unordered_map<unsigned int, BossConfig>::const_iterator boss = s_Bosses.find(monsterClass);
	if (boss != s_Bosses.end())
	{
		if (selected)
			RenderBoss(boss->second, centerX, screenY,
				name, level, currentLife, maximumLife);
		return true;
	}

	RenderNormalMonster(centerX, screenY, name, level, currentLife, maximumLife, selected);
	return false;
}
