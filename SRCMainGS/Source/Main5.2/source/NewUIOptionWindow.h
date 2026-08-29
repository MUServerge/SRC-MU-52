// NewUIOptionWindow.h: interface for the CNewUIOptionWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEWUIOPTIONWINDOW_H__1469FA1D_7C15_4AFE_AD6E_59C303E72BC0__INCLUDED_)
#define AFX_NEWUIOPTIONWINDOW_H__1469FA1D_7C15_4AFE_AD6E_59C303E72BC0__INCLUDED_

#pragma once

#include "NewUIManager.h"
#include "NewUIMyInventory.h"
#include "NewUIQuestProgress.h"
#include "NewUIListGroupButton.h"
#include "NewUIDropDown.h"

namespace SEASON3B
{
	
	class CNewUIOptionWindow  : public CNewUIObj 
	{
#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM
	public:
#endif // KJH_ADD_INGAMESHOP_UI_SYSTEM
		enum IMAGE_LIST
		{
			IMAGE_MAIN_TABLE_TOP_LEFT = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_LEFT,	//. newui_item_table01(L).tga (14,14)
			IMAGE_MAIN_TABLE_TOP_RIGHT = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_RIGHT,	//. newui_item_table01(R).tga (14,14)
			IMAGE_MAIN_TABLE_BOTTOM_LEFT = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_LEFT,	//. newui_item_table02(L).tga (14,14)
			IMAGE_MAIN_TABLE_BOTTOM_RIGHT = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_RIGHT,	//. newui_item_table02(R).tga (14,14)
			IMAGE_MAIN_TABLE_TOP_PIXEL = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_PIXEL,			//. newui_item_table03(up).tga (1, 14)
			IMAGE_MAIN_TABLE_BOTTOM_PIXEL = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_PIXEL,	//. newui_item_table03(dw).tga (1,14)
			IMAGE_MAIN_TABLE_LEFT_PIXEL = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_LEFT_PIXEL,		//. newui_item_table03(L).tga (14,1)
			IMAGE_MAIN_TABLE_RIGHT_PIXEL = CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_RIGHT_PIXEL,
			IMAGE_OPTION_FRAME_BACK = CNewUIMessageBoxMng::IMAGE_MSGBOX_BACK,
			IMAGE_OPTION_BTN_CLOSE = CNewUIMessageBoxMng::IMAGE_MSGBOX_BTN_CLOSE,
			IMAGE_OPTION_FRAME_DOWN = CNewUIMyInventory::IMAGE_INVENTORY_BACK_BOTTOM,

			IMAGE_OPTION_FRAME_UP = BITMAP_OPTION_BEGIN,
			IMAGE_OPTION_FRAME_LEFT,
			IMAGE_OPTION_FRAME_RIGHT,
			IMAGE_OPTION_LINE,
			IMAGE_OPTION_POINT,
			IMAGE_OPTION_BTN_CHECK,
			IMAGE_OPTION_EFFECT_BACK,
			IMAGE_OPTION_EFFECT_COLOR,
			IMAGE_OPTION_VOLUME_BACK,
			IMAGE_OPTION_VOLUME_COLOR,
			IMAGE_ND_BTN_L = CNewUIQuestProgress::IMAGE_QP_BTN_L,				// Quest_bt_L.tga	(17,36)
			IMAGE_ND_BTN_R = CNewUIQuestProgress::IMAGE_QP_BTN_R,
			IMAGE_CHECK_LIVE = BITMAP_IMAGE_FRAME_EMU + 10,
			IMAGE_UNCHECK_LIVE = BITMAP_IMAGE_FRAME_EMU + 11,
		};

	public:
		CNewUIOptionWindow();
		virtual ~CNewUIOptionWindow();

		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		void Release();
		
		void SetPos(int x, int y);
		
		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();

		float GetLayerDepth();	//. 10.5f
		float GetKeyEventOrder();	// 10.f;
		
		void OpenningProcess();
		void ClosingProcess();

		void SetGameOptions(BYTE GameOption);

		void SetAutoAttack(bool bAuto);
		bool IsAutoAttack();
		void SetWhisperSound(bool bSound);
		bool IsWhisperSound();
		void SetSlideHelp(bool bHelp);
		bool IsSlideHelp();
		void SetVolumeLevel(int iVolume);
		int GetVolumeLevel();
		void SetRenderLevel(int iRender);
		int GetRenderLevel();

		void SetRenderEffect(bool bHelp);
		bool GetRenderEffect();
		void SetRenderEquipment(bool bHelp);
		bool GetRenderEquipment();
		void SetRenderTerrain(bool bHelp);
		bool GetRenderTerrain();
		void SetRenderObjects(bool bHelp);
		bool GetRenderObjects();
		void RenderTable(float x, float y, float width, float height);
	private:
		enum OPTION_TAB
		{
			TAB_GAME = 0,
			TAB_GRAPHICS,
			TAB_OPTIONS,
			TAB_COUNT,
		};

		enum SKIN_IMAGE
		{
			SKIN_WINDOW = BITMAP_INTERFACE_OPTION_SKIN_BEGIN,
			SKIN_TAB,
			SKIN_CLOSE,
			SKIN_SECTION,
			SKIN_CHECKBOX,
			SKIN_DROPDOWN,
			SKIN_DROPDOWN_LIST,
			SKIN_DROPDOWN_ROW,
			SKIN_EFFECT_SELECTOR,
			SKIN_ACTION_BUTTON,
			SKIN_VOLUME_TRACK,
			SKIN_VOLUME_FILL,
			SKIN_VOLUME_THUMB,
			SKIN_SCROLL_TRACK,
			SKIN_SCROLL_THUMB,
			SKIN_SCROLL_ARROW,
		};

		void LoadImages();
		void UnloadImages();
		void SetButtonInfo();
		void UpdateChildPositions();
		bool UpdateDragEvent();
		bool UpdateTabMouseEvent();
		bool UpdateCheckboxMouseEvent();
		bool UpdateSliderMouseEvent();
		bool UpdateActionMouseEvent();
		void RenderFrame();
		void RenderContents();
		void RenderChecked(float x, float y, bool bEnable);
		void RenderButtons();
		void RenderTabs();
		void RenderSection(float y, const char* pszText);
		void RenderOptionRow(float y, const char* pszText, bool bChecked, bool bEnabled = true);
		void RenderGameTab();
		void RenderGraphicsTab();
		void RenderOptionsTab();
		void RenderActionButton(float x, float y, float width, const char* pszText, bool bEnabled = true);
		void change_resolution();
		void change_fontsize();
		void LoadResolution(const char* filename);
	private:
		CNewUIManager*				m_pNewUIMng;
		POINT						m_Pos;
		CNewUIButton m_BtnClose;
		bool m_bAutoAttack;
		bool m_bWhisperSound;
		bool m_bSlideHelp;
		int m_iVolumeLevel;
		int m_iRenderLevel;
		bool m_RenderEffect;
		bool m_RenderEquipment;
		bool m_RenderTerrain;
		bool m_RenderObjects;
		CNewUIListGroupButton resolutionList;
		CNewUIListGroupButton fonttextList;
		CNewUIDropDown m_ResolutionDropDown;
		CNewUIDropDown m_FontDropDown;
		int m_iActiveTab;
		bool m_bDragging;
		POINT m_DragOffset;
	};
}

#endif // !defined(AFX_NEWUIOPTIONWINDOW_H__1469FA1D_7C15_4AFE_AD6E_59C303E72BC0__INCLUDED_)
