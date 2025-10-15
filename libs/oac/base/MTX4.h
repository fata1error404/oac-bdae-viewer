
#ifndef __MATRIX_H_INCLUDED__
#define __MATRIX_H_INCLUDED__
#include <cstring>
#include <string.h>
#include <cmath>
#include "AABB.h"
#include "VEC3.h"
#include "Base.h"

// 4x4 matrix. Mostly used as transformation matrix for 3d calculations.
/** The matrix is a D3D style matrix, row major with translations in the 4th row. */
//

class MTX4
{
  public:
	// constructor Flags
	enum eConstructor
	{
		EM4CONST_NOTHING = 0,
		EM4CONST_COPY,
		EM4CONST_IDENTITY,
		EM4CONST_TRANSPOSED,
		EM4CONST_INVERSE,
		EM4CONST_INVERSE_TRANSPOSED
	};

	// default constructor
	/** \param constructor Choose the initialization style */
	MTX4(eConstructor constructor = EM4CONST_IDENTITY);

	// copy constructor
	/** \param other Other matrix to copy from
		\param constructor Choose the initialization style */
	MTX4(const MTX4 &other, eConstructor constructor = EM4CONST_COPY);

	// OPERATORS
	// _________

	//! Directly accesses every element of the matrix.
	float &operator()(const int row, const int col)
	{
		definitelyIdentityMatrix = false;
		return M[row * 4 + col];
	}
	const float &operator()(const int row, const int col) const
	{
		return M[row * 4 + col];
	}

	//! Linearly accesses every element of the matrix.
	float &operator[](unsigned int index)
	{
		definitelyIdentityMatrix = false;
		return M[index];
	}
	const float &operator[](unsigned int index) const
	{
		return M[index];
	}

	//! Sets all elements of this matrix to the value.
	inline MTX4 &operator=(const float &scalar);

	//! Returns pointer to internal array.
	const float *pointer() const { return M; }
	float *pointer()
	{
		definitelyIdentityMatrix = false;
		return M;
	}

	//! Returns true (or false respecively) if other matrix is equal to this matrix.
	bool operator==(const MTX4 &other) const;
	bool operator!=(const MTX4 &other) const;

	//! Add another matrix.
	MTX4 operator+(const MTX4 &other) const;
	MTX4 &operator+=(const MTX4 &other);

	//! Subtract another matrix.
	MTX4 operator-(const MTX4 &other) const;
	MTX4 &operator-=(const MTX4 &other);

	//! set this matrix to the product of two matrices
	inline MTX4 &setbyproduct(const MTX4 &other_a, const MTX4 &other_b);
	MTX4 &setbyproduct_nocheck(const MTX4 &other_a, const MTX4 &other_b);

	//! Multiply by another matrix.
	MTX4 operator*(const MTX4 &other) const;
	MTX4 &operator*=(const MTX4 &other);

	//! Multiply by another matrix as if both matrices where 3x4.
	MTX4 mult34(const MTX4 &m2) const;

	//! Multiply by another matrix as if both matrices where 3x4.
	MTX4 &mult34(const MTX4 &m2, MTX4 &out) const;

	//! Multiply by scalar.
	MTX4 operator*(const float &scalar) const;

	//! Multiply by scalar.
	MTX4 &operator*=(const float &scalar);

	//! Set matrix to identity.
	inline MTX4 &makeIdentity();

	//! Returns true if the matrix is the identity matrix
	inline bool isIdentity() const;

	//! Returns true if the matrix is the identity matrix
	bool isIdentity_integer_base() const;

	//! Returns the c'th column of the matrix, without the lowest row.
	VEC3 getColumn(unsigned int c) const;

	//! Returns the c'th column of the matrix.
	//	vector4d getFullColumn(unsigned int c) const;

	//! Sets the c'th column of the matrix, without the lowest row.
	MTX4 &setColumn(unsigned int c, const VEC3 &v);

	//! Sets the c'th column of the matrix.
	// MTX4& setFullColumn(unsigned int c, const vector4d& v);

	//! Gets the current translation
	VEC3 getTranslation() const;

	//! Set the translation of the current matrix. Will erase any previous values.
	MTX4 &setTranslation(const VEC3 &translation);

	//! Set the inverse translation of the current matrix. Will erase any previous values.
	MTX4 &setInverseTranslation(const VEC3 &translation);

	//! Make a rotation matrix from Euler angles. The 4th row and column are unmodified.
	inline MTX4 &setRotationRadians(const VEC3 &rotation);

	//! Make a rotation matrix from Euler angles. The 4th row and column are unmodified.
	MTX4 &setRotationDegrees(const VEC3 &rotation);

	//! Returns the rotation, as set by setRotation().
	/** This code was orginally written by by Chev. */
	VEC3 getRotationDegrees() const;

	//! Make an inverted rotation matrix from Euler angles.
	/** The 4th row and column are unmodified. */
	inline MTX4 &setInverseRotationRadians(const VEC3 &rotation);

	//! Make an inverted rotation matrix from Euler angles.
	/** The 4th row and column are unmodified. */
	MTX4 &setInverseRotationDegrees(const VEC3 &rotation);

	//! Set Scale
	MTX4 &setScale(const VEC3 &scale);

	//! Set Scale
	MTX4 &setScale(const float scale)
	{
		return setScale(VEC3(scale, scale, scale));
	}

	//! Apply scale to this matrix as if multiplication was on the left.
	MTX4 &preScale(const VEC3 &scale);

	//! Apply scale to this matrix as if multiplication was on the right.
	MTX4 &postScale(const VEC3 &scale);

	//! Get Scale
	VEC3 getScale() const;

	//! Translate a vector by the inverse of the translation part of this matrix.
	void inverseTranslateVect(VEC3 &vect) const;

	//! Rotate a vector by the inverse of the rotation part of this matrix.
	void inverseRotateVect(VEC3 &vect) const;

	//! Rotate a vector by the rotation part of this matrix.
	void rotateVect(VEC3 &vect) const;

	//! An alternate transform vector method, writing into a second vector
	void rotateVect(VEC3 &out, const VEC3 &in) const;

	//! An alternate transform vector method, writing into an array of 3 floats
	void rotateVect(float *out, const VEC3 &in) const;

	//! Transforms the vector by this matrix
	void transformVect(VEC3 &vect) const;

	//! Transforms input vector by this matrix and stores result in output vector
	void transformVect(VEC3 &out, const VEC3 &in) const;

	//! Transforms the vector by this matrix as though it was in 2D (Z ignored).
	void transformVect2D(VEC3 &vect) const;

	//! Transforms input vector by this matrix and stores result in output vector as though it was in 2D (Z ignored).
	void transformVect2D(VEC3 &out, const VEC3 &in) const;

	//! An alternate transform vector method, writing into an array of 4 floats.
	void transformVect(float *out, const VEC3 &in) const;

	//! Translate a vector by the translation part of this matrix.
	void translateVect(VEC3 &vect) const;

