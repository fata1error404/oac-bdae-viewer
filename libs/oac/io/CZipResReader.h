//-*-c++-*-
// Copyright (C) 2002-2008 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h
#pragma once
#ifndef __C_ZIP_RES_READER_H_INCLUDED__
#define __C_ZIP_RES_READER_H_INCLUDED__

//#define PTW32_STATIC_LIB
#include <pthread.h>
#include "IReadResFile.h"
#include "ResReferenceCounted.h"

template<class T>
inline
S32
my_binary_search(const std::vector<T>& vec, const T& element)
{
	if (vec.empty())
	{
		return -1;
	}
    typename std::vector<T>::const_iterator ite = std::lower_bound(vec.begin(), vec.end(), element);
	if (ite != vec.end() && 
		//!(*ite < element) && !(element < *ite)
		(*ite == element)
		)
	{
		return std::distance(vec.begin(), ite);
	}
	else
	{
		return -1;
	}
}

// set if the file is encrypted
//const int16 ZIP_FILE_ENCRYPTED =		0x0001;
// the fields crc-32, compressed size and uncompressed size are set to
// zero in the local header
//const int16 ZIP_INFO_IN_DATA_DESCRITOR =	;
#define ZIP_RES_INFO_IN_DATA_DESCRITOR 0x0008
#pragma pack(push,1)
#define PACK_STRUCT

struct SZIPResFileDataDescriptor
{
	int32 CRC32;
	int32 CompressedSize;
	int32 UncompressedSize;
} PACK_STRUCT;

struct SZIPResFileHeader
{
	int32 Sig;
	int16 VersionToExtract;
	int16 GeneralBitFlag;
	int16 CompressionMethod;
	int16 LastModFileTime;
	int16 LastModFileDate;
	SZIPResFileDataDescriptor DataDescriptor;
	int16 FilenameLength;
	int16 ExtraFieldLength;
} PACK_STRUCT;

// Default alignment
#pragma pack(pop)

struct SZipResFileEntry
{
	std::string zipFileName;
	std::string simpleFileName;
	std::string path;
	int32 fileDataPosition; // position of compressed data in file
	SZIPResFileHeader header;

	bool operator < (const SZipResFileEntry& other) const
	{
		for(U32 i=0; i<simpleFileName.length() && i < other.simpleFileName.length(); ++i)
		{
			int32 diff = simpleFileName[i] - other.simpleFileName[i];
			if ( diff )
				return diff < 0;
		}
		return simpleFileName.length() < other.simpleFileName.length();
	}

	bool operator == (const SZipResFileEntry& other) const
	{
		return simpleFileName.compare(other.simpleFileName) == 0;
	}
};

/*!
	Zip file Reader written April 2002 by N.Gebhardt.
	Doesn't decompress data, only reads the file and is able to
	open uncompressed entries.
*/
class CZipResReader : public IResReferenceCounted
{
public:
	CZipResReader(const char* filename, bool ignoreCase, bool ignorePaths);

	CZipResReader(IReadResFile* file, bool ignoreCase, bool ignorePaths);

	virtual ~CZipResReader();

	//! opens a file by file name
	virtual IReadResFile* openFile(const char* filename);

	//! opens a file by index
	IReadResFile* openFile(int32 index);

	//! returns count of files in archive
	int32 getFileCount();

	//! returns data of file
	const SZipResFileEntry* getFileInfo(int32 index) const;

	//! returns fileindex
	int32 findFile(const char* filename);

	//! returns the name of the zip file archive
	const char* getZipFileName() const
	{
		return File ? File->getFileName() : NULL;
	}

	static bool isValid(IReadResFile* file);

	static bool isValid(const char* filename);
private:
		
	//! scans for a local header, returns false if there is no more
	//! local file header.
	bool scanLocalHeader();
	IReadResFile* File;

protected:

	//! splits filename from zip file into useful filenames and paths
	void extractFilename(SZipResFileEntry* entry);

	//! deletes the path from a filename
	void deletePathFromFilename(std::string& filename);


	bool IgnoreCase;
	bool IgnorePaths;
	std::vector<SZipResFileEntry> FileList;
	pthread_mutex_t mutex;
};

//Not Support UnZipReader, currently
#if 0
class CUnZipReader : public CZipReader
{
public:

	CUnZipReader(IFileSystem *parent, const char* basename,
				 bool ignoreCase, bool ignorePaths);

	//! opens a file by file name
	virtual IReadFile* openFile(const char* filename);

	//! returns fileindex
	int32 findFile(const char* filename);

	//! returns the base folder name
	const char* getBaseFolderName() const
	{
		return Base.c_str();
	}

private:

	IFileSystem *Parent;
	void buildDirectory ();

	std::string Base;
};
#endif

#endif

