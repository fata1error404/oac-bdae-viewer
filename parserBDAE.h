#ifndef PARSER_BDAE_H
#define PARSER_BDAE_H

#include <string>
#include <vector>
#include "libs/glm/fwd.hpp"
#include "shader.h"
#include "sound.h"
#include "light.h"

const float meshRotationSensitivity = 0.3f;

// Class for loading 3D model.
// ___________________________

class Model
{
public:
    Shader shader;
    std::string fileName;
    int fileSize, vertexCount, faceCount, textureCount, alternativeTextureCount, selectedTexture, totalSubmeshCount;
    unsigned int VAO, VBO;
    std::vector<unsigned int> EBOs;
    std::vector<float> vertices;
    std::vector<std::vector<unsigned short>> indices;
    std::vector<unsigned int> textures;
    std::vector<std::string> sounds;
    glm::vec3 meshCenter;
    glm::mat4 model;

    float meshPitch = 0.0f;
    float meshYaw = 0.0f;
    bool modelLoaded;

    Model()
        : shader("shaders/model.vs", "shaders/model.fs"),
          VAO(0),
          VBO(0),
          modelLoaded(false),
          meshCenter(glm::vec3(-1.0f))
    {
        shader.use();
        shader.setVec3("lightPos", lightPos);
        shader.setVec3("lightColor", lightColor);
        shader.setFloat("ambientStrength", ambientStrength);
        shader.setFloat("diffuseStrength", diffuseStrength);
        shader.setFloat("specularStrength", specularStrength);
    }

    //! For 3D Model Viewer. Loads .bdae file from disk, performs in-memory initialization and parsing, sets up model mesh data, textures and sounds.
    void load(const char *fpath, Sound &sound);

    //! For Terrain Viewer. Loads .bdae file from disk, performs in-memory initialization and parsing, sets up model mesh data and textures (does not search for associated textures and sounds).
    void load(const char *fpath, glm::mat4 model);

    //! Clears GPU memory and resets viewer state.
    void reset();

    //! Renders .bdae model.
    void draw(glm::mat4 view, glm::mat4 projection, glm::vec3 cameraPos, bool lighting, bool simple);
};

#endif
