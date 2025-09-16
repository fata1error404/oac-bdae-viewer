#ifndef PARSER_PHY_H
#define PARSER_PHY_H

#define PHYSICS_FACE_SIZE 4

#define PHYSICS_GEOM_TYPE_SPHERE 1
#define PHYSICS_GEOM_TYPE_PLANE 2
#define PHYSICS_GEOM_TYPE_BOX 3
#define PHYSICS_GEOM_TYPE_CYLINDER 4
#define PHYSICS_GEOM_TYPE_HORIZONTAL_CYLINDER 5
#define PHYSICS_GEOM_TYPE_TRIANGLE 6
#define PHYSICS_GEOM_TYPE_MESH 7
#define PHYSICS_GEOM_TYPE_INFINITE_PLANE 8

// class Terrain;

class PhysicsRefMesh
{
  public:
	PhysicsRefMesh(int nVertex, float *fVertex, int nFaces, unsigned short *facePtr)
		: ReferenceCounter(1), m_vertex(fVertex), m_nbVertex(nVertex), m_nbFaces(nFaces), m_faces(facePtr) {}

	~PhysicsRefMesh()
	{
		delete[] m_vertex;
		delete[] m_faces;
	}

	void grab() const
	{
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

	int getReferenceCount() const { return ReferenceCounter; }

	int GetNbVertex() const { return m_nbVertex; }

	int GetNbFace() const { return m_nbFaces; }

	const float *GetVertexPointer() const { return m_vertex; }

	const unsigned short *GetFacePointer() const { return m_faces; }

	mutable int ReferenceCounter;
	float *m_vertex;
	int m_nbVertex;
	int m_nbFaces;
	unsigned short *m_faces;
};

struct PHYFileHeader
{
	int meshCount;
	int type;
	int flags;
	float rotY;
	float transX;
	float transY;
	float transZ;
	float halfSizeX;
	float halfSizeY;
	float halfSizeZ;
};

// Abstract class for the Geometry of physics.
// ___________________________________________

class Physics
{
  public:
	VEC3 m_pos;
	VEC3 m_halfSize;

	float m_rotY;
	int m_geomType;

	AABB m_AABB;
	AABB m_groupAABB;
	Physics *m_pNext;
	Physics *m_pTrigger; // (?)

	MTX4 m_absTransform;
	MTX4 m_invAbsTransform;
	VEC3 m_absScale;
	VEC3 m_invAbsScale;

	PhysicsRefMesh *m_refMesh;

	union
	{
		int s32value;
		struct
		{
			int house_inner_type : 8; // indicate entity_house flag
			int mtl_type : 8;
			int entityIndex : 14;
			int parentTransformSet : 1;
			int _pad1 : 1;
		};

	} m_info;

	Physics() : m_rotY(0), m_pNext(NULL), m_pTrigger(NULL) {};

	Physics(const VEC3 &pos, float rotY, float halfW, float halfH, float halfL, int type)
		: m_pos(pos),
		  m_rotY(rotY),
		  m_halfSize(halfW, halfH, halfL),
		  m_geomType(type),
		  m_pNext(NULL),
		  m_pTrigger(NULL),
		  m_refMesh(NULL) {};

	~Physics()
	{
		if (m_refMesh)
		{
			m_refMesh->drop();
		}

		if (m_pNext)
			delete m_pNext;

		if (m_pTrigger)
			delete m_pNext;
	}

	//!
	static Physics *load(CZipResReader *archive, const char *fname, bool isEntityHouse)
	{
		std::string tmpName = std::string(fname).replace(std::strlen(fname) - 5, 5, ".phy");

		IReadResFile *phyFile = archive->openFile(tmpName.c_str());

		if (!phyFile)
			return nullptr;

		phyFile->seek(0);
		int fileSize = static_cast<int>(phyFile->getSize());
		unsigned char *buffer = new unsigned char[fileSize];
		phyFile->read(buffer, fileSize);
		phyFile->drop();

		int nbMesh;
		std::memcpy(&nbMesh, buffer, sizeof(int));

		Physics *head = nullptr;
		Physics *tail = nullptr;

		for (int i = 0; i < nbMesh; ++i)
		{
			int type;
			std::memcpy(&type, buffer + 4, sizeof(int));
			Physics *node = nullptr;

			if (type == PHYSICS_GEOM_TYPE_MESH)
			{
				node = new Physics();
				node->loadMesh(buffer);
			}
			else
			{
				PHYFileHeader header;
				std::memcpy(&header, buffer, sizeof(PHYFileHeader));
				VEC3 pos(header.transX, header.transY, header.transZ);
				float ry = header.rotY;
				float hx = header.halfSizeX;
				float hy = header.halfSizeY;
				float hz = header.halfSizeZ;

				switch (type)
				{
				case PHYSICS_GEOM_TYPE_PLANE:
					node = new Physics(pos, ry, hx, hy, 0.0f, PHYSICS_GEOM_TYPE_PLANE);
					break;
				case PHYSICS_GEOM_TYPE_BOX:
					node = new Physics(pos, ry, hx, hy, hz, PHYSICS_GEOM_TYPE_BOX);
					break;
				case PHYSICS_GEOM_TYPE_CYLINDER:
					node = new Physics(pos, 0.0f, hx, hy, hz, PHYSICS_GEOM_TYPE_CYLINDER);
					break;
				default:
					std::cout << "Unknown Physics geom type " << type << std::endl;
					break;
				}
			}

			if (!node)
				continue;

			if (!head)
				head = tail = node;
			else
			{
				tail->m_pNext = node;
				tail = node;
			}
		}

		delete[] buffer;
		return head;
	}

