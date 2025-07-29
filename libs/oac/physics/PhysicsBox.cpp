// PhysicsBox.cpp: implementation of the CPhysicsBox class.
//
//////////////////////////////////////////////////////////////////////
//#include "preHeader.h"
#include "PhysicsBox.h"
#include "PhysicsGeom.h"
//#include "physics.h"
#ifdef _MMO_GAME_
#include "game/game.h"
using namespace glitch;
using namespace scene;
using namespace video;
#endif
#ifdef _AFANTY_
#include "Common/NavMesh/include/MeshLoaderObj.h"
#endif
//#include "../BackGround.h"
//#include "generic/sys/shared_stdinc.h"

//#ifdef _DEBUG
//#undef THIS_FILE
//static char THIS_FILE[]=__FILE__;
//#define new DEBUG_NEW
//#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPhysicsBox::CPhysicsBox(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL)
: CPhysicsGeom(pos, rotY, halfW,halfH, halfL)
{
	m_geomType = PHYSICS_GEOM_TYPE_BOX;


}

CPhysicsBox::~CPhysicsBox()
{

}


CPhysicsGeom * CPhysicsBox::Clone() const
{
	CPhysicsBox * cloneGeom = gll_new CPhysicsBox(m_pos, m_rotY, m_halfSize.X, m_halfSize.Y, m_halfSize.Z);
	cloneGeom->SetInfoFlag(m_info.s32value);
	if(m_pNext)
	{
		cloneGeom->m_pNext = m_pNext->Clone();
	}
	if(m_pTrigger)
	{
		cloneGeom->m_pTrigger = m_pTrigger->Clone();
	}
	return cloneGeom;
}

#if RENDER_GEOM
void CPhysicsBox::Render()  const
{
	if(!m_info.parentTransformSet)
		return;
	IVideoDriver * irrDriver = GetGame()->GetIrrDevice()->getVideoDriver();
	//	IVideoDriver * irrDriver = Game::GetInstance()->GetIrrDevice()->getVideoDriver();

	MTX4 matOldWorld = irrDriver->getTransform(video::ETS_WORLD);
	MTX4 matNewWorld = matOldWorld;
	matNewWorld.setTranslation(VEC3());

	//float fAngle = DEG_TO_RAD(m_rotY);
	//quaternion rotate;
	//rotate.fromAngleAxis(fAngle,VEC3(0,1,0));
	//MTX4 matRotate = rotate.getMatrix();
	//matNewWorld = matNewWorld * matRotate;


	irrDriver->setTransform(video::ETS_WORLD, /*matNewWorld **/ m_absTransform);
	//
	VEC3 vertex[10];
	vertex[0] = VEC3(-GetSizeX(), +GetSizeY(), -GetSizeZ());
	vertex[1] = VEC3(-GetSizeX(), -GetSizeY(), -GetSizeZ());
	vertex[2] = VEC3(+GetSizeX(), +GetSizeY(), -GetSizeZ());
	vertex[3] = VEC3(+GetSizeX(), -GetSizeY(), -GetSizeZ());
	vertex[4] = VEC3(+GetSizeX(), +GetSizeY(), +GetSizeZ());
	vertex[5] = VEC3(+GetSizeX(), -GetSizeY(), +GetSizeZ());
	vertex[6] = VEC3(-GetSizeX(), +GetSizeY(), +GetSizeZ());
	vertex[7] = VEC3(-GetSizeX(), -GetSizeY(), +GetSizeZ());
	vertex[8] = vertex[0];
	vertex[9] = vertex[1];

	glitch::video::SColor groundColor (0x88, 0, 0, 0xFF);
	for(int i = 0; i < 4; i++)
	{
		irrDriver->draw3DLine(vertex[i*2], vertex[i*2+1], groundColor);
		irrDriver->draw3DLine(vertex[i*2], vertex[i*2+2], groundColor);
		irrDriver->draw3DLine(vertex[i*2+1], vertex[i*2+3], groundColor);
	}
	//
	irrDriver->setTransform(video::ETS_WORLD,matOldWorld);
}
#endif
#ifdef _AFANTY_
int CPhysicsBox::GenerateMeshObj(rcMeshLoaderObj& meshObj, int vOff) const
{
	if(!m_info.parentTransformSet)
		return 0;
	VEC3 vertex[8];
	vertex[0] = VEC3(-GetSizeX(), +GetSizeY(), -GetSizeZ());
	vertex[1] = VEC3(-GetSizeX(), -GetSizeY(), -GetSizeZ());
	vertex[2] = VEC3(+GetSizeX(), +GetSizeY(), -GetSizeZ());
	vertex[3] = VEC3(+GetSizeX(), -GetSizeY(), -GetSizeZ());
	vertex[4] = VEC3(+GetSizeX(), +GetSizeY(), +GetSizeZ());
	vertex[5] = VEC3(+GetSizeX(), -GetSizeY(), +GetSizeZ());
	vertex[6] = VEC3(-GetSizeX(), +GetSizeY(), +GetSizeZ());
	vertex[7] = VEC3(-GetSizeX(), -GetSizeY(), +GetSizeZ());
	for(int i= 0; i<8; i++)
	{
		m_absTransform.transformVect(vertex[i]);
		meshObj.addVertex(vertex[i].X, vertex[i].Y, vertex[i].Z);
	}
	meshObj.addTriangle(vOff + 0, vOff + 1, vOff + 2);
	meshObj.addTriangle(vOff + 2, vOff + 1, vOff + 3);
	
	meshObj.addTriangle(vOff + 2, vOff + 3, vOff + 4);
	meshObj.addTriangle(vOff + 4, vOff + 3, vOff + 5);
	
	meshObj.addTriangle(vOff + 2, vOff + 0, vOff + 4);
	meshObj.addTriangle(vOff + 4, vOff + 0, vOff + 6);
	
	meshObj.addTriangle(vOff + 4, vOff + 6, vOff + 5);
	meshObj.addTriangle(vOff + 5, vOff + 6, vOff + 7);
	
	meshObj.addTriangle(vOff + 0, vOff + 1, vOff + 6);
	meshObj.addTriangle(vOff + 6, vOff + 1, vOff + 7);
	
	meshObj.addTriangle(vOff + 1, vOff + 3, vOff + 7);
	meshObj.addTriangle(vOff + 7, vOff + 3, vOff + 5);



	return 8;
}
#endif


