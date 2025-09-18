#ifndef WATER_H
#define WATER_H

#include <vector>
#include "shader.h"
#include "camera.h"
#include "light.h"
#include "libs/glad/glad.h"
#include "libs/glm/glm.hpp"
#include "libs/stb_image.h"

const float waterTextureSpeed = 0.5f;
const float waterTextureScale = 0.8f;

class Water
{
  public:
	Shader shader;
	Camera &camera;
	unsigned int VAO, VBO;
	unsigned int texture;
	std::vector<float> vertices;
	float waterOffset;

	Water(Camera &cam)
		: camera(cam),
		  shader("shaders/water.vs", "shaders/water.fs"),
		  waterOffset(0.0f)
	{
		shader.use();
		shader.setInt("waterTexture", 0);
		shader.setFloat("textureScale", waterTextureScale);

		shader.setVec3("lightPos", lightPos);
		shader.setVec3("lightColor", lightColor);
		shader.setFloat("ambientStrength", waterAmbientStrength);
		shader.setFloat("diffuseStrength", waterDiffuseStrength);
		shader.setFloat("specularStrength", waterSpecularStrength);

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		int width, height, nrChannels;
		unsigned char *data = stbi_load("data/texture/unsorted/tiles/water.png", &width, &height, &nrChannels, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		stbi_image_free(data);
	}

	void draw(glm::mat4 view, glm::mat4 projection, bool lighting, bool simple, float dt)
	{
		if (vertices.empty())
			return;

		shader.use();
		shader.setMat4("model", glm::mat4(1.0f));
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setBool("lighting", lighting);
		shader.setVec3("cameraPos", camera.Position);

		waterOffset += waterTextureSpeed * dt;
		shader.setFloat("textureOffset", waterOffset);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);

		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);
		glBindVertexArray(0);
	}
};

#endif
