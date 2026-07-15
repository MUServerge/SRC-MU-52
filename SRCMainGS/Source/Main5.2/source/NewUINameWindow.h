// NewUINameWindow.h: interface for the CNewUINameWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEWUINAMEWINDOW_H__76B140FF_46CB_4DB6_9DA2_5F84F294D212__INCLUDED_)
#define AFX_NEWUINAMEWINDOW_H__76B140FF_46CB_4DB6_9DA2_5F84F294D212__INCLUDED_

#pragma once

#include "NewUIManager.h"
#include "ZzzTexture.h"

#include <map>
#include <string>

namespace SEASON3B
{
	class CNewUINameWindow  : public CNewUIObj
	{
	public:
		CNewUINameWindow();
		virtual ~CNewUINameWindow();

		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		void Release();
		void LoadImages();
		void SetPos(int x, int y);
		
		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();
		void RenderTooltip(float x, float y, float width, float height, bool fixed = false);
		void RenderTooltip_old(float x, float y, float width, float height, bool fixed = false);
		float GetLayerDepth();
		bool GetItemNameVisible() {
			return m_bShowItemName;
		};
		float GetNpcNameOffsetY() const { return m_fNpcNameOffsetY; }
	private:
		struct NPC_ICON_RENDER_CONFIG
		{
			int effectSpeed;
			char iconFile[32];
		};

		void RenderName();
		void RenderNpcIcons();
		void LoadNpcIconConfig();
		int ResolveNpcIconTexture(const char* iconFile);

		CNewUIManager* m_pNewUIMng;
		POINT m_Pos;
		bool m_bShowItemName;
		bool m_bNpcIconRendererEnabled;
		int m_iNextNpcIconSlot;
		float m_fNpcNameOffsetY;
		float m_fNpcIconGap;
		float m_fNpcEffectOffsetX;
		float m_fNpcEffectOffsetY;
		std::map<int, NPC_ICON_RENDER_CONFIG> m_NpcIconConfig;
		std::map<std::string, int> m_NpcIconSlots;
	};
	
}

#endif // !defined(AFX_NEWUINAMEWINDOW_H__76B140FF_46CB_4DB6_9DA2_5F84F294D212__INCLUDED_)