//parameters
//@theSegment	SEGMENT3			the line struct for intersect test
//@outA		the length of intersect point to the line start point when the return value is true. 
//			(the intersect point is theSegment.v0 + (outA * theSegment.vDir )).
//return value:
//			return whether this geometry is intersected with the line
bool CPhysicsBox::IsIntersectSegment(const _Intersect_Segment &theSegment, f32 &outA, _Intersect_Info *pIntersectInfo /*= NULL*/) const
{
	//if(!AABB_IntersectSegment(theSegment))
	//	return false;

	_Intersect_Segment localLine;
	TransformSegmentToLocalCoordinateSystem(theSegment, localLine);
	//AABB TEST first
	//VEC3 minV(min(localLine.seg.P0.x, localLine.seg.P1.x), min(localLine.seg.P0.y, localLine.seg.P1.y), min(localLine.seg.P0.z, localLine.seg.P1.z));
	//VEC3 maxV(max(localLine.seg.P0.x, localLine.seg.P1.x), max(localLine.seg.P0.y, localLine.seg.P1.y), max(localLine.seg.P0.z, localLine.seg.P1.z));

	if(localLine.aabb.MinEdge.X > GetSizeX()  || localLine.aabb.MaxEdge.X < -GetSizeX()  ||
	   localLine.aabb.MinEdge.Y > GetSizeY() || localLine.aabb.MaxEdge.Y < -GetSizeY() ||
	   localLine.aabb.MinEdge.Z > GetSizeZ() || localLine.aabb.MaxEdge.Z < -GetSizeZ())
		return false;

	AABB box(VEC3(-GetSizeX(), -GetSizeY(), -GetSizeZ()), VEC3(GetSizeX(), GetSizeY(), GetSizeZ()));
	RAY3 ray3(localLine.seg.P0, localLine.dir, false);

	VEC3 vecIntersectPt, vecLocalNormal;
	int nRet = CPhysicsGeom::IntersectionRay3AABB(&ray3, &box, &outA, &vecIntersectPt, &vecLocalNormal);
	if(nRet && outA <= localLine.len)
	{
	//	pIntersectInfo->pActor = this->m_pActor;
		if(pIntersectInfo)
		{	
			pIntersectInfo->pGeom = this;
		//m_absTransform.transformVect(pIntersectInfo->point, vecIntersectPt);
			pIntersectInfo->normal.X *=	m_invAbsScale.X;
			pIntersectInfo->normal.Y *=	m_invAbsScale.Y;
			pIntersectInfo->normal.Z *=	m_invAbsScale.Z;
			m_absTransform.rotateVect(pIntersectInfo->normal, vecLocalNormal);

			pIntersectInfo->normal.normalize();
		}
		//LocalToWorld(vecLocalNormal,pIntersectInfo->normal);
		//pIntersectInfo->normal -= m_pos;
		outA *= theSegment.len / localLine.len;;
		return true;
	}

/*
	AABB box(VEC3(-GetSizeX(), -GetSizeY(), -GetSizeZ()), VEC3(GetSizeX(), GetSizeY(), GetSizeZ()));
	RAY3 ray3(localLine.seg.P0, localLine.dir, true);
	f32 t;
	if(IntersectionRay3AABB(&ray3, &box, &t))
	{
		if(t <= localLine.len)
		{
			outA = t;//outA = t / localLine.len;
			if(pIntersectInfo) {
				pIntersectInfo->pGeom = this;
				pIntersectInfo->pActor = m_pActor;

				MTX34 matRotation;
				VEC3 angle = m_pActor->GetRotate();
				VEC3 point = localLine.seg.P0 + localLine.dir*t;

				MTX34Identity(&matRotation);
				MTX34RotXYZDeg(&matRotation, angle.x, angle.y, angle.z);

				// Front plane
				if (nw4r::math::FAbs(point.z - box.MaxEdge.z) < 0.01f) {
					pIntersectInfo->normal.x = 0.0f;
					pIntersectInfo->normal.y = 0.0f;
					pIntersectInfo->normal.z = 1.0f;
				}
				// Back plane
				else if (nw4r::math::FAbs(point.z - box.MinEdge.z) < 0.01f) {
					pIntersectInfo->normal.x = 0.0f;
					pIntersectInfo->normal.y = 0.0f;
					pIntersectInfo->normal.z = -1.0f;
				}
				// Left plane
				else if (nw4r::math::FAbs(point.x - box.MinEdge.x) < 0.01f) {
					pIntersectInfo->normal.x = -1.0f;
					pIntersectInfo->normal.y = 0.0f;
					pIntersectInfo->normal.z = 0.0f;
				}
				// Right plane
				else if (nw4r::math::FAbs(point.x - box.MaxEdge.x) < 0.01f) {
					pIntersectInfo->normal.x = 1.0f;
					pIntersectInfo->normal.y = 0.0f;
					pIntersectInfo->normal.z = 0.0f;
				}
				// Top plane
				else if (nw4r::math::FAbs(point.y - box.MaxEdge.y) < 0.01f) {
					pIntersectInfo->normal.x = 0.0f;
					pIntersectInfo->normal.y = 1.0f;
					pIntersectInfo->normal.z = 0.0f;
				}
				// Bottom plane
				else if (nw4r::math::FAbs(point.y - box.MinEdge.y) < 0.01f) {
					pIntersectInfo->normal.x = 0.0f;
					pIntersectInfo->normal.y = -1.0f;
					pIntersectInfo->normal.z = 0.0f;
				}

				MTXMultVec(matRotation, pIntersectInfo->normal, pIntersectInfo->normal);
				VEC3Normalize(&pIntersectInfo->normal, &pIntersectInfo->normal);
			}
			return true;
		}
	}
*/
 /*	//TEST FRONT FACE First
 	if(localLine.seg.P0.y <= -GetSizeZ() && localLine.seg.P0.y < localLine.seg.P1.y)
 	{
 		outA = (-GetSizeZ() - localLine.seg.P0.y )  * localLine.len / (localLine.seg.P1.y - localLine.seg.P0.y);
 		int x = localLine.seg.P0.x + (outA * localLine.seg.dir.x );
 		if( x <= GetSizeX() && x >= -GetSizeX())
 		{
 			int z = localLine.seg.P0.z + (outA * localLine.seg.PDir.z );
 			if( z <= GetSizeY() && z >= -GetSizeY())
 			{
 				if(pIntersectInfo)
 				{
 					pIntersectInfo->pGeom = this;
 					//VEC3 interPt;
 					//GetPoint(interPt, BOX_PT_DOWN_FRONT_LEFT);
 					//pIntersectInfo->x0 = interPt.x;
 					//pIntersectInfo->y0 = interPt.y;
 					//GetPoint(interPt, BOX_PT_DOWN_FRONT_RIGHT);
 					//pIntersectInfo->x1 = interPt.x;
 					//pIntersectInfo->y1 = interPt.y;
 					//pIntersectInfo->depth = GetSizeZ() << 1;
 				}
 				return true;
 			}
 		}	
 
 	}
// 
	//Test Back FACE
	if(localLine.seg.P0.y >= GetSizeZ() && localLine.seg.P0.y > localLine.seg.P1.y)
	{
		outA = ( GetSizeZ() - localLine.seg.P0.y )  * localLine.len / (localLine.seg.P1.y - localLine.seg.P0.y);
		int x = localLine.seg.P0.x + (outA * localLine.seg.PDir.x );
		if( x <= GetSizeX() && x >= -GetSizeX())
		{
			int z = localLine.seg.P0.z + (outA * localLine.seg.PDir.z );
			if( z <= GetSizeY() && z >= -GetSizeY())
			{
				if(pIntersectInfo)
				{
					pIntersectInfo->pGeom = this;
					//VEC3 interPt;
					//GetPoint(interPt, BOX_PT_DOWN_BACK_RIGHT);
					//pIntersectInfo->x0 = interPt.x;
					//pIntersectInfo->y0 = interPt.y;
					//GetPoint(interPt, BOX_PT_DOWN_BACK_LEFT);
					//pIntersectInfo->x1 = interPt.x;
					//pIntersectInfo->y1 = interPt.y;
					//pIntersectInfo->depth = GetSizeZ() << 1;

				}
				return true;
			}
		}	

		
	}

	//Test Left FACE
	if(localLine.seg.P0.x <= -GetSizeX() && localLine.seg.P0.x < localLine.seg.P1.x)
	{
		outA = (-GetSizeX() - localLine.seg.P0.x)  * localLine.len / (localLine.seg.P1.x - localLine.seg.P0.x);
		int y = localLine.seg.P0.y + (outA * localLine.seg.PDir.y );
		if( y <= GetSizeZ() && y >= -GetSizeZ())
		{
			int z = localLine.seg.P0.z + (outA * localLine.seg.PDir.z );
			if( z <= GetSizeY() && z >= -GetSizeY())
			{
				if(pIntersectInfo)
				{
					pIntersectInfo->pGeom = this;
					//VEC3 interPt;
					//GetPoint(interPt, BOX_PT_DOWN_BACK_LEFT);
					//pIntersectInfo->x0 = interPt.x;
					//pIntersectInfo->y0 = interPt.y;
					//GetPoint(interPt, BOX_PT_DOWN_FRONT_LEFT);
					//pIntersectInfo->x1 = interPt.x;
					//pIntersectInfo->y1 = interPt.y;
					//pIntersectInfo->depth = GetSizeX() << 1;
				}
				return true;
			}
		}	


	}

	//Test Right FACE
	if(localLine.seg.P0.x >= GetSizeX()  && localLine.seg.P0.x > localLine.seg.P1.x)
	{
		outA = (GetSizeX() - localLine.seg.P0.x)  * localLine.len / (localLine.seg.P1.x - localLine.seg.P0.x);
		int y = localLine.seg.P0.y + (outA * localLine.seg.PDir.y );
		if( y <= GetSizeZ() && y >= -GetSizeZ())
		{
			int z = localLine.seg.P0.z + (outA * localLine.seg.PDir.z );
			if( z <= GetSizeY() && z >= -GetSizeY())
			{
				if(pIntersectInfo)
				{
					pIntersectInfo->pGeom = this;
					//VEC3 interPt;
					//GetPoint(interPt, BOX_PT_DOWN_FRONT_RIGHT);
					//pIntersectInfo->x0 = interPt.x;
					//pIntersectInfo->y0 = interPt.y;
					//GetPoint(interPt, BOX_PT_DOWN_BACK_RIGHT);
					//pIntersectInfo->x1 = interPt.x;
					//pIntersectInfo->y1 = interPt.y;
					//pIntersectInfo->depth = GetSizeX() << 1;
				}
				return true;
			}
		}	

	}

	//Test TOP FACE
	if(localLine.seg.P0.z >= GetSizeY() && localLine.seg.P0.z > localLine.seg.P1.z)
	{
		outA = (GetSizeY() - localLine.seg.P0.z)  * localLine.len / (localLine.seg.P1.z - localLine.seg.P0.z);
		int x = localLine.seg.P0.x + (outA * localLine.seg.PDir.x );
		if( x <= GetSizeX() && x >= -GetSizeX())
		{
			int y = localLine.seg.P0.y + (outA * localLine.seg.PDir.y );
			if( y <= GetSizeZ() && y >= -GetSizeZ())
			{
				if(pIntersectInfo)
				{
					pIntersectInfo->pGeom = this;
				}
				return true;
			}
		}	

	}
*/
	return false;
}


