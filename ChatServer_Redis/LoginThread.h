#pragma once
#include <cpp_redis\cpp_redis>
#include "GameServer.h"
#include "ParallelContent.h"
#include "CommonProtocol.h"

class LoginThread : public ParallelContent
{
public:
	LoginThread(GameServer* pGameServer);
	// ContentsBase overridng 
	virtual void OnEnter(void* pPlayer) override;
	virtual void OnLeave(void* pPlayer) override;
	virtual void OnRecv(Packet* pPacket, void* pPlayer) override;
	void CS_CHAT_REQ_LOGIN(void* pPlayer, Packet* pPacket);
};
