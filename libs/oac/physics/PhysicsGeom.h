// PhysicsGeom.h: interface for the CPhysicsGeom class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PHYSICSGEOM_H__FADB4E8A_CA4E_4744_A4D8_3D4700B05164__INCLUDED_)
#define AFX_PHYSICSGEOM_H__FADB4E8A_CA4E_4744_A4D8_3D4700B05164__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define INVALID_TERRAIN_HEIGHT -10000
// #include "AIConstantDef.h"

inline float CosDeg(float fDegree)
{
	return cosf(DEG_TO_RAD(fDegree));
}

inline float SinDeg(float fDegree)
{
	return sinf(DEG_TO_RAD(fDegree));
}
#ifdef _MMO_GAME_
#if defined(_DEBUG) || defined(_AFANTY_) || defined(_WIN32)
#define RENDER_GEOM 1
#else
#define RENDER_GEOM 0
#endif
#else
#define RENDER_GEOM 0
#endif

#ifdef _WINDOWS
// #include "..\M3D\M3DDevice2.h"
#endif

#define PHYSICS_GEOM_TYPE_SPHERE 1
#define PHYSICS_GEOM_TYPE_PLANE 2
#define PHYSICS_GEOM_TYPE_BOX 3
#define PHYSICS_GEOM_TYPE_CYLINDER 4
#define PHYSICS_GEOM_TYPE_HORIZONTAL_CYLINDER 5
#define PHYSICS_GEOM_TYPE_TRIANGLE 6
#define PHYSICS_GEOM_TYPE_MESH 7
#define PHYSICS_GEOM_TYPE_INFINITE_PLANE 8

enum
{
	HYSICS_INFO_MATERIAL_GROUND = 0,
	HYSICS_INFO_MATERIAL_WATER, //= 1/

};
// #define PHYSICS_INFO_MATERIAL_MASK				0x0000000F

// #define PHYSICS_INFO_BLOCK_BULLET				0x00000001
// #define PHYSICS_INFO_BLOCK_MOVING				0x00000002
////#define PHYSICS_INFO_LOW_COVER  				0x00000004
// #define PHYSICS_INFO_BLOCK_CANNON  				0x00000004
// #define PHYSICS_INFO_FRIEND						0x00000008
// #define PHYSICS_INFO_CAN_HIT					0x00000010
////Friend means player team
// #define PHYSICS_INFO_BLOCK_ONLY					0x00000020
//
//
////#define PHYSICS_INFO_DIE_EXPLODE				0x00000020
// #define PHYSICS_INFO_RECV_SHADOW				0x00000040

#define PHYSICS_INFO_FLAG_MASK 0x0000FFFF
#define PHYSICS_INFO_MTL_MASK 0x0000FF00
// #define PHYSICS_INFO_AREA_ID_SHIFT				8

// #define PHYSICS_INFO_INTERNAL_MASK				0x000F0000
// #define PHYSICS_INFO_INTERNAL_SHIFT				16
// #define PHYSICS_INFO_INTERNAL_ALONG_X			0x00010000
// #define PHYSICS_INFO_INTERNAL_ALONG_Z			0x00020000

// #define PHYSICS_INFO_OBJ_ID_MASK				0xFFF00000
// #define PHYSICS_INFO_OBJ_ID_SHIFT				20

// #define PHYSICS_INFO_SET_AREA_ID(areaId)	(m_info |= ((areaId << PHYSICS_INFO_AREA_ID_SHIFT) & PHYSICS_INFO_AREA_ID_MASK))
// #define PHYSICS_INFO_GET_AREA_ID	((s8)((m_info & PHYSICS_INFO_AREA_ID_MASK) >> PHYSICS_INFO_AREA_ID_SHIFT))

// #define PHYSICS_INFO_SET_OBJ_ID(objId)	(m_info |= ((objId << PHYSICS_INFO_OBJ_ID_SHIFT) & PHYSICS_INFO_OBJ_ID_MASK))
// #define PHYSICS_INFO_GET_OBJ_ID	((u16)((m_info & PHYSICS_INFO_OBJ_ID_MASK) >> PHYSICS_INFO_OBJ_ID_SHIFT))

#define PHYSICS_ROT_Y_THRESHOLD 2.0 // 2 degree
#define PHYSICS_ROT_DEG_360 360.0
#define PHYSICS_ROT_DEG_270 270.0
#define PHYSICS_ROT_DEG_180 180.0
#define PHYSICS_ROT_DEG_90 90.0

#define BOX_PT_DOWN_FRONT_LEFT 0
#define BOX_PT_DOWN_FRONT_RIGHT 1
#define BOX_PT_DOWN_BACK_RIGHT 2
#define BOX_PT_DOWN_BACK_LEFT 3

