#ifndef PARSER_TRN_H
#define PARSER_TRN_H

#include "CZipResReader.h"
#include "AABB.h"
#include "VEC3.h"
#include "Quaternion.h"
#include "parserPHY.h"

#define UnitsInTile (8 * 8)
#define ChunksInTileRow 8
#define ChunksInTileCol 8
#define UnitsInTileRow 64
#define UnitsInTileCol 64
#define DEFAULT_LOAD_BUFFER_SIZE 102400 // 100 KB

static unsigned char s_loadBuffer[DEFAULT_LOAD_BUFFER_SIZE];

// bool terrainLoaded = false;

struct TRNFileHeader
{
    char Signature[4];    // 4 bytes  signature
    unsigned int Version; // 4 bytes  format version
    int GridX;            // 4 bytes  x axis grid position
    int GridY;            // 4 bytes  x axis grid position
    unsigned int Flag;    // 4 bytes  explain
    short WaterTexID;     // 2 bytes  explain
    short LiquidType;     // 2 bytes  explain
};

struct TileChunk
{
    unsigned int Flag;
    short WaterLevel;
    short TexNameIndex1;
    short TexNameIndex2;
    short TexNameIndex3;
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

    TileChunk chunks[UnitsInTile];

    TileTerrain()
    {
        startX = 0;
        startZ = 0;

        memset(&chunks, 0, sizeof(chunks));
        memset(&Y, 0, sizeof(Y));
    };

    //! Processes a single .trn file and returns a newly created TileTerrain object with the tile's mesh data saved.
    static TileTerrain *loadTileTerrain(IReadResFile *trnFile, int &gridX, int &gridZ)
    {
        // 2.1. Load .trn file into memory.
        if (!trnFile)
            return NULL;

        TileTerrain *tileTerrain = new TileTerrain();

        trnFile->seek(0);
        int fileSize = (int)trnFile->getSize();
        unsigned char *buffer = s_loadBuffer;

        if (fileSize > DEFAULT_LOAD_BUFFER_SIZE)
            buffer = new unsigned char[fileSize];

        trnFile->read(buffer, fileSize);
        trnFile->drop();

        /* 2.2. Parse .trn file header. Retrieve:
                – position on the grid
                – liquid type */

        TRNFileHeader *header = (TRNFileHeader *)buffer; // interpret the first sizeof(TileFileHeader) bytes in buffer as a corresponding structure
        gridX = header->GridX;                           // write extracted grid position to the parent Terrain class
        gridZ = header->GridY;

        tileTerrain->startX = (float)gridX * UnitsInTile;
        tileTerrain->startZ = (float)gridZ * UnitsInTile;
        tileTerrain->BBox.MinEdge = VEC3(tileTerrain->startX, 0, tileTerrain->startZ);
        tileTerrain->BBox.MaxEdge = VEC3(tileTerrain->startX + UnitsInTile, 0, tileTerrain->startZ + UnitsInTile);
        // tileTerrain->_LiquidType = header->_LiquidType;

        /* 2.3. Parse chunk headers. Retrieve:
                – water level and other metadata about each chunk */

        unsigned int chunkOffset = sizeof(TRNFileHeader);

        // loop through each chunk cell in the tile (1 tile = 8 × 8 chunks = 64 × 64 units)
        for (int index = 0, i = 0; i < ChunksInTileRow; i++)
        {
            for (int j = 0; j < ChunksInTileCol; j++, index++)
            {
                TileChunk *chunk = (TileChunk *)(buffer + chunkOffset); // read the next chunk header
                tileTerrain->chunks[index] = *chunk;                    // copy its data into the tile's chunk array
                chunkOffset += sizeof(TileChunk);
            }
        }

        /* 2.4. Parse height map section. Retrieve:
                – height map */

        int minHeight = 50000;
        int maxHeight = -50000;

        // set a pointer to the height data; interpret bytes as an array of short values, accessible via indexings
        short *height = (short *)(buffer + chunkOffset);

        // loop through each unit in the tile (1 tile = 64 × 64 height values / units)
        for (int v = 0, vy = 0; vy <= UnitsInTileRow; vy++)
        {
            for (int vx = 0; vx <= UnitsInTileCol; vx++, v++)
            {
                // read 1 value and apply height scaling
                tileTerrain->Y[vy][vx] = height[v] * 0.01f;

                // update min / max for bounding box adjustment
                if (height[v] > maxHeight)
                    maxHeight = height[v];
                if (height[v] < minHeight)
                    minHeight = height[v];
            }
        }

        tileTerrain->BBox.MinEdge.Y = minHeight * 0.01f;
        tileTerrain->BBox.MaxEdge.Y = maxHeight * 0.01f;

        /* 2.5. Parse string data section. Retrieve:
                – number of textures
                – texture file names
        */

        // [TODO] annotate, save as a Class variable
        int textureCount;
        int stringDataOffset = chunkOffset + 7 * ((UnitsInTile + 1) * (UnitsInTile + 1)) + 1;
        memcpy(&textureCount, buffer + stringDataOffset, sizeof(int));

        // std::cout << "\nTEXTURES: " << textureCount << std::endl;

        int sizeOfName[textureCount];
        int prevPosition = 0;

        for (int i = 0; i < textureCount; i++)
        {
            memcpy(&sizeOfName[i], buffer + stringDataOffset + 4 + i * 4, sizeof(int));
            sizeOfName[i] -= prevPosition;

            std::string name(reinterpret_cast<char *>(buffer + stringDataOffset + 4 + 4 * textureCount + prevPosition), sizeOfName[i]);
            // std::cout << name << std::endl;
            prevPosition += sizeOfName[i];
        }

        if (buffer != s_loadBuffer)
            delete buffer;

        return tileTerrain;
    };
};

