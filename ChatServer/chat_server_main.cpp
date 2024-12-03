#pragma once

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>
#include <string>
#include "buffer.h"
#include "serializer.h"

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "8412"

int main(int arg, char** argv)
{
	// Initiliaze Winsock
	WSADATA wsaData;
	int result;

	// Set version 2.2 with MAKEWORD(2,2)
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		printf("WSAStartup failed with error %d\n", result);
		return 1;
	}

	printf("WSAStartup successfully!\n");

	struct addrinfo* info = nullptr;
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));  // ensure we dont have garbage data
	hints.ai_family = AF_INET;			// IPv4
	hints.ai_socktype = SOCK_STREAM;	// Stream
	hints.ai_protocol = IPPROTO_TCP;	// TCP
	hints.ai_flags = AI_PASSIVE;

	result = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &info);
	if (result != 0)
	{
		printf("getaddrinfo failed with error %d\n", result);
		WSACleanup();
		return 1;
	}

	printf("getaddrinfo was successful!\n");

	// Create the server socket (auth server)
	SOCKET serverSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (serverSocket == INVALID_SOCKET)
	{
		printf("socket failed with error %d\n", WSAGetLastError());
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("socket created successfully!\n");

	// Connect to auth server
	result = connect(serverSocket, info->ai_addr, (int)info->ai_addrlen);
	if (result == INVALID_SOCKET)
	{
		printf("connect failed with error %d\n", WSAGetLastError());
		closesocket(serverSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("Connected to the server successfully!\n\n");



	// Create the socket
	SOCKET listenSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (listenSocket == INVALID_SOCKET)
	{
		printf("socket failed with error %d\n", WSAGetLastError());
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("socket created successfully!\n");

	result = bind(listenSocket, info->ai_addr, (int)info->ai_addrlen);
	if (result == SOCKET_ERROR)
	{
		printf("bind failed with error %d\n", result);
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("bind was successful!\n");

	// listen
	result = listen(listenSocket, SOMAXCONN);
	if (result == SOCKET_ERROR)
	{
		printf("listen failed with error %d\n", result);
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("listen was successful!\n");


	std::vector<SOCKET> activeConnections;

	FD_SET socketsReadyForReading;		// list of all the clients ready to ready
	FD_ZERO(&socketsReadyForReading);

	// use a timeval to prevent select from waiting forever
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while (true)
	{
		FD_ZERO(&socketsReadyForReading);

		FD_SET(serverSocket, &socketsReadyForReading);

		int count = select(0, &socketsReadyForReading, NULL, NULL, &tv);

		if (count == 0)
		{
			// Timevalue expired
		//	continue;
		}
		if (count == SOCKET_ERROR)
		{
			printf("select had an error %d\n", WSAGetLastError());
			continue;
		}

		// Recieve from auth server
		if (FD_ISSET(serverSocket, &socketsReadyForReading))
		{
			const int bufSize = 512;
			Buffer buffer(bufSize);
			int result = recv(serverSocket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);

			if (result > 0)
			{
				uint32_t packetSize = buffer.ReadUInt32LE();
				uint16_t messageType = buffer.ReadUInt16LE();

				if (messageType == AUTH || messageType == REGI)
				{
					// handle the message
					uint32_t messageLength = buffer.ReadUInt32LE();
					std::string msg = buffer.ReadString(messageLength);

					std::cout << msg << std::endl;

					// Only send to other clients?
					for (int j = 0; j < activeConnections.size(); j++)
					{
						SOCKET outSocket = activeConnections[j];

						if (messageType == AUTH || messageType == REGI)
						{
							send(outSocket, (const char*)(&buffer.m_BufferData[0]), packetSize, 0);
						}
					}
				}
			}
		}

		FD_ZERO(&socketsReadyForReading);

		FD_SET(listenSocket, &socketsReadyForReading);

		for (int i = 0; i < activeConnections.size(); i++)
		{
			FD_SET(activeConnections[i], &socketsReadyForReading);
		}

		count = select(0, &socketsReadyForReading, NULL, NULL, &tv);

		if (count == 0)
		{
			// Timevalue expired
			continue;
		}
		if (count == SOCKET_ERROR)
		{
			printf("select had an error %d\n", WSAGetLastError());
			continue;
		}

		// Receive from chat servers

		for (int i = 0; i < activeConnections.size(); i++)
		{
			SOCKET socket = activeConnections[i];

			if (FD_ISSET(socket, &socketsReadyForReading))
			{
				const int bufSize = 512;
				Buffer buffer(bufSize);

				int result = recv(socket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);

				if (result == SOCKET_ERROR)
				{
					printf("recv failed with error %d\n", WSAGetLastError());
					closesocket(socket);
					activeConnections.erase(activeConnections.begin() + i);
					i--;
					continue;
				}
				else if (result == 0)
				{
					printf("Client disconnect\n");
					closesocket(socket);
					activeConnections.erase(activeConnections.begin() + i);
					i--;
					continue;
				}

				uint32_t packetSize = buffer.ReadUInt32LE();
				uint16_t messageType = buffer.ReadUInt16LE();

				uint32_t messageLength = buffer.ReadUInt32LE();
				std::string msg = buffer.ReadString(messageLength);

				if (messageType == AUTH || messageType == REGI)
				{
					User user = user.deserializeUserFromBinary(msg);
					printf("PacketSize:%d\nMessageType:%d\nMessageLength:%d\nMessage:%s\n\n", packetSize, messageType, messageLength, user.username.c_str());
					send(serverSocket, (const char*)(&buffer.m_BufferData[0]), packetSize, 0);
				}
				else
				{
					// Only send to other clients?
					for (int j = 0; j < activeConnections.size(); j++)
					{
						SOCKET outSocket = activeConnections[j];

						if (outSocket != listenSocket && outSocket != socket)
						{
							printf("PacketSize:%d\nMessageType:%d\nMessageLength:%d\nMessage:%s\n\n", packetSize, messageType, messageLength, msg.c_str());
							send(outSocket, (const char*)(&buffer.m_BufferData[0]), packetSize, 0);
						}
					}
				}


				FD_CLR(socket, &socketsReadyForReading);
				count--;
			}
		}

		if (count > 0)
		{
			if (FD_ISSET(listenSocket, &socketsReadyForReading))
			{
				SOCKET newConnection = accept(listenSocket, NULL, NULL);
				if (newConnection == INVALID_SOCKET)
				{
					printf("accept failed with error %d\n", WSAGetLastError());
				}
				else
				{
					activeConnections.push_back(newConnection);
					FD_SET(newConnection, &activeConnections);
					FD_CLR(listenSocket, &socketsReadyForReading);

					printf("Client connect with socket: %d\n", (int)newConnection);
				}
			}
		}
	}

	freeaddrinfo(info);
	closesocket(listenSocket);

	for (int i = 0; i < activeConnections.size(); i++)
	{
		closesocket(activeConnections[i]);
	}

	WSACleanup();

	return 0;
}