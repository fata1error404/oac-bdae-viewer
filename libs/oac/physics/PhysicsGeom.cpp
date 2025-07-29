// PhysicsGeom.cpp: implementation of the CPhysicsGeom class.
//
//////////////////////////////////////////////////////////////////////
//#include "preHeader.h"
#include "PhysicsGeom.h"
// #include "physics.h"

// #ifdef _DEBUG
// #undef THIS_FILE
// static char THIS_FILE[]=__FILE__;
// #define new DEBUG_NEW
// #endif

CPhysicsGeom::_PHYSICS_FACE_CULLING CPhysicsGeom::s_faceCulling = CPhysicsGeom::PHYSICS_BACK_FACE_CULLING;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
_Intersect_Info::_Intersect_Info()
	: pGeom(NULL)
//, pActor(NULL)
{
}

CPhysicsGeom::CPhysicsGeom(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL)

	: /*m_speed(0, 0, 0)*/
	  /*, m_scale(1.0f)
	  ,*/
	  m_pActor(NULL), m_pNext(NULL), m_pos(pos), m_rotY(rotY), m_pTrigger(NULL), m_absScale(1.0f, 1.0f, 1.0f), m_invAbsScale(1.0f, 1.0f, 1.0f)
{
	// m_pNext = NULL;
	m_info.s32value = 0;
	// m_rotY = 0;
	// m_cosRotY = 1.0;
	// m_sinRotY = 0;
	// m_cosRotY = CosDeg(m_rotY);
	// m_sinRotY = SinDeg(m_rotY);
	m_geomType = 0;
	// m_bPGFollowDummy = false;
	SetSize(halfW, halfH, halfL);
}

CPhysicsGeom::~CPhysicsGeom()
{
}

// Must fill theSegment.seg.P0 and theSegment.seg.P1 before calling this function
void CPhysicsGeom::ComputeIntersectLine(_Intersect_Segment &theSegment)
{
	theSegment.dir = theSegment.seg.P1 - theSegment.seg.P0;

	theSegment.len = sqrt(theSegment.dir.getLengthSQ());

	theSegment.dir.X = (theSegment.dir.X) / theSegment.len;
	theSegment.dir.Y = (theSegment.dir.Y) / theSegment.len;
	theSegment.dir.Z = (theSegment.dir.Z) / theSegment.len;

	UpdateIntersectLineAABB(theSegment);
}

void CPhysicsGeom::CalSegmentAABB(const VEC3 &P0, const VEC3 &P1, AABB &aabb)
{
	if (P0.X < P1.X)
	{
		aabb.MinEdge.X = P0.X;
		aabb.MaxEdge.X = P1.X;
	}
	else
	{
		aabb.MinEdge.X = P1.X;
		aabb.MaxEdge.X = P0.X;
	}

	if (P0.Y < P1.Y)
	{
		aabb.MinEdge.Y = P0.Y;
		aabb.MaxEdge.Y = P1.Y;
	}
	else
	{
		aabb.MinEdge.Y = P1.Y;
		aabb.MaxEdge.Y = P0.Y;
	}

	if (P0.Z < P1.Z)
	{
		aabb.MinEdge.Z = P0.Z;
		aabb.MaxEdge.Z = P1.Z;
	}
	else
	{
		aabb.MinEdge.Z = P1.Z;
		aabb.MaxEdge.Z = P0.Z;
	}
}
void CPhysicsGeom::CalRayFromSegment(const _Intersect_Segment &theSegment, _Intersect_Ray &theRay)
{
	//	theRay.ray = RAY3(theSegment.seg.P0, theSegment.dir, true);
	theRay.ray.P = theSegment.seg.P0;
	theRay.ray.d = theSegment.dir;
	// theRay.ray.d.normalize();

	int majorDir = 0; // x major
	if (fabs(theSegment.dir.Y) > fabs(theSegment.dir.X))
	{
		if (fabs(theSegment.dir.Y) >= fabs(theSegment.dir.Z))
			majorDir = 1; // y major
		else
			majorDir = 2; // z major
	}
	else if (fabs(theSegment.dir.Z) > fabs(theSegment.dir.X))
		majorDir = 2; // z major
	theRay.major = majorDir;
	theRay.isAlongY = 0;
	if (majorDir == 1 && theSegment.dir.X == 0 && theSegment.dir.Z == 0)
		theRay.isAlongY = 1;
}

