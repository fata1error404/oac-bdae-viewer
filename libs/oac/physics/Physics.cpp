// Physics.cpp: implementation of the CPhysicsMesh class.
//
//////////////////////////////////////////////////////////////////////
// #include "preHeader.h"

#include "Physics.h"
#ifdef _MMO_GAME_
#include "game/game/game.h"
#endif
#include "PhysicsGeom.h"
// #include "../System/NDSFile.h"
#include "PhysicsMesh.h"
#ifdef _MMO_GAME_
#include "game/Entities/Entity.h"
#endif
#include "PhysicsCylinder.h"
#include "PhysicsBox.h"
#include "PhysicsPlane.h"
#include "io/ResFileStream.h"
#include "io/PackPatchReader.h"
#include "io/IReadResFile.h"
#include "PhysicsGeomPool.h"
// #ifdef _MMO_GAME_
// #include "game/GameResMgr.h"
// // #include "engine/io/IFileStream.h"
// // #include "engine/io/FileSystemWrapper.h"
// #include "engine/3d/SceneMgr.h"
// #include "engine/3d/TerrainTiled.h"
// #include "Profile.h"

#else
// #include "ResFileSystemWrapper.h"

// #include "Map/Map.h"
// #include "Map/InstanceMgr.h"
#endif
#include "Mutex.h"
#include "OS.h"

// #ifdef _MMO_SERVER_
CPackPatchReader *CPhysics::s_zipPhysics = NULL;
PhysicsGeomPool *CPhysics::s_physicsPool = NULL;
// #endif
// #include "PhysicsHorizontalCylinder.h"
// #include "AreaManager.h"
// #include "Camera.h"

#define USE_NW4R_COORDINATE 1
// NW4R --- Coordinate System:
//      Y
//      |___X
//     /
//    Z
//
//  The original physics Resource Coordinate System:
//      Z
//      |  Y
//      | /
//      |/______X
//
//  The relation is
//     Xn = X
//     Yn = Z
//     Zn = -Y
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
#define MIN_IN_THREE(x, y, z) ((x < y) ? (x < z ? x : z) : (y < z ? y : z))
#define MAX_IN_THREE(x, y, z) ((x > y) ? (x > z ? x : z) : (y > z ? y : z))

#ifdef _MMO_GAME_
#endif

CPhysics::CPhysics(/*CBackGround * pLevel*/)
{
	// m_pM3DDevice = pM3DDevice;
	// m_pAIManager = pAIManager;
	// m_pLevel = pLevel;
	//
	//	pLevel = CMyGame::GetGame()->GetLevel();

	//	m_pGeom = gll_new CPhysicsBox();
	//	((CPhysicsBox * )m_pGeom)->SetSize(100, 50, 50);

	//	m_pGeom = gll_new CPhysicsHorizontalCylinder();
	//	((CPhysicsHorizontalCylinder *)m_pGeom)->SetSize(100, 50);

	//	m_pGeom = gll_new CPhysicsCylinder();
	//	((CPhysicsCylinder *)m_pGeom)->SetSize(100, 50);

	//	m_pGeom = gll_new CPhysicsSphere();
	//	((CPhysicsSphere *)m_pGeom)->SetRadius(100);
	//	m_pGeom = gll_new CPhysicsPlane();
	//	((CPhysicsPlane *)m_pGeom)->SetSize(100, 50, 50);
	//	m_pGeom->SetPosition( VEC3(3347, 1500, 3315));
	//	m_pGeom->SetRotationZ(-16384);

	// m_nGeomNb = 0;
	m_pGeomList.clear();
	// m_groundMeshList.clear();
}
#if RENDER_GEOM
glitch::video::CMaterialPtr s_physicsMtrl = NULL;
#endif
CPhysics::~CPhysics()
{

	UnloadPhysics();
//	SAFE_DEL(m_pGeom);
#if RENDER_GEOM
	s_physicsMtrl = NULL;
#endif
}
struct _GEOM_PROPERTY
{
	//	int type;
	int flags;
	float rotY;
	float transX;
	float transY;
	float transZ;
	float halfSizeX;
	float halfSizeY;
	float halfSizeZ;
	void loadFromFile(CResFileStream *pFile);
};

void _GEOM_PROPERTY::loadFromFile(CResFileStream *pFile)
{
	// type = pFile->ReadU8();
	flags = pFile->ReadS32(); // PHYSICS_INFO_BLOCK_BULLET | PHYSICS_INFO_BLOCK_MOVING | PHYSICS_INFO_HIGH_COVER
	rotY = pFile->Readfloat() * 180.0f / PI;
#if USE_NW4R_COORDINATE
	transX = pFile->Readfloat();
	transZ = -pFile->Readfloat();
	transY = pFile->Readfloat();

	halfSizeX = pFile->Readfloat();
	halfSizeZ = pFile->Readfloat();
	halfSizeY = pFile->Readfloat();

#else
	transX = pFile->Readfloat();
	transY = pFile->Readfloat();
	transZ = pFile->Readfloat();

	halfSizeX = pFile->Readfloat();
	halfSizeY = pFile->Readfloat();
	halfSizeZ = pFile->Readfloat();
#endif
}

void CPhysics::UnloadPhysics()
{
	// std::vector<CPhysicsGeom *>::iterator it;
	// for (it = m_pGeomList.begin(); it != m_pGeomList.end(); ++it)
	//{
	//	CPhysicsGeom *pGeom = *it;
	//	if(pGeom->m_pAcctor == NULL)
	//		SAFE_DEL(pGeom); //only delete the geom without a parent of any acctor.
	// }
	// m_pGeomList.clear();
	// m_groundMeshList.clear();
	// m_nGeomNb = 0;
}

void CPhysics::OpenZipPhysics()
{
	if (s_zipPhysics == NULL)
	{
#ifdef _MMO_GAME_
		s_zipPhysics = GameResMgr::instance()->CreateZipPatchReader("model_phy.bin", true);
#else
		IReadResFile *zipFile = sOS.CreateReadResFile("model_phy.bin");
		if (zipFile)
		{
			s_zipPhysics = gll_new CPackPatchReader(zipFile, true, false);
			zipFile->drop();
		}
#endif
	}
	s_physicsPool = gll_new PhysicsGeomPool();
}

void CPhysics::CloseZipPhysics()
{
#ifdef _MMO_GAME_
	GameResMgr::instance()->DropZipPatchReader(s_zipPhysics);
#else
	if (s_zipPhysics)
	{
		gll_delete s_zipPhysics;
		s_zipPhysics = NULL;
	}
#endif
	SAFE_DEL(s_physicsPool);
}

