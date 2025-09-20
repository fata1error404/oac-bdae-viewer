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

static inline long long makeTileKey(int tx, int tz)
{
	return (((long long)tx) << 32) | ((long long)((unsigned)tz) & 0xffffffffULL);
}

bool Terrain::peekTileCoordsFromTrn(IReadResFile *trnFile, int &gridX, int &gridZ)
{
	if (!trnFile)
		return false;

	// ensure we read from the start
	trnFile->seek(0);

	int size = trnFile->getSize();
	if (size < (int)sizeof(TRNFileHeader))
		return false; // file too small

	// read header bytes only
	unsigned char headerBuf[sizeof(TRNFileHeader)];
	trnFile->read(headerBuf, sizeof(TRNFileHeader));

	// copy into typed struct to avoid strict-aliasing/unaligned access
	TRNFileHeader header;
	std::memcpy(&header, headerBuf, sizeof(TRNFileHeader));

	// optional: verify signature (helps avoid false positives)
	// signature expected to be 'ATIL' according to TRNFileHeader comment
	const char expectedSig[4] = {'A', 'T', 'I', 'L'};
	if (std::memcmp(header.signature, expectedSig, 4) != 0)
	{
		// signature mismatch -> still possible older/newer format, but reject by default
		return false;
	}

	gridX = header.gridX;
	gridZ = header.gridZ;

	return true;
}

void Terrain::load(const char *fpath, Sound &sound)
{
	reset();

	// keep the archive readers open for later on-demand loads
	terrainArchive = new CZipResReader(fpath, true, false);

	// construct related archive names
	std::string itemsPath = std::string(fpath);
	itemsPath.replace(itemsPath.size() - 4, 4, ".itm");
	itemsArchive = new CZipResReader(itemsPath.c_str(), true, false);

	std::string navPath = std::string(fpath);
	navPath.replace(navPath.size() - 4, 4, ".nav");
	navigationArchive = new CZipResReader(navPath.c_str(), true, false);

	physicsArchive = new CZipResReader("data/terrain/physics.zip", true, false);

	// scan archive to discover tile bounds and build trnIndexByCoord
	int fileCount = terrainArchive->getFileCount();
	trnIndexByCoord.clear();

	tileMinX = tileMinZ = 100000;
	tileMaxX = tileMaxZ = -100000;

	for (int i = 0; i < fileCount; ++i)
	{
		IReadResFile *f = terrainArchive->openFile(i);

		if (!f)
			continue;

		int tx = 0, tz = 0;
		bool ok = false;

		// Try to peek tile coords cheaply (implement this helper for your .trn).
		if (peekTileCoordsFromTrn(f, tx, tz))
			ok = true;

		f->drop();

		if (ok)
		{
			// update bounds
			tileMinX = std::min(tileMinX, tx);
			tileMaxX = std::max(tileMaxX, tx);
			tileMinZ = std::min(tileMinZ, tz);
			tileMaxZ = std::max(tileMaxZ, tz);

			trnIndexByCoord.emplace(makeTileKey(tx, tz), i);
		}
	}

	if (tileMaxX < tileMinX || tileMaxZ < tileMinZ)
	{
		// no tiles discovered — treat as empty
		tilesX = tilesZ = 0;
		terrainLoaded = true;
		sound.searchSoundFiles(std::filesystem::path(fpath).filename().string(), sounds);
		return;
	}

	tilesX = (tileMaxX - tileMinX) + 1;
	tilesZ = (tileMaxZ - tileMinZ) + 1;

	// allocate tiles grid but leave pointers null (we'll fill them on demand)
	tiles.assign(tilesX, std::vector<TileTerrain *>(tilesZ, nullptr));
	tileState.assign(tilesX, std::vector<TileLoadState>(tilesZ, TileLoadState::Unloaded));

	// load sounds only
	sound.searchSoundFiles(std::filesystem::path(fpath).filename().string(), sounds);

	terrainLoaded = true;
}

