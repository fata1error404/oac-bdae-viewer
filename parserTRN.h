#ifndef PARSER_TRN_H
#define PARSER_TRN_H

#define UnitsInTile (8 * 8)
#define ChunksInTileRow 8
#define ChunksInTileCol 8
#define UnitsInTileRow 64
#define UnitsInTileCol 64
#define DEFAULT_LOAD_BUFFER_SIZE 102400 // 100 KB

#include "CZipResReader.h"
#include "AABB.h"
#include "VEC3.h"
#include "Quaternion.h"
#include "parserITM.h"

static unsigned char s_loadBuffer[DEFAULT_LOAD_BUFFER_SIZE];

bool terrainLoaded = false;

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
    unsigned int VAO, VBO;
    std::vector<float> vertices;
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

        for (int i = 0, n = terrainArchive->getFileCount(); i < n; i++)
        {
            IReadResFile *trnFile = terrainArchive->openFile(i); // open i-th .trn file inside the archive and return memory-read file object with the decompressed content

            if (trnFile)
            {
                int tileX, tileZ;                                                        // variables that will be assigned tile's position on the grid
                TileTerrain *tile = TileTerrain::loadTileTerrain(trnFile, tileX, tileZ); // .trn: load tile's terrain mesh data
                loadTileEntities(itemsArchive, tileX, tileZ, tile);

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
            }
        }

        delete terrainArchive;
        delete itemsArchive;

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
                        vertices.push_back(x0);
                        vertices.push_back(y00);
                        vertices.push_back(z0);

                        vertices.push_back(x0);
                        vertices.push_back(y01);
                        vertices.push_back(z1);

                        vertices.push_back(x1);
                        vertices.push_back(y11);
                        vertices.push_back(z1);

                        // second triangle: top-left, bottom-right, top-right
                        vertices.push_back(x0);
                        vertices.push_back(y00);
                        vertices.push_back(z0);

                        vertices.push_back(x1);
                        vertices.push_back(y11);
                        vertices.push_back(z1);

                        vertices.push_back(x1);
                        vertices.push_back(y10);
                        vertices.push_back(z0);
                    }
                }
            }
        }

        // 4. setup buffers
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        // set file info to be displayed in the settings panel
        fileName = std::filesystem::path(fpath).filename().string();
        fileSize = std::filesystem::file_size(fpath);
        vertexCount = vertices.size() / 3;
        faceCount = vertices.size() / 9;

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
        if (VAO)
        {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }

        if (VBO)
        {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }

        if (!tiles.empty())
            for (int x = 0; x < tilesX; ++x)
                for (int z = 0; z < tilesZ; ++z)
                    delete tiles[x][z];

        tiles.clear();

        vertices.clear();
        sounds.clear();
        fileSize = vertexCount = faceCount = 0;
        tileMinX = tileMinZ = -1;
        tileMaxX = tileMaxZ = 1;

        terrainLoaded = false;
    }

    //! Renders terrain mesh.
    void draw(glm::mat4 view, glm::mat4 projection)
    {
        if (terrainLoaded)
        {
            shader.use();
            shader.setMat4("model", glm::mat4(1.0f));
            shader.setMat4("view", view);
            shader.setMat4("projection", projection);

            glBindVertexArray(VAO);
            shader.setInt("renderMode", 1);
            glLineWidth(2.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);

            shader.setInt("renderMode", 2);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);
            glBindVertexArray(0);
        }
    }
};

#endif
