// Copyright (C) 2002-2008 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h
#pragma once
#ifndef __I_READ_RES_FILE_H_INCLUDED__
#define __I_READ_RES_FILE_H_INCLUDED__
#include "TypeDef.h"
#include "ResReferenceCounted.h"

//! Interface providing read acess to a file.
class IReadResFile : public IResReferenceCounted
{
private:
	//! The reference counter. Mutable to do reference counting on const objects.
	// mutable int32 ReferenceCounter;
	//! The debug name.
	// const char* DebugName;
public:
	IReadResFile() {}
	virtual ~IReadResFile() {};
	/*
		void grab() const { ++ReferenceCounter; }
		bool drop() const
		{
			--ReferenceCounter;
			if (!ReferenceCounter)
			{
				const_cast<IReadResFile*>(this)->onDelete();
				delete this;
				return true;
			}

			return false;
		}

		//! Get the reference count.
		int32 getReferenceCount() const
		{
			return ReferenceCounter;
		}

		//! Returns the debug name of the object.
		const char* getDebugName() const
		{
			return DebugName;
		}

		//! Sets the debug name of the object.
		void setDebugName(const char* newName)
		{
			DebugName = newName;
		}

		//! Called by drop just before deleting.
		virtual void onDelete() {}
	*/
	//! Reads an amount of bytes from the file.
	/** \param buffer Pointer to buffer where read bytes are written to.
	\param sizeToRead Amount of bytes to read from the file.
	\return How much bytes were read. */
	virtual S32 read(void *buffer, U32 sizeToRead) = 0;

	//! Changes position in file
	/** \param finalPos Destination position in the file.
	\param relativeMovement If set to true, the position in the file is
	changed relative to current position. Otherwise the position is changed
	from beginning of file.
	\return True if successful, otherwise false. */
	virtual bool seek(long finalPos, bool relativeMovement = false) = 0;

	//!
	virtual IReadResFile *clone() const = 0;

	//! Get size of file.
	/** \return Size of the file in bytes. */
	virtual long getSize() const = 0;

	//! Get the current position in the file.
	/** \return Current position in the file in bytes. */
	virtual long getPos() const = 0;

	//! Get name of file.
	/** \return File name as zero terminated character string. */
	virtual const char *getFileName() const = 0;

	//! Get the file complete path
	/** \return File complete path as zero terminated character string. */
	//	virtual const char* getFullPath() const = 0;

	virtual void *getBuffer(long *size)
	{
		return 0;
	}

	virtual void *getExternalBuffer(const char *filename, long *size)
	{
		return 0;
	}

	virtual bool isAllInMemory() const
	{
		return false;
	}
};

//! Internal function, please do not use.
IReadResFile *createReadFile(const char *fileName);
//! Internal function, please do not use.
IReadResFile *createLimitReadFile(const char *fileName, IReadResFile *alreadyOpenedFile, long areaSize);
//! Internal function, please do not use.
IReadResFile *createMemoryReadFile(void *memory, long size, const char *fileName, bool deleteMemoryWhenDropped);

#endif