#ifdef _MMO_SERVER_
bool g_isDevice2Gor3G = false;
#endif
bool CPhysics::LoadModelPhysics(const char *resourceName, CPhysicsGeom *&physicsGeom, bool isEntityHouse)
{
#ifndef MONKEY
	// #ifdef PHYSICS_SUPPORT

	if (physicsGeom)
	{
		GLL_ASSERT(false);
		// CPhysics::instance()->UnRegisterGeom(*physicsGeom, true);
		//*physicsGeom = NULL;
	}
	char modelPath[512];

	strcpy(modelPath, resourceName);
	char *q = strrchr(modelPath, '.');
	if (q)
		*q = 0;
	int modelNameLen = strlen(modelPath);
	if (modelPath[modelNameLen - 1] == 'h' && modelPath[modelNameLen - 2] == '_')
	{
		modelPath[modelNameLen - 2] = 0;
		modelNameLen -= 2;
	}

	char tempModelPath[512];
	// IReadResFile *readFile;
	if (g_isDevice2Gor3G)
	{
		strcpy(tempModelPath, modelPath);
		strcat(tempModelPath, "_s.phy");
		if (s_zipPhysics)
		{
			readFile = s_zipPhysics->openFile(tempModelPath);
		}
		else
			readFile = createReadFile(tempModelPath);

		if (readFile)
			strcat(modelPath, "_s.phy");
		else
			strcat(modelPath, ".phy");
	}
	else
		strcat(modelPath, ".phy");
	for (int i = 0; i < modelNameLen; i++)
	{
		if (modelPath[i] >= 'A' && modelPath[i] <= 'Z')
			modelPath[i] += 'a' - 'A';
	}

	physicsGeom = CPhysics::LoadPhysics(modelPath);
	if (physicsGeom && isEntityHouse)
	{
		modelPath[modelNameLen] = 0;
		strcpy(tempModelPath, modelPath);
		strcat(tempModelPath, "_inside_s.phy");
		if (g_isDevice2Gor3G)
		{
			if (s_zipPhysics)
			{
				readFile = s_zipPhysics->openFile(tempModelPath);
			}
			else
				readFile = createReadFile(tempModelPath);
			if (readFile)
				strcat(modelPath, "_inside_s.phy");
			else
				strcat(modelPath, "_inside.phy");
		}
		else
			strcat(modelPath, "_inside.phy");
		CPhysicsGeom *physicsGeomInside = CPhysics::LoadPhysics(modelPath);
		if (physicsGeomInside)
		{
			// find the inside trigger;
			CPhysicsGeom *trigger = physicsGeomInside;
			CPhysicsGeom *pPrev = NULL;
			CPhysicsGeom *triggerTail = NULL;
			CPhysicsGeom *pTemp = NULL;
			CPhysicsGeom *insideTrigger = NULL;

			bool isTrigger = false;
			while (trigger)
			{
				isTrigger = false;
				if (trigger->GetInfoFlag() & TRIGGER_FLAGS)
				{
					isTrigger = true;
					// remove trigger from chain
					if (trigger == physicsGeomInside)
					{
						physicsGeomInside = physicsGeomInside->m_pNext;
					}
					else
					{
						GLL_ASSERT(pPrev);
						pPrev->m_pNext = trigger->m_pNext;
					}

					// add trigger to trigger chain.
					if (!insideTrigger)
					{
						insideTrigger = trigger;
						triggerTail = trigger;
					}
					else
					{
						triggerTail->m_pNext = trigger;
						triggerTail = trigger;
					}
				}

				if (isTrigger)
				{
					pTemp = trigger->m_pNext;
					trigger->m_pNext = NULL;
					trigger = pTemp;
				}
				else
				{
					pPrev = trigger;
					trigger = trigger->m_pNext;
				}
			}
			// Added Inside Geom to Outside geom
			pTemp = physicsGeom;
			while (pTemp->m_pNext)
			{
				pTemp = pTemp->m_pNext;
			}
			pTemp->m_pNext = physicsGeomInside;
			// Added Trigger to outside Geom's trigger
			physicsGeom->m_pTrigger = insideTrigger;
		}
	}
	return physicsGeom != NULL;
	// #else
	//	return false;
	// #endif

#else
	return false;
#endif
}

Mutex CPhysics::m_mutex;
CPhysicsGeom *CPhysics::LoadPhysics(const char *filename) // main loading function
{
	CAutoLock alock(&m_mutex);
#ifdef _DEBUG
	// For debug error Physics
	if (std::strcmp(filename, "model/scene/build/human/building_watermill_01.phy") == 0)
	{
		int k = 0;
		k++;
	}
#endif
	if (s_physicsPool)
	{
		CPhysicsGeom *cloneGeom = s_physicsPool->ClonePhysicsGeom(filename);
		if (cloneGeom)
			return cloneGeom;
	}

	CResFileStream *in_file = NULL;
	IReadResFile *readFile = NULL;
	// #ifdef _MMO_SERVER_
	if (s_zipPhysics)
	{
		readFile = s_zipPhysics->openFile(filename);
	}
	else
		readFile = createReadFile(filename);
	if (readFile)
	{
		in_file = gll_new CResFileStream();
		in_file->setInputFileObject(readFile);
		readFile->drop();
	}
	// #else

	// #ifdef _MMO_SERVER_
	//		CFileSystemWrapper *pFileSystemWrapper = CFileSystemWrapper::instance();
	// #else
	//	CResFileSystemWrapper *pFileSystemWrapper = GetFileSystemWrapper();
	// #endif

	//	in_file = pFileSystemWrapper->OpenFile(filename);

	// #endif

	if (in_file)
	{
		CPhysicsGeom *pGeom = LoadPhysics(in_file);
		if (pGeom == NULL)
		{
			_LOG("Physics File %s has error!\n", filename);
		}
		// #ifdef _MMO_SERVER_
		gll_delete in_file;
		// #else
		//		pFileSystemWrapper->CloseFile(in_file);
		// #endif
		if (pGeom && s_physicsPool)
		{
			s_physicsPool->CachePhysicsGeom(pGeom, filename);
		}
		return pGeom;
	}
	return NULL;
}

CPhysicsGeom *CPhysics::LoadPhysics(CResFileStream *pFile) // main loading function
{
	int i;
	// Read Area and Portal information

	int nbMesh = pFile->ReadS32();

	//	for(i=0; i< nbMesh; i++)
	//	{
	//		m_isAreaEnable[i] = 1;
	//	}
	CPhysicsGeom *pFirstGeom = NULL;
	CPhysicsGeom *pTailGeom = NULL;
	CPhysicsGeom *pCurGeom = NULL;

	for (i = 0; i < nbMesh; i++)
	{
#ifdef _DEBUG
		// For debug error Physics
		int curPos = pFile->tell();
#endif
		int type = pFile->ReadS32();
		if (type == PHYSICS_GEOM_TYPE_MESH)
		{

			CPhysicsMesh *pMesh = gll_new CPhysicsMesh();
			pCurGeom = pMesh;

			if (!pMesh)
				return NULL;
			if (pMesh->loadFromFile(pFile) < 0)
			{
				return NULL;
			}

			// pMesh->SetInfoFlag(0/*PHYSICS_INFO_BLOCK_MOVING | PHYSICS_INFO_BLOCK_BULLET | PHYSICS_INFO_RECV_SHADOW*/);
			// pMesh->SetAreaId(-1);
			//	RegisterGeom(pMesh);
			//		m_groundMeshList.push_back(pMesh);
		}
		else // Other Physics Geom
		{

			_GEOM_PROPERTY geomProperty;

			geomProperty.loadFromFile(pFile);
			pCurGeom = NewOnePhysicsGeom(
				type, VEC3(geomProperty.transX, geomProperty.transY, geomProperty.transZ),
				geomProperty.rotY,
				geomProperty.halfSizeX, geomProperty.halfSizeY, geomProperty.halfSizeZ);
			if (!pCurGeom)
			{
				return pFirstGeom;
			}
			pCurGeom->SetInfoFlag(geomProperty.flags);
			// pCurGeom->SetObjId(1023);
			// pCurGeom->SetAreaId(-1);
		}
		if (pFirstGeom == NULL)
			pTailGeom = pFirstGeom = pCurGeom;
		else
		{
			pTailGeom->m_pNext = pCurGeom;
			pTailGeom = pCurGeom;
		}
	}
	//	RegisterGeom(pFirstGeom);

	return pFirstGeom;
}
/*
	Node* find(const KeyType& keyToFind) const
	{
		Node* pNode = Root;

		while(pNode!=0)
		{
			const KeyType& key = pNode->getKey();

			if (keyToFind == key)
				return pNode;
			else if (keyToFind < key)
				pNode = pNode->getLeftChild();
			else //keyToFind > key
				pNode = pNode->getRightChild();
		}

		return 0;
	}
*/
void CPhysics::RegisterGeom(CPhysicsGeom *pGeom, std::vector<CPhysicsGeom *> *geomList)
{
	if (pGeom == NULL)
		return;
	/*	while (pGeom)
		{
			std::vector<CPhysicsGeom *>::iterator it = find(m_pGeomList.begin(), m_pGeomList.end(), pGeom);
			if(it == m_pGeomList.end())
			{
				m_pGeomList.push_back(pGeom);
				m_nGeomNb ++;
			}
			pGeom = pGeom->m_pNext;
		}
	*/
	// while(pGeom)
	//{
	//	int i,count = m_pGeomList.size();
	//	for(i=0;i<count;i++)
	//	{
	//		if(m_pGeomList[i] == pGeom)
	//			break;
	//	}
	//	m_pGeomList.push_back(pGeom);
	//	m_nGeomNb ++;
	//	//
	//	pGeom = pGeom->m_pNext;
	// }
	if (geomList == NULL)
		geomList = &m_pGeomList;
	geomList->push_back(pGeom);
	//	m_nGeomNb++;
	//	GLL_ASSERT(m_nGeomNb == m_pGeomList.size());
}
void CPhysics::DeleteGeom(CPhysicsGeom *&pGeom)
{
	CPhysicsGeom *pCur = pGeom;
	if (pCur->m_pTrigger)
	{
		pCur = pCur->m_pTrigger;
		while (pCur)
		{
			CPhysicsGeom *pNext = pCur->m_pNext;
			SAFE_DEL(pCur);
			pCur = pNext;
		}
	}
	pCur = pGeom;
	while (pCur)
	{
		CPhysicsGeom *pNext = pCur->m_pNext;
		SAFE_DEL(pCur);
		pCur = pNext;
	}
	pGeom = NULL;
}

