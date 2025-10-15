#ifndef __VECTOR_3D_H_INCLUDED__
#define __VECTOR_3D_H_INCLUDED__

#include <cmath>
#include "Base.h"

// Returns if a equals b, taking possible rounding errors into account.
inline bool equals_float(const float a, const float b, const float tolerance = ROUNDING_ERROR_32)
{
	return (a + tolerance >= b) && (a - tolerance <= b);
}

// Returns if a equals zero, taking rounding errors into account.
inline bool iszero_float(const float a, const float tolerance = ROUNDING_ERROR_32)
{
	return fabsf(a) <= tolerance;
}

// 3D vector class
// _______________

class VEC3
{
  public:
	// vector components
	float X;
	float Y;
	float Z;

	typedef float SValueType;

	// default constructor (null vector)
	VEC3() : X(0), Y(0), Z(0) {}
	// constructor with 3 different values
	VEC3(float nx, float ny, float nz) : X(nx), Y(ny), Z(nz) {}
	// constructor with the same value for all elements
	explicit VEC3(float n) : X(n), Y(n), Z(n) {}
	// copy constructor
	VEC3(const VEC3 &other) : X(other.X), Y(other.Y), Z(other.Z) {}

	// OPERATORS
	// _________

	float &operator[](unsigned int i)
	{
		return getDataPtr()[i];
	}

	const float &operator[](unsigned int i) const
	{
		return getDataPtr()[i];
	}

	VEC3 operator-() const { return VEC3(-X, -Y, -Z); }

	VEC3 &operator=(const VEC3 &other)
	{
		X = other.X;
		Y = other.Y;
		Z = other.Z;
		return *this;
	}

	VEC3 operator+(const VEC3 &other) const { return VEC3(X + other.X, Y + other.Y, Z + other.Z); }
	VEC3 &operator+=(const VEC3 &other)
	{
		X += other.X;
		Y += other.Y;
		Z += other.Z;
		return *this;
	}
	VEC3 operator+(const float val) const { return VEC3(X + val, Y + val, Z + val); }
	VEC3 &operator+=(const float val)
	{
		X += val;
		Y += val;
		Z += val;
		return *this;
	}

	VEC3 operator-(const VEC3 &other) const { return VEC3(X - other.X, Y - other.Y, Z - other.Z); }
	VEC3 &operator-=(const VEC3 &other)
	{
		X -= other.X;
		Y -= other.Y;
		Z -= other.Z;
		return *this;
	}
	VEC3 operator-(const float val) const { return VEC3(X - val, Y - val, Z - val); }
	VEC3 &operator-=(const float val)
	{
		X -= val;
		Y -= val;
		Z -= val;
		return *this;
	}

	VEC3 operator*(const VEC3 &other) const { return VEC3(X * other.X, Y * other.Y, Z * other.Z); }
	VEC3 &operator*=(const VEC3 &other)
	{
		X *= other.X;
		Y *= other.Y;
		Z *= other.Z;
		return *this;
	}
	VEC3 operator*(const float val) const { return VEC3(X * val, Y * val, Z * val); }
	VEC3 &operator*=(const float val)
	{
		X *= val;
		Y *= val;
		Z *= val;
		return *this;
	}

	VEC3 operator/(const VEC3 &other) const { return VEC3(X / other.X, Y / other.Y, Z / other.Z); }
	VEC3 &operator/=(const VEC3 &other)
	{
		X /= other.X;
		Y /= other.Y;
		Z /= other.Z;
		return *this;
	}
	VEC3 operator/(const float val) const
	{
		float i = (float)1.0 / val;
		return VEC3(X * i, Y * i, Z * i);
	}
	VEC3 &operator/=(const float val)
	{
		float i = (float)1.0 / val;
		X *= i;
		Y *= i;
		Z *= i;
		return *this;
	}

	bool operator<=(const VEC3 &other) const { return X <= other.X && Y <= other.Y && Z <= other.Z; }
	bool operator>=(const VEC3 &other) const { return X >= other.X && Y >= other.Y && Z >= other.Z; }
	bool operator<(const VEC3 &other) const { return X < other.X && Y < other.Y && Z < other.Z; }
	bool operator>(const VEC3 &other) const { return X > other.X && Y > other.Y && Z > other.Z; }

	bool operator==(const VEC3 &other) const
	{
		return this->equals(other);
	}

	bool operator!=(const VEC3 &other) const
	{
		return !this->equals(other);
	}

	// FUNCTIONS
	// _________

	VEC3 &set(const float nx, const float ny, const float nz)
	{
		X = nx;
		Y = ny;
		Z = nz;
		return *this;
	}