void CPhysicsGeom::SetSerilParentTransform(const MTX4 &transform, const VEC3 *scale /*= NULL*/)
{
	CPhysicsGeom *pCur = this;
	while (pCur)
	{
		pCur->SetParentTransform(transform, scale);
		if (pCur == this)
			m_groupAABB = pCur->m_AABB;
		else
		{
			m_groupAABB.addInternalBox(pCur->m_AABB);
		}
		pCur = pCur->m_pNext;
	}
	if (m_pTrigger)
		m_pTrigger->SetSerilParentTransform(transform, scale);
}

void CPhysicsGeom::SetParentTransform(const MTX4 &transform, const VEC3 *scale)
{
	m_info.parentTransformSet = 1;

	// Calculated the Local Transform
	m_absTransform.makeIdentity();
	m_absTransform.setRotationDegrees(VEC3(0, m_rotY, 0));
	m_absTransform.setTranslation(m_pos);
	m_absTransform = transform * m_absTransform;

	if (scale != NULL && m_absScale != *scale)
	{
		m_absScale = *scale;
		m_invAbsScale.X = 1.0f / m_absScale.X;
		m_invAbsScale.Y = 1.0f / m_absScale.Y;
		m_invAbsScale.Z = 1.0f / m_absScale.Z;
	}
#if 0
	VEC3 temp = m_absTransform.getColumn(0);
	m_absScale.X = temp.getLength();
	m_invAbsScale.X = 1.0f / m_absScale.X;
	temp = m_absTransform.getColumn(1);
	m_absScale.Y = temp.getLength();
	m_invAbsScale.Y = 1.0f / m_absScale.Y;

	temp = m_absTransform.getColumn(2);
	m_absScale.Z = temp.getLength();
	m_invAbsScale.Z = 1.0f / m_absScale.Z;
#endif

	m_absTransform.getInverse(m_invAbsTransform);
	UpdateAABB();
}

