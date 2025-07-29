//#include "preHeader.h"
#include "Physics.h"
#include "PhysicsMesh.h"
#include "io/ResFileStream.h"
#ifdef _MMO_GAME_
#include "game/game.h"
#else
// #include "ResFileStream.h"
#endif
#ifdef _AFANTY_
#include "Common/NavMesh/include/MeshLoaderObj.h"
#endif

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

// MeshBlock::MeshBlock()
//{
//	m_blockFaces = NULL;
//	m_blockFacesNumber = 0;
//	m_blockCurrentIndex = 0;
// }

// #define USE_BUFFER_ALLOCATE_BLOCKFACE 1
// MeshBlock::~MeshBlock()
//{
// #if USE_BUFFER_ALLOCATE_BLOCKFACE
// #else
//	SAFE_DEL_ARRAY(m_blockFaces);
//	m_blockFaces = NULL;
// #endif
//	m_blockFacesNumber = 0;
//	m_blockCurrentIndex = 0;
//
// }
CPhysicsMesh::CPhysicsMesh()
	: CPhysicsGeom(VEC3(0, 0, 0), 0, 1, 1, 1)
	  //, m_vertex(NULL)
	  //, m_nbVertex(0)
	  //, 	m_nbFaces(0)
	  //, m_faces( NULL)
	  //, m_faceFlags( NULL)
	  ,
	  m_refMesh(NULL)
{
	m_geomType = PHYSICS_GEOM_TYPE_MESH;
}

CPhysicsMesh::CPhysicsMesh(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL)
	: CPhysicsGeom(pos, 0, halfW, halfH, halfL)
	  //, m_vertex(NULL)
	  //, m_nbVertex(0)
	  ,
	  m_refMesh(NULL)
{
	m_geomType = PHYSICS_GEOM_TYPE_MESH;
	// #if defined(OS_NITRO)
	//	MTX_Identity44(&m_currentClipMatrix);
	// #else
	//	m_currentClipMatrix.LoadIdentity();
	// #endif

	//	m_bFaceRendered = NULL;
	// int i;
	// for(i =0; i< PHYSICS_TYPE_NUM; i++)
	//{
	//	m_pBlockArray/*[i]*/ = NULL;
	// m_nbFaces/*[i]*/ = 0;
	// m_faces/*[i]*/ = NULL;
	// m_faceFlags/*[i]*/ = NULL;
	//}

	// m_pM3DDevice = pM3DDevice;
	// m_pAIManager = pAIManager;
	// m_pLevel = pLevel;
	//	pLevel = CMyGame::GetGame()->GetLevel();

	// m_blocksNumber = 0;
	// m_blocksLine = 0;
	// m_blocksSX = 0;
	// m_blocksSZ = 0;
}

CPhysicsMesh::~CPhysicsMesh(void)
{
	if (m_refMesh)
		m_refMesh->drop();
}

