#include "model.h"

//! Renders .bdae model.
void Model::draw(glm::mat4 model, glm::mat4 view, glm::mat4 projection, glm::vec3 cameraPos, float dt, bool lighting, bool simple)
{
	if (!modelLoaded)
		return;

	if (animationsLoaded && animationPlaying)
	{
		float duration = animations[selectedAnimation].first;
		std::vector<BaseAnimation> &baseAnimations = animations[selectedAnimation].second;

		currentAnimationTime += dt;

		if (currentAnimationTime >= duration)
			currentAnimationTime = 0.0f;

		// update local translation / rotation / scale for each animated node (base animations target specific nodes)
		for (int i = 0; i < baseAnimations.size(); i++)
			applyBaseAnimation(baseAnimations[i], currentAnimationTime);

		// update total transformation matrix for each node
		for (int i = 0; i < nodes.size(); i++)
		{
			if (nodes[i].parentIndex == -1)
				updateNodesTransformationsRecursive(i, glm::mat4(1.0f));
		}
	}

	if (modelCenter != glm::vec3(-1.0f)) // = if using 3D model viewer, where model center is initialized
	{
		model = glm::mat4(1.0f);
		model = glm::translate(model, modelCenter); // a trick to build the correct model matrix that rotates the model around its center
		model = glm::rotate(model, glm::radians(meshPitch), glm::vec3(1, 0, 0));
		model = glm::rotate(model, glm::radians(meshYaw), glm::vec3(0, 1, 0));
		model = glm::translate(model, -modelCenter);
	}

	shader.use();
	shader.setMat4("view", view);
	shader.setMat4("projection", projection);
	shader.setBool("lighting", lighting);
	shader.setVec3("cameraPos", cameraPos);

	// only for skinned (non-static) models: update total transformation matrix for each bone and send to GPU
	// (final model matrix calculation for these models is done per vertex on GPU, which is how the skinning works)
	if (hasSkinningData)
	{
		if (!nodes.empty() && !bindPoseMatrices.empty())
		{
			shader.setBool("useSkinning", true);

			for (int i = 0, boneCount = boneTotalTransforms.size(); i < boneCount; i++)
			{
				int nodeIndex = boneToNodeIdx[i];
				boneTotalTransforms[i] = bindShapeMatrix * nodes[nodeIndex].totalTransform * bindPoseMatrices[i]; // this is core formula for skeletal animation skinning; the resulting skinning matrix needs to be applied to a vertex to make it move with a specific bone (with respect to this bone weight and influence of other bones; see vertex shader)
				shader.setMat4("boneTotalTransforms[" + std::to_string(i) + "]", boneTotalTransforms[i]);
			}
		}
	}
	else
		shader.setBool("useSkinning", false);

	// only for non-skinned models: calculate final model matrices for each submesh
	// (final model matrix calculation for these models is done per submesh on CPU)
	std::vector<glm::mat4> submeshModelMatrices(totalSubmeshCount);

	for (int i = 0; i < totalSubmeshCount; i++)
	{
		int meshIndex = submeshToMeshIdx[i];
		submeshModelMatrices[i] = model;

		if (!hasSkinningData)
		{
			auto it = meshToNodeIdx.find(meshIndex);

			if (it != meshToNodeIdx.end())
			{
				int nodeIndex = it->second;
				submeshModelMatrices[i] *= nodes[nodeIndex].totalTransform;
			}
		}
	}

	// render model
	glBindVertexArray(VAO);

	if (!simple)
	{
		shader.setInt("renderMode", 1);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		for (int i = 0; i < totalSubmeshCount; i++)
		{
			int meshIndex = submeshToMeshIdx[i];

			if (meshIndex < 0)
			{
				std::cout << "[Warning] Model::draw: skipping submesh [" << i << "] --> invalid mesh index [" << meshIndex << "]" << std::endl;
				continue;
			}

			shader.setMat4("model", submeshModelMatrices[i]);

			glActiveTexture(GL_TEXTURE0);

			if (alternativeTextureCount > 0 && textureCount == 1)
				glBindTexture(GL_TEXTURE_2D, textures[selectedTexture]);
			else if (textureCount > 1)
			{
				if (submeshTextureIndex[i] == -1)
				{
					std::cout << "[Warning] Model::draw: skipping submesh [" << i << "] --> invalid texture index [" << submeshTextureIndex[i] << "]" << std::endl;
					continue;
				}

				glBindTexture(GL_TEXTURE_2D, textures[submeshTextureIndex[i]]);
			}
			else
				glBindTexture(GL_TEXTURE_2D, textures[0]);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
		}
	}
	else
	{
		// first pass: render mesh edges (wireframe mode)
		shader.setInt("renderMode", 2);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		for (int i = 0; i < totalSubmeshCount; i++)
		{
			shader.setMat4("model", submeshModelMatrices[i]);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
		}

		// second pass: render mesh faces
		shader.setInt("renderMode", 3);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		for (int i = 0; i < totalSubmeshCount; i++)
		{
			shader.setMat4("model", submeshModelMatrices[i]);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
		}

		glBindVertexArray(0);

		// render nodes (only in simple mode)
		if (!nodes.empty())
		{
			defaultShader.use();
			defaultShader.setMat4("projection", projection);
			defaultShader.setMat4("view", view);

			glBindVertexArray(nodeVAO);

			for (int i = 0; i < nodes.size(); i++)
			{
				Node &node = nodes[i];

				glm::mat4 nodeModel = model * node.totalTransform;
				nodeModel = glm::scale(nodeModel, glm::vec3(0.05f));
				defaultShader.setMat4("model", nodeModel);

				// set color based on node type
				glm::vec3 color;

				if (node.parentIndex == -1)
					color = glm::vec3(1.0f, 0.0f, 0.0f); // red for root nodes
				else if (node.childIndices.empty())
					color = glm::vec3(0.0f, 0.5f, 1.0f); // blue for leaf nodes
				else
					color = glm::vec3(0.0f, 1.0f, 0.0f); // green for normal joint nodes

				defaultShader.setVec3("color", color);

				glDrawElements(GL_TRIANGLES, 60, GL_UNSIGNED_INT, 0); // one icosahedron has 20 faces * 3 indices / face = 60 indices
			}

			glBindVertexArray(0);
		}
	}
}

