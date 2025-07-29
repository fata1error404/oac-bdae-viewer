// Physics.h: interface for the CPhysicsMesh class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PHYSICS_H__4EAF61B3_E918_469B_AD15_4A4D808E7CEF__INCLUDED_)
#define AFX_PHYSICS_H__4EAF61B3_E918_469B_AD15_4A4D808E7CEF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// #include "Lib3D/GroundMesh.h"
//
// #include "ThreadSingleton.h"
#include "PhysicsGeom.h"
#include <vector>

// #define PHYSICS_HIGH_COVER	2  //Same as Wall, but different in AI
// This one has been changed to use Geom type, Not in Ground Mesh   [11/15/2007 baibo.zhang]

// class CLevel;
class CResFileStream;
// class CPhysicsGeom;
// class VEC3;

class Entity;
class Mutex;

// #ifdef _MMO_SERVER_
class CPackPatchReader;
// #else
class CResFileStream;
class PhysicsGeomPool;
// class glitch::io::CZipResReader;
// #endif
struct WATER_INFO;

enum
{
	TRIGGER_AREA_IN = 1 << 0,
	TRIGGER_AREA_MID = 1 << 1,

	TRIGGER_FLAGS = (TRIGGER_AREA_IN | TRIGGER_AREA_MID),

	MAT_FLAG_START = 1 << 8,
	MAT_FLAG_GROUND = MAT_FLAG_START,
	MAT_FLAG_GRASS = 1 << 9,
	MAT_FLAG_WOOD = 1 << 10,
	MAT_FLAG_ROCK = 1 << 11,
	MAT_FLAG_WATER = 1 << 12,
	MAT_FLAG_METAL = 1 << 13,

	PHY_FLAG_IN = 1 << 14,
};

class CPhysics : public CThreadSingleton<CPhysics>
{
public:
	static int IntersectionRay3AABB(RAY3 *pRay, AABB *pAABB, float *pDist);

	// CBackGround *						m_pLevel;

	CPhysics(/*CBackGround * pLevel*/);
	~CPhysics();
	static bool LoadModelPhysics(const char *resourceName, CPhysicsGeom *&physicsGeom, bool isEntityHouse = false);
	static CPhysicsGeom *LoadPhysics(const char *filename); // main loading function

	static CPhysicsGeom *LoadPhysics(CResFileStream *pFile); // main loading function
	void UnloadPhysics();
	// void MoveSlideOnGround(VEC3 &curPos, VEC3 endPos, f32 upSlope = 1.5f, f32 downSlope = 2.0f);
	static void OpenZipPhysics();
	static void CloseZipPhysics();

	void Init() {}
	void DeInit() {}

	// bool GetHeightWithActor(f32 x, f32 z, f32 yMax, f32 * pRetHeight, Entity *pIgnoreActor, int flag,_Intersect_Info *intersectInfo);
	f32 GetHeightWithWater(f32 x, f32 z, f32 yMax, uint32 mapId, VEC3 *pGroundNormal, const Entity *pIgnoreActor, bool walkOnWater, bool doesSwim, WATER_INFO *waterInfo = NULL, _Intersect_Info *intersectInfo = NULL);
	// bool GetHeight(f32 x, f32 z, f32 yMax, f32 * pRetHeight, Entity *pIgnoreActor,int * pGroundMaterial,VEC3 * pGroundNormal, int flag);
	f32 GetHeight(f32 x, f32 z, f32 yMax, uint32 mapId, VEC3 *pGroundNormal, const Entity *pIgnoreActor, WATER_INFO *waterInfo = NULL, _Intersect_Info *intersectInfo = NULL);
	int Move(VEC3 &curPos, const VEC3 &speed, uint32 mapId, Entity *pIgnoreActor, WATER_INFO &waterInfo, bool checkWhenNoSpeed = false, _Intersect_Info *intersectInfo = NULL, bool fitOnGround = true, bool walkOnWater = false, f32 upSlope = 3.0f, f32 downSlope = 4.0f);
#if RENDER_GEOM
	void Render();
	static void DrawTriangle(VEC3 p1, VEC3 p2, VEC3 p3, u32 color);
	static void DrawLine(VEC3 p1, VEC3 p2, u32 color);
	static void DrawLine(VEC3 *vertex, int vNum, u32 color);
#else
	void Render() {};
#endif
protected:
	int MoveInAir(VEC3 &curPos, const VEC3 &speed, uint32 mapId, Entity *pIgnoreActor, WATER_INFO &waterInfo, _Intersect_Info *intersectInfo /*= NULL*/);

public:
	void RegisterGeom(CPhysicsGeom *pGeom, std::vector<CPhysicsGeom *> *geomList);
	void UnRegisterGeom(CPhysicsGeom *pGeom, std::vector<CPhysicsGeom *> *geomList, bool needDel = false);