void CPhysicsGeom::TransformSegmentToLocalCoordinateSystem(const _Intersect_Segment &theSegment, _Intersect_Segment &outLocalSegment) const
{
	outLocalSegment = theSegment;

	m_invAbsTransform.transformVect(outLocalSegment.seg.P0, theSegment.seg.P0);
	m_invAbsTransform.transformVect(outLocalSegment.seg.P1, theSegment.seg.P1);
	ComputeIntersectLine(outLocalSegment);

	// outLocalSegment.seg.P0 -= m_pos;
	// outLocalSegment.seg.P1 -= m_pos;
	////if has rotation -- update the dir, v0, v1
	////to increase the speed, keep m_rotY to zero as possible.
	// if(m_rotY)
	//{
	//	if(IsAlongX()) //180 degree
	//	{
	//		outLocalSegment.seg.P0.X = -outLocalSegment.seg.P0.X;
	//		outLocalSegment.seg.P0.Z = -outLocalSegment.seg.P0.Z;
	//		outLocalSegment.seg.P1.X = -outLocalSegment.seg.P1.X;
	//		outLocalSegment.seg.P1.Z = -outLocalSegment.seg.P1.Z;
	//		outLocalSegment.dir.X = -outLocalSegment.dir.X;
	//		outLocalSegment.dir.Z = -outLocalSegment.dir.Z;
	//	}
	//	else if(IsAlongZ())
	//	{
	//		if(m_rotY == 90.0) //90degree
	//		{
	//
	//			f32 temp =outLocalSegment.seg.P0.X;
	//			outLocalSegment.seg.P0.X = outLocalSegment.seg.P0.Z;
	//			outLocalSegment.seg.P0.Z = -temp;
	//
	//			temp = outLocalSegment.seg.P1.X;
	//			outLocalSegment.seg.P1.X = outLocalSegment.seg.P1.Z;
	//			outLocalSegment.seg.P1.Z = -temp;

	//			temp = outLocalSegment.dir.X;
	//			outLocalSegment.dir.X = outLocalSegment.dir.Z;
	//			outLocalSegment.dir.Z = -temp;

	//		}
	//		else	//270 degree
	//		{
	//			f32 temp =outLocalSegment.seg.P0.X;
	//			outLocalSegment.seg.P0.X = - outLocalSegment.seg.P0.Z;
	//			outLocalSegment.seg.P0.Z = temp;
	//
	//			temp = outLocalSegment.seg.P1.X;
	//			outLocalSegment.seg.P1.X = - outLocalSegment.seg.P1.Z;
	//			outLocalSegment.seg.P1.Z = temp;

	//			temp = outLocalSegment.dir.X;
	//			outLocalSegment.dir.X = -outLocalSegment.dir.Z;
	//			outLocalSegment.dir.Z = temp;

	//		}

	//	}
	//	else
	//	{
	//		f32 x = (outLocalSegment.seg.P0.X * m_cosRotY +  outLocalSegment.seg.P0.Z * m_sinRotY)  ;
	//		outLocalSegment.seg.P0.Z = (outLocalSegment.seg.P0.Z * m_cosRotY -  outLocalSegment.seg.P0.X * m_sinRotY)  ;
	//		outLocalSegment.seg.P0.X = x;

	//		x = (outLocalSegment.seg.P1.X * m_cosRotY +  outLocalSegment.seg.P1.Z * m_sinRotY)  ;
	//		outLocalSegment.seg.P1.Z = (outLocalSegment.seg.P1.Z * m_cosRotY -  outLocalSegment.seg.P1.X * m_sinRotY)  ;
	//		outLocalSegment.seg.P1.X = x;

	//		x = (outLocalSegment.dir.X * m_cosRotY +  outLocalSegment.dir.Z * m_sinRotY)  ;
	//		outLocalSegment.dir.Z = (outLocalSegment.dir.Z * m_cosRotY -  outLocalSegment.dir.X * m_sinRotY)  ;
	//		outLocalSegment.dir.X = x;

	//	}
	//}
	////update AABB
	// outLocalSegment.aabb.MinEdge =
	//	VEC3(MIN(outLocalSegment.seg.P0.X, outLocalSegment.seg.P1.X),
	//	MIN(outLocalSegment.seg.P0.Y, outLocalSegment.seg.P1.Y),
	//	MIN(outLocalSegment.seg.P0.Z, outLocalSegment.seg.P1.Z));
	// outLocalSegment.aabb.MaxEdge =
	//	VEC3(MAX(outLocalSegment.seg.P0.X, outLocalSegment.seg.P1.X),
	//	MAX(outLocalSegment.seg.P0.Y, outLocalSegment.seg.P1.Y),
	//	MAX(outLocalSegment.seg.P0.Z, outLocalSegment.seg.P1.Z));
}

void CPhysicsGeom::UpdateAABB()
{
	GLL_ASSERT(m_info.parentTransformSet);

	m_AABB.MaxEdge.X = fabs(m_halfSize.X * m_absTransform[0]) +
					   fabs(m_halfSize.Y * m_absTransform[4]) +
					   fabs(m_halfSize.Z * m_absTransform[8]);
	m_AABB.MinEdge.X = -m_AABB.MaxEdge.X + m_absTransform[12];
	m_AABB.MaxEdge.X += m_absTransform[12];

	m_AABB.MaxEdge.Y = fabs(m_halfSize.X * m_absTransform[1]) +
					   fabs(m_halfSize.Y * m_absTransform[5]) +
					   fabs(m_halfSize.Z * m_absTransform[9]);
	m_AABB.MinEdge.Y = -m_AABB.MaxEdge.Y + m_absTransform[13];
	m_AABB.MaxEdge.Y += m_absTransform[13];

	m_AABB.MaxEdge.Z = fabs(m_halfSize.X * m_absTransform[2]) +
					   fabs(m_halfSize.Y * m_absTransform[6]) +
					   fabs(m_halfSize.Z * m_absTransform[10]);
	m_AABB.MinEdge.Z = -m_AABB.MaxEdge.Z + m_absTransform[14];
	m_AABB.MaxEdge.Z += m_absTransform[14];
}