void CPhysics::UnRegisterGeom(CPhysicsGeom *pGeom, std::vector<CPhysicsGeom *> *geomList, bool needDel)
{
	if (pGeom == NULL)
		return;

	// GLL_ASSERT(m_nGeomNb == geomList.size());
	// CPhysicsGeom *pCur = pGeom;
	/*
	while (pCur)
	{
	CPhysicsGeom *pNext = pCur->m_pNext;
	std::vector<CPhysicsGeom *>::iterator it = find(m_pGeomList.begin(), m_pGeomList.end(), pCur);
	if(it != m_pGeomList.end())
	{
	m_pGeomList.erase(it);
	m_nGeomNb--;
	//		areaMgr.SetObjDie(pGeom->GetObjId());
	if(needDel)
	SAFE_DEL(pCur);
	}
	pCur = pNext;
	}
	*/
	if (geomList == NULL)
		geomList = &m_pGeomList;

	//	std::vector<CPhysicsGeom *>::iterator it;
	int count = geomList->size();
	bool found = false;
	for (int i = 0; i < count; ++i)
	{
		if ((*geomList)[i] == pGeom)
		{
			geomList->erase(geomList->begin() + i);
			//	m_nGeomNb --;
			found = true;
			break;
		}
	}

	if (needDel)
	{
		if (found) // not deleted yet
		{
			CPhysicsGeom *pCur = pGeom;
			while (pCur)
			{
				CPhysicsGeom *pNext = pCur->m_pNext;
				SAFE_DEL(pCur);
				pCur = pNext;
			}
		}
		else // already deleted
		{
			pGeom = NULL;
		}
	}
}
//	int i= 0;
//	while (pCur) //if pGoem in the list, then the brothers are in the list, and they should be located adjecently.
//	{
//		CPhysicsGeom *pNext = pCur->m_pNext;
//		std::vector<CPhysicsGeom *>::iterator it;
//		int count = m_pGeomList.size();
//		for(;i<count;++i)
//		{
//			if(m_pGeomList[i] == pCur)
//			{
//				m_pGeomList.erase(m_pGeomList.begin()+i);
//				m_nGeomNb --;
//				break;
//			}
//		}
//		//if(i == count) //if not found
//		//	i = 0;
//		if(needDel)
//			SAFE_DEL(pCur);
//		pCur = pNext;
//	}
//}

f32 CPhysics::GetHeightWithWater(f32 x, f32 z, f32 yMax, uint32 mapId, VEC3 *pGroundNormal, const Entity *pIgnoreActor, bool walkOnWater, bool doesSwim, WATER_INFO *waterInfo, _Intersect_Info *intersectInfo)
{
	WATER_INFO waterInfoTemp;
	if (waterInfo == NULL)
		waterInfo = &waterInfoTemp;

	f32 h = GetHeight(x, z, yMax, mapId, pGroundNormal, pIgnoreActor, waterInfo, intersectInfo);

	if (waterInfo->hasWater())
	{
		f32 waterH = waterInfo->getWaterSurface();
		if (!walkOnWater)
			waterH = waterInfo->getWaterVirtualLine();
		if (waterH > h)
		{
			if (walkOnWater || !doesSwim) // if walk on Water, then water is like ground
			{							  // if swimming, then return bottom Height, need post process by caller
				h = waterH;
				if (pGroundNormal)
					*pGroundNormal = VEC3(0.0f, 1.0f, 0.0f);
			}
		}
	}
	return h;
}

f32 CPhysics::GetHeight(f32 x, f32 z, f32 yMax, uint32 mapId, VEC3 *pGroundNormal, const Entity *pIgnoreActor, WATER_INFO *waterInfo, _Intersect_Info *intersectInfo)
{
	// Profile("Update", "phycics");
	f32 outY = INVALID_TERRAIN_HEIGHT;
#ifdef _MMO_GAME_
	TerrainTiled *terrain = GetGame()->GetSceneMgr()->GetTerrainTiled();
#else
	GameMap *terrain = InstanceMgr::instance()->GetMap(mapId);
#endif
	if (!terrain)
		return INVALID_TERRAIN_HEIGHT;
#ifdef _MMO_GAME_
	if (!terrain->IsTileLoaded(x, z))
	{
		return yMax - 0.5f;
	}
	f32 terrainH = terrain->GetHeight(x, z, yMax, pGroundNormal, pIgnoreActor, waterInfo, intersectInfo);
#else
	f32 terrainH = terrain->GetHeight(x, z, yMax, pGroundNormal, waterInfo);
#endif

	//	if(yMax <= terrainH)
	//	{
	//		yMax = terrainH + 0.5f;
	//	}
	//	VEC3 v0(x, yMax, z);
	//	VEC3 v1(x, terrainH, z);
	//	_Intersect_Info intersectInfo;
	//	memset(&intersectInfo, 0, sizeof(_Intersect_Info));
	//	if(_IntersectTestLine(v0, v1/*, flag, -1*//* PHYSICS_INFO_BLOCK_ONLY*/,  pIgnoreActor, &intersectInfo))
	//	{
	//	//	if(outY != INVALID_TERRAIN_HEIGHT)
	//		//if(!intersectInfo.pActor)//should not move when block by an actor than the floor!
	//		//{
	////			*pGroundMaterial = 0;//intersectInfo.nGroundMaterialType;
	//			if(pGroundNormal)
	//				*pGroundNormal = intersectInfo.normal;
	//			return intersectInfo.point.Y;
	//		//}
	//		//else if(intersectInfo.pActor)
	//		//{
	//		//}
	//	}
	return terrainH;
}

