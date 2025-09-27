#ifndef WATER_H
#define WATER_H

#include <vector>
#include "shader.h"
#include "light.h"
#include "libs/glad/glad.h"
#include "libs/glm/glm.hpp"
#include "libs/stb_image.h"

const float waterTextureSpeed = 0.5f;
const float waterTextureScale = 0.8f;

// Class for loading water.
// ________________________

class Water
{
  public:
	Shader shader;
	unsigned int VAO, VBO;
	unsigned int texture;
	unsigned int waterVertexCount;
	std::vector<float> vertices;
	float waterOffset;

	Water()
		: shader("shaders/water.vs", "shaders/water.fs"),
		  waterOffset(0.0f),
		  waterVertexCount(0),
		  VAO(0), VBO(0),
		  texture(0)
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

	~Water() { release(); }

	void release()
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteTextures(1, &texture);
		VAO = VBO = texture = waterVertexCount = 0;
		vertices.clear();
	}

	void draw(glm::mat4 view, glm::mat4 projection, bool lighting, bool simple, float dt, glm::vec3 camera)
	{
		if (vertices.empty())
			return;
		if (VAO == 0 || VBO == 0)
			return;
		if (waterVertexCount == 0)
			return;

		shader.use();
		shader.setMat4("model", glm::mat4(1.0f));
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setBool("lighting", lighting);
		shader.setVec3("cameraPos", camera);

		waterOffset += waterTextureSpeed * dt;
		shader.setFloat("textureOffset", waterOffset);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);

		glBindVertexArray(VAO);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDrawArrays(GL_TRIANGLES, 0, waterVertexCount);
		glBindVertexArray(0);
	}
};

#endif
