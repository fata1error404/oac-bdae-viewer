#ifndef PARSER_TRN_H
#define PARSER_TRN_H

#include <vector>
#include <memory>  // for std::shared_ptr
#include <utility> // for std::pair, std::move
#include <unordered_map>
#include "libs/glm/glm.hpp"
#include "AABB.h"
#include "Quaternion.h"
#include "terrain.h"
#include "water.h"
#include "CZipResReader.h"
#include "parserBDAE.h"
#include "parserPHY.h"

#define ChunksInTile (8 * 8)
#define ChunksInTileRow 8
#define ChunksInTileCol 8
#define UnitsInTileRow 64
#define UnitsInTileCol 64

#define DEFAULT_LOAD_BUFFER_SIZE 102400 // 100 KB

static unsigned char loadBuffer[DEFAULT_LOAD_BUFFER_SIZE]; // static read buffer to load .trn files into memory without dynamic allocation

static std::unordered_map<std::string, std::shared_ptr<Model>> bdaeModelCache; // terrain's global cache for .bdae models (key — filename, value — shared pointer)

// 1 tile = 8 × 8 chunks = 64 × 64 units = 65 x 65 vertices

// Vertices:   o──o──o──o──o   ← 4 units → 5 vertices
// Units:       ── ── ── ──

// 24 bytes
struct TRNFileHeader
{
	char signature[4];		 // 4 bytes  file signature – 'ATIL' for .trn file
	unsigned int version;	 // 4 bytes  format version
	int gridX;				 // 4 bytes  X-axis grid position
	int gridZ;				 // 4 bytes  Z-axis grid position
	unsigned int flag;		 // 4 bytes  mask layers detector (bitmask)
	short waterTexNameIndex; // 2 bytes  index of water texture (where (?))
	short liquidType;		 // 2 bytes  liquid type: water = 0 | magma = 1 | death = 2
};

// 12 bytes, repeated ChunksInTileRow x ChunksInTileCol = 64 times
struct ChunkInfo
{
	unsigned int flag;	 // 4 bytes  water flag (?)
	short waterLevel;	 // 2 bytes  water plane height
	short texNameIndex1; // 2 bytes  index into texture list in the string data section
	short texNameIndex2; // 2 bytes  ..
	short texNameIndex3; // 2 bytes  ..
};

// per-chunk terrain flags
enum
{
	TRNF_VISIBLE = 1 << 0,
	TRNF_DIRTYLAYER0 = 1 << 1,
	TRNF_DIRTYLAYER1 = 1 << 2,
	TRNF_DIRTYLAYER2 = 1 << 3,
	TRNF_HASWATER = 1 << 16,
	TRNF_ISHOLE = 1 << 17,
};

// Class for loading terrain tiles.
// ________________________________

class TileTerrain
{
  public:
	unsigned int trnVAO, trnVBO, navVAO, navVBO, phyVAO, phyVBO;
	unsigned int terrainVertexCount, navmeshVertexCount, physicsVertexCount;
	std::vector<float> terrainVertices, navigationVertices, physicsVertices; // vertex data
	std::vector<Physics *> physicsGeometry;									 // .phy models
	std::vector<std::pair<std::shared_ptr<Model>, glm::mat4>> models;		 // .bdae models
	std::vector<std::string> textureNames;
	unsigned int textureMap;
	Water water; // water surface

	float startX, startZ;							 // position on the grid in world space coordinates
	float Y[UnitsInTileRow + 1][UnitsInTileCol + 1]; // height map (unscaled)
	AABB BBox;										 // bounding box

	ChunkInfo chunks[ChunksInTile];

	glm::u8vec4 colors[UnitsInTileRow + 1][UnitsInTileCol + 1]; // vertex colors
	glm::vec3 normals[UnitsInTileRow + 1][UnitsInTileCol + 1];	// normal vectors

	TileTerrain()
		: startX(0), startZ(0),
		  trnVAO(0), trnVBO(0),
		  navVAO(0), navVBO(0),
		  phyVAO(0), phyVBO(0),
		  terrainVertexCount(0),
		  navmeshVertexCount(0),
		  physicsVertexCount(0),
		  BBox(VEC3(0, 0, 0), VEC3(0, 0, 0))
	{
		memset(&chunks, 0, sizeof(chunks));
		memset(&Y, 0, sizeof(Y));
	};

	~TileTerrain()
	{
		glDeleteVertexArrays(1, &trnVAO);
		glDeleteVertexArrays(1, &navVAO);
		glDeleteVertexArrays(1, &phyVAO);
		glDeleteBuffers(1, &trnVBO);
		glDeleteBuffers(1, &navVBO);
		glDeleteBuffers(1, &phyVBO);

		if (!textureNames.empty())
		{
			glDeleteTextures(1, &textureMap);
			textureNames.clear();
		}

		trnVAO = trnVBO = navVAO = navVBO = phyVAO = phyVBO = 0;

		terrainVertexCount = navmeshVertexCount = physicsVertexCount = 0;

		terrainVertices.clear();
		navigationVertices.clear();
		physicsVertices.clear();

		models.clear();

		water.release();
	}

	//! Processes a single .trn file and returns a newly created TileTerrain object with the tile's mesh data saved.
	static TileTerrain *load(IReadResFile *trnFile, int &gridX, int &gridZ, Terrain &terrain);
};

#endif