// bool CPhysics::GetHeightWithActor(f32 x, f32 z, f32 yMax, f32 * pRetHeight, Entity *pIgnoreActor, int flag,_Intersect_Info *intersectInfo)
//{
//	f32 outY = INVALID_TERRAIN_HEIGHT;
//	VEC3 v0(x, yMax, z);
//	VEC3 v1(x, INVALID_TERRAIN_HEIGHT, z);
//	memset(intersectInfo, 0, sizeof(_Intersect_Info));
//	if(_IntersectTestLine(false, v0, v1, flag, -1 /*PHYSICS_INFO_BLOCK_ONLY*/,  pIgnoreActor, intersectInfo))
//	{
//		//	if(outY != INVALID_TERRAIN_HEIGHT)
//		//if(!intersectInfo.pActor)//should not move when block by an actor than the floor!
//		//{
//		*pRetHeight = intersectInfo->point.Y;
//		return true;
//		//}
//		//else if(intersectInfo.pActor)
//		//{
//		//}
//	}
//	return false;
// }

_Intersect_Info s_intersect_info;
#define SAFE_DIST_THRESHOLD 0.05f

int CPhysics::Move(VEC3 &curPos, const VEC3 &speed, uint32 mapId, Entity *pIgnoreActor, WATER_INFO &waterInfo, bool checkWhenNoSpeed, _Intersect_Info *intersectInfo /*= NULL*/, bool fitOnGround, bool walkOnWater, f32 upSlope /*= 1.0f*/, f32 downSlope /*= 1.5f*/)
{
	VEC3 moveSpeed = speed;
	if (fitOnGround)
		moveSpeed.Y = 0;
	if (moveSpeed.X == 0 && moveSpeed.Z == 0 && moveSpeed.Y == 0 && !checkWhenNoSpeed) // no speed
		return HIT_TARGET_TYPE_AIR;
	if (!fitOnGround)
		return MoveInAir(curPos, speed, mapId, pIgnoreActor, waterInfo, intersectInfo);

// #define DISABLE_PHY 1
#if DISABLE_PHY
	curPos += speed;
	return HIT_TARGET_TYPE_GROUND;
#endif

	float TEST_LINE_UP_OFF = fitOnGround ? /*43*/ upSlope * moveSpeed.getLength() : 0.0f;
	if (TEST_LINE_UP_OFF > 1.5f)
		TEST_LINE_UP_OFF = 1.5f;
	VEC3 oldPos = curPos;
	static const float TEST_LINE_FLY_OFF = 0.5f;
	if (fitOnGround)
		oldPos.Y += TEST_LINE_UP_OFF;
	else
		oldPos.Y += TEST_LINE_FLY_OFF;

	// Test PhysicsGeom
	VEC3 endPos = oldPos;
	endPos += moveSpeed;
	VEC3 outPutPos = endPos;
	if (!fitOnGround)
	{
		outPutPos.Y -= TEST_LINE_FLY_OFF;
	}
#ifdef ENABLE_CHEAT
	VEC3 recordPutpos = outPutPos;
#endif
	_Intersect_Info intersect; // test physics of the geom
	// endPos += moveSpeed*100;
	// if(IntersectTestLine(oldPos, endPos, PHYSICS_INFO_BLOCK_BULLET|PHYSICS_INFO_BLOCK_MOVING|PHYSICS_INFO_CAN_HIT, 0, NULL, &intersect)
	//	&&
	//	intersect.pActor == pIgnoreActor)//get actor itself
	//{
	//	oldPos = intersect.point;
	//	endPos = oldPos;
	//	endPos += moveSpeed;
	// }
	// else
	//{
	//	endPos -= moveSpeed*100;
	// }

	// bool testGroundSlide = true;
	if (intersectInfo == NULL) //	[1/20/2008 xiaopeng.kang]
	{
		intersectInfo = &intersect;
	}
	int ret = HIT_TARGET_TYPE_AIR;
	bool needHandleSlide = false;
	bool hasIntersect = false;
	if (_IntersectTestLine(oldPos, endPos, mapId, pIgnoreActor, intersectInfo))
	{
		// handle Slide
		VEC3 normalXZ = intersectInfo->normal;
		outPutPos = intersectInfo->point;
		hasIntersect = true;
#ifndef MONKEY
		if (fabs(normalXZ.Y) < 0.5f) // correspond to 60 degree, slope
		/*intersectInfo->pGeom->GetGeomType() != PHYSICS_GEOM_TYPE_MESH)*/
		{
			needHandleSlide = true;
			ret = HIT_TARGET_TYPE_WALL;
#ifdef ENABLE_CHEAT
			if (Game::IsDisalbeObjPhysics() && intersectInfo->pGeom)
			{
				needHandleSlide = false;
				outPutPos = recordPutpos;
			}
#endif
		}
		else if (fitOnGround)
#endif
		{
			ret = HIT_TARGET_TYPE_GROUND;
		}
		if (needHandleSlide)
		{
			normalXZ.Y = 0;
			if (normalXZ.X != 0 || normalXZ.Z != 0)
				normalXZ.normalize();

			VEC3 dist = intersectInfo->point - endPos;
			//		f32 l = VEC3Dot(&dist, &normalXZ) + SAFE_DIST_THRESHOLD;
			f32 l = dist.dotProduct(normalXZ) + SAFE_DIST_THRESHOLD;
			endPos += normalXZ * l;
			oldPos = intersectInfo->point;
			if (!_IntersectTestLine(oldPos, endPos, mapId, pIgnoreActor, NULL))
				outPutPos = endPos;
			else
			{
				endPos = oldPos + normalXZ * SAFE_DIST_THRESHOLD;
				if (!_IntersectTestLine(oldPos, endPos, mapId, pIgnoreActor, NULL))
					outPutPos = endPos;
				else
				{
					if (!fitOnGround)
						ret = HIT_TARGET_TYPE_GROUND;
					else
						intersectInfo->normal.Y = 1.0f;

					return ret;
				}
			}
			if (!fitOnGround)
			{
				outPutPos.Y -= TEST_LINE_FLY_OFF;
			}
		}
	}
	// else //not hit anything include the physics!
	//{
	//	oldPos = curPos;
	//	if(fitOnGround)
	//		oldPos.Y += TEST_LINE_UP_OFF;
	//	endPos = oldPos;
	//	endPos += moveSpeed;
	// }
	if (fitOnGround)
	{
		VEC3 groundNomal;
		f32 h = GetHeightWithWater(outPutPos.X, outPutPos.Z, outPutPos.Y + TEST_LINE_UP_OFF, mapId, &groundNomal, pIgnoreActor, walkOnWater, false, &waterInfo, intersectInfo);
		outPutPos.Y = h;
		intersectInfo->normal = groundNomal;
		if (h > curPos.Y /*&& hasIntersect*/)
		{
			// #define SLOPE_NORMAL_Y_SLIDE 0.574f
			if (fabs(groundNomal.Y) < 0.574f)
			{
				intersectInfo->normal.Y = 1.0f;
				return ret;
			}

			// VEC3 dist = outPutPos - curPos;
			// dist.X = fabs(dist.X);
			// dist.Z = fabs(dist.Z);
			// if(dist.X * upSlope < dist.Y && dist.Z * upSlope < dist.Y &&
			//	(dist.X * dist.X + dist.Z * dist.Z)  * upSlope * upSlope < dist.Y * dist.Y)
			//{
			//	return ret;

			//}
			// else
			//{
			//}
		}

		curPos = outPutPos;
		curPos.Y = h;
	}
	else // fly or swimming
	{
		VEC3 groundNomal;
		f32 testH = speed.Y >= 0 ? outPutPos.Y : curPos.Y;
		f32 terrainH = GetHeightWithWater(outPutPos.X, outPutPos.Z, testH + TEST_LINE_FLY_OFF + 0.5f, mapId, &groundNomal, pIgnoreActor, walkOnWater, true, &waterInfo);
		// TerrainTiled* terrain = Game::instance()->GetSceneMgr()->GetTerrainTiled();
		// f32 terrainH = terrain->GetHeight(outPutPos.X, outPutPos.Z, NULL);
		if (outPutPos.Y + 1.10f < terrainH || (outPutPos.Y < terrainH && speed.Y <= 0.0f))
		// if ( outPutPos.Y < terrainH)
		{
			intersectInfo->normal = groundNomal;
			outPutPos.Y = terrainH;
			ret = HIT_TARGET_TYPE_GROUND;
		}
		curPos = outPutPos;
	}

	// hit ground
	//	if(fitOnGround && testGroundSlide)
	//	{
	//		//Check Get Height
	//		f32 pNewH;
	//		if(GetHeight(outPutPos.X, outPutPos.Z, outPutPos.Y, &pNewH, pIgnoreActor,pGroundMaterial,pGroundNormal, -1/*PHYSICS_INFO_BLOCK_MOVING*/))
	////		if(GetHeightWithActor(outPutPos.X, outPutPos.Z, outPutPos.Y, &pNewH, pIgnoreActor, PHYSICS_INFO_BLOCK_MOVING,intersectInfo ))
	//		{
	//			VEC3 moveDist = outPutPos - oldPos;
	//			f32 dist = sqrt(moveDist.getLengthSQ());
	//			f32 yCanUp = curPos.Y + dist * upSlope;
	//			f32 yCanDown = curPos.Y - dist * downSlope;
	//
	//			if(*pGroundMaterial == 1)
	//			{
	//				//���㲻����
	////				f32 yCanUp = curPos.Y + dist * 0.1f;
	//				if(pNewH < (curPos.Y+1))
	//				{
	//					curPos = outPutPos;
	//					curPos.Y = pNewH;
	//					return HIT_TARGET_TYPE_AIR;
	//				}
	//				else
	//				{
	//					*pGroundMaterial = 0;
	//				}
	//			}
	//			else
	//			{
	//				if(pNewH < yCanUp/* && pNewH > yCanDown*/)
	//				{
	//					curPos = outPutPos;
	//					curPos.Y = pNewH;
	//					return HIT_TARGET_TYPE_AIR;
	//				}
	//			}
	//		}
	//
	//		//if(testGroundSlide)	//Handle Slide in Ground
	//		//{
	//		//	//Recovered y offset
	//		//	oldPos.Y -= TEST_LINE_UP_OFF;
	//		//	outPutPos.Y = oldPos.Y;
	//		//	MoveSlideOnGround(oldPos, outPutPos, upSlope, downSlope);
	//		//	curPos = oldPos;
	//		//}
	//	}
	//	else
	//	{
	//		//Check Get Height
	//		f32 pNewH;
	//		if(GetHeight(outPutPos.X, outPutPos.Z, outPutPos.Y, &pNewH, pIgnoreActor,pGroundMaterial,pGroundNormal, -1 /*PHYSICS_INFO_BLOCK_MOVING*/))
	//		{
	//			curPos = outPutPos;
	//			return HIT_TARGET_TYPE_AIR;
	//		}
	////		if(testGroundSlide)	//Handle Slide in Ground
	////		{
	//////			curPos = oldPos;
	//////			curPos.Y = outPutPos.Y;
	////
	////			if(GetHeight(oldPos.X, oldPos.Z, oldPos.Y, &pNewH, pIgnoreActor,pGroundMaterial,pGroundNormal, -1 /*PHYSICS_INFO_BLOCK_MOVING*/ ))
	////			{
	////				float fSavedY = outPutPos.Y;
	////				VEC3 vecGroundPos = oldPos;
	////				vecGroundPos.Y = pNewH;
	////				outPutPos.Y = pNewH;
	////				MoveSlideOnGround(vecGroundPos, outPutPos, upSlope, downSlope);
	////				curPos = vecGroundPos;
	////				curPos.Y = fSavedY;
	//////				curPos = oldPos;
	//////				curPos.Y = outPutPos.Y;
	////			}
	////		}
	////		else
	//			curPos = outPutPos;
	//	}
	return ret;
}

