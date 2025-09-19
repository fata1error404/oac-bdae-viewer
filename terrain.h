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
#include "parserBDAE.h"
#include "CZipResReader.h"
#include "DetourNavMesh.h"

const int visibleRadiusTiles = 5; // 3 tiles each direction -> (2*2+1)^2 = 25 tiles

class TileTerrain;

// Class for loading terrain.
// __________________________

class Terrain
{
  public:
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

	dtNavMesh *navMesh;

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

	void getTerrainVertices();

	void getNavigationVertices();

	void getPhysicsVertices();

	void getWaterVertices();

	void loadTileNavigation(CZipResReader *navigationArchive, int gridX, int gridZ);

	void updateVisibleTiles(glm::mat4 view, glm::mat4 projection);

	//! Clears GPU memory and resets viewer state.
	void reset();

	//! Renders terrain and physics geometry meshes.
	void draw(glm::mat4 view, glm::mat4 projection, bool simple, bool renderNavMesh, float dt);
};

#endif