	VEC3 &set(const VEC3 &p)
	{
		X = p.X;
		Y = p.Y;
		Z = p.Z;
		return *this;
	}

	//! Check if 2 vectors are equal, taking floating point rounding errors into account.
	bool equals(const VEC3 &other, const float tolerance = (float)ROUNDING_ERROR_32) const
	{
		return equals_float(X, other.X, tolerance) &&
			   equals_float(Y, other.Y, tolerance) &&
			   equals_float(Z, other.Z, tolerance);
	}

	//! Get length of the vector.
	float getLength() const { return (float)sqrt((double)(X * X + Y * Y + Z * Z)); }

	//! Get squared length of the vector.
	/** This is useful because it is much faster than getLength().
		\return Squared length of the vector. */
	float getLengthSQ() const { return X * X + Y * Y + Z * Z; }

	//! Get the dot product with another vector.
	float dotProduct(const VEC3 &other) const
	{
		return X * other.X + Y * other.Y + Z * other.Z;
	}

	//! Get distance from another point.
	/** Here, the vector is interpreted as point in 3 dimensional space. */
	float getDistanceFrom(const VEC3 &other) const
	{
		return VEC3(X - other.X, Y - other.Y, Z - other.Z).getLength();
	}

	//! Returns squared distance from another point.
	/** Here, the vector is interpreted as point in 3 dimensional space. */
	float getDistanceFromSQ(const VEC3 &other) const
	{
		return VEC3(X - other.X, Y - other.Y, Z - other.Z).getLengthSQ();
	}

	//! Calculates the cross product with another vector.
	/** \param p Vector to multiply with.
		\return Crossproduct of this vector with p. */
	VEC3 crossProduct(const VEC3 &p) const
	{
		return VEC3(Y * p.Z - Z * p.Y, Z * p.X - X * p.Z, X * p.Y - Y * p.X);
	}

	//! Returns if this vector interpreted as a point is on a line between two other points.
	/** It is assumed that the point is on the line.
		\param begin Beginning vector to compare between.
		\param end Ending vector to compare between.
		\return True if this vector is between begin and end, false if not. */
	bool isBetweenPoints(const VEC3 &begin, const VEC3 &end) const
	{
		const float f = (end - begin).getLengthSQ();
		return getDistanceFromSQ(begin) <= f &&
			   getDistanceFromSQ(end) <= f;
	}

	//! Normalizes the vector.
	/** In case of the 0 vector the result is still 0, otherwise
		the length of the vector will be 1.
		TODO: 64 Bit template doesnt work.. need specialized template
		\return Reference to this vector after normalization. */
	VEC3 &normalize()
	{
		float l = X * X + Y * Y + Z * Z;
		if (l == 0)
			return *this;
		l = (float)1.0f / sqrtf((float)l);
		X *= l;
		Y *= l;
		Z *= l;
		return *this;
	}

	//! Sets the length of the vector to a new value.
	VEC3 &setLength(float newlength)
	{
		normalize();
		return (*this *= newlength);
	}

	//! Inverts the vector.
	VEC3 &invert()
	{
		X *= -1.0f;
		Y *= -1.0f;
		Z *= -1.0f;
		return *this;
	}

	//! Rotates the vector by a specified number of degrees around the Y axis and the specified center.
	/** \param degrees Number of degrees to rotate around the Y axis.
		\param center The center of the rotation. */
	void rotateXZBy(double degrees, const VEC3 &center = VEC3())
	{
		degrees *= DEGTORAD64;
		float cs = (float)cos(degrees);
		float sn = (float)sin(degrees);
		X -= center.X;
		Z -= center.Z;
		set(X * cs - Z * sn, Y, X * sn + Z * cs);
		X += center.X;
		Z += center.Z;
	}

	//! Rotates the vector by a specified number of degrees around the Z axis and the specified center.
	/** \param degrees: Number of degrees to rotate around the Z axis.
		\param center: The center of the rotation. */
	void rotateXYBy(double degrees, const VEC3 &center = VEC3())
	{
		degrees *= DEGTORAD64;
		float cs = (float)cos(degrees);
		float sn = (float)sin(degrees);
		X -= center.X;
		Y -= center.Y;
		set(X * cs - Y * sn, X * sn + Y * cs, Z);
		X += center.X;
		Y += center.Y;
	}

	//! Rotates the vector by a specified number of degrees around the X axis and the specified center.
	/** \param degrees: Number of degrees to rotate around the X axis.
		\param center: The center of the rotation. */
	void rotateYZBy(double degrees, const VEC3 &center = VEC3())
	{
		degrees *= DEGTORAD64;
		float cs = (float)cos(degrees);
		float sn = (float)sin(degrees);
		Z -= center.Z;
		Y -= center.Y;
		set(X, Y * cs - Z * sn, Y * sn + Z * cs);
		Z += center.Z;
		Y += center.Y;
	}

