#include "terrain.h"
#include <filesystem>
#include <array>
#include "libs/stb_image.h"
#include "Logger.h"
#include "parserTRN.h"
#include "libs/glm/glm.hpp"
#include "libs/glm/fwd.hpp"
#include "libs/glm/gtc/type_ptr.hpp"
#include "libs/glm/gtc/constants.hpp"
#include "libs/glm/gtc/packing.hpp"
#include "libs/glm/ext/vector_uint4.hpp"
#include "libs/glm/gtc/type_precision.hpp"
#include "parserITM.h"

// [TODO] annotate

//! Loads .zip file from disk, performs parsing for each contained .trn file, sets up terrain mesh data and sound.
void Terrain::load(const char *fpath, Sound &sound)
{
	reset();

	// process archive with .trn files and store map's tile data in a temporary vector of TileTerrain objects (we cannot just push back a loaded tile to the main 2D vector; at the same time, we cannot resize it, since dimensions of the terrain are yet unknown)

	struct tmp_TileTerrain
	{
		int tileX;
		int tileZ;
		TileTerrain *tileData;
	};

	std::vector<tmp_TileTerrain> tmp_tiles;

	CZipResReader *terrainArchive = new CZipResReader(fpath, true, false);

	CZipResReader *itemsArchive = new CZipResReader(std::string(fpath).replace(std::strlen(fpath) - 4, 4, ".itm").c_str(), true, false);

	CZipResReader *navigationArchive = new CZipResReader(std::string(fpath).replace(std::strlen(fpath) - 4, 4, ".nav").c_str(), true, false);

	CZipResReader *physicsArchive = new CZipResReader("data/terrain/physics.zip", true, false);

	dtNavMesh *navMesh = new dtNavMesh();

	for (int i = 0, n = terrainArchive->getFileCount(); i < n; i++)
	{
		IReadResFile *trnFile = terrainArchive->openFile(i); // open i-th .trn file inside the archive and return memory-read file object with the decompressed content

		if (trnFile)
		{
			int tileX, tileZ;													 // variables that will be assigned tile's position on the grid
			TileTerrain *tile = TileTerrain::load(trnFile, tileX, tileZ, *this); // .trn: load tile's terrain mesh data
			loadTileEntities(itemsArchive, physicsArchive, tileX, tileZ, tile, *this);
			loadTileNavigation(navigationArchive, navMesh, tileX, tileZ);

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

	// build vertex data in world space coordinates
	getTerrainVertices();

	getPhysicsVertices();

	getWaterVertices();

	getNavigationVertices(navMesh);

	// load skybox
	std::string terrainFileName = std::filesystem::path(fpath).filename().string();
	std::string terrainName = terrainFileName.replace(terrainFileName.size() - 4, 4, "");
	std::string skyName = "model/skybox/" + terrainName + "_sky.bdae";
	std::string hillName = "model/skybox/" + terrainName + "_hill.bdae";

	sky.load(skyName.c_str(), sound, true);
	hill.load(hillName.c_str(), sound, true);

	// set camera starting point
	if (terrainName == "pvp_forsaken_shrine")
	{
		camera.Position = glm::vec3(-125, 85, 160);
		camera.Pitch = -50.0f;
		camera.Yaw = 0.0f;
	}
	else if (terrainName == "pvp_garrison_quarter")
	{
		camera.Position = glm::vec3(95, 70, 250);
		camera.Pitch = -35.0f;
		camera.Yaw = -50.0f;
	}
	else if (terrainName == "pvp_mephitis_backwoods")
	{
		camera.Position = glm::vec3(40, 30, 110);
		camera.Pitch = -35.0f;
		camera.Yaw = -100.0f;
	}
	else if (terrainName == "pvp_merciless_ring")
	{
		camera.Position = glm::vec3(-40, 50, 400);
		camera.Pitch = -30.0f;
		camera.Yaw = -40.0f;
	}
	else if (terrainName == "pvp_arena_of_courage")
	{
		camera.Position = glm::vec3(110, 130, 230);
		camera.Pitch = -30.0f;
		camera.Yaw = -40.0f;
	}
	else if (terrainName == "pvp_the_lost_city")
	{
		camera.Position = glm::vec3(-280, 70, -330);
		camera.Pitch = -15.0f;
		camera.Yaw = -125.0f;
	}
	else if (terrainName == "relic's_key")
	{
		camera.Position = glm::vec3(-200, 100, 210);
		camera.Pitch = -25.0f;
		camera.Yaw = -40.0f;
	}
	else if (terrainName == "knahswahs_prison")
	{
		camera.Position = glm::vec3(-475, 140, -2050);
		camera.Pitch = -30.0f;
		camera.Yaw = -125.0f;
	}
	else if (terrainName == "tanned_land")
	{
		camera.Position = glm::vec3(-2600, 120, 195);
		camera.Pitch = -30.0f;
		camera.Yaw = -130.0f;
	}
	else if (terrainName == "sandbox")
	{
		camera.Position = glm::vec3(1110, 10, 700);
		camera.Pitch = 0.0f;
		camera.Yaw = 70.0f;
	}

	camera.updateCameraVectors();

	// set file info to be displayed in the settings panel
	fileName = std::filesystem::path(fpath).filename().string();
	fileSize = std::filesystem::file_size(fpath);
	vertexCount = 0;
	faceCount = 0;

	sound.searchSoundFiles(fileName, sounds);

	terrainLoaded = true;
}

void Terrain::uploadToGPU(TileTerrain *tile)
{
	if (!tile)
		return;

	if (!tile->terrainVertices.empty())
	{
		glGenVertexArrays(1, &tile->trnVAO);
		glGenBuffers(1, &tile->trnVBO);

		glBindVertexArray(tile->trnVAO);
		glBindBuffer(GL_ARRAY_BUFFER, tile->trnVBO);
		GLsizeiptr bufSize = (GLsizeiptr)(tile->terrainVertices.size() * sizeof(float));
		glBufferData(GL_ARRAY_BUFFER, bufSize, tile->terrainVertices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void *)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void *)(8 * sizeof(float)));
		glEnableVertexAttribArray(3);

		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void *)(11 * sizeof(float)));
		glEnableVertexAttribArray(4);

		glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void *)(15 * sizeof(float)));
		glEnableVertexAttribArray(5);

		glBindVertexArray(0);
	}

	if (!tile->water.vertices.empty())
	{
		glGenVertexArrays(1, &tile->water.VAO);
		glGenBuffers(1, &tile->water.VBO);

		glBindVertexArray(tile->water.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, tile->water.VBO);
		glBufferData(GL_ARRAY_BUFFER, tile->water.vertices.size() * sizeof(float), tile->water.vertices.empty() ? nullptr : tile->water.vertices.data(), GL_STATIC_DRAW);
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
	}

	if (!tile->navigationVertices.empty())
	{
		glGenVertexArrays(1, &tile->navVAO);
		glGenBuffers(1, &tile->navVBO);
		glBindVertexArray(tile->navVAO);
		glBindBuffer(GL_ARRAY_BUFFER, tile->navVBO);
		GLsizeiptr bufSize = (GLsizeiptr)(tile->navigationVertices.size() * sizeof(float));
		glBufferData(GL_ARRAY_BUFFER, bufSize, tile->navigationVertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
	}
}

