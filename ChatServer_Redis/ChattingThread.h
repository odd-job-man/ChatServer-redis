#pragma once
#include <windows.h>
#include "SerialContent.h"
#include "Player.h"
#include "CLinkedList.h"
class ChattingThread : public SerialContent 
{
public:
	ChattingThread(const DWORD tickPerFrame, const HANDLE hCompletionPort, const LONG pqcsLimit, GameServer* pGameServer);
	virtual void OnEnter(void* pPlayer) override;
	virtual void OnLeave(void* pPlayer) override;
	virtual void OnRecv(Packet* pPacket, void* pPlayer) override;
	virtual void ProcessEachPlayer() override;
	void CS_CHAT_REQ_SECTOR_MOVE(INT64 accountNo, WORD sectorX, WORD sectorY, Player* pPlayer);
	void CS_CHAT_REQ_MESSAGE(INT64 accountNo, WORD messageLen, WCHAR* pMessage, Player* pPlayer);
private:
	CLinkedList playerList;
};
