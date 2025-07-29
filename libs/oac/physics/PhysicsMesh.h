#pragma once
#include "PhysicsGeom.h"

/*#define PHYSICS_GROUND		0
#define PHYSICS_WATER		1
#define PHYSICS_TYPE_NUM	2	*/ 

//#define PHYSICS_HIGH_COVER	2  //Same as Wall, but different in AI
//This one has been changed to use Geom type, Not in Ground Mesh   [11/15/2007 baibo.zhang]


class CLevel;
class IResFileStream;


//#define PHISICS_CELL_SHIFT			(10)
//#define PHISICS_CELL_HALF_DIM		(1 << (PHISICS_CELL_SHIFT - 1))
//#define PHISICS_CELL_FULL_DIM		(1 << (PHISICS_CELL_SHIFT))
//#define PHISICS_CELL_CLIP_DIM		((PHISICS_CELL_FULL_DIM) + (PHISICS_CELL_HALF_DIM))


#define PHYSICS_FACE_A				0
#define PHYSICS_FACE_B				1
#define PHYSICS_FACE_C				2
#define PHYSICS_FACE_EDGE_INFO	    3
#define PHYSICS_FACE_SIZE			4

#define EDGE_INFO_OUTER_AB		0x1
#define EDGE_INFO_OUTER_BC		0x2
#define EDGE_INFO_OUTER_CA		0x4
#define EDGE_INFO_OUTER_ABC		(EDGE_INFO_OUTER_AB | EDGE_INFO_OUTER_BC | EDGE_INFO_OUTER_CA)


class CPhysicsRefMesh 
{
public:
	CPhysicsRefMesh(int nVertex, f32 * fVertex, int nFaces, u16 *facePtr) 
		: ReferenceCounter(1)
		, m_nbVertex(nVertex)
		, m_vertex(fVertex)
		, m_nbFaces(nFaces)
		, m_faces(facePtr)
	{
	}
	~CPhysicsRefMesh(){
		SAFE_DEL_ARRAY(m_vertex);	
		SAFE_DEL_ARRAY(m_faces);
	};
	void grab() const { 
		++ReferenceCounter; 
	}
	bool drop() const
	{
		--ReferenceCounter;
		if (!ReferenceCounter)
		{
			delete this;
			return true;
		}
		return false;
	}


	//! Get the reference count.
	/** \return Current value of the reference counter. */
	int32 getReferenceCount() const
	{
		return ReferenceCounter;
	}
	
	//void Set(int nVertex, f32 * fVertex, int nFaces, u16 *facePtr)
	//{
	//	m_nbVertex = nVertex;
	//	m_vertex = fVertex;
	//	m_nbFaces = nFaces;
	//	m_faces = facePtr;
	//}

	int GetNbVertex() const {
		return  m_nbVertex;
	}
	
	const f32 * GetVertexPointer() const {
		return m_vertex;
	}

	int GetNbFace() const {
		return m_nbFaces;
	}
	const u16 * GetFacePointer() const {
		return m_faces;
	}

private:
	mutable int32	ReferenceCounter;
	int				m_nbVertex;
	f32*			m_vertex;

	int				m_nbFaces/*[PHYSICS_TYPE_NUM]*/;
	u16				*m_faces/*[PHYSICS_TYPE_NUM]*/;

};
//struct MeshBlock
//{
//	u16*	m_blockFaces;
//	u16		m_blockFacesNumber;
//	u16		m_blockCurrentIndex;
//
//	MeshBlock();
//	~MeshBlock();
//};

//ignore the m_rotY member in the base class
//ignore the m_scale
class CPhysicsMesh :
	public CPhysicsGeom
{
public:
	CPhysicsMesh();
	CPhysicsMesh(const VEC3 &pos, f32 rotY, f32 halfW, f32 halfH, f32 halfL);
public:
	~CPhysicsMesh(void);
	//int GetBlockIndex(int x, int z) const;
	int loadFromFile(CResFileStream * pFile);
	
	//virtual void SetSize(f32 halfW, f32 halfH, f32 halfL) {};
//	f32 GetSize() const {return 1.0f;}
	virtual bool IsIntersectSegment(const _Intersect_Segment &theSegment, f32 &outA, _Intersect_Info *pIntersectInfo = NULL)  const;
	virtual bool IsPointInThis(const VEC3 &pt) const;
	//virtual f32 GetSizeZ() const {return m_AABB.MaxEdge.Z - m_AABB.MinEdge.Z;};
	//virtual f32 GetSizeX() const {return m_AABB.MaxEdge.X - m_AABB.MinEdge.X;};
	//virtual f32 GetSizeY() const {return m_AABB.MaxEdge.Y - m_AABB.MinEdge.Y;};
	//virtual bool IsIntersectSphere(const VEC3 &pos, f32 radius, const VEC3 &speed, _Intersect_Info *pIntersectInfo = NULL) const;

	//f32 GetHeight(f32 x, f32 z, f32 yMin, f32 yMax);
	//int MoveSlide(f32 &x0, f32 &y0, f32 &z0, f32 x1, f32 z1, f32 upSlope = 1.5f, f32 downSlope = 2.0f);
	virtual CPhysicsGeom * Clone() const;

	//CPhysicsRefMesh * GetRefMesh()  {
	//	return m_refMesh;
	//}
	const CPhysicsRefMesh * GetRefMesh() const {
		return m_refMesh;
	}
	void SetRefMesh(const CPhysicsRefMesh *refMesh)
	{
		if(refMesh)
			refMesh->grab();
			
		if(m_refMesh)
			m_refMesh->drop();
		m_refMesh = (CPhysicsRefMesh *)refMesh;

	}


#if RENDER_GEOM
	virtual void Render() const;	
#endif
#ifdef _AFANTY_
	virtual int GenerateMeshObj(rcMeshLoaderObj& meshObj, int vOff) const;
#endif
protected:
	//void UpdateAABB();

private:
	CPhysicsRefMesh *m_refMesh;
	//int				m_nbVertex;
	//f32*			m_vertex;

	//int				m_nbFaces/*[PHYSICS_TYPE_NUM]*/;
	//u16				*m_faces/*[PHYSICS_TYPE_NUM]*/;
//	mutable	u8		*m_faceFlags/*[PHYSICS_TYPE_NUM]*/;





	//MeshBlock*		m_pBlockArray[PHYSICS_TYPE_NUM];
	//AABB			m_localAABB;	
	//f32				m_minX;
	//f32				m_minZ;
	//f32				m_maxX;
	//f32				m_maxZ;

	//s32				m_blocksNumber;
	//s32				m_blocksLine;
	//s32				m_blocksLineH;
	//s32				m_blocksSX;
	//s32				m_blocksSZ;
	//s32				m_blocksEX;
	//s32				m_blocksEZ;

};
