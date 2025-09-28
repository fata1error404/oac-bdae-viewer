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

	CZipResReader *masksArchive = new CZipResReader(std::string(fpath).replace(std::strlen(fpath) - 4, 4, ".msk").c_str(), true, false);

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
			loadTileMasks(masksArchive, tileX, tileZ, tile);
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

	// build vertex data in world space coordinates
	getTerrainVertices();

	getNavigationVertices(navMesh);

	getPhysicsVertices();

	getWaterVertices();

	// load skybox
	std::string terrainFileName = std::filesystem::path(fpath).filename().string();
	std::string terrainName = terrainFileName.replace(terrainFileName.size() - 4, 4, "");
	std::string skyName = "model/skybox/" + terrainName + "_sky.bdae";
	std::string hillName = "model/skybox/" + terrainName + "_hill.bdae";

	sky.load(skyName.c_str(), sound, true);
	hill.load(hillName.c_str(), sound, true);

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

	/*
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
	else if (terrainName == "1_relic's_key")
	{
		camera.Position = glm::vec3(-222, 42, 214);
		camera.Pitch = 0.0f;
		camera.Yaw = -25.0f;
	}
	else if (terrainName == "2_knahswahs_prison")
	{
		camera.Position = glm::vec3(-545, 15, -2325);
		camera.Pitch = 0.0f;
		camera.Yaw = -90.0f;
	}
	else if (terrainName == "3_young_deity's_realm")
	{
		camera.Position = glm::vec3(-115, -8, 150);
		camera.Pitch = 0.0f;
		camera.Yaw = 90.0f;
	}
	else if (terrainName == "4_sailen_the_lower_city")
	{
		camera.Position = glm::vec3(1528, 12, -1080);
		camera.Pitch = 10.0f;
		camera.Yaw = 140.0f;
	}
	else if (terrainName == "6_eidolon's_horizon")
	{
		camera.Position = glm::vec3(-156, 53, -1600);
		camera.Pitch = 5.0f;
		camera.Yaw = -100.0f;
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
	else if (terrainName == "human_selection")
	{
		camera.Position = glm::vec3(1200, 120, 40);
		camera.Pitch = -20.0f;
		camera.Yaw = 45.0f;
	}
	else if (terrainName == "amusement_park1")
	{
		camera.Position = glm::vec3(-975, 50, -160);
		camera.Pitch = 10.0f;
		camera.Yaw = -280.0f;
	}
	else if (terrainName == "amusement_park2")
	{
		camera.Position = glm::vec3(1214, 7, 351);
		camera.Pitch = 5.0f;
		camera.Yaw = -370.0f;
	}
	else if (terrainName == "ghost_island")
	{
		camera.Position = glm::vec3(-625, 87, -590);
		camera.Pitch = 0.0f;
		camera.Yaw = 685.0f;
	}
	else if (terrainName == "flare_island")
	{
		camera.Position = glm::vec3(-1373, 45, -430);
		camera.Pitch = 0.0f;
		camera.Yaw = -50.0f;
	}
	else if (terrainName == "hanging_gardens")
	{
		camera.Position = glm::vec3(1509, 265, 805);
		camera.Pitch = 0.0f;
		camera.Yaw = -110.0f;
	}
	else if (terrainName == "polynia")
	{
		camera.Position = glm::vec3(901, 3, -217);
		camera.Pitch = 6.0f;
		camera.Yaw = -150.0f;
	}
	else if (terrainName == "greenmont")
	{
		camera.Position = glm::vec3(-183, 60, -20);
		camera.Pitch = -21.0f;
		camera.Yaw = 65.0f;
	}

	camera.updateCameraVectors();

	// set file info to be displayed in the settings panel
	fileName = std::filesystem::path(fpath).filename().string();
	fileSize = std::filesystem::file_size(fpath);
	vertexCount = 0;
	faceCount = 0;

	sound.searchSoundFiles(fileName, sounds);

	light.showLighting = true;
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
}

void Terrain::getTerrainVertices()
{
	unsigned int textureCount = uniqueTextureNames.size();
	std::vector<unsigned char *> trnTextures(textureCount, nullptr);
	std::vector<int> width(textureCount, 0);
	std::vector<int> height(textureCount, 0);

	auto alloc_white_256 = []() -> unsigned char *
	{
		const int sz = 256 * 256 * 4;
		unsigned char *p = (unsigned char *)malloc(sz);
		if (p)
			memset(p, 255, sz);
		return p;
	};

	auto resize_to_256 = [](unsigned char *src, int srcW, int srcH) -> unsigned char *
	{
		const int dstW = 256, dstH = 256;
		unsigned char *dst = (unsigned char *)malloc(dstW * dstH * 4);
		if (!dst)
			return nullptr;
		for (int y = 0; y < dstH; ++y)
		{
			int sy = (y * srcH) / dstH;
			for (int x = 0; x < dstW; ++x)
			{
				int sx = (x * srcW) / dstW;
				unsigned char *s = src + (sy * srcW + sx) * 4;
				unsigned char *d = dst + (y * dstW + x) * 4;
				d[0] = s[0];
				d[1] = s[1];
				d[2] = s[2];
				d[3] = s[3];
			}
		}
		return dst;
	};

	// load and normalize textures to 256x256 RGBA
	for (int ti = 0; ti < (int)textureCount; ++ti)
	{
		std::string textureName = uniqueTextureNames[ti];
		std::replace(textureName.begin(), textureName.end(), '\\', '/');

		const std::string needle = "texture/";
		const std::string insertStr = "unsorted/";
		if (auto pos = textureName.find(needle); pos != std::string::npos)
		{
			size_t ins = pos + needle.size();
			if (textureName.compare(ins, insertStr.size(), insertStr) != 0)
				textureName.insert(ins, insertStr);
		}

		textureName = "data/" + textureName;

		int w = 0, h = 0, channels = 0;
		unsigned char *data = stbi_load(textureName.c_str(), &w, &h, &channels, 4);

		if (!data)
		{
			std::cout << "[Warning] Failed to load texture: " << textureName << "\n"
					  << "          Using fallback white 256x256 texture.\n";
			trnTextures[ti] = alloc_white_256();
			width[ti] = 256;
			height[ti] = 256;
		}
		else
		{
			if (w != 256 || h != 256)
			{
				unsigned char *resized = resize_to_256(data, w, h);
				if (resized)
				{
					stbi_image_free(data);
					trnTextures[ti] = resized;
					width[ti] = 256;
					height[ti] = 256;
					std::cout << "[Info] Resized texture " << textureName << " from " << w << "x" << h << " to 256x256.\n";
				}
				else
				{
					stbi_image_free(data);
					trnTextures[ti] = alloc_white_256();
					width[ti] = 256;
					height[ti] = 256;
					std::cout << "[Warning] Failed to resize texture: " << textureName << "\n";
				}
			}
			else
			{
				trnTextures[ti] = data;
				width[ti] = w;
				height[ti] = h;
			}
		}
	}

	// 3. build the terrain mesh vertex data in world space coordinates (triangles)
	// loop through each tile in the terrain
	for (int ix = 0; ix < tilesX; ix++)
	{
		for (int jx = 0; jx < tilesZ; jx++)
		{
			TileTerrain *tile = tiles[ix][jx];
			if (!tile)
				continue;

			// build mapping from global texture index -> layer index in this tile
			std::unordered_map<int, int> globalToLayer;
			for (size_t ti = 0; ti < tile->textureIndices.size(); ++ti)
			{
				globalToLayer[tile->textureIndices[ti]] = (int)ti;
			}

			// reserve expected capacity (6 vertices per cell, 20 floats per vertex)
			tile->terrainVertices.reserve(UnitsInTileRow * UnitsInTileCol * 6 * 20);

			for (int col = 0; col < UnitsInTileCol; col++)
			{
				for (int row = 0; row < UnitsInTileRow; row++)
				{
					float x0 = tile->startX + float(col);
					float x1 = tile->startX + float(col + 1);
					float z0 = tile->startZ + float(row);
					float z1 = tile->startZ + float(row + 1);

					float y00 = tile->Y[row][col];
					float y10 = tile->Y[row][col + 1];
					float y01 = tile->Y[row + 1][col];
					float y11 = tile->Y[row + 1][col + 1];

					float base_u0 = float(col) / 8.0f;
					float base_u1 = float(col + 1) / 8.0f;
					float base_v0 = float(row) / 8.0f;
					float base_v1 = float(row + 1) / 8.0f;

					float mask_u0 = float(col) / 64.0f;
					float mask_u1 = float(col + 1) / 64.0f;
					float mask_v0 = float(row) / 64.0f;
					float mask_v1 = float(row + 1) / 64.0f;

					int chunkX = col / 8;
					int chunkY = row / 8;
					int chunkIndex = chunkX + chunkY * 8;
					// if (chunkIndex < 0 || chunkIndex >= (int)tile->chunks.size())
					// {
					// 	// invalid chunk index — skip this cell
					// 	continue;
					// }
					ChunkInfo &chunk = tile->chunks[chunkIndex];

					int layer1 = -1, layer2 = -1, layer3 = -1;
					auto it1 = globalToLayer.find(chunk.texNameIndex1);
					if (it1 != globalToLayer.end())
						layer1 = it1->second;
					auto it2 = globalToLayer.find(chunk.texNameIndex2);
					if (it2 != globalToLayer.end())
						layer2 = it2->second;
					auto it3 = globalToLayer.find(chunk.texNameIndex3);
					if (it3 != globalToLayer.end())
						layer3 = it3->second;

					float texIdx1 = (layer1 >= 0) ? (float)layer1 : -1.0f;
					float texIdx2 = (layer2 >= 0) ? (float)layer2 : -1.0f;
					float texIdx3 = (layer3 >= 0) ? (float)layer3 : -1.0f;

					auto unpack_blend = [](glm::u8vec4 rgba) -> std::array<float, 4>
					{
						return {
							static_cast<float>(rgba.r) / 255.0f,
							static_cast<float>(rgba.g) / 255.0f,
							static_cast<float>(rgba.b) / 255.0f,
							static_cast<float>(rgba.a) / 255.0f};
					};

					auto blend00 = unpack_blend(tile->colors[row][col]);
					auto blend10 = unpack_blend(tile->colors[row][col + 1]);
					auto blend01 = unpack_blend(tile->colors[row + 1][col]);
					auto blend11 = unpack_blend(tile->colors[row + 1][col + 1]);

					glm::vec3 n00 = tile->normals[row][col];
					glm::vec3 n10 = tile->normals[row][col + 1];
					glm::vec3 n01 = tile->normals[row + 1][col];
					glm::vec3 n11 = tile->normals[row + 1][col + 1];

					// first triangle
					tile->terrainVertices.insert(tile->terrainVertices.end(), {x0, y00, z0,
																			   n00.x, n00.y, n00.z,
																			   base_u0, base_v0,
																			   mask_u0, mask_v0,
																			   texIdx1, texIdx2, texIdx3,
																			   blend00[0], blend00[1], blend00[2], blend00[3],
																			   1.0f, 0.0f, 0.0f});

					tile->terrainVertices.insert(tile->terrainVertices.end(), {x0, y01, z1,
																			   n01.x, n01.y, n01.z,
																			   base_u0, base_v1,
																			   mask_u0, mask_v1,
																			   texIdx1, texIdx2, texIdx3,
																			   blend01[0], blend01[1], blend01[2], blend01[3],
																			   0.0f, 1.0f, 0.0f});

					tile->terrainVertices.insert(tile->terrainVertices.end(), {x1, y11, z1,
																			   n11.x, n11.y, n11.z,
																			   base_u1, base_v1,
																			   mask_u1, mask_v1,
																			   texIdx1, texIdx2, texIdx3,
																			   blend11[0], blend11[1], blend11[2], blend11[3],
																			   0.0f, 0.0f, 1.0f});

					// second triangle
					tile->terrainVertices.insert(tile->terrainVertices.end(), {x0, y00, z0,
																			   n00.x, n00.y, n00.z,
																			   base_u0, base_v0,
																			   mask_u0, mask_v0,
																			   texIdx1, texIdx2, texIdx3,
																			   blend00[0], blend00[1], blend00[2], blend00[3],
																			   1.0f, 0.0f, 0.0f});

					tile->terrainVertices.insert(tile->terrainVertices.end(), {x1, y11, z1,
																			   n11.x, n11.y, n11.z,
																			   base_u1, base_v1,
																			   mask_u1, mask_v1,
																			   texIdx1, texIdx2, texIdx3,
																			   blend11[0], blend11[1], blend11[2], blend11[3],
																			   0.0f, 0.0f, 1.0f});

					tile->terrainVertices.insert(tile->terrainVertices.end(), {x1, y10, z0,
																			   n10.x, n10.y, n10.z,
																			   base_u1, base_v0,
																			   mask_u1, mask_v0,
																			   texIdx1, texIdx2, texIdx3,
																			   blend10[0], blend10[1], blend10[2], blend10[3],
																			   0.0f, 1.0f, 0.0f});
				}
			}

			// upload textures for this tile into a texture array
			if (!tile->textureIndices.empty())
			{
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // prevent alignment issues

				glGenTextures(1, &tile->textureMap);
				glBindTexture(GL_TEXTURE_2D_ARRAY, tile->textureMap);

				int layers = (int)tile->textureIndices.size();
				glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 256, 256, layers);

				for (int layer = 0; layer < layers; ++layer)
				{
					int globalIdx = tile->textureIndices[layer];
					if (globalIdx < 0 || globalIdx >= (int)trnTextures.size())
					{
						std::cout << "[Error] Texture index out of range for tile: " << globalIdx << "\n";
						unsigned char *fallback = alloc_white_256();
						if (fallback)
						{
							glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, 256, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
							stbi_image_free(fallback);
						}
						continue;
					}

					unsigned char *imgptr = trnTextures[globalIdx];
					if (!imgptr)
					{
						std::cout << "[Warning] trnTextures[" << globalIdx << "] == NULL; using fallback\n";
						unsigned char *fallback = alloc_white_256();
						if (fallback)
						{
							glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, 256, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
							stbi_image_free(fallback);
						}
						continue;
					}

					// all trnTextures entries are guaranteed to be 256x256x4 bytes
					glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, 256, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, imgptr);
				}

				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

				// levels == 1 used in glTexStorage3D, so do not call glGenerateMipmap.
				glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

				glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore default
			}

			// set vertex count (20 floats per vertex)
			if (tile->terrainVertices.empty())
				tile->terrainVertexCount = 0;
			else
				tile->terrainVertexCount = (int)(tile->terrainVertices.size() / 20);
		}
	}

	// free CPU image data (works with stbi_image_free for data allocated by stbi or malloc)
	for (auto ptr : trnTextures)
		if (ptr)
			stbi_image_free(ptr);
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

void Terrain::loadTileMasks(CZipResReader *masksArchive, int gridX, int gridZ, TileTerrain *tile)
{
	if (!masksArchive || !tile)
		return;

	constexpr int MASK_MAP_RESOLUTION = 256;
	const int expectedSize = MASK_MAP_RESOLUTION * MASK_MAP_RESOLUTION;

	// helper to read file into a buffer (returns true if successful)
	auto readFileToBuf = [&](const char *name, std::vector<unsigned char> &outBuf) -> bool
	{
		IReadResFile *f = masksArchive->openFile(name);
		if (!f)
			return false;
		int sz = (int)f->getSize();
		if (sz != expectedSize)
		{
			std::cout << "[Warning] " << name << " unexpected size: " << sz << " (expected " << expectedSize << ")\n";
			f->drop();
			return false;
		}
		outBuf.resize(expectedSize);
		f->read(outBuf.data(), expectedSize);
		f->drop();
		return true;
	};

	char name0[256], name1[256], shwName[256];
	sprintf(name0, "%04d_%04d_0.msk", gridX, gridZ);
	sprintf(name1, "%04d_%04d_1.msk", gridX, gridZ);
	sprintf(shwName, "%04d_%04d.shw", gridX, gridZ);

	// read primary mask (_0) — required
	std::vector<unsigned char> buf0;
	if (!readFileToBuf(name0, buf0))
	{
		// required file missing or wrong size — bail out
		// std::cout << "[Warning] missing or invalid primary mask " << name0 << " for tile " << gridX << "," << gridZ << std::endl;
		return;
	}

	// read secondary mask (_1), optional (filled with zeros if missing/invalid)
	std::vector<unsigned char> buf1(expectedSize, 0);
	if (!readFileToBuf(name1, buf1))
	{
		// missing or invalid is OK; buf1 stays zero
		// std::cout << "[Info] secondary mask not found for tile, using zeros\n";
	}

	// read shadow mask (.shw), optional (filled with zeros if missing/invalid)
	std::vector<unsigned char> bufShw(expectedSize, 0);
	if (!readFileToBuf(shwName, bufShw))
	{
		// missing is OK; bufShw stays zero
		// std::cout << "[Info] shadow mask not found for tile, using zeros\n";
	}

	// Pack R = buf0, G = buf1, B = bufShw
	std::vector<unsigned char> rgb;
	rgb.resize(expectedSize * 3);
	for (int i = 0; i < expectedSize; ++i)
	{
		rgb[3 * i + 0] = buf0[i];	// R = mask0
		rgb[3 * i + 1] = buf1[i];	// G = mask1
		rgb[3 * i + 2] = bufShw[i]; // B = shadow mask
	}

	// Upload to GL
	glGenTextures(1, &tile->maskTexture);
	glBindTexture(GL_TEXTURE_2D, tile->maskTexture);

	// RGB8 internal format, upload RGB data
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, MASK_MAP_RESOLUTION, MASK_MAP_RESOLUTION, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

	// parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // or GL_LINEAR_MIPMAP_LINEAR if you generate mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// free CPU buffers
	rgb.clear();
	buf0.clear();
	buf1.clear();
	bufShw.clear();
}

// Load Nav Mesh if exists
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
	uniqueTextureNames.clear();
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

	// // Tell shader which texture units to use
	shader.setInt("baseTextureArray", 0);
	shader.setInt("maskTexture", 1);

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
