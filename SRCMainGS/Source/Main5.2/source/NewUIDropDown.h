// NewUIDropDown.h: a combo box for the new-UI windows.
//
// Closed it is a single value box with an arrow button on the right; open it
// drops a scrollable list over whatever is underneath. Drawing is split into
// Render() (the box) and RenderPopup() (the open list) so the owner can draw
// every popup last and keep it above the rest of the window.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include "NewUIStyleFX.h"

namespace SEASON3B
{
	class CNewUIDropDown
	{
	public:
		CNewUIDropDown();
		~CNewUIDropDown();

		void Create(float x, float y, float width, float height, int iMaxVisible = 6);

		//. Optional texture skin. Without it the box falls back to the plain
		//. code-drawn plate.
		void SetSkin(int iBoxImage, float fBoxSrcWidth, float fBoxSrcHeight, float fBoxCorner,
			int iArrowImage, float fArrowSrcSize);
		void SetPopupSkin(int iListImage, float fListSrcWidth, float fListSrcHeight,
			int iRowImage, float fRowSrcWidth, float fRowSrcHeight,
			int iScrollTrackImage, float fScrollTrackSrcWidth, float fScrollTrackSrcHeight,
			int iScrollThumbImage, float fScrollThumbSrcWidth, float fScrollThumbSrcHeight,
			int iScrollArrowImage, float fScrollArrowSrcSize, float fPopupWidth);
		void SetPosition(float x, float y);

		void Clear();
		void PushBack(const std::string& strText);
		size_t GetCount() const { return m_TextList.size(); }

		void SetCurrent(int iIndex);
		int GetCurrent() const { return m_iCurrent; }
		std::string AsString() const;
		int AsInteger() const;

		bool IsOpen() const { return m_bOpen; }
		void Close();

		//. True when the mouse is inside the box or the open list, so the owner
		//. can stop its own rows from reacting underneath.
		bool IsMouseOver() const;

		//. True when the selection changed this frame.
		bool UpdateMouseEvent();

		void Render();
		void RenderPopup();

	private:
		float GetPopupHeight() const;
		float GetPopupX() const;
		float GetPopupWidth() const;
		int GetHoverIndex() const;
		void ClampScroll();

		POINT m_Pos;
		float m_fWidth;
		float m_fHeight;
		int m_iMaxVisible;
		int m_iCurrent;
		int m_iScroll;
		bool m_bOpen;
		std::vector<std::string> m_TextList;
		CUIFade m_BoxFade;
		int m_iBoxImage;
		float m_fBoxSrcWidth;
		float m_fBoxSrcHeight;
		float m_fBoxCorner;
		int m_iArrowImage;
		float m_fArrowSrcSize;
		int m_iListImage;
		float m_fListSrcWidth;
		float m_fListSrcHeight;
		int m_iRowImage;
		float m_fRowSrcWidth;
		float m_fRowSrcHeight;
		int m_iScrollTrackImage;
		float m_fScrollTrackSrcWidth;
		float m_fScrollTrackSrcHeight;
		int m_iScrollThumbImage;
		float m_fScrollThumbSrcWidth;
		float m_fScrollThumbSrcHeight;
		int m_iScrollArrowImage;
		float m_fScrollArrowSrcSize;
		float m_fPopupWidth;
	};
}
