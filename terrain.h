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

class TileTerrain;

// Class for loading terrain.
// __________________________

class Terrain
{
public:
    Shader shader;
    Camera &camera;
    Light &light;
    std::string fileName;
    int fileSize, vertexCount, faceCount, modelCount;
    unsigned int trnVAO, trnVBO, phyVAO, phyVBO;
    std::vector<float> terrainVertices, physicsVertices;
    std::vector<std::string> textureNames;
    unsigned int textureMap;
    std::vector<std::string> sounds;
    std::vector<std::vector<TileTerrain *>> tiles; // 2D grid of terrain tiles stored as pointers (represents all terrain data)
    std::vector<unsigned int> geometries;          //
    float minX, minZ, maxX, maxZ;                  // terrain borders in world space coordinates
    int tileMinX, tileMinZ, tileMaxX, tileMaxZ;    // terrain borders in tile numbers (indices)
    int tilesX, tilesZ;                            // terrain size in tiles
    bool terrainLoaded;

    int fillCount; // NEW

    Terrain(Camera &cam, Light &light)
        : shader("shaders/terrain.vs", "shaders/terrain.fs"),
          camera(cam),
          light(light),
          trnVAO(0),
          trnVBO(0),
          phyVAO(0),
          phyVBO(0),
          tileMinX(-1),
          tileMinZ(-1),
          tileMaxX(1),
          tileMaxZ(1),
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

    //! [TODO] merge with load
    void getEntitiesVertices(int &outFillVertexCount);

    //! Clears GPU memory and resets viewer state.
    void reset();

    //! Renders terrain and physics geometry meshes.
    void draw(glm::mat4 view, glm::mat4 projection, bool simple);
};

#endif
