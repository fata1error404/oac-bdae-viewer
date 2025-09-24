#include "parserTRN.h"
#include "terrain.h"

//! Processes a single .trn file and returns a newly created TileTerrain object with the tile's mesh data saved.
TileTerrain *TileTerrain::load(IReadResFile *trnFile, int &gridX, int &gridZ, Terrain &terrain)
{
	// 1. load .trn file into memory
	// ____________________

	if (!trnFile)
		return NULL;

	TileTerrain *tileTerrain = new TileTerrain();

	trnFile->seek(0);
	int fileSize = (int)trnFile->getSize();
	unsigned char *buffer = loadBuffer;

	if (fileSize > DEFAULT_LOAD_BUFFER_SIZE)
		buffer = new unsigned char[fileSize];

	trnFile->read(buffer, fileSize);

	// 2. parse .trn file header, retrieve: tile's position on the grid, bounding box
	// ____________________

	TRNFileHeader *header = (TRNFileHeader *)buffer; // interpret the first sizeof(TileFileHeader) bytes in buffer as a corresponding structure
	gridX = header->gridX;							 // write extracted grid position to the parent Terrain class
	gridZ = header->gridZ;

	tileTerrain->startX = (float)gridX * UnitsInTileCol;
	tileTerrain->startZ = (float)gridZ * UnitsInTileCol;
	tileTerrain->BBox.MinEdge = VEC3(tileTerrain->startX, 0, tileTerrain->startZ);
	tileTerrain->BBox.MaxEdge = VEC3(tileTerrain->startX + UnitsInTileRow, 0, tileTerrain->startZ + UnitsInTileCol);

	// 3. parse string data section, retrieve: number of textures, texture file names
	// ____________________

	int textureCount;
	int stringDataOffset = sizeof(TRNFileHeader) + ChunksInTile * sizeof(ChunkInfo) + ((UnitsInTileRow + 1) * (UnitsInTileCol + 1)) * 7 + 1;
	memcpy(&textureCount, buffer + stringDataOffset, sizeof(int));

	int sizeOfName[textureCount], newTexNameIndex[textureCount];
	int prevPosition = 0;

	for (int i = 0; i < textureCount; i++)
	{
		memcpy(&sizeOfName[i], buffer + stringDataOffset + 4 + i * 4, sizeof(int));
		sizeOfName[i] -= prevPosition; // stored size is cumulative (relative to the base), not the actual string length

		std::string textureName(reinterpret_cast<char *>(buffer + stringDataOffset + 4 + 4 * textureCount + prevPosition), sizeOfName[i]);

		// convert to lowercase
		for (char &c : textureName)
			c = std::tolower(c);

		// replace the ending with '.png'
		size_t extensionPos = textureName.find(".tga");
		if (extensionPos != std::string::npos)
			textureName = textureName.substr(0, extensionPos) + ".png";

		// std::cout << textureName << std::endl;

		prevPosition += sizeOfName[i];

		// register texture: check if texture name already exists in the list of unique names (if it doesn't, add it and assign a new index; if it does, reuse the existing index)
		std::vector<std::string>::iterator foundPos = std::find(tileTerrain->textureNames.begin(), tileTerrain->textureNames.end(), textureName);
		if (foundPos == tileTerrain->textureNames.end())
		{
			tileTerrain->textureNames.push_back(textureName);
			newTexNameIndex[i] = (int)tileTerrain->textureNames.size() - 1;
		}
		else
			newTexNameIndex[i] = (int)std::distance(tileTerrain->textureNames.begin(), foundPos);
	}

	// 4. parse chunk data section, retrieve: textures and other metadata about each chunk
	// ____________________

	int chunkOffset = sizeof(TRNFileHeader);

	// loop through each chunk cell in the tile (1 tile = 8 × 8 chunks = 64 × 64 units)
	for (int index = 0, i = 0; i < ChunksInTileRow; i++)
	{
		for (int j = 0; j < ChunksInTileCol; j++, index++)
		{
			ChunkInfo *chunk = (ChunkInfo *)(buffer + chunkOffset); // read the next chunk header
			tileTerrain->chunks[index] = *chunk;					// copy its data into the tile's chunk array
			chunkOffset += sizeof(ChunkInfo);

			if (tileTerrain->chunks[index].texNameIndex1 != -1)
				tileTerrain->chunks[index].texNameIndex1 = newTexNameIndex[tileTerrain->chunks[index].texNameIndex1];
			if (tileTerrain->chunks[index].texNameIndex2 != -1)
				tileTerrain->chunks[index].texNameIndex2 = newTexNameIndex[tileTerrain->chunks[index].texNameIndex2];
			if (tileTerrain->chunks[index].texNameIndex3 != -1)
				tileTerrain->chunks[index].texNameIndex3 = newTexNameIndex[tileTerrain->chunks[index].texNameIndex3];
		}
	}

	// 5. parse height map
	// ____________________

	int minHeight = 50000;
	int maxHeight = -50000;

	// set a pointer to the height data; interpret bytes as an array of short values, accessible via indexings
	short *height = (short *)(buffer + sizeof(TRNFileHeader) + ChunksInTile * sizeof(ChunkInfo));

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

	// 6. parse vertex colors (colors are stored as 32-bit packed RGBA)
	// ____________________

	short *color = (short *)(buffer + sizeof(TRNFileHeader) + ChunksInTile * sizeof(ChunkInfo) + ((UnitsInTileRow + 1) * (UnitsInTileCol + 1)) * 2);

	for (int v = 0, vy = 0; vy <= UnitsInTileRow; vy++)
	{
		for (int vx = 0; vx <= UnitsInTileCol; vx++, v++)
		{
			// unpack 32-bit RGBA value into 8-bit components (bitwise arithmetic)
			uint32_t packed = static_cast<uint32_t>(color[v]);
			uint8_t a = (packed >> 24) & 0xFF;
			uint8_t r = (packed >> 16) & 0xFF;
			uint8_t g = (packed >> 8) & 0xFF;
			uint8_t b = (packed >> 0) & 0xFF;

			tileTerrain->colors[vy][vx] = glm::u8vec4(r, g, b, a);
		}
	}

	// 7. parse vertex normal vectors (normals are stores as unsigned bytes)
	// ____________________

	char *normal = (char *)(buffer + sizeof(TRNFileHeader) + ChunksInTile * sizeof(ChunkInfo) + ((UnitsInTileRow + 1) * (UnitsInTileCol + 1)) * 4);

	for (int v = 0, vy = 0; vy <= UnitsInTileRow; vy++)
	{
		for (int vx = 0; vx <= UnitsInTileCol; vx++, v++)
		{
			glm::vec3 n;
			uint8_t nx = normal[v * 3];
			uint8_t ny = normal[v * 3 + 1];
			uint8_t nz = normal[v * 3 + 2];

			// convert from [0, 255] range to [-1, 1]
			n.x = (nx / 127.5f) - 1.0f;
			n.y = (ny / 127.5f) - 1.0f;
			n.z = (nz / 127.5f) - 1.0f;

			n = glm::normalize(n); // ensure the vector is unit length

			tileTerrain->normals[vy][vx] = n;
		}
	}

	if (buffer != loadBuffer)
		delete[] buffer;

	return tileTerrain;
}