	void loadMesh(unsigned char *buffer)
	{
		PHYFileHeader *header = (PHYFileHeader *)buffer;

		int offset = 8;
		int size;
		VEC3 pos;

		m_geomType = 7;

		memcpy(&m_info.s32value, buffer + offset, sizeof(int));
		memcpy(&m_pos.X, buffer + offset + 4, sizeof(float));
		memcpy(&m_pos.Z, buffer + offset + 4 + sizeof(float), sizeof(float));
		memcpy(&m_pos.Y, buffer + offset + 4 + 2 * sizeof(float), sizeof(float));
		memcpy(&m_halfSize.X, buffer + offset + 4 + 3 * sizeof(float), sizeof(float));
		memcpy(&m_halfSize.Z, buffer + offset + 4 + 4 * sizeof(float), sizeof(float));
		memcpy(&m_halfSize.Y, buffer + 4 + offset + 5 * sizeof(float), sizeof(float));

		// Read mesh header
		short nv, nbFaces;
		memcpy(&nv, buffer + offset + 4 + 6 * sizeof(float), sizeof(short));
		memcpy(&nbFaces, buffer + offset + 4 + 6 * sizeof(float) + sizeof(short), sizeof(short));

		// Read mesh vertices
		size = 3 * nv;
		float *vertex = new float[size];

		if (!vertex)
			return;

		offset += 4 + 6 * sizeof(float) + 2 * sizeof(short);

		for (int i = 0; i < nv; i++)
		{
			memcpy(&vertex[i * 3], buffer + offset, sizeof(float));
			offset += sizeof(float); // X
			memcpy(&vertex[i * 3 + 2], buffer + offset, sizeof(float));
			offset += sizeof(float);
			memcpy(&vertex[i * 3 + 1], buffer + offset, sizeof(float));
			offset += sizeof(float); // Y
		}

		unsigned short *faces = NULL;

		// Read mesh faces
		size = nbFaces * PHYSICS_FACE_SIZE;
		if (size > 0)
		{
			faces = new unsigned short[size];

			if (!faces)
			{
				std::cout << "SHOULDN'T BE HERE" << std::endl;
				delete[] vertex;
				return;
			}

			for (int j = 0; j < size; ++j)
			{
				memcpy(&faces[j], buffer + offset, sizeof(short));
				offset += sizeof(short);
			}
		}

		m_refMesh = new PhysicsRefMesh(nv, vertex, nbFaces, faces);
	}

	//!
	void SetSerilParentTransform(const MTX4 &transform)
	{
		Physics *pCur = this;

		while (pCur)
		{
			pCur->SetParentTransform(transform);

			if (pCur == this)
				m_groupAABB = pCur->m_AABB;
			else
				m_groupAABB.addInternalBox(pCur->m_AABB);

			pCur = pCur->m_pNext;
		}

		if (m_pTrigger)
			m_pTrigger->SetSerilParentTransform(transform);
	}

	//!
	void SetParentTransform(const MTX4 &transform)
	{
		m_info.parentTransformSet = 1;
		m_absTransform.makeIdentity();
		m_absTransform.setRotationDegrees(VEC3(0, m_rotY, 0));
		m_absTransform.setTranslation(m_pos);
		m_absTransform = transform * m_absTransform;

		m_absTransform.getInverse(m_invAbsTransform);

		UpdateAABB();
	}

	//!
	void UpdateAABB()
	{
		m_AABB.MaxEdge.X = fabs(m_halfSize.X * m_absTransform[0]) +
						   fabs(m_halfSize.Y * m_absTransform[4]) +
						   fabs(m_halfSize.Z * m_absTransform[8]);
		m_AABB.MinEdge.X = -m_AABB.MaxEdge.X + m_absTransform[12];
		m_AABB.MaxEdge.X += m_absTransform[12];

		m_AABB.MaxEdge.Y = fabs(m_halfSize.X * m_absTransform[1]) +
						   fabs(m_halfSize.Y * m_absTransform[5]) +
						   fabs(m_halfSize.Z * m_absTransform[9]);
		m_AABB.MinEdge.Y = -m_AABB.MaxEdge.Y + m_absTransform[13];
		m_AABB.MaxEdge.Y += m_absTransform[13];

		m_AABB.MaxEdge.Z = fabs(m_halfSize.X * m_absTransform[2]) +
						   fabs(m_halfSize.Y * m_absTransform[6]) +
						   fabs(m_halfSize.Z * m_absTransform[10]);
		m_AABB.MinEdge.Z = -m_AABB.MaxEdge.Z + m_absTransform[14];
		m_AABB.MaxEdge.Z += m_absTransform[14];
	}
};

#endif