int CPhysics::MoveInAir(VEC3 &curPos, const VEC3 &speed, uint32 mapId, Entity *pIgnoreActor, WATER_INFO &waterInfo, _Intersect_Info *intersectInfo /*= NULL*/)
{
// #define DISABLE_PHY 1
#if DISABLE_PHY
	curPos += speed;
	return HIT_TARGET_TYPE_GROUND;
#endif
	VEC3 oldPos = curPos;
	static const float TEST_LINE_FLY_OFF = 0.5f;
	oldPos.Y += TEST_LINE_FLY_OFF;

	// Test PhysicsGeom
	VEC3 endPos = oldPos;
	endPos += speed;
	VEC3 outPutPos = endPos;

	outPutPos.Y -= TEST_LINE_FLY_OFF;

#ifdef ENABLE_CHEAT
	VEC3 recordPutpos = outPutPos;
#endif
	_Intersect_Info intersect; // test physics of the geom

	if (intersectInfo == NULL) //	[1/20/2008 xiaopeng.kang]
	{
		intersectInfo = &intersect;
	}
	int ret = HIT_TARGET_TYPE_AIR;
	bool needHandleSlide = false;
	bool hasIntersect = false;
	if (_IntersectTestLine(oldPos, endPos, mapId, pIgnoreActor, intersectInfo, true, false, false))
	{
		// handle Slide
		VEC3 normalXZ = intersectInfo->normal;
		outPutPos = intersectInfo->point;
		hasIntersect = true;
#ifndef MONKEY
		if (intersectInfo->pGeom == NULL && fabs(normalXZ.Y) < 0.5f) // correspond to 60 degree, slope
		/*intersectInfo->pGeom->GetGeomType() != PHYSICS_GEOM_TYPE_MESH)*/
		{
			needHandleSlide = true;
			ret = HIT_TARGET_TYPE_WALL;
#ifdef ENABLE_CHEAT
			if (Game::IsDisalbeObjPhysics() && intersectInfo->pGeom)
			{
				needHandleSlide = false;
				outPutPos = recordPutpos;
			}
#endif
		}
#endif

		if (needHandleSlide)
		{
			normalXZ.Y = 0;
			if (normalXZ.X != 0 || normalXZ.Z != 0)
				normalXZ.normalize();

			VEC3 dist = intersectInfo->point - endPos;
			//		f32 l = VEC3Dot(&dist, &normalXZ) + SAFE_DIST_THRESHOLD;
			f32 l = dist.dotProduct(normalXZ) + SAFE_DIST_THRESHOLD;
			endPos += normalXZ * l;
			oldPos = intersectInfo->point;
			if (!_IntersectTestLine(oldPos, endPos, mapId, pIgnoreActor, NULL))
				outPutPos = endPos;
			else
			{
				endPos = oldPos + normalXZ * SAFE_DIST_THRESHOLD;
				if (!_IntersectTestLine(oldPos, endPos, mapId, pIgnoreActor, NULL))
					outPutPos = endPos;
				else
				{

					ret = HIT_TARGET_TYPE_GROUND;

					return ret;
				}
			}
		}
		outPutPos.Y -= TEST_LINE_FLY_OFF;
	}

	// fly or swimming
	{
		// TerrainTiled* terrain = Game::instance()->GetSceneMgr()->GetTerrainTiled();
		// f32 terrainH = terrain->GetHeight(outPutPos.X, outPutPos.Z, NULL);
		//(!waterInfo.hasWater() || waterInfo.hasWater() && terrainH >= waterInfo.getWaterVirtualLine()) &&
		if (hasIntersect && intersectInfo->normal.Y < -0.5f) // when jump on hit some plane at first
			outPutPos.Y = curPos.Y - 0.5f;
		else
		{
			VEC3 groundNomal;
			f32 testH = (speed.Y >= 0 || hasIntersect) ? outPutPos.Y : curPos.Y;
			f32 terrainH = GetHeightWithWater(outPutPos.X, outPutPos.Z, testH + TEST_LINE_FLY_OFF + 0.5f, mapId, &groundNomal, pIgnoreActor, false, true, &waterInfo);
			if ((hasIntersect && (intersectInfo->pGeom == NULL || (intersectInfo->pGeom != NULL && speed.Y <= 0.0f && intersectInfo->normal.Y >= 0.5f))) || outPutPos.Y + 1.10f < terrainH || (outPutPos.Y < (terrainH + 0.5f) && speed.Y <= 0.0f))
			// if ( outPutPos.Y < terrainH)
			{
				intersectInfo->normal = groundNomal;
				outPutPos.Y = terrainH;
				ret = HIT_TARGET_TYPE_GROUND;
			}
			else
				outPutPos.Y = curPos.Y + speed.Y;
		}
		curPos = outPutPos;
	}

	return ret;
}

