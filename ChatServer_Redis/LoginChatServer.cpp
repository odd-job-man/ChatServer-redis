#include <Winsock2.h>
#include "LoginChatServer.h"
#include "Parser.h"
#include "Player.h"
#include "en_ChatContentsType.h"
#include "Parser.h"
#include "LoginThread.h"
#include "ChattingThread.h"
#include "ServerNum.h"
#include "Timer.h"
#include "Monitorable.h"
#include "UpdateBase.h"
#include "CommonProtocol.h"
#include "Packet.h"
#include "CMClient.h"
#include "CTlsObjectPool.h"
#include <time.h>

template<typename T>
__forceinline T& IGNORE_CONST(const T& value)
{
	return const_cast<T&>(value);
}

LoginChatServer::LoginChatServer()
	:GameServer{ L"LoginChatConfig.txt" },TICK_PER_FRAME_{0},SESSION_TIMEOUT_{0}
{
}

void LoginChatServer::Start()
{
	SetEntirePlayerMemory(sizeof(Player));
	Player::MAX_PLAYER_NUM = maxPlayer_;
	char* pStart;

	PARSER psr = CreateParser(L"LoginChatConfig.txt");
	GetValue(psr, L"TICK_PER_FRAME", (PVOID*)&pStart, nullptr);
	IGNORE_CONST(TICK_PER_FRAME_) = _wtoi((LPCWSTR)pStart);

	GetValue(psr, L"SESSION_TIMEOUT", (PVOID*)&pStart, nullptr);
	IGNORE_CONST(SESSION_TIMEOUT_) = (ULONGLONG)_wtoi64((LPCWSTR)pStart);
	ReleaseParser(psr);

	LoginThread* pLogin = new LoginThread{ this };
	ContentsBase::RegisterContents((int)en_ChatContentsType::LOGIN,static_cast<ContentsBase*>(pLogin));
	ContentsBase::SetContentsToFirst((int)en_ChatContentsType::LOGIN);

	ChattingThread* pChatting = new ChattingThread{ 40,hcp_,5,this };
	ContentsBase::RegisterContents((int)en_ChatContentsType::CHAT, static_cast<ContentsBase*>(pChatting));

	MonitoringUpdate* pMonitor = new MonitoringUpdate{ hcp_,1000,5 };
	pMonitor->RegisterMonitor(static_cast<const Monitorable*>(this));

	Timer::Reigster_UPDATE(pChatting);
	Timer::Reigster_UPDATE(pMonitor);

	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
		ResumeThread(hIOCPWorkerThreadArr_[i]);

	ResumeThread(hAcceptThread_);

	pLanClient_ = new CMClient{ L"LoginChatLanClientConfig.txt", SERVERNUM::CHAT };
	pLanClient_->Start();


	Timer::Start();
}

BOOL LoginChatServer::OnConnectionRequest()
{
	return TRUE;
}

void* LoginChatServer::OnAccept(void* pPlayer)
{
	ContentsBase::FirstEnter(pPlayer);
	return nullptr;
}

void LoginChatServer::OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket)
{
}

void LoginChatServer::OnPost(void* order)
{
}