void CPhysicsBox::GetPoint(VEC3 &pt, int index)  const
{
	VEC3 localPt;
	switch(index)
	{
	case BOX_PT_DOWN_FRONT_LEFT:
		localPt.X = -GetSizeX();
		localPt.Y = -GetSizeZ();
		break;
	case BOX_PT_DOWN_FRONT_RIGHT:
		localPt.X = GetSizeX();
		localPt.Y = -GetSizeZ();
		break;
	case BOX_PT_DOWN_BACK_RIGHT:
		localPt.X = GetSizeX();
		localPt.Y = GetSizeZ();
	    break;
	case BOX_PT_DOWN_BACK_LEFT:
		localPt.X = -GetSizeX();
		localPt.Y = GetSizeZ();
	    break;
	default: //SHOULD NOT Go Here
		GLL_ASSERT(0);
	    break;
	}
	localPt.Z = -GetSizeY();
	LocalToWorld(localPt ,pt);

}

bool CPhysicsBox::IsPointInThis(const VEC3 &pt) const
{
	if(!AABB_IntersectPoint(pt))
		return false;
	//if(m_rotY == 0)
	//	return true;
	VEC3 localPt;
	WorldToLocal(pt, localPt);
	if(localPt.X <= GetSizeX() && localPt.X >= -GetSizeX() &&localPt.Y <= GetSizeY() && localPt.Y >= -GetSizeY()&& localPt.Z <= GetSizeZ() && localPt.Z >= -GetSizeZ())
		return true;

	return false;

}

