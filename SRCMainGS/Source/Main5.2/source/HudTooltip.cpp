#include "stdafx.h"
#include "HudTooltip.h"
#include "UIControls.h"

namespace
{
	const float PADDING_X = 6.f;
	const float PADDING_Y = 4.f;
	const float LINE_GAP = 1.f;
	const float EMPTY_LINE_HEIGHT = 4.f;
	const float SCREEN_MARGIN = 2.f;

	bool IsEmptyLine(const char* text)
	{
		return text == NULL || text[0] == '\0' || text[0] == '\n';
	}
}

void HudTooltip::RenderBoxScreen(float x, float y, float width, float height, float backgroundAlpha)
{
	const float edge = 1.f;
	backgroundAlpha = max(0.f, min(1.f, backgroundAlpha));

	EnableAlphaTest(false);

	// Keep the shadow and border at one physical pixel. Using virtual-pixel widths here
	// made the top/left edge look heavier at high resolutions after UI scaling.
	glColor4f(0.f, 0.f, 0.f, 0.50f);
	RenderColor(x - edge, y - edge, width + edge * 2.f, height + edge * 2.f, 0.f, 0, false);
	glColor4f(0.015f, 0.018f, 0.022f, backgroundAlpha);
	RenderColor(x, y, width, height, 0.f, 0, false);

	glColor4f(0.50f, 0.54f, 0.60f, 0.88f);
	RenderColor(x, y, width, edge, 0.f, 0, false);
	RenderColor(x, y + height - edge, width, edge, 0.f, 0, false);
	RenderColor(x, y, edge, height, 0.f, 0, false);
	RenderColor(x + width - edge, y, edge, height, 0.f, 0, false);
	EndRenderColor();
}

void HudTooltip::RenderCentered(float centerX, float topY, const Line* lines, std::size_t lineCount)
{
	if (lines == NULL || lineCount == 0)
		return;

	std::vector<float> lineHeights(lineCount, EMPTY_LINE_HEIGHT);
	float contentWidth = 0.f;
	float contentHeight = 0.f;

	for (std::size_t i = 0; i < lineCount; ++i)
	{
		if (!IsEmptyLine(lines[i].text))
		{
			SIZE textSize = { 0, 0 };
			g_pRenderText->SetFont(lines[i].bold ? g_hFontBold : g_hFont);
			g_pMultiLanguage->_GetTextExtentPoint32(
				g_pRenderText->GetFontDC(), lines[i].text, lstrlen(lines[i].text), &textSize);

			contentWidth = max(contentWidth, (float)textSize.cx / g_fScreenRate_x);
			lineHeights[i] = max(1.f, (float)textSize.cy / g_fScreenRate_y);
		}

		contentHeight += lineHeights[i];
		if (i + 1 < lineCount)
			contentHeight += LINE_GAP;
	}

	const float boxWidth = contentWidth + PADDING_X * 2.f;
	const float boxHeight = contentHeight + PADDING_Y * 2.f;
	const float screenWidth = (float)WindowWidth / g_fScreenRate_x;
	const float screenHeight = (float)WindowHeight / g_fScreenRate_y;
	float boxX = centerX - boxWidth * 0.5f;
	float boxY = topY;

	boxX = max(SCREEN_MARGIN, min(screenWidth - boxWidth - SCREEN_MARGIN, boxX));
	boxY = max(SCREEN_MARGIN, min(screenHeight - boxHeight - SCREEN_MARGIN, boxY));

	RenderBoxScreen(
		boxX * g_fScreenRate_x,
		boxY * g_fScreenRate_y,
		boxWidth * g_fScreenRate_x,
		boxHeight * g_fScreenRate_y);

	const HFONT oldFont = g_pRenderText->GetFont();
	const DWORD oldTextColor = g_pRenderText->GetTextColor();
	const DWORD oldBgColor = g_pRenderText->GetBgColor();
	float textY = boxY + PADDING_Y;

	g_pRenderText->SetBgColor(0, 0, 0, 0);
	for (std::size_t i = 0; i < lineCount; ++i)
	{
		if (!IsEmptyLine(lines[i].text))
		{
			g_pRenderText->SetFont(lines[i].bold ? g_hFontBold : g_hFont);
			g_pRenderText->SetTextColor(lines[i].color);
			g_pRenderText->RenderTextClipped(
				boxX * g_fScreenRate_x,
				textY * g_fScreenRate_y,
				lines[i].text,
				boxWidth * g_fScreenRate_x,
				lineHeights[i] * g_fScreenRate_y,
				RT3_SORT_CENTER);
		}

		textY += lineHeights[i] + LINE_GAP;
	}

	g_pRenderText->SetFont(oldFont);
	g_pRenderText->SetTextColor(oldTextColor);
	g_pRenderText->SetBgColor(oldBgColor);
	glColor4f(1.f, 1.f, 1.f, 1.f);
}

void HudTooltip::RenderCenteredText(float centerX, float topY, const char* text)
{
	const Line line = { text, RGBA(230, 232, 236, 255), false };
	RenderCentered(centerX, topY, &line, 1);
}
