#ifndef __QUATERNION_H_INCLUDED__
#define __QUATERNION_H_INCLUDED__

#include "MTX4.h"

class Quaternion
{
  public:
	float X, Y, Z, W;

	// creates a matrix from a quaternion
	void getMatrix(MTX4 &dest) const
	{
		const float _2xx = 2.0f * X * X;
		const float _2yy = 2.0f * Y * Y;
		const float _2zz = 2.0f * Z * Z;
		const float _2xy = 2.0f * X * Y;
		const float _2xz = 2.0f * X * Z;
		const float _2xw = 2.0f * X * W;
		const float _2yz = 2.0f * Y * Z;
		const float _2yw = 2.0f * Y * W;
		const float _2zw = 2.0f * Z * W;

		dest[0] = 1.0f - _2yy - _2zz;
		dest[4] = _2xy + _2zw;
		dest[8] = _2xz - _2yw;
		dest[12] = 0.0f;

		dest[1] = _2xy - _2zw;
		dest[5] = 1.0f - _2xx - _2zz;
		dest[9] = _2yz + _2xw;
		dest[13] = 0.0f;

		dest[2] = _2xz + _2yw;
		dest[6] = _2yz - _2xw;
		dest[10] = 1.0f - _2yy - _2xx;
		dest[14] = 0.0f;

		dest[3] = 0.f;
		dest[7] = 0.f;
		dest[11] = 0.f;
		dest[15] = 1.f;
	}
};

#endif