// bool CPhysicsGeom::Is2dBallIntersect2dLine(const M3DXVector2 &ballC, int ballR, const SEGMENT3_2d &theSegment, f32 &outA)
//{
//	if(( ballC.x + ballR < theSegment.minV.x) || ( ballC.x - ballR > theSegment.maxV.x) ||
//		( ballC.z + ballR < theSegment.minV.z) || ( ballC.z - ballR > theSegment.maxV.z) )
//		return false;
//
//
//	M3DXVector2 l = ballC - theSegment.P0;
//	M3DXVector2 vDir = theSegment.vDir;
//	int s = (vDir.x * l.x + vDir.z * l.z)  ;
//	int dist2 =l.x * l.x + l.z * l.z;
//	int r2 = ballR * ballR;
//	if(s < 0 && dist2 > r2)
//		return false;
//	int m2 = dist2 - s * s;
//	if(m2 < 0 || m2 > r2)
//		return false;
//
//	int q = FX_Sqrt(r2 - m2);
//	if(dist2 >= r2) //only test the line intersect from the outer
//	{
//		outA = (s - q);
//		if(outA > theSegment.len)
//			return false;
//		return true;
//	}
//
//	return false;
// }

bool CPhysicsGeom::IntersectionAABB(const AABB *pAABB1, const AABB *pAABB2) const
{
	// x,z plane
	// if(
	//	(pAABB1->MinEdge.X < pAABB2->MaxEdge.X && pAABB1->MinEdge.X > pAABB2->MinEdge.X) ||
	//	(pAABB1->MaxEdge.X > pAABB2->MinEdge.X && pAABB1->MaxEdge.X < pAABB2->MaxEdge.X) ||

	//	(pAABB1->MinEdge.Z < pAABB2->MaxEdge.Z && pAABB1->MinEdge.Z > pAABB2->MinEdge.Z) ||
	//	(pAABB1->MaxEdge.Z > pAABB2->MinEdge.Z && pAABB1->MaxEdge.Z < pAABB2->MaxEdge.Z) ||

	//	(pAABB1->MinEdge.X < pAABB2->MinEdge.X && pAABB1->MinEdge.Z < pAABB2->MinEdge.Z && pAABB1->MaxEdge.X > pAABB2->MaxEdge.X && pAABB1->MaxEdge.Z > pAABB2->MaxEdge.Z) ||
	//	(pAABB1->MinEdge.X > pAABB2->MinEdge.X && pAABB1->MinEdge.Z > pAABB2->MinEdge.Z && pAABB1->MaxEdge.X < pAABB2->MaxEdge.X && pAABB1->MaxEdge.Z < pAABB2->MaxEdge.Z)
	//	)
	//{
	//	if(
	//		(pAABB1->MinEdge.Y < pAABB2->MaxEdge.Y && pAABB1->MinEdge.Y > pAABB2->MinEdge.Y) ||
	//		(pAABB1->MaxEdge.Y > pAABB2->MinEdge.Y && pAABB1->MaxEdge.Y < pAABB2->MaxEdge.Y)
	//		)
	//		return true;
	//	else
	//		return false;
	//}
	// return false;

	if (pAABB1->MinEdge.X >= pAABB2->MaxEdge.X || pAABB1->MaxEdge.X <= pAABB2->MinEdge.X)
		return false;

	if (pAABB1->MinEdge.Z >= pAABB2->MaxEdge.Z || pAABB1->MaxEdge.Z <= pAABB2->MinEdge.Z)
		return false;

	if (pAABB1->MinEdge.Y >= pAABB2->MaxEdge.Y || pAABB1->MaxEdge.Y <= pAABB2->MinEdge.Y)
		return false;

	return true;
}

bool CPhysicsGeom::AABB_IntersectSegment(const _Intersect_Segment &theSegment) const
{
	if (IntersectionAABB(&theSegment.aabb, &m_AABB))
	{
		return true;
	}
	return false;
}

bool CPhysicsGeom::AABB_IntersectPoint(const VEC3 &pt) const
{
	if (pt.X > m_AABB.MaxEdge.X || pt.X < m_AABB.MinEdge.X ||
		pt.Y > m_AABB.MaxEdge.Y || pt.Y < m_AABB.MinEdge.Y ||
		pt.Z > m_AABB.MaxEdge.Z || pt.Z < m_AABB.MinEdge.Z)
		return false;
	return true;
}

