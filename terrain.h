#ifndef PARSER_TERRAIN_H
#define PARSER_TERRAIN_H

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
#include "model.h"
#include "CZipResReader.h"
#include "DetourNavMesh.h"

class TileTerrain;

// Class for loading and rendering terrain.
// ________________________________________

class Terrain
{
  public:
	Shader shader;
	Camera &camera;
	Light &light;
	Model sky;
	Model hill;
	std::string fileName;
	int fileSize, vertexCount, faceCount, modelCount;
	std::vector<std::string> sounds;
	std::vector<std::vector<TileTerrain *>> tiles; // 2D grid of terrain tiles stored as pointers (represents all terrain data)
	std::vector<TileTerrain *> tilesVisible;	   // list of terrain tiles that will be rendered, updates each frame
	float minX, minZ, maxX, maxZ;				   // terrain borders in world space coordinates
	int tileMinX, tileMinZ, tileMaxX, tileMaxZ;	   // terrain borders in tile numbers (indices)
	int tilesX, tilesZ;							   // terrain size in tiles
	bool terrainLoaded;

	std::vector<std::string> uniqueTextureNames; // global unique texture names for terrain surface

	Terrain(Camera &cam, Light &light)
		: shader("shaders/terrain.vs", "shaders/terrain.fs"),
		  sky("shaders/skybox.vs", "shaders/skybox.fs"),
		  hill("shaders/skybox.vs", "shaders/skybox.fs"),
		  camera(cam),
		  light(light),
		  vertexCount(0), faceCount(0), modelCount(0),
		  tileMinX(-1), tileMinZ(-1),
		  tileMaxX(1), tileMaxZ(1),
		  terrainLoaded(false)
	{
		shader.use();
		shader.setVec3("lightColor", lightColor);
		shader.setFloat("ambientStrength", ambientStrength);
		shader.setFloat("diffuseStrength", diffuseStrength);
		shader.setFloat("specularStrength", specularStrength);
		shader.setInt("baseTextureArray", 0);
		shader.setInt("maskTexture", 1);
	};

	~Terrain() { reset(); }

	//! CPU-side map loading (called once on map startup, pre-loads all tiles for selected map): opens resource archives, calls parsers for each asset type and each map's tile, then builds vertex and index data.
	void load(const char *fpath, Sound &sound);

	//! Processes .msk and .shw files for a terrain tile and packs all 3 mask layers in 1 texture where each channel encodes the whole layer (R → primary mask, G → secondary mask, B → pre-rendered shadows).
	void loadTileMasks(CZipResReader *masksArchive, int gridX, int gridZ, TileTerrain *tile);

	//! Processes a single .nav file of a terrain tile and adds its data to the Detour navigation system.
	void loadTileNavigation(CZipResReader *navigationArchive, dtNavMesh *navMesh, int gridX, int gridZ);

	//! Builds terrain surface vertex data for each square unit and loads textures (terrain is rendered per square unit, however some data is defined per chunk or even per tile, so it must be mapped to square units).
	void getTerrainVertices();

	//! Builds flat water surface vertex data for each terrain chunk that contains water (water is defined and rendered per chunk, not per unit).
	void getWaterVertices();

	// void getPhysicsVertices();

	// void getNavigationVertices(dtNavMesh *navMesh);

	//! Uploads tile to GPU (GPU-side tile loading, called per-frame for all tiles that need to be activated).
	void activateTile(TileTerrain *tile);

	//! Releases tile from GPU.
	void deactivateTile(TileTerrain *tile);

	//! Computes which tiles will be rendered in the current frame based on camera position and orientation (distance-based culling + frustum culling).
	void updateVisibleTiles(glm::mat4 view, glm::mat4 projection);

	//! Clears CPU memory (resets viewer state).
	void reset();

	//! Renders terrain (.trn + .phy + .nav + .bdae).
	void draw(glm::mat4 view, glm::mat4 projection, bool simple, bool renderNavMesh, bool renderPhysics, float dt);
};

#endif