void Terrain::getTerrainVertices(TileTerrain *tile)
{
	static const int FLOATS_PER_TERR_VERTEX = 18;

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

	// Reserve approx size: UnitsInTileCol * UnitsInTileRow * 6 vertices * floatsPerVertex
	// (6 vertices per unit because 2 triangles × 3 verts)
	size_t approxVerts = (size_t)UnitsInTileCol * (size_t)UnitsInTileRow * FLOATS_PER_TERR_VERTEX;
	tile->terrainVertices.clear();
	tile->terrainVertices.reserve(approxVerts * 6);

	for (int col = 0; col < UnitsInTileCol; ++col)
	{
		for (int row = 0; row < UnitsInTileRow; ++row)
		{
			float x0 = tile->startX + (float)col;
			float x1 = tile->startX + (float)(col + 1);
			float z0 = tile->startZ + (float)row;
			float z1 = tile->startZ + (float)(row + 1);

			float y00 = tile->Y[row][col];
			float y10 = tile->Y[row][col + 1];
			float y01 = tile->Y[row + 1][col];
			float y11 = tile->Y[row + 1][col + 1];

			float u0 = 0.0f, u1 = 1.0f;
			float v0 = 0.0f, v1 = 1.0f;

			int chunkCol = col / 8;
			int chunkRow = row / 8;
			ChunkInfo &chunk = tile->chunks[chunkRow * 8 + chunkCol];
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

			auto blend00 = unpack_blend(tile->colors[row][col]);
			auto blend10 = unpack_blend(tile->colors[row][col + 1]);
			auto blend01 = unpack_blend(tile->colors[row + 1][col]);
			auto blend11 = unpack_blend(tile->colors[row + 1][col + 1]);

			glm::vec3 n00 = tile->normals[row][col];
			glm::vec3 n10 = tile->normals[row][col + 1];
			glm::vec3 n01 = tile->normals[row + 1][col];
			glm::vec3 n11 = tile->normals[row + 1][col + 1];

			// === First Triangle ===
			tile->terrainVertices.insert(tile->terrainVertices.end(), {x0, y00, z0, 1.0f, 0.0f, 0.0f, u0, v0,
																	   (float)tex1, (float)tex2, (float)tex3,
																	   blend00[0], blend00[1], blend00[2], blend00[3],
																	   n00.x, n00.y, n00.z});

			tile->terrainVertices.insert(tile->terrainVertices.end(), {x0, y01, z1, 0.0f, 1.0f, 0.0f, u0, v1,
																	   (float)tex1, (float)tex2, (float)tex3,
																	   blend01[0], blend01[1], blend01[2], blend01[3],
																	   n01.x, n01.y, n01.z});

			tile->terrainVertices.insert(tile->terrainVertices.end(), {x1, y11, z1, 0.0f, 0.0f, 1.0f, u1, v1,
																	   (float)tex1, (float)tex2, (float)tex3,
																	   blend11[0], blend11[1], blend11[2], blend11[3],
																	   n11.x, n11.y, n11.z});

			// === Second Triangle ===
			tile->terrainVertices.insert(tile->terrainVertices.end(), {x0, y00, z0, 1.0f, 0.0f, 0.0f, u0, v0,
																	   (float)tex1, (float)tex2, (float)tex3,
																	   blend00[0], blend00[1], blend00[2], blend00[3],
																	   n00.x, n00.y, n00.z});

			tile->terrainVertices.insert(tile->terrainVertices.end(), {x1, y11, z1, 0.0f, 0.0f, 1.0f, u1, v1,
																	   (float)tex1, (float)tex2, (float)tex3,
																	   blend11[0], blend11[1], blend11[2], blend11[3],
																	   n11.x, n11.y, n11.z});

			tile->terrainVertices.insert(tile->terrainVertices.end(), {x1, y10, z0, 0.0f, 1.0f, 0.0f, u1, v0,
																	   (float)tex1, (float)tex2, (float)tex3,
																	   blend10[0], blend10[1], blend10[2], blend10[3],
																	   n10.x, n10.y, n10.z});
		}
	}

	// compute vertex count (every vertex = 18 floats)
	if (tile->terrainVertices.empty())
		tile->terrainVertexCount = 0;
	else
		tile->terrainVertexCount = (int)(tile->terrainVertices.size() / FLOATS_PER_TERR_VERTEX);

	// Upload to GPU (must be on GL thread/context)
	if (!tile->terrainVertices.empty())
	{
		glGenVertexArrays(1, &tile->trnVAO);
		glGenBuffers(1, &tile->trnVBO);

		glBindVertexArray(tile->trnVAO);
		glBindBuffer(GL_ARRAY_BUFFER, tile->trnVBO);
		GLsizeiptr bufSize = (GLsizeiptr)(tile->terrainVertices.size() * sizeof(float));
		glBufferData(GL_ARRAY_BUFFER, bufSize, tile->terrainVertices.data(), GL_STATIC_DRAW);

		// attribute layout (same as original)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_TERR_VERTEX * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_TERR_VERTEX * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_TERR_VERTEX * sizeof(float), (void *)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_TERR_VERTEX * sizeof(float), (void *)(8 * sizeof(float)));
		glEnableVertexAttribArray(3);

		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, FLOATS_PER_TERR_VERTEX * sizeof(float), (void *)(11 * sizeof(float)));
		glEnableVertexAttribArray(4);

		glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_TERR_VERTEX * sizeof(float), (void *)(15 * sizeof(float)));
		glEnableVertexAttribArray(5);

		glBindVertexArray(0);
	}
}

