#ifndef PARSER_TERRAIN_H
#define PARSER_TERRAIN_H

// [TODO] annotate

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

const int visibleRadiusTiles = 4; // 3 tiles each direction -> (2*2+1)^2 = 25 tiles

class TileTerrain;

// Class for loading terrain.
// __________________________

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

	std::vector<std::string> uniqueTextureNames; // global unique texture names

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
	};

	~Terrain() { reset(); }

	//! Loads .zip file from disk, performs parsing for each contained .trn file, sets up terrain mesh data and sound.
	void load(const char *fpath, Sound &sound);

	void getTerrainVertices();

	void getWaterVertices();

	void getPhysicsVertices();

	void getNavigationVertices(dtNavMesh *navMesh);

	void loadTileNavigation(CZipResReader *navigationArchive, dtNavMesh *navMesh, int gridX, int gridZ);

	void updateVisibleTiles(glm::mat4 view, glm::mat4 projection);

	void uploadToGPU(TileTerrain *tile);

	void releaseFromGPU(TileTerrain *tile);

	//! Clears GPU memory and resets viewer state.
	void reset();

	//! Renders terrain (.trn + .phy + .nav + .bdae).
	void draw(glm::mat4 view, glm::mat4 projection, bool simple, bool renderNavMesh, bool renderPhysics, float dt);
};

#endif