// bool CPhysicsGeom::AABB_IntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed) const
//{
//	VEC3 comSpeed = speed - GetSpeed();
//	AABB aabb;
//	VEC3 endPos = pos + comSpeed;
//	CPhysics::CalSegmentAABB(pos, endPos, aabb);
//	aabb.MinEdge.X -= radius;
//	aabb.MinEdge.Y -= radius;
//	aabb.MinEdge.Z -= radius;
//	aabb.MaxEdge.X += radius;
//	aabb.MaxEdge.Y += radius;
//	aabb.MaxEdge.Z += radius;
//
//	//AABB TEST First
//	if(IntersectionAABB(&aabb, &m_AABB))
//		return true;
//	return false;
// }

bool CPhysicsGeom::AABB_IntersectAABB(const AABB &pAABB) const
{
	if (IntersectionAABB(&pAABB, &m_AABB))
	{
		return true;
	}
	return false;
}

void CPhysicsGeom::LocalToWorld(const VEC3 &localPos, VEC3 &worldPos) const
{
	m_absTransform.transformVect(worldPos, localPos);
	// if has rotation
	// to increase the speed, keep m_rotY to zero as possible.
	// if(m_rotY)
	//{
	//	if(IsAlongX()) //180 degree
	//	{
	//		worldPos.X = -localPos.X;
	//		worldPos.Z = -localPos.Z;
	//	}
	//	else if(IsAlongZ())
	//	{
	//		if(m_rotY == 90.0) //90degree
	//		{
	//			worldPos.X = -localPos.Z;
	//			worldPos.Z = localPos.X;
	//		}
	//		else	//270 degree
	//		{
	//			worldPos.X = localPos.Z;
	//			worldPos.Z = -localPos.X;
	//		}

	//	}
	//	else
	//	{
	//		worldPos.X = (localPos.X * m_cosRotY -  localPos.Z * m_sinRotY)  ;
	//		worldPos.Z = (localPos.Z * m_cosRotY +  localPos.X * m_sinRotY)  ;
	//	}
	//	worldPos.Y = localPos.Y;
	//	worldPos = worldPos + m_pos;
	//}
	// else
	//{
	//	worldPos = localPos + m_pos;
	//}
}

void CPhysicsGeom::WorldToLocal(const VEC3 &worldPos, VEC3 &localPos) const
{
	m_invAbsTransform.transformVect(localPos, worldPos);
	// localPos = worldPos - m_pos;
	////if has rotation
	////to increase the speed, keep m_rotY to zero as possible.
	// if(m_rotY)
	//{
	//	if(IsAlongX()) //180 degree
	//	{
	//		localPos.X = -localPos.X;
	//		localPos.Z = -localPos.Z;
	//	}
	//	else if(IsAlongZ())
	//	{
	//		if(m_rotY == 90.0) //90degree
	//		{
	//			float temp = localPos.X;
	//			localPos.X = localPos.Z;
	//			localPos.Z = -temp;
	//		}
	//		else	//270 degree
	//		{
	//			float temp = localPos.X;
	//			localPos.X = -localPos.Z;
	//			localPos.Z = temp;
	//		}

	//	}
	//	else
	//	{
	//		float x = (localPos.X * m_cosRotY +  localPos.Z * m_sinRotY)  ;
	//		localPos.Z = (localPos.Z * m_cosRotY -  localPos.X * m_sinRotY)  ;
	//		localPos.X = x;
	//	}
	//}
}

