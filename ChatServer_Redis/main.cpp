#include <winsock2.h>
#include "LoginChatServer.h"
#include <cpp_redis/cpp_redis>
#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")
#pragma comment (lib, "ws2_32.lib")
LoginChatServer g_ChatServer;

int main()
{
	g_ChatServer.Start();
	g_ChatServer.WaitUntilShutDown();
}