void Terrain::releaseFromGPU(TileTerrain *tile)
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

	// for (auto &m : tile->models)
	// {
	// 	if (m)
	// 	{
	// 		// Provide a safe model-level unload method if your Model class supports it,
	// 		// e.g. m->releaseGPU(); Otherwise skip this.
	// 		// m->releaseGPU();
	// 	}
	// }
}

void Terrain::getTerrainVertices()
{
	static const int FLOATS_PER_TERR_VERTEX = 18;

	// 3. build the terrain mesh vertex data in world space coordinates (structured as triangles, which is optimal for rendering terrain surface; 1D vector with the following format: x1, y1, z1, x2, y2, z2, .. )

	// loop through each tile in the terrain
	for (int i = 0; i < tilesX; i++)
	{
		for (int j = 0; j < tilesZ; j++)
		{
			if (!tiles[i][j])
				continue;

			TileTerrain *t = tiles[i][j];

			// Reserve approx size: UnitsInTileCol * UnitsInTileRow * 6 vertices * floatsPerVertex
			// (6 vertices per unit because 2 triangles × 3 verts)
			size_t approxVerts = (size_t)UnitsInTileCol * (size_t)UnitsInTileRow * FLOATS_PER_TERR_VERTEX;
			t->terrainVertices.clear();
			t->terrainVertices.reserve(approxVerts * 6);

			for (int col = 0; col < UnitsInTileCol; ++col)
			{
				for (int row = 0; row < UnitsInTileRow; ++row)
				{
					float x0 = t->startX + (float)col;
					float x1 = t->startX + (float)(col + 1);
					float z0 = t->startZ + (float)row;
					float z1 = t->startZ + (float)(row + 1);

					float y00 = t->Y[row][col];
					float y10 = t->Y[row][col + 1];
					float y01 = t->Y[row + 1][col];
					float y11 = t->Y[row + 1][col + 1];

					// <-- minimal change here -->
					// texture coordinates: match original TRN behavior:
					// TCoords.X = vx * 0.125f; TCoords.Y = vy * 0.125f
					float u0 = (float)col * 0.125f;
					float u1 = (float)(col + 1) * 0.125f;
					float v0 = (float)row * 0.125f;
					float v1 = (float)(row + 1) * 0.125f;
					// <-- end change -->

					int chunkCol = col / 8;
					int chunkRow = row / 8;
					ChunkInfo &chunk = t->chunks[chunkRow * 8 + chunkCol];
					int tex1 = chunk.texNameIndex1;
					int tex2 = chunk.texNameIndex2;
					int tex3 = chunk.texNameIndex3;

					auto unpack_blend = [](glm::u8vec4 rgba) -> std::array<float, 4>
					{
						return {
							rgba.r / 255.0f,
							rgba.g / 255.0f,
							rgba.b / 255.0f,
							rgba.a / 255.0f};
					};

					auto blend00 = unpack_blend(t->colors[row][col]);
					auto blend10 = unpack_blend(t->colors[row][col + 1]);
					auto blend01 = unpack_blend(t->colors[row + 1][col]);
					auto blend11 = unpack_blend(t->colors[row + 1][col + 1]);

					glm::vec3 n00 = t->normals[row][col];
					glm::vec3 n10 = t->normals[row][col + 1];
					glm::vec3 n01 = t->normals[row + 1][col];
					glm::vec3 n11 = t->normals[row + 1][col + 1];

					// === First Triangle ===
					t->terrainVertices.insert(t->terrainVertices.end(), {x0, y00, z0, 1.0f, 0.0f, 0.0f, u0, v0,
																		 (float)tex1, (float)tex2, (float)tex3,
																		 blend00[0], blend00[1], blend00[2], blend00[3],
																		 n00.x, n00.y, n00.z});

					t->terrainVertices.insert(t->terrainVertices.end(), {x0, y01, z1, 0.0f, 1.0f, 0.0f, u0, v1,
																		 (float)tex1, (float)tex2, (float)tex3,
																		 blend01[0], blend01[1], blend01[2], blend01[3],
																		 n01.x, n01.y, n01.z});

					t->terrainVertices.insert(t->terrainVertices.end(), {x1, y11, z1, 0.0f, 0.0f, 1.0f, u1, v1,
																		 (float)tex1, (float)tex2, (float)tex3,
																		 blend11[0], blend11[1], blend11[2], blend11[3],
																		 n11.x, n11.y, n11.z});

					// === Second Triangle ===
					t->terrainVertices.insert(t->terrainVertices.end(), {x0, y00, z0, 1.0f, 0.0f, 0.0f, u0, v0,
																		 (float)tex1, (float)tex2, (float)tex3,
																		 blend00[0], blend00[1], blend00[2], blend00[3],
																		 n00.x, n00.y, n00.z});

					t->terrainVertices.insert(t->terrainVertices.end(), {x1, y11, z1, 0.0f, 0.0f, 1.0f, u1, v1,
																		 (float)tex1, (float)tex2, (float)tex3,
																		 blend11[0], blend11[1], blend11[2], blend11[3],
																		 n11.x, n11.y, n11.z});

					t->terrainVertices.insert(t->terrainVertices.end(), {x1, y10, z0, 0.0f, 1.0f, 0.0f, u1, v0,
																		 (float)tex1, (float)tex2, (float)tex3,
																		 blend10[0], blend10[1], blend10[2], blend10[3],
																		 n10.x, n10.y, n10.z});
				}
			}

			/*
			// load texture(s)
			int textureCount = (int)t->textureNames.size();
			std::vector<unsigned char *> pixelData(textureCount);
			int w, h, c, nrChannels = 0;

			for (int i = 0; i < textureCount; ++i)
			{
				// normalize slashes
				std::replace(t->textureNames[i].begin(), t->textureNames[i].end(), '\\', '/');

				const std::string needle = "texture/";
				const std::string insertStr = "unsorted/";

				size_t pos = t->textureNames[i].find(needle);

				if (pos != std::string::npos)
				{
					size_t ins = pos + needle.size();
					// only insert if "unsorted/" is not already there
					if (t->textureNames[i].compare(ins, insertStr.size(), insertStr) != 0)
					{
						t->textureNames[i].insert(ins, insertStr);
					}
				}

				t->textureNames[i] = "data/" + t->textureNames[i];

				pixelData[i] = stbi_load(t->textureNames[i].c_str(), &w, &h, &c, 4);
				if (!pixelData[i])
				{
					std::cout << "Failed to load texture: " << t->textureNames[i] << "\n";
					continue;
				}
			}

			glGenTextures(1, &t->textureMap);
			glBindTexture(GL_TEXTURE_2D_ARRAY, t->textureMap);

			// Allocate storage: 1 mip level, RGBA8, width x height, depth = textureCount
			glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, w, h, textureCount);

			// 3) Upload each image into its layer
			for (int i = 0; i < textureCount; ++i)
			{
				if (pixelData[i])
				{
					glTexSubImage3D(
						GL_TEXTURE_2D_ARRAY,
						0,		 // mip level
						0, 0, i, // x, y, layer
						w,
						h,
						1, // depth = 1 slice
						GL_RGBA,
						GL_UNSIGNED_BYTE,
						pixelData[i]);
				}
			}

			// 4) Set filtering/wrap params once on the array
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

			// generate mipmaps (optional if you allocated >1 level)
			glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

			// free CPU image data
			for (auto ptr : pixelData)
				if (ptr)
					stbi_image_free(ptr);
			*/

			// compute vertex count (every vertex = 18 floats)
			if (t->terrainVertices.empty())
				t->terrainVertexCount = 0;
			else
				t->terrainVertexCount = (int)(t->terrainVertices.size() / FLOATS_PER_TERR_VERTEX);
		}
	}
}