//! Applies a base animation (translation / rotation / scale) at a specific time, targeting one node.
void Model::applyBaseAnimation(BaseAnimation &baseAnim, float time)
{
	auto it = nodeNameToIdx.find(baseAnim.targetNodeName); // [TODO] use boneNameToNodeIdx instead

	if (it == nodeNameToIdx.end())
		return; // target node not found

	int nodeIndex = it->second;
	Node *node = &nodes[nodeIndex];

	std::vector<float> &timestamps = baseAnim.timestamps;
	std::vector<std::vector<float>> &transformations = baseAnim.transformations;

	if (timestamps.empty() || transformations.empty())
		return;

	int keyframeCount = timestamps.size();

	int keyframe0 = 0, keyframe1 = 0; // indices into the animation data array

	float t = 0.0f; // normalized current animation time

	// find two timestamps that surround the current time
	if (keyframeCount == 1)
		keyframe0 = keyframe1 = 0; // only one keyframe, use it directly (nothing to interpolate)
	else
	{
		for (int i = 0; i < keyframeCount - 1; i++)
		{
			if (time >= timestamps[i] && time <= timestamps[i + 1])
			{
				keyframe0 = i;
				keyframe1 = i + 1;
				float t0 = timestamps[i];
				float t1 = timestamps[i + 1];

				if (t1 > t0)
					t = (time - t0) / (t1 - t0); // convert to range [0, 1]
				else
					LOG("[Warning] Model::applyBaseAnimation invalid timestamps data: next timestamp value is lower than previous one. t0 = ", t0, " > t1 = ", t1);

				break;
			}
		}
	}

	switch (baseAnim.animationType)
	{
	case 1: // translation --> X Y Z
	{
		glm::vec3 v0(transformations[keyframe0][0], transformations[keyframe0][1], transformations[keyframe0][2]);
		glm::vec3 v1(transformations[keyframe1][0], transformations[keyframe1][1], transformations[keyframe1][2]);
		node->localTranslation = interpolateVec3(v0, v1, t, baseAnim.interpolationType);
		break;
	}
	case 5: // rotation --> X Y Z W (GLM constructor expects W X Y Z)
	{
		glm::quat q0(-transformations[keyframe0][3], transformations[keyframe0][0], transformations[keyframe0][1], transformations[keyframe0][2]);
		glm::quat q1(-transformations[keyframe1][3], transformations[keyframe1][0], transformations[keyframe1][1], transformations[keyframe1][2]);
		node->localRotation = interpolateQuat(q0, q1, t, baseAnim.interpolationType);
		break;
	}
	case 10: // scale --> X Y Z
	{
		glm::vec3 v0(transformations[keyframe0][0], transformations[keyframe0][1], transformations[keyframe0][2]);
		glm::vec3 v1(transformations[keyframe1][0], transformations[keyframe1][1], transformations[keyframe1][2]);
		node->localScale = interpolateVec3(v0, v1, t, baseAnim.interpolationType);
		break;
	}
	default:
		break;
	}
}