int CPhysicsGeom::IntersectionRay3AABB(const RAY3 *pRay, const AABB *pAABB, float *pDist, VEC3 *pPt, VEC3 *pNormal)
{

	// pRay->d���뵥һ��
	float outA;
	// TEST FRONT FACE First
	if (pRay->d.Z > 0 && pRay->P.Z <= pAABB->MinEdge.Z)
	{
		outA = (pAABB->MinEdge.Z - pRay->P.Z) / pRay->d.Z;
		float x = pRay->P.X + (outA * pRay->d.X);
		if (x <= pAABB->MaxEdge.X && x >= pAABB->MinEdge.X)
		{
			float y = pRay->P.Y + (outA * pRay->d.Y);
			if (y <= pAABB->MaxEdge.Y && y >= pAABB->MinEdge.Y)
			{
				*pDist = outA;
				if (pPt)
				{
					pPt->X = x;
					pPt->Y = y;
					pPt->Z = pRay->P.Z + (outA * pRay->d.Z);
					;
				}
				if (pNormal)
				{
					pNormal->X = pNormal->Y = 0;
					pNormal->Z = -1;
				}
				return 1;
			}
		}
	}

	// Test Back FACE
	if (pRay->d.Z < 0 && pRay->P.Z >= pAABB->MaxEdge.Z)
	{
		outA = (pAABB->MaxEdge.Z - pRay->P.Z) / pRay->d.Z;
		float x = pRay->P.X + (outA * pRay->d.X);
		if (x <= pAABB->MaxEdge.X && x >= pAABB->MinEdge.X)
		{
			float y = pRay->P.Y + (outA * pRay->d.Y);
			if (y <= pAABB->MaxEdge.Y && y >= pAABB->MinEdge.Y)
			{
				*pDist = outA;
				if (pPt)
				{
					pPt->X = x;
					pPt->Y = y;
					pPt->Z = pRay->P.Z + (outA * pRay->d.Z);
					;
				}
				if (pNormal)
				{
					pNormal->X = pNormal->Y = 0;
					pNormal->Z = 1;
				}
				return 2;
			}
		}
	}
	// Test Left FACE
	if (pRay->d.X > 0 && pRay->P.X <= pAABB->MinEdge.X)
	{
		outA = (pAABB->MinEdge.X - pRay->P.X) / pRay->d.X;
		float z = pRay->P.Z + (outA * pRay->d.Z);
		if (z <= pAABB->MaxEdge.Z && z >= pAABB->MinEdge.Z)
		{
			float y = pRay->P.Y + (outA * pRay->d.Y);
			if (y <= pAABB->MaxEdge.Y && y >= pAABB->MinEdge.Y)
			{
				*pDist = outA;
				if (pPt)
				{
					pPt->X = pRay->P.X + (outA * pRay->d.X);
					pPt->Y = y;
					pPt->Z = z;
				}
				if (pNormal)
				{
					pNormal->Y = pNormal->Z = 0;
					pNormal->X = -1;
				}
				return 3;
			}
		}
	}
	// Test Right FACE
	if (pRay->d.X < 0 && pRay->P.X >= pAABB->MaxEdge.X)
	{
		outA = (pAABB->MaxEdge.X - pRay->P.X) / pRay->d.X;
		float z = pRay->P.Z + (outA * pRay->d.Z);
		if (z <= pAABB->MaxEdge.Z && z >= pAABB->MinEdge.Z)
		{
			float y = pRay->P.Y + (outA * pRay->d.Y);
			if (y <= pAABB->MaxEdge.Y && y >= pAABB->MinEdge.Y)
			{
				*pDist = outA;
				if (pPt)
				{
					pPt->X = pRay->P.X + (outA * pRay->d.X);
					pPt->Y = y;
					pPt->Z = z;
				}
				if (pNormal)
				{
					pNormal->Y = pNormal->Z = 0;
					pNormal->X = 1;
				}
				return 4;
			}
		}
	}

	// Test TOP FACE
	if (pRay->d.Y < 0 && pRay->P.Y >= pAABB->MaxEdge.Y)
	{
		outA = (pAABB->MaxEdge.Y - pRay->P.Y) / pRay->d.Y;
		float x = pRay->P.X + (outA * pRay->d.X);
		if (x <= pAABB->MaxEdge.X && x >= pAABB->MinEdge.X)
		{
			float z = pRay->P.Z + (outA * pRay->d.Z);
			if (z <= pAABB->MaxEdge.Z && z >= pAABB->MinEdge.Z)
			{
				*pDist = outA;
				if (pPt)
				{
					pPt->X = x;
					pPt->Y = pRay->P.Y + (outA * pRay->d.Y);
					pPt->Z = z;
				}
				if (pNormal)
				{
					pNormal->X = pNormal->Z = 0;
					pNormal->Y = 1;
				}
				return 5;
			}
		}
	}
	//	Test Bottom Face
	if (pRay->d.Y > 0 && pRay->P.Y <= pAABB->MinEdge.Y)
	{
		outA = (pAABB->MinEdge.Y - pRay->P.Y) / pRay->d.Y;
		float x = pRay->P.X + (outA * pRay->d.X);
		if (x <= pAABB->MaxEdge.X && x >= pAABB->MinEdge.X)
		{
			float z = pRay->P.Z + (outA * pRay->d.Z);
			if (z <= pAABB->MaxEdge.Z && z >= pAABB->MinEdge.Z)
			{
				*pDist = outA;
				if (pPt)
				{
					pPt->X = x;
					pPt->Y = pRay->P.Y + (outA * pRay->d.Y);
					pPt->Z = z;
				}
				if (pNormal)
				{
					pNormal->X = pNormal->Z = 0;
					pNormal->Y = -1;
				}
				return 6;
			}
		}
	}
	return 0;
}