// void CPhysics::MoveSlideOnGround(VEC3 &curPos, VEC3 endPos, f32 upSlope /*= 1.0f*/, f32 downSlope /*= 1.5f*/)
//{
//	int nbMesh = m_groundMeshList.size();
//	for(int i=0; i< nbMesh; i++)
//	{
//		CPhysicsMesh * pMesh = (CPhysicsMesh *)m_groundMeshList[i];
//		int groundMoveRet = pMesh->MoveSlide(curPos.X, curPos.Y, curPos.Z, endPos.X, endPos.Z, 1.0f, 1.5f);
//	}
// }

#if RENDER_GEOM

void CPhysics::Render()
{
	video::IVideoDriver *driver = GetGame()->GetIrrDevice()->getVideoDriver();
	if (s_physicsMtrl == NULL)
	{
		glitch::video::CMaterialRendererManager &mgr = driver->getMaterialRendererManager();
		//	u16 id = mgr.createMaterialRenderer(video::EMT_UNLIT_NON_TEXTURED_SOLID);
		s_physicsMtrl = mgr.createMaterialInstance(glitch::video::EMT_UNLIT_NON_TEXTURED_BLEND);
		//! No lighting, no texturing, vertex coloring, default render states

		u16 id = s_physicsMtrl->getParameterID(glitch::video::ESPT_COLOR);
		if (id != u16(-1))
		{
			s_physicsMtrl->setParameter(id, video::SColor(0xff, 0xff, 0xff, 0));
		}
		// baibo_Glitch_TODO
	}

	// mtrl.setFlag(glitch::video::EMF_LIGHTING, false);
	// mtrl.setDiffuseColor(0xffffFF00);
	// mtrl.setFlag(glitch::video::EMF_WIREFRAME, true);

	driver->setMaterial(s_physicsMtrl);

	//	CSceneManager &areaMgr = m_pLevel->m_sceneManager;
	for (u32 i = 0; i < m_pGeomList.size(); i++)
	{
		const CPhysicsGeom *pGeom = m_pGeomList[i];

		for (; pGeom != NULL; pGeom = pGeom->m_pNext)
			//		if(m_pGeomList[i]->m_pActor /*&& m_pGeomList[i]->m_pActor->IsActivate() && m_pGeomList[i]->m_pActor->m_Status.bActiveByCameraWP*/)
			pGeom->Render();
	}
	GetGame()->GetSceneMgr()->GetTerrainTiled()->RenderPhysicsGeom();

	/*
		for(i = 0; i< 3; i++)
		{
			CActor *pCurActor = m_pActorPool[i];
			while(pCurActor )	//the head actor is equal to pActor
			{
				if(pCurActor->getActorFlag(ACTOR_FLAG_ACTIVE))
				{
					for (j = 0; j < pCurActor->GetPhysicsGeomNum(); j++)
					{
						pGeom = pCurActor->GetPhysicsGeom(j);
						if(pGeom)
							pGeom->Render(m_pM3DDevice);
					}
				}
				pCurActor= pCurActor->m_pNext;
			}
		}
	*/
}

void CPhysics::DrawTriangle(VEC3 p1, VEC3 p2, VEC3 p3, u32 color)
{
	/*
		//GXColor		red_clr1      = { 237, 28, 36, 255 } ;
		//u8 Colors_rgba8[] ATTRIBUTE_ALIGN(32) =
		//{
		//	255,  0,  50, 255,   // 0
		//	255,  0,  80, 255,   // 1
		//	255, 0, 110, 255    // 2
		//};

		GXSetVtxDesc( GX_VA_POS, GX_DIRECT ) ;
		//GXSetVtxDesc(GX_VA_CLR0, GX_INDEX8);
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GXSetVtxAttrFmt( GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0 ) ;
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	//	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);

		//GXSetArray(GX_VA_CLR0, Colors_rgba8, 4*sizeof(u8));

		//GXSetChanCtrl(
		//	GX_COLOR0A0,
	 //       GX_DISABLE,    // disable channel
	 //       GX_SRC_VTX,    // amb source
	 //       GX_SRC_VTX,    // mat source
	 //       GX_LIGHT_NULL, // light mask
	 //       GX_DF_NONE,    // diffuse function
	 //       GX_AF_NONE);
		GXSetNumChans(1);
		GXSetChanAmbColor(GX_COLOR0A0, white_clr);
		GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
	  //  GXSetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);


		//GXSetTevColor( GX_TEVREG0, red_clr1 ) ;
		//GXSetCullMode(GX_CULL_NONE);

		GXSetBlendMode( GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP ) ;

		GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);
		//----- TEXTURE
		GXSetNumTexGens( 0 ) ;

		//----- TEV
		GXSetNumTevStages(1);
		//GXSetTevOp(GX_TEVSTAGE0, GX_DECAL);
		GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
		GXSetTevOrder( GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
		//GXSetTevOrder( GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,  GX_COLOR_NULL);

		//GXColor	gx_color = {color[0] >> 24, (color[0] & 0x00FF0000) >> 16, (color[0] & 0x0000FF00) >> 8, (color[0] & 0x000000FF)};
		//GXColor	gx_color = white_clr;
		//GXSetTevColor(GX_TEVREG0, gx_color);

		GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_RASA, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

		GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_RASC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
		GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);


		GXBegin( GX_TRIANGLES, GX_VTXFMT0, 3 ) ;
			GXPosition3f32( p1.x, p1.y, p1.z  ) ;
		//	GXNormal3f32(1, 0, 0);
			GXColor1u32(color);
			GXPosition3f32( p2.x, p2.y, p2.z  ) ;
		//	GXNormal3f32(1, 0, 0);
			GXColor1u32(color);
			GXPosition3f32( p3.x, p3.y, p3.z  ) ;
		//	GXNormal3f32(1, 0, 0);
			GXColor1u32(color);
		GXEnd() ;
	*/
}

void CPhysics::DrawLine(VEC3 p1, VEC3 p2, u32 color)
{
	/*	GXSetVtxDesc( GX_VA_POS, GX_DIRECT ) ;
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GXSetVtxAttrFmt( GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0 ) ;
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

		GXSetChanCtrl(
			GX_COLOR0A0,
			GX_DISABLE,    // disable channel
			GX_SRC_REG,    // amb source
			GX_SRC_VTX,    // mat source
			GX_LIGHT_NULL, // light mask
			GX_DF_NONE,    // diffuse function
			GX_AF_NONE);

		GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);
		GXBegin( GX_LINES, GX_VTXFMT0, 2 ) ;
			GXPosition3f32( p1.x, p1.y, p1.z  ) ;
			GXColor1u32(color);
			GXPosition3f32( p2.x, p2.y, p2.z  ) ;
			GXColor1u32(color);
		GXEnd() ;
	*/
}