void Terrain::getWaterVertices()
{
	const float unitsPerChunkX = (float)UnitsInTileRow / ChunksInTileRow;
	const float unitsPerChunkZ = (float)UnitsInTileCol / ChunksInTileCol;
	const float nx = 0.0f, ny = 1.0f, nz = 0.0f;

	for (int ti = 0; ti < tilesX; ++ti)
	{
		for (int tj = 0; tj < tilesZ; ++tj)
		{
			TileTerrain *tile = tiles[ti][tj];

			if (!tile || !tile->chunks)
				continue;

			for (int cz = 0; cz < ChunksInTileCol; ++cz)
			{
				for (int cx = 0; cx < ChunksInTileRow; ++cx)
				{
					ChunkInfo &chunk = tile->chunks[cz * ChunksInTileRow + cx];

					if (!(chunk.flag & TRNF_HASWATER) || chunk.waterLevel == 0 || chunk.waterLevel == -5000)
						continue;

					float y = chunk.waterLevel * 0.01f;
					float x0 = tile->startX + cx * unitsPerChunkX;
					float z0 = tile->startZ + cz * unitsPerChunkZ;
					float x1 = x0 + unitsPerChunkX;
					float z1 = z0 + unitsPerChunkZ;

					float u0 = x0, v0 = z0, u1 = x1, v1 = z1;

					std::initializer_list<float> quad = {
						x0, y, z0, nx, ny, nz, u0, v0,
						x1, y, z0, nx, ny, nz, u1, v0,
						x1, y, z1, nx, ny, nz, u1, v1,

						x0, y, z0, nx, ny, nz, u0, v0,
						x1, y, z1, nx, ny, nz, u1, v1,
						x0, y, z1, nx, ny, nz, u0, v1};
					tile->water.vertices.insert(tile->water.vertices.end(), quad);
				}
			}

			tile->water.waterVertexCount = tile->water.vertices.size() / 8;
		}
	}
}