#include "parserITM.h"

struct tmp_TileTerrain
{
    int tileX;
    int tileZ;
    TileTerrain *tileData;
};

// Class for loading terrain.
// __________________________

class Terrain
{
public:
    Shader shader;
    std::string fileName;
    int fileSize, vertexCount, faceCount;
    unsigned int trnVAO, trnVBO, phyVAO, phyVBO;
    std::vector<float> terrainVertices, physicsVertices;
    std::vector<std::string> sounds;
    std::vector<std::vector<TileTerrain *>> tiles; // 2D grid of terrain tiles stored as pointers (represents all terrain data)
    float minX, minZ, maxX, maxZ;                  // terrain borders in world space coordinates
    int tileMinX, tileMinZ, tileMaxX, tileMaxZ;    // terrain borders in tile numbers (indices)
    int tilesX, tilesZ;                            // terrain size in tiles

    Terrain()
        : shader("shader terrain.vs", "shader terrain.fs"),
          tileMinX(-1),
          tileMinZ(-1),
          tileMaxX(1),
          tileMaxZ(1) {};

    //! Loads .zip file from disk, performs parsing for each contained .trn file, sets up terrain mesh data and sound.
    void load(const char *fpath, Sound &sound)
    {
        reset();

        // 1. process archive with .trn files and store map's tile data in a temporary vector of TileTerrain objects (we cannot just push back a loaded tile to the main 2D vector; at the same time, we cannot resize it, since dimensions of the terrain are yet unknown)
        std::vector<tmp_TileTerrain> tmp_tiles;

        CZipResReader *terrainArchive = new CZipResReader(fpath, true, false);

        CZipResReader *itemsArchive = new CZipResReader(std::string(fpath).replace(std::strlen(fpath) - 4, 4, ".itm").c_str(), true, false);

        CZipResReader *physicsArchive = new CZipResReader("data/terrain/physics.zip", true, false);

        for (int i = 0, n = terrainArchive->getFileCount(); i < n; i++)
        {
            IReadResFile *trnFile = terrainArchive->openFile(i); // open i-th .trn file inside the archive and return memory-read file object with the decompressed content

            if (trnFile)
            {
                int tileX, tileZ;                                                        // variables that will be assigned tile's position on the grid
                TileTerrain *tile = TileTerrain::loadTileTerrain(trnFile, tileX, tileZ); // .trn: load tile's terrain mesh data
                loadTileEntities(itemsArchive, physicsArchive, tileX, tileZ, tile);

                if (tile)
                {
                    tmp_tiles.push_back(tmp_TileTerrain{tileX, tileZ, tile});

                    // update Class variables that track the min and max tile indices (grid borders)
                    if (tileX < tileMinX)
                        tileMinX = tileX;
                    if (tileX > tileMaxX)
                        tileMaxX = tileX;
                    if (tileZ < tileMinZ)
                        tileMinZ = tileZ;
                    if (tileZ > tileMaxZ)
                        tileMaxZ = tileZ;
                }

                trnFile->drop();
            }
        }

        delete terrainArchive;
        delete itemsArchive;
        delete physicsArchive;

        /* 2. initialize Class variables inside the Terrain object
                – terrain borders
                – terrain size
                – map's tile data */

        // terrain borders in world space coordinates
        minX = (float)tileMinX * UnitsInTile;
        minZ = (float)tileMinZ * UnitsInTile;
        maxX = (float)tileMaxX * UnitsInTile;
        maxZ = (float)tileMaxZ * UnitsInTile;

        // 2D array for storing data of all tiles on the terrain
        // (basically 1D temp tmp_tiles vector is converted into a 2D array)
        tilesX = (tileMaxX - tileMinX) + 1; // number of tiles in X direction
        tilesZ = (tileMaxZ - tileMinZ) + 1; // number of tiles in Z direction

        tiles.assign(tilesX, std::vector<TileTerrain *>(tilesZ, NULL)); // resize to terrain dimensions

        for (int i = 0, n = tmp_tiles.size(); i < n; i++)
        {
            int indexX = tmp_tiles[i].tileX - tileMinX; // convert from [-128, 127] range to [0, 255]
            int indexZ = tmp_tiles[i].tileZ - tileMinZ;
            tiles[indexX][indexZ] = tmp_tiles[i].tileData;
        }

        tmp_tiles.clear();

        // 3. build the terrain mesh vertex data in world space coordinates (structured as triangles, which is optimal for rendering terrain surface; 1D vector with the following format: x1, y1, z1, x2, y2, z2, .. )

        // loop through each tile in the terrain
        for (int i = 0; i < tilesX; i++)
        {
            for (int j = 0; j < tilesZ; j++)
            {
                if (!tiles[i][j])
                    continue;

                // loop through each unit in a tile (1 tile = 64 × 64 units, 1 unit -> 4 vertices -> 2 triangles)
                for (int col = 0; col < UnitsInTileCol; col++)
                {
                    for (int row = 0; row < UnitsInTileRow; row++)
                    {
                        // calculate unit's horizontal coordinates in world space based on the tile's origin coordinates and current unit row / col offset (row corresponds to Z-axis and col to X-axis), which forms a rectangle (4 vertices)
                        float x0 = tiles[i][j]->startX + (float)col;
                        float x1 = tiles[i][j]->startX + (float)(col + 1);
                        float z0 = tiles[i][j]->startZ + (float)row;
                        float z1 = tiles[i][j]->startZ + (float)(row + 1);

                        // retrieve heights of the unit's 4 vertices from the tile's height map
                        float y00 = tiles[i][j]->Y[row][col];
                        float y10 = tiles[i][j]->Y[row][col + 1];
                        float y01 = tiles[i][j]->Y[row + 1][col];
                        float y11 = tiles[i][j]->Y[row + 1][col + 1];

                        // first triangle: top-left, bottom-left, bottom-right
                        terrainVertices.push_back(x0);
                        terrainVertices.push_back(y00);
                        terrainVertices.push_back(z0);

                        terrainVertices.push_back(x0);
                        terrainVertices.push_back(y01);
                        terrainVertices.push_back(z1);

                        terrainVertices.push_back(x1);
                        terrainVertices.push_back(y11);
                        terrainVertices.push_back(z1);

                        // second triangle: top-left, bottom-right, top-right
                        terrainVertices.push_back(x0);
                        terrainVertices.push_back(y00);
                        terrainVertices.push_back(z0);

                        terrainVertices.push_back(x1);
                        terrainVertices.push_back(y11);
                        terrainVertices.push_back(z1);

                        terrainVertices.push_back(x1);
                        terrainVertices.push_back(y10);
                        terrainVertices.push_back(z0);
                    }
                }
            }
        }

        // 4. setup buffers
        glGenVertexArrays(1, &trnVAO);
        glGenBuffers(1, &trnVBO);
        glBindVertexArray(trnVAO);
        glBindBuffer(GL_ARRAY_BUFFER, trnVBO);
        glBufferData(GL_ARRAY_BUFFER, terrainVertices.size() * sizeof(float), terrainVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        // ENTITIES
        physicsVertices = getEntitiesVertices();
        std::cout << physicsVertices.size() / 3 << " vertices" << std::endl;

        glGenVertexArrays(1, &phyVAO);
        glGenBuffers(1, &phyVBO);
        glBindVertexArray(phyVAO);
        glBindBuffer(GL_ARRAY_BUFFER, phyVBO);
        glBufferData(GL_ARRAY_BUFFER, physicsVertices.size() * sizeof(float), physicsVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        // set file info to be displayed in the settings panel
        fileName = std::filesystem::path(fpath).filename().string();
        fileSize = std::filesystem::file_size(fpath);
        vertexCount = terrainVertices.size() / 3;
        faceCount = terrainVertices.size() / 9;

        sound.searchSoundFiles(fileName, sounds);

        std::cout << "\nSOUNDS: " << ((sounds.size() != 0) ? sounds.size() : 0) << std::endl;

        for (int i = 0; i < (int)sounds.size(); i++)
            std::cout << "[" << i + 1 << "]  " << sounds[i] << std::endl;

        terrainLoaded = true;
    }

    //! Clears GPU memory and resets viewer state.
    void reset()
    {
        // 1. clear GPU memory and reset viewer state
        if (trnVAO)
        {
            glDeleteVertexArrays(1, &trnVAO);
            trnVAO = 0;
        }

        if (trnVBO)
        {
            glDeleteBuffers(1, &trnVBO);
            trnVBO = 0;
        }

        if (phyVAO)
        {
            glDeleteVertexArrays(1, &phyVAO);
            phyVAO = 0;
        }

        if (phyVBO)
        {
            glDeleteVertexArrays(1, &phyVBO);
            phyVBO = 0;
        }

        if (!tiles.empty())
            for (int x = 0; x < tilesX; ++x)
                for (int z = 0; z < tilesZ; ++z)
                    delete tiles[x][z];

        tiles.clear();

        terrainVertices.clear();
        physicsVertices.clear();
        sounds.clear();
        fileSize = vertexCount = faceCount = 0;
        tileMinX = tileMinZ = -1;
        tileMaxX = tileMaxZ = 1;

        terrainLoaded = false;
    }

    //! Returns a vector of floats containing the physics mesh vertex data in world space coordinates (structured as vertex pairs for line rendering, which matches the wireframe format used in Glitch Engine).
    std::vector<float> getEntitiesVertices()
    {
        std::vector<float> allVertices;

        // loop through each tile in the terrain
        for (int i = 0; i < tilesX; i++)
        {
            for (int j = 0; j < tilesZ; j++)
            {
                if (!tiles[i][j] || tiles[i][j]->physicsGeometry.empty())
                    continue;

                // std::cout << "tile [" << i << "][" << j << "] has " << tiles[i][j]->physicsGeometry.size() << " models" << std::endl;

                // loop through each entity in a tile
                for (int k = 0, geomNum = tiles[i][j]->physicsGeometry.size(); k < geomNum; k++)
                {
                    Physics *geom = tiles[i][j]->physicsGeometry[k];
                    int type = geom->m_geomType;

                    /* handle one of the supported physics geometries:
                        – box
                        – cylinder
                        – custom mesh */

                    if (type == PHYSICS_GEOM_TYPE_BOX)
                    {
                        /* define a cube: 8 vertices and 12 edges

                                v6---------v4
                          top  /|          /|    Y Z
                              / |         / |    |/
                            v0--|-------v2  |    • ── X
                            |  v7-------|--v5
                            | /         | /
                            v1----------v3
                                bottom

                        */

                        VEC3 &h = geom->m_halfSize;
                        VEC3 vertex[8];

                        vertex[0] = VEC3(-h.X, +h.Y, -h.Z);
                        vertex[1] = VEC3(-h.X, -h.Y, -h.Z);
                        vertex[2] = VEC3(+h.X, +h.Y, -h.Z);
                        vertex[3] = VEC3(+h.X, -h.Y, -h.Z);
                        vertex[4] = VEC3(+h.X, +h.Y, +h.Z);
                        vertex[5] = VEC3(+h.X, -h.Y, +h.Z);
                        vertex[6] = VEC3(-h.X, +h.Y, +h.Z);
                        vertex[7] = VEC3(-h.X, -h.Y, +h.Z);

                        int edgePairs[12][2] = {
                            {1, 3},
                            {3, 5},
                            {5, 7},
                            {7, 1},
                            {0, 2},
                            {2, 4},
                            {4, 6},
                            {6, 0},
                            {0, 1},
                            {2, 3},
                            {4, 5},
                            {6, 7},
                        };

                        // loop through each vertex
                        for (int v = 0; v < 8; v++)
                            geom->m_absTransform.transformVect(vertex[v]); // local2world space transformation

                        // loop through each edge
                        for (int e = 0; e < 12; e++)
                        {
                            int a = edgePairs[e][0];
                            int b = edgePairs[e][1];

                            // single cube edge
                            allVertices.push_back(vertex[a].X);
                            allVertices.push_back(vertex[a].Y);
                            allVertices.push_back(vertex[a].Z);
                            allVertices.push_back(vertex[b].X);
                            allVertices.push_back(vertex[b].Y);
                            allVertices.push_back(vertex[b].Z);
                        }
                    }
                    else if (type == PHYSICS_GEOM_TYPE_CYLINDER)
                    {
                        /* define a cylinder: 32 vertices and 48 edges

                            bottom          top
                               • ─────────── •
                             /   \         /   \     • ── Y
                            •     •       •     •    |\
                            │     │       │     │    X Z
                            •     •       •     •
                             \   /         \   /
                               • ─────────── •

                        */

                        int CUT_NUM = 16;                       // number of radial segments; controls smoothness
                        float angle_step = 2.0f * PI / CUT_NUM; // angle increment between cylinder segments (in radians)
                        float radius = geom->m_halfSize.X;
                        float height = geom->m_halfSize.Y;

                        // loop through each segment
                        for (int s = 0; s < CUT_NUM; s++)
                        {
                            // calculate this and next vertices in the ring
                            float angle = s * angle_step;
                            float x = radius * cosf(angle), z = radius * sinf(angle);
                            VEC3 bottom(x, -height, z), top(x, height, z); // points on bottom and top rings
                            geom->m_absTransform.transformVect(bottom);    // local2world space transformation
                            geom->m_absTransform.transformVect(top);

                            float next_angle = (s + 1) * angle_step;
                            float nx = radius * cosf(next_angle), nz = radius * sinf(next_angle);
                            VEC3 next_bottom(nx, -height, nz), next_top(nx, height, nz);
                            geom->m_absTransform.transformVect(next_bottom);
                            geom->m_absTransform.transformVect(next_top);

                            // single bottom ring edge
                            allVertices.push_back(bottom.X);
                            allVertices.push_back(bottom.Y);
                            allVertices.push_back(bottom.Z);
                            allVertices.push_back(next_bottom.X);
                            allVertices.push_back(next_bottom.Y);
                            allVertices.push_back(next_bottom.Z);

                            // single top ring edge
                            allVertices.push_back(top.X);
                            allVertices.push_back(top.Y);
                            allVertices.push_back(top.Z);
                            allVertices.push_back(next_top.X);
                            allVertices.push_back(next_top.Y);
                            allVertices.push_back(next_top.Z);

                            // single vertical side edge
                            allVertices.push_back(bottom.X);
                            allVertices.push_back(bottom.Y);
                            allVertices.push_back(bottom.Z);
                            allVertices.push_back(top.X);
                            allVertices.push_back(top.Y);
                            allVertices.push_back(top.Z);
                        }
                    }
                    else if (type == PHYSICS_GEOM_TYPE_MESH)
                    {
                        PhysicsRefMesh *meshRef = tiles[i][j]->physicsGeometry[k]->m_refMesh; // cast from CPhysicsGeom to CPhysicsMesh class to access mesh reference

                        if (!meshRef)
                            continue;

                        // // --- DEBUG OUTPUT ---
                        // if (meshRef)
                        // {
                        //     if (meshRef)
                        //     {
                        //         std::cout << "\n=== RENDERING DEBUG ===\n";

                        //         std::cout << "--- RefMesh ---\n";
                        //         std::cout << "  m_nbVertex: " << meshRef->m_nbVertex << "\n";
                        //         std::cout << "  m_nbFaces:  " << meshRef->m_nbFaces << "\n";

                        //         std::cout << "  First 5 vertices:\n";
                        //         for (int i = 0; i < std::min(meshRef->m_nbVertex, 5); ++i)
                        //         {
                        //             float *v = meshRef->m_vertex + i * 3;
                        //             std::cout << "    [" << i << "]: ("
                        //                       << v[0] << ", "
                        //                       << v[1] << ", "
                        //                       << v[2] << ")\n";
                        //         }

                        //         std::cout << "===========================\n";
                        //     }
                        // }
                        // else
                        // {
                        //     std::cout << "m_refMesh is null!\n";
                        // }

                        float RENDER_H_OFF = 0.10f;                             // vertical offset for rendering
                        int nbFaces = meshRef->GetNbFace();                     // number of faces
                        const unsigned short *face = meshRef->GetFacePointer(); // face vertex indices (3 vertices per face, forming a triangle)
                        const float *vertex = meshRef->GetVertexPointer();      // vertex data

                        // loop through each face
                        for (int f = 0; f < nbFaces; f++)
                        {
                            int a = face[4 * f]; // 1st vertex index of face (stride = 4, because 3 vertex indices + 1 metadata flag)
                            int b = face[4 * f + 1];
                            int c = face[4 * f + 2];

                            VEC3 v0(vertex[a * 3], vertex[a * 3 + 1] + RENDER_H_OFF, vertex[a * 3 + 2]);
                            VEC3 v1(vertex[b * 3], vertex[b * 3 + 1] + RENDER_H_OFF, vertex[b * 3 + 2]);
                            VEC3 v2(vertex[c * 3], vertex[c * 3 + 1] + RENDER_H_OFF, vertex[c * 3 + 2]);

                            // local2world space transformation
                            tiles[i][j]->physicsGeometry[k]->m_absTransform.transformVect(v0);
                            tiles[i][j]->physicsGeometry[k]->m_absTransform.transformVect(v1);
                            tiles[i][j]->physicsGeometry[k]->m_absTransform.transformVect(v2);

                            // store triangles as 3 edges, because GL_LINE is the rendering mode set for game objects in OpenGL
                            // triangle edge v0->v1
                            allVertices.push_back(v0.X);
                            allVertices.push_back(v0.Y);
                            allVertices.push_back(v0.Z);
                            allVertices.push_back(v1.X);
                            allVertices.push_back(v1.Y);
                            allVertices.push_back(v1.Z);

                            // triangle edge v1->v2
                            allVertices.push_back(v1.X);
                            allVertices.push_back(v1.Y);
                            allVertices.push_back(v1.Z);
                            allVertices.push_back(v2.X);
                            allVertices.push_back(v2.Y);
                            allVertices.push_back(v2.Z);

                            // triangle edge v2->v0
                            allVertices.push_back(v2.X);
                            allVertices.push_back(v2.Y);
                            allVertices.push_back(v2.Z);
                            allVertices.push_back(v0.X);
                            allVertices.push_back(v0.Y);
                            allVertices.push_back(v0.Z);
                        }
                    }
                }
            }
        }

        return allVertices;
    }

    //! Renders terrain and physics geometry meshes.
    void draw(glm::mat4 view, glm::mat4 projection)
    {
        if (terrainLoaded)
        {
            shader.use();
            shader.setMat4("model", glm::mat4(1.0f));
            shader.setMat4("view", view);
            shader.setMat4("projection", projection);

            glBindVertexArray(trnVAO);
            shader.setInt("renderMode", 1);
            glLineWidth(2.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES, 0, terrainVertices.size() / 3);

            shader.setInt("renderMode", 2);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDrawArrays(GL_TRIANGLES, 0, terrainVertices.size() / 3);

            glBindVertexArray(phyVAO);
            shader.setInt("renderMode", 2);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDrawArrays(GL_LINES, 0, physicsVertices.size() / 3);
            glBindVertexArray(0);
        }
    }
};

#endif