	//! Returns interpolated vector.
	/** \param other Other vector to interpolate between
		\param d Value between 0.0f and 1.0f. */
	VEC3 getInterpolated(const VEC3 &other, const float d) const
	{
		const float inv = (float)1.0 - d;
		return VEC3(other.X * inv + X * d, other.Y * inv + Y * d, other.Z * inv + Z * d);
	}

	//! Returns interpolated vector. ( quadratic )
	/** \param v2 Second vector to interpolate with
		\param v3 Third vector to interpolate with
		\param d Value between 0.0f and 1.0f. */
	VEC3 getInterpolated_quadratic(const VEC3 &v2, const VEC3 &v3, const float d) const
	{
		const float inv = (float)1.0 - d;
		const float mul0 = inv * inv;
		const float mul1 = (float)2.0 * d * inv;
		const float mul2 = d * d;

		return VEC3(X * mul0 + v2.X * mul1 + v3.X * mul2,
					Y * mul0 + v2.Y * mul1 + v3.Y * mul2,
					Z * mul0 + v2.Z * mul1 + v3.Z * mul2);
	}

	//! Gets the Y and Z rotations of a vector.
	/** Thanks to Arras on the Irrlicht forums for this method.
		\return A vector representing the rotation in degrees of
		this vector. The Z component of the vector will always be 0. */
	VEC3 getHorizontalAngle() const
	{
		VEC3 angle;

		angle.Y = (float)(atan2(X, Z) * RADTODEG64);

		if (angle.Y < 0.0f)
			angle.Y += 360.0f;
		if (angle.Y >= 360.0f)
			angle.Y -= 360.0f;

		const double z1 = sqrt(X * X + Z * Z);

		angle.X = (float)(atan2(z1, (double)Y) * RADTODEG64 - 90.0);

		if (angle.X < 0.0f)
			angle.X += 360.0f;
		if (angle.X >= 360.0f)
			angle.X -= 360.0f;

		return angle;
	}

	//! Builds a direction vector from (this) rotation vector.
	/** This vector is assumed to hold 3 Euler angle rotations, in degrees.
		The implementation performs the same calculations as using a matrix to
		do the rotation.
		\param[in] forwards  The direction representing "forwards" which will be
		rotated by this vector.  If you do not provide a
		direction, then the positive Z axis (0, 0, 1) will
		be assumed to be fowards.
		\return A vector calculated by rotating the forwards direction by
		the 3 Euler angles that this vector is assumed to represent. */
	VEC3 rotationToDirection(const VEC3 &forwards = VEC3(0, 0, 1)) const
	{
		const double cr = cos(DEGTORAD64 * X);
		const double sr = sin(DEGTORAD64 * X);
		const double cp = cos(DEGTORAD64 * Y);
		const double sp = sin(DEGTORAD64 * Y);
		const double cy = cos(DEGTORAD64 * Z);
		const double sy = sin(DEGTORAD64 * Z);

		const double srsp = sr * sp;
		const double crsp = cr * sp;

		const double pseudoMatrix[] = {
			(cp * cy), (cp * sy), (-sp),
			(srsp * cy - cr * sy), (srsp * sy + cr * cy), (sr * cp),
			(crsp * cy + sr * sy), (crsp * sy - sr * cy), (cr * cp)};

		return VEC3(
			(float)(forwards.X * pseudoMatrix[0] +
					forwards.Y * pseudoMatrix[3] +
					forwards.Z * pseudoMatrix[6]),
			(float)(forwards.X * pseudoMatrix[1] +
					forwards.Y * pseudoMatrix[4] +
					forwards.Z * pseudoMatrix[7]),
			(float)(forwards.X * pseudoMatrix[2] +
					forwards.Y * pseudoMatrix[5] +
					forwards.Z * pseudoMatrix[8]));
	}

	//! Fills an array of 4 values with the vector data (usually floats).
	/** Useful for setting in shader constants for example. The fourth value
		will always be 0. */
	void getAs4Values(float *array) const
	{
		array[0] = X;
		array[1] = Y;
		array[2] = Z;
		array[3] = 0;
	}

	const float *getDataPtr() const { return reinterpret_cast<const float *>(this); }

	float *getDataPtr() { return reinterpret_cast<float *>(this); }
};

// Multiplies a scalar and a vector component-wise.
inline VEC3 operator*(const float scalar, const VEC3 &vector)
{
	return vector * scalar;
}

#endif
