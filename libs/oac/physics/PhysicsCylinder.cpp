// PhysicsCylinder.cpp: implementation of the CPhysicsCylinder class.
//
//////////////////////////////////////////////////////////////////////
//#include "preHeader.h"
#include "Physics.h"
#include "PhysicsCylinder.h"
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

CPhysicsCylinder::CPhysicsCylinder(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL)
: CPhysicsGeom(pos, 0, halfW,halfH, halfW)
{
	m_geomType = PHYSICS_GEOM_TYPE_CYLINDER;
}

CPhysicsCylinder::~CPhysicsCylinder()
{

}

CPhysicsGeom * CPhysicsCylinder::Clone() const
{
	CPhysicsCylinder * cloneGeom = gll_new CPhysicsCylinder(m_pos, 0, m_halfSize.X, m_halfSize.Y, m_halfSize.X);
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

#define CUT_NUM 8

#if RENDER_GEOM
void CPhysicsCylinder::Render() const 
{
/*

	GXVtxDescList descBackup[GX_MAX_VTXDESCLIST_SZ];
	GXVtxAttrFmtList attrBackup[GX_MAX_VTXATTRFMTLIST_SZ];

	GXGetVtxDescv(descBackup);
	GXGetVtxAttrFmtv( GX_VTXFMT0,  attrBackup);

	GXClearVtxDesc();
	
	MTX34 mv;	
	MTX44 p;
//	Camera3d cam = CLevel::Get()->GetSceneGraph()->GetCurrentCamera3d();	
	cam.GetCameraMtx(&mv);
	cam.GetProjectionMtx(&p);
	GXSetProjection( p, GX_PERSPECTIVE );

	Mtx	mtxTranslation;
	Mtx	mtxRotation;
	MTXIdentity(mtxTranslation);
	MTXIdentity(mtxRotation);
	MTXTrans(mtxTranslation, m_pos.x, m_pos.y, m_pos.z);

	MTXRotDeg(mtxRotation, 'Y', m_rotY);

	MTXConcat(mv, mtxTranslation, mv);	
	MTXConcat(mv, mtxRotation, mv);

	GXLoadPosMtxImm(mv, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);


	VEC3 vertex[2 * CUT_NUM + 2];
	int j =0;
	int thelta = 0;
	f32 C, S;
	f32 z, x;
	for(int i = 0; i<=CUT_NUM ; i++)
	{
		thelta = ((i << 16 ) / CUT_NUM ) & 65535;
		C = SinIdx(thelta);//(i % 2) == 0 ? 1 : 0;//FX_SinIdx(thelta);
		S = CosIdx(thelta);// == 0 ? 0 : 1;//FX_CosIdx(thelta);
		x = GetSizeX() * C ;
		z = GetSizeX() * S ;

		vertex[j].x = x;// + m_pos.x;
		vertex[j].y = -GetSizeY();// + m_pos.y;
		vertex[j].z = z;// + m_pos.z;
		j++;
		vertex[j].x = x;// + m_pos.x;
		vertex[j].y = GetSizeY();// + m_pos.y;
		vertex[j].z = z;// + m_pos.z;
		j++;
	}
	u32 groundColor =  0x888888FF;
	//CPhysics::DrawLine(vertex[0], vertex[1], groundColor);
	for(int i = 0; i < CUT_NUM; i++)
	{
		CPhysics::DrawLine(vertex[i*2], vertex[i*2+1], groundColor);
		CPhysics::DrawLine(vertex[i*2], vertex[i*2+2], groundColor);
		CPhysics::DrawLine(vertex[i*2+1], vertex[i*2+3], groundColor);
	}

	GXSetVtxDescv(descBackup);
	GXSetVtxAttrFmtv(GX_VTXFMT0, attrBackup);
*/
	if(!m_info.parentTransformSet)
		return;

	IVideoDriver * irrDriver = GetGame()->GetIrrDevice()->getVideoDriver();

	MTX4 matOldWorld = irrDriver->getTransform(video::ETS_WORLD);
	irrDriver->setTransform(video::ETS_WORLD, m_absTransform);

	VEC3 vertex[2 * CUT_NUM + 2];
	int j =0;
	int thelta = 0;
	f32 C, S;
	f32 z, x;
	for(int i = 0; i<=CUT_NUM ; i++)
	{
		thelta = i * 360 / CUT_NUM;
		S = SinDeg((f32)thelta);//(i % 2) == 0 ? 1 : 0;//FX_SinIdx(thelta);
		C = CosDeg((f32)thelta);// == 0 ? 0 : 1;//FX_CosIdx(thelta);
		x = GetSizeX() * C ;
		z = GetSizeX() * S ;

		vertex[j].X = x;// + m_pos.x;
		vertex[j].Y = -GetSizeY();// + m_pos.y;
		vertex[j].Z = z;// + m_pos.z;
		j++;
		vertex[j].X = x;// + m_pos.x;
		vertex[j].Y = GetSizeY();// + m_pos.y;
		vertex[j].Z = z;// + m_pos.z;
		j++;
	}
	glitch::video::SColor groundColor (0x88, 0x88, 0x88, 0xFF);
	//CPhysics::DrawLine(vertex[0], vertex[1], groundColor);
	for(int i = 0; i < CUT_NUM; i++)
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
int CPhysicsCylinder::GenerateMeshObj(rcMeshLoaderObj& meshObj, int vOff) const
{
	if(!m_info.parentTransformSet)
		return 0;
	VEC3 vertex[2 * CUT_NUM + 2];
	int j =0;
	int thelta = 0;
	f32 C, S;
	f32 z, x;
	for(int i = 0; i< CUT_NUM ; i++)
	{
		thelta = i * 360 / CUT_NUM;
		S = SinDeg((f32)thelta);//(i % 2) == 0 ? 1 : 0;//FX_SinIdx(thelta);
		C = CosDeg((f32)thelta);// == 0 ? 0 : 1;//FX_CosIdx(thelta);
		x = GetSizeX() * C ;
		z = GetSizeX() * S ;

		vertex[j].X = x;// + m_pos.x;
		vertex[j].Y = -GetSizeY();// + m_pos.y;
		vertex[j].Z = z;// + m_pos.z;
		j++;
		vertex[j].X = x;// + m_pos.x;
		vertex[j].Y = GetSizeY();// + m_pos.y;
		vertex[j].Z = z;// + m_pos.z;
		j++;
	}
	vertex[j++] = VEC3(0,  -GetSizeY(), 0);
	vertex[j++] = VEC3(0,  GetSizeY(), 0);
	for(int i=0; i<j; i++)
	{
		m_absTransform.transformVect(vertex[i]);
		meshObj.addVertex(vertex[i].X, vertex[i].Y, vertex[i].Z);
	}


	for(int i = 0; i< CUT_NUM; i++)
	{
		int k = i+1;
		if(k == CUT_NUM)
			k = 0;
		meshObj.addTriangle(vOff + 2 * i, vOff + 2 * i + 1, vOff + 2 * k);     //Side
		meshObj.addTriangle(vOff + 2 * k, vOff + 2 * i + 1, vOff + 2 * k + 1); //Side

		meshObj.addTriangle(vOff + 2 * k, vOff + 2 * CUT_NUM -2, vOff+2*i);    //Bottom
		meshObj.addTriangle(vOff + 2 * i + 1, vOff + 2 * CUT_NUM -1, vOff + 2 * k + 1); //TOP
	}

	return j;
}
#endif




//parameters
//@theSegment	SEGMENT3			the line struct for intersect test
//@outA		the length of intersect point to the line start point when the return value is true. 
//			(the intersect point is theSegment.seg.P0 + (outA * theSegment.dir )).
//return value:
//			return whether this geometry is intersected with the line

bool CPhysicsCylinder::IsIntersectSegment(const _Intersect_Segment &theSegment, f32 &outA, _Intersect_Info *pIntersectInfo /*= NULL*/) const
{
	//AABB TEST First
	//if(!AABB_IntersectSegment(theSegment))
	//	return false;
	_Intersect_Segment localLine;
	TransformSegmentToLocalCoordinateSystem(theSegment, localLine);

	//Test the column face First
	if(localLine.dir.X || localLine.dir.Z)
	{

		f32 scaleDir = 1.0;

		VEC3 l =  - localLine.seg.P0;	
		l.Y = 0;

		f32 dist2 = l.X * l.X + l.Z * l.Z;
		f32 r2 = GetSizeX() * GetSizeX();
		if(dist2 > r2)
		{

			f32 s = (localLine.dir.X * l.X + localLine.dir.Z * l.Z) ;
			if(s > 0  /*&&  dist2 >= r2*/) //only test the line intersect from the outer
			{
				f32 dirLen2 = localLine.dir.X * localLine.dir.X + localLine.dir.Z * localLine.dir.Z;
				f32 m2 = dist2 - s * s / dirLen2;
				if(m2 >= 0 && m2 <= r2)
				{
					f32 q = sqrt(r2 - m2);
					f32 scaleDir = sqrt(dirLen2);
					s /= scaleDir;
					if(localLine.dir.Y)
						outA =(s - q) / scaleDir; 
					else
						outA = s - q;
					if(outA <= localLine.len)			
					{
						//Test intersect point's y
						f32 y = localLine.seg.P0.Y + (outA * localLine.dir.Y );
						if(fabs(y) <= GetSizeY())
						{
							if(pIntersectInfo) {
								pIntersectInfo->pGeom = this;
								//	pIntersectInfo->pActor = m_pActor;
								VEC3 intersectPt;
								intersectPt.X = localLine.seg.P0.X + ((localLine.dir.X * outA ) );
								intersectPt.Z = localLine.seg.P0.Z + ((localLine.dir.Z * outA ) );
								intersectPt.Y = y;
								outA *= theSegment.len / localLine.len;


								pIntersectInfo->normal.X = intersectPt.X;
								pIntersectInfo->normal.Y = 0.0f;
								pIntersectInfo->normal.Z = intersectPt.Z;
								pIntersectInfo->normal.X *=	m_invAbsScale.X;
								pIntersectInfo->normal.Y *=	m_invAbsScale.Y;
								pIntersectInfo->normal.Z *=	m_invAbsScale.Z;
								m_absTransform.rotateVect(pIntersectInfo->normal);
								//							VEC3Normalize(&pIntersectInfo->normal, &pIntersectInfo->normal);
								pIntersectInfo->normal.normalize();
							}
							return true;
						}	
					}	
				}
			}		


		}
	}

	//Test the Top face
	if(localLine.seg.P0.Y  > GetSizeY())
	{
		outA = ( GetSizeY() - localLine.seg.P0.Y )  * localLine.len / (localLine.seg.P1.Y - localLine.seg.P0.Y);
		f32 x = localLine.seg.P0.X + (outA * localLine.dir.X );
		
		if( x <= GetSizeX() && x >= -GetSizeX())
		{
			f32 z = localLine.seg.P0.Z + (outA * localLine.dir.Z );
			if( z <= GetSizeX() && z >= -GetSizeX() && (x * x + z * z < GetSizeX() * GetSizeX()))
			{
				if(pIntersectInfo) {
					pIntersectInfo->pGeom = this;
					//pIntersectInfo->pActor = m_pActor;
					pIntersectInfo->normal.X = 0;
					pIntersectInfo->normal.Y = 1.0f;
					pIntersectInfo->normal.Z = 0;
					m_absTransform.rotateVect(pIntersectInfo->normal);
					pIntersectInfo->normal.normalize();
					outA *= theSegment.len / localLine.len;
				}

				return true;
			}
		}	
	}

	//Test the Bottom face



	return false;
}

bool CPhysicsCylinder::IsPointInThis(const VEC3 &pt) const
{
	if(!AABB_IntersectPoint(pt))
		return false;
	VEC3 localPt;
	m_invAbsTransform.transformVect(localPt, pt);
	if(  fabs(localPt.X) <= GetSizeX() &&
		fabs(localPt.Z) <= GetSizeX() &&
		fabs(localPt.Y) <= GetSizeY() &&
		localPt.X * localPt.X+ localPt.Z * localPt.Z  <= GetSizeX() * GetSizeX())
		return true;

	return false;

}

//bool CPhysicsCylinder::IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo /*= NULL*/) const
//{
//	return false;
//}