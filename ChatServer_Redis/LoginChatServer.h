#pragma once
#include "GameServer.h"
#include "CMClient.h"
#include "HMonitor.h"

class LoginChatServer : public GameServer
{
public:
	LoginChatServer();
	void Start();

	virtual BOOL OnConnectionRequest() override;
	virtual void* OnAccept(void* pPlayer) override;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) override;
	virtual void OnPost(void* order) override;


	// Monitorable override
	virtual void OnMonitor() override; 
	const LONG TICK_PER_FRAME_;
	const ULONGLONG SESSION_TIMEOUT_;
	alignas(64) LONG UPDATE_CNT_TPS = 0;
	alignas(64) ULONGLONG UPDATE_CNT_TOTAL = 0;
	alignas(64) ULONGLONG RECV_TOTAL = 0;
	static inline HMonitor monitor;
	CMClient* pLanClient_;
};