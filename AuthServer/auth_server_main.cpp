#pragma once

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <vector>
#include <string>
#include "buffer.h"
#include "serializer.h"

#include <iostream>

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>

#include <bcrypt/BCrypt.hpp>

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "8412"

bool registerUser(sql::Connection* con, const std::string& username, const std::string& password)
{
	// Check if username is taken
	sql::PreparedStatement* prepstmt = con->prepareStatement(
		"SELECT * FROM users WHERE username = ?"
	);

	prepstmt->setString(1, username);
	sql::ResultSet* res = prepstmt->executeQuery();

	if (res->next())
	{
		// Username taken
		std::cout << "Username already in use" << std::endl;

		return false;
	}

	int id = 0;

	// Get unique id
	prepstmt = con->prepareStatement(
		"SELECT * FROM users WHERE id = ?"
	);

	prepstmt->setInt(1, id);
	res = prepstmt->executeQuery();

	while (res->first())
	{
		id++;

		prepstmt = con->prepareStatement(
			"SELECT * FROM users WHERE id = ?"
		);

		prepstmt->setInt(1, id);
		res = prepstmt->executeQuery();
	}

	printf("Unique ID: %d\n", id);

	time_t timestamp;

	try
	{
		std::string passwordHash = BCrypt::generateHash(password);

		sql::PreparedStatement* prepstmt = con->prepareStatement(
			"INSERT INTO users (id, username, hashed_password, last_login) VALUES (?, ?, ?, CURRENT_TIMESTAMP)"
		);
		prepstmt->setInt(1, id);
		prepstmt->setString(2, username);
		prepstmt->setString(3, passwordHash);
		prepstmt->executeUpdate();

		std::cout << "User registered successfully" << std::endl;
		return true;
	}
	catch (sql::SQLException& e)
	{
		std::cerr << "MySQL Error: " << e.what() << std::endl;
	}
}

