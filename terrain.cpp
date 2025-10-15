#include "terrain.h"
#include <filesystem>
#include "libs/stb_image.h"
#include "libs/glm/glm.hpp"
#include "libs/glm/fwd.hpp"
#include "libs/glm/gtc/type_ptr.hpp"
#include "libs/glm/gtc/constants.hpp"
#include "libs/glm/gtc/packing.hpp"
#include "libs/glm/ext/vector_uint4.hpp"
#include "libs/glm/gtc/type_precision.hpp"
#include "parserTRN.h"
#include "parserITM.h"

//! CPU-side map loading (called once on map startup, pre-loads all tiles for selected map): opens resource archives, calls parsers for each asset type and each map's tile, then builds vertex and index data.
void Terrain::load(const char *fpath, Sound &sound)
{
	reset();

	// open map's resource archives (de-facto ZIP archives but formally named with the same file extension as the assets they contain)
	CZipResReader *terrainArchive = new CZipResReader(fpath, true, false);

	CZipResReader *itemsArchive = new CZipResReader(std::string(fpath).replace(std::strlen(fpath) - 4, 4, ".itm").c_str(), true, false);

	CZipResReader *masksArchive = new CZipResReader(std::string(fpath).replace(std::strlen(fpath) - 4, 4, ".msk").c_str(), true, false);

	CZipResReader *navigationArchive = new CZipResReader(std::string(fpath).replace(std::strlen(fpath) - 4, 4, ".nav").c_str(), true, false);

	CZipResReader *physicsArchive = new CZipResReader("data/terrain/physics.zip", true, false);

	// dtNavMesh *navMesh = new dtNavMesh();

	struct tmp_TileTerrain
	{
		int tileX;
		int tileZ;
		TileTerrain *tileData;
	};

	// we will first store map's tile data in a temporary vector of TileTerrain objects (we cannot just push back a loaded tile to the main 2D vector; at the same time, we cannot resize it, since dimensions of the terrain are yet unknown)
	std::vector<tmp_TileTerrain> tmp_tiles;

	// loop through each tile in the terrain (it is equal to the number of .trn files in the terrain archive)
	for (int i = 0, n = terrainArchive->getFileCount(); i < n; i++)
	{
		IReadResFile *trnFile = terrainArchive->openFile(i); // open i-th .trn file inside the archive and return memory-read file object with the decompressed content

		if (trnFile)
		{
			int tileX, tileZ;														   // variables that will be assigned tile's position on the grid
			TileTerrain *tile = TileTerrain::load(trnFile, tileX, tileZ, *this);	   // .trn: parse tile's terrain surface data
			loadTileEntities(itemsArchive, physicsArchive, tileX, tileZ, tile, *this); // .itm: parse tile's 3D objects info, then parse their model data (.phy and .bdae files)
			loadTileMasks(masksArchive, tileX, tileZ, tile);						   // .msk, .shw: parse tile's mask layers (for terrain surface textures)
			// loadTileNavigation(navigationArchive, navMesh, tileX, tileZ);

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

	if (terrainArchive)
		delete terrainArchive;

	if (itemsArchive)
		delete itemsArchive;

	if (masksArchive)
		delete masksArchive;

	if (navigationArchive)
		delete navigationArchive;

	if (physicsArchive)
		delete physicsArchive;

	/* initialize Class variables inside the Terrain object
		– terrain borders
		– terrain size
		– map's tile data */

	// terrain borders in world space coordinates
	minX = (float)tileMinX * ChunksInTile;
	minZ = (float)tileMinZ * ChunksInTile;
	maxX = (float)tileMaxX * ChunksInTile;
	maxZ = (float)tileMaxZ * ChunksInTile;

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

	// build meshes (vertex and index data) in world space coordinates
	getTerrainVertices();

	getWaterVertices();

	// getPhysicsVertices();

	// getNavigationVertices(navMesh);

	// load skybox and hillbox
	std::string terrainFileName = std::filesystem::path(fpath).filename().string();
	std::string terrainName = terrainFileName.replace(terrainFileName.size() - 4, 4, "");
	std::string skyName = "model/skybox/" + terrainName + "_sky.bdae";
	std::string hillName = "model/skybox/" + terrainName + "_hill.bdae";

	sky.load(skyName.c_str(), sound, true);
	// hill.load(hillName.c_str(), sound, true);

	if (sky.modelLoaded)
	{
		sky.EBOs.resize(sky.totalSubmeshCount);
		glGenVertexArrays(1, &sky.VAO);
		glGenBuffers(1, &sky.VBO);
		glGenBuffers(sky.totalSubmeshCount, sky.EBOs.data());
		glBindVertexArray(sky.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, sky.VBO);
		glBufferData(GL_ARRAY_BUFFER, sky.vertices.size() * sizeof(float), sky.vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		for (int i = 0; i < sky.totalSubmeshCount; i++)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sky.EBOs[i]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sky.indices[i].size() * sizeof(unsigned short), sky.indices[i].data(), GL_STATIC_DRAW);
		}

		glBindVertexArray(0);
	}

	/* [TODO] fix hillbox displayed incorrectly

		if (hill.modelLoaded)
		{
			hill.EBOs.resize(hill.totalSubmeshCount);
			glGenVertexArrays(1, &hill.VAO);
			glGenBuffers(1, &hill.VBO);
			glGenBuffers(hill.totalSubmeshCount, hill.EBOs.data());
			glBindVertexArray(hill.VAO);
			glBindBuffer(GL_ARRAY_BUFFER, hill.VBO);
			glBufferData(GL_ARRAY_BUFFER, hill.vertices.size() * sizeof(float), hill.vertices.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
			glEnableVertexAttribArray(2);

			for (int i = 0; i < hill.totalSubmeshCount; i++)
			{
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hill.EBOs[i]);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, hill.indices[i].size() * sizeof(unsigned short), hill.indices[i].data(), GL_STATIC_DRAW);
			}

			glBindVertexArray(0);
		}
	*/

	// set camera starting point
	if (auto it = terrainSpawnPos.find(terrainName); it != terrainSpawnPos.end())
	{
		auto [pos, pitch, yaw] = it->second;
		camera.Position = pos;
		camera.Pitch = pitch;
		camera.Yaw = yaw;
	}

	camera.updateCameraVectors();

	// set file info to be displayed in the settings panel
	fileName = std::filesystem::path(fpath).filename().string();
	fileSize = std::filesystem::file_size(fpath);
	vertexCount = 0; // [TODO] compute for the entire terrain
	faceCount = 0;

	// search for sounds (ambient music)
	sound.searchSoundFiles(fileName, sounds);

	light.showLighting = true;
	terrainLoaded = true;
}

//! Processes .msk and .shw files for a terrain tile and packs all 3 mask layers in 1 texture where each channel encodes the whole layer (R → primary mask, G → secondary mask, B → pre-rendered shadows).
void Terrain::loadTileMasks(CZipResReader *masksArchive, int gridX, int gridZ, TileTerrain *tile)
{
	if (!masksArchive || !tile)
		return;

	// mask is a per-pixel value that modulates some effect (like texture blending or shadow intensity)
	// mask layer file stores these values as 1 byte per pixel, though each value is in range [0, 255]
	const int MASK_MAP_RESOLUTION = 256;
	const int expectedFileSize = MASK_MAP_RESOLUTION * MASK_MAP_RESOLUTION;

	char tmpName0[256], tmpName1[256], tmpName2[256];
	sprintf(tmpName0, "%04d_%04d_0.msk", gridX, gridZ);
	sprintf(tmpName1, "%04d_%04d_1.msk", gridX, gridZ);
	sprintf(tmpName2, "%04d_%04d.shw", gridX, gridZ);

	unsigned char bufferMask0[expectedFileSize];

	// if mask layer files not exist, these masks remain zeros (no influence)
	unsigned char bufferMask1[expectedFileSize] = {0};
	unsigned char bufferShadow[expectedFileSize] = {0};

	//! Lambda function to read binary content of .msk or .shw file into buffer.
	auto readFileToBuffer = [&](const char *fname, unsigned char *buffer) -> bool
	{
		IReadResFile *mskFile = masksArchive->openFile(fname);

		if (!mskFile)
			return false;

		int realFileSize = (int)mskFile->getSize();

		if (realFileSize != expectedFileSize)
		{
			std::cout << "[Warning] " << fname << " unexpected size: " << realFileSize << " (expected " << expectedFileSize << ")\n";
			mskFile->drop();
			return false;
		}

		mskFile->read(buffer, expectedFileSize);
		mskFile->drop();

		return true;
	};

	// read required primary mask + optional secondary and shadow masks
	if (!readFileToBuffer(tmpName0, bufferMask0))
		return;

	readFileToBuffer(tmpName1, bufferMask1);
	readFileToBuffer(tmpName2, bufferShadow);

	// pack into RGB texture
	unsigned char rgb[expectedFileSize * 3];

	for (int i = 0; i < expectedFileSize; i++)
	{
		rgb[3 * i + 0] = bufferMask0[i];
		rgb[3 * i + 1] = bufferMask1[i];
		rgb[3 * i + 2] = bufferShadow[i];
	}

	glGenTextures(1, &tile->maskTexture);
	glBindTexture(GL_TEXTURE_2D, tile->maskTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, MASK_MAP_RESOLUTION, MASK_MAP_RESOLUTION, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

//! Processes a single .nav file for a terrain tile and adds its data to the Detour navigation system.
void Terrain::loadTileNavigation(CZipResReader *navigationArchive, dtNavMesh *navMesh, int gridX, int gridZ)
{
	if (!navigationArchive)
		return;

	char tmpName[256];
	sprintf(tmpName, "%04d_%04d.nav", gridX, gridZ);

	IReadResFile *navFile = navigationArchive->openFile(tmpName);

	if (!navFile)
		return;

	navFile->seek(0);
	int fileSize = navFile->getSize();
	unsigned char *buffer = new unsigned char[fileSize];
	navFile->read(buffer, fileSize);
	navFile->drop();

	// register in navigation system, granting Detour in-memory ownership (buffer will be freed automatically when the tile is destroyed)
	int tileRef = navMesh->addTile(buffer, fileSize, DT_TILE_FREE_DATA, 0);
}

//! Builds terrain surface vertex data for each square unit and loads textures (terrain is rendered per square unit, however some data is defined per chunk or even per tile, so it must be mapped to square units).
void Terrain::getTerrainVertices()
{
	const int TERRAIN_TEXTURE_RESOLUTION = 256;

	//! Lambda function to allocate a 256 x 256 RGBA image filled with white pixels.
	auto alloc_white_256 = []() -> unsigned char *
	{
		int size = TERRAIN_TEXTURE_RESOLUTION * TERRAIN_TEXTURE_RESOLUTION * 4; // image size in bytes: 256 * 256 pixels * 4 channels
		unsigned char *data = (unsigned char *)malloc(size);

		if (!data)
			return NULL;

		// if allocation succeeded, fill all bytes with 255 (white color, full alpha)
		memset(data, 255, size);

		return data;
	};

	//! Lambda function to resize an RGBA image to 256 x 256 resolution using nearest-neighbor sampling (returns a resized copy).
	auto resize_to_256 = [](unsigned char *src, int srcWidth, int srcHeight) -> unsigned char *
	{
		unsigned char *data = (unsigned char *)malloc(TERRAIN_TEXTURE_RESOLUTION * TERRAIN_TEXTURE_RESOLUTION * 4);

		if (!data)
			return NULL;

		// loop through each pixel in the new image
		for (int destY = 0; destY < TERRAIN_TEXTURE_RESOLUTION; destY++)
		{
			// map new pixel y coordinate to source pixel y coordinate using nearest-neighbor
			int srcY = (destY * srcHeight) / TERRAIN_TEXTURE_RESOLUTION;

			for (int destX = 0; destX < TERRAIN_TEXTURE_RESOLUTION; destX++)
			{
				int srcX = (destX * srcWidth) / TERRAIN_TEXTURE_RESOLUTION;

				// get source and destination pixel pointers
				unsigned char *srcPixel = src + (srcY * srcWidth + srcX) * 4;
				unsigned char *destPixel = data + (destY * TERRAIN_TEXTURE_RESOLUTION + destX) * 4;

				// copy one pixel (R, G, B, A) = 4 bytes from source to destination
				memcpy(destPixel, srcPixel, 4);
			}
		}

		return data;
	};

	// load all terrain's unique surface textures and normalize to 256 x 256 RGBA if needed
	int terrainTextureCount = uniqueTextureNames.size();
	std::vector<unsigned char *> trnTextures(terrainTextureCount, NULL);

	for (int i = 0; i < terrainTextureCount; i++)
	{
		// adjust texture path: fix slashes, insert 'unsorted/' after 'texture/', and prepend 'data/'
		std::string textureName = uniqueTextureNames[i];
		std::replace(textureName.begin(), textureName.end(), '\\', '/');

		auto pos = textureName.find("texture/");

		if (pos != std::string::npos)
			textureName.insert(pos + 8, "unsorted/");

		textureName = "data/" + textureName;

		// load texture as RGBA (4 channels)
		int width = 0, height = 0, nrChannels = 0;
		unsigned char *data = stbi_load(textureName.c_str(), &width, &height, &nrChannels, 4);

		if (!data) // load failed — use 256 x 256 white fallback
		{
			std::cout << "[Warning] Failed to load texture: " << textureName << "\n"
					  << "          Using fallback white 256x256 texture." << std::endl;
			trnTextures[i] = alloc_white_256();
		}
		else if (width != TERRAIN_TEXTURE_RESOLUTION || height != TERRAIN_TEXTURE_RESOLUTION) // load success but resolution mismatch — resize to 256 x 256
		{
			unsigned char *resized = resize_to_256(data, width, height);
			stbi_image_free(data);

			if (resized) // resize success
			{
				std::cout << "[Info] Resized texture " << textureName << " from " << width << "x" << height << " to " << TERRAIN_TEXTURE_RESOLUTION << "x" << TERRAIN_TEXTURE_RESOLUTION << "." << std::endl;
				trnTextures[i] = resized;
			}
			else // resize failed — use 256 x 256 white fallback
			{
				std::cout << "[Warning] Failed to resize texture: " << textureName << std::endl;
				trnTextures[i] = alloc_white_256();
			}
		}
		else // load success (normal case)
			trnTextures[i] = data;
	}

	// loop through each tile in the terrain
	for (int i = 0; i < tilesX; i++)
	{
		for (int j = 0; j < tilesZ; j++)
		{
			TileTerrain *tile = tiles[i][j];

			if (!tile)
				continue;

			// reserve expected capacity to avoid repeated reallocation (6 vertices per square unit, 20 floats per vertex)
			tile->terrainVertices.reserve(UnitsInTileRow * UnitsInTileCol * 6 * 20);

			// build hash table (index in terrain's global texture list → index in tile's local texture array on GPU [0, 1, 2, ..]) to allow O(1) lookup when assigning texture indices to each chunk without scanning tile's texture list every time
			std::unordered_map<int, int> mapGlobalToLocalTexIdx;

			for (int k = 0; k < tile->textureIndices.size(); k++)
				mapGlobalToLocalTexIdx[tile->textureIndices[k]] = k;

			// loop through each square unit in tile
			for (int col = 0; col < UnitsInTileCol; col++)
			{
				for (int row = 0; row < UnitsInTileRow; row++)
				{
					/* 1 square unit (quad = 2 triangles = 6 vertices)

					 (row, col)   (row, col+1)
							•───────•
							│     / │
							│   /   │
							│ /     │
							•───────•
					 (row+1, col)  (row + 1, col + 1) */

					// values per square unit
					float x0 = tile->startX + col;
					float z0 = tile->startZ + row;
					float x1 = x0 + 1.0f;
					float z1 = z0 + 1.0f;

					float y00 = tile->Y[row][col];
					float y10 = tile->Y[row][col + 1];
					float y01 = tile->Y[row + 1][col];
					float y11 = tile->Y[row + 1][col + 1];

					glm::vec3 n00 = tile->normals[row][col];
					glm::vec3 n10 = tile->normals[row][col + 1];
					glm::vec3 n01 = tile->normals[row + 1][col];
					glm::vec3 n11 = tile->normals[row + 1][col + 1];

					// "vertex color" in combination with mask layer texture determine blending weights for 3 main textures in fragment shader
					// convert from [0, 255] to range [0, 1]
					glm::vec4 blend00 = glm::vec4(tile->colors[row][col]) / 255.0f;
					glm::vec4 blend10 = glm::vec4(tile->colors[row][col + 1]) / 255.0f;
					glm::vec4 blend01 = glm::vec4(tile->colors[row + 1][col]) / 255.0f;
					glm::vec4 blend11 = glm::vec4(tile->colors[row + 1][col + 1]) / 255.0f;

					// values per 8 x 8 square units (per chunk)
					float base_u0 = col / 8.0f;
					float base_u1 = (col + 1) / 8.0f;
					float base_v0 = row / 8.0f;
					float base_v1 = (row + 1) / 8.0f;

					ChunkInfo &chunk = tile->chunks[(row / 8) * ChunksInTileCol + col / 8]; // parent chunk that contains this square unit

					float texIdx1 = mapGlobalToLocalTexIdx[chunk.texNameIndex1];
					float texIdx2 = mapGlobalToLocalTexIdx[chunk.texNameIndex2];
					float texIdx3 = mapGlobalToLocalTexIdx[chunk.texNameIndex3];

					// values per 64 x 64 square units (per tile)
					float mask_u0 = col / 64.0f;
					float mask_u1 = (col + 1) / 64.0f;
					float mask_v0 = row / 64.0f;
					float mask_v1 = (row + 1) / 64.0f;

					// 2 triangles forming a terrain surface quad (per square unit)
					// each vertex is 20 floats: position coords (x, y, z), normal vector (nx, ny, nz), main texture coords (u, v), mask texture coords, 3 texture indices, vertex color (r, g, b, a), 3 barycentric coords
					float quad[] = {
						x0, y00, z0, n00.x, n00.y, n00.z, base_u0, base_v0, mask_u0, mask_v0, texIdx1, texIdx2, texIdx3, blend00[0], blend00[1], blend00[2], blend00[3], 1.0f, 0.0f, 0.0f,
						x0, y01, z1, n01.x, n01.y, n01.z, base_u0, base_v1, mask_u0, mask_v1, texIdx1, texIdx2, texIdx3, blend01[0], blend01[1], blend01[2], blend01[3], 0.0f, 1.0f, 0.0f,
						x1, y11, z1, n11.x, n11.y, n11.z, base_u1, base_v1, mask_u1, mask_v1, texIdx1, texIdx2, texIdx3, blend11[0], blend11[1], blend11[2], blend11[3], 0.0f, 0.0f, 1.0f,

						x0, y00, z0, n00.x, n00.y, n00.z, base_u0, base_v0, mask_u0, mask_v0, texIdx1, texIdx2, texIdx3, blend00[0], blend00[1], blend00[2], blend00[3], 1.0f, 0.0f, 0.0f,
						x1, y11, z1, n11.x, n11.y, n11.z, base_u1, base_v1, mask_u1, mask_v1, texIdx1, texIdx2, texIdx3, blend11[0], blend11[1], blend11[2], blend11[3], 0.0f, 0.0f, 1.0f,
						x1, y10, z0, n10.x, n10.y, n10.z, base_u1, base_v0, mask_u1, mask_v0, texIdx1, texIdx2, texIdx3, blend10[0], blend10[1], blend10[2], blend10[3], 0.0f, 1.0f, 0.0f};

					tile->terrainVertices.insert(tile->terrainVertices.end(), std::begin(quad), std::end(quad));
				}
			}

			// upload tile's surface textures into a texture array on GPU — array where each element is a full image of the same size and format (it is more efficient than individual textures as binding several textures per draw call is slow; instead, fragment shader will sample from a single texture array using texture indices defined per chunk (not forget, each chunk has up to 3 textures))
			if (!tile->textureIndices.empty())
			{
				int tileTextureCount = tile->textureIndices.size();

				glGenTextures(1, &tile->textureMap);
				glBindTexture(GL_TEXTURE_2D_ARRAY, tile->textureMap);
				glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, TERRAIN_TEXTURE_RESOLUTION, TERRAIN_TEXTURE_RESOLUTION, tileTextureCount);

				for (int k = 0; k < tileTextureCount; k++)
				{
					int globalIdx = tile->textureIndices[k];
					glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, k, TERRAIN_TEXTURE_RESOLUTION, TERRAIN_TEXTURE_RESOLUTION, 1, GL_RGBA, GL_UNSIGNED_BYTE, trnTextures[globalIdx]);
				}

				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
			}

			if (tile->terrainVertices.empty())
				tile->terrainVertexCount = 0;
			else
				tile->terrainVertexCount = (int)(tile->terrainVertices.size() / 20);
		}
	}

	for (int i = 0, n = trnTextures.size(); i < n; i++)
		if (trnTextures[i])
			stbi_image_free(trnTextures[i]);
}

//! Builds flat water surface vertex data for each terrain chunk that contains water (water is defined and rendered per chunk, not per unit).
void Terrain::getWaterVertices()
{
	const float UnitsInChunk = UnitsInTileRow / ChunksInTileRow; // length of one chunk in world space units along 1 dimension

	// loop through each tile in the terrain
	for (int i = 0; i < tilesX; i++)
	{
		for (int j = 0; j < tilesZ; j++)
		{
			TileTerrain *tile = tiles[i][j];

			if (!tile || !tile->chunks)
				continue;

			// loop through each chunk in a tile
			for (int col = 0; col < ChunksInTileCol; col++)
			{
				for (int row = 0; row < ChunksInTileRow; row++)
				{
					ChunkInfo &chunk = tile->chunks[col * ChunksInTileRow + row];

					// [TODO] differ by liquid type
					// skip chunks without water or with invalid water level
					if (!(chunk.flag & TRNF_HASWATER) || chunk.waterLevel == 0 || chunk.waterLevel == -5000)
						continue;

					// chunk water height in world space coordinates
					float y = chunk.waterLevel * 0.01f;

					// chunk corners in world space coordinates
					float x0 = tile->startX + row * UnitsInChunk;
					float z0 = tile->startZ + col * UnitsInChunk;
					float x1 = x0 + UnitsInChunk;
					float z1 = z0 + UnitsInChunk;

					// texture coordinates (simple mapping)
					float u0 = x0, v0 = z0, u1 = x1, v1 = z1;

					// 2 triangles forming a water surface quad (per chunk square)
					// each vertex is 8 floats: position coords (x, y, z), normal vector (nx, ny, nz), texture coords (u, v)
					float quad[] = {
						x0, y, z0, 0.0f, 1.0f, 0.0f, u0, v0,
						x1, y, z0, 0.0f, 1.0f, 0.0f, u1, v0,
						x1, y, z1, 0.0f, 1.0f, 0.0f, u1, v1,

						x0, y, z0, 0.0f, 1.0f, 0.0f, u0, v0,
						x1, y, z1, 0.0f, 1.0f, 0.0f, u1, v1,
						x0, y, z1, 0.0f, 1.0f, 0.0f, u0, v1};

					tile->water.vertices.insert(tile->water.vertices.end(), std::begin(quad), std::end(quad));
				}
			}

			tile->water.waterVertexCount = tile->water.vertices.size() / 8;
		}
	}
}

/*
	#include <unordered_map>
	#include <string>
	#include <tuple>
	#include <cstdint>
	#include <cmath>
	#include <sstream>

	// helper: quantize a coordinate to integer to avoid tiny floating differences.
	static inline long long quantizeCoord(float v, float scale = 1000.0f)
	{
		return llround(v * scale);
	}

	// make a canonical undirected edge key based on quantized endpoints.
	static inline std::string makeEdgeKeyQ(float ax, float ay, float az, float bx, float by, float bz, float scale = 1000.0f)
	{
		long long a0 = quantizeCoord(ax, scale);
		long long a1 = quantizeCoord(ay, scale);
		long long a2 = quantizeCoord(az, scale);
		long long b0 = quantizeCoord(bx, scale);
		long long b1 = quantizeCoord(by, scale);
		long long b2 = quantizeCoord(bz, scale);

		// lexicographically order the endpoints so key is undirected
		if (std::tie(a0, a1, a2) <= std::tie(b0, b1, b2))
		{
			std::ostringstream ss;
			ss << a0 << ':' << a1 << ':' << a2 << '|' << b0 << ':' << b1 << ':' << b2;
			return ss.str();
		}
		else
		{
			std::ostringstream ss;
			ss << b0 << ':' << b1 << ':' << b2 << '|' << a0 << ':' << a1 << ':' << a2;
			return ss.str();
		}
	}

	struct EdgeInfo
	{
		int count = 0;		   // how many times the undirected edge was seen
		int area = -1;		   // area id of the first polygon that inserted this edge
		bool diffArea = false; // true if an insertion with a different area occurred
		float ax, ay, az;	   // original floating endpoints (first seen)
		float bx, by, bz;
	};

	void Terrain::getNavigationVertices(dtNavMesh *navMesh)
	{
		if (!navMesh)
			return;

		const float navMeshOffset = 1.0f;

		const int maxTiles = navMesh->getMaxTiles();

		for (int ti = 0; ti < maxTiles; ++ti)
		{
			dtMeshTile *dTile = navMesh->getTile(ti);
			if (!dTile || !dTile->header || !dTile->verts || !dTile->polys)
				continue;

			dtMeshHeader *hdr = dTile->header;
			const float *verts = dTile->verts;
			int polyCount = hdr->polyCount;

			std::vector<float> tileNavVerts;
			tileNavVerts.reserve(polyCount * 9 * 2); // rough reserve: 3 verts per tri, 3 floats per vert

			int triVertexCounter = 0;

			// per-tile edge dedup map
			std::unordered_map<std::string, EdgeInfo> edgeMap;
			edgeMap.reserve(512);

			// scan polygons in this detour tile
			for (int p = 0; p < polyCount; ++p)
			{
				const dtPoly &poly = dTile->polys[p];

				if (poly.type == DT_POLYTYPE_OFFMESH_CONNECTION)
					continue;

				int vcount = (int)poly.vertCount;
				if (vcount >= 3)
				{
					int i0 = poly.verts[0];
					for (int k = 2; k < vcount; ++k)
					{
						int i1 = poly.verts[k - 1];
						int i2 = poly.verts[k];

						// vertex a
						tileNavVerts.push_back(verts[3 * i0 + 0]);
						tileNavVerts.push_back(verts[3 * i0 + 1] + navMeshOffset);
						tileNavVerts.push_back(verts[3 * i0 + 2]);

						// vertex b
						tileNavVerts.push_back(verts[3 * i1 + 0]);
						tileNavVerts.push_back(verts[3 * i1 + 1] + navMeshOffset);
						tileNavVerts.push_back(verts[3 * i1 + 2]);

						// vertex c
						tileNavVerts.push_back(verts[3 * i2 + 0]);
						tileNavVerts.push_back(verts[3 * i2 + 1] + navMeshOffset);
						tileNavVerts.push_back(verts[3 * i2 + 2]);

						triVertexCounter += 3;
					}
				}

				// collect edges for this tile
				if (vcount >= 2)
				{
					int area = (int)poly.area;
					for (int e = 0; e < vcount; ++e)
					{
						int ia = poly.verts[e];
						int ib = poly.verts[(e + 1) % vcount];

						float ax = verts[3 * ia + 0];
						float ay = verts[3 * ia + 1];
						float az = verts[3 * ia + 2];

						float bx = verts[3 * ib + 0];
						float by = verts[3 * ib + 1];
						float bz = verts[3 * ib + 2];

						std::string key = makeEdgeKeyQ(ax, ay, az, bx, by, bz);

						auto it = edgeMap.find(key);
						if (it == edgeMap.end())
						{
							EdgeInfo info;
							info.count = 1;
							info.area = area;
							info.diffArea = false;
							info.ax = ax;
							info.ay = ay;
							info.az = az;
							info.bx = bx;
							info.by = by;
							info.bz = bz;
							edgeMap.emplace(std::move(key), std::move(info));
						}
						else
						{
							EdgeInfo &info = it->second;
							info.count += 1;
							if (info.area != area)
								info.diffArea = true;
						}
					}
				}
			}

			// Append boundary edges (lines) to tileNavVerts
			for (auto &kv : edgeMap)
			{
				const EdgeInfo &info = kv.second;
				if (info.count == 1 || info.diffArea)
				{
					tileNavVerts.push_back(info.ax);
					tileNavVerts.push_back(info.ay + navMeshOffset);
					tileNavVerts.push_back(info.az);

					tileNavVerts.push_back(info.bx);
					tileNavVerts.push_back(info.by + navMeshOffset);
					tileNavVerts.push_back(info.bz);
				}
			}

			// locate matching TileTerrain* using hdr->x, hdr->y
			int tileX = hdr->x;
			int tileY = hdr->y;
			int idxX = tileX - tileMinX;
			int idxZ = tileY - tileMinZ;

			if (idxX >= 0 && idxX < tilesX && idxZ >= 0 && idxZ < tilesZ)
			{
				TileTerrain *t = tiles[idxX][idxZ];
				if (t)
				{
					// move new vertices into the tile
					t->navigationVertices = std::move(tileNavVerts);
					t->navmeshVertexCount = triVertexCounter;
				}
			}
		}

		if (navMesh)
		{
			delete navMesh;
			navMesh = NULL;
		}
	}
*/

/*
	void Terrain::getPhysicsVertices()
	{
		for (int i = 0; i < tilesX; i++)
		{
			for (int j = 0; j < tilesZ; j++)
			{
				TileTerrain *tile = tiles[i][j];

				if (!tile || tile->physicsGeometry.empty())
					continue;

				for (Physics *headGeom : tile->physicsGeometry)
				{
					for (Physics *geom = headGeom; geom; geom = geom->pNext)
					{
						int type = geom->geometryType;

						if (type == PHYSICS_GEOM_TYPE_BOX)
						{
							VEC3 &h = geom->halfSize;
							VEC3 v[8] = {
								{-h.X, +h.Y, -h.Z}, {+h.X, +h.Y, -h.Z}, {+h.X, -h.Y, -h.Z}, {-h.X, -h.Y, -h.Z}, {-h.X, +h.Y, +h.Z}, {+h.X, +h.Y, +h.Z}, {+h.X, -h.Y, +h.Z}, {-h.X, -h.Y, +h.Z}};
							for (auto &vv : v)
								geom->model.transformVect(vv);
							int F[6][4] = {
								{0, 1, 2, 3}, {5, 4, 7, 6}, {0, 3, 7, 4}, {1, 5, 6, 2}, {0, 4, 5, 1}, {3, 2, 6, 7}};
							for (int f = 0; f < 6; ++f)
							{
								int a = F[f][0], b = F[f][1], c = F[f][2], d = F[f][3];
								tile->physicsVertices.insert(tile->physicsVertices.end(), {v[a].X, v[a].Y, v[a].Z, v[b].X, v[b].Y, v[b].Z, v[c].X, v[c].Y, v[c].Z,
																						v[a].X, v[a].Y, v[a].Z, v[c].X, v[c].Y, v[c].Z, v[d].X, v[d].Y, v[d].Z});
							}
						}
						else if (type == PHYSICS_GEOM_TYPE_CYLINDER)
						{
							const int CUT_NUM = 16;
							const float pi = 3.14159265359f;
							float angle_step = 2.0f * pi / CUT_NUM;
							float radius = geom->halfSize.X;
							float height = geom->halfSize.Y;

							int myoffset = 0.5 * radius;

							VEC3 centerBottom(myoffset, -height, -myoffset);
							VEC3 centerTop(myoffset, height, -myoffset);
							geom->model.transformVect(centerBottom);
							geom->model.transformVect(centerTop);

							for (int s = 0; s < CUT_NUM; s++)
							{
								float angle0 = s * angle_step;
								float angle1 = (s + 1) * angle_step;

								float x0 = radius * cosf(angle0) + myoffset, z0 = radius * sinf(angle0) - myoffset;
								float x1 = radius * cosf(angle1) + myoffset, z1 = radius * sinf(angle1) - myoffset;

								VEC3 b0(x0, -height, z0);
								VEC3 b1(x1, -height, z1);
								VEC3 t0(x0, +height, z0);
								VEC3 t1(x1, +height, z1);

								geom->model.transformVect(b0);
								geom->model.transformVect(b1);
								geom->model.transformVect(t0);
								geom->model.transformVect(t1);

								tile->physicsVertices.insert(tile->physicsVertices.end(), {b1.X, b1.Y, b1.Z,
																						b0.X, b0.Y, b0.Z,
																						centerBottom.X, centerBottom.Y, centerBottom.Z});

								tile->physicsVertices.insert(tile->physicsVertices.end(), {t0.X, t0.Y, t0.Z,
																						t1.X, t1.Y, t1.Z,
																						centerTop.X, centerTop.Y, centerTop.Z});

								tile->physicsVertices.insert(tile->physicsVertices.end(), {b0.X, b0.Y, b0.Z,
																						t0.X, t0.Y, t0.Z,
																						t1.X, t1.Y, t1.Z});

								tile->physicsVertices.insert(tile->physicsVertices.end(), {b0.X, b0.Y, b0.Z,
																						t1.X, t1.Y, t1.Z,
																						b1.X, b1.Y, b1.Z});
							}
						}
						else if (type == PHYSICS_GEOM_TYPE_MESH)
						{
							const auto *facePtr = geom->mesh ? &geom->mesh->second : nullptr;
							const auto *vertPtr = geom->mesh ? &geom->mesh->first : nullptr;

							if (!facePtr || !vertPtr || facePtr->empty() || vertPtr->empty())
								continue;

							const float RENDER_H_OFF = 0.10f;
							int F = static_cast<int>(facePtr->size() / PHYSICS_FACE_SIZE);
							const auto &face = *facePtr;
							const auto &vert = *vertPtr;

							for (int f = 0; f < F; ++f)
							{
								int a = face[4 * f];
								int b = face[4 * f + 1];
								int c = face[4 * f + 2];

								// guard against bad indices
								if ((3 * a + 2) >= (int)vert.size() || (3 * b + 2) >= (int)vert.size() || (3 * c + 2) >= (int)vert.size())
									continue;

								VEC3 v0(vert[3 * a], vert[3 * a + 1] + RENDER_H_OFF, -vert[3 * a + 2]);
								VEC3 v1(vert[3 * b], vert[3 * b + 1] + RENDER_H_OFF, -vert[3 * b + 2]);
								VEC3 v2(vert[3 * c], vert[3 * c + 1] + RENDER_H_OFF, -vert[3 * c + 2]);

								geom->model.transformVect(v0);
								geom->model.transformVect(v1);
								geom->model.transformVect(v2);

								tile->physicsVertices.insert(tile->physicsVertices.end(), {v0.X, v0.Y, v0.Z,
																						v2.X, v2.Y, v2.Z,
																						v1.X, v1.Y, v1.Z});
							}
						}
					}
				}
				tile->physicsVertexCount = tile->physicsVertices.size() / 3;
			}
		}

		for (int i = 0; i < tilesX; i++)
			for (int j = 0; j < tilesZ; j++)
			{
				if (!tiles[i][j] || tiles[i][j]->physicsGeometry.empty())
					continue;

				TileTerrain *tile = tiles[i][j];

				for (Physics *headGeom : tile->physicsGeometry)
				{
					for (Physics *geom = headGeom; geom; geom = geom->pNext)
					{
						int type = geom->geometryType;
						if (type == PHYSICS_GEOM_TYPE_BOX)
						{
							VEC3 &h = geom->halfSize;
							VEC3 v[8] = {
								{-h.X, +h.Y, -h.Z}, {+h.X, +h.Y, -h.Z}, {+h.X, -h.Y, -h.Z}, {-h.X, -h.Y, -h.Z}, {-h.X, +h.Y, +h.Z}, {+h.X, +h.Y, +h.Z}, {+h.X, -h.Y, +h.Z}, {-h.X, -h.Y, +h.Z}};
							for (auto &vv : v)
								geom->model.transformVect(vv);
							int E[12][2] = {
								{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
							for (auto &e : E)
							{
								tile->physicsVertices.insert(tile->physicsVertices.end(), {v[e[0]].X, v[e[0]].Y, v[e[0]].Z,
																						v[e[1]].X, v[e[1]].Y, v[e[1]].Z});
							}
						}
						else if (type == PHYSICS_GEOM_TYPE_CYLINDER)
						{
							const int CUT_NUM = 16;
							const float pi = 3.14159265359f;
							float angle_step = 2.0f * pi / CUT_NUM;
							float radius = geom->halfSize.X;
							float height = geom->halfSize.Y;

							int myoffset = 0.5 * radius;

							VEC3 centerBottom(myoffset, -height, -myoffset);
							VEC3 centerTop(myoffset, height, -myoffset);
							geom->model.transformVect(centerBottom);
							geom->model.transformVect(centerTop);

							for (int s = 0; s < CUT_NUM; s++)
							{
								float angle0 = s * angle_step;
								float angle1 = (s + 1) * angle_step;

								float x0 = radius * cosf(angle0) + myoffset, z0 = radius * sinf(angle0) - myoffset;
								float x1 = radius * cosf(angle1) + myoffset, z1 = radius * sinf(angle1) - myoffset;

								VEC3 b0(x0, -height, z0);
								VEC3 t0(x0, height, z0);
								VEC3 b1(x1, -height, z1);
								VEC3 t1(x1, height, z1);

								geom->model.transformVect(b0);
								geom->model.transformVect(t0);
								geom->model.transformVect(b1);
								geom->model.transformVect(t1);

								tile->physicsVertices.insert(tile->physicsVertices.end(), {b1.X, b1.Y, b1.Z,
																						b0.X, b0.Y, b0.Z,
																						centerBottom.X, centerBottom.Y, centerBottom.Z});

								tile->physicsVertices.insert(tile->physicsVertices.end(), {b0.X, b0.Y, b0.Z,
																						t1.X, t1.Y, t1.Z,
																						b1.X, b1.Y, b1.Z});

								tile->physicsVertices.insert(tile->physicsVertices.end(), {b0.X, b0.Y, b0.Z,
																						t0.X, t0.Y, t0.Z,
																						t1.X, t1.Y, t1.Z});

								tile->physicsVertices.insert(tile->physicsVertices.end(), {t0.X, t0.Y, t0.Z,
																						t1.X, t1.Y, t1.Z,
																						centerTop.X, centerTop.Y, centerTop.Z});
							}
						}
						else if (type == PHYSICS_GEOM_TYPE_MESH)
						{
							const auto *facePtr = geom->mesh ? &geom->mesh->second : nullptr;
							const auto *vertPtr = geom->mesh ? &geom->mesh->first : nullptr;

							if (!facePtr || !vertPtr || facePtr->empty() || vertPtr->empty())
								continue;

							const float RENDER_H_OFF = 0.10f;
							int F = static_cast<int>(facePtr->size() / PHYSICS_FACE_SIZE);
							const auto &face = *facePtr;
							const auto &vert = *vertPtr;

							for (int f = 0; f < F; ++f)
							{
								int a = face[4 * f];
								int b = face[4 * f + 1];
								int c = face[4 * f + 2];

								if ((3 * a + 2) >= (int)vert.size() || (3 * b + 2) >= (int)vert.size() || (3 * c + 2) >= (int)vert.size())
									continue;

								VEC3 v0(vert[3 * a], vert[3 * a + 1] + RENDER_H_OFF, -vert[3 * a + 2]);
								VEC3 v1(vert[3 * b], vert[3 * b + 1] + RENDER_H_OFF, -vert[3 * b + 2]);
								VEC3 v2(vert[3 * c], vert[3 * c + 1] + RENDER_H_OFF, -vert[3 * c + 2]);

								geom->model.transformVect(v0);
								geom->model.transformVect(v1);
								geom->model.transformVect(v2);

								tile->physicsVertices.insert(tile->physicsVertices.end(), {v0.X, v0.Y, v0.Z, v2.X, v2.Y, v2.Z,
																						v2.X, v2.Y, v2.Z, v1.X, v1.Y, v1.Z,
																						v1.X, v1.Y, v1.Z, v0.X, v0.Y, v0.Z});
							}
						}
					}
				}

				for (Physics *p : tile->physicsGeometry)
					delete p;

				tile->physicsGeometry.clear();
			}
	}
*/

//! Uploads tile to GPU (GPU-side tile loading, called per-frame for all tiles that need to be activated).
void Terrain::activateTile(TileTerrain *tile)
{
	if (!tile)
		return;

	if (!tile->terrainVertices.empty())
	{
		glGenVertexArrays(1, &tile->trnVAO);
		glGenBuffers(1, &tile->trnVBO);
		glBindVertexArray(tile->trnVAO);
		glBindBuffer(GL_ARRAY_BUFFER, tile->trnVBO);
		glBufferData(GL_ARRAY_BUFFER, tile->terrainVertices.size() * sizeof(float), tile->terrainVertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 20 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 20 * sizeof(float), (void *)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 20 * sizeof(float), (void *)(8 * sizeof(float)));
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 20 * sizeof(float), (void *)(10 * sizeof(float)));
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 20 * sizeof(float), (void *)(13 * sizeof(float)));
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, 20 * sizeof(float), (void *)(17 * sizeof(float)));
		glEnableVertexAttribArray(6);
		glBindVertexArray(0);
	}

	if (!tile->water.vertices.empty())
	{
		glGenVertexArrays(1, &tile->water.VAO);
		glGenBuffers(1, &tile->water.VBO);
		glBindVertexArray(tile->water.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, tile->water.VBO);
		glBufferData(GL_ARRAY_BUFFER, tile->water.vertices.size() * sizeof(float), tile->water.vertices.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
		glBindVertexArray(0);
	}

	if (!tile->physicsVertices.empty())
	{
		glGenVertexArrays(1, &tile->phyVAO);
		glGenBuffers(1, &tile->phyVBO);
		glBindVertexArray(tile->phyVAO);
		glBindBuffer(GL_ARRAY_BUFFER, tile->phyVBO);
		glBufferData(GL_ARRAY_BUFFER, tile->physicsVertices.size() * sizeof(float), tile->physicsVertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
	}

	if (!tile->navigationVertices.empty())
	{
		glGenVertexArrays(1, &tile->navVAO);
		glGenBuffers(1, &tile->navVBO);
		glBindVertexArray(tile->navVAO);
		glBindBuffer(GL_ARRAY_BUFFER, tile->navVBO);
		glBufferData(GL_ARRAY_BUFFER, tile->navigationVertices.size() * sizeof(float), tile->navigationVertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
	}

	for (auto &m : tile->models)
	{
		std::shared_ptr<Model> model = m.first;

		if (model && model->modelLoaded)
		{
			model->EBOs.resize(model->totalSubmeshCount);
			glGenVertexArrays(1, &model->VAO);
			glGenBuffers(1, &model->VBO);
			glGenBuffers(model->totalSubmeshCount, model->EBOs.data());
			glBindVertexArray(model->VAO);
			glBindBuffer(GL_ARRAY_BUFFER, model->VBO);
			glBufferData(GL_ARRAY_BUFFER, model->vertices.size() * sizeof(float), model->vertices.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
			glEnableVertexAttribArray(2);

			for (int i = 0; i < model->totalSubmeshCount; i++)
			{
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->EBOs[i]);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, model->indices[i].size() * sizeof(unsigned short), model->indices[i].data(), GL_STATIC_DRAW);
			}

			glBindVertexArray(0);
		}
	}

	tile->activated = true;
}

//! Releases tile from GPU.
void Terrain::deactivateTile(TileTerrain *tile)
{
	if (!tile)
		return;

	if (tile->trnVAO)
	{
		glDeleteVertexArrays(1, &tile->trnVAO);
		tile->trnVAO = 0;
	}

	if (tile->trnVBO)
	{
		glDeleteBuffers(1, &tile->trnVBO);
		tile->trnVBO = 0;
	}

	if (tile->water.VAO)
	{
		glDeleteVertexArrays(1, &tile->water.VAO);
		tile->water.VAO = 0;
	}

	if (tile->water.VBO)
	{
		glDeleteBuffers(1, &tile->water.VBO);
		tile->water.VBO = 0;
	}

	if (tile->phyVAO)
	{
		glDeleteVertexArrays(1, &tile->phyVAO);
		tile->phyVAO = 0;
	}

	if (tile->phyVBO)
	{
		glDeleteBuffers(1, &tile->phyVBO);
		tile->phyVBO = 0;
	}

	if (tile->navVAO)
	{
		glDeleteVertexArrays(1, &tile->navVAO);
		tile->navVAO = 0;
	}

	if (tile->navVBO)
	{
		glDeleteBuffers(1, &tile->navVBO);
		tile->navVBO = 0;
	}

	for (auto &m : tile->models)
	{
		std::shared_ptr<Model> model = m.first;

		if (model && model->modelLoaded)
		{
			if (!model->EBOs.empty())
			{
				glDeleteBuffers(model->EBOs.size(), model->EBOs.data());
				model->EBOs.clear();
			}

			if (model->VBO)
			{
				glDeleteBuffers(1, &model->VBO);
				model->VBO = 0;
			}

			if (model->VAO)
			{
				glDeleteVertexArrays(1, &model->VAO);
				model->VAO = 0;
			}
		}
	}

	tile->activated = false;
}

//! Computes which tiles will be rendered in the current frame based on camera position and orientation (distance-based culling + frustum culling).
void Terrain::updateVisibleTiles(glm::mat4 view, glm::mat4 projection)
{
	// rebuild the list of visible tiles from scratch each frame
	tilesVisible.clear();

	if (!terrainLoaded || tilesX == 0 || tilesZ == 0)
		return;

	// first pass: distance-based culling (GPU activation / deactivation logic executed on every tile)

	// loop through each tile in the terrain
	for (int i = 0; i < tilesX; i++)
	{
		for (int j = 0; j < tilesZ; j++)
		{
			TileTerrain *tile = tiles[i][j];

			if (!tile)
				continue;

			// compute distance from camera to tile's center in world space coordinates
			float centerX = tile->startX + 0.5f * UnitsInTileRow;
			float centerZ = tile->startZ + 0.5f * UnitsInTileCol;

			float dx = camera.Position.x - centerX;
			float dz = camera.Position.z - centerZ;
			float distSq = dx * dx + dz * dz;

			/*
				Scenarios:
				  - tile is inside load radius and not activated → upload to GPU (activate)
				  - tile is inside load radius and activated → no action
				  - tile is outside of unload radius and activated → release from GPU (deactivate)
				  - tile is outside of unload radius and not activated → no action
				  - tile is between load and unload radius, whether it's active or not → no action; this is a buffer (or "hysteresis") zone that prevents visual lag – tiles at the boundary may constantly toggle on and off every frame as the camera moves a little (this behaviour is called "thrashing")

				Camera
					|
					|<-- load radius --[tile gets loaded]
					|
					|<-- unload radius --[tile stays loaded until beyond this] */

			if (distSq <= loadRadiusSq && !tile->activated)
				activateTile(tile);
			else if (distSq > unloadRadiusSq && tile->activated)
				deactivateTile(tile);
		}
	}

	// second pass: frustum culling (even though many tiles may be active in GPU memory, only tiles inside camera's view frustum are added to the render list)

	glm::mat4 clip = projection * view; // clip matrix that encodes camera’s view volume in world space; combining its rows allows to compute planes defining the visible frustum for culling (see below)

	// -w <= x <= w  →  left / right planes
	// -w <= y <= w  →  bottom / top planes
	// -w <= z <= w  →  near / far planes

	// --> each plane is defined as 0 = w ± x / y / z = row3 ± row0 / row1 / row2 (in clip matrix)

	//! Lambda function to construct a frustum plane equation for the current camera position and orientation.
	auto extractPlane = [&](int row, bool subtract) -> glm::vec4
	{
		float nx, ny, nz, d;

		// 'subtract' selects negative (false = left / bottom / near) or positive (true = right / top / far) plane
		// 'row' selects x / y / z axis (0 = x, 1 = y, 2 = z)
		if (!subtract)
		{
			nx = clip[0][3] + clip[0][row];
			ny = clip[1][3] + clip[1][row];
			nz = clip[2][3] + clip[2][row];
			d = clip[3][3] + clip[3][row];
		}
		else
		{
			nx = clip[0][3] - clip[0][row];
			ny = clip[1][3] - clip[1][row];
			nz = clip[2][3] - clip[2][row];
			d = clip[3][3] - clip[3][row];
		}

		glm::vec3 n(nx, ny, nz);

		// normalize plane equation so that |n| = 1
		// n' = n / |n|, d' = d / |n|
		float len = glm::length(n);

		if (len > 0.0f) // safety check to avoid division by zero
		{
			n /= len;
			d /= len;
		}

		// return plane as vec4 = (nx, ny, nz, d): it represents plane equation: (n, p) + d = 0 in world space, where
		// p – any point that lies on the plane
		// n – plane normal vector
		// d – plane distance from origin
		return glm::vec4(n, d);
	};

	//! Lambda function to test if tile's AABB (axis-aligned bounding box) lies completely outside a frustum plane).
	auto isOutsidePlane = [](const glm::vec4 &plane, const glm::vec3 &aabbMin, const glm::vec3 &aabbMax) -> bool
	{
		glm::vec3 pos;

		// choose the AABB corner vertex that is farthest in the direction of plane normal vector – this vertex is most likely to be outside
		pos.x = (plane.x >= 0.0f) ? aabbMax.x : aabbMin.x;
		pos.y = (plane.y >= 0.0f) ? aabbMax.y : aabbMin.y;
		pos.z = (plane.z >= 0.0f) ? aabbMax.z : aabbMin.z;

		// (n, p) + d = 0  →  point p lies on the plane
		// (n, p) + d > 0  →  point is in front of the plane (inside the frustum)
		// (n, p) + d < 0  →  point is behind the plane (outside the frustum)

		float dist = glm::dot(glm::vec3(plane), pos) + plane.w;

		// if this most "extreme" AABB corner is outside a frustum plane, then the entire box is considered outside
		return dist < 0.0f;
	};

	// build view frustum planes
	glm::vec4 planes[6];

	planes[0] = extractPlane(0, false); // left (w + x)
	planes[1] = extractPlane(0, true);	// right (w - x)
	planes[2] = extractPlane(1, false); // bottom
	planes[3] = extractPlane(1, true);	// top
	planes[4] = extractPlane(2, false); // near
	planes[5] = extractPlane(2, true);	// far

	// compute which tile the camera is currently above (grid position)
	int cameraTileX = (int)std::floor(camera.Position.x / UnitsInTileRow); // e.g. ⌊170 / 64⌋ = ⌊2.66⌋ = 2
	int cameraTileZ = (int)std::floor(camera.Position.z / UnitsInTileRow);

	cameraTileX = std::clamp(cameraTileX - tileMinX, 0, tilesX - 1); // convert to range [0, tilesX - 1]
	cameraTileZ = std::clamp(cameraTileZ - tileMinZ, 0, tilesZ - 1);

	// compute a tight square of tiles around the camera to reduce the number of tiles tested during frustum culling (apply distance-based culling)
	int x0 = std::max(0, cameraTileX - visibleRadiusTiles);
	int x1 = std::min(tilesX - 1, cameraTileX + visibleRadiusTiles);
	int z0 = std::max(0, cameraTileZ - visibleRadiusTiles);
	int z1 = std::min(tilesZ - 1, cameraTileZ + visibleRadiusTiles);

	tilesVisible.reserve((x1 - x0 + 1) * (z1 - z0 + 1));

	// loop through each tile within "pre-visible" range
	for (int i = x0; i <= x1; i++)
	{
		for (int j = z0; j <= z1; j++)
		{
			TileTerrain *tile = tiles[i][j];

			if (!tile)
				continue;

			glm::vec3 tileBBoxMin(tile->BBox.MinEdge.X, tile->BBox.MinEdge.Y, tile->BBox.MinEdge.Z);
			glm::vec3 tileBBoxMax(tile->BBox.MaxEdge.X, tile->BBox.MaxEdge.Y, tile->BBox.MaxEdge.Z);

			bool culled = false;

			// test tile against all 6 frustum planes
			for (int k = 0; k < 6; k++)
			{
				// if tile's bounding box is completely outside any plane, it can be culled (not rendered in the current frame)
				if (isOutsidePlane(planes[k], tileBBoxMin, tileBBoxMax))
				{
					culled = true;
					break; // no need to test the remaining planes
				}
			}

			// if tile is inside camera's view frustum (surrounded by all 6 frustum planes), add it to the visible list
			if (!culled)
				tilesVisible.push_back(tile);
		}
	}
}

//! Clears CPU memory (resets viewer state).
void Terrain::reset()
{
	terrainLoaded = false;

	tileMinX = tileMinZ = 1000;
	tileMaxX = tileMaxZ = -1000;
	tilesX = tilesZ = 0;
	fileSize = vertexCount = faceCount = modelCount = 0;

	for (auto &column : tiles)
		for (TileTerrain *tile : column)
			delete tile;

	sky.reset();
	hill.reset();

	tiles.clear();
	tilesVisible.clear();
	sounds.clear();

	bdaeModelCache.clear();
	physicsModelCache.clear();
	uniqueTextureNames.clear();
}

//! Renders terrain (.trn + .phy + .nav + .bdae).
void Terrain::draw(glm::mat4 view, glm::mat4 projection, bool simple, bool renderNavMesh, bool renderPhysics, float dt)
{
	if (!terrainLoaded)
		return;

	shader.use();
	shader.setMat4("model", glm::mat4(1.0f));
	shader.setMat4("view", view);
	shader.setMat4("projection", projection);
	shader.setBool("lighting", light.showLighting);
	shader.setVec3("lightPos", glm::vec3(camera.Position.x, camera.Position.y + 600.0f, camera.Position.z));
	shader.setVec3("cameraPos", camera.Position);

	// activate / deactivate tiles based on camera position and view
	updateVisibleTiles(view, projection);

	// render terrain
	if (!simple)
		shader.setInt("renderMode", 1);
	else
		shader.setInt("renderMode", 3);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	for (TileTerrain *tile : tilesVisible)
	{
		if (!tile)
			continue;

		// [TODO] fix minor valgrind runtime errors
		if (tile->trnVAO == 0 || tile->trnVBO == 0 || tile->terrainVertexCount == 0 || !tile->textureMap)
			continue;

		glBindVertexArray(tile->trnVAO);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, tile->textureMap);

		if (tile->maskTexture)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, tile->maskTexture);
		}

		glDrawArrays(GL_TRIANGLES, 0, tile->terrainVertexCount);
		glBindVertexArray(0);
	}

	/*
		// render walkable surfaces
		if (renderNavMesh)
		{
			shader.setInt("renderMode", 5);

			for (TileTerrain *tile : tilesVisible)
			{
				if (!tile)
					continue;

				if (tile->navVAO == 0 || tile->navVBO == 0)
					continue;

				glBindVertexArray(tile->navVAO);

				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				glDrawArrays(GL_TRIANGLES, 0, tile->navmeshVertexCount);

				glBindVertexArray(0);
			}
		}

		// render physics
		if (renderPhysics)
		{
			for (TileTerrain *tile : tilesVisible)
			{
				if (!tile)
					continue;

				if (tile->phyVAO == 0 || tile->phyVBO == 0)
					continue;

				glBindVertexArray(tile->phyVAO);

				shader.setInt("renderMode", 4);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				glDrawArrays(GL_TRIANGLES, 0, tile->physicsVertexCount);

				shader.setInt("renderMode", 2);
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				glDrawArrays(GL_TRIANGLES, 0, tile->physicsVertexCount);

				glBindVertexArray(0);
			}
		}
	*/

	// render water and 3D models
	for (TileTerrain *tile : tilesVisible)
	{
		if (!tile)
			continue;

		tile->water.draw(view, projection, light.showLighting, simple, dt, camera.Position);

		if (tile->models.empty())
			continue;

		for (auto &mi : tile->models)
		{
			const std::shared_ptr<Model> &m = mi.first;
			const glm::mat4 &instModel = mi.second;

			if (!m)
				continue;

			m->draw(instModel, view, projection, camera.Position, light.showLighting, simple);
		}
	}

	// render skybox
	if (!simple)
	{
		glDepthMask(GL_FALSE);
		glDepthFunc(GL_LEQUAL);
		sky.draw(glm::mat4(1.0f), glm::mat4(glm::mat3(view)), projection, camera.Position, false, false);
		hill.draw(glm::mat4(1.0f), glm::mat4(glm::mat3(view)), projection, camera.Position, false, false);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
	}

	glBindVertexArray(0);
}
