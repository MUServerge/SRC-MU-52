// CustomQuest.h: interface for the CCustomQuest class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Item.h"
#include "CustomQuest.h"

struct QUESTNPC_TYPE_INFO
{
	int Index;
	int MonsterClass;
	int Map;
	int X;
	int Y;
	int MaxQuest;
	int OpenNpc;
};

struct SDHP_CUSTOMNPCQUEST_SEND
{
    PSBMSG_HEAD header; // C1:F1
    WORD index;
    char name[11];
    WORD quest;
	WORD indexnpc;
};

struct SDHP_CUSTOMNPCQUEST_SAVE_SEND
{
    PSBMSG_HEAD header; // C1:F2
    WORD index;
    char name[11];
	WORD quest;
};

struct SDHP_CUSTOMNPCQUEST_RECV
{
    PSBMSG_HEAD header; // C1:F1
    WORD index;
    WORD quest;
	WORD indexnpc;
	DWORD questcount;
	DWORD monstercount;
};

struct SDHP_CUSTOMNPCQUESTMONSTERSAVE_SEND
{
    PSBMSG_HEAD header; // C1:F1
    WORD index;
    char name[11];
    WORD quest;
	DWORD monsterqtd;
};

//**********************************************//
//********** GameServer -> Cliente    **********//
//**********************************************//

enum eCustomNpcQuestHudState
{
	CUSTOMNPCQUEST_HUD_STATE_NONE   = 0, // no active quest -> client hides the HUD
	CUSTOMNPCQUEST_HUD_STATE_ACTIVE = 1, // tracking a kill objective
};

// Quest tracker HUD feed. MUST stay byte-identical with
// PMSG_CUSTOMNPCQUEST_HUD_RECV in the client (NewUIQuestInfoHUD.cpp).
struct PMSG_CUSTOMNPCQUEST_HUD_SEND
{
	PSBMSG_HEAD header; // C1:F3:EF
	BYTE state;			// eCustomNpcQuestHudState
	BYTE map;			// quest map used by the client-side current-map filter
	WORD quest;			// quest index
	WORD monsterClass;	// target monster class used by the client-side quest marker
	DWORD killed;		// monsters killed so far
	DWORD total;		// monsters required
	char monster[32];	// target monster name
};

class CCustomNpcQuest
{
public:
	CCustomNpcQuest();
	virtual ~CCustomNpcQuest();
	void Init();
	void Load(char* path);
	bool CheckNpcQuest(LPOBJ lpObj,LPOBJ lpNpc);
	bool CheckNpcOpen(int quest);
	bool CheckAll(int quest);
	bool CheckQuest(LPOBJ lpObj,int quest,int Npc, int qtd);
	bool CheckCharacter(LPOBJ lpObj,int quest,int Npc);
	bool CheckItem(LPOBJ lpObj,int quest,int Npc);
	bool CheckMonster(LPOBJ lpObj,int quest,int Npc,int Count);
	bool CheckItemRewardInventorySpace(LPOBJ lpObj,int quest,int Npc);
	bool CheckItemInventorySpace(LPOBJ lpObj,int index);
	void RemoveItem(LPOBJ lpObj,int quest);
	void RemoveMoney(LPOBJ lpObj,int quest);
	void AddRewardCoin(LPOBJ lpObj,int quest);
	void AddRewardItem(LPOBJ lpObj,int quest);
	void AddRewardBuff(LPOBJ lpObj,int quest);
	void AddRewardExperience(LPOBJ lpObj,int quest);
	void MonsterDieProc(LPOBJ lpObj,LPOBJ lpTarget);
	void DGCustomNpcQuestRecv(SDHP_CUSTOMNPCQUEST_RECV* lpMsg);
	void DGCustomNpcQuestSend(int aIndex,int Quest, int NpcIndex);
	void DGCustomNpcQuestMonsterSaveSend(int aIndex,int Quest, int Count);
	void GCCustomNpcQuestHudSend(LPOBJ lpObj);
private:
	CUSTOM_QUEST_MONSTER* FindMonsterInfo(int quest);
private:
	std::vector<QUESTNPC_TYPE_INFO>			m_CustomNpcQuestNpcTypeInfo;
	std::vector<CUSTOM_QUEST_CHARACTER>		m_CustomNpcQuestCharacterInfo;
	std::vector<CUSTOM_QUEST_ITEM>			m_CustomNpcQuestItemInfo;
	std::vector<CUSTOM_QUEST_MONSTER>		m_CustomNpcQuestMonsterInfo;
	std::vector<CUSTOM_QUEST_REWARD>		m_CustomNpcQuestRewardInfo;
	std::vector<CUSTOM_QUEST_REWARD_ITEM>	m_CustomNpcQuestRewardItemInfo;
	std::vector<CUSTOM_QUEST_REWARD_BUFF>	m_CustomNpcQuestRewardBuffInfo;
	std::vector<CUSTOM_QUEST_REWARD_EXP>	m_CustomNpcQuestRewardExpInfo;
private:
	int m_LastPosX;
	int m_LastPosY;
};

extern CCustomNpcQuest gCustomNpcQuest;