bool CPhysicsBox::IsPointInThisXZ(const VEC3 &pt) const
{
	if(pt.X > m_AABB.MaxEdge.X  || pt.X < m_AABB.MinEdge.X ||
		pt.Z > m_AABB.MaxEdge.Z || pt.Z < m_AABB.MinEdge.Z)
		return false;
	//if(m_rotY == 0)
	//	return true;
	VEC3 localPt;
	WorldToLocal(pt, localPt);
	if(localPt.X <= GetSizeX() && localPt.X >= -GetSizeX() && localPt.Z <= GetSizeZ() && localPt.Z >= -GetSizeZ())
		return true;
	return false;
}

VEC3 CPhysicsBox::CalXZNearestPointFrom(const VEC3& pt)
{	
	VEC3 ptLocal(0,0,0);
	WorldToLocal(pt,ptLocal);
	VEC3 rightBottom,ptNearest(0,0,0);

	if( ptLocal.X  < -GetSizeX() )
	{
		ptNearest.X = - GetSizeX();
	}else if( ptLocal.X < GetSizeX() )
	{
		ptNearest.X = ptLocal.X;
	}else
	{
		ptNearest.X = GetSizeX();
	}

	if( ptLocal.Z  < -GetSizeZ())
	{
		ptNearest.Z= -GetSizeZ();
	}else if( ptLocal.Z< GetSizeZ())
	{
		ptNearest.Z= ptLocal.Z;
	}else
	{
		ptNearest.Z= GetSizeZ();
	}

	VEC3 nearestRet;
	LocalToWorld(ptNearest,nearestRet);
	return nearestRet;
}

