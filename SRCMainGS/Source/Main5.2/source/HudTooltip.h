#pragma once

#include <cstddef>

namespace HudTooltip
{
	struct Line
	{
		const char* text;
		unsigned int color;
		bool bold;
	};

	void RenderBoxScreen(float x, float y, float width, float height, float backgroundAlpha = 0.93f);
	void RenderCentered(float centerX, float topY, const Line* lines, std::size_t lineCount);
	void RenderCenteredText(float centerX, float topY, const char* text);
}
