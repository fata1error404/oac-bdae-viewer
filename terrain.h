#ifndef PARSER_TERRAIN_H
#define PARSER_TERRAIN_H

#include <future>
#include <string>
#include <vector>
#include "libs/glm/glm.hpp"
#include "shader.h"
#include "camera.h"
#include "sound.h"
#include "light.h"
#include "libs/glm/fwd.hpp"
#include "libs/glm/gtc/type_ptr.hpp"
#include "libs/glm/gtc/constants.hpp"
#include "libs/glm/gtc/packing.hpp"
#include "libs/glm/ext/vector_uint4.hpp"
#include "libs/glm/gtc/type_precision.hpp"
#include "parserBDAE.h"
#include "CZipResReader.h"
#include "DetourNavMesh.h"

#include <unordered_map>
#include <unordered_set>

const int visibleRadiusTiles = 4; // 3 tiles each direction -> (2*2+1)^2 = 25 tiles

class TileTerrain;

// Class for loading terrain.
// __________________________

class Terrain
{
  public:
	struct PendingTileLoad
	{
		std::future<TileTerrain *> fut;
		int ix, iz;
		int gx, gz;

		PendingTileLoad(std::future<TileTerrain *> &&f, int ix_, int iz_, int gx_, int gz_)
			: fut(std::move(f)), ix(ix_), iz(iz_), gx(gx_), gz(gz_)
		{
		}
	};

	Shader shader;
	Camera &camera;
	Light &light;
	Model skybox;
	std::string fileName;
	int fileSize, vertexCount, faceCount, modelCount;
	std::vector<std::string> sounds;
	std::vector<std::vector<TileTerrain *>> tiles; // 2D grid of terrain tiles stored as pointers (represents all terrain data)
	float minX, minZ, maxX, maxZ;				   // terrain borders in world space coordinates
	int tileMinX, tileMinZ, tileMaxX, tileMaxZ;	   // terrain borders in tile numbers (indices)
	int tilesX, tilesZ;							   // terrain size in tiles
	std::vector<TileTerrain *> visibleTiles;	   // reused each frame
	bool terrainLoaded;

	std::unordered_map<long long, int> trnIndexByCoord;

	dtNavMesh *navMesh;

	// new members for streaming
	CZipResReader *terrainArchive = nullptr;
	CZipResReader *itemsArchive = nullptr;
	CZipResReader *navigationArchive = nullptr;
	CZipResReader *physicsArchive = nullptr;

	// mapping from tile coords -> index in archive (so we can find a tile file fast) // key = (tileX<<32) | (tileZ & 0xffffffff)

	// a small queue for tiles to load; coordinates are global tile coordinates
	std::vector<PendingTileLoad> pendingTileLoads;
	std::unordered_set<long long> pendingTileKeys;

	// control how many tiles we allow to load per frame to reduce stalls
	int maxTilesToLoadPerFrame = 2;

	// load a single tile from the opened archives and prepare its GL buffers
	std::future<TileTerrain *> loadTileAsync(int gx, int gz);

	// unload a single tile (free GL and CPU memory)
	void unloadTileAt(int indexX, int indexZ);

	// per-tile vertex builders (factored out from the monolithic builders)
	enum class TileLoadState : uint8_t
	{
		Unloaded = 0,
		Loading = 1,
		Ready = 2,
		Unloading = 3
	};
	std::vector<std::vector<TileLoadState>> tileState;
	std::vector<std::pair<int, int>> pendingTileUnloads;

	Terrain(Camera &cam, Light &light)
		: shader("shaders/terrain.vs", "shaders/terrain.fs"),
		  skybox("shaders/skybox.vs", "shaders/skybox.fs"),
		  camera(cam),
		  light(light),
		  vertexCount(0), faceCount(0), modelCount(0),
		  tileMinX(-1), tileMinZ(-1),
		  tileMaxX(1), tileMaxZ(1),
		  terrainLoaded(false),
		  navMesh(NULL)
	{
		shader.use();
		shader.setVec3("lightColor", lightColor);
		shader.setFloat("ambientStrength", ambientStrength);
		shader.setFloat("diffuseStrength", diffuseStrength);
		shader.setFloat("specularStrength", specularStrength);
	};

	~Terrain() { reset(); }

	//! Loads .zip file from disk, performs parsing for each contained .trn file, sets up terrain mesh data and sound.
	void load(const char *fpath, Sound &sound);

	void getTerrainVertices(TileTerrain *tile);

	void getNavigationVertices(TileTerrain *tile);

	void getPhysicsVertices(TileTerrain *tile);

	void getWaterVertices(TileTerrain *tile);

	void loadTileNavigation(CZipResReader *navigationArchive, int gridX, int gridZ);

	void updateVisibleTiles(glm::mat4 view, glm::mat4 projection);

	bool peekTileCoordsFromTrn(IReadResFile *trnFile, int &gridX, int &gridZ);

	//! Clears GPU memory and resets viewer state.
	void reset();

	//! Renders terrain and physics geometry meshes.
	void draw(glm::mat4 view, glm::mat4 projection, bool simple, bool renderNavMesh, float dt);
};

#endif
