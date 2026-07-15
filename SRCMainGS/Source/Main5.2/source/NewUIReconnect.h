#pragma once
#include "NewUIBase.h"
#include "NewUIManager.h"

enum eReconnectStatus
{
	RECONNECT_STATUS_NONE = 0,
	RECONNECT_STATUS_RECONNECT = 1,
	RECONNECT_STATUS_DISCONNECT = 2,
};

enum eReconnectProgress
{
	RECONNECT_PROGRESS_NONE = 0,
	RECONNECT_PROGRESS_CONNECTED = 1,
	RECONNECT_PROGRESS_JOINED = 2,
	RECONNECT_PROGRESS_CHAR_LIST = 3,
	RECONNECT_PROGRESS_CHAR_INFO = 4,
};

namespace SEASON3B
{
	class CNewUIReconnect : public CNewUIObj
	{
	public:
		// LegendUI\reconnect.ozt is a 440x160 ATLAS: window frame (0,0 440x102),
		// progress fill (0,108 362x24), progress track (0,132 368x28).
		// On-screen layout below is in virtual 640x480 px and keeps the window aspect.
		enum
		{
			RECONNECT_WIDTH      = 352,	// window
			RECONNECT_HEIGHT     = 82,	// 352 * 102/440
			RECONNECT_TITLE_TOP  = 9,	// "Reconnecting..." centered in the TITLE BAR
			RECONNECT_TITLE_HEIGHT = 16,
			RECONNECT_BAR_X      = 29,	// progress track, centered in the body
			RECONNECT_BAR_Y      = 50,
			RECONNECT_BAR_WIDTH  = 294,	// 352 * 368/440
			RECONNECT_BAR_HEIGHT = 16,	// slim so it sits inside the frame
			RECONNECT_BAR_INSET  = 2,	// fill inset inside the track border
		};

	private:
		CNewUIManager* m_pNewUIMng;
		POINT m_Pos;
		char GameServerAddress[16];
		WORD GameServerPort;
		char ReconnectAccount[11];
		char ReconnectPassword[11];
		char ReconnectName[11];
		char ReconnectMapServerAddress[16];
		WORD ReconnectMapServerPort;
		DWORD ReconnectStatus;
		DWORD ReconnectProgress;
		DWORD ReconnectCurTime;
		DWORD ReconnectMaxTime;
		DWORD ReconnectCurWait;
		DWORD ReconnectMaxWait;
		DWORD ReconnectAuthSend;
		DWORD ReconnectHelperOn;
	public:
		CNewUIReconnect();
		virtual~CNewUIReconnect();
		bool Create(CNewUIManager* pNewUIMng, int ix, int iy);
		void Release();
		void SetPos(int x, int y);
		bool UpdateKeyEvent();
		bool UpdateMouseEvent();
		bool Render();
		bool Update();
		float GetLayerDepth(); //. 10.5f
	private:
		void RenderFrame();
		void RenderText();
	public:
		DWORD GetReconnectStatus();
		DWORD GetReconnectMaxTime();
		DWORD GetReconnectMaxWait();
		DWORD GetReconnectCurTime();
		DWORD GetReconnectCurWait();
		DWORD GetReconnectProgress();
		void ReconnecGameServerLoad();
		void ReconnecGameServerAuth();
		void SetReconnectCurTime(DWORD value);
		void ReconnectSetInfo(DWORD status, DWORD progress, DWORD CurWait, DWORD MaxWait);
		void ReconnectOnCloseSocket();
		void ReconnectOnMapServerMove(char* address, WORD port);
		void ReconnectOnMapServerMoveAuth(BYTE Value);
		void ReconnectOnConnectAccount(BYTE Value);
		void ReconnectOnCloseClient(BYTE Value);
		void ReconnectOnCharacterList();
		void ReconnectOnCharacterInfo();
		void ReconnectViewportDestroy();
		BOOL ReconnectCreateConnection(char* address, WORD port);

		void ReconnectGetAccountInfo(char* szID, char* szPass);

	};
}