void Terrain::getWaterVertices(TileTerrain *tile)
{
	const float unitsPerChunkX = (float)UnitsInTileRow / ChunksInTileRow;
	const float unitsPerChunkZ = (float)UnitsInTileCol / ChunksInTileCol;
	const float nx = 0.0f, ny = 1.0f, nz = 0.0f;

	if (!tile || !tile->chunks)
		return;

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
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Load Nav Mesh if exists
void Terrain::loadTileNavigation(CZipResReader *navigationArchive, int gridX, int gridZ)
{
	char tmpName[256];

#ifdef BETA_GAME_VERSION
	std::string terrainName = std::filesystem::path(navigationArchive->getZipFileName()).stem().string();
	sprintf(tmpName, "world/%s/navmesh/%04d_%04d.nav", terrainName.c_str(), gridX, gridZ); // path inside the .nav archive
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

// [TODO] annotate
#include <unordered_map>
#include <string>
#include <tuple>
#include <cstdint>
#include <cmath>
#include <sstream>

// Helper: quantize a coordinate to integer to avoid tiny floating differences.
static inline long long quantizeCoord(float v, float scale = 1000.0f)
{
	return llround(v * scale);
}

// Make a canonical undirected edge key based on quantized endpoints.
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

void Terrain::getNavigationVertices(TileTerrain *tile)
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

		// Temporary per-detour-tile buffers
		std::vector<float> tileNavVerts;
		tileNavVerts.reserve(polyCount * 9 * 2); // rough reserve: 3 verts per tri, 3 floats per vert

		int triVertexCounter = 0;

		// per-tile edge dedup map
		std::unordered_map<std::string, EdgeInfo> edgeMap;
		edgeMap.reserve(512);

		// Scan polygons in this detour tile
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
				tile->navigationVertices = std::move(tileNavVerts);
				tile->navmeshVertexCount = triVertexCounter;

				// upload GL buffers for this tile
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
			else
			{
				// Tile not present in memory: we still generated tileNavVerts (since navMesh had the tile)
				// Optionally: keep a small cache / discard. Here we simply drop it.
			}
		}
		else
		{
			// header x/y outside bounds of loaded tiles or tileMin/tileMax not set yet.
			// We can drop tileNavVerts or store elsewhere if desired.
		}
	}

	if (navMesh)
	{
		delete navMesh;
		navMesh = NULL;
	}
}