// struct _AABB_BOX {
//	VEC3 minV;
//	VEC3 maxV;
// };

class Entity;
class CPhysicsGeom;
// struct _IntersectA_Line {
//	VEC3 v0;		//the start point of the line
//	VEC3 v1;		//the end point of the line
//	f32	 len;	//the length of the line = Sqrt( Dot(v1- v0, v1 - v0))
//	VEC3 vDir;	//the unit direction vector of v1 - v0
//	VEC3 minV;	//the min vector: minV.x = min(v0.x, v1.x), minV.y = min(v0.y, v1.y), minV.z = min(v0.z, v1.z)
//	VEC3 maxV;	//the max vector: maxV.x = max(v0.x, v1.x), maxV.y = max(v0.y, v1.y), maxV.z = max(v0.z, v1.z)
// };

struct _Intersect_Info
{
	_Intersect_Info();
	const CPhysicsGeom *pGeom;
	// Entity *		pActor;
	VEC3 normal;
	VEC3 point;
	//////////////////////////////////////////////////////////////////////////
	// VEC3			GroundPoint,GroundNormal;
	// int					nGroundMaterialType;
};

struct SEGMENT3
{
	VEC3 P0, P1;
};

class RAY3
{
public:
	RAY3(VEC3 pos, VEC3 dir, bool bNormalize)
	{
		P = pos;
		d = dir;
		if (bNormalize)
			d.normalize();
	};
	RAY3() {}
	VEC3 P, d;
};

struct _Intersect_Segment
{
	SEGMENT3 seg;
	//	VEC3			seg_start,seg_end;
	// the start point of the line
	// the end point of the line
	f32 len;  // the length of the line = Sqrt( Dot(v1- v0, v1 - v0))
	VEC3 dir; // the unit direction vector of v1 - v0
	AABB aabb;
	//	VEC3			aabb_min,aabb_max;
	// the min vector: minV.x = min(v0.x, v1.x), minV.y = min(v0.y, v1.y), minV.z = min(v0.z, v1.z)
	// the max vector: maxV.x = max(v0.x, v1.x), maxV.y = max(v0.y, v1.y), maxV.z = max(v0.z, v1.z)
};

struct _Intersect_Ray
{
	RAY3 ray;
	//	VEC3			ray_start,ray_dir;
	int major;
	bool isAlongY;
};

// struct _Intersect_Sphere {
//	SEGMENT3	seg;
//				//the start point of the line
//				//the end point of the line
//	f32			len;	//the length of the line = Sqrt( Dot(v1- v0, v1 - v0))
//	VEC3		dir;	//the unit direction vector of v1 - v0
//	AABB		aabb;
//			//the min vector: minV.x = min(v0.x, v1.x), minV.y = min(v0.y, v1.y), minV.z = min(v0.z, v1.z)
//				//the max vector: maxV.x = max(v0.x, v1.x), maxV.y = max(v0.y, v1.y), maxV.z = max(v0.z, v1.z)
// };

#define HIT_TARGET_TYPE_AIR 0
#define HIT_TARGET_TYPE_GROUND 1
#define HIT_TARGET_TYPE_WALL 2
#define HIT_TARGET_TYPE_ACTOR 3

#define HIT_TARGET_TEST_TEAM_FILTER_NONE 0
#define HIT_TARGET_TEST_TEAM_FILTER_0 0x1
#define HIT_TARGET_TEST_TEAM_FILTER_1 0x2
#define HIT_TARGET_TEST_TEAM_FILTER_ALL 0x3

struct _Hit_Target_Info
{
	VEC3 position;		 // the target Position
	f32 dist;			 // the distance between the target position and the start point of the line
	CPhysicsGeom *pGeom; // the target geom pointer, call CPhysicsGeom::canHit() to make sure the geom can be hitted
	int type;			 // the target Position's type
	int headHit;		 // whether point to the actor's head
	Entity *pActor;		 // the target's actor if any actor
};
#ifdef _AFANTY_
class rcMeshLoaderObj;
#endif

// the super abstract class for the Geometry of physics.
class CPhysicsGeom
{
public:
	static void ComputeIntersectLine(_Intersect_Segment &theSegment);
	static void UpdateIntersectLineAABB(_Intersect_Segment &theSegment)
	{
		CalSegmentAABB(theSegment.seg.P0, theSegment.seg.P1, theSegment.aabb);
	}
	static void CalSegmentAABB(const VEC3 &P0, const VEC3 &P1, AABB &aabb);
	static void CalRayFromSegment(const _Intersect_Segment &theSegment, _Intersect_Ray &theRay);

	static bool IsLineIntersectLine(f32 x00, f32 y00, f32 x01, f32 y01, f32 x10, f32 y10, f32 x11, f32 y11);
	static bool IsVerticalRayIntersectTriangle(f32 *pPoint, f32 x, f32 z, f32 &outY);

