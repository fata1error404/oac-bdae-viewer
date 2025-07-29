#ifndef __MMO_DEBUGGER_H_
#define __MMO_DEBUGGER_H_

#include <cstdio>
#include "Logger.h"

#define MMO_ASSERT_STR(cond, msg) \
    if (!(cond))                  \
    {                             \
        ELOG(msg);                \
    }

#define MMO_ASSERT_P1(cond, msg, p1) \
    if (!(cond))                     \
    {                                \
        ELOG(msg, p1);               \
    }

#define MMO_ASSERT_P2(cond, msg, p1, p2) \
    if (!(cond))                         \
    {                                    \
        ELOG(msg, p1, p2);               \
    }

#define MMO_ASSERT_P3(cond, msg, p1, p2, p3) \
    if (!(cond))                             \
    {                                        \
        ELOG(msg, p1, p2, p3);               \
    }

#define MMO_ASSERT(cond) MMO_ASSERT_STR((cond), "MMO_ASSERT")

#endif
