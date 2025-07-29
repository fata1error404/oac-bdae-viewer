// PhysicsPlane.cpp: implementation of the CPhysicsPlane class.
//
//////////////////////////////////////////////////////////////////////
// #include "preHeader.h"
#include "PhysicsPlane.h"
// #ifdef _MMO_GAME_
// #include "game/game.h"
// #endif
// #ifdef _AFANTY_
// #include "Common/NavMesh/include/MeshLoaderObj.h"
// #endif
// #include "Level.h"
// #include "generic/scene/Camera3d.h"
// #include "generic/scene/SceneGraph.h"
// #include "generic/sys/shared_stdinc.h"

// #ifdef _DEBUG
// #undef THIS_FILE
// static char THIS_FILE[]=__FILE__;
// #define new DEBUG_NEW
// #endif

#include "VEC3.h"
#include "TypeDef.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPhysicsPlane::CPhysicsPlane(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL)
	: CPhysicsGeom(pos, rotY, halfW, halfH, 0)
{
	m_geomType = PHYSICS_GEOM_TYPE_PLANE;
}

CPhysicsPlane::~CPhysicsPlane()
{
}

CPhysicsGeom *CPhysicsPlane::Clone() const
{
	CPhysicsPlane *cloneGeom = new CPhysicsPlane(m_pos, m_rotY, m_halfSize.X, m_halfSize.Y, 0);
	cloneGeom->SetInfoFlag(m_info.s32value);
	if (m_pNext)
	{
		cloneGeom->m_pNext = m_pNext->Clone();
	}
	if (m_pTrigger)
	{
		cloneGeom->m_pTrigger = m_pTrigger->Clone();
	}
	return cloneGeom;
}
#if RENDER_GEOM
using namespace glitch;
using namespace scene;
using namespace video;
void CPhysicsPlane::Render(/*Camera3d &cam*/) const
{
	if (!m_info.parentTransformSet)
		return;
#define CUT_NUM 8
	IVideoDriver *irrDriver = GetGame()->GetIrrDevice()->getVideoDriver();

	MTX4 matOldWorld = irrDriver->getTransform(video::ETS_WORLD);
	irrDriver->setTransform(video::ETS_WORLD, m_absTransform);
	/*
		1 ____3
		|\   |
		| \  |
		|  \ |
		|___\|
		0    2
		*/
	glitch::video::SColor groundColor(0x88, 0, 0, 0xFF);
	VEC3 vertex[6]; // 2,0,1,2,3,1
	vertex[0] = VEC3(+GetSizeX(), -GetSizeY(), 0);
	vertex[1] = VEC3(-GetSizeX(), -GetSizeY(), 0);
	vertex[2] = VEC3(-GetSizeX(), +GetSizeY(), 0);
	vertex[3] = vertex[0];
	vertex[4] = VEC3(+GetSizeX(), +GetSizeY(), 0);
	vertex[5] = vertex[2];

	// short indexList[6] = { 0, 1, 2, 2, 1, 3};

	// irrDriver->drawVertexPrimitiveList(vertex,indexList, 0, 4, 2, video::EVT_STANDARD, EPT_TRIANGLES, EIT_16BIT);

	//
	for (int i = 0; i < 5; i++)
	{
		irrDriver->draw3DLine(vertex[i], vertex[i + 1], groundColor);
	}

	// CPhysics::DrawLine(vertex, 5, groundColor);

	irrDriver->setTransform(video::ETS_WORLD, matOldWorld);
}
#endif

#ifdef _AFANTY_
int CPhysicsPlane::GenerateMeshObj(rcMeshLoaderObj &meshObj, int vOff) const
{
	VEC3 vertex[4]; // 0 ,1, 2, 3
	vertex[0] = VEC3(-GetSizeX(), -GetSizeY(), 0);
	vertex[1] = VEC3(-GetSizeX(), +GetSizeY(), 0);
	vertex[2] = VEC3(+GetSizeX(), -GetSizeY(), 0);
	vertex[3] = VEC3(+GetSizeX(), +GetSizeY(), 0);
	for (int i = 0; i < 4; i++)
	{
		m_absTransform.transformVect(vertex[i]);
		meshObj.addVertex(vertex[i].X, vertex[i].Y, vertex[i].Z);
	}
	meshObj.addTriangle(vOff + 0, vOff + 1, vOff + 2);
	meshObj.addTriangle(vOff + 2, vOff + 1, vOff + 3);

	return 4; // Return the vertex number
}
#endif

