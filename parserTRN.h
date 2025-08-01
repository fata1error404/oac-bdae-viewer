#ifndef PARSER_TRN_H
#define PARSER_TRN_H

#include "CZipResReader.h"
#include "AABB.h"
#include "VEC3.h"
#include "Quaternion.h"
#include "parserBDAE.h"
#include "parserPHY.h"
#include "libs/glm/glm.hpp"
#include "libs/glm/fwd.hpp"
#include "libs/glm/gtc/type_ptr.hpp"
#include "libs/glm/gtc/constants.hpp"
#include "libs/glm/gtc/packing.hpp"
#include "libs/glm/ext/vector_uint4.hpp"
#include "libs/glm/gtc/type_precision.hpp"
#include "libs/glm/gtc/matrix_transform.hpp"

class IReadResFile;
class Terrain;

#define UnitsInTile (8 * 8)
#define ChunksInTileRow 8
#define ChunksInTileCol 8
#define UnitsInTileRow 64
#define UnitsInTileCol 64

#define DEFAULT_LOAD_BUFFER_SIZE 102400 // 100 KB

static unsigned char s_loadBuffer[DEFAULT_LOAD_BUFFER_SIZE];

//
struct TRNFileHeader
{
    char Signature[4];    // 4 bytes  signature
    unsigned int Version; // 4 bytes  format version
    int gridX;            // 4 bytes  X-axis grid position
    int gridZ;            // 4 bytes  Z-axis grid position
    unsigned int flag;    // 4 bytes  explain
    short waterTexID;     // 2 bytes  explain
    short liquidType;     // 2 bytes  explain
};

//
struct TileChunk
{
    unsigned int flag;
    short waterLevel;
    short texNameIndex1;
    short texNameIndex2;
    short texNameIndex3;
};

// Class for loading terrain tiles.
// ________________________________

class TileTerrain
{
public:
    float startX, startZ;                      // tile's position on the grid in world space coordinates
    float Y[UnitsInTile + 1][UnitsInTile + 1]; // tile's height map (unscaled)
    AABB BBox;                                 // tile's bounding box
    std::vector<Physics *> physicsGeometry;
    std::vector<Model *> models;

    TileChunk chunks[UnitsInTile];
    glm::u8vec4 blending[UnitsInTile + 1][UnitsInTile + 1];
    glm::vec3 normals[UnitsInTile + 1][UnitsInTile + 1];

    TileTerrain()
    {
        startX = 0;
        startZ = 0;

        memset(&chunks, 0, sizeof(chunks));
        memset(&Y, 0, sizeof(Y));
    };

    //! Processes a single .trn file and returns a newly created TileTerrain object with the tile's mesh data saved.
    static TileTerrain *loadTileTerrain(IReadResFile *trnFile, int &gridX, int &gridZ, Terrain &terrain);
};

#include "parserITM.h"

#endif