//! Interpolates between two floats.
float Model::interpolateFloat(float a, float b, float t, int interpolationType)
{
	// a – previous keyframe value
	// b – next keyframe value
	// t – current time between these keyframes, normalized to range [0, 1] (also known as interpolation factor)

	switch (interpolationType)
	{
	case 0: // STEP
		return a;
	case 1: // LINEAR
		return a + (b - a) * t;
	case 2: // HERMITE (smooth cubic interpolation)
		return a + (b - a) * (t * t * (3.0f - 2.0f * t));
	default:
		return a;
	}
}

//! Interpolates between two vectors.
glm::vec3 Model::interpolateVec3(glm::vec3 &a, glm::vec3 &b, float t, int interpolationType)
{
	return glm::vec3(
		interpolateFloat(a.x, b.x, t, interpolationType),
		interpolateFloat(a.y, b.y, t, interpolationType),
		interpolateFloat(a.z, b.z, t, interpolationType));
}

//! Interpolates between two quaternions.
glm::quat Model::interpolateQuat(glm::quat &a, glm::quat &b, float t, int interpolationType)
{
	switch (interpolationType)
	{
	case 0:
		return a;
	case 1:
	case 2:
		return glm::slerp(a, b, t); // spherical linear interpolation for 2 quaternions
	default:
		return a;
	}
}

//! Resets animation to beginning.
void Model::resetAnimation()
{
	currentAnimationTime = 0.0f;

	for (int i = 0; i < nodes.size(); i++)
	{
		Node &node = nodes[i];
		node.localTranslation = node.defaultTranslation;
		node.localRotation = node.defaultRotation;
		node.localScale = node.defaultScale;
	}

	for (int i = 0; i < nodes.size(); i++)
	{
		if (nodes[i].parentIndex == -1)
			updateNodesTransformationsRecursive(i, glm::mat4(1.0f));
	}
}

//! Clears GPU memory and resets viewer state.
void Model::reset()
{
	modelLoaded = false;

	fileSize = 0;

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);

	VAO = VBO = 0;

	if (!EBOs.empty())
	{
		glDeleteBuffers(EBOs.size(), EBOs.data());
		EBOs.clear();
	}

	vertices.clear();
	indices.clear();
	vertexCount = faceCount = 0;
	totalSubmeshCount = 0;
	submeshToMeshIdx.clear();

	if (!textures.empty())
	{
		glDeleteTextures(textures.size(), textures.data());
		textures.clear();
	}

	textureCount = alternativeTextureCount = selectedTexture = 0;
	textureNames.clear();
	submeshTextureIndex.clear();

	glDeleteVertexArrays(1, &nodeVAO);
	glDeleteBuffers(1, &nodeVBO);
	glDeleteBuffers(1, &nodeEBO);

	nodeVAO = nodeVBO = nodeEBO = 0;

	nodes.clear();
	meshToNodeIdx.clear();
	nodeNameToIdx.clear();

	boneNames.clear();
	bindPoseMatrices.clear();
	boneTotalTransforms.clear();
	boneNameToNodeIdx.clear();
	hasSkinningData = false;

	animations.clear();
	currentAnimationTime = 0.0f;
	selectedAnimation = 0;
	animationCount = 0;
	animationPlaying = false;
	animationsLoaded = false;

	sounds.clear();
}
