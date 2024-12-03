#include "buffer.h"

Buffer::Buffer(int size = 512)
{
	m_BufferData.resize(size);
	m_WriteIndex = 0;
	m_ReadIndex = 0;
}

Buffer::~Buffer() {}

void Buffer::WriteUInt32LE(uint32_t value)
{
	m_BufferData[m_WriteIndex++] = value;
	m_BufferData[m_WriteIndex++] = value >> 8;
	m_BufferData[m_WriteIndex++] = value >> 16;
	m_BufferData[m_WriteIndex++] = value >> 24;
}

uint32_t Buffer::ReadUInt32LE()
{
	uint32_t value = 0;

	value |= m_BufferData[m_ReadIndex++];
	value |= m_BufferData[m_ReadIndex++] << 8;
	value |= m_BufferData[m_ReadIndex++] << 16;
	value |= m_BufferData[m_ReadIndex++] << 24;

	return value;
}

void Buffer::WriteUInt16LE(uint16_t value)
{
	m_BufferData[m_WriteIndex++] = value;
	m_BufferData[m_WriteIndex++] = value >> 8;
}

uint16_t Buffer::ReadUInt16LE()
{
	uint16_t value = 0;

	value |= m_BufferData[m_ReadIndex++];
	value |= m_BufferData[m_ReadIndex++] << 8;

	return value;
}

void Buffer::WriteString(const std::string& str)
{
	int strLength = str.length();
	for (int i = 0; i < strLength; i++)
	{
		m_BufferData[m_WriteIndex++] = str[i];
	}
}

std::string Buffer::ReadString(uint32_t length)
{
	std::string str;
	for (int i = 0; i < length; i++)
	{
		str.push_back(m_BufferData[m_ReadIndex++]);
	}
	return str;
}