// void CPhysicsPlane::SetSize(f32 halfW, f32 halfH, f32 halfL) {
//	GetSizeX() = halfW;
//	GetSizeY() = halfH;
//	ResetAlongXZ();
//
//	//handle rotZ
//	//Near 0 degree and near 180 degree all set to 0 degree
//	if(m_rotY < PHYSICS_ROT_Y_THRESHOLD || m_rotY > 65536 - PHYSICS_ROT_Y_THRESHOLD)
//	{
//		SetRotationY(0);		//Near 0 degree set to 0 degree
//		SetAlongX();
//	}
//	else if(m_rotY > 32768 - PHYSICS_ROT_Y_THRESHOLD && m_rotY < 32768 + PHYSICS_ROT_Y_THRESHOLD)
//	{
//		SetRotationY(32768); 		//Near 180 degree set to 180 degree
//		SetAlongX();
//	}
//	else if(m_rotY < 16384 + PHYSICS_ROT_Y_THRESHOLD && m_rotY > 16384 - PHYSICS_ROT_Y_THRESHOLD)
//	{
//		//Near 90 degree set to 90 degree
//		SetRotationY(16384);
//		SetAlongZ();
//	}
//	else if(m_rotY > 49152 - PHYSICS_ROT_Y_THRESHOLD && m_rotY < 49152 + PHYSICS_ROT_Y_THRESHOLD)
//	{
//		//Near 270 degree set to 270 degree
//		SetRotationY(49152);
//		SetAlongZ();	//270 degree
//	}
//
//	//update the AABB
//	UpdateAABB();
//
//
// }
// void CPhysicsPlane::UpdateAABB()
//{
//	//update the AABB
//	m_AABB.MinEdge.Z = m_pos.Z - GetSizeY();
//	m_AABB.MaxEdge.Z = m_pos.Z + GetSizeY();
//	if(IsAlongX())
//	{
//		m_AABB.MinEdge.X = m_pos.X - GetSizeX();
//		m_AABB.MaxEdge.X = m_pos.X + GetSizeX();
//		m_AABB.MaxEdge.Y = m_AABB.MinEdge.Y = m_pos.Y;
//	}
//	else if(IsAlongZ())
//	{
//		m_AABB.MaxEdge.X = m_AABB.MinEdge.X = m_pos.X;
//		m_AABB.MinEdge.Y = m_pos.Y - GetSizeX();
//		m_AABB.MaxEdge.Y = m_pos.Y + GetSizeX();
//
//	}
//	else
//	{
//		int absCosZ = m_cosRotY >= 0 ? m_cosRotY : -m_cosRotY;
//		int absSinZ = m_sinRotY >= 0 ? m_sinRotY : -m_sinRotY;
//		m_AABB.MaxEdge.X =  (GetSizeX() * absCosZ ) ;
//		m_AABB.MinEdge.X = -m_AABB.MaxEdge.X + m_pos.X;
//		m_AABB.MaxEdge.X += m_pos.X;
//
//		m_AABB.MaxEdge.Y =( GetSizeX() * absSinZ) ;
//		m_AABB.MinEdge.Y = -m_AABB.MaxEdge.Y + m_pos.Y;
//		m_AABB.MaxEdge.Y += m_pos.Y;
//
//	}
// }
//
// parameters
//@theSegment	_Intersect_Segment			the line struct for intersect test
//@outA		the length of intersect point to the line start point when the return value is true.
//			(the intersect point is theSegment.v0 + (outA * theSegment.vDir )).
// return value:
//			return whether this geometry is intersected with the line

bool CPhysicsPlane::IsIntersectSegment(const _Intersect_Segment &theSegment, f32 &outA, _Intersect_Info *pIntersectInfo /*= NULL*/) const
{
	////AABB TEST First
	// if(!AABB_IntersectSegment(theSegment))
	//	return false;

	_Intersect_Segment localLine;
	TransformSegmentToLocalCoordinateSystem(theSegment, localLine);
	// AABB TEST first
	if (localLine.seg.P0.Z < 0 || localLine.seg.P1.Z > 0 || localLine.seg.P0.Z == localLine.seg.P1.Z)
		return false;

	if (localLine.aabb.MinEdge.X > GetSizeX() || localLine.aabb.MaxEdge.X < -GetSizeX() ||
		localLine.aabb.MinEdge.Y > GetSizeY() || localLine.aabb.MaxEdge.Y < -GetSizeY())
		return false;

	// calculate the intersect point
	outA = -localLine.seg.P0.Z * localLine.len / (localLine.seg.P1.Z - localLine.seg.P0.Z);
	f32 x = localLine.seg.P0.X + (outA * localLine.dir.X);
	if (x > GetSizeX() || x < -GetSizeX())
		return false;
	f32 y = localLine.seg.P0.Y + (outA * localLine.dir.Y);
	if (y > GetSizeY() || y < -GetSizeY())
		return false;
	// if(outA <= 0.01f)
	//	outA = 0.0f;
	// else
	//	outA -= 0.01f;
	outA *= theSegment.len / localLine.len;
	if (pIntersectInfo)
	{
		pIntersectInfo->pGeom = this;
		// pIntersectInfo->point.X = x;
		// pIntersectInfo->point.Y = y;
		// pIntersectInfo->point.Z = 0;
		pIntersectInfo->normal.X = m_absTransform[8];
		pIntersectInfo->normal.Y = m_absTransform[9];
		pIntersectInfo->normal.Z = m_absTransform[10];
		pIntersectInfo->normal.normalize();
		// m_absTransform.transformVect(pIntersectInfo->point);

		// VEC3 interPt;
		// GetPoint(interPt, BOX_PT_DOWN_FRONT_LEFT);
		// pIntersectInfo->x0 = interPt.X;
		// pIntersectInfo->y0 = interPt.Y;
		// GetPoint(interPt, BOX_PT_DOWN_FRONT_RIGHT);
		// pIntersectInfo->x1 = interPt.X;
		// pIntersectInfo->y1 = interPt.Y;
		// pIntersectInfo->depth = 10;
	}

	return true;
}
void CPhysicsPlane::GetPoint(VEC3 &pt, int index) const
{
	VEC3 localPt;
	switch (index)
	{
	case BOX_PT_DOWN_FRONT_LEFT:
		localPt.X = -GetSizeX();
		break;
	case BOX_PT_DOWN_FRONT_RIGHT:
		localPt.X = GetSizeX();
		break;
	default: // SHOULD NOT Go Here
		GLL_ASSERT(0);
		break;
	}
	localPt.Y = 0;
	localPt.Z = -GetSizeY();
	LocalToWorld(localPt, pt);
}

// bool CPhysicsPlane::IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo /*= NULL*/) const
//{
//	return false;
// }