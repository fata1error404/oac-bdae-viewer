// PhysicsPlane.h: interface for the CPhysicsPlane class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PHYSICSPLANE_H__0E643A95_48C2_416A_A269_34C8E5E3496F__INCLUDED_)
#define AFX_PHYSICSPLANE_H__0E643A95_48C2_416A_A269_34C8E5E3496F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PhysicsGeom.h"

//Default Positive Direction is  (0, 0, 1) (towards z)
class CPhysicsPlane : public CPhysicsGeom  
{
public:
	CPhysicsPlane(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL);
	virtual ~CPhysicsPlane();
	//virtual void SetSize(f32 halfW, f32 halfH, f32 halfL) ;
	virtual bool IsIntersectSegment(const _Intersect_Segment &theSegment, f32 &outA, _Intersect_Info *pIntersectInfo = NULL)  const;
	virtual bool IsPointInThis(const VEC3 &pt) const { return false;};
	void GetPoint(VEC3 &pt, int index)  const;
	//virtual void UpdateAABB();
	//virtual bool IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo = NULL) const;
	virtual CPhysicsGeom * Clone() const;
#if RENDER_GEOM
	virtual void Render(/*Camera3d &cam*/)  const;
#endif
#ifdef _AFANTY_
	virtual int GenerateMeshObj(rcMeshLoaderObj& meshObj, int vOff) const;
#endif
private:
	//f32 m_halfWidth;		//x
	//f32 m_halfHeight;		//z
	


};

#endif // !defined(AFX_PHYSICSPLANE_H__0E643A95_48C2_416A_A269_34C8E5E3496F__INCLUDED_)