CPhysicsGeom *CPhysicsMesh::Clone() const
{
	CPhysicsMesh *cloneGeom = gll_new CPhysicsMesh(m_pos, m_rotY, m_halfSize.X, m_halfSize.Y, m_halfSize.Z);
	cloneGeom->SetInfoFlag(m_info.s32value);
	cloneGeom->SetRefMesh(GetRefMesh());
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
void CPhysicsMesh::Render() const
{
	if (!m_info.parentTransformSet)
		return;

	glitch::video::IVideoDriver *irrDriver = GetGame()->GetIrrDevice()->getVideoDriver();

	MTX4 matOldWorld = irrDriver->getTransform(video::ETS_WORLD);
	irrDriver->setTransform(video::ETS_WORLD, m_absTransform);

	VEC3 vertex[3];
	glitch::video::SColor groundColor(0xA0, 0x88, 0, 0xFF);
#define RENDER_H_OFF 0.10f
	int nbFaces = m_refMesh->GetNbFace();
	const u16 *faces = m_refMesh->GetFacePointer();
	const f32 *v = m_refMesh->GetVertexPointer();
	for (int i = 0; i < nbFaces; i++)
	{
		int a = faces[PHYSICS_FACE_SIZE * i];
		int b = faces[PHYSICS_FACE_SIZE * i + 1];
		int c = faces[PHYSICS_FACE_SIZE * i + 2];
		int index = a * 3;
		vertex[0].X = v[index++];
		vertex[0].Y = v[index++] + RENDER_H_OFF;
		vertex[0].Z = v[index++];
		index = b * 3;
		vertex[1].X = v[index++];
		vertex[1].Y = v[index++] + RENDER_H_OFF;
		vertex[1].Z = v[index++];
		index = c * 3;
		vertex[2].X = v[index++];
		vertex[2].Y = v[index++] + RENDER_H_OFF;
		vertex[2].Z = v[index++];
		irrDriver->draw3DLine(vertex[0], vertex[1], groundColor);
		irrDriver->draw3DLine(vertex[1], vertex[2], groundColor);
		irrDriver->draw3DLine(vertex[2], vertex[0], groundColor);
	}

	irrDriver->setTransform(video::ETS_WORLD, matOldWorld);
}
#endif

#ifdef _AFANTY_
int CPhysicsMesh::GenerateMeshObj(rcMeshLoaderObj &meshObj, int vOff) const
{
	if (!m_info.parentTransformSet)
		return 0;

	VEC3 vertex;
	int nbFaces = m_refMesh->GetNbFace();
	const u16 *faces = m_refMesh->GetFacePointer();
	const f32 *v = m_refMesh->GetVertexPointer();
	int nbVertex = m_refMesh->GetNbVertex();
	int index = 0;
	for (int i = 0; i < nbVertex; i++)
	{
		vertex.X = v[index++];
		vertex.Y = v[index++];
		vertex.Z = v[index++];
		m_absTransform.transformVect(vertex);
		meshObj.addVertex(vertex.X, vertex.Y, vertex.Z);
	}

	for (int i = 0; i < nbFaces; i++)
	{
		int a = faces[PHYSICS_FACE_SIZE * i];
		int b = faces[PHYSICS_FACE_SIZE * i + 1];
		int c = faces[PHYSICS_FACE_SIZE * i + 2];
		meshObj.addTriangle(vOff + a, vOff + b, vOff + c);
	}

	return nbVertex;
}
#endif

// int CPhysicsMesh::GetBlockIndex(int x, int z) const
//{
//	int lx = x - m_blocksSX;
//	int lz = z - m_blocksSZ;
//	if(lx < 0)
//		lx = 0;
//	lx >>= PHISICS_CELL_SHIFT;
//	if(lx >= m_blocksLine)
//		lx = m_blocksLine - 1;
//	if(lz < 0)
//		lz = 0;
//	lz >>= PHISICS_CELL_SHIFT;
//	if(lz >= m_blocksLineH)
//		lz = m_blocksLineH - 1;
//
//	int _list = lz * m_blocksLine + lx;
//	return _list;
// }

#define MIN_IN_THREE(x, y, z) ((x < y) ? (x < z ? x : z) : (y < z ? y : z))
#define MAX_IN_THREE(x, y, z) ((x > y) ? (x > z ? x : z) : (y > z ? y : z))
int CPhysicsMesh::loadFromFile(CResFileStream *pFile)
{
	int i;
	int size;
	// int			meshType = pFile->ReadS32();
	m_info.s32value = pFile->ReadS32();
	VEC3 pos;

#if USE_NW4R_COORDINATE
	m_pos.X = pFile->Readfloat();
	m_pos.Z = -pFile->Readfloat();
	m_pos.Y = pFile->Readfloat();
	m_halfSize.X = pFile->Readfloat();
	m_halfSize.Z = pFile->Readfloat();
	m_halfSize.Y = pFile->Readfloat();

#else
	m_pos.X = pFile->Readfloat();
	m_pos.Y = pFile->Readfloat();
	m_pos.Z = pFile->Readfloat();
	m_halfSize.X = pFile->Readfloat();
	m_halfSize.Y = pFile->Readfloat();
	m_halfSize.Z = pFile->Readfloat();
#endif

	// this->SetInfoFlag(meshType);
	// #if USE_NW4R_COORDINATE
	//	m_minX = pFile->Readfloat();
	//	m_maxZ = -pFile->Readfloat();
	//	m_maxX = pFile->Readfloat();
	//	m_minZ = -pFile->Readfloat();
	// #else
	//	m_minX = pFile->Readfloat();
	//	m_minZ = pFile->Readfloat();
	//	m_maxX = pFile->Readfloat();
	//	m_maxZ = pFile->Readfloat();
	// #endif

	// Read mesh header
	int nv = pFile->ReadS16();
	// int nPhysicsType = pFile->ReadS16();
	// if(nPhysicsType != PHYSICS_TYPE_NUM)
	//{
	//	return -100;
	// }
	//	for (i = 0; i< PHYSICS_TYPE_NUM; i++)

	int nbFaces /*[i]*/ = pFile->ReadS16();

	//	globalSprites += (nf * sizeof(TFace)) + (nv * sizeof(M3DXVector3));

	// Read mesh vertices

	size = 3 * nv;
	f32 *vertex = gll_new f32[size];
	if (!vertex)
		return -1;
#if USE_NW4R_COORDINATE
	for (i = 0; i < nv; i++)
	{
		vertex[i * 3] = pFile->Readfloat();
		vertex[i * 3 + 2] = -pFile->Readfloat();
		vertex[i * 3 + 1] = pFile->Readfloat();
	}
#else
	for (i = 0; i < size; i++)
		vertex[i] = pFile->Readfloat();
#endif

	u16 *faces = NULL;
	// for (i = 0; i< PHYSICS_TYPE_NUM; i++)
	{
		// Read mesh faces
		size = nbFaces /*[i]*/ * PHYSICS_FACE_SIZE;
		if (size > 0)
		{

			faces /*[i]*/ = gll_new u16[size];
			// m_faceFlags/*[i]*/ = gll_new u8[m_nbFaces/*[i]*/];
			if (!faces /*[i]*/) // || !m_faceFlags/*[i]*/ )
			{
				SAFE_DEL_ARRAY(vertex);
				return -2;
			}

			for (int j = 0; j < size; j++)
			{
				faces /*[i]*/[j] = pFile->ReadS16();
			}

			// stream.Read(m_faces/*[i]*/, sizeof(u16) *size);
		}
	}
	m_refMesh = gll_new CPhysicsRefMesh(nv, vertex, nbFaces, faces);
	// m_refMesh->Set(nv, vertex, nbFaces, faces);

	// s32 nX = m_maxX - m_minX;
	// s32 nZ = m_maxZ - m_minZ;
	// printf("Physics X(%d,%d), Z(%d,%d)\n", m_minX, m_maxX, m_minZ, m_maxZ);

	//// build the positioning grid
	// s32 gridW = ((nX + PHISICS_CELL_FULL_DIM - 1) >> PHISICS_CELL_SHIFT);
	// s32 gridH = ((nZ + PHISICS_CELL_FULL_DIM - 1) >> PHISICS_CELL_SHIFT);

	// m_blocksLine = gridW;
	// m_blocksLineH = gridH;
	// m_blocksNumber = (gridW * gridH);

	// m_blocksSX = m_minX;
	// m_blocksSZ = m_minZ;
	// m_blocksEX = nX;
	// m_blocksEZ = nZ;

	// printf("Physics Block GridW(%d), GridH(%d), BLock Num(%d)\n", gridW, gridH, m_blocksNumber);
	// OUTPUT_MEMORY_INFO;

	// alloc the blocks array
	//	int k;
	//	for (k=0; k< PHYSICS_TYPE_NUM; k++)
	//	{
	// #ifdef IPHONEOS
	//		if(m_blocksNumber <= 0)
	//		{
	//			ReleaseMemory();
	//			return -6;
	//		}
	// #endif
	//		m_pBlockArray[k] = gll_new MeshBlock[m_blocksNumber];
	//		if (m_pBlockArray[k] == NULL)
	//		{
	//			ReleaseMemory();
	//			return -6;
	//		}
	//		memset(m_pBlockArray[k], 0, m_blocksNumber * sizeof(MeshBlock));
	//		int faceIndex = 0;
	//		for (i = 0; i < m_nbFaces[k]; i++)
	//		{
	//
	//			int a = m_faces[k][faceIndex++];
	//			int b = m_faces[k][faceIndex++];
	//			int c = m_faces[k][faceIndex++];
	//			faceIndex++;
	//			//Register the face to block
	//			int minX = MIN_IN_THREE(m_vertex[3 * a], m_vertex[3 * b], m_vertex[3 * c]);
	//			int maxX = MAX_IN_THREE(m_vertex[3 * a], m_vertex[3 * b], m_vertex[3 * c]);
	//			int minZ = MIN_IN_THREE(m_vertex[3 * a + 2], m_vertex[3 * b + 2], m_vertex[3 * c + 2]);
	//			int maxZ = MAX_IN_THREE(m_vertex[3 * a + 2], m_vertex[3 * b + 2], m_vertex[3 * c + 2]);
	//			int block_leftdown = GetBlockIndex(minX, minZ);
	//			int block_rightup = GetBlockIndex(maxX, maxZ);
	//			int h = (block_rightup - block_leftdown) / m_blocksLine ;
	//			int w = block_rightup - block_leftdown - m_blocksLine * h + 1;
	//			for(int m= block_leftdown; m <= block_rightup; m += m_blocksLine )
	//			{
	//				for(int n =m; n<m + w; n++ )
	//				{
	//					m_pBlockArray[k][n].m_blockFacesNumber++;
	//				}
	//			}
	//
	//		}
	// #if USE_BUFFER_ALLOCATE_BLOCKFACE
	//		int nBuffSize = 0;
	//		//Alloc the space
	//		for (i = 0; i < m_blocksNumber; i++)
	//		{
	//			int nbFace = m_pBlockArray[k][i].m_blockFacesNumber;
	//			if(nbFace > 0)
	//			{
	//				nBuffSize += (nbFace + 1) &~1;
	//				//m_pBlockArray[k][i].m_blockFaces =  gll_new u16[nbFace];
	//				//if(!m_pBlockArray[k][i].m_blockFaces )
	//				//	return -101;
	//			}
	//		}
	//		//New buffer
	//		u16 *pBuff = nBuffSize > 0 ?  gll_new u16[nBuffSize] : NULL;
	//		GLL_ASSERT(nBuffSize == 0 || pBuff);
	//		if( nBuffSize > 0)
	//		{
	//			//Alloc the space
	//			for (i = 0; i < m_blocksNumber; i++)
	//			{
	//				int nbFace = m_pBlockArray[k][i].m_blockFacesNumber;
	//				if(nbFace > 0)
	//				{
	//					m_pBlockArray[k][i].m_blockFaces =  pBuff;
	//					pBuff += (nbFace + 1) &~1;;
	//					//if(!m_pBlockArray[k][i].m_blockFaces )
	//					//	return -101;
	//				}
	//			}
	//		}
	// #else
	//		//Alloc the space
	//		for (i = 0; i < m_blocksNumber; i++)
	//		{
	//			int nbFace = m_pBlockArray[k][i].m_blockFacesNumber;
	//			if(nbFace > 0)
	//			{
	//				m_pBlockArray[k][i].m_blockFaces = gll_new u16[nbFace];
	//				if(!m_pBlockArray[k][i].m_blockFaces )
	//					return -101;
	//			}
	//		}
	// #endif
	//	// fill the lists

	//	faceIndex = 0;
	//	for (i = 0; i < m_nbFaces[k]; i++)
	//	{

	//		int a = m_faces[k][faceIndex++];
	//		int b = m_faces[k][faceIndex++];
	//		int c = m_faces[k][faceIndex++];
	//		faceIndex++;
	//		//Register the face to block
	//		int minX = MIN_IN_THREE(m_vertex[3 * a], m_vertex[3 * b], m_vertex[3 * c]);
	//		int maxX = MAX_IN_THREE(m_vertex[3 * a], m_vertex[3 * b], m_vertex[3 * c]);
	//		int minZ = MIN_IN_THREE(m_vertex[3 * a + 2], m_vertex[3 * b + 2], m_vertex[3 * c + 2]);
	//		int maxZ = MAX_IN_THREE(m_vertex[3 * a + 2], m_vertex[3 * b + 2], m_vertex[3 * c + 2]);
	//		int block_leftdown = GetBlockIndex(minX, minZ);
	//		int block_rightup = GetBlockIndex(maxX, maxZ);
	//		int h = (block_rightup - block_leftdown) / m_blocksLine ;
	//		int w = block_rightup - block_leftdown - m_blocksLine * h + 1;
	//		for(int m = block_leftdown; m <= block_rightup; m += m_blocksLine )
	//		{
	//			for(int n = m; n<m + w; n++ )
	//			{
	//				u16 &curIndex = m_pBlockArray[k][n].m_blockCurrentIndex;
	//				if( curIndex < m_pBlockArray[k][n].m_blockFacesNumber)
	//				{
	//					m_pBlockArray[k][n].m_blockFaces[curIndex++] = i;
	//				}
	//			}
	//		}

	//	}
	//}
	// printf("After Construct Physics Block\n");
	// OUTPUT_MEMORY_INFO;

	return 0;
}

f32 point[9];

bool CPhysicsMesh::IsIntersectSegment(const _Intersect_Segment &theSegment, f32 &outA, _Intersect_Info *pIntersectInfo /*= NULL*/) const
{
	////AABB TEST First
	// if(!AABB_IntersectSegment(theSegment))
	//	return false;

	// int block_leftdown = GetBlockIndex((int)theSegment.aabb.MinEdge.X, (int)theSegment.aabb.MinEdge.Z);
	// int block_rightup = GetBlockIndex((int)theSegment.aabb.MaxEdge.X, (int)theSegment.aabb.MaxEdge.Z);
	// int h = (block_rightup - block_leftdown) / m_blocksLine ;
	// int w = block_rightup - block_leftdown - m_blocksLine * h + 1;
	_Intersect_Segment localLine;
	TransformSegmentToLocalCoordinateSystem(theSegment, localLine);

	_Intersect_Ray theRay;
	CalRayFromSegment(localLine, theRay);
	//
	//	int ret = 0/*HIT_TARGET_TYPE_AIR*/;
	// int k;
	f32 localoutA = localLine.len;
	f32 outK = localLine.len;
	bool ret = false;
	int nbFaces = m_refMesh->GetNbFace();
	const u16 *faces = m_refMesh->GetFacePointer();
	const f32 *v = m_refMesh->GetVertexPointer();

	// for (k=0; k< PHYSICS_TYPE_NUM; k++)
	{
		// if(m_nbFaces/*[k]*/ <= 0 /*|| (typeFilter &  (1 << k)) == 0*/)	//No any face then skip
		//	continue;
		// Reset the tested flags.
		// memset(m_faceFlags/*[k]*/, 0, m_nbFaces/*[k]*/);
		// int curRet = (k == PHYSICS_GROUND) ? /*HIT_TARGET_TYPE_GROUND*/ 1: 2/*HIT_TARGET_TYPE_WALL*/;
		int curRet = 1;
		// for(int m= block_leftdown; m <= block_rightup; m += m_blocksLine )
		//{
		//	for(int n = m; n<m + w; n++ )
		//	{
		//		u16 listNumber = m_pBlockArray/*[k]*/[n].m_blockFacesNumber;
		//		u16* pList = m_pBlockArray/*[k]*/[n].m_blockFaces;
		f32 maxY = INVALID_TERRAIN_HEIGHT;
		for (int i = 0; i < nbFaces; i++)
		{
			//	while (listNumber--)
			{
				;
				int faceId = i; //*pList++;

				// if(m_faceFlags/*[k]*/[faceId])	//has been tested in other blocks
				//	continue;
				// m_faceFlags/*[k]*/[faceId] = 1;	//Set this face to be tested

				faceId *= PHYSICS_FACE_SIZE;
				int iA = faces /*[k]*/[faceId++] * 3;
				int iB = faces /*[k]*/[faceId++] * 3;
				int iC = faces /*[k]*/[faceId++] * 3;

				// AABB Test first
				if (
					(localLine.aabb.MaxEdge.Y < v[iA + 1] && localLine.aabb.MaxEdge.Y < v[iB + 1] && localLine.aabb.MaxEdge.Y < v[iC + 1]) ||
					(localLine.aabb.MinEdge.Y > v[iA + 1] && localLine.aabb.MinEdge.Y > v[iB + 1] && localLine.aabb.MinEdge.Y > v[iC + 1]) ||
					(localLine.aabb.MaxEdge.Z < v[iA + 2] && localLine.aabb.MaxEdge.Z < v[iB + 2] && localLine.aabb.MaxEdge.Z < v[iC + 2]) ||
					(localLine.aabb.MinEdge.Z > v[iA + 2] && localLine.aabb.MinEdge.Z > v[iB + 2] && localLine.aabb.MinEdge.Z > v[iC + 2]) ||
					(localLine.aabb.MaxEdge.X < v[iA] && localLine.aabb.MaxEdge.X < v[iB] && localLine.aabb.MaxEdge.X < v[iC]) ||
					(localLine.aabb.MinEdge.X > v[iA] && localLine.aabb.MinEdge.X > v[iB] && localLine.aabb.MinEdge.X > v[iC]))
				{
					continue;
				}

				point[0] = v[iA++];
				point[1] = v[iA++];
				point[2] = v[iA++];
				point[3] = v[iB++];
				point[4] = v[iB++];
				point[5] = v[iB++];
				point[6] = v[iC++];
				point[7] = v[iC++];
				point[8] = v[iC++];
				if (CPhysicsGeom::IsRayIntersectTriangle(theRay, point, outK, NULL))
				{
					if (outK < localoutA)
					{
						localoutA = outK;
						if (pIntersectInfo)
						{
							pIntersectInfo->pGeom = this;
							// pIntersectInfo->pAcctor = m_pcActor;
							// Calculate Normal and Intersection Point
							VEC3 vAB(point[3] - point[0], point[4] - point[1], point[5] - point[2]);
							VEC3 vAC(point[6] - point[0], point[7] - point[1], point[8] - point[2]);
							//								VEC3Cross(&pIntersectInfo->normal, &vAB, &vAC);
							//								pIntersectInfo->normal = vAB;
							pIntersectInfo->normal = vAC.crossProduct(vAB);
							//								VEC3Normalize(&pIntersectInfo->normal, &pIntersectInfo->normal);
							pIntersectInfo->normal.normalize();
							// pIntersectInfo->nGroundMaterialType = k;
						}

						ret = true;
						//	return true;
					}
				}

				//		}
			}
		}
	}
	if (ret)
	{
		outA = localoutA * theSegment.len / localLine.len;
		if (pIntersectInfo)
		{
			pIntersectInfo->pGeom = this;
			// pIntersectInfo->pActor = m_pActor;
			// Calculate Normal and Intersection Point
			pIntersectInfo->point = theRay.ray.P + theRay.ray.d * localoutA;

			pIntersectInfo->normal.X *= m_invAbsScale.X;
			pIntersectInfo->normal.Y *= m_invAbsScale.Y;
			pIntersectInfo->normal.Z *= m_invAbsScale.Z;
			m_absTransform.rotateVect(pIntersectInfo->normal);

			pIntersectInfo->normal.normalize();

#ifdef _DEBUG
			VEC3 testInterPoint;
			m_absTransform.transformVect(testInterPoint, pIntersectInfo->point);
			f32 testError = (testInterPoint - theSegment.seg.P0).getLengthSQ() - outA * outA;
			if (testError > 0.1f)
			{
				_LOG("Physics Mesh Numeric Error %2.3f\n", testError);
			}
#endif
		}
	}
	return ret;
}

// f32 CPhysicsMesh::GetHeight(f32 x, f32 z, f32 yMin, f32 yMax)
//{
//	//int blocksList = GetBlockIndex((int)x, (int)z);
//	//u16 listNumber = m_pBlockArray/*[PHYSICS_GROUND]*/[blocksList].m_blockFacesNumber;
//	//u16* pList = m_pBlockArray/*[PHYSICS_GROUND]*/[blocksList].m_blockFaces;
//
//	f32 maxY = INVALID_TERRAIN_HEIGHT;
//	//while (listNumber--)
//	//{
//	//	int faceId = *pList++;
//	for(int i = 0; i< m_nbFaces; i++)
//	{
//
//		int faceId = i;//*pList++;
//			faceId *= PHYSICS_FACE_SIZE;
//		int iA = m_faces/*[PHYSICS_GROUND]*/[faceId ++] * 3;
//		int iB = m_faces/*[PHYSICS_GROUND]*/[faceId ++] * 3;
//		int iC = m_faces/*[PHYSICS_GROUND]*/[faceId ++] * 3;
//
//		// SET coords
//		if ((x < m_vertex[iA] && x < m_vertex[iB]  && x < m_vertex[iC] ) ||
//			(x > m_vertex[iA] && x > m_vertex[iB]  && x > m_vertex[iC]) ||
//			(z < m_vertex[iA + 2] && z < m_vertex[iB + 2] && z < m_vertex[iC + 2]) ||
//			(z > m_vertex[iA + 2] && z > m_vertex[iB + 2] && z > m_vertex[iC + 2]) ||
//			(yMax < m_vertex[iA + 1] && yMax < m_vertex[iB + 1] && yMax < m_vertex[iC + 1]) ||
//			(yMin > m_vertex[iA + 1] && yMin > m_vertex[iB + 1] && yMin > m_vertex[iC + 1])
//			)
//		{
//			continue;
//		}
//
//
//		point[0] = m_vertex[iA++];
//		point[1] = m_vertex[iA++];
//		point[2] = m_vertex[iA++];
//		point[3] = m_vertex[iB++];
//		point[4] = m_vertex[iB++];
//		point[5] = m_vertex[iB++];
//		point[6] = m_vertex[iC++];
//		point[7] = m_vertex[iC++];
//		point[8] = m_vertex[iC++];
//
//		f32 outY;
//		if(IsVerticalRayIntersectTriangle(point, x, z, outY))
//		{
//			if(outY > yMin && outY <= yMax && outY > maxY)
//			{
//				maxY = outY;
//
//			}
//		}
//
//	}
//	return maxY;
// }

// TODO:
bool CPhysicsMesh::IsPointInThis(const VEC3 &pt) const
{
	if (!AABB_IntersectPoint(pt))
		return false;
	//	if(	)
	return true;

	return false;
}

// void CPhysicsMesh::UpdateAABB()
//{
//	GLL_ASSERT(m_info.parentTransformSet);
//
//	m_AABB.MaxEdge.X = fabs(m_localAABB.MaxEdge.X * m_absTransform[0]) +
//		fabs(m_halfSize.Y * m_absTransform[4]) +
//		fabs(m_halfSize.Z * m_absTransform[8]) + m_absTransform[12];
//	m_AABB.MinEdge.X = -m_AABB.MaxEdge.X + m_absTransform[12];
//	m_AABB.MaxEdge.X += m_absTransform[12];
//
//	m_AABB.MaxEdge.Y = fabs(m_halfSize.X * m_absTransform[1]) +
//		fabs(m_halfSize.Y * m_absTransform[5]) +
//		fabs(m_halfSize.Z * m_absTransform[9]);
//	m_AABB.MinEdge.Y = -m_AABB.MaxEdge.Y + m_absTransform[13];
//	m_AABB.MaxEdge.Y += m_absTransform[13];
//
//
//	m_AABB.MaxEdge.Z = fabs(m_halfSize.X * m_absTransform[2]) +
//		fabs(m_halfSize.Y * m_absTransform[6]) +
//		fabs(m_halfSize.Z * m_absTransform[10]);
//	m_AABB.MinEdge.Z = -m_AABB.MaxEdge.Z + m_absTransform[14];
//	m_AABB.MaxEdge.Z += m_absTransform[14];
// }

// bool CPhysicsMesh::IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo /*= NULL*/) const
//{
//	return false;
// }

// int CPhysicsMesh::MoveSlide(f32 &x0, f32 &y0, f32 &z0, f32 x1, f32 z1, f32 upSlope /*= 1.0f*/, f32 downSlope /*= 1.5f*/)
//{
//	f32 dist = sqrt((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0));
//	f32 yCanUp = y0 + dist * upSlope;
//	f32 yOrigin = yCanUp + 100;
//	f32 yCanDown = y0 - dist * downSlope;
//	int ret = HIT_TARGET_TYPE_WALL;
//
//	f32 outY = GetHeight(x1, z1, yCanDown, yOrigin);
//	if(outY <= yCanUp && outY >= yCanDown)
//	{
//		x0 = x1;
//		z0 = z1;
//		y0 = outY;
//		return HIT_TARGET_TYPE_AIR;
//	}
//	else	// find the block edge
//	{
//		//int blockList0 = GetBlockIndex((int)x0, (int)z0);
//		//u16 listNumber0 = m_pBlockArray/*[PHYSICS_GROUND]*/[blockList0].m_blockFacesNumber;
//		//u16 *pList = m_pBlockArray/*[PHYSICS_GROUND]*/[blockList0].m_blockFaces;
//		//while (listNumber0--)
//		for(int i = 0; i< m_nbFaces; i++)
//		{
//
//			int faceId = i;//*pList++;
//			faceId *= PHYSICS_FACE_SIZE;
//			int edgeInfo = m_faces/*[PHYSICS_GROUND]*/[faceId + 3];
//			if(edgeInfo == 0)
//				continue;
//
//			int iA = m_faces/*[PHYSICS_GROUND]*/[faceId ++] * 3;
//			int iB = m_faces/*[PHYSICS_GROUND]*/[faceId ++] * 3;
//			int iC = m_faces/*[PHYSICS_GROUND]*/[faceId ++] * 3;
//			int index0, index1;
//			for(int i=0; i<3; i++)
//			{
//				if(edgeInfo & (1 << i))
//				{
//					if(i == 0)
//					{
//						index0 = iA;
//						index1 = iB;
//					}
//					else if(i == 1)
//					{
//						index0 = iB;
//						index1 = iC;
//					}
//					else
//					{
//						index0 = iA;
//						index1 = iC;
//					}
//
//
//					if ((x0 < m_vertex[index0] && x0 < m_vertex[index1] &&
//						x1 < m_vertex[index0] && x1 < m_vertex[index1] ) ||
//						(x0 > m_vertex[index0] && x0 > m_vertex[index1] &&
//						x1 > m_vertex[index0] && x1 > m_vertex[index1] ) ||
//						(z0 < m_vertex[2 + index0] && z0 < m_vertex[2 + index1]  &&
//						z1 < m_vertex[2 + index0] && z1 < m_vertex[2 + index1] ) ||
//						(z0 > m_vertex[2 + index0] && z0 > m_vertex[2 + index1]  &&
//						z1 > m_vertex[2 + index0] && z1 > m_vertex[2 + index1] ) ||
//
//						(yOrigin < m_vertex[index0 + 1] && yOrigin < m_vertex[index1 + 1] ) ||
//						 (yCanDown > m_vertex[index0 + 1] && yCanDown > m_vertex[index1 + 1] )
//						)
//					{
//						continue;
//					}
//					if(IsLineIntersectLine(x0, z0, x1, z1, m_vertex[index0], m_vertex[index0 + 2], m_vertex[index1], m_vertex[index1 + 2]))
//					{
//						//find a outer line
//						f32 dc_x = (f32)(m_vertex[index1] - m_vertex[index0]);
//						f32 dc_z = (f32)(m_vertex[index1 + 2] - m_vertex[index0 + 2]);
//						f32 dc_l = /*FSqrt*/(dc_x * dc_x + dc_z * dc_z);
//						f32 dot_l = ((x1 - x0) * dc_x + (z1 - z0) * dc_z);
//
//						f32 tgX = x0 + dc_x * dot_l / dc_l;
//						f32 tgZ = z0 + dc_z * dot_l / dc_l;
//						if(dc_x && dc_z) // the line is not parallel to X-Axis or Y-Axis
//						{
//							f32 normX = tgX - x1;
//							f32 normZ = tgZ - z1;
//							if( fabs(normX) >= fabs(normZ))
//							{
//								tgX += (normX > 0) ? 1 : -1;
//							}
//							else
//							{
//								tgZ += (normZ > 0) ? 1 : -1;
//							}
//						}
//
//
//						outY = (GetHeight(tgX, tgZ, yCanDown, yOrigin));
//						if(outY <= yCanUp && outY >= yCanDown)
//						{
//							x0 = tgX;
//							z0 = tgZ;
//							y0 = outY;
//							return HIT_TARGET_TYPE_GROUND;
//						}
//						ret = HIT_TARGET_TYPE_WALL;
////						return false;
//
//					}
//
//				}
//
//			}
//
//		}
//
//	}
//
//	return ret;
//
//}

#undef USE_NW4R_COORDINATE