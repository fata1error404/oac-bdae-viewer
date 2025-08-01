#include "parserTRN.h"
#include "terrain.h"

//! Processes a single .trn file and returns a newly created TileTerrain object with the tile's mesh data saved.
TileTerrain *TileTerrain::loadTileTerrain(IReadResFile *trnFile, int &gridX, int &gridZ, Terrain &terrain)
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
    gridX = header->gridX;                           // write extracted grid position to the parent Terrain class
    gridZ = header->gridZ;

    tileTerrain->startX = (float)gridX * UnitsInTile;
    tileTerrain->startZ = (float)gridZ * UnitsInTile;
    tileTerrain->BBox.MinEdge = VEC3(tileTerrain->startX, 0, tileTerrain->startZ);
    tileTerrain->BBox.MaxEdge = VEC3(tileTerrain->startX + UnitsInTile, 0, tileTerrain->startZ + UnitsInTile);

    /* 2.3. Parse chunk headers. Retrieve:
            – water level and other metadata about each chunk */

    unsigned int chunkOffset = sizeof(TRNFileHeader);

    /* 2.5. Parse string data section. Retrieve:
            – number of textures
            – texture file names */

    // [TODO] annotate, save as a Class variable
    int textureCount;
    int stringDataOffset = sizeof(TRNFileHeader) + 64 * sizeof(TileChunk) + 7 * ((UnitsInTile + 1) * (UnitsInTile + 1)) + 1;
    memcpy(&textureCount, buffer + stringDataOffset, sizeof(int));

    // std::cout << "\nTEXTURES: " << textureCount << std::endl;

    int sizeOfName[textureCount], realTextureIndex[textureCount];
    int prevPosition = 0;

    for (int i = 0; i < textureCount; i++)
    {
        memcpy(&sizeOfName[i], buffer + stringDataOffset + 4 + i * 4, sizeof(int));
        sizeOfName[i] -= prevPosition;

        std::string name(reinterpret_cast<char *>(buffer + stringDataOffset + 4 + 4 * textureCount + prevPosition), sizeOfName[i]);

        for (char &c : name)
            c = std::tolower(c);

        std::string::size_type pos = 0;
        while ((pos = name.find(".tga", pos)) != std::string::npos)
        {
            name.replace(pos, 4, ".png");
            pos += 4; // move past the replacement
        }

        name = "data/" + name;

        // std::cout << name << std::endl;
        prevPosition += sizeOfName[i];

        auto vit = std::find(terrain.textureNames.begin(), terrain.textureNames.end(), name);
        if (vit == terrain.textureNames.end())
        {
            terrain.textureNames.push_back(name);
            realTextureIndex[i] = (int)terrain.textureNames.size() - 1;
        }
        else
            realTextureIndex[i] = (int)std::distance(terrain.textureNames.begin(), vit);
    }

    // loop through each chunk cell in the tile (1 tile = 8 × 8 chunks = 64 × 64 units)
    for (int index = 0, i = 0; i < ChunksInTileRow; i++)
    {
        for (int j = 0; j < ChunksInTileCol; j++, index++)
        {
            TileChunk *chunk = (TileChunk *)(buffer + chunkOffset); // read the next chunk header
            tileTerrain->chunks[index] = *chunk;                    // copy its data into the tile's chunk array
            chunkOffset += sizeof(TileChunk);

            if (tileTerrain->chunks[index].texNameIndex1 != -1)
                tileTerrain->chunks[index].texNameIndex1 = realTextureIndex[tileTerrain->chunks[index].texNameIndex1];
            if (tileTerrain->chunks[index].texNameIndex2 != -1)
                tileTerrain->chunks[index].texNameIndex2 = realTextureIndex[tileTerrain->chunks[index].texNameIndex2];
            if (tileTerrain->chunks[index].texNameIndex3 != -1)
                tileTerrain->chunks[index].texNameIndex3 = realTextureIndex[tileTerrain->chunks[index].texNameIndex3];

            // std::cout << "index1 " << tileTerrain->chunks[index].texNameIndex1 << std::endl;
            // std::cout << "index2 " << tileTerrain->chunks[index].texNameIndex2 << std::endl;
            // std::cout << "index3 " << tileTerrain->chunks[index].texNameIndex3 << std::endl;
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

    // PARSE VERTEX COLORS
    short *color = (short *)(buffer + chunkOffset + 65 * 65 * 2);

    // loop through each unit in the tile (1 tile = 64 × 64 height values / units)
    for (int v = 0, vy = 0; vy <= UnitsInTileRow; vy++)
    {
        for (int vx = 0; vx <= UnitsInTileCol; vx++, v++)
        {
            uint32_t packed = static_cast<uint32_t>(color[v]);
            uint8_t a = (packed >> 24) & 0xFF;
            uint8_t r = (packed >> 16) & 0xFF;
            uint8_t g = (packed >> 8) & 0xFF;
            uint8_t b = (packed >> 0) & 0xFF;
            tileTerrain->blending[vy][vx] = glm::u8vec4(r, g, b, a);

            // if (vy == 0 && vx == 0)
            // {
            //     std::cout << "Packed value: 0x" << std::hex << packed << std::dec << "\n";
            //     std::cout << "Unpacked RGBA: R=" << static_cast<int>(r)
            //               << ", G=" << static_cast<int>(g)
            //               << ", B=" << static_cast<int>(b)
            //               << ", A=" << static_cast<int>(a) << "\n";
            // }
        }
    }

    // PARSE NORMALS
    char *normalBytes = (char *)(buffer + chunkOffset + 65 * 65 * 4);
    for (int v = 0, vy = 0; vy <= UnitsInTileRow; vy++)
    {
        for (int vx = 0; vx <= UnitsInTileCol; vx++, v++)
        {
            int i = v * 3; // 3 bytes per normal
            uint8_t nx = normalBytes[i];
            uint8_t ny = normalBytes[i + 1];
            uint8_t nz = normalBytes[i + 2];

            glm::vec3 n;
            n.x = (nx / 127.5f) - 1.0f;
            n.y = (ny / 127.5f) - 1.0f;
            n.z = (nz / 127.5f) - 1.0f;
            n = glm::normalize(n);

            tileTerrain->normals[vy][vx] = n;
        }
    }

    if (buffer != s_loadBuffer)
        delete buffer;

    return tileTerrain;
}
