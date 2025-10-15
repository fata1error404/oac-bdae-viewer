#ifndef TYPE_DEF_H_
#define TYPE_DEF_H_

// type define without irrlicht

/////////////////////////////////////////////////
// typedef
typedef unsigned char BYTE;
typedef unsigned int UINT;
/* Use correct types for x64 platforms, too */

#ifdef __GNUC__
typedef long long int64;
typedef int int32;
typedef short int16;
typedef signed char int8;
typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
// typedef unsigned int DWORD;
typedef unsigned char byte;
#else
typedef signed __int64 int64;
typedef signed __int32 int32;
typedef signed __int16 int16;
typedef signed __int8 int8;
typedef unsigned __int64 uint64;
typedef unsigned __int32 uint32;
typedef unsigned __int16 uint16;
typedef unsigned __int8 uint8;
#endif

typedef uint64 ObjGUID;
const static ObjGUID ObjGUID_NULL = 0;

union InstanceGUID
{
	inline bool operator<(const InstanceGUID &p) const
	{
		return instanceGUID < p.instanceGUID;
	}

	inline bool operator==(const InstanceGUID &other) const
	{
		return instanceGUID == other.instanceGUID;
	}

	inline bool operator!=(const InstanceGUID &other) const
	{
		return instanceGUID != other.instanceGUID;
	}

	uint64 instanceGUID;
	struct
	{
		uint16 GSID;
		uint16 MAPID;
		uint32 UID;
	};
};

const static InstanceGUID InsGUID_INVALID = {0};

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64)
typedef uint64 dword_ptr;
#else
typedef uint32 dword_ptr;
#endif

typedef uint32 U32;
typedef uint16 U16;
typedef uint8 U8;
typedef int32 S32;
typedef int16 S16;
typedef int8 S8;
typedef float F32;

// ANSI C .h
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>

// STL .h
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <queue>
#include <map>
#include <set>
#include <deque>
#include <string>
#include <limits>
#include <set>
using namespace std;

#endif