void CPhysics::DrawLine(VEC3 *vertex, int vNum, u32 color)
{
	/*
		//GXColor		red_clr1      = { 37, 228, 136, 255 } ;
		//u8 Colors_rgba8[] ATTRIBUTE_ALIGN(32) =
		//{
		//	255, 0, 0, 255,   // 0
		//	255, 0, 0, 255,   // 1
		////	114, 0, 110, 255    // 2

		//};

		GXSetVtxDesc( GX_VA_POS, GX_DIRECT ) ;
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GXSetVtxAttrFmt( GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0 ) ;
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
		//GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);

		//GXSetArray(GX_VA_CLR0, Colors_rgba8, 4*sizeof(u8));

		GXSetChanCtrl(
			GX_COLOR0A0,
			GX_DISABLE,    // disable channel
			GX_SRC_REG,    // amb source
			GX_SRC_VTX,    // mat source
			GX_LIGHT_NULL, // light mask
			GX_DF_NONE,    // diffuse function
			GX_AF_NONE);

		//GXSetCullMode(GX_CULL_NONE);
		GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);
		GXBegin( GX_LINESTRIP, GX_VTXFMT0, vNum ) ;
			for(int i = 0; i < vNum; i++)
			{
				GXPosition3f32( vertex[i].x, vertex[i].y, vertex[i].z );
				GXColor1u32(color);
			}
		GXEnd() ;
	*/
}
#endif

bool CPhysics::_IntersectTestLine(const VEC3 &v0, const VEC3 &v1, uint32 mapId, const Entity *pIgnoreActor, _Intersect_Info *pIntersectInfo /*= NULL*/, bool checkTerrain /* = false*/, bool checkWater, bool offsetGround /*= false*/) const
{
	//	Profile("Update", "phycics");
	if (v0 == v1)
		return false;
	_Intersect_Segment theSegment;
	theSegment.seg.P0 = v0;
	theSegment.seg.P1 = v1;
	CPhysicsGeom::ComputeIntersectLine(theSegment);
	{
		// if (theSegment.len < 0.20f)
		//{
		//	theSegment.len = 0.25f;
		// }
		// else
		theSegment.len += SAFE_DIST_THRESHOLD;
		theSegment.seg.P1 = theSegment.seg.P0 + ((theSegment.dir * theSegment.len));
		CPhysicsGeom::ComputeIntersectLine(theSegment);
	}
#ifdef _MMO_GAME_
	TerrainTiled *terrain = GetGame()->GetSceneMgr()->GetTerrainTiled();
#else
	GameMap *terrain = InstanceMgr::instance()->GetMap(mapId);
#endif
	f32 outA = -1;
	bool ret = false;
#ifdef _MMO_GAME_
	ret = terrain->IsIntersectSegment(theSegment, outA, pIgnoreActor, pIntersectInfo, checkTerrain, checkWater, offsetGround);
#else
	ret = terrain->IsIntersectSegment(theSegment, outA, pIntersectInfo, checkTerrain, checkWater, offsetGround);
#endif

	if (ret)
	{
		if (outA <= SAFE_DIST_THRESHOLD)
			outA = 0;
		else
			outA -= SAFE_DIST_THRESHOLD;
		if (pIntersectInfo)
			pIntersectInfo->point = theSegment.seg.P0 + theSegment.dir * outA;
		return true;
	}
	return false;
}

bool CPhysics::IntersectTestPoint(const VEC3 &pt, int geomInfoFlag, int geomInfoIgnoreFlag, Entity *pIgnoreActor) const
{
	/*
		std::vector<CPhysicsGeom *>::iterator it = m_pGeomList.begin();

		const CSceneManager &areaMgr = m_pLevel->m_sceneManager;
		for( ;it != m_pGeomList.end(); it++)
		{
			const CPhysicsGeom *pGeom = *it;

			//if(!areaMgr.IsObjPhysicsGeomEnabled( pGeom->GetObjId() ))
			//	continue;

			if((pGeom->GetInfoFlag() & geomInfoFlag) == 0)
				continue;
			if((pGeom->GetInfoFlag() & geomInfoIgnoreFlag) != 0) //added Ignore Flag
				continue;
			//check pGeom is in the Enabled area
			if(!areaMgr.IsAreaEnabled( pGeom->GetAreaId()))
			{
				continue;
			}

			if(pIgnoreActor)	//if need ignore an actor
			{
				if(pGeom->m_pActor == pIgnoreActor)
					continue;
			}
			if(pGeom->m_pActor )	//added die checking
			{
				if(!pGeom->m_pActor->IsActive())
					continue;
			}

			if(pGeom->IsPointInThis(pt))
			{
				return true;
			}

		}
	*/
	return false;
}

bool CPhysics::IntersectTestSphere(const VEC3 &v0, f32 &radius, const VEC3 &speed, int geomInfoFlag, int geomInfoIgnoreFlag, Entity *pIgnoreActor, _Intersect_Info *pIntersectInfo) const
{
	/*	std::vector<CPhysicsGeom *>::iterator it = m_pGeomList.begin();

		const CSceneManager &areaMgr = m_pLevel->m_sceneManager;
		for( ;it != m_pGeomList.end(); it++)
		{
			const CPhysicsGeom *pGeom = *it;


			if((pGeom->GetInfoFlag() & geomInfoFlag) == 0)
				continue;
			if((pGeom->GetInfoFlag() & geomInfoIgnoreFlag) != 0) //added Ignore Flag
				continue;
			//check pGeom is in the Enabled area
			if(!areaMgr.IsAreaEnabled( pGeom->GetAreaId()))
			{
				continue;
			}

			if(pIgnoreActor)	//if need ignore an actor
			{
				if(pGeom->m_pActor == pIgnoreActor)
					continue;
			}
			if(pGeom->m_pActor )	//added die checking
			{
				if(!pGeom->m_pActor->IsActive())
					continue;
			}

			if(pGeom->IsIntersectSphere(v0, radius, speed, pIntersectInfo))
			{
				return true;
			}

		}
	*/
	return false;
}

CPhysicsGeom *CPhysics::NewOnePhysicsGeom(int geomType, const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL)
{
	CPhysicsGeom *pGeom = NULL;
	switch (geomType)
	{
	/*	case PHYSICS_GEOM_TYPE_SPHERE:
			pGeom = gll_new CPhysicsSphere();
			break;
	*/
	case PHYSICS_GEOM_TYPE_PLANE:
		pGeom = gll_new CPhysicsPlane(pos, rotY, halfW, halfH, halfL);
		break;
	case PHYSICS_GEOM_TYPE_BOX:
		pGeom = gll_new CPhysicsBox(pos, rotY, halfW, halfH, halfL);
		break;
	case PHYSICS_GEOM_TYPE_SPHERE:
	case PHYSICS_GEOM_TYPE_CYLINDER:
		pGeom = gll_new CPhysicsCylinder(pos, rotY, halfW, halfH, halfL);
		break;
		// case PHYSICS_GEOM_TYPE_HORIZONTAL_CYLINDER:
		//	pGeom = gll_new1 CPhysicsHorizontalCylinder();
		//	break;
	/*	case PHYSICS_GEOM_TYPE_TRIANGLE:
			pGeom = gll_new CPhysicsTriangle();
			break;
	*/
	case PHYSICS_GEOM_TYPE_MESH:
		pGeom = gll_new CPhysicsMesh(pos, rotY, halfW, halfH, halfL);
		break;
		/*	case PHYSICS_GEOM_TYPE_INFINITE_PLANE:
				pGeom = gll_new CPhysicsInfinitePlane();
				break;
		*/

	default: // Should not go here
	{
#ifndef MONKEY
		_LOG("Physics Geom type error %d\n", geomType);
#endif
	}
		//	GLL_ASSERT(0);
	}
	return pGeom;
}

