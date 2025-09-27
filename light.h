#ifndef LIGHT_H
#define LIGHT_H

#include "libs/glad/glad.h"
#include "libs/glm/glm.hpp"
#include "shader.h"

// lighting settings
const glm::vec3 lightPos(-20.0f, 0.0f, 0.0f); // world-space position of the light source
const glm::vec3 lightColor(1.0f);			  // white color
const float ambientStrength = 0.5f;
const float diffuseStrength = 0.6f;
const float specularStrength = 0.5f;
const float waterAmbientStrength = 0.2f;
const float waterDiffuseStrength = 0.5f;
const float waterSpecularStrength = 0.5f;

class Light
{
  public:
	Shader shader;
	unsigned int VAO, VBO;
	unsigned int texture;
	bool showLighting;

	Light()
		: shader("shaders/lightcube.vs", "shaders/lightcube.fs"),
		  showLighting(true)
	{
		shader.use();
		shader.setMat4("model", glm::translate(glm::mat4(1.0f), lightPos));
		shader.setVec3("lightColor", lightColor);

		float vertices[] = {
			-0.5f, -0.5f, -0.5f,
			0.5f, -0.5f, -0.5f,
			0.5f, 0.5f, -0.5f,
			0.5f, 0.5f, -0.5f,
			-0.5f, 0.5f, -0.5f,
			-0.5f, -0.5f, -0.5f,

			-0.5f, -0.5f, 0.5f,
			0.5f, -0.5f, 0.5f,
			0.5f, 0.5f, 0.5f,
			0.5f, 0.5f, 0.5f,
			-0.5f, 0.5f, 0.5f,
			-0.5f, -0.5f, 0.5f,

			-0.5f, 0.5f, 0.5f,
			-0.5f, 0.5f, -0.5f,
			-0.5f, -0.5f, -0.5f,
			-0.5f, -0.5f, -0.5f,
			-0.5f, -0.5f, 0.5f,
			-0.5f, 0.5f, 0.5f,

			0.5f, 0.5f, 0.5f,
			0.5f, 0.5f, -0.5f,
			0.5f, -0.5f, -0.5f,
			0.5f, -0.5f, -0.5f,
			0.5f, -0.5f, 0.5f,
			0.5f, 0.5f, 0.5f,

			-0.5f, -0.5f, -0.5f,
			0.5f, -0.5f, -0.5f,
			0.5f, -0.5f, 0.5f,
			0.5f, -0.5f, 0.5f,
			-0.5f, -0.5f, 0.5f,
			-0.5f, -0.5f, -0.5f,

			-0.5f, 0.5f, -0.5f,
			0.5f, 0.5f, -0.5f,
			0.5f, 0.5f, 0.5f,
			0.5f, 0.5f, 0.5f,
			-0.5f, 0.5f, 0.5f,
			-0.5f, 0.5f, -0.5f};

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
	}

	void draw(glm::mat4 view, glm::mat4 projection)
	{
		if (!showLighting)
			return;

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
	}
};

#endif