// Triangle Vertex:
// A: pPoint[0], pPoint[1], pPoint[2]
// B: pPoint[3], pPoint[4], pPoint[5]
// C: pPoint[6], pPoint[7], pPoint[8]
// RayOrigin: pPoint[9], pPoint[10], pPoint[11]
// RayDir	: pPoint[12], pPoint[13], pPoint[14]
// Ray: RayOrigin + k * RayDir (Should k >= 0)
//
// Test Method:
// a * A + b * B + c * C = RayOrigin + k * RayDir
// and a + b + c = 1
// if (a >=0 && a <= 1 && b >= 0 && b <= 1 &&  a + b <= 1)
// then the line Intersect the Triangle (but k may be < 0 )
//
// theRay.major : 0, means the RayDir is X major ( FAbs(pPoint[12]) is maximum )
// theRay.major : 1, means the RayDir is Y major ( FAbs(pPoint[13]) is maximum )
// theRay.major : 2, means the RayDir is Z major ( FAbs(pPoint[14]) is maximum )

bool CPhysicsGeom::IsRayIntersectTriangle(const _Intersect_Ray &theRay, f32 *pPoint, f32 &outK, VEC3 *outPos)
{
	f32 A11, A12, A13, A21, A22, A23, A31, A32, A33;
	A11 = pPoint[0] - pPoint[6];
	A12 = pPoint[3] - pPoint[6];
	A13 = theRay.ray.d.X;
	A21 = pPoint[1] - pPoint[7];
	A22 = pPoint[4] - pPoint[7];
	A23 = theRay.ray.d.Y;
	A31 = pPoint[2] - pPoint[8];
	A32 = pPoint[5] - pPoint[8];
	A33 = theRay.ray.d.Z;
	f32 det11 = (A22 * A33 - A23 * A32);
	f32 det21 = (A13 * A32 - A12 * A33);
	f32 det31 = (A12 * A23 - A13 * A22);
	f32 Det = det11 * A11 + det21 * A21 + det31 * A31;

	if (Det == 0 ||
		(s_faceCulling == PHYSICS_BACK_FACE_CULLING && Det > 0) ||
		(s_faceCulling == PHYSICS_FRONT_FACE_CULLING && Det < 0))
		return false;

	// if(Det >= 0) //the triangle parallels to vertical line
	//	return false;
	//	if ((faceMode == GL_FRONT && Det > 0) ||
	//		(faceMode == GL_BACK && Det < 0))
	//	{
	//		return false;
	//	}
	f32 B1, B2, B3;
	B1 = theRay.ray.P.X - pPoint[6];
	B2 = theRay.ray.P.Y - pPoint[7];
	B3 = theRay.ray.P.Z - pPoint[8];
	f32 absDet = fabs(Det);
	bool signDet = Det > 0;
	f32 a = det11 * B1 + det21 * B2 + det31 * B3;

	if (!signDet)
	{
		a = -a;
	}

	if (a < 0 || a > absDet) // not intersect
	{
		return false;
	}

	f32 b = ((A23 * A31 - A21 * A33)) * B1 +
			((A11 * A33 - A13 * A31)) * B2 +
			((A13 * A21 - A11 * A23)) * B3;

	if (!signDet)
	{
		b = -b;
	}
	if (b < 0 || b > absDet) // not intersect
	{
		return false;
	}

	f32 c = absDet - a - b;
	if (c < 0) // not intersect
	{
		return false;
	}

	a = a / absDet;
	b = b / absDet;

	GLL_ASSERT(theRay.major >= 0 && theRay.major <= 2);
	// TODO: maybe need increase the precision of OutK, such as << COS_SIN_SHIFT
	f32 curK;
	if (theRay.major == 0) // x Major
	{
		curK = ((A11 * a + A12 * b) - B1) / A13;
	}
	else if (theRay.major == 1) // y Major
	{
		curK = ((A21 * a + A22 * b) - B2) / A23;
	}
	else // if(theRay.major == 2)  z Major
	{
		curK = ((A31 * a + A32 * b) - B3) / A33;
		//		curK =( (((A31 * a + A32 * b) / absDet) - B3 ) << COS_SIN_SHIFT) / A33;
	}

	if (curK > outK || curK < 0)
		return false;
	outK = curK;
	if (outPos)
	{
		*outPos = theRay.ray.P + theRay.ray.d * outK;
	}

	return true;
}