// Load Nav Mesh if exists
void Terrain::loadTileNavigation(CZipResReader *navigationArchive, dtNavMesh *navMesh, int gridX, int gridZ)
{
	if (!navigationArchive)
		return;

	if (navigationArchive->getFileCount() == 0)
		return;

	char tmpName[256];

#ifdef BETA_GAME_VERSION
	std::string terrainName = std::filesystem::path(navigationArchive->getZipFileName()).stem().string();
	sprintf(tmpName, "%04d_%04d.nav", gridX, gridZ); // path inside the .nav archive
#else
	sprintf(tmpName, "navmesh/%04d_%04d.nav", gridX, gridZ); // [TODO] test!
#endif

	IReadResFile *navFile = navigationArchive->openFile(tmpName);

	if (!navFile)
		return;

	navFile->seek(0);
	int fileSize = navFile->getSize();
	unsigned char *buffer = new unsigned char[fileSize];
	navFile->read(buffer, fileSize);
	navFile->drop();

	int tileRef = navMesh->addTile(buffer, fileSize, DT_TILE_FREE_DATA, 0); // register in navigation system, granting Detour in-memory ownership (buffer will be freed automatically when the tile is destroyed)
}

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

//! Returns a single vector partitioned into [0…fillVerts) = triangles, [fillVerts…end) = lines.
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