// CPhysicsGeom * CPhysics::AddOnePhysicsGeom(int geomType, const VEC3 &pos, f32 rotZ,  f32 sizeX, f32 sizeY, f32 sizeZ)
//{
//	CPhysicsGeom *pGeom = NewOnePhysicsGeom(geomType, pos, rotY, halfW,halfH, halfL);
//	//pGeom->SetPosition(pos);
//	//pGeom->SetRotationY(rotZ);
//	//pGeom->SetSize(sizeX, sizeY, sizeZ);
////	int areaId = m_pLevel->m_pAreaManager->GetAreaIndex(pos.x, pos.y);
////	pGeom->SetAreaId( areaId);
////	if(m_pGeomList == NULL)
////		m_pGeomList = pGeom;
////	else
//
////	RegisterGeom(pGeom);
//	return pGeom;
//
//}

// void CPhysics::GetAimTargetPoint(int x, int y, int nMaxDist, _Hit_Target_Info &hitTargetInfo, int teamFilter) const
//{
//	_Intersect_Segment theSegment;
//	const CCamera *pCamera = m_pAIManager->GetCamera();
//
//	theSegment.seg.P0 = pCamera->m_vecEyePos;
//	theSegment.dir = pCamera->m_vecLookAtPos - pCamera->m_vecEyePos;
//	theSegment.dir.Normalize();
//	theSegment.len = nMaxDist;
//	theSegment.seg.P1.x =(theSegment.dir.x * nMaxDist) ;
//	theSegment.seg.P1.y =(theSegment.dir.y * nMaxDist) ;
//	theSegment.seg.P1.z =(theSegment.dir.z * nMaxDist) ;
//	theSegment.seg.P1 += theSegment.seg.P0;
//	VEC3::MinMax(theSegment.seg.P0, theSegment.seg.P1, theSegment.aabb.MinEdge, theSegment.aabb.MaxEdge);
//
//	HitTargetTest(theSegment, hitTargetInfo, teamFilter);
//
//
//
// }
//
//
//
//
//
//
//
// void CPhysics::HitTargetTest(_Intersect_Segment &theSegment, _Hit_Target_Info &hitTargetInfo, int teamFilter) const
//{
//	//Tested Scene Actors
//	_Intersect_Info intersectInfo;
//	_Intersect_Info *pIntersectInfo = &intersectInfo;
//	int outA = -1;
//	hitTargetInfo.dist = theSegment.len;
//	hitTargetInfo.pActor = NULL;
//	hitTargetInfo.type = HIT_TARGET_TYPE_AIR;
//	hitTargetInfo.position = theSegment.seg.P1;
//	hitTargetInfo.pGeom = NULL;
//
//	int geomInfoFlag = PHYSICS_INFO_BLOCK_BULLET;
//
//	CPhysicsGeom *pGeom = m_pGeomList;
//	const CSceneManager *pAreaMgr = m_pLevel->GetSceneManager();
//	for( ;pGeom; pGeom = pGeom->m_pNext)
//	{
//		if(!areaMgr.IsObjPhysicsGeomEnabled( pGeom->GetObjId() ))
//			continue;
//
//		if((pGeom->GetInfoFlag() & geomInfoFlag) == 0)
//			continue;
//		//check pGeom is in the Enabled area
//		if(!areaMgr.IsAreaEnabled( pGeom->GetAreaId()))
//		{
//			continue;
//		}
//
//		if(pGeom->IsIntersectLine(theSegment, outA, pIntersectInfo))
//		{
//			theSegment.seg.P1.x = theSegment.seg.P0.x + ((theSegment.dir.x * outA) );
//			theSegment.seg.P1.y = theSegment.seg.P0.y + ((theSegment.dir.y * outA) );
//			theSegment.seg.P1.z = theSegment.seg.P0.z + ((theSegment.dir.z * outA) );
//			hitTargetInfo.type = HIT_TARGET_TYPE_SCENE_ACTOR;
//			hitTargetInfo.position =  theSegment.seg.P1;
//			hitTargetInfo.dist = outA;
//			hitTargetInfo.pGeom = pGeom;
//			if(theSegment.seg.P1 == theSegment.seg.P0 )
//			{
//				hitTargetInfo.dist = outA + 10;
//				return;
////				break;
//			}
//			ComputeIntersectLine(theSegment);
//		}
//
//	}
//
//	outA = theSegment.len;
//
//
//	//Tested Ground Mesh
//	int i, j;
//	for(i =0; i< nbMesh; i++)
//	{
//		if(theSegment.aabb.MaxEdge.x < m_pMesh[i]->m_minX || theSegment.aabb.MinEdge.x > m_pMesh[i]->m_maxX ||
//			theSegment.aabb.MaxEdge.y < m_pMesh[i]->m_minY || theSegment.aabb.MinEdge.y > m_pMesh[i]->m_maxY)
//			continue;
//		//check if curPos and target Position is in a same area
//		int ret = m_pMesh[i]->IsIntersectLine(theSegment, outA);
//
//		if(ret != HIT_TARGET_TYPE_AIR)
//		{
//			theSegment.seg.P1.x = theSegment.seg.P0.x + ((theSegment.dir.x * outA) );
//			theSegment.seg.P1.y = theSegment.seg.P0.y + ((theSegment.dir.y * outA) );
//			theSegment.seg.P1.z = theSegment.seg.P0.z + ((theSegment.dir.z * outA) );
//			hitTargetInfo.type = ret;
//			hitTargetInfo.position = theSegment.seg.P1;
//			hitTargetInfo.dist = outA;
//			if(theSegment.seg.P1 == theSegment.seg.P0 )
//			{
//				hitTargetInfo.dist = outA + 10;
//				return;
////				break;
//			}
//			ComputeIntersectLine(theSegment);
//
//		}
//	}
//
//
//
//	outA = theSegment.len;
//	for(i=0; i< 3; i++ )
//	{
//		if(i < 2 && (teamFilter & (1 << i)) == 0)
//			continue;
//
//		//Tested register actors
//
//		CActor *pActorPool = m_pActorPool[i];
//		for( ;pActorPool; pActorPool = pActorPool->m_pNext)
//		{
//			if(pActorPool->getActorFlag(ACTOR_FLAG_ACTIVE) == 0)
//				continue;
//
//			for (j = 0; j < pActorPool->GetPhysicsGeomNum(); j++) {
//				pGeom = pActorPool->GetPhysicsGeom(j);
//				if((pGeom->GetInfoFlag() & geomInfoFlag) == 0)
//					continue;
//				//check pGeom is in the Enabled area
//				//TODO
//				//		if(!areaMgr.IsAreaEnabled( pGeom->GetAreaId()))
//				//		{
//				//			continue;
//				//		}
//
//				if(pGeom->IsIntersectLine(theSegment, outA, NULL))
//				{
//					theSegment.seg.P1.x = theSegment.seg.P0.x + ((theSegment.dir.x * outA) );
//					theSegment.seg.P1.y = theSegment.seg.P0.y + ((theSegment.dir.y * outA) );
//					theSegment.seg.P1.z = theSegment.seg.P0.z + ((theSegment.dir.z * outA) );
//					hitTargetInfo.pGeom = pGeom;
//					hitTargetInfo.type = (i== 0) ? HIT_TARGET_TYPE_ACTOR_TEAM_0 : HIT_TARGET_TYPE_ACTOR_TEAM_1;
//					hitTargetInfo.position =  theSegment.seg.P1;
//					hitTargetInfo.dist = outA;
//					hitTargetInfo.pActor = pActorPool;
//					if(theSegment.seg.P1 == theSegment.seg.P0 )
//					{
//						hitTargetInfo.dist = outA + 10;
//						return;
//						//				break;
//					}
//					ComputeIntersectLine(theSegment);
//				}
//			}
//		}
//	}
//
//}

#undef USE_NW4R_COORDINATE