void LoginChatServer::OnMonitor()
{
	FILETIME ftCreationTime, ftExitTime, ftKernelTime, ftUsertTime;
	FILETIME ftCurTime;
	GetProcessTimes(GetCurrentProcess(), &ftCreationTime, &ftExitTime, &ftKernelTime, &ftUsertTime);
	GetSystemTimeAsFileTime(&ftCurTime);

	ULARGE_INTEGER start, now;
	start.LowPart = ftCreationTime.dwLowDateTime;
	start.HighPart = ftCreationTime.dwHighDateTime;
	now.LowPart = ftCurTime.dwLowDateTime;
	now.HighPart = ftCurTime.dwHighDateTime;


	ULONGLONG ullElapsedSecond = (now.QuadPart - start.QuadPart) / 10000 / 1000;

	ULONGLONG temp = ullElapsedSecond;

	ULONGLONG ullElapsedMin = ullElapsedSecond / 60;
	ullElapsedSecond %= 60;

	ULONGLONG ullElapsedHour = ullElapsedMin / 60;
	ullElapsedMin %= 60;

	ULONGLONG ullElapsedDay = ullElapsedHour / 24;
	ullElapsedHour %= 24;

	monitor.UpdateCpuTime(nullptr, nullptr);
	monitor.UpdateQueryData();

	ULONGLONG acceptTPS = InterlockedExchange(&acceptCounter_, 0);
	ULONGLONG disconnectTPS = InterlockedExchange(&disconnectTPS_, 0);
	ULONGLONG recvTPS = InterlockedExchange(&recvTPS_, 0);
	LONG sendTPS = InterlockedExchange(&sendTPS_, 0);
	LONG sessionNum = InterlockedXor(&lSessionNum_, 0);
	LONG playerNum = InterlockedXor(&lPlayerNum_, 0);
	LONG UpdateTPS = InterlockedExchange(&UPDATE_CNT_TPS, 0);

	acceptTotal_ += acceptTPS;
	RECV_TOTAL += recvTPS;

	double processPrivateMByte = monitor.GetPPB() / (1024 * 1024);
	double nonPagedPoolMByte = monitor.GetNPB() / (1024 * 1024);
	double availableByte = monitor.GetAB();
	double networkSentBytes = monitor.GetNetWorkSendBytes();
	double networkRecvBytes = monitor.GetNetWorkRecvBytes();

	printf(
		"Elapsed Time : %02lluD-%02lluH-%02lluMin-%02lluSec\n"
		"MonitorServerConnected : %s\n"
		"update Count : %d\n"
		"Packet Pool Alloc Capacity : %d\n"
		"Packet Pool Alloc UseSize: %d\n"
		"SendQ Pool Capacity : %d\n"
		"Accept TPS: %llu\n"
		"Accept Total : %llu\n"
		"Disconnect TPS: %llu\n"
		"Recv Msg TPS: %llu\n"
		"Send Msg TPS: %d\n"
		"Session Num : %d\n"
		"Player Num : %d\n"
		"----------------------\n"
		"Process Private MBytes : %.2lf\n"
		"Process NP Pool KBytes : %.2lf\n"
		"Memory Available MBytes : %.2lf\n"
		"Machine NP Pool MBytes : %.2lf\n"
		"Processor CPU Time : %.2f\n"
		"Process CPU Time : %.2f\n"
		"TCP Retransmitted/sec : %.2f\n\n",
		ullElapsedDay, ullElapsedHour, ullElapsedMin, ullElapsedSecond,
		(pLanClient_->bLogin_ == TRUE) ? "True" : "False",
		UpdateTPS,
		Packet::packetPool_.capacity_ * Bucket<Packet, false>::size,
		Packet::packetPool_.AllocSize_,
		pSessionArr_[0].sendPacketQ_.nodePool_.capacity_,
		acceptTPS,
		acceptTotal_,
		disconnectTPS,
		recvTPS,
		sendTPS,
		sessionNum,
		playerNum,
		processPrivateMByte,
		monitor.GetPNPB() / 1024,
		availableByte,
		nonPagedPoolMByte,
		monitor._fProcessorTotal,
		monitor._fProcessTotal,
		monitor.GetRetranse()
	);

	if (pLanClient_->bLogin_ == FALSE)
		return;

	time_t curTime;
	time(&curTime);

#pragma warning(disable : 4244) // 프로토콜이 4바이트를 받고 상위4바이트는 버려서 별수없음
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN, (int)1, curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU, (int)monitor._fProcessTotal, curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM, (int)processPrivateMByte, curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_CHAT_SESSION, (int)sessionNum, curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_CHAT_PLAYER, (int)playerNum, curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS, (int)UpdateTPS, curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL, (int)Packet::packetPool_.AllocSize_, curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL, (int)(pSessionArr_[0].recvMsgQ_.nodePool_.AllocSize_), curTime);

	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL, (int)monitor._fProcessorTotal, curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, (int)nonPagedPoolMByte, curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, (int)(networkRecvBytes / (float)1024), curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, (int)(networkSentBytes / (float)1024), curTime);
	pLanClient_->SendToMonitoringServer(CHAT, dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, (int)availableByte, curTime);
#pragma warning(default : 4244)
}