//bool CPhysicsBox::IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo /*= NULL*/) const
//{
//	//aabb Test
//	if(!AABB_IntersectSphere(pos, radius, speed))
//		return false;
//
//	VEC3 comSpeed = speed - GetSpeed();
//	VEC3 endPos = pos + comSpeed;
//
//	//SEGMENT3 seg(pos, endPos);
//	_Intersect_Segment theSegment;
//	theSegment.seg.P0 = pos;
//	theSegment.seg.P1 = endPos;
//	CPhysics::ComputeIntersectLine(theSegment);
//
//	_Intersect_Segment localLine;
//	TransformSegmentToLocalCoordinateSystem(theSegment, localLine);
//	//AABB TEST first
//	if(localLine.aabb.MinEdge.X > GetSizeX() + radius  || localLine.aabb.MaxEdge.X < -GetSizeX() - radius ||
//	   localLine.aabb.MinEdge.Y > GetSizeY() + radius || localLine.aabb.MaxEdge.Y < -GetSizeY() - radius  ||
//	   localLine.aabb.MinEdge.Z > GetSizeZ() + radius || localLine.aabb.MaxEdge.Z < -GetSizeZ() - radius )
//		return false;
//
//	AABB box(VEC3(-GetSizeX() - radius, -GetSizeY() - radius, -GetSizeZ() - radius), 
//		VEC3(GetSizeX() + radius, GetSizeY() + radius, GetSizeZ() + radius));
//	RAY3 ray3(localLine.seg.P0, localLine.dir, true);
//	f32 t;
//	if(CPhysics::IntersectionRay3AABB(&ray3, &box, &t))
//	{
//		if(t <= localLine.len)
//		{
//			if(pIntersectInfo) {
//				pIntersectInfo->pGeom = this;
//				pIntersectInfo->pActor = m_pActor;
//			}
//			return true;
//		}
//	}
//	return false;
//
//}