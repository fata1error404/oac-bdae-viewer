#ifndef _COMMON_DEF_H_
#define _COMMON_DEF_H_

// #include "Base.h"
#include <cmath>

//////////////////////////////////////////////////////////////////////////
// System stuff

// //    include/common/physics/Physics.cpp
//     include/common/physics/PhysicsBox.cpp
//     include/common/physics/PhysicsCylinder.cpp
//     include/common/physics/PhysicsGeom.cpp
//     include/common/physics/PhysicsGeomPool.cpp
//     include/common/physics/PhysicsMesh.cpp
//     include/common/physics/PhysicsPlane.cpp

// //
#define DLL_EXP

#if defined(WIN32) || defined(WINCE)
#include "platform/win32/Win32.h"
#elif defined BREW
#include "platform/brew/Brew.h"
#elif defined SYMBIAN
#include "platform/symbian/Symbian.h"
#elif defined SDK_TS
#include "platform/nitro/Nitro.h"
#elif defined IPHONEOS
#include "platform/iphone/iPhone.h"
#endif
#if defined(_DEBUG) || defined(DEBUG)

#define REMINDER(msg) message(__FILE__ "(" STRING(__LINE__) ") : REMINDER " msg)
#define REMINDERTO(to, msg) message(__FILE__ "(" STRING(__LINE__) ") : REMINDER [TO " #to "] " msg)
#define HARDCODED(v) message(__FILE__ "(" STRING(__LINE__) ") : HARDCODED '" #v "'")
#define MUST_NOT_COMMIT message(__FILE__ "(" STRING(__LINE__) ") : *************************** MUST NOT COMMIT THIS ***********************")

#else

#define REMINDER(msg)
#define REMINDERTO(to, msg)
#define HARDCODED(v)
#define MUST_NOT_CHECKIN

#endif

// General Macros
// ______________

#define GET_APP_TIME System::GetAppTime()
#define GET_SYS_TIME System::CurrentTimeMillis()
#define GET_REAL_TIME System::GetRealTime()

#define INT_MAXVALUE 0x7fffffff

// --- usefull key defines
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#define NORMALIZE(x, max)    \
    {                        \
        while ((x) >= (max)) \
            (x) -= (max);    \
        while ((x) < 0)      \
            (x) += (max);    \
    }

#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif
#ifndef SGN
#define SGN(a) (a < 0 ? -1 : 1)
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#define CLAMP(min, x, max)     \
    {                          \
        (x) = MAX((x), (min)); \
        (x) = MIN((x), (max)); \
    }
#define SQUARE(x) ((x) * (x))
#define FLOAT_EPSINON 0.000001
#define FLOAT_EQUALS_ZERO(a) ((a) > -FLOAT_EPSINON && (a) < FLOAT_EPSINON)

#define CM2M (0.01f)

#define XTOF(x) ((float)((x) * (1.0f / 65536.0f)))
#define FTOX(x) ((GLfixed)((x) * (65536.0f)))

#define SAFE_FREE(a)  \
    {                 \
        if (a)        \
        {             \
            free(a);  \
            a = NULL; \
        }             \
    }
#define SAFE_DEL(a)   \
    {                 \
        if (a)        \
        {             \
            delete a; \
            a = NULL; \
        }             \
    }
#define SAFE_DEL_ARRAY(a) \
    {                     \
        if (a)            \
        {                 \
            delete[] (a); \
            a = NULL;     \
        }                 \
    }
#define SAFE_DEL_STRING_ARRAY(a, len)                    \
    {                                                    \
        if (a)                                           \
        {                                                \
            for (int __iii1 = 0; __iii1 < len; __iii1++) \
            {                                            \
                SAFE_DEL_ARRAY(a[__iii1]);               \
            }                                            \
            delete[] (a);                                \
            a = NULL;                                    \
        }                                                \
    }
#define SAFE_DEL_POINTER_ARRAY(a, len)                   \
    {                                                    \
        if (a)                                           \
        {                                                \
            for (int __iii1 = 0; __iii1 < len; __iii1++) \
            {                                            \
                SAFE_DEL(a[__iii1]);                     \
            }                                            \
            delete[] (a);                                \
            a = NULL;                                    \
        }                                                \
    }

#if defined(WIN32) || defined(__WINS__)
#define DEBUG_MEMORY_USAGE_TO_FILE(fileName)                          \
    {                                                                 \
        if (CMemoryManager::GetInstance())                            \
        {                                                             \
            CMemoryManager::GetInstance()->DumpMemoryUsage(fileName); \
        }                                                             \
    }
#else
#define DEBUG_MEMORY_USAGE_TO_FILE(fileName) \
    {                                        \
    }
#endif

#define _GET_U32(array) ((array[off]) | ((array[off + 1]) << 8) | ((array[off + 2]) << 16) | ((array[off + 3]) << 24))
#define _GET_U16(array) ((array[off]) | ((array[off + 1]) << 8))
#define _GET_U8(array) ((array[off]))

#define GET_U32(array)     \
    (U32) _GET_U32(array); \
    off += 4;
#define GET_U16(array)     \
    (U16) _GET_U16(array); \
    off += 2;
#define GET_U8(array)    \
    (U8) _GET_U8(array); \
    off += 1;

#define GET_S32(array)     \
    (S32) _GET_U32(array); \
    off += 4;
#define GET_S16(array)     \
    (S16) _GET_U16(array); \
    off += 2;
#define GET_S8(array)    \
    (S8) _GET_U8(array); \
    off += 1;

#endif
