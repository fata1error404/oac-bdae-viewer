// PhysicsBox.h: interface for the CPhysicsCuboid class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PHYSICSBOX_H__342A1FCB_47BF_4E26_A6E5_B5D8B2915822__INCLUDED_)
#define AFX_PHYSICSBOX_H__342A1FCB_47BF_4E26_A6E5_B5D8B2915822__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PhysicsGeom.h"


class CPhysicsBox : public CPhysicsGeom  
{
public:
	void GetPoint(VEC3 &pt, int index) const;
	CPhysicsBox(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL);
	virtual ~CPhysicsBox();
	virtual bool IsIntersectSegment(const _Intersect_Segment &theSegment, f32 &outA, _Intersect_Info *pIntersectInfo = NULL)  const;
	virtual bool IsPointInThis(const VEC3 &pt) const;
	virtual bool IsPointInThisXZ(const VEC3 &pt) const;

	virtual VEC3 CalXZNearestPointFrom(const VEC3& pt);
	CPhysicsGeom * Clone() const;

	//virtual void UpdateAABB();
	//virtual bool IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo = NULL) const;

#if RENDER_GEOM
	virtual void Render()  const;
#endif
#ifdef _AFANTY_
	virtual int GenerateMeshObj(rcMeshLoaderObj& meshObj, int vOff) const;
#endif

	
private:
	//f32 m_halfWidth;		//x
	//f32	m_halfLength;		//z
	//f32 m_halfHeight;		//y

};

#endif // !defined(AFX_PHYSICSBOX_H__342A1FCB_47BF_4E26_A6E5_B5D8B2915822__INCLUDED_)
