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

//! Loads .zip file from disk, performs parsing for each contained .trn file, sets up terrain mesh data and sound.
void Terrain::load(const char *fpath, Sound &sound)
{
	reset();

	// 1. process archive with .trn files and store map's tile data in a temporary vector of TileTerrain objects (we cannot just push back a loaded tile to the main 2D vector; at the same time, we cannot resize it, since dimensions of the terrain are yet unknown)

	struct tmp_TileTerrain
	{
		int tileX;
		int tileZ;
		TileTerrain *tileData;
	};

	std::vector<tmp_TileTerrain> tmp_tiles;

	CZipResReader *terrainArchive = new CZipResReader(fpath, true, false);

	CZipResReader *itemsArchive = new CZipResReader(std::string(fpath).replace(std::strlen(fpath) - 4, 4, ".itm").c_str(), true, false);

	CZipResReader *physicsArchive = new CZipResReader("data/terrain/physics.zip", true, false);

	for (int i = 0, n = terrainArchive->getFileCount(); i < n; i++)
	{
		IReadResFile *trnFile = terrainArchive->openFile(i); // open i-th .trn file inside the archive and return memory-read file object with the decompressed content

		if (trnFile)
		{
			int tileX, tileZ;													 // variables that will be assigned tile's position on the grid
			TileTerrain *tile = TileTerrain::load(trnFile, tileX, tileZ, *this); // .trn: load tile's terrain mesh data
			loadTileEntities(itemsArchive, physicsArchive, tileX, tileZ, tile, *this);

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
					float x0 = tiles[i][j]->startX + (float)col;
					float x1 = tiles[i][j]->startX + (float)(col + 1);
					float z0 = tiles[i][j]->startZ + (float)row;
					float z1 = tiles[i][j]->startZ + (float)(row + 1);

					float y00 = tiles[i][j]->Y[row][col];
					float y10 = tiles[i][j]->Y[row][col + 1];
					float y01 = tiles[i][j]->Y[row + 1][col];
					float y11 = tiles[i][j]->Y[row + 1][col + 1];

					float u0 = 0.0f, u1 = 1.0f;
					float v0 = 0.0f, v1 = 1.0f;

					int chunkCol = col / 8;
					int chunkRow = row / 8;
					ChunkInfo &chunk = tiles[i][j]->chunks[chunkRow * 8 + chunkCol];
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

					auto blend00 = unpack_blend(tiles[i][j]->colors[row][col]);
					auto blend10 = unpack_blend(tiles[i][j]->colors[row][col + 1]);
					auto blend01 = unpack_blend(tiles[i][j]->colors[row + 1][col]);
					auto blend11 = unpack_blend(tiles[i][j]->colors[row + 1][col + 1]);

					// === Unpacked normals ===
					glm::vec3 n00 = tiles[i][j]->normals[row][col];
					glm::vec3 n10 = tiles[i][j]->normals[row][col + 1];
					glm::vec3 n01 = tiles[i][j]->normals[row + 1][col];
					glm::vec3 n11 = tiles[i][j]->normals[row + 1][col + 1];

					// === First Triangle ===
					terrainVertices.insert(terrainVertices.end(), {x0, y00, z0, 1, 0, 0, u0, v0,
																   (float)tex1, (float)tex2, (float)tex3,
																   blend00[0], blend00[1], blend00[2], blend00[3],
																   n00.x, n00.y, n00.z});

					terrainVertices.insert(terrainVertices.end(), {x0, y01, z1, 0, 1, 0, u0, v1,
																   (float)tex1, (float)tex2, (float)tex3,
																   blend01[0], blend01[1], blend01[2], blend01[3],
																   n01.x, n01.y, n01.z});

					terrainVertices.insert(terrainVertices.end(), {x1, y11, z1, 0, 0, 1, u1, v1,
																   (float)tex1, (float)tex2, (float)tex3,
																   blend11[0], blend11[1], blend11[2], blend11[3],
																   n11.x, n11.y, n11.z});

					// === Second Triangle ===
					terrainVertices.insert(terrainVertices.end(), {x0, y00, z0, 1, 0, 0, u0, v0,
																   (float)tex1, (float)tex2, (float)tex3,
																   blend00[0], blend00[1], blend00[2], blend00[3],
																   n00.x, n00.y, n00.z});

					terrainVertices.insert(terrainVertices.end(), {x1, y11, z1, 0, 0, 1, u1, v1,
																   (float)tex1, (float)tex2, (float)tex3,
																   blend11[0], blend11[1], blend11[2], blend11[3],
																   n11.x, n11.y, n11.z});

					terrainVertices.insert(terrainVertices.end(), {x1, y10, z0, 0, 1, 0, u1, v0,
																   (float)tex1, (float)tex2, (float)tex3,
																   blend10[0], blend10[1], blend10[2], blend10[3],
																   n10.x, n10.y, n10.z});
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

	// 5. load texture(s)
	int textureCount = (int)textureNames.size();
	std::vector<unsigned char *> pixelData(textureCount);
	int w, h, c, nrChannels = 0;

	for (int i = 0; i < textureCount; ++i)
	{
		pixelData[i] = stbi_load(textureNames[i].c_str(), &w, &h, &c, 4);

		if (!pixelData[i])
		{
			std::cout << "Failed to load texture: " << textureNames[i] << "\n";
			continue;
		}
	}

	// 2) Create the texture‐array
	glGenTextures(1, &textureMap);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textureMap);

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
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// 5) Generate mipmaps (optional if you allocated >1 level)
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

	// 6) Free CPU image data
	for (auto ptr : pixelData)
		if (ptr)
			stbi_image_free(ptr);

	// 5. build the physics mesh vertex data in world space coordinates
	getEntitiesVertices(fillCount);

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

//! Returns a single vector partitioned into [0…fillVerts) = triangles, [fillVerts…end) = lines.
void Terrain::getEntitiesVertices(int &outFillVertexCount)
{
	std::vector<float> allVertices;
	// first, accumulate *only* triangles
	for (int i = 0; i < tilesX; i++)
		for (int j = 0; j < tilesZ; j++)
		{
			if (!tiles[i][j] || tiles[i][j]->physicsGeometry.empty())
				continue;
			for (auto *geom : tiles[i][j]->physicsGeometry)
			{
				int type = geom->m_geomType;
				// --- BOX: emit 12 tris (6 faces × 2) ---
				if (type == PHYSICS_GEOM_TYPE_BOX)
				{
					VEC3 &h = geom->m_halfSize;
					VEC3 v[8] = {
						{-h.X, +h.Y, -h.Z}, {+h.X, +h.Y, -h.Z}, {+h.X, -h.Y, -h.Z}, {-h.X, -h.Y, -h.Z}, {-h.X, +h.Y, +h.Z}, {+h.X, +h.Y, +h.Z}, {+h.X, -h.Y, +h.Z}, {-h.X, -h.Y, +h.Z}};
					for (auto &vv : v)
						geom->m_absTransform.transformVect(vv);
					// correct faces: front, back, left, right, top, bottom
					int F[6][4] = {
						{0, 1, 2, 3}, {5, 4, 7, 6}, {0, 3, 7, 4}, {1, 5, 6, 2}, {0, 4, 5, 1}, {3, 2, 6, 7}};
					for (int f = 0; f < 6; ++f)
					{
						int a = F[f][0], b = F[f][1], c = F[f][2], d = F[f][3];
						// tri1 (a,b,c), tri2 (a,c,d)
						allVertices.insert(allVertices.end(), {v[a].X, v[a].Y, v[a].Z, v[b].X, v[b].Y, v[b].Z, v[c].X, v[c].Y, v[c].Z,
															   v[a].X, v[a].Y, v[a].Z, v[c].X, v[c].Y, v[c].Z, v[d].X, v[d].Y, v[d].Z});
					}
				}
				// --- CYLINDER (unchanged) ---
				else if (type == PHYSICS_GEOM_TYPE_CYLINDER)
				{
					const int CUT_NUM = 16;
					const float pi = 3.14159265359f;
					float angle_step = 2.0f * pi / CUT_NUM;
					float radius = geom->m_halfSize.X;
					float height = geom->m_halfSize.Y;

					int myoffset = 0.5 * radius;

					// Local center points
					VEC3 centerBottom(myoffset, -height, -myoffset);
					VEC3 centerTop(myoffset, height, -myoffset);
					geom->m_absTransform.transformVect(centerBottom);
					geom->m_absTransform.transformVect(centerTop);

					for (int s = 0; s < CUT_NUM; s++)
					{
						float angle0 = s * angle_step;
						float angle1 = (s + 1) * angle_step;

						float x0 = radius * cosf(angle0) + myoffset, z0 = radius * sinf(angle0) - myoffset;
						float x1 = radius * cosf(angle1) + myoffset, z1 = radius * sinf(angle1) - myoffset;

						// Local space points
						VEC3 b0(x0, -height, z0);
						VEC3 b1(x1, -height, z1);
						VEC3 t0(x0, +height, z0);
						VEC3 t1(x1, +height, z1);

						// Transform to world
						geom->m_absTransform.transformVect(b0);
						geom->m_absTransform.transformVect(b1);
						geom->m_absTransform.transformVect(t0);
						geom->m_absTransform.transformVect(t1);

						// --- Bottom Cap (CCW when viewed from below)
						allVertices.insert(allVertices.end(), {b1.X, b1.Y, b1.Z,
															   b0.X, b0.Y, b0.Z,
															   centerBottom.X, centerBottom.Y, centerBottom.Z});

						// --- Top Cap (CCW when viewed from above)
						allVertices.insert(allVertices.end(), {t0.X, t0.Y, t0.Z,
															   t1.X, t1.Y, t1.Z,
															   centerTop.X, centerTop.Y, centerTop.Z});

						// --- Side Face (two triangles)
						allVertices.insert(allVertices.end(), {b0.X, b0.Y, b0.Z,
															   t0.X, t0.Y, t0.Z,
															   t1.X, t1.Y, t1.Z});

						allVertices.insert(allVertices.end(), {b0.X, b0.Y, b0.Z,
															   t1.X, t1.Y, t1.Z,
															   b1.X, b1.Y, b1.Z});
					}
				}

				// --- MESH: emit each face as one triangle ---
				else if (type == PHYSICS_GEOM_TYPE_MESH)
				{
					auto *m = geom->m_refMesh;
					if (!m)
						continue;

					const float RENDER_H_OFF = 0.10f;
					int F = m->GetNbFace();
					auto face = m->GetFacePointer();
					auto vert = m->GetVertexPointer();

					for (int f = 0; f < F; ++f)
					{
						int a = face[4 * f];
						int b = face[4 * f + 1];
						int c = face[4 * f + 2];

						// Mirror Z BEFORE transformation
						VEC3 v0(vert[3 * a], vert[3 * a + 1] + RENDER_H_OFF, -vert[3 * a + 2]);
						VEC3 v1(vert[3 * b], vert[3 * b + 1] + RENDER_H_OFF, -vert[3 * b + 2]);
						VEC3 v2(vert[3 * c], vert[3 * c + 1] + RENDER_H_OFF, -vert[3 * c + 2]);

						geom->m_absTransform.transformVect(v0);
						geom->m_absTransform.transformVect(v1);
						geom->m_absTransform.transformVect(v2);

						// Fix winding: v0 → v2 → v1 instead of v0 → v1 → v2
						allVertices.insert(allVertices.end(), {v0.X, v0.Y, v0.Z,
															   v2.X, v2.Y, v2.Z,
															   v1.X, v1.Y, v1.Z});
					}
				}
			}
		}

	// mark where the triangle section ends
	outFillVertexCount = (int)allVertices.size();

	for (int i = 0; i < tilesX; i++)
		for (int j = 0; j < tilesZ; j++)
		{
			if (!tiles[i][j] || tiles[i][j]->physicsGeometry.empty())
				continue;
			for (auto *geom : tiles[i][j]->physicsGeometry)
			{
				int type = geom->m_geomType;
				// BOX edges
				if (type == PHYSICS_GEOM_TYPE_BOX)
				{
					VEC3 &h = geom->m_halfSize;
					VEC3 v[8] = {/* same 8 vertices */};
					for (auto &vv : v)
						geom->m_absTransform.transformVect(vv);
					int E[12][2] = {
						{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
					for (auto &e : E)
					{
						allVertices.insert(allVertices.end(), {v[e[0]].X, v[e[0]].Y, v[e[0]].Z,
															   v[e[1]].X, v[e[1]].Y, v[e[1]].Z});
					}
				}
				// CYLINDER edges
				else if (type == PHYSICS_GEOM_TYPE_CYLINDER)
				{
					const int CUT_NUM = 16;
					const float pi = 3.14159265359f;
					float angle_step = 2.0f * pi / CUT_NUM;
					float radius = geom->m_halfSize.X;
					float height = geom->m_halfSize.Y;

					int myoffset = 0.5 * radius;

					// Local center points
					VEC3 centerBottom(myoffset, -height, -myoffset);
					VEC3 centerTop(myoffset, height, -myoffset);
					geom->m_absTransform.transformVect(centerBottom);
					geom->m_absTransform.transformVect(centerTop);

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

						geom->m_absTransform.transformVect(b0);
						geom->m_absTransform.transformVect(t0);
						geom->m_absTransform.transformVect(b1);
						geom->m_absTransform.transformVect(t1);

						// --- Bottom Cap (CCW from below)
						allVertices.insert(allVertices.end(), {b1.X, b1.Y, b1.Z,
															   b0.X, b0.Y, b0.Z,
															   centerBottom.X, centerBottom.Y, centerBottom.Z});

						allVertices.insert(allVertices.end(), {b0.X, b0.Y, b0.Z,
															   t1.X, t1.Y, t1.Z,
															   b1.X, b1.Y, b1.Z});

						// --- Side Face
						allVertices.insert(allVertices.end(), {b0.X, b0.Y, b0.Z,
															   t0.X, t0.Y, t0.Z,
															   t1.X, t1.Y, t1.Z});

						// --- Top Cap (CCW from above)
						allVertices.insert(allVertices.end(), {t0.X, t0.Y, t0.Z,
															   t1.X, t1.Y, t1.Z,
															   centerTop.X, centerTop.Y, centerTop.Z});
					}
				}
				// MESH edges
				else if (type == PHYSICS_GEOM_TYPE_MESH)
				{
					auto *m = geom->m_refMesh;
					if (!m)
						continue;

					const float RENDER_H_OFF = 0.10f;
					int F = m->GetNbFace();
					auto face = m->GetFacePointer();
					auto vert = m->GetVertexPointer();

					for (int f = 0; f < F; ++f)
					{
						int a = face[4 * f];
						int b = face[4 * f + 1];
						int c = face[4 * f + 2];

						// Mirror Z by negating Z value
						VEC3 v0(vert[3 * a], vert[3 * a + 1] + RENDER_H_OFF, -vert[3 * a + 2]);
						VEC3 v1(vert[3 * b], vert[3 * b + 1] + RENDER_H_OFF, -vert[3 * b + 2]);
						VEC3 v2(vert[3 * c], vert[3 * c + 1] + RENDER_H_OFF, -vert[3 * c + 2]);

						// Apply transform
						geom->m_absTransform.transformVect(v0);
						geom->m_absTransform.transformVect(v1);
						geom->m_absTransform.transformVect(v2);

						// Fix winding after mirroring: v0 → v2 → v1
						allVertices.insert(allVertices.end(), {v0.X, v0.Y, v0.Z, v2.X, v2.Y, v2.Z,
															   v2.X, v2.Y, v2.Z, v1.X, v1.Y, v1.Z,
															   v1.X, v1.Y, v1.Z, v0.X, v0.Y, v0.Z});
					}
				}
			}
		}

	physicsVertices = allVertices;

	// 5. build the physics mesh vertex data in world space coordinates
	glGenVertexArrays(1, &phyVAO);
	glGenBuffers(1, &phyVBO);
	glBindVertexArray(phyVAO);
	glBindBuffer(GL_ARRAY_BUFFER, phyVBO);
	glBufferData(GL_ARRAY_BUFFER, physicsVertices.size() * sizeof(float), physicsVertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
}

//! Clears GPU memory and resets viewer state.
void Terrain::reset()
{
	tileMinX = tileMinZ = 1000;
	tileMaxX = tileMaxZ = -1000;
	tilesX = tilesZ = 0;
	fileSize = vertexCount = faceCount = modelCount = 0;

	glDeleteVertexArrays(1, &trnVAO);
	glDeleteVertexArrays(1, &phyVAO);
	glDeleteBuffers(1, &trnVBO);
	glDeleteBuffers(1, &phyVBO);

	trnVAO = trnVBO = phyVAO = phyVBO = 0;

	if (!textureNames.empty())
	{
		glDeleteTextures(1, &textureMap);
		textureNames.clear();
	}

	for (auto &column : tiles)
		for (TileTerrain *tile : column)
			delete tile;

	tiles.clear();

	terrainVertices.clear();
	physicsVertices.clear();
	sounds.clear();

	terrainLoaded = false;
}

//! Renders terrain and physics geometry meshes.
void Terrain::draw(glm::mat4 view, glm::mat4 projection, bool simple)
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

	glBindVertexArray(trnVAO);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textureMap);

	if (!simple)
	{
		shader.setInt("renderMode", 1);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDrawArrays(GL_TRIANGLES, 0, terrainVertices.size() / 6);
	}
	else
	{
		shader.setInt("renderMode", 3);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDrawArrays(GL_TRIANGLES, 0, terrainVertices.size() / 6);
	}

	// glBindVertexArray(phyVAO);

	// shader.setInt("renderMode", 4);
	// glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	// glDrawArrays(GL_TRIANGLES, 0, fillCount / 3);

	// shader.setInt("renderMode", 2);
	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	// glDrawArrays(GL_LINES, fillCount / 3, (physicsVertices.size() - fillCount) / 3);

	for (int i = 0; i < tilesX; i++)
		for (int j = 0; j < tilesZ; j++)
		{
			if (!tiles[i][j])
				continue;

			int n = tiles[i][j]->models.size();

			for (int k = 0, n = tiles[i][j]->models.size(); k < n; k++)
				tiles[i][j]->models[k]->draw(view, projection, camera.Position, light.showLighting, true);

			// if (tiles[i][j]->models.size() > 0)
			// {
			//     // std::cout << "Rendering " << tiles[i][j]->models[0]->fileName << std::endl;
			//     for (int k = 0)
			//     tiles[i][j]->models[0]->draw(view, projection, camera.Position, simple);
			// }
		}

	glBindVertexArray(0);
}
