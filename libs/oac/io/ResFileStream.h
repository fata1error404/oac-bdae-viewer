#ifndef _RES_FILE_STREAM_H_
#define _RES_FILE_STREAM_H_

#include "TypeDef.h"

class IReadResFile;
class CResFileStream
{
public:
	// virtual int tell();

	////////////////////////////////////////////////////////////
	inline int8 ReadS8()
	{
		int8 val = 0; // Return 0 if read failed
		read((U8 *)&val, sizeof(int8));
		return val;
	}

	inline int16 ReadS16()
	{
		int16 val = 0;
		read((U8 *)&val, sizeof(int16));
		return val;
	}

	inline int32 ReadS32()
	{
		int32 val = 0;
		read((U8 *)&val, sizeof(int32));
		return val;
	}

	inline uint8 ReadU8()
	{
		uint8 val = 0;
		read((U8 *)&val, sizeof(uint8));
		return val;
	}

	inline uint16 ReadU16()
	{
		uint16 val = 0;
		read((U8 *)&val, sizeof(uint16));
		return val;
	}

	inline uint32 ReadU32()
	{
		uint32 val = 0;
		read((U8 *)&val, sizeof(uint32));
		return val;
	}

	inline float Readfloat()
	{
		float val = 0;
		read((U8 *)&val, sizeof(float));
		return val;
	}

	inline std::string ReadStr()
	{
		int stringLen = ReadS16();
		char *stringBuf = new char[stringLen + 1];
		read((U8 *)stringBuf, stringLen);
		stringBuf[stringLen] = '\0';
		std::string result = std::string(stringBuf);
		delete[] stringBuf;
		stringBuf = NULL;
		return result;
	}

	inline std::wstring ReadWStr()
	{
		int stringLen = ReadS16();
		wchar_t *stringBuf = new wchar_t[stringLen + 1];
		for (int i = 0; i < stringLen; i++)
		{
			int symbol = ReadS16();
			stringBuf[i] = symbol;
		}
		stringBuf[stringLen] = L'\0';
		std::wstring result = std::wstring(stringBuf);
		delete[] stringBuf;
		stringBuf = NULL;
		return result;
	}

	inline void WriteS8(int8 val)
	{
		write((U8 *)&val, sizeof(int8));
	}

	inline void WriteU8(uint8 val)
	{
		write((U8 *)&val, sizeof(uint8));
	}

	inline void WriteS16(int16 val)
	{
		write((U8 *)&val, sizeof(int16));
	}

	inline void WriteU16(uint16 val)
	{
		write((U8 *)&val, sizeof(uint16));
	}

	inline void WriteS32(int32 val)
	{
		write((U8 *)&val, sizeof(int32));
	}

	inline void WriteU32(uint32 val)
	{
		write((U8 *)&val, sizeof(uint32));
	}

	inline void Writefloat(float val)
	{
		write((U8 *)&val, sizeof(float));
	}

	inline void WriteStr(std::string str)
	{
		int count = str.size();
		WriteS16(count);
		for (int i = 0; i < count; i++)
		{
			int8 val = str[i];
			WriteS8(val);
		}
	}

	inline void WriteWStr(std::wstring str)
	{
		int count = str.size();
		WriteS16(count);
		for (int i = 0; i < count; i++)
		{
			int16 val = str[i];
			WriteS16(val);
		}
	}

	inline void skip(int size)
	{
		if (size <= 0)
		{
			return;
		}
		seek(tell() + size);
	}

	inline void AllocateAndReadStr(char *&buf)
	{
		int strLen = ReadS16();
		buf = new char[strLen + 1];
		read((U8 *)buf, strLen);
		buf[strLen] = 0;
	}

public:
	bool m_readError;

public:
	CResFileStream(IReadResFile *pFile = NULL);
	~CResFileStream();

public:
	void setInputFileObject(IReadResFile *pFile);
	// void setOutputFileObject(io::IWriteFile *pFile);
public:
	void seek(int offset);
	int tell() const;

	int size() const;

	bool canRead() const;
	bool canWrite() const;

	int read(U8 *data, int len);
	int write(const U8 *data, int len);

private:
	IReadResFile *m_pReadFile;
	// io::IWriteFile *m_pWriteFile;
};

#endif //_RES_FILE_STREAM_H_