int verifyUser(sql::Connection* con, const std::string& username, const std::string& password)
{
	sql::PreparedStatement* prepstmt = con->prepareStatement(
		"SELECT hashed_password FROM users WHERE username = ?"
	);

	prepstmt->setString(1, username);
	sql::ResultSet* res = prepstmt->executeQuery();
	try
	{
		if (res->next())
		{
			std::string storedHash = res->getString("hashed_password");

			if (BCrypt::validatePassword(password, storedHash))
			{
				std::cout << "Authentification successful!" << std::endl;

				// Update login time
				sql::PreparedStatement* prepstmt = con->prepareStatement(
					"UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE username = ?"
				);

				prepstmt->setString(1, username);
				sql::ResultSet* res = prepstmt->executeQuery();

				return 1;
			}
			else
			{
				std::cout << "Authentification failed, incorrect password" << std::endl;
				return 2;
			}
		}
		else
		{
			std::cout << "User not found" << std::endl;
			return 0;
		}
	}
	catch (sql::SQLException& e)
	{
		std::cerr << "MySQL Error: " << e.what() << std::endl;
	}
}

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

	result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &info);
	if (result != 0)
	{
		printf("getaddrinfo failed with error %d\n", result);
		WSACleanup();
		return 1;
	}

	printf("getaddrinfo was successful!\n");

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

	// SQL Stuff
	sql::mysql::MySQL_Driver* driver = nullptr;
	sql::Connection* con = nullptr;

	try
	{
		driver = sql::mysql::get_mysql_driver_instance();

		con = driver->connect("tcp://127.0.0.1:3306", "user", "user");

		if (con->isValid())
		{
			std::cout << "Connected to MySQL!!" << std::endl;
		}

		con->setSchema("serverdb_wthompson");
	}
	catch (sql::SQLException& e)
	{
		std::cerr << "MySQL Error: " << e.what() << std::endl;
	}

	while (true)
	{
		FD_ZERO(&socketsReadyForReading);

		FD_SET(listenSocket, &socketsReadyForReading);

		for (int i = 0; i < activeConnections.size(); i++)
		{
			FD_SET(activeConnections[i], &socketsReadyForReading);
		}

		int count = select(0, &socketsReadyForReading, NULL, NULL, &tv);

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

				if (messageType == AUTH)
				{
					User user = user.deserializeUserFromBinary(msg);

					SOCKET outSocket = activeConnections[0];

					ChatMessage verifyMessage;

					int verifyResult = verifyUser(con, user.username, user.password);

					if (verifyResult == 1)
					{
						// Success Message
						verifyMessage.message = "success";
						verifyMessage.messageLength = verifyMessage.message.length();
						verifyMessage.header.messageType = 0;
						verifyMessage.header.packetSize = verifyMessage.messageLength + sizeof(verifyMessage.messageLength) + sizeof(verifyMessage.header.messageType) + sizeof(verifyMessage.header.packetSize);
					}
					else if (verifyResult == 0)
					{
						// Fail message
						verifyMessage.message = "fail";
						verifyMessage.messageLength = verifyMessage.message.length();
						verifyMessage.header.messageType = 0;
						verifyMessage.header.packetSize = verifyMessage.messageLength + sizeof(verifyMessage.messageLength) + sizeof(verifyMessage.header.messageType) + sizeof(verifyMessage.header.packetSize);
					}
					else if (verifyResult == 2)
					{
						// Fail message
						verifyMessage.message = "pass";
						verifyMessage.messageLength = verifyMessage.message.length();
						verifyMessage.header.messageType = 0;
						verifyMessage.header.packetSize = verifyMessage.messageLength + sizeof(verifyMessage.messageLength) + sizeof(verifyMessage.header.messageType) + sizeof(verifyMessage.header.packetSize);
					}

					const int bufSize = 512;
					Buffer buffer(bufSize);

					buffer.WriteUInt32LE(verifyMessage.header.packetSize);
					buffer.WriteUInt16LE(verifyMessage.header.messageType);
					buffer.WriteUInt32LE(verifyMessage.messageLength);
					buffer.WriteString(verifyMessage.message);

					//if (outSocket != listenSocket && outSocket != socket)
					{
						send(outSocket, (const char*)(&buffer.m_BufferData[0]), verifyMessage.header.packetSize, 0);
					}

					printf("PacketSize:%d\nMessageType:%d\nMessageLength:%d\nMessage:%s\n\n", packetSize, messageType, messageLength, verifyMessage.message.c_str());
				}
				else if (messageType == REGI)
				{
					User user = user.deserializeUserFromBinary(msg);

					SOCKET outSocket = activeConnections[0];

					ChatMessage verifyMessage;

					if (registerUser(con, user.username, user.password))
					{
						// Success Message
						verifyMessage.message = "success";
						verifyMessage.messageLength = verifyMessage.message.length();
						verifyMessage.header.messageType = 1;
						verifyMessage.header.packetSize = verifyMessage.messageLength + sizeof(verifyMessage.messageLength) + sizeof(verifyMessage.header.messageType) + sizeof(verifyMessage.header.packetSize);
					}
					else
					{
						// Fail message
						verifyMessage.message = "fail";
						verifyMessage.messageLength = verifyMessage.message.length();
						verifyMessage.header.messageType = 1;
						verifyMessage.header.packetSize = verifyMessage.messageLength + sizeof(verifyMessage.messageLength) + sizeof(verifyMessage.header.messageType) + sizeof(verifyMessage.header.packetSize);
					}

					const int bufSize = 512;
					Buffer buffer(bufSize);

					buffer.WriteUInt32LE(verifyMessage.header.packetSize);
					buffer.WriteUInt16LE(verifyMessage.header.messageType);
					buffer.WriteUInt32LE(verifyMessage.messageLength);
					buffer.WriteString(verifyMessage.message);

					//if (outSocket != listenSocket && outSocket != socket)
					{
						send(outSocket, (const char*)(&buffer.m_BufferData[0]), verifyMessage.header.packetSize, 0);
					}

					printf("PacketSize:%d\nMessageType:%d\nMessageLength:%d\nMessage:%s\n\n", packetSize, messageType, messageLength, verifyMessage.message.c_str());
				}
				else
				{
					//printf("PacketSize:%d\nMessageType:%d\nMessageLength:%d\nMessage:%s\n\n", packetSize, messageType, messageLength, msg.c_str());
				}

				for (int j = 0; j < activeConnections.size(); j++)
				{
					SOCKET outSocket = activeConnections[j];

					if (outSocket != listenSocket && outSocket != socket)
					{
						send(outSocket, (const char*)(&buffer.m_BufferData[0]), packetSize, 0);
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