	static CPhysicsGeom *NewOnePhysicsGeom(int geomType, const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL);
	static void DeleteGeom(CPhysicsGeom *&pGeom);
	// CPhysicsGeom * AddOnePhysicsGeom(int geomType, const VEC3 &pos, f32 rotZ,  f32 sizeX, f32 sizeY, f32 sizeZ);

	// void HitTargetTest(_Intersect_Segment &theLine, _Hit_Target_Info &hitTargetInfo, int teamFilter) const;
	// void GetAimTargetPoint(int x, int y, int nMaxDist, _Hit_Target_Info &hitTargetInfo, int teamFilter = -1) const;
	bool _IntersectTestLine(const VEC3 &v0, const VEC3 &v1, uint32 mapId, const Entity *pIgnoreActor, _Intersect_Info *pIntersectInfo = NULL, bool checkTerrain = false, bool checkWater = false, bool offsetGround = false) const;
	// bool IntersectTestLine(const VEC3 &v0, const VEC3 &v1, int geomInfoFlag, int geomInfoIgnoreFlag, Entity *pIgnoreActor, _Intersect_Info *pIntersectInfo = NULL) const
	//{
	//	return _IntersectTestLine(false, v0, v1, geomInfoFlag, geomInfoIgnoreFlag, pIgnoreActor,pIntersectInfo);
	// }
	bool IntersectTestPoint(const VEC3 &pt, int geomInfoFlag, int geomInfoIgnoreFlag, Entity *pIgnoreActor) const;
	bool IntersectTestSphere(const VEC3 &v0, f32 &radius, const VEC3 &speed, int geomInfoFlag, int geomInfoIgnoreFlag, Entity *pIgnoreActor, _Intersect_Info *pIntersectInfo = NULL) const;

	static void EnablePhysicsBackFaceCulling(bool bEnable)
	{

		CPhysicsGeom::s_faceCulling = bEnable ? CPhysicsGeom::PHYSICS_BACK_FACE_CULLING : CPhysicsGeom::PHYSICS_FACE_CULLING_NONE;
	}

	// bool IntersectTestLineWithGround(const VEC3 &v0, const VEC3 &v1, _Intersect_Info *pIntersectInfo = NULL) const
	//{
	//	return _IntersectTestLine(true, v0, v1, -1, 0, NULL, pIntersectInfo);
	// }

	//	CLevel* pLevel;
	// #ifdef _MMO_SERVER_
	static CPackPatchReader *s_zipPhysics;
	static PhysicsGeomPool *s_physicsPool;
	// #endif
	std::vector<CPhysicsGeom *> m_pGeomList;
	// std::vector<CPhysicsGeom *> * m_pGeomListArray;
	// mutable int			m_nGeomNb;
	// mutable std::vector<CPhysicsGeom *> m_groundMeshList; //GroundMesh List
	static Mutex m_mutex;
};

#endif // !defined(AFX_PHYSICS_H__4EAF61B3_E918_469B_AD15_4A4D808E7CEF__INCLUDED_)
