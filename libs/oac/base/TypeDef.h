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
// #include <string>

/* Define NULL pointer value */
/*
#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif
*/

#ifdef __GNUC__

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define I64FMT "%016llX"
#define I64FMTD "%llu"
#define SI64FMTD "%lld"

#else

#define I64FMT "%016I64X"
#define I64FMTD "%I64u"
#define SI64FMTD "%I64d"
#define atoll _atoi64
#define ltoa _ltoa
#endif

#ifdef __GNUC__
#define LIKELY(_x) \
	__builtin_expect((_x), 1)
#define UNLIKELY(_x) \
	__builtin_expect((_x), 0)
#else
#define LIKELY(_x) \
	_x
#define UNLIKELY(_x) \
	_x
#endif

// #ifndef SOCKET
// typedef uint32 SOCKET;
// #define INVALID_SOCKET  (SOCKET)(~0)
// #endif

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

/*
#ifdef __GNUC__
#include <ext/hash_map>
#include <ext/hash_set>
#else
#include <hash_map>
#include <hash_set>
#endif

#if COMPILER == COMPILER_MICROSOFT && _MSC_VER >= 1300
#define HM_NAMESPACE stdext
#else
#define HM_NAMESPACE __gnu_cxx
#endif
*/

// CPU endian
#ifndef BIG_ENDIAN
#define LITTLE_ENDIAN 1
#endif

#if !LITTLE_ENDIAN
#define _BITSWAP16(x) (((x << 8) & 0xff00) | ((x >> 8) & 0x00ff))

#define _BITSWAP32(x) (((x << 24) & 0xff000000) | ((x << 8) & 0x00ff0000) | \
					   ((x >> 8) & 0x0000ff00) | ((x >> 24) & 0x000000ff))

#define _BITSWAP64(x) (((x << 56) & 0xff00000000000000ULL) | ((x << 40) & 0x00ff000000000000ULL) | \
					   ((x << 24) & 0x0000ff0000000000ULL) | ((x << 8) & 0x000000ff00000000ULL) |  \
					   ((x >> 8) & 0x00000000ff000000ULL) | ((x >> 24) & 0x0000000000ff0000ULL) |  \
					   ((x >> 40) & 0x000000000000ff00ULL) | ((x >> 56) & 0x00000000000000ffULL))
#else
#define _BITSWAP16(x) (x)
#define _BITSWAP32(x) (x)
#define _BITSWAP64(x) (x)
#endif

struct FTableHeader
{
	uint8 FLAG[8];
	uint32 nVersion;
	uint32 nRows;
	uint32 nCols;
	uint32 nOffsetIndex;
	uint32 nOffsetFormat;
	uint32 nOffsetEntry;
	uint32 nEntrySize;
	uint32 nOffsetStrTable;
	uint32 nLengthStrTable;
};
#define TABLE_STR_FLAG "FTABLE"
#define TABLE_VERSION 0x100000
#endif // TYPE_COMMON_H_