	static int IntersectionRay3AABB(const RAY3 *pRay, const AABB *pAABB, float *pDist, VEC3 *pPt = NULL, VEC3 *pNormal = NULL);
	static bool IsRayIntersectTriangle(const _Intersect_Ray &theRay, f32 *pPoint, f32 &outK, VEC3 *outPos);

public:
	bool IntersectionAABB(const AABB *pAABB1, const AABB *pAABB2) const;

	void WorldToLocal(const VEC3 &worldPos, VEC3 &localPos) const;
	void LocalToWorld(const VEC3 &localPos, VEC3 &worldPos) const;
	bool AABB_IntersectSegment(const _Intersect_Segment &theSegment) const;
	bool AABB_IntersectPoint(const VEC3 &pt) const;
	bool AABB_IntersectAABB(const AABB &pAABB) const;
	virtual CPhysicsGeom *Clone() const = 0;
	// bool AABB_IntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed) const;

	//	static bool Is2dBallIntersect2dLine(const M3DXVector2 &ballC, int ballR, const SEGMENT3_2d &theSegment, f32 &outA);
	void TransformSegmentToLocalCoordinateSystem(const _Intersect_Segment &theSegment, _Intersect_Segment &outLocalSegment) const;
	CPhysicsGeom(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL);
	virtual ~CPhysicsGeom();
#ifdef _AFANTY_
	virtual int GenerateMeshObj(rcMeshLoaderObj &meshObj, int vOff) const
	{
		return 0;
	};
#endif
	// Method of intersect test with a line
	// parameters
	//@v0		the line start point
	//@v1		the line end point
	//@outA		the interpolate value of intersect point when the return value is true.
	//			(the intersect point is (1 - outA)v0 + outA * v1). And to keep precision, the outA is a fixed point with precision of COS_SIN_SHIFT.
	// return value:
	//			return whether this geometry is intersected with the line
	virtual bool IsIntersectSegment(const _Intersect_Segment &theSegment, f32 &outA, _Intersect_Info *pIntersectInfo = NULL) const = 0;
	virtual bool IsPointInThis(const VEC3 &pt) const = 0;

	// virtual bool IsIntersectPhysicsGeom(const CPhysicsGeom& theGeom, _Intersect_Info *pIntersectInfo = NULL) const;
	// virtual bool IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo = NULL) const;

	virtual void Render() const {}
	int GetGeomType() const
	{
		return m_geomType;
	}
	int GetInfoFlag() const
	{
		return m_info.s32value & PHYSICS_INFO_FLAG_MASK;
	}
	void SetInfoFlag(int infoFlag)
	{
		m_info.s32value &= ~PHYSICS_INFO_FLAG_MASK;
		m_info.s32value |= (infoFlag & PHYSICS_INFO_FLAG_MASK);
	}
	void SetObjId(int objId)
	{
		m_info.entityIndex = objId;
	}
	int GetObjId() const
	{
		return m_info.entityIndex;
	}
	void SetSerilParentTransform(const MTX4 &transform, const VEC3 *scale = NULL); // if scale = NULL, keep last setting

	AABB &GetCurAABB() { return m_AABB; }
	const AABB &GetCurAABB() const { return m_AABB; }

public:
	CPhysicsGeom *m_pNext;
	CPhysicsGeom *m_pTrigger;
	Entity *m_pActor;

	// Group Geom's Bounding Box in the world Matrix
	AABB m_groupAABB; // the AABB

protected:
	// void SetPosition(const VEC3 &pos) {
	//	m_pos = pos;
	// }
	// const VEC3& GetPosition() const { return m_pos; }
	// VEC3& GetPosition() { return m_pos; }

	void SetParentTransform(const MTX4 &transform, const VEC3 *scale);
	virtual void SetRotationY(f32 rotY) { m_rotY = rotY; /*m_cosRotY = CosDeg(m_rotY); m_sinRotY = SinDeg(m_rotY);*/ }
	f32 GetRotationY() const { return m_rotY; }

	// void SetAreaId(int areaId) {
	//	m_info &= ~PHYSICS_INFO_AREA_ID_MASK;
	//	PHYSICS_INFO_SET_AREA_ID(areaId);
	// }
	// int GetAreaId() const {
	//	return PHYSICS_INFO_GET_AREA_ID;
	// }

	// bool canHit() const {
	//	return (m_info & PHYSICS_INFO_CAN_HIT) != 0;
	// }

	// bool blockOnly() const {
	//	return (m_info & PHYSICS_INFO_BLOCK_ONLY) != 0;
	// }

	// int	GetLife() const { return m_life; }
	// void SetLife(int life) { m_life = life; }