//! Returns a single vector partitioned into [0…fillVerts) = triangles, [fillVerts…end) = lines.
void Terrain::getPhysicsVertices(TileTerrain *tile)
{
	// first, accumulate *only* triangles
	if (!tile)
		return;

	for (Physics *geom : tile->physicsGeometry)
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
				tile->physicsVertices.insert(tile->physicsVertices.end(), {v[a].X, v[a].Y, v[a].Z, v[b].X, v[b].Y, v[b].Z, v[c].X, v[c].Y, v[c].Z,
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
				tile->physicsVertices.insert(tile->physicsVertices.end(), {b1.X, b1.Y, b1.Z,
																		   b0.X, b0.Y, b0.Z,
																		   centerBottom.X, centerBottom.Y, centerBottom.Z});

				// --- Top Cap (CCW when viewed from above)
				tile->physicsVertices.insert(tile->physicsVertices.end(), {t0.X, t0.Y, t0.Z,
																		   t1.X, t1.Y, t1.Z,
																		   centerTop.X, centerTop.Y, centerTop.Z});

				// --- Side Face (two triangles)
				tile->physicsVertices.insert(tile->physicsVertices.end(), {b0.X, b0.Y, b0.Z,
																		   t0.X, t0.Y, t0.Z,
																		   t1.X, t1.Y, t1.Z});

				tile->physicsVertices.insert(tile->physicsVertices.end(), {b0.X, b0.Y, b0.Z,
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
				tile->physicsVertices.insert(tile->physicsVertices.end(), {v0.X, v0.Y, v0.Z,
																		   v2.X, v2.Y, v2.Z,
																		   v1.X, v1.Y, v1.Z});
			}
		}
	}

	// mark where the triangle section ends
	tile->physicsVertexCount = tile->physicsVertices.size() / 3;

	if (!tile || tile->physicsGeometry.empty())
		return;

	for (auto *geom : tile->physicsGeometry)
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
				tile->physicsVertices.insert(tile->physicsVertices.end(), {v[e[0]].X, v[e[0]].Y, v[e[0]].Z,
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
				tile->physicsVertices.insert(tile->physicsVertices.end(), {b1.X, b1.Y, b1.Z,
																		   b0.X, b0.Y, b0.Z,
																		   centerBottom.X, centerBottom.Y, centerBottom.Z});

				tile->physicsVertices.insert(tile->physicsVertices.end(), {b0.X, b0.Y, b0.Z,
																		   t1.X, t1.Y, t1.Z,
																		   b1.X, b1.Y, b1.Z});

				// --- Side Face
				tile->physicsVertices.insert(tile->physicsVertices.end(), {b0.X, b0.Y, b0.Z,
																		   t0.X, t0.Y, t0.Z,
																		   t1.X, t1.Y, t1.Z});

				// --- Top Cap (CCW from above)
				tile->physicsVertices.insert(tile->physicsVertices.end(), {t0.X, t0.Y, t0.Z,
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
				tile->physicsVertices.insert(tile->physicsVertices.end(), {v0.X, v0.Y, v0.Z, v2.X, v2.Y, v2.Z,
																		   v2.X, v2.Y, v2.Z, v1.X, v1.Y, v1.Z,
																		   v1.X, v1.Y, v1.Z, v0.X, v0.Y, v0.Z});
			}
		}
	}

	glGenVertexArrays(1, &tile->phyVAO);
	glGenBuffers(1, &tile->phyVBO);
	glBindVertexArray(tile->phyVAO);
	glBindBuffer(GL_ARRAY_BUFFER, tile->phyVBO);
	glBufferData(GL_ARRAY_BUFFER, tile->physicsVertices.size() * sizeof(float), tile->physicsVertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

#include <future>

// Change loadTileAt to an async-friendly version
// Async tile loader

void Terrain::unloadTileAt(int ix, int iz)
{
	if (!tiles[ix][iz])
	{
		tileState[ix][iz] = TileLoadState::Unloaded;
		return;
	}
	TileTerrain *tile = tiles[ix][iz];
	// unbind if currently bound
	GLint curVAO = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &curVAO);
	if ((GLuint)curVAO == tile->trnVAO)
		glBindVertexArray(0);

	// delete GL objects and models/physics
	delete tile;
	tiles[ix][iz] = nullptr;
	tileState[ix][iz] = TileLoadState::Unloaded;
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
			if (tile)
				delete tile;

	tiles.clear();
	sounds.clear();

	trnIndexByCoord.clear();
	pendingTileLoads.clear();

	if (terrainArchive)
	{
		delete terrainArchive;
		terrainArchive = NULL;
	}

	if (itemsArchive)
	{
		delete itemsArchive;
		itemsArchive = NULL;
	}

	if (navigationArchive)
	{
		delete navigationArchive;
		navigationArchive = NULL;
	}

	if (physicsArchive)
	{
		delete physicsArchive;
		physicsArchive = NULL;
	}
}

std::future<TileTerrain *> Terrain::loadTileAsync(int gx, int gz)
{
	return std::async(std::launch::async, [this, gx, gz]() -> TileTerrain *
					  {
						  int ix = gx - tileMinX;
						  int iz = gz - tileMinZ;

						  if (tiles[ix][iz] != nullptr)
							  return tiles[ix][iz];

						  long long key = makeTileKey(gx, gz);
						  auto it = trnIndexByCoord.find(key);
						  if (it == trnIndexByCoord.end())
							  return nullptr;

						  int fileIndex = it->second;
						  IReadResFile *trnFile = terrainArchive->openFile(fileIndex);
						  if (!trnFile)
							  return nullptr;

						  int tileX = 0, tileZ = 0;
						  TileTerrain *tile = TileTerrain::load(trnFile, tileX, tileZ, *this);
						  trnFile->drop();
						  if (!tile)
							  return nullptr;

						  // CPU-heavy setup
						  loadTileEntities(itemsArchive, physicsArchive, tileX, tileZ, tile, *this);
						  getTerrainVertices(tile);
						  getNavigationVertices(tile);
						  getPhysicsVertices(tile);
						  getWaterVertices(tile);

						  return tile; // main thread will mark Ready and assign to tiles[ix][iz]
					  });
}

// call: terrain.updateVisibleTiles(camera, projection);
// cheap per-frame visible tile selection (square radius) + frustum culling
// Replace your existing updateVisibleTiles with this distance-prioritized version.
void Terrain::updateVisibleTiles(glm::mat4 view, glm::mat4 projection)
{
	if (!terrainLoaded || tilesX == 0 || tilesZ == 0)
		return;

	const glm::vec3 camPos = camera.Position;
	const float tileSize = static_cast<float>(ChunksInTile);

	const int prefetchTiles = 2;
	const int renderRadiusTiles = visibleRadiusTiles;
	const int unloadRadiusTiles = renderRadiusTiles + prefetchTiles + 1;

	const float renderRadiusW = (float)renderRadiusTiles * tileSize;
	const float unloadRadiusW = (float)unloadRadiusTiles * tileSize;
	const float renderRadiusSq = renderRadiusW * renderRadiusW;
	const float unloadRadiusSq = unloadRadiusW * unloadRadiusW;

	const int camTileX = (int)std::floor(camPos.x / tileSize);
	const int camTileZ = (int)std::floor(camPos.z / tileSize);

	int wantGX0 = std::max(camTileX - unloadRadiusTiles, tileMinX);
	int wantGZ0 = std::max(camTileZ - unloadRadiusTiles, tileMinZ);
	int wantGX1 = std::min(camTileX + unloadRadiusTiles, tileMaxX);
	int wantGZ1 = std::min(camTileZ + unloadRadiusTiles, tileMaxZ);

	struct Cand
	{
		int gx, gz;
		int ix, iz;
		float distSq;
	};
	std::vector<Cand> candidates;
	candidates.reserve(trnIndexByCoord.size() / 8 + 16);

	auto extractFromKey = [](long long key) -> std::pair<int, int>
	{
		int gx = (int)(key >> 32);
		int gz = (int)(key & 0xffffffffULL);
		return {gx, gz};
	};

	// --- Collect candidates ---
	for (auto &kv : trnIndexByCoord)
	{
		auto [gx, gz] = extractFromKey(kv.first);
		if (gx < wantGX0 || gx > wantGX1 || gz < wantGZ0 || gz > wantGZ1)
			continue;

		int ix = gx - tileMinX;
		int iz = gz - tileMinZ;
		if (ix < 0 || iz < 0 || ix >= tilesX || iz >= tilesZ)
			continue;
		if (tileState[ix][iz] != TileLoadState::Unloaded || tiles[ix][iz] != nullptr)
			continue;

		float centerX = (gx + 0.5f) * tileSize;
		float centerZ = (gz + 0.5f) * tileSize;
		float dx = camPos.x - centerX;
		float dz = camPos.z - centerZ;
		float dsq = dx * dx + dz * dz;

		if (dsq <= unloadRadiusSq)
			candidates.push_back({gx, gz, ix, iz, dsq});
	}

	std::sort(candidates.begin(), candidates.end(),
			  [](const Cand &a, const Cand &b)
			  { return a.distSq < b.distSq; });

	const int budget = std::max(1, maxTilesToLoadPerFrame);
	int loadsThisFrame = 0;

	// --- Schedule async loads ---
	// Schedule new async loads
	// --- Schedule async loads ---
	for (const Cand &c : candidates)
	{
		if (loadsThisFrame >= budget)
			break;
		if (tileState[c.ix][c.iz] != TileLoadState::Unloaded)
			continue;

		// Skip if already pending
		bool alreadyPending = false;
		for (auto &p : pendingTileLoads)
			if (p.ix == c.ix && p.iz == c.iz)
			{
				alreadyPending = true;
				break;
			}
		if (alreadyPending)
			continue;

		tileState[c.ix][c.iz] = TileLoadState::Loading;

		pendingTileLoads.emplace_back(
			loadTileAsync(c.gx, c.gz),
			c.ix, c.iz, c.gx, c.gz);
		++loadsThisFrame;
	}

	// Collect completed async loads
	// Collect completed async loads and upload GPU
	for (auto it = pendingTileLoads.begin(); it != pendingTileLoads.end();)
	{
		auto &p = *it;
		if (p.fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			TileTerrain *tile = p.fut.get();
			if (tile)
			{
				// Upload GPU resources on main thread
				getTerrainVertices(tile);
				getNavigationVertices(tile);
				getPhysicsVertices(tile);
				getWaterVertices(tile);

				tiles[p.ix][p.iz] = tile;
				tileState[p.ix][p.iz] = TileLoadState::Ready;
			}
			else
			{
				tileState[p.ix][p.iz] = TileLoadState::Unloaded; // retry next frame
			}

			it = pendingTileLoads.erase(it);
		}
		else
		{
			++it;
		}
	}

	// --- Unload pass ---
	int unloadsThisFrame = 0;
	for (int ix = 0; ix < tilesX && unloadsThisFrame < budget; ++ix)
	{
		for (int iz = 0; iz < tilesZ && unloadsThisFrame < budget; ++iz)
		{
			TileTerrain *t = tiles[ix][iz];
			if (!t)
			{
				tileState[ix][iz] = TileLoadState::Unloaded;
				continue;
			}
			if (tileState[ix][iz] != TileLoadState::Ready)
				continue;

			int gx = ix + tileMinX;
			int gz = iz + tileMinZ;
			float centerX = (gx + 0.5f) * tileSize;
			float centerZ = (gz + 0.5f) * tileSize;
			float dx = camPos.x - centerX;
			float dz = camPos.z - centerZ;
			float dsq = dx * dx + dz * dz;

			if (dsq > unloadRadiusSq)
			{
				tileState[ix][iz] = TileLoadState::Unloading;
				unloadTileAt(ix, iz);
				tileState[ix][iz] = TileLoadState::Unloaded;
				++unloadsThisFrame;
			}
		}
	}

	// --- Build visible list ---
	std::vector<TileTerrain *> newVisible;
	newVisible.reserve((renderRadiusTiles * 2 + 1) * (renderRadiusTiles * 2 + 1));

	int vix0 = std::clamp(camTileX - renderRadiusTiles - tileMinX, 0, tilesX - 1);
	int viz0 = std::clamp(camTileZ - renderRadiusTiles - tileMinZ, 0, tilesZ - 1);
	int vix1 = std::clamp(camTileX + renderRadiusTiles - tileMinX, 0, tilesX - 1);
	int viz1 = std::clamp(camTileZ + renderRadiusTiles - tileMinZ, 0, tilesZ - 1);

	for (int ix = vix0; ix <= vix1; ++ix)
	{
		for (int iz = viz1; iz >= viz0; --iz) // обратный порядок
		{
			if (tileState[ix][iz] != TileLoadState::Ready)
				continue;
			TileTerrain *t = tiles[ix][iz];
			if (!t)
			{
				tileState[ix][iz] = TileLoadState::Unloaded;
				continue;
			}

			float centerX = (ix + tileMinX + 0.5f) * tileSize;
			float centerZ = (iz + tileMinZ + 0.5f) * tileSize;
			float dx = camPos.x - centerX;
			float dz = camPos.z - centerZ;
			float dsq = dx * dx + dz * dz;

			if (dsq <= renderRadiusSq)
				newVisible.push_back(t);
		}
	}

	visibleTiles.swap(newVisible);
}

//! Renders terrain and physics geometry meshes.
void Terrain::draw(glm::mat4 view, glm::mat4 projection, bool simple, bool renderNavMesh, float dt)
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

	// render terrain
	if (!simple)
		shader.setInt("renderMode", 1);
	else
		shader.setInt("renderMode", 3);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	for (int i = 0, n = visibleTiles.size(); i < n; i++)
	{
		TileTerrain *tile = visibleTiles[i];

		if (!tile || tile->terrainVertices.empty())
			continue;

		glBindVertexArray(tile->trnVAO);
		glDrawArrays(GL_TRIANGLES, 0, tile->terrainVertexCount);
		glBindVertexArray(0);
	}

	// render walkable surfaces
	if (renderNavMesh)
	{
		shader.setInt("renderMode", 5);

		for (int i = 0, n = visibleTiles.size(); i < n; i++)
		{
			TileTerrain *tile = visibleTiles[i];

			if (!tile || tile->navigationVertices.empty())
				continue;

			glBindVertexArray(tile->navVAO);

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glDrawArrays(GL_TRIANGLES, 0, tile->navmeshVertexCount);

			glBindVertexArray(0);
		}
	}

	// render physics
	for (int i = 0, n = visibleTiles.size(); i < n; i++)
	{
		TileTerrain *tile = visibleTiles[i];

		if (!tile || tile->physicsVertices.empty())
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

	// render water and 3D models
	for (int i = 0, n = visibleTiles.size(); i < n; i++)
	{
		TileTerrain *tile = visibleTiles[i];

		if (!tile)
			continue;

		tile->water.draw(view, projection, light.showLighting, simple, dt, camera.Position);

		for (int k = 0, n = tile->models.size(); k < n; k++)
			tile->models[k]->draw(view, projection, camera.Position, light.showLighting, simple);
	}

	// render skybox
	if (!simple)
	{
		glDepthFunc(GL_LEQUAL);
		skybox.draw(glm::mat4(glm::mat3(view)), projection, camera.Position, false, false);
		glDepthFunc(GL_LESS);
	}

	glBindVertexArray(0);
}
