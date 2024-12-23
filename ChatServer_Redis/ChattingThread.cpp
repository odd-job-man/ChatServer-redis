#include "Winsock2.h"
#include "SCCContents.h"
#include "GameServer.h"
#include "ChattingThread.h"
#include "en_ChatContentsType.h"
#include "Sector.h"
#include "LoginChatServer.h"
#include "Packet.h"
#include "CommonProtocol.h"

ChattingThread::ChattingThread(const DWORD tickPerFrame, const HANDLE hCompletionPort, const LONG pqcsLimit, GameServer* pGameServer)
	:SerialContent{ tickPerFrame,hCompletionPort,pqcsLimit,pGameServer }, playerList{ offsetof(Player,contentLink) }
{
}

void ChattingThread::OnEnter(void* pPlayer)
{
	playerList.push_back(pPlayer);
	Player* pAuthPlayer = (Player*)pPlayer;
	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_LOGIN(1, pAuthPlayer->accountNo_, sp);
	pGameServer_->SendPacket(pAuthPlayer->sessionId_, sp);;
	InterlockedIncrement(&pGameServer_->lPlayerNum_);
}

void ChattingThread::OnLeave(void* pPlayer)
{
	playerList.remove(pPlayer);
	// 이미 RELEASE 된 플레이어에 대해서 다시한번 RELEASE JOB이 도착한것임
	Player* pLeavePlayer = (Player*)pPlayer;

	// 세션만 생성되고 로그인 안하다가 자의로 끊거나 타임아웃된경우
	if (pLeavePlayer->bLogin_ == false)
		return;
	else
		pLeavePlayer->bLogin_ = false;

	// 로그인햇다면 이미 이동 메시지를 보내고 등록된 좌표에 해당하는 섹터에 등록햇을것이므로 삭제한다.
	if (pLeavePlayer->bRegisterAtSector_ == true)
		RemoveClientAtSector(pLeavePlayer->sectorX_, pLeavePlayer->sectorY_, pLeavePlayer);

	InterlockedDecrement(&pGameServer_->lPlayerNum_);
}

void ChattingThread::OnRecv(Packet* pPacket, void* pPlayer)
{
	Player* pRecvPlayer = (Player*)pPlayer;
	WORD Type;
	try
	{
		(*pPacket) >> Type;
		switch ((en_PACKET_TYPE)Type)
		{
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		{
			INT64 accountNo;
			WORD sectorX;
			WORD sectorY;
			(*pPacket) >> accountNo >> sectorX >> sectorY;
			if(pPacket->IsBufferEmpty() == false)
			{
				pGameServer_->Disconnect(pRecvPlayer->sessionId_);
				return;
			}
			CS_CHAT_REQ_SECTOR_MOVE(accountNo, sectorX, sectorY, pRecvPlayer);
			break;
		}
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
		{
			INT64 accountNo;
			WORD messageLen;
			(*pPacket) >> accountNo >> messageLen;
			WCHAR* pMessage = (WCHAR*)pPacket->GetPointer(messageLen);
			if (pMessage == nullptr || pPacket->IsBufferEmpty() == false)
			{
				pGameServer_->Disconnect(pRecvPlayer->sessionId_);
				return;
			}
			CS_CHAT_REQ_MESSAGE(accountNo, messageLen, pMessage, pRecvPlayer);
			break;
		}
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			break;
		case en_PACKET_MONITOR_CLIENT_LOGIN: // 나중에 다시구현
			break;
		default:
			pGameServer_->Disconnect(pGameServer_->GetSessionID(pPlayer));
			break;
		}
	}
	catch (int errCode)
	{
		if (errCode == ERR_PACKET_EXTRACT_FAIL)
		{
			pGameServer_->Disconnect(pGameServer_->GetSessionID(pPlayer));
		}
		else if (errCode == ERR_PACKET_RESIZE_FAIL)
		{
			// 지금은 이게 실패할리가 없음 사실 모니터링클라 접속안하면 리사이즈조차 아예 안일어날수도잇음, 원래는 Disconnect가 맞음
			LOG(L"RESIZE", ERR, TEXTFILE, L"Resize Fail ShutDown Server");
			//pGameServer_->Disconnect(pGameServer_->GetSessionID(pPlayer));
			__debugbreak();
		}
	}
	InterlockedIncrement(&static_cast<LoginChatServer*>(pGameServer_)->UPDATE_CNT_TPS);
}

void ChattingThread::ProcessEachPlayer()
{
}

void ChattingThread::CS_CHAT_REQ_SECTOR_MOVE(INT64 accountNo, WORD sectorX, WORD sectorY, Player* pPlayer)
{
	if (pPlayer->bLogin_ == false)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	if (pPlayer->accountNo_ != accountNo)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	// 클라가 유효하지 않은 좌표로 요청햇다면 끊어버린다
	if (IsNonValidSector(sectorX, sectorY))
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	if (pPlayer->bRegisterAtSector_ == false)
		pPlayer->bRegisterAtSector_ = true;
	else
		RemoveClientAtSector(pPlayer->sectorX_, pPlayer->sectorY_, pPlayer);


	pPlayer->sectorX_ = sectorX;
	pPlayer->sectorY_ = sectorY;
	RegisterClientAtSector(sectorX, sectorY, pPlayer);

	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_SECTOR_MOVE(accountNo, sectorX, sectorY, sp);
	pGameServer_->SendPacket(pPlayer->sessionId_, sp);
}

void ChattingThread::CS_CHAT_REQ_MESSAGE(INT64 accountNo, WORD messageLen, WCHAR* pMessage, Player* pPlayer)
{
	if (pPlayer->bLogin_ == false)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	// 디버깅용
	if (pPlayer->accountNo_ != accountNo)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_MESSAGE(accountNo, pPlayer->ID_, pPlayer->nickName_, messageLen, pMessage, sp);
	SECTOR_AROUND sectorAround;
	GetSectorAround(pPlayer->sectorX_, pPlayer->sectorY_, &sectorAround);
	SendPacket_AROUND(&sectorAround, sp);
}
