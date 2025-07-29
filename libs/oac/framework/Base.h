#ifndef _MMO_HEAD_H_
#define _MMO_HEAD_H_

#include "Singleton.h"
#include "Debugger.h"
#include "Logger.h"

#define PI 3.14159265359f

#define PI64 3.1415926535897932384626433832795028841971693993751

#define DEGTORAD (PI / 180.0f)

#define DEGTORAD64 (PI64 / 180.0)

#define RADTODEG (180.0f / PI)

#define RADTODEG64 (180.0 / PI64)

#define RAD_TO_DEG(x) ((x) * RADTODEG)

#define DEG_TO_RAD(x) ((x) * DEGTORAD)

const float ROUNDING_ERROR_32 = 0.00005f;
const double ROUNDING_ERROR_64 = 0.000005;

#define F32_VALUE_0 0x00000000
#define F32_VALUE_1 0x3f800000
#define F32_SIGN_BIT 0x80000000U
#define F32_EXPON_MANTISSA 0x7FFFFFFFU

#endif