	// void SetSpeed(f32 speedX, f32 speedY, f32 speedZ) {
	//	m_speed.X = speedX;
	//	m_speed.Y = speedY;
	//	m_speed.Z = speedZ;
	// }

	// void SetSpeed(const VEC3& speed){
	//	m_speed = speed;
	// }
	// VEC3 GetSpeed() const {
	//	return m_speed;
	// }

	void SetSize(f32 halfW, f32 halfH, f32 halfL)
	{
		m_halfSize.X = halfW;
		m_halfSize.Y = halfH;
		m_halfSize.Z = halfL;
	}
	void UpdateAABB();

	f32 GetSizeY() const
	{
		return m_halfSize.Y;
	};
	f32 GetSizeX() const
	{
		return m_halfSize.X;
	};
	f32 GetSizeZ() const
	{
		return m_halfSize.Z;
	};

	// f32 GetFinalSizeZ() const {
	//	return GetSizeZ() * m_scale;
	// }

	// f32 GetFinalSizeX() const {
	//	return GetSizeX() * m_scale;
	// }

	// f32 GetFinalSizeY() const {
	//	return GetSizeY() * m_scale;
	// }
	f32 GetMaxSize() const
	{
		f32 sizeY = GetSizeX();
		f32 sizeZ = GetSizeZ();
		f32 sizeX = GetSizeX();
		return sizeY > sizeX ? (sizeY > sizeZ ? sizeY : sizeZ) : (sizeX > sizeZ ? sizeX : sizeZ);
	}
	// f32 GetFinalMaxSize() const {
	//	return GetMaxSize() * m_scale;
	// }
	// void SetScale(f32 scale) {
	//	m_scale = scale;
	//	//UpdateAABB();
	// }
	// f32 GetScale() const {
	//	return m_scale;
	// }

	// bool	m_bPGFollowDummy;
	// char	m_sPGFollowDummyName[32];
protected:
	// Along X and Along Z only used in CPhysicsPlane and CPhysicsHorizontalCylinder to speed up the calculation
	// void ResetAlongXZ()
	//{
	//	m_info &= ~(PHYSICS_INFO_INTERNAL_ALONG_X | PHYSICS_INFO_INTERNAL_ALONG_Z);
	// }

	// void SetAlongX() {
	//	m_info &= ~PHYSICS_INFO_INTERNAL_ALONG_Z;
	//	m_info |= PHYSICS_INFO_INTERNAL_ALONG_X;
	// }
	// void SetAlongZ() {
	//	m_info &= ~PHYSICS_INFO_INTERNAL_ALONG_X;
	//	m_info |= PHYSICS_INFO_INTERNAL_ALONG_Z;
	// }
	// bool IsAlongX() const {
	//	return ((m_info & PHYSICS_INFO_INTERNAL_ALONG_X) != 0);
	// }
	// bool IsAlongZ() const {
	//	return ((m_info & PHYSICS_INFO_INTERNAL_ALONG_Z) != 0);
	// }

protected:
	// Absolute Transform In the world (= ParentTransform * Local Transform)
	MTX4 m_absTransform;
	MTX4 m_invAbsTransform;
	VEC3 m_absScale;	// Scale Part in AbsTransform
	VEC3 m_invAbsScale; // Invert Scale Part in AbsTransform

	// const MTX4* m_parentTransform;

	// Local Transform Part
	VEC3 m_halfSize;
	VEC3 m_pos; // Transform  --- Position
	f32 m_rotY; // Transform  --- RotY, only allowed this type of rotation, in degree
	//	f32			 m_scale;   //Transform  ---- uniform scale only

	// f32          m_cosRotY;		//the cosine value of m_rotY
	// f32			 m_sinRotY;		//the sine value of m_rotY

	// Bounding Box in the world Matrix
	AABB m_AABB; // the AABB

	// Other Members
	// VEC3	 m_speed;		//cm / Frame
	union
	{
		int s32value;
		struct
		{
			int house_inner_type : 8; // indicate entity_house flag
			int mtl_type : 8;

			//		int parentTransformDirty	: 1
			int entityIndex : 14;
			int parentTransformSet : 1; // 0 : not set Parent Transform, 1 : set
			int _pad1 : 1;
		};

	} m_info; // Extra Information: such as if it is a low cover
	// int			 m_life;	// life
	int m_geomType; // GeomType

public:
	enum _PHYSICS_FACE_CULLING
	{
		PHYSICS_FACE_CULLING_NONE = 0,
		PHYSICS_BACK_FACE_CULLING = 1,
		PHYSICS_FRONT_FACE_CULLING = 2,
	};
	static _PHYSICS_FACE_CULLING s_faceCulling;
};

#endif // !defined(AFX_PHYSICSGEOM_H__FADB4E8A_CA4E_4744_A4D8_3D4700B05164__INCLUDED_)