bool CPhysicsGeom::IsVerticalRayIntersectTriangle(f32 *pPoint, f32 x, f32 z, f32 &outY)
{
	f32 A11, A12, A21, A22;
	A11 = pPoint[0] - pPoint[6];
	A12 = pPoint[3] - pPoint[6];
	A21 = pPoint[2] - pPoint[8];
	A22 = pPoint[5] - pPoint[8];
	f32 Det = A11 * A22 - A12 * A21;

	if (Det >= 0) // the triangle parallels to vertical line
		return false;

	f32 B1, B2;
	B1 = x - pPoint[6];
	B2 = z - pPoint[8];
	f32 absDet = fabs(Det);
	bool signDet = Det > 0;
	f32 a = A22 * B1 - A12 * B2;
	if (!signDet)
	{
		a = -a;
	}

	if (a < 0 || a > absDet) // not intersect
	{
		return false;
	}
	f32 b = A11 * B2 - A21 * B1;
	if (!signDet)
	{
		b = -b;
	}
	if (b < 0 || b > absDet) // not intersect
	{
		return false;
	}

	f32 c = absDet - a - b;
	if (c < 0) // not intersect
	{
		return false;
	}

	outY = (a * (pPoint[1] - pPoint[7]) + b * (pPoint[4] - pPoint[7])) / absDet + pPoint[7];

	return true;
}
// Line (x00, y00)(x01, y01)  intersect with line(x10, y10) (x11, y11)
bool CPhysicsGeom::IsLineIntersectLine(f32 x00, f32 y00, f32 x01, f32 y01, f32 x10, f32 y10, f32 x11, f32 y11)
{
	f32 A11, A12, A21, A22;
	A11 = x00 - x01;
	A12 = x11 - x10;
	A21 = y00 - y01;
	A22 = y11 - y10;
	f32 Det = A11 * A22 - A12 * A21;
	if (Det == 0)
		return false;
	f32 B1, B2;
	B1 = x11 - x01;
	B2 = y11 - y01;
	f32 absDet = fabs(Det);
	f32 signDet = Det > 0;
	f32 a = A22 * B1 - A12 * B2;
	if (!signDet)
	{
		a = -a;
	}

	if (a < 0 || a > absDet) // not intersect
	{
		return false;
	}
	f32 b = A11 * B2 - A21 * B1;
	if (!signDet)
	{
		b = -b;
	}
	if (b < 0 || b > absDet) // not intersect
	{
		return false;
	}

	return true;
}

// bool CPhysicsGeom::IsIntersectPhysicsGeom(const CPhysicsGeom& theGeom, _Intersect_Info *pIntersectInfo /* = NULL*/) const
//{
//	int geomTypeThis =  GetGeomType();
//
//	int geomType =  theGeom.GetGeomType();
//	if(geomType == PHYSICS_GEOM_TYPE_SPHERE)
//	{
//		VEC3 pos = theGeom.GetPosition();
//		f32 radius = theGeom.GetSizeX();
//		VEC3 speed = theGeom.GetSpeed();
//		return IsIntersectSphere(pos, radius, speed, pIntersectInfo);
//	}
//	else if(geomTypeThis == PHYSICS_GEOM_TYPE_SPHERE)
//	{
//		VEC3 pos = GetPosition();
//		f32 radius = GetSizeX();
//		VEC3 speed = GetSpeed();
//		return theGeom.IsIntersectSphere(pos, radius, speed, pIntersectInfo);
//	}
//
//	return false;
// }

// bool CPhysicsGeom::IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo /*= NULL*/) const
//{
//	//default use AABB test Sphere
//
//	return false;
// }
