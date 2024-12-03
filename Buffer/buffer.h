#pragma once

#include <vector>
#include <string>

struct PacketHeader
{
	uint32_t packetSize;
	uint32_t messageType;
};

struct ChatMessage
{
	PacketHeader header;
	uint32_t messageLength;
	std::string message;
};

enum
{
	AUTH,
	REGI,
	CHAT
};

class Buffer
{
public:
	std::vector<uint8_t> m_BufferData;
	int m_WriteIndex;
	int m_ReadIndex;

	Buffer(int size);

	~Buffer();

	void WriteUInt32LE(uint32_t value);
	uint32_t ReadUInt32LE();

	void WriteUInt16LE(uint16_t value);
	uint16_t ReadUInt16LE();

	void WriteString(const std::string& str);
	std::string ReadString(uint32_t length);
};