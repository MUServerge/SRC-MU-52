#pragma once
#include "NewUIBase.h"
#include "NewUIManager.h"
#include "UIControlRender.h"

namespace SEASON3B
{
	//-- Master Reset, Reset and Level are merged into one table; everything
	//-- after them is an event board, paged with the arrows as before.
	enum { RANK_MERGED_BOARDS = 3 };

	//-- One ranked player. Strings are formatted when the packet arrives, not
	//-- on every frame of every visible row.
	struct SRankingEntry
	{
		char	szRank[8];
		char	szName[24];
		char	szClass[32];
		char	szScore[24];
		BYTE	byClass;
		BYTE	byVip;
		int		iScore;
		DWORD	dwEquipment[EQUIPMENT_LENGTH];
	};

	struct SRankingBoard
	{
		bool						bLoaded;
		char						szName[24];
		char						szScoreColumn[24];
		std::vector<SRankingEntry>	Entries;

		SRankingBoard() : bLoaded(false)
		{
			szName[0] = 0;
			szScoreColumn[0] = 0;
		}
	};

	//-- One player folded across the merged boards.
	struct SRankingMerged
	{
		char	szRank[8];
		char	szName[24];
		char	szClass[32];
		char	szScore[RANK_MERGED_BOARDS][16];
		int		iScore[RANK_MERGED_BOARDS];
		short	sBoard;			//. where the equipment came from
		short	sEntry;
	};

	class CNewUIRankingTop : public CNewUIObj
	{
		enum
		{
			MAX_RANKING_BOARD = 10,		//. MAX_RANK in the GameServer
			ROWS_VISIBLE = 10,
		};

		enum IMAGE_LIST
		{
			IMAGE_TOP_BACK1 = BITMAP_IMAGE_FRAME_EMU + 1,
			IMAGE_TOP_BACK2 = BITMAP_IMAGE_FRAME_EMU + 2,
			IMAGE_TOP_BACK3 = BITMAP_IMAGE_FRAME_EMU + 3,
			IMAGE_TOP_LEVEL1 = BITMAP_IMAGE_FRAME_EMU + 4,
			IMAGE_TOP_LEVEL2 = BITMAP_IMAGE_FRAME_EMU + 5,
			IMAGE_TOP_LEVEL3 = BITMAP_IMAGE_FRAME_EMU + 6,
		};

	private:
		CNewUIManager* m_pNewUIMng;
		POINT m_Pos;
		CUIPhotoViewer m_RenderCharacter;
		CNewUIScrollBarHTML m_pScrollBar;

		SRankingBoard m_Board[MAX_RANKING_BOARD];
		std::vector<SRankingMerged> m_Table;
		int m_iBoardCount;

		//-- Page 0 is the merged table, 1..n are the event boards.
		int m_iPage;
		int m_iSelectRow;
		int m_iHoverRow;

		int m_iPendingBoard;
		float m_fRequestTime;

		bool m_bDragging;
		float m_fDragOffsetX;
		float m_fDragOffsetY;

	public:
		CNewUIRankingTop();
		virtual ~CNewUIRankingTop();

		bool Create(CNewUIManager* pNewUIMng, float x, float y);
		void Release();
		void SetInfo();
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

		void RenderFrame();
		void RenderTexte();

		void ReceiveRankingInfo(BYTE* ReceiveBuffer);
		void ReceiveRankingListInfo(BYTE* ReceiveBuffer);

	private:
		void RequestServerRankingInfo(int iBoard);
		void PumpRequests();
		void RebuildTable();

		int GetPageCount() const;
		int GetPageBoard() const;			//. -1 while the merged table is shown
		int GetRowCount() const;
		int GetFirstVisibleRow();
		void StepPage(int iDelta);
		void SelectRow(int iRow);

		void GetListRect(float& x, float& y, float& width, float& height) const;
		void GetRowRect(int iVisibleSlot, float& x, float& y, float& width, float& height) const;
		void GetPageArrowRect(bool bRight, float& x, float& y, float& width, float& height) const;
		void GetTitleRect(float& x, float& y, float& width, float& height) const;
		void GetCloseRect(float& x, float& y, float& width, float& height) const;

		void RenderHeader();
		void RenderRows();

		void UpdateChildPositions();
		void ClampPosition();
		bool UpdateDragEvent();
	};
}
