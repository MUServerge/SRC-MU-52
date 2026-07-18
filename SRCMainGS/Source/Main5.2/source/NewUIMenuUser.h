#pragma once
#include "NewUIBase.h"
#include "NewUIManager.h"

namespace SEASON3B
{
	class CNewUIMenuUser : public CNewUIObj
	{
		enum { MENU_ITEM_COUNT = 16 };
		enum IMAGE_LIST
		{
			IMG_ICON_BEGIN = BITMAP_INTERFACE_MAINMENU_BEGIN,
			IMG_BG = BITMAP_INTERFACE_MAINMENU_BEGIN + 16,
			IMG_TITLE = BITMAP_INTERFACE_MAINMENU_BEGIN + 17,
			IMG_TILE = BITMAP_INTERFACE_MAINMENU_BEGIN + 18,
			IMG_TILE_PRESS = BITMAP_INTERFACE_MAINMENU_BEGIN + 19,
			IMG_CLOSE = BITMAP_INTERFACE_MAINMENU_BEGIN + 20,
			IMG_CLOSE_OVER = BITMAP_INTERFACE_MAINMENU_BEGIN + 21,
			IMG_CLOSE_DOWN = BITMAP_INTERFACE_MAINMENU_BEGIN + 22,
			IMG_BTN = BITMAP_INTERFACE_MAINMENU_BEGIN + 23,
		};
	private:
		CNewUIManager* m_pNewUIMng;
		POINT m_Pos;
		int m_iHoverItem;
		bool m_bHoverClose;
		bool m_bHoverBottom;
	public:
		CNewUIMenuUser();
		virtual~CNewUIMenuUser();
		bool Create(CNewUIManager* pNewUIMng, float x, float y);
		void Release();

		void SetPos(float x, float y);
		void LoadImages();
		void UnloadImages();

		bool UpdateKeyEvent();
		bool UpdateMouseEvent();
		bool Render();
		bool Update();
		float GetLayerDepth();

		void OpenningProcess();
		void ClosingProcess();
	private:
		void GetItemRect(int index, float& x, float& y, float& w, float& h);
		void GetCloseRect(float& x, float& y, float& w, float& h);
		void GetBottomRect(float& x, float& y, float& w, float& h);
		bool IsItemEnabled(int index) const;
		bool ActivateItem(int index);
		void RenderFrame();
		void RenderItems();
		void RenderCloseButton();
		void RenderBottomButton();
	};
}