	//! Transforms a axis aligned bounding box
	/** The result box of this operation may not be accurate at all. For
		correct results, use transformBoxEx() */
	void transformBox(AABB &box) const;

	//! Transforms a axis aligned bounding box
	/** The result box of this operation should by accurate, but this operation
		is slower than transformBox(). */
	void transformBoxEx(AABB &box) const;

	//! Multiplies this matrix by a 1x4 matrix
	void multiplyWith1x4Matrix(float *matrix) const;

	//! Calculates inverse of matrix. Slow.
	/** \return Returns false if there is no inverse matrix.*/
	bool makeInverse();

	//! Inverts a primitive matrix which only contains a translation and a rotation
	/** \param out: where result matrix is written to. */
	bool getInversePrimitive(MTX4 &out) const;

	//! Gets the inversed matrix of this one
	/** \param out: where result matrix is written to.
		\return Returns false if there is no inverse matrix. */
	bool getInverse(MTX4 &out) const;

	//! Builds a right-handed perspective projection matrix based on a field of view
	// MTX4& buildProjectionMatrixPerspectiveFovRH(float fieldOfViewRadians, float aspectRatio, float zNear, float zFar);

	//! Builds a right-handed perspective projection matrix based on a field of view
	MTX4 &buildProjectionMatrixPerspectiveFov(float fieldOfViewRadians, float aspectRatio, float zNear, float zFar);

	//! Builds a right-handed perspective projection matrix.
	// MTX4& buildProjectionMatrixPerspectiveRH(float widthOfViewVolume, float heightOfViewVolume, float zNear, float zFar);

	//! Builds a left-handed perspective projection matrix.
	MTX4 &buildProjectionMatrixPerspective(float widthOfViewVolume, float heightOfViewVolume, float zNear, float zFar);

	//! Builds a centered right-handed orthogonal projection matrix.
	MTX4 &buildProjectionMatrixOrtho(float widthOfViewVolume, float heightOfViewVolume, float zNear, float zFar);

	//! Builds a right-handed orthogonal projection matrix.
	MTX4 &buildProjectionMatrixOrtho(float left, float right, float bottom, float top, float zNear, float zFar);

	//! Builds a right-handed orthogonal projection matrix.
	// MTX4& buildProjectionMatrixOrthoRH(float widthOfViewVolume, float heightOfViewVolume, float zNear, float zFar);

	//! Builds a right-handed look-at matrix.
	MTX4 &buildCameraLookAtMatrix(
		const VEC3 &position,
		const VEC3 &target,
		const VEC3 &upVector);

	//! Builds a right-handed look-at matrix.
	// MTX4& buildCameraLookAtMatrixRH(
	//		const VEC3& position,
	//		const VEC3& target,
	//		const VEC3& upVector);

	//! Builds a matrix that flattens geometry into a plane.
	/** \param light: light source
		\param plane: plane into which the geometry if flattened into
		\param point: value between 0 and 1, describing the light source.
		If this is 1, it is a point light, if it is 0, it is a directional light. */
	//	MTX4& buildShadowMatrix(const VEC3& light,  plane3df plane, float point=1.0f);

	//! Builds a matrix which transforms a normalized Device Coordinate to Device Coordinates.
	/** Used to scale <-1,-1><1,1> to viewport, for example from von <-1,-1> <1,1> to the viewport <0,0><0,640> */
	// MTX4& buildNDCToDCMatrix( const  rect<int>& area, float zScale);

	//! Creates a new matrix as interpolated matrix from two other ones.
	/** \param b: other matrix to interpolate with
		\param time: Must be a value between 0 and 1. */
	MTX4 interpolate(const MTX4 &b, float time) const;

	//! Gets transposed matrix
	MTX4 getTransposed() const;

	//! Gets transposed matrix
	inline void getTransposed(MTX4 &dest) const;

	/*
	  construct 2D Texture transformations
	  rotate about center, scale, and transform.
	*/
	//! Set to a texture transformation matrix with the given parameters.
	// MTX4& buildTextureTransform( float rotateRad,
	//									const  vector2df &rotatecenter,
	//									const  vector2df &translate,
	//									const  vector2df &scale);

	//! Set texture transformation rotation
	/** Rotate about z axis, recenter at (0.5,0.5).
		Doesn't clear other elements than those affected
		\param radAngle Angle in radians
		\return Altered matrix */
	MTX4 &setTextureRotationCenter(float radAngle);

	//! Set texture transformation translation
	/** Doesn't clear other elements than those affected.
		\param x Offset on x axis
		\param y Offset on y axis
		\return Altered matrix */
	MTX4 &setTextureTranslate(float x, float y);

	//! Set texture transformation translation, using a transposed representation
	/** Doesn't clear other elements than those affected.
		\param x Offset on x axis
		\param y Offset on y axis
		\return Altered matrix */
	MTX4 &setTextureTranslateTransposed(float x, float y);

	//! Set texture transformation scale
	/** Doesn't clear other elements than those affected.
		\param sx Scale factor on x axis
		\param sy Scale factor on y axis
		\return Altered matrix. */
	MTX4 &setTextureScale(float sx, float sy);

	//! Set texture transformation scale, and recenter at (0.5,0.5)
	/** Doesn't clear other elements than those affected.
		\param sx Scale factor on x axis
		\param sy Scale factor on y axis
		\return Altered matrix. */
	MTX4 &setTextureScaleCenter(float sx, float sy);

	//! Applies a texture post scale.
	/**	\param sx Scale factor on x axis
		\param sy Scale factor on y axis
		\return Altered matrix. */
	MTX4 &postTextureScale(float sx, float sy);

	//! Sets all matrix data members at once.
	MTX4 &setM(const float *data);

	//! Gets all matrix data members at once.
	/** \returns data */
	float *getM(float *data) const;

	//! Sets if the matrix is definitely identity matrix.
	void setDefinitelyIdentityMatrix(bool isDefinitelyIdentityMatrix);

	//! Gets if the matrix is definitely identity matrix.
	bool getDefinitelyIdentityMatrix() const;