//! Clears GPU memory and resets viewer state.
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
}

// Replace the whole function in your file with this version.
void Terrain::updateVisibleTiles(glm::mat4 view, glm::mat4 projection)
{
	tilesVisible.clear();

	if (!terrainLoaded || tilesX == 0 || tilesZ == 0)
		return;

	const glm::vec3 camPos = camera.Position;
	const float tileSize = static_cast<float>(ChunksInTile);

	// Distance thresholds (in tiles)
	const int prefetchTiles = 2;								   // how many extra tiles to keep uploaded beyond render radius
	const int renderRadiusTiles = visibleRadiusTiles;			   // radius used for visible list & frustum culling
	const int loadRadiusTiles = renderRadiusTiles + prefetchTiles; // tiles within this radius will be uploaded to GPU
	const int unloadRadiusTiles = loadRadiusTiles + 2;			   // tiles beyond this radius will be unloaded from GPU

	const float loadRadiusW = (float)loadRadiusTiles * tileSize;
	const float unloadRadiusW = (float)unloadRadiusTiles * tileSize;
	const float loadRadiusSq = loadRadiusW * loadRadiusW;
	const float unloadRadiusSq = unloadRadiusW * unloadRadiusW;

	// Camera tile indices (grid coords)
	const int camTileX = (int)std::floor(camPos.x / tileSize);
	const int camTileZ = (int)std::floor(camPos.z / tileSize);

	// --- First pass: distance-based GPU upload/unload for EVERY tile ---
	// This ensures tiles within loadRadius are uploaded to GPU (so they can be rendered),
	// and tiles outside unloadRadius are freed from GPU (keeping memory bounded).
	for (int ix = 0; ix < tilesX; ++ix)
	{
		for (int iz = 0; iz < tilesZ; ++iz)
		{
			TileTerrain *t = tiles[ix][iz];
			if (!t)
				continue;

			int gx = ix + tileMinX;
			int gz = iz + tileMinZ;

			float centerX = (gx + 0.5f) * tileSize;
			float centerZ = (gz + 0.5f) * tileSize;
			float dx = camPos.x - centerX;
			float dz = camPos.z - centerZ;
			float dsq = dx * dx + dz * dz;

			// Upload if inside load radius and not yet uploaded
			if (dsq <= loadRadiusSq)
			{
				// Use tile->trnVAO == 0 as indicator that tile is not uploaded.
				// Ensure terrainVertices are present before upload.
				if ((t->trnVAO == 0) && !t->terrainVertices.empty())
				{
					uploadToGPU(t); // your function that creates VAOs/VBOs on the main thread
				}
			}
			// Unload if outside unload radius and currently uploaded
			// t->navVAO != 0 || t->navVBO != 0 ||

			else if (dsq > unloadRadiusSq)
			{
				// If the tile has GPU buffers, free them (but keep CPU data)
				if (t->trnVAO != 0 || t->trnVBO != 0 ||
					t->phyVAO != 0 || t->phyVBO != 0 ||
					t->water.VAO != 0 || t->water.VBO != 0)
				{
					releaseFromGPU(t);
				}
			}
			// else: tile is between load and unload radius — keep its current GPU state
		}
	}

	// --- Second pass: frustum culling to build tilesVisible (render list) ---
	// compute square around camera for frustum test
	int centerX = camTileX - tileMinX;
	int centerZ = camTileZ - tileMinZ;
	centerX = std::clamp(centerX, 0, tilesX - 1);
	centerZ = std::clamp(centerZ, 0, tilesZ - 1);

	int r = renderRadiusTiles;
	int x0 = std::max(0, centerX - r);
	int x1 = std::min(tilesX - 1, centerX + r);
	int z0 = std::max(0, centerZ - r);
	int z1 = std::min(tilesZ - 1, centerZ + r);

	tilesVisible.reserve((x1 - x0 + 1) * (z1 - z0 + 1));

	// Build frustum planes
	glm::mat4 clip = projection * view;
	struct Plane
	{
		glm::vec3 n;
		float d;
	};
	std::array<Plane, 6> planes;

	auto extractPlane = [&](int rowA, int rowB, bool subtract) -> Plane
	{
		float a, b, c, dd;
		if (!subtract)
		{
			a = clip[0][3] + clip[0][rowB];
			b = clip[1][3] + clip[1][rowB];
			c = clip[2][3] + clip[2][rowB];
			dd = clip[3][3] + clip[3][rowB];
		}
		else
		{
			a = clip[0][3] - clip[0][rowB];
			b = clip[1][3] - clip[1][rowB];
			c = clip[2][3] - clip[2][rowB];
			dd = clip[3][3] - clip[3][rowB];
		}
		Plane p;
		p.n = glm::vec3(a, b, c);
		float len = glm::length(p.n);
		if (len > 0.0f)
		{
			p.n /= len;
			p.d = dd / len;
		}
		else
		{
			p.d = dd;
		}
		return p;
	};

	planes[0] = extractPlane(3, 0, false); // left
	planes[1] = extractPlane(3, 0, true);  // right
	planes[2] = extractPlane(3, 1, false); // bottom
	planes[3] = extractPlane(3, 1, true);  // top
	planes[4] = extractPlane(3, 2, false); // near
	planes[5] = extractPlane(3, 2, true);  // far

	auto aabbOutsidePlane = [](const Plane &p, const glm::vec3 &aabbMin, const glm::vec3 &aabbMax) -> bool
	{
		glm::vec3 pos;
		pos.x = (p.n.x >= 0.0f) ? aabbMax.x : aabbMin.x;
		pos.y = (p.n.y >= 0.0f) ? aabbMax.y : aabbMin.y;
		pos.z = (p.n.z >= 0.0f) ? aabbMax.z : aabbMin.z;
		float dist = glm::dot(p.n, pos) + p.d;
		return (dist < 0.0f);
	};

	for (int x = x0; x <= x1; ++x)
	{
		for (int z = z0; z <= z1; ++z)
		{
			TileTerrain *t = tiles[x][z];
			if (!t)
				continue;

			glm::vec3 amin = glm::vec3(t->BBox.MinEdge.X, t->BBox.MinEdge.Y, t->BBox.MinEdge.Z);
			glm::vec3 amax = glm::vec3(t->BBox.MaxEdge.X, t->BBox.MaxEdge.Y, t->BBox.MaxEdge.Z);

			bool culled = false;

			for (const Plane &pl : planes)
			{
				if (aabbOutsidePlane(pl, amin, amax))
				{
					culled = true;
					break;
				}
			}
			if (!culled)
				tilesVisible.push_back(t);
		}
	}
}

