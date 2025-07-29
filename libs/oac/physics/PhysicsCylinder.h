// PhysicsCylinder.h: interface for the CPhysicsCylinder class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PHYSICSCYLINDER_H__57BC001A_ABCC_4C57_AC26_EDD053F56EC3__INCLUDED_)
#define AFX_PHYSICSCYLINDER_H__57BC001A_ABCC_4C57_AC26_EDD053F56EC3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PhysicsGeom.h"
//Cylinder is parallel to Y-Axis
//ignore the rotation member in the base class
class CPhysicsCylinder : public CPhysicsGeom  
{
public:
	CPhysicsCylinder(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL);
	virtual ~CPhysicsCylinder();
	virtual bool IsIntersectSegment(const _Intersect_Segment &theSegment, f32 &outA, _Intersect_Info *pIntersectInfo = NULL)  const;
	virtual bool IsPointInThis(const VEC3 &pt) const;
	virtual CPhysicsGeom * Clone() const;

	//virtual f32 GetSizeY() const {return m_halfHeight;};
	//virtual f32 GetSizeX() const {return m_radius;};
	//virtual f32 GetSizeZ() const {return m_radius;};
//	virtual void SetRotationY(int rotY) { };
//	virtual bool IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo = NULL) const;

#if RENDER_GEOM
	virtual void Render()  const;
#endif
#ifdef _AFANTY_
	virtual int GenerateMeshObj(rcMeshLoaderObj& meshObj, int vOff) const;
#endif
private:
	//f32 m_radius;
	//f32 m_halfHeight;		//Y

};

#endif // !defined(AFX_PHYSICSCYLINDER_H__57BC001A_ABCC_4C57_AC26_EDD053F56EC3__INCLUDED_)