	static inline void rowMatrixProduct(float *M, const float *m1, const float *m2)
	{
		M[0] = m1[0] * m2[0] + m1[4] * m2[1] + m1[8] * m2[2] + m1[12] * m2[3];
		M[1] = m1[1] * m2[0] + m1[5] * m2[1] + m1[9] * m2[2] + m1[13] * m2[3];
		M[2] = m1[2] * m2[0] + m1[6] * m2[1] + m1[10] * m2[2] + m1[14] * m2[3];
		M[3] = m1[3] * m2[0] + m1[7] * m2[1] + m1[11] * m2[2] + m1[15] * m2[3];

		M[4] = m1[0] * m2[4] + m1[4] * m2[5] + m1[8] * m2[6] + m1[12] * m2[7];
		M[5] = m1[1] * m2[4] + m1[5] * m2[5] + m1[9] * m2[6] + m1[13] * m2[7];
		M[6] = m1[2] * m2[4] + m1[6] * m2[5] + m1[10] * m2[6] + m1[14] * m2[7];
		M[7] = m1[3] * m2[4] + m1[7] * m2[5] + m1[11] * m2[6] + m1[15] * m2[7];

		M[8] = m1[0] * m2[8] + m1[4] * m2[9] + m1[8] * m2[10] + m1[12] * m2[11];
		M[9] = m1[1] * m2[8] + m1[5] * m2[9] + m1[9] * m2[10] + m1[13] * m2[11];
		M[10] = m1[2] * m2[8] + m1[6] * m2[9] + m1[10] * m2[10] + m1[14] * m2[11];
		M[11] = m1[3] * m2[8] + m1[7] * m2[9] + m1[11] * m2[10] + m1[15] * m2[11];

		M[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[8] * m2[14] + m1[12] * m2[15];
		M[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[9] * m2[14] + m1[13] * m2[15];
		M[14] = m1[2] * m2[12] + m1[6] * m2[13] + m1[10] * m2[14] + m1[14] * m2[15];
		M[15] = m1[3] * m2[12] + m1[7] * m2[13] + m1[11] * m2[14] + m1[15] * m2[15];
	}

	static inline void rowMatrixProduct34(float *M, const float *m1, const float *m2)
	{
		M[0] = m1[0] * m2[0] + m1[4] * m2[1] + m1[8] * m2[2];
		M[1] = m1[1] * m2[0] + m1[5] * m2[1] + m1[9] * m2[2];
		M[2] = m1[2] * m2[0] + m1[6] * m2[1] + m1[10] * m2[2];
		M[3] = float(0);

		M[4] = m1[0] * m2[4] + m1[4] * m2[5] + m1[8] * m2[6];
		M[5] = m1[1] * m2[4] + m1[5] * m2[5] + m1[9] * m2[6];
		M[6] = m1[2] * m2[4] + m1[6] * m2[5] + m1[10] * m2[6];
		M[7] = float(0);

		M[8] = m1[0] * m2[8] + m1[4] * m2[9] + m1[8] * m2[10];
		M[9] = m1[1] * m2[8] + m1[5] * m2[9] + m1[9] * m2[10];
		M[10] = m1[2] * m2[8] + m1[6] * m2[9] + m1[10] * m2[10];
		M[11] = float(0);

		M[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[8] * m2[14] + m1[12];
		M[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[9] * m2[14] + m1[13];
		M[14] = m1[2] * m2[12] + m1[6] * m2[13] + m1[10] * m2[14] + m1[14];
		M[15] = float(1);
	}

  private:
	float M[16];						   // matrix data, stored in row-major order
	mutable bool definitelyIdentityMatrix; // flag is this matrix is identity matrix
};

// Default constructor
inline MTX4::MTX4(eConstructor constructor) : definitelyIdentityMatrix(false)
{
	switch (constructor)
	{
	case EM4CONST_NOTHING:
	case EM4CONST_COPY:
		break;
	case EM4CONST_IDENTITY:
	case EM4CONST_INVERSE:
	default:
		makeIdentity();
		break;
	}
}

// Copy constructor
inline MTX4::MTX4(const MTX4 &other, eConstructor constructor) : definitelyIdentityMatrix(false)
{
	switch (constructor)
	{
	case EM4CONST_IDENTITY:
		makeIdentity();
		break;
	case EM4CONST_NOTHING:
		break;
	case EM4CONST_COPY:
		*this = other;
		break;
	case EM4CONST_TRANSPOSED:
		other.getTransposed(*this);
		break;
	case EM4CONST_INVERSE:
		if (!other.getInverse(*this))
			memset(M, 0, 16 * sizeof(float));
		break;
	case EM4CONST_INVERSE_TRANSPOSED:
		if (!other.getInverse(*this))
			memset(M, 0, 16 * sizeof(float));
		else
			*this = getTransposed();
		break;
	}
}

//! Add another matrix.
inline MTX4 MTX4::operator+(const MTX4 &other) const
{
	MTX4 temp(EM4CONST_NOTHING);

	temp[0] = M[0] + other[0];
	temp[1] = M[1] + other[1];
	temp[2] = M[2] + other[2];
	temp[3] = M[3] + other[3];
	temp[4] = M[4] + other[4];
	temp[5] = M[5] + other[5];
	temp[6] = M[6] + other[6];
	temp[7] = M[7] + other[7];
	temp[8] = M[8] + other[8];
	temp[9] = M[9] + other[9];
	temp[10] = M[10] + other[10];
	temp[11] = M[11] + other[11];
	temp[12] = M[12] + other[12];
	temp[13] = M[13] + other[13];
	temp[14] = M[14] + other[14];
	temp[15] = M[15] + other[15];

	return temp;
}

//! Add another matrix.
inline MTX4 &MTX4::operator+=(const MTX4 &other)
{
	M[0] += other[0];
	M[1] += other[1];
	M[2] += other[2];
	M[3] += other[3];
	M[4] += other[4];
	M[5] += other[5];
	M[6] += other[6];
	M[7] += other[7];
	M[8] += other[8];
	M[9] += other[9];
	M[10] += other[10];
	M[11] += other[11];
	M[12] += other[12];
	M[13] += other[13];
	M[14] += other[14];
	M[15] += other[15];

	return *this;
}

//! Subtract another matrix.
inline MTX4 MTX4::operator-(const MTX4 &other) const
{
	MTX4 temp(EM4CONST_NOTHING);

	temp[0] = M[0] - other[0];
	temp[1] = M[1] - other[1];
	temp[2] = M[2] - other[2];
	temp[3] = M[3] - other[3];
	temp[4] = M[4] - other[4];
	temp[5] = M[5] - other[5];
	temp[6] = M[6] - other[6];
	temp[7] = M[7] - other[7];
	temp[8] = M[8] - other[8];
	temp[9] = M[9] - other[9];
	temp[10] = M[10] - other[10];
	temp[11] = M[11] - other[11];
	temp[12] = M[12] - other[12];
	temp[13] = M[13] - other[13];
	temp[14] = M[14] - other[14];
	temp[15] = M[15] - other[15];

	return temp;
}

//! Subtract another matrix.
inline MTX4 &MTX4::operator-=(const MTX4 &other)
{
	M[0] -= other[0];
	M[1] -= other[1];
	M[2] -= other[2];
	M[3] -= other[3];
	M[4] -= other[4];
	M[5] -= other[5];
	M[6] -= other[6];
	M[7] -= other[7];
	M[8] -= other[8];
	M[9] -= other[9];
	M[10] -= other[10];
	M[11] -= other[11];
	M[12] -= other[12];
	M[13] -= other[13];
	M[14] -= other[14];
	M[15] -= other[15];

	return *this;
}

//! Multiply by scalar.
inline MTX4 MTX4::operator*(const float &scalar) const
{
	MTX4 temp(EM4CONST_NOTHING);

	temp[0] = M[0] * scalar;
	temp[1] = M[1] * scalar;
	temp[2] = M[2] * scalar;
	temp[3] = M[3] * scalar;
	temp[4] = M[4] * scalar;
	temp[5] = M[5] * scalar;
	temp[6] = M[6] * scalar;
	temp[7] = M[7] * scalar;
	temp[8] = M[8] * scalar;
	temp[9] = M[9] * scalar;
	temp[10] = M[10] * scalar;
	temp[11] = M[11] * scalar;
	temp[12] = M[12] * scalar;
	temp[13] = M[13] * scalar;
	temp[14] = M[14] * scalar;
	temp[15] = M[15] * scalar;

	return temp;
}

//! Multiply by scalar.
inline MTX4 &MTX4::operator*=(const float &scalar)
{
	M[0] *= scalar;
	M[1] *= scalar;
	M[2] *= scalar;
	M[3] *= scalar;
	M[4] *= scalar;
	M[5] *= scalar;
	M[6] *= scalar;
	M[7] *= scalar;
	M[8] *= scalar;
	M[9] *= scalar;
	M[10] *= scalar;
	M[11] *= scalar;
	M[12] *= scalar;
	M[13] *= scalar;
	M[14] *= scalar;
	M[15] *= scalar;

	return *this;
}

//! Multiply by another matrix.
inline MTX4 &MTX4::operator*=(const MTX4 &other)
{
	// do checks on your own in order to avoid copy creation
	if (!other.getDefinitelyIdentityMatrix())
	{
		if (this->getDefinitelyIdentityMatrix())
		{
			return (*this = other);
		}
		else
		{
			MTX4 temp(*this);
			return setbyproduct_nocheck(temp, other);
		}
	}

	return *this;
}

//! Multiply by another matrix.
// set this matrix to the product of two other matrices
// goal is to reduce stack use and copy
inline MTX4 &MTX4::setbyproduct_nocheck(const MTX4 &other_a, const MTX4 &other_b)
{
	const float *m1 = other_a.M;
	const float *m2 = other_b.M;

	rowMatrixProduct(M, m1, m2);

	definitelyIdentityMatrix = false;
	return *this;
}

//! Multiply by another matrix.
// set this matrix to the product of two other matrices
// goal is to reduce stack use and copy
inline MTX4 &MTX4::setbyproduct(const MTX4 &other_a, const MTX4 &other_b)
{
	if (other_a.getDefinitelyIdentityMatrix())
		return (*this = other_b);
	else if (other_b.getDefinitelyIdentityMatrix())
		return (*this = other_a);
	else
		return setbyproduct_nocheck(other_a, other_b);
}

//! Multiply by another matrix.
inline MTX4 MTX4::operator*(const MTX4 &m2) const
{
	// Testing purpose..
	if (this->getDefinitelyIdentityMatrix())
		return m2;
	if (m2.getDefinitelyIdentityMatrix())
		return *this;

	MTX4 m3(EM4CONST_NOTHING);

	const float *m1 = M;

	m3[0] = m1[0] * m2[0] + m1[4] * m2[1] + m1[8] * m2[2] + m1[12] * m2[3];
	m3[1] = m1[1] * m2[0] + m1[5] * m2[1] + m1[9] * m2[2] + m1[13] * m2[3];
	m3[2] = m1[2] * m2[0] + m1[6] * m2[1] + m1[10] * m2[2] + m1[14] * m2[3];
	m3[3] = m1[3] * m2[0] + m1[7] * m2[1] + m1[11] * m2[2] + m1[15] * m2[3];

	m3[4] = m1[0] * m2[4] + m1[4] * m2[5] + m1[8] * m2[6] + m1[12] * m2[7];
	m3[5] = m1[1] * m2[4] + m1[5] * m2[5] + m1[9] * m2[6] + m1[13] * m2[7];
	m3[6] = m1[2] * m2[4] + m1[6] * m2[5] + m1[10] * m2[6] + m1[14] * m2[7];
	m3[7] = m1[3] * m2[4] + m1[7] * m2[5] + m1[11] * m2[6] + m1[15] * m2[7];

	m3[8] = m1[0] * m2[8] + m1[4] * m2[9] + m1[8] * m2[10] + m1[12] * m2[11];
	m3[9] = m1[1] * m2[8] + m1[5] * m2[9] + m1[9] * m2[10] + m1[13] * m2[11];
	m3[10] = m1[2] * m2[8] + m1[6] * m2[9] + m1[10] * m2[10] + m1[14] * m2[11];
	m3[11] = m1[3] * m2[8] + m1[7] * m2[9] + m1[11] * m2[10] + m1[15] * m2[11];

	m3[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[8] * m2[14] + m1[12] * m2[15];
	m3[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[9] * m2[14] + m1[13] * m2[15];
	m3[14] = m1[2] * m2[12] + m1[6] * m2[13] + m1[10] * m2[14] + m1[14] * m2[15];
	m3[15] = m1[3] * m2[12] + m1[7] * m2[13] + m1[11] * m2[14] + m1[15] * m2[15];

	return m3;
}

//! Multiply by another matrix
inline MTX4 MTX4::mult34(const MTX4 &m2) const
{
	MTX4 out;
	this->mult34(m2, out);
	return out;
}

//! Multiply by another matrix.
inline MTX4 &MTX4::mult34(const MTX4 &m2, MTX4 &out) const
{
	// Testing purpose..
	if (getDefinitelyIdentityMatrix())
	{
		out = m2;
		return out;
	}

	if (m2.getDefinitelyIdentityMatrix())
	{
		out = *this;
		return out;
	}

	const float *m1 = M;

	out.M[0] = m1[0] * m2[0] + m1[4] * m2[1] + m1[8] * m2[2];
	out.M[1] = m1[1] * m2[0] + m1[5] * m2[1] + m1[9] * m2[2];
	out.M[2] = m1[2] * m2[0] + m1[6] * m2[1] + m1[10] * m2[2];
	out.M[3] = 0.0f;

	out.M[4] = m1[0] * m2[4] + m1[4] * m2[5] + m1[8] * m2[6];
	out.M[5] = m1[1] * m2[4] + m1[5] * m2[5] + m1[9] * m2[6];
	out.M[6] = m1[2] * m2[4] + m1[6] * m2[5] + m1[10] * m2[6];
	out.M[7] = 0.0f;

	out.M[8] = m1[0] * m2[8] + m1[4] * m2[9] + m1[8] * m2[10];
	out.M[9] = m1[1] * m2[8] + m1[5] * m2[9] + m1[9] * m2[10];
	out.M[10] = m1[2] * m2[8] + m1[6] * m2[9] + m1[10] * m2[10];
	out.M[11] = 0.0f;

	out.M[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[8] * m2[14] + m1[12];
	out.M[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[9] * m2[14] + m1[13];
	out.M[14] = m1[2] * m2[12] + m1[6] * m2[13] + m1[10] * m2[14] + m1[14];
	out.M[15] = 1.0f;

	out.definitelyIdentityMatrix = false;
	return out;
}

inline VEC3 MTX4::getColumn(unsigned int c) const
{
	const float *v = &M[c * 4];
	return VEC3(v[0], v[1], v[2]);
}

inline MTX4 &MTX4::setColumn(unsigned int c, const VEC3 &v)
{
	float *dst = &M[c * 4];
	dst[0] = v.X;
	dst[1] = v.Y;
	dst[2] = v.Z;
	return *this;
}

inline VEC3 MTX4::getTranslation() const
{
	return VEC3(M[12], M[13], M[14]);
}

inline MTX4 &MTX4::setTranslation(const VEC3 &translation)
{
	M[12] = translation.X;
	M[13] = translation.Y;
	M[14] = translation.Z;
	definitelyIdentityMatrix = false;
	return *this;
}

inline MTX4 &MTX4::setInverseTranslation(const VEC3 &translation)
{
	M[12] = -translation.X;
	M[13] = -translation.Y;
	M[14] = -translation.Z;
	definitelyIdentityMatrix = false;
	return *this;
}

inline MTX4 &MTX4::setScale(const VEC3 &scale)
{
	M[0] = scale.X;
	M[5] = scale.Y;
	M[10] = scale.Z;
	definitelyIdentityMatrix = false;
	return *this;
}

inline MTX4 &MTX4::preScale(const VEC3 &scale)
{
	if (definitelyIdentityMatrix)
	{
		setScale(scale);
	}
	else
	{
		M[0] *= scale.X;
		M[1] *= scale.Y;
		M[2] *= scale.Z;

		M[4] *= scale.X;
		M[5] *= scale.Y;
		M[6] *= scale.Z;

		M[8] *= scale.X;
		M[9] *= scale.Y;
		M[10] *= scale.Z;

		M[12] *= scale.X;
		M[13] *= scale.Y;
		M[14] *= scale.Z;

		definitelyIdentityMatrix = false;
	}
	return *this;
}

inline MTX4 &MTX4::postScale(const VEC3 &scale)
{
	if (definitelyIdentityMatrix)
	{
		setScale(scale);
	}
	else
	{
		M[0] *= scale.X;
		M[1] *= scale.X;
		M[2] *= scale.X;

		M[4] *= scale.Y;
		M[5] *= scale.Y;
		M[6] *= scale.Y;

		M[8] *= scale.Z;
		M[9] *= scale.Z;
		M[10] *= scale.Z;

		definitelyIdentityMatrix = false;
	}

	return *this;
}

inline VEC3 MTX4::getScale() const
{
	VEC3 vScale;
	vScale.X = VEC3(M[0], M[1], M[2]).getLength();
	vScale.Y = VEC3(M[4], M[5], M[6]).getLength();
	vScale.Z = VEC3(M[8], M[9], M[10]).getLength();
	return vScale;
}

inline MTX4 &MTX4::setRotationDegrees(const VEC3 &rotation)
{
	return setRotationRadians(rotation * DEGTORAD);
}

inline MTX4 &MTX4::setInverseRotationDegrees(const VEC3 &rotation)
{
	return setInverseRotationRadians(rotation * DEGTORAD);
}

inline MTX4 &MTX4::setRotationRadians(const VEC3 &rotation)
{
	const double cr = cos(rotation.X);
	const double sr = sin(rotation.X);
	const double cp = cos(rotation.Y);
	const double sp = sin(rotation.Y);
	const double cy = cos(rotation.Z);
	const double sy = sin(rotation.Z);

	M[0] = (float)(cp * cy);
	M[1] = (float)(cp * sy);
	M[2] = (float)(-sp);

	const double srsp = sr * sp;
	const double crsp = cr * sp;

	M[4] = (float)(srsp * cy - cr * sy);
	M[5] = (float)(srsp * sy + cr * cy);
	M[6] = (float)(sr * cp);

	M[8] = (float)(crsp * cy + sr * sy);
	M[9] = (float)(crsp * sy - sr * cy);
	M[10] = (float)(cr * cp);

	definitelyIdentityMatrix = false;
	return *this;
}

//! Returns the rotation, as set by setRotation().
inline VEC3 MTX4::getRotationDegrees() const
{
	const MTX4 &mat = *this;

	double Y = -asin(mat(0, 2));
	const double C = cos(Y);
	Y *= RADTODEG64;

	double rotx, roty, X, Z;

	if (fabs(C) > ROUNDING_ERROR_64)
	{
		const float invC = (float)(1.0 / C);
		rotx = mat(2, 2) * invC;
		roty = mat(1, 2) * invC;
		X = atan2(roty, rotx) * RADTODEG64;
		rotx = mat(0, 0) * invC;
		roty = mat(0, 1) * invC;
		Z = atan2(roty, rotx) * RADTODEG64;
	}
	else
	{
		X = 0.0;
		rotx = mat(1, 1);
		roty = -mat(1, 0);
		Z = atan2(roty, rotx) * RADTODEG64;
	}

	if (X < 0.0)
		X += 360.0;
	if (Y < 0.0)
		Y += 360.0;
	if (Z < 0.0)
		Z += 360.0;

	return VEC3((float)X, (float)Y, (float)Z);
}

inline MTX4 &MTX4::setInverseRotationRadians(const VEC3 &rotation)
{
	double cr = cos(rotation.X);
	double sr = sin(rotation.X);
	double cp = cos(rotation.Y);
	double sp = sin(rotation.Y);
	double cy = cos(rotation.Z);
	double sy = sin(rotation.Z);

	M[0] = (float)(cp * cy);
	M[4] = (float)(cp * sy);
	M[8] = (float)(-sp);

	double srsp = sr * sp;
	double crsp = cr * sp;

	M[1] = (float)(srsp * cy - cr * sy);
	M[5] = (float)(srsp * sy + cr * cy);
	M[9] = (float)(sr * cp);

	M[2] = (float)(crsp * cy + sr * sy);
	M[6] = (float)(crsp * sy - sr * cy);
	M[10] = (float)(cr * cp);

	definitelyIdentityMatrix = false;
	return *this;
}

inline MTX4 &MTX4::makeIdentity()
{
	memset(M, 0, 16 * sizeof(float));
	M[0] = M[5] = M[10] = M[15] = (float)1;
	definitelyIdentityMatrix = true;
	return *this;
}

/*
  Check identity with epsilon
  solve floating range problems..
*/
inline bool MTX4::isIdentity() const
{
	if (definitelyIdentityMatrix)
		return true;
	if (!equals_float(M[0], (float)1) ||
		!equals_float(M[5], (float)1) ||
		!equals_float(M[10], (float)1) ||
		!equals_float(M[15], (float)1))
		return false;

	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			if ((j != i) && (!iszero_float((*this)(i, j))))
				return false;

	definitelyIdentityMatrix = true;
	return true;
}

/*
  Doesn't solve floating range problems..
  but takes care on +/- 0 on translation because we are changing it..
  reducing floating point branches
  but it needs the floats in memory..
*/
// #define IR(x) ((unsigned int &)(x))

inline unsigned int IR(float f)
{
	unsigned int i;
	std::memcpy(&i, &f, sizeof(float));
	return i;
}

inline bool MTX4::isIdentity_integer_base() const
{
	if (definitelyIdentityMatrix)
		return true;
	if (IR(M[0]) != F32_VALUE_1)
		return false;
	if (IR(M[1]) != 0)
		return false;
	if (IR(M[2]) != 0)
		return false;
	if (IR(M[3]) != 0)
		return false;

	if (IR(M[4]) != 0)
		return false;
	if (IR(M[5]) != F32_VALUE_1)
		return false;
	if (IR(M[6]) != 0)
		return false;
	if (IR(M[7]) != 0)
		return false;

	if (IR(M[8]) != 0)
		return false;
	if (IR(M[9]) != 0)
		return false;
	if (IR(M[10]) != F32_VALUE_1)
		return false;
	if (IR(M[11]) != 0)
		return false;

	if (IR(M[12]) != 0)
		return false;
	if (IR(M[13]) != 0)
		return false;
	if (IR(M[13]) != 0)
		return false;
	if (IR(M[15]) != F32_VALUE_1)
		return false;

	definitelyIdentityMatrix = true;
	return true;
}

inline void MTX4::rotateVect(VEC3 &vect) const
{
	VEC3 tmp = vect;
	vect.X = tmp.X * M[0] + tmp.Y * M[4] + tmp.Z * M[8];
	vect.Y = tmp.X * M[1] + tmp.Y * M[5] + tmp.Z * M[9];
	vect.Z = tmp.X * M[2] + tmp.Y * M[6] + tmp.Z * M[10];
}

// An alternate transform vector method, writing into a second vector.
inline void MTX4::rotateVect(VEC3 &out, const VEC3 &in) const
{
	out.X = in.X * M[0] + in.Y * M[4] + in.Z * M[8];
	out.Y = in.X * M[1] + in.Y * M[5] + in.Z * M[9];
	out.Z = in.X * M[2] + in.Y * M[6] + in.Z * M[10];
}

// An alternate transform vector method, writing into an array of 3 floats.
inline void MTX4::rotateVect(float *out, const VEC3 &in) const
{
	out[0] = in.X * M[0] + in.Y * M[4] + in.Z * M[8];
	out[1] = in.X * M[1] + in.Y * M[5] + in.Z * M[9];
	out[2] = in.X * M[2] + in.Y * M[6] + in.Z * M[10];
}

inline void MTX4::inverseRotateVect(VEC3 &vect) const
{
	VEC3 tmp = vect;
	vect.X = tmp.X * M[0] + tmp.Y * M[1] + tmp.Z * M[2];
	vect.Y = tmp.X * M[4] + tmp.Y * M[5] + tmp.Z * M[6];
	vect.Z = tmp.X * M[8] + tmp.Y * M[9] + tmp.Z * M[10];
}

inline void MTX4::transformVect(VEC3 &vect) const
{
	float vector[3];

	vector[0] = vect.X * M[0] + vect.Y * M[4] + vect.Z * M[8] + M[12];
	vector[1] = vect.X * M[1] + vect.Y * M[5] + vect.Z * M[9] + M[13];
	vector[2] = vect.X * M[2] + vect.Y * M[6] + vect.Z * M[10] + M[14];

	vect.X = vector[0];
	vect.Y = vector[1];
	vect.Z = vector[2];
}

inline void MTX4::transformVect2D(VEC3 &vect) const
{
	float vector[2];

	vector[0] = vect.X * M[0] + vect.Y * M[4] + M[12];
	vector[1] = vect.X * M[1] + vect.Y * M[5] + M[13];

	vect.X = vector[0];
	vect.Y = vector[1];
}

inline void MTX4::transformVect2D(VEC3 &out, const VEC3 &in) const
{
	out.X = in.X * M[0] + in.Y * M[4] + M[12];
	out.Y = in.X * M[1] + in.Y * M[5] + M[13];
}

inline void MTX4::transformVect(VEC3 &out, const VEC3 &in) const
{
	out.X = in.X * M[0] + in.Y * M[4] + in.Z * M[8] + M[12];
	out.Y = in.X * M[1] + in.Y * M[5] + in.Z * M[9] + M[13];
	out.Z = in.X * M[2] + in.Y * M[6] + in.Z * M[10] + M[14];
}

inline void MTX4::transformVect(float *out, const VEC3 &in) const
{
	out[0] = in.X * M[0] + in.Y * M[4] + in.Z * M[8] + M[12];
	out[1] = in.X * M[1] + in.Y * M[5] + in.Z * M[9] + M[13];
	out[2] = in.X * M[2] + in.Y * M[6] + in.Z * M[10] + M[14];
	out[3] = in.X * M[3] + in.Y * M[7] + in.Z * M[11] + M[15];
}

// Transforms a axis aligned bounding box.
inline void MTX4::transformBox(AABB &box) const
{
	if (getDefinitelyIdentityMatrix())
		return;

	transformVect(box.MinEdge);
	transformVect(box.MaxEdge);
	box.repair();
}

// Transforms a axis aligned bounding box more accurately than transformBox().
inline void MTX4::transformBoxEx(AABB &box) const
{
	const float Amin[3] = {box.MinEdge.X, box.MinEdge.Y, box.MinEdge.Z};
	const float Amax[3] = {box.MaxEdge.X, box.MaxEdge.Y, box.MaxEdge.Z};

	float Bmin[3];
	float Bmax[3];

	Bmin[0] = Bmax[0] = M[12];
	Bmin[1] = Bmax[1] = M[13];
	Bmin[2] = Bmax[2] = M[14];

	const MTX4 &m = *this;

	for (unsigned int i = 0; i < 3; ++i)
	{
		for (unsigned int j = 0; j < 3; ++j)
		{
			const float a = m(j, i) * Amin[j];
			const float b = m(j, i) * Amax[j];

			if (a < b)
			{
				Bmin[i] += a;
				Bmax[i] += b;
			}
			else
			{
				Bmin[i] += b;
				Bmax[i] += a;
			}
		}
	}

	box.MinEdge.X = Bmin[0];
	box.MinEdge.Y = Bmin[1];
	box.MinEdge.Z = Bmin[2];

	box.MaxEdge.X = Bmax[0];
	box.MaxEdge.Y = Bmax[1];
	box.MaxEdge.Z = Bmax[2];
}

// Multiplies this matrix by a 1x4 matrix.
inline void MTX4::multiplyWith1x4Matrix(float *matrix) const
{
	/*
	  0  1  2  3
	  4  5  6  7
	  8  9  10 11
	  12 13 14 15
	*/

	float mat[4];
	mat[0] = matrix[0];
	mat[1] = matrix[1];
	mat[2] = matrix[2];
	mat[3] = matrix[3];

	matrix[0] = M[0] * mat[0] + M[4] * mat[1] + M[8] * mat[2] + M[12] * mat[3];
	matrix[1] = M[1] * mat[0] + M[5] * mat[1] + M[9] * mat[2] + M[13] * mat[3];
	matrix[2] = M[2] * mat[0] + M[6] * mat[1] + M[10] * mat[2] + M[14] * mat[3];
	matrix[3] = M[3] * mat[0] + M[7] * mat[1] + M[11] * mat[2] + M[15] * mat[3];
}

inline void MTX4::inverseTranslateVect(VEC3 &vect) const
{
	vect.X = vect.X - M[12];
	vect.Y = vect.Y - M[13];
	vect.Z = vect.Z - M[14];
}

inline void MTX4::translateVect(VEC3 &vect) const
{
	vect.X = vect.X + M[12];
	vect.Y = vect.Y + M[13];
	vect.Z = vect.Z + M[14];
}

inline bool MTX4::getInverse(MTX4 &out) const
{
	if (this->getDefinitelyIdentityMatrix())
	{
		out = *this;
		return true;
	}

	// Cramer's rule.
	float t0 = M[10] * M[15] - M[11] * M[14];
	float t1 = M[6] * M[15] - M[7] * M[14];
	float t2 = M[6] * M[11] - M[7] * M[10];
	float t3 = M[2] * M[15] - M[3] * M[14];
	float t4 = M[2] * M[11] - M[3] * M[10];
	float t5 = M[2] * M[7] - M[3] * M[6];

	float t6 = M[8] * M[13] - M[9] * M[12];
	float t7 = M[4] * M[13] - M[5] * M[12];
	float t8 = M[4] * M[9] - M[5] * M[8];
	float t9 = M[0] * M[13] - M[1] * M[12];
	float t10 = M[0] * M[9] - M[1] * M[8];
	float t11 = M[0] * M[5] - M[1] * M[4];

	float det = t0 * t11 - t1 * t10 + t2 * t9 + t3 * t8 - t4 * t7 + t5 * t6;

	if (iszero_float(det))
		return false;

	out.M[0] = M[5] * t0 - M[9] * t1 + M[13] * t2;
	out.M[1] = M[9] * t3 - M[1] * t0 - M[13] * t4;
	out.M[2] = M[1] * t1 - M[5] * t3 + M[13] * t5;
	out.M[3] = M[5] * t4 - M[1] * t2 - M[9] * t5;

	out.M[4] = M[8] * t1 - M[4] * t0 - M[12] * t2;
	out.M[5] = M[0] * t0 - M[8] * t3 + M[12] * t4;
	out.M[6] = M[4] * t3 - M[0] * t1 - M[12] * t5;
	out.M[7] = M[0] * t2 - M[4] * t4 + M[8] * t5;

	out.M[8] = M[7] * t6 - M[11] * t7 + M[15] * t8;
	out.M[9] = M[11] * t9 - M[3] * t6 - M[15] * t10;
	out.M[10] = M[3] * t7 - M[7] * t9 + M[15] * t11;
	out.M[11] = M[7] * t10 - M[3] * t8 - M[11] * t11;

	out.M[12] = M[10] * t7 - M[6] * t6 - M[14] * t8;
	out.M[13] = M[2] * t6 - M[10] * t9 + M[14] * t10;
	out.M[14] = M[6] * t9 - M[2] * t7 - M[14] * t11;
	out.M[15] = M[2] * t8 - M[6] * t10 + M[10] * t11;

	det = 1.0f / sqrtf(det);
	for (int i = 0; i < 16; ++i)
		out.M[i] *= det;

	out.definitelyIdentityMatrix = definitelyIdentityMatrix;
	return true;
}

/** Inverts a primitive matrix which only contains a translation and a rotation
	\param out: where result matrix is written to.*/
inline bool MTX4::getInversePrimitive(MTX4 &out) const
{
	out.M[0] = M[0];
	out.M[1] = M[4];
	out.M[2] = M[8];
	out.M[3] = 0;

	out.M[4] = M[1];
	out.M[5] = M[5];
	out.M[6] = M[9];
	out.M[7] = 0;

	out.M[8] = M[2];
	out.M[9] = M[6];
	out.M[10] = M[10];
	out.M[11] = 0;

	out.M[12] = (float)-(M[12] * M[0] + M[13] * M[1] + M[14] * M[2]);
	out.M[13] = (float)-(M[12] * M[4] + M[13] * M[5] + M[14] * M[6]);
	out.M[14] = (float)-(M[12] * M[8] + M[13] * M[9] + M[14] * M[10]);
	out.M[15] = 1;

	out.definitelyIdentityMatrix = definitelyIdentityMatrix;
	return true;
}

inline bool MTX4::makeInverse()
{
	if (definitelyIdentityMatrix)
		return true;

	MTX4 temp(EM4CONST_NOTHING);

	if (getInverse(temp))
	{
		*this = temp;
		return true;
	}

	return false;
}

inline MTX4 &MTX4::operator=(const float &scalar)
{
	for (int i = 0; i < 16; ++i)
		M[i] = scalar;

	definitelyIdentityMatrix = false;
	return *this;
}

inline bool MTX4::operator==(const MTX4 &other) const
{
	if (definitelyIdentityMatrix && other.definitelyIdentityMatrix)
		return true;
	for (int i = 0; i < 16; ++i)
		if (M[i] != other.M[i])
			return false;

	return true;
}

inline bool MTX4::operator!=(const MTX4 &other) const
{
	return !(*this == other);
}

// Builds a left-handed perspective projection matrix based on a field of view.
inline MTX4 &MTX4::buildProjectionMatrixPerspectiveFov(
	float fieldOfViewRadians, float aspectRatio, float zNear, float zFar)
{
	const double h = 1.0 / tan(fieldOfViewRadians / 2.0);
	const float w = (float)(h / aspectRatio);

	M[0] = w;
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (float)h;
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	M[10] = (float)(zFar / (zFar - zNear));
	M[11] = 1;

	M[12] = 0;
	M[13] = 0;
	M[14] = (float)(-zNear * zFar / (zFar - zNear));
	M[15] = 0;

	definitelyIdentityMatrix = false;
	return *this;
}

// Builds centered a left-handed orthogonal projection matrix.
inline MTX4 &MTX4::buildProjectionMatrixOrtho(
	float widthOfViewVolume, float heightOfViewVolume, float zNear, float zFar)
{
	M[0] = (float)(2 / widthOfViewVolume);
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (float)(2 / heightOfViewVolume);
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	M[10] = (float)(1 / (zFar - zNear));
	M[11] = 0;

	M[12] = 0;
	M[13] = 0;
	M[14] = (float)(zNear / (zNear - zFar));
	M[15] = 1;

	definitelyIdentityMatrix = false;
	return *this;
}

// Builds a left-handed orthogonal projection matrix.
inline MTX4 &MTX4::buildProjectionMatrixOrtho(
	float left, float right, float bottom, float top, float zNear, float zFar)
{
	float w = right - left;
	float h = top - bottom;
	M[0] = (float)(2 / w);
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (float)(2 / h);
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	M[10] = (float)(1 / (zFar - zNear));
	M[11] = 0;

	M[12] = -(left + right) / w;
	M[13] = -(bottom + top) / h;
	M[14] = (float)(zNear / (zNear - zFar));
	M[15] = 1;

	definitelyIdentityMatrix = false;
	return *this;
}

// Builds a left-handed perspective projection matrix.
inline MTX4 &MTX4::buildProjectionMatrixPerspective(
	float widthOfViewVolume, float heightOfViewVolume, float zNear, float zFar)
{
	M[0] = (float)(2 * zNear / widthOfViewVolume);
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (float)(2 * zNear / heightOfViewVolume);
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	M[10] = (float)(zFar / (zFar - zNear));
	M[11] = 1;

	M[12] = 0;
	M[13] = 0;
	M[14] = (float)(zNear * zFar / (zNear - zFar));
	M[15] = 0;

	definitelyIdentityMatrix = false;
	return *this;
}

// Builds a left-handed look-at matrix.
inline MTX4 &MTX4::buildCameraLookAtMatrix(
	const VEC3 &position,
	const VEC3 &target,
	const VEC3 &upVector)
{
	VEC3 zaxis = target - position;
	zaxis.normalize();

	VEC3 xaxis = upVector.crossProduct(zaxis);
	xaxis.normalize();

	VEC3 yaxis = zaxis.crossProduct(xaxis);

	M[0] = (float)xaxis.X;
	M[1] = (float)yaxis.X;
	M[2] = (float)zaxis.X;
	M[3] = 0;

	M[4] = (float)xaxis.Y;
	M[5] = (float)yaxis.Y;
	M[6] = (float)zaxis.Y;
	M[7] = 0;

	M[8] = (float)xaxis.Z;
	M[9] = (float)yaxis.Z;
	M[10] = (float)zaxis.Z;
	M[11] = 0;

	M[12] = (float)-xaxis.dotProduct(position);
	M[13] = (float)-yaxis.dotProduct(position);
	M[14] = (float)-zaxis.dotProduct(position);
	M[15] = 1;

	definitelyIdentityMatrix = false;
	return *this;
}

// Creates a new matrix as interpolated matrix from this and the passed one.
inline MTX4 MTX4::interpolate(const MTX4 &b, float time) const
{
	MTX4 mat(EM4CONST_NOTHING);

	for (unsigned int i = 0; i < 16; i += 4)
	{
		mat.M[i + 0] = (float)(M[i + 0] + (b.M[i + 0] - M[i + 0]) * time);
		mat.M[i + 1] = (float)(M[i + 1] + (b.M[i + 1] - M[i + 1]) * time);
		mat.M[i + 2] = (float)(M[i + 2] + (b.M[i + 2] - M[i + 2]) * time);
		mat.M[i + 3] = (float)(M[i + 3] + (b.M[i + 3] - M[i + 3]) * time);
	}

	return mat;
}

// Returns transposed matrix.
inline MTX4 MTX4::getTransposed() const
{
	MTX4 t(EM4CONST_NOTHING);
	getTransposed(t);
	return t;
}

// Returns transposed matrix.
inline void MTX4::getTransposed(MTX4 &o) const
{
	o.M[0] = M[0];
	o.M[1] = M[4];
	o.M[2] = M[8];
	o.M[3] = M[12];

	o.M[4] = M[1];
	o.M[5] = M[5];
	o.M[6] = M[9];
	o.M[7] = M[13];

	o.M[8] = M[2];
	o.M[9] = M[6];
	o.M[10] = M[10];
	o.M[11] = M[14];

	o.M[12] = M[3];
	o.M[13] = M[7];
	o.M[14] = M[11];
	o.M[15] = M[15];

	o.definitelyIdentityMatrix = definitelyIdentityMatrix;
}

// Rotate about z axis, center ( 0.5, 0.5 ).
inline MTX4 &MTX4::setTextureRotationCenter(float rotateRad)
{
	const float c = cosf(rotateRad);
	const float s = sinf(rotateRad);
	M[0] = (float)c;
	M[1] = (float)s;

	M[4] = (float)-s;
	M[5] = (float)c;

	M[8] = (float)(0.5f * (s - c) + 0.5f);
	M[9] = (float)(-0.5f * (s + c) + 0.5f);
	definitelyIdentityMatrix = definitelyIdentityMatrix && (rotateRad == 0.0f);
	return *this;
}

inline MTX4 &MTX4::setTextureTranslate(float x, float y)
{
	M[8] = (float)x;
	M[9] = (float)y;
	definitelyIdentityMatrix = definitelyIdentityMatrix && (x == 0.0f) && (y == 0.0f);
	return *this;
}

inline MTX4 &MTX4::setTextureTranslateTransposed(float x, float y)
{
	M[2] = (float)x;
	M[6] = (float)y;
	definitelyIdentityMatrix = definitelyIdentityMatrix && (x == 0.0f) && (y == 0.0f);
	return *this;
}

inline MTX4 &MTX4::setTextureScale(float sx, float sy)
{
	M[0] = (float)sx;
	M[5] = (float)sy;
	definitelyIdentityMatrix = definitelyIdentityMatrix && (sx == 1.0f) && (sy == 1.0f);
	return *this;
}

inline MTX4 &MTX4::postTextureScale(float sx, float sy)
{
	M[0] *= (float)sx;
	M[1] *= (float)sx;
	M[4] *= (float)sy;
	M[5] *= (float)sy;
	definitelyIdentityMatrix = definitelyIdentityMatrix && (sx == 1.0f) && (sy == 1.0f);
	return *this;
}

inline MTX4 &MTX4::setTextureScaleCenter(float sx, float sy)
{
	M[0] = (float)sx;
	M[5] = (float)sy;
	M[8] = (float)(0.5f - 0.5f * sx);
	M[9] = (float)(0.5f - 0.5f * sy);
	definitelyIdentityMatrix = definitelyIdentityMatrix && (sx == 1.0f) && (sy == 1.0f);
	return *this;
}

// Sets all matrix data members at once.
inline MTX4 &MTX4::setM(const float *data)
{
	memcpy(M, data, 16 * sizeof(float));

	definitelyIdentityMatrix = false;
	return *this;
}

// Gets all matrix data members at once.
inline float *MTX4::getM(float *data) const
{
	memcpy(data, M, 16 * sizeof(float));
	return data;
}

// Sets if the matrix is definitely identity matrix.
inline void MTX4::setDefinitelyIdentityMatrix(bool isDefinitelyIdentityMatrix)
{
	definitelyIdentityMatrix = isDefinitelyIdentityMatrix;
}

// Checks if the matrix is definitely identity matrix.
inline bool MTX4::getDefinitelyIdentityMatrix() const
{
	return definitelyIdentityMatrix;
}

// Multiply by scalar.
inline MTX4 operator*(const float scalar, const MTX4 &mat) { return mat * scalar; }

extern const MTX4 IdentityMatrix;

#endif
