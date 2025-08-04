//-*-c++-*-
// Copyright (C) 2002-2008 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h
#pragma once
#ifndef __C_PACK_RES_READER_H_INCLUDED__
#define __C_PACK_RES_READER_H_INCLUDED__

//#define PTW32_STATIC_LIB
#include <pthread.h>
#include "IReadResFile.h"
#include "ResReferenceCounted.h"
#include "CZipResReader.h"


// set if the file is encrypted
//const int16 Pack_FILE_ENCRYPTED =		0x0001;
// the fields crc-32, compressed size and uncompressed size are set to
// zero in the local header
//const int16 Pack_INFO_IN_DATA_DESCRITOR =	;
//#define Pack_RES_INFO_IN_DATA_DESCRITOR 0x0008

struct SPackResFileEntry
{
	U32	 filePathHash;
	int32 fileDataPosition; // position of compressed data in file
	const char* fileName;   // Pointer to fileName 
	int16 localheaderSize;	// when it is equal to zero, then need read
	int16 compressionMethod;
	U32 uncompressedSize;
	U32 compressedSize;

	SPackResFileEntry() 
		: localheaderSize(0)
	{
	}

	bool operator < (const SPackResFileEntry& other) const
	{
		return filePathHash < other.filePathHash;
	}

	bool operator == (const SPackResFileEntry& other) const
	{
		return filePathHash == other.filePathHash;
	}
};

/*!
	Pack file Reader written Dec 2002 by Baibo.Zhang.
	a custom pack reader to reduce memory cost when containing very many files in a pack:
	for example, the file unit's mem manage cost is reduced from hundreads bytes to only 12 bytes (the size of SPackResFileEntry);
	The file unit is compressed by zip generally.
*/
class CPackResReader : public IResReferenceCounted
{
public:
	CPackResReader(const char* filename, bool ignoreCase, bool ignorePaths);

	CPackResReader(IReadResFile* file, bool ignoreCase, bool ignorePaths);

	virtual ~CPackResReader();

	//! opens a file by file name
	virtual IReadResFile* openFile(const char* filename);

	//! opens a file by index
	IReadResFile* openFile(int32 index);

	//! returns count of files in archive
	int32 getFileCount();

	//! returns data of file
	const SPackResFileEntry* getFileInfo(int32 index) const;

	//! returns fileindex
	int32 findFile(const char* filename);

	//! returns the name of the Pack file archive
	const char* getPackFileName() const
	{
		return m_file ? m_file->getFileName() : NULL;
	}

	static bool isValid(IReadResFile* file);

	static bool isValid(const char* filename);
private:
		
	//! scans for a local header, returns false if there is no more
	//! local file header.
	void scanFileHeader();
	IReadResFile* m_file;

protected:

	//! splits filename from Pack file into useful filenames and paths
	void extractFilename(SPackResFileEntry* entry);

	//! deletes the path from a filename
	void deletePathFromFilename(std::string& filename);


//	bool IgnoreCase;  //always ignore case
//	bool IgnorePaths;
	pthread_mutex_t mutex;
	int m_fileNb;
	//int m_filePosOffset;
	std::vector<SPackResFileEntry> m_fileList;
	char * m_fileNameBuffer;		//All file Name
};


#endif