//! Renders terrain and physics geometry meshes.
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

	updateVisibleTiles(view, projection);

	// render terrain (safe!)
	if (!simple)
		shader.setInt("renderMode", 1);
	else
		shader.setInt("renderMode", 3);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	for (TileTerrain *tile : tilesVisible)
	{
		if (!tile)
			continue;

		if (tile->trnVAO == 0 || tile->trnVBO == 0)
			continue;

		glBindVertexArray(tile->trnVAO);
		// glActiveTexture(GL_TEXTURE0);
		// glBindTexture(GL_TEXTURE_2D_ARRAY, tile->textureMap);
		glDrawArrays(GL_TRIANGLES, 0, tile->terrainVertexCount);
		glBindVertexArray(0);
	}

	// render walkable surfaces
	if (renderNavMesh)
	{
		shader.setInt("renderMode", 5);
		shader.setInt("terrainBlendMode", 2);

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

	// render water and 3D models
	for (TileTerrain *tile : tilesVisible)
	{
		if (!tile)
			continue;

		tile->water.draw(view, projection, light.showLighting, simple, dt, camera.Position);

		for (auto &mi : tile->models)
		{
			const std::shared_ptr<Model> &m = mi.first;
			const glm::mat4 &instModel = mi.second;

			if (!m)
				continue;
			if (!m->modelLoaded)
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
