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

// 36 bytes
struct GeometryInfo
{
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

// terrain's global cache for .phy models vertex data (key — filename, value — shared pointer)
inline std::unordered_map<std::string, std::shared_ptr<std::pair<std::vector<float>, std::vector<unsigned short>>>> physicsModelCache;

// Class for loading physics geometry.
// ___________________________________

class Physics
{
  public:
	std::shared_ptr<std::pair<std::vector<float>, std::vector<unsigned short>>> mesh; // .phy mesh shared pointer to store pair vertices + indices vectors (this data will be share pointer with cache)

	// values read from .phy file
	int geometryType;
	VEC3 position; // local translation
	VEC3 halfSize; // local 0.5 width, height and length

	MTX4 model; // model matrix that combines local space translation from .phy file and world space transformation from .itm file

	Physics *pNext; // pointer to next submesh within a single physics model

	Physics(VEC3 pos, float halfW, float halfH, float halfL, int type)
		: position(pos),
		  halfSize(halfW, halfH, halfL),
		  geometryType(type),
		  pNext(NULL) {}

	~Physics()
	{
		if (pNext)
			delete pNext;
	}

	//! Processes a single .phy file, handling multiple submeshes, different geometry types and saving all data required for rendering.
	static Physics *load(CZipResReader *archive, const char *fname)
	{
		std::string tmpName = std::string(fname);
		if (tmpName.size() >= 5)
			tmpName.replace(tmpName.size() - 5, 5, ".phy");

		IReadResFile *phyFile = archive->openFile(tmpName.c_str());

		if (!phyFile)
			return NULL;

		phyFile->seek(0);
		int fileSize = (int)phyFile->getSize();
		unsigned char *buffer = new unsigned char[fileSize];
		phyFile->read(buffer, fileSize);
		phyFile->drop();

		int submeshCount;
		memcpy(&submeshCount, buffer, sizeof(int));

		// head (first submesh = node) o ⭢ o ... o ⭢ o ⭢ o tail
		// physics model may consist of several submeshes, so we use linked list to have a chain of submeshes (gives O(1) for append operation and when deleting head node it frees the whole linked structure without leaks)

		Physics *head = NULL;
		Physics *tail = NULL;

		int offset = 4;

		// loop through each submesh
		for (int i = 0; i < submeshCount; i++)
		{
			int type;
			memcpy(&type, buffer + offset, sizeof(int));

			Physics *node = NULL; // current submesh

			if (type == PHYSICS_GEOM_TYPE_MESH) // physics mesh: search in cache; if not found, save local position and dimensions in tile's storage and vertex data in cache
			{
				// (not using PHYGeometry struct because layout for physics mesh info is 32 bytes)
				VEC3 pos(0.0f, 0.0f, 0.0f);
				float hx, hy, hz;

				memcpy(&pos.X, buffer + offset + 8, sizeof(float));
				memcpy(&pos.Z, buffer + offset + 12, sizeof(float));
				memcpy(&pos.Y, buffer + offset + 16, sizeof(float));
				memcpy(&hx, buffer + offset + 20, sizeof(float));
				memcpy(&hz, buffer + offset + 24, sizeof(float));
				memcpy(&hy, buffer + offset + 28, sizeof(float));

				short vertexCount, faceCount;
				memcpy(&vertexCount, buffer + offset + 32, sizeof(short));
				memcpy(&faceCount, buffer + offset + 34, sizeof(short));

				offset += 36;

				std::string cacheKey = std::string(fname) + "#" + std::to_string(i); // (using just file name as a key for unsorted map does not work because all physics meshes of the same .phy file would share one key)
				auto it = physicsModelCache.find(cacheKey);

				if (it != physicsModelCache.end()) // reuse cached model
				{
					offset += vertexCount * 3 * sizeof(float);
					offset += faceCount * PHYSICS_FACE_SIZE * sizeof(short);

					node = new Physics(pos, hx, hy, hz, PHYSICS_GEOM_TYPE_MESH);
					node->mesh = it->second;
				}
				else // not cached — build vertex data and insert to cache on success
				{
					std::vector<float> vertices;
					vertices.reserve(vertexCount * 3);

					for (int i = 0; i < vertexCount; i++)
					{
						float vx, vy, vz;
						memcpy(&vx, buffer + offset, sizeof(float));
						memcpy(&vz, buffer + offset + 4, sizeof(float));
						memcpy(&vy, buffer + offset + 8, sizeof(float));
						offset += 3 * sizeof(float);
						vertices.push_back(vx);
						vertices.push_back(vy);
						vertices.push_back(vz);
					}

					std::vector<unsigned short> indices;
					indices.reserve(faceCount * PHYSICS_FACE_SIZE);

					for (int i = 0, n = faceCount * PHYSICS_FACE_SIZE; i < n; i++)
					{
						unsigned short v;
						memcpy(&v, buffer + offset, sizeof(unsigned short));
						offset += sizeof(unsigned short);
						indices.push_back(v);
					}

					node = new Physics(pos, hx, hy, hz, PHYSICS_GEOM_TYPE_MESH); // create new physics mesh object

					node->mesh = std::make_shared<std::pair<std::vector<float>, std::vector<unsigned short>>>(std::move(vertices), std::move(indices)); // init shared pointer for vertex data (we share only vertices and indices, but not Physics objects! move vectors, no extra copy)

					if (node->mesh)
						physicsModelCache[cacheKey] = node->mesh; // add to global cache with filename as a key for quick lookup
					else
						std::cout << "[Debug] Physics::load: failed to load " << fname << std::endl;
				}
			}
			else // non-mesh primitive: only save local position and dimensions (these values are enough to build vertex data later)
			{
				GeometryInfo geom;
				memcpy(&geom, buffer + offset, sizeof(GeometryInfo));
				VEC3 pos(geom.transX, geom.transY, geom.transZ);
				float hx = geom.halfSizeX;
				float hy = geom.halfSizeY;
				float hz = geom.halfSizeZ;

				offset += sizeof(GeometryInfo);

				switch (type)
				{
				case PHYSICS_GEOM_TYPE_PLANE:
					node = new Physics(pos, hx, hy, 0.0f, PHYSICS_GEOM_TYPE_PLANE);
					break;
				case PHYSICS_GEOM_TYPE_BOX:
					node = new Physics(pos, hx, hy, hz, PHYSICS_GEOM_TYPE_BOX);
					break;
				case PHYSICS_GEOM_TYPE_CYLINDER:
					node = new Physics(pos, hx, hy, hz, PHYSICS_GEOM_TYPE_CYLINDER);
					break;
				default:
					std::cout << "[Debug] Physics::load: unknown physics geometry type " << type << std::endl;
					break;
				}
			}

			// if unknown geometry type, go to next submesh
			if (!node)
				continue;

			// append node to linked list
			if (!head)
				head = tail = node; // if this is first submesh in list, node becomes head and tail
			else
			{
				tail->pNext = node; // otherwise, link node after tail and update tail pointer
				tail = node;
			}
		}

		delete[] buffer;
		return head;
	}

	//! Builds local-to-world space transformation matrix for each physics model submesh (starting from head).
	void buildModelMatrix(MTX4 worldTransform)
	{
		Physics *node = this;

		// iterate through linked list
		while (node)
		{
			// compute model matrix
			node->model.makeIdentity();
			node->model.setTranslation(node->position);
			node->model = worldTransform * node->model;

			// go to next submesh
			node = node->pNext;
		}
	}
};

#endif
