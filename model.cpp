#include "model.h"
#include <algorithm>
#include <filesystem>
#include <cmath>
#include "PackPatchReader.h"

//! Update mesh transforms from animated node transforms.
void Model::updateMeshTransformsFromNodes()
{
	// This function is now deprecated - we use node.localTransform directly
	// Kept for compatibility but does nothing
}

//! Generate unit sphere geometry for node visualization.
void Model::generateUnitSphere()
{
	std::vector<float> vertices;
	std::vector<unsigned int> indices;

	// Generate icosphere (subdivided icosahedron)
	const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

	// 12 vertices of icosahedron
	std::vector<glm::vec3> positions = {
		glm::normalize(glm::vec3(-1, t, 0)),
		glm::normalize(glm::vec3(1, t, 0)),
		glm::normalize(glm::vec3(-1, -t, 0)),
		glm::normalize(glm::vec3(1, -t, 0)),
		glm::normalize(glm::vec3(0, -1, t)),
		glm::normalize(glm::vec3(0, 1, t)),
		glm::normalize(glm::vec3(0, -1, -t)),
		glm::normalize(glm::vec3(0, 1, -t)),
		glm::normalize(glm::vec3(t, 0, -1)),
		glm::normalize(glm::vec3(t, 0, 1)),
		glm::normalize(glm::vec3(-t, 0, -1)),
		glm::normalize(glm::vec3(-t, 0, 1))};

	// 20 faces of icosahedron
	std::vector<unsigned int> faces = {
		0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11,
		1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
		3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
		4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};

	// Build vertex data (position + normal)
	for (const auto &pos : positions)
	{
		vertices.push_back(pos.x);
		vertices.push_back(pos.y);
		vertices.push_back(pos.z);
		// Normal = position for unit sphere
		vertices.push_back(pos.x);
		vertices.push_back(pos.y);
		vertices.push_back(pos.z);
	}

	indices = faces;

	nodeSphereVertexCount = (int)positions.size();
	nodeSphereIndexCount = (int)indices.size();

	// Create OpenGL buffers
	glGenVertexArrays(1, &nodeVAO);
	glGenBuffers(1, &nodeVBO);
	glGenBuffers(1, &nodeEBO);

	glBindVertexArray(nodeVAO);

	glBindBuffer(GL_ARRAY_BUFFER, nodeVBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, nodeEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

	LOG("[Init] Generated unit sphere: ", nodeSphereVertexCount, " vertices, ", nodeSphereIndexCount / 3, " triangles");
}

//! Render nodes as small spheres.
void Model::drawNodes(glm::mat4 baseModel, glm::mat4 view, glm::mat4 projection)
{
	// Task 1: Only render nodes when NOT in simple mode (displayBaseMesh = true)
	if (nodes.empty() || nodeVAO == 0)
		return;

	// Apply the same transformations as the mesh (for viewer rotation/positioning)
	if (meshCenter != glm::vec3(-1.0f))
	{
		baseModel = glm::mat4(1.0f);
		baseModel = glm::translate(baseModel, meshCenter);
		baseModel = glm::rotate(baseModel, glm::radians(meshPitch), glm::vec3(1, 0, 0));
		baseModel = glm::rotate(baseModel, glm::radians(meshYaw), glm::vec3(0, 1, 0));
		baseModel = glm::translate(baseModel, -meshCenter);
	}

	nodeShader.use();
	nodeShader.setMat4("projection", projection);
	nodeShader.setMat4("view", view);

	glBindVertexArray(nodeVAO);

	const float FIXED_SPHERE_SIZE = 0.05f;

	// Render each node
	for (size_t i = 0; i < nodes.size(); i++)
	{
		const Node &node = nodes[i];

		// Use node's local transform directly
		glm::mat4 nodeTransform = node.totalTransform; // Use world transform
		glm::mat4 fullTransform = baseModel * nodeTransform;
		glm::vec3 nodePosition = glm::vec3(fullTransform[3]);

		// Build node model matrix
		glm::mat4 nodeModel = glm::translate(glm::mat4(1.0f), nodePosition);
		nodeModel = glm::scale(nodeModel, glm::vec3(FIXED_SPHERE_SIZE));

		// Set color based on node type
		glm::vec3 color;
		if (node.parentIndex == -1)
			color = glm::vec3(1.0f, 0.0f, 0.0f); // Red for root nodes
		else if (node.childIndices.empty())
			color = glm::vec3(0.0f, 0.5f, 1.0f); // Blue for leaf nodes
		else
			color = glm::vec3(0.0f, 1.0f, 0.0f); // Green for joint nodes

		nodeShader.setMat4("model", nodeModel);
		nodeShader.setVec3("Color", color);

		glDrawElements(GL_TRIANGLES, nodeSphereIndexCount, GL_UNSIGNED_INT, 0);
	}

	glBindVertexArray(0);
}

//! Clears GPU memory and resets viewer state.
void Model::reset()
{
	modelLoaded = false;

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);

	VAO = VBO = 0;

	if (!EBOs.empty())
	{
		glDeleteBuffers(EBOs.size(), EBOs.data());
		EBOs.clear();
	}

	// Clean up node visualization buffers
	if (nodeVAO != 0)
	{
		glDeleteVertexArrays(1, &nodeVAO);
		glDeleteBuffers(1, &nodeVBO);
		glDeleteBuffers(1, &nodeEBO);
		nodeVAO = nodeVBO = nodeEBO = 0;
	}

	if (!textures.empty())
	{
		glDeleteTextures(textures.size(), textures.data());
		textures.clear();
	}

	textureNames.clear();
	submeshTextureIndex.clear();
	vertices.clear();
	indices.clear();
	sounds.clear();
	submeshToMeshIdx.clear();
	nodes.clear();
	nodeNameToIdx.clear();
	boneNameToNodeIdx.clear();
	meshToNodeIdx.clear();

	// Clear animation data
	animationSets.clear();
	keyframeSourceSets.clear();
	animationFileNames.clear();
	animationPlaying = false;
	animationsLoaded = false;
	currentAnimationTime = 0.0f;
	animationSpeed = 1.0f;
	selectedAnimation = 0;
	animationCount = 0;
	animationReversed = false;
	loopMode = AnimationLoopMode::PINGPONG;

	// Clear skinning data
	boneNames.clear();
	bindPoseMatrices.clear();
	boneTransforms.clear();
	boneInfluences.clear();
	hasSkinningData = false;
	bone001RootTransform = glm::mat4(1.0f);

	fileSize = vertexCount = faceCount = textureCount = alternativeTextureCount = selectedTexture = totalSubmeshCount = 0;
	nodeSphereVertexCount = nodeSphereIndexCount = 0;
}

//! Renders .bdae model.
void Model::draw(glm::mat4 model, glm::mat4 view, glm::mat4 projection, glm::vec3 cameraPos, float dt, bool lighting, bool simple)
{
	if (!modelLoaded || EBOs.empty())
		return;

	// Update animations
	if (animationsLoaded)
		updateAnimations(dt);

	if (meshCenter != glm::vec3(-1.0f)) // = if using 3D model viewer, where mesh center is initialized
	{
		model = glm::mat4(1.0f);
		model = glm::translate(model, meshCenter); // a trick to build the correct model matrix that rotates the mesh around its center
		model = glm::rotate(model, glm::radians(meshPitch), glm::vec3(1, 0, 0));
		model = glm::rotate(model, glm::radians(meshYaw), glm::vec3(0, 1, 0));
		model = glm::translate(model, -meshCenter);
	}

	shader.use();
	shader.setMat4("view", view);
	shader.setMat4("projection", projection);
	shader.setBool("lighting", lighting);
	shader.setVec3("cameraPos", cameraPos);

	// Setup skeletal animation if skinning data is present
	if (hasSkinningData && !bindPoseMatrices.empty())
	{
		updateBoneTransforms();

		// Debug: Print bone transforms once at start and once at 1 second
		static bool debugPrintedStart = false;
		static bool debugPrintedMid = false;
		if (!debugPrintedStart && currentAnimationTime < 0.1f)
		{
			debugBoneTransforms(currentAnimationTime);
			debugPrintedStart = true;
		}
		else if (!debugPrintedMid && currentAnimationTime > 1.0f && currentAnimationTime < 1.1f)
		{
			debugBoneTransforms(currentAnimationTime);
			debugPrintedMid = true;
		}

		// Only enable skinning if animations are actually loaded and playing.
		// For static models, we treat them as regular rigid meshes.
		shader.setBool("useSkinning", animationsLoaded);

		// Send bone transformation matrices to shader
		for (int i = 0; i < (int)boneTransforms.size() && i < 128; i++)
		{
			std::string uniformName = "boneTransforms[" + std::to_string(i) + "]";
			shader.setMat4(uniformName, boneTransforms[i]);
		}
	}
	else
	{
		shader.setBool("useSkinning", false);
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

			glm::mat4 finalModelMatrix = model;

			// If skinning is not active (i.e., for non-skinned models OR for skinned models without animations),
			// we treat the mesh as a rigid object and apply its node's world transform.
			if (!animationsLoaded)
			{
				auto it = meshToNodeIdx.find(meshIndex);
				if (it != meshToNodeIdx.end())
				{
					int nodeIndex = it->second;
					if (nodeIndex >= 0 && nodeIndex < (int)nodes.size())
					{
						finalModelMatrix *= nodes[nodeIndex].totalTransform;
					}
				}
			}
			shader.setMat4("model", finalModelMatrix);

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
			int meshIndex = submeshToMeshIdx[i];

			glm::mat4 finalModelMatrix = model;

			// If skinning is not active (i.e., for non-skinned models OR for skinned models without animations),
			// we treat the mesh as a rigid object and apply its node's world transform.
			if (!animationsLoaded)
			{
				auto it = meshToNodeIdx.find(meshIndex);
				if (it != meshToNodeIdx.end())
				{
					int nodeIndex = it->second;
					if (nodeIndex >= 0 && nodeIndex < (int)nodes.size())
					{
						finalModelMatrix *= nodes[nodeIndex].totalTransform;
					}
				}
			}
			shader.setMat4("model", finalModelMatrix);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
		}

		// second pass: render mesh faces
		shader.setInt("renderMode", 3);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		for (int i = 0; i < totalSubmeshCount; i++)
		{
			int meshIndex = submeshToMeshIdx[i];

			glm::mat4 finalModelMatrix = model;

			// If skinning is not active (i.e., for non-skinned models OR for skinned models without animations),
			// we treat the mesh as a rigid object and apply its node's world transform.
			if (!animationsLoaded)
			{
				auto it = meshToNodeIdx.find(meshIndex);
				if (it != meshToNodeIdx.end())
				{
					int nodeIndex = it->second;
					if (nodeIndex >= 0 && nodeIndex < (int)nodes.size())
					{
						finalModelMatrix *= nodes[nodeIndex].totalTransform;
					}
				}
			}
			shader.setMat4("model", finalModelMatrix);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
		}

		// render node visualization (bones/skeleton)
		drawNodes(glm::mat4(1.0f), view, projection);
	}

	glBindVertexArray(0);
}

//! Update animations based on elapsed time.
void Model::updateAnimations(float deltaTime)
{
	if (!animationPlaying || animationSets.empty())
		return;

	// Validate selected animation index
	if (selectedAnimation < 0 || selectedAnimation >= (int)animationSets.size())
		return;

	const std::vector<Animation> &animations = animationSets[selectedAnimation];

	// Find the maximum duration across all animations
	float maxDuration = 0.0f;
	for (const Animation &anim : animations)
	{
		if (anim.duration > maxDuration)
			maxDuration = anim.duration;
	}

	// If no valid duration found, don't animate
	if (maxDuration <= 0.0f)
		return;

	// DEBUG: Print frame info
	static int frameCount = 0;
	static float lastPrintTime = 0.0f;
	frameCount++;

	float oldTime = currentAnimationTime;
	bool oldReversed = animationReversed;

	// Update time based on direction (forward or reverse for ping-pong)
	float timeStep = deltaTime * animationSpeed;
	if (animationReversed)
		timeStep = -timeStep;

	currentAnimationTime += timeStep;

	// Handle looping based on mode
	if (loopMode == AnimationLoopMode::PINGPONG)
	{
		// Ping-pong: reverse direction at boundaries and bounce the excess time
		if (currentAnimationTime >= maxDuration)
		{
			float excess = currentAnimationTime - maxDuration;
			currentAnimationTime = maxDuration - excess; // Bounce back
			animationReversed = true;					 // Start playing in reverse
			printf("[FRAME %d] BOUNCE AT END: oldTime=%.4f, excess=%.4f, newTime=%.4f, reversed=%d->%d\n",
				   frameCount, oldTime, excess, currentAnimationTime, oldReversed, animationReversed);
		}
		else if (currentAnimationTime <= 0.0f)
		{
			float excess = -currentAnimationTime;
			currentAnimationTime = excess; // Bounce forward
			animationReversed = false;	   // Start playing forward
			printf("[FRAME %d] BOUNCE AT START: oldTime=%.4f, excess=%.4f, newTime=%.4f, reversed=%d->%d\n",
				   frameCount, oldTime, excess, currentAnimationTime, oldReversed, animationReversed);
		}
	}
	else // AnimationLoopMode::LOOP
	{
		// Normal loop: wrap around to start
		if (currentAnimationTime >= maxDuration)
		{
			currentAnimationTime = fmod(currentAnimationTime, maxDuration);
		}
	}

	// Clamp time to valid range
	float time = std::max(0.0f, std::min(currentAnimationTime, maxDuration));

	// DEBUG: Print every 30 frames or when direction changes
	if (frameCount % 30 == 0 || oldReversed != animationReversed)
	{
		printf("[FRAME %d] deltaTime=%.4f, timeStep=%.4f, currentTime=%.4f, maxDuration=%.4f, reversed=%d, speed=%.2f\n",
			   frameCount, deltaTime, timeStep, currentAnimationTime, maxDuration, animationReversed, animationSpeed);
	}

	// Apply all animations at the same time point
	for (const Animation &anim : animations)
	{
		// Skip animations with zero duration (they contain no animation data)
		if (anim.duration <= 0.0f)
			continue;

		// Each animation can have different duration, but we use the same time point
		// If time exceeds this animation's duration, clamp to its end
		float animTime = std::min(time, anim.duration);
		applyAnimation(anim, animTime);
	}

	// DEBUG: Print first bone position every 30 frames
	if (frameCount % 30 == 0 && !nodes.empty())
	{
		int boneNode = -1;
		// Find first node with a skinning name (bone)
		for (size_t i = 0; i < nodes.size(); i++)
		{
			if (!nodes[i].boneName.empty())
			{
				boneNode = i;
				break;
			}
		}
		if (boneNode >= 0)
		{
			glm::vec3 pos = nodes[boneNode].localTranslation;
			printf("[FRAME %d] Bone '%s' local pos: (%.3f, %.3f, %.3f)\n",
				   frameCount, nodes[boneNode].boneName.c_str(), pos.x, pos.y, pos.z);
		}
	}

	// Recalculate world transforms after animation
	for (int i = 0; i < nodes.size(); i++)
	{
		if (nodes[i].parentIndex == -1)
			updateNodesTransformationsRecursive(i, glm::mat4(1.0f));
	}

	// Update mesh transforms from animated nodes
	updateMeshTransformsFromNodes();
}

//! Apply an animation at a specific time.
void Model::applyAnimation(const Animation &anim, float time)
{
	// Validate selected animation index
	if (selectedAnimation < 0 || selectedAnimation >= (int)keyframeSourceSets.size())
		return;

	const std::vector<KeyframeSource> &keyframeSources = keyframeSourceSets[selectedAnimation];

	// For each channel, find the keyframes and interpolate
	for (const AnimationChannel &channel : anim.channels)
	{
		if (channel.samplerIndex < 0 || channel.samplerIndex >= (int)anim.samplers.size())
			continue;

		const AnimationSampler &sampler = anim.samplers[channel.samplerIndex];

		// Get time and keyframe data
		if (sampler.inputSourceIndex < 0 || sampler.inputSourceIndex >= (int)keyframeSources.size())
			continue;
		if (sampler.outputSourceIndex < 0 || sampler.outputSourceIndex >= (int)keyframeSources.size())
			continue;

		const KeyframeSource &timeSource = keyframeSources[sampler.inputSourceIndex];
		const KeyframeSource &keyframeSource = keyframeSources[sampler.outputSourceIndex];

		if (timeSource.data.empty() || keyframeSource.data.empty())
			continue;

		// Calculate actual keyframe count from output source
		// The output source contains all keyframe data (e.g., vec3 or quat values)
		// We need to divide by component count to get the number of keyframes
		int actualKeyframeCount = keyframeSource.data.size() / sampler.outputComponentCount;

		// Debug: Show mismatch if time source and output source have different counts
		if ((int)timeSource.data.size() != actualKeyframeCount)
		{
			static bool warned = false;
			if (!warned)
			{
				LOG("[Info] Time/Output count mismatch: time=", timeSource.data.size(),
					", output=", actualKeyframeCount, " (", keyframeSource.data.size(),
					"/", sampler.outputComponentCount, "). Using minimum.");
				warned = true;
			}
		}

		// Use the minimum of time source size and actual keyframe count to avoid out-of-bounds
		int keyframeCount = std::min((int)timeSource.data.size(), actualKeyframeCount);

		// If there's still a mismatch, something is wrong with the data, so skip this channel
		if (keyframeCount != (int)timeSource.data.size())
		{
			continue;
		}

		if (keyframeCount < 2)
		{
			// Not enough keyframes to interpolate
			continue;
		}

		// CRITICAL FIX: Loop time within this channel's keyframe range
		// Each channel may have different keyframe ranges (e.g., translation 0-0.8s, rotation 0-2.4s)
		// We need to loop the time within THIS channel's range, not the global animation duration
		float channelStartTime = timeSource.data[0];
		float channelEndTime = timeSource.data[keyframeCount - 1];
		float channelDuration = channelEndTime - channelStartTime;

		float channelTime = time;
		if (channelDuration > 0.0f && time > channelEndTime)
		{
			// Loop the time within this channel's range
			float timeOffset = time - channelStartTime;
			channelTime = channelStartTime + fmod(timeOffset, channelDuration);
		}

		int keyframe0 = 0, keyframe1 = 0;
		float t = 0.0f;
		bool foundKeyframe = false;

		// Find keyframes using the looped channel time
		for (int i = 0; i < keyframeCount - 1; i++)
		{
			if (channelTime >= timeSource.data[i] && channelTime <= timeSource.data[i + 1])
			{
				keyframe0 = i;
				keyframe1 = i + 1;
				float t0 = timeSource.data[i];
				float t1 = timeSource.data[i + 1];
				t = (channelTime - t0) / (t1 - t0);
				foundKeyframe = true;
				break;
			}
		}

		// If we didn't find a keyframe pair, handle edge cases
		if (!foundKeyframe)
		{
			if (channelTime >= timeSource.data[keyframeCount - 1])
			{
				// Time is past the last keyframe, use the last keyframe
				keyframe0 = keyframe1 = keyframeCount - 1;
				t = 0.0f;
			}
			else if (channelTime <= timeSource.data[0])
			{
				// Time is before the first keyframe, use the first keyframe
				keyframe0 = keyframe1 = 0;
				t = 0.0f;
			}
		}

		// Find target node
		auto nodeIt = nodeNameToIdx.find(channel.targetNodeName);

		if (nodeIt == nodeNameToIdx.end())
		{
			continue; // Node not found, skip this channel
		}

		int nodeIndex = nodeIt->second;
		if (nodeIndex < 0 || nodeIndex >= (int)nodes.size())
			continue;

		Node *node = &nodes[nodeIndex];

		// Apply animation based on channel type
		switch (channel.channelType)
		{
		case ChannelType::Translation:
		{
			int comp = sampler.outputComponentCount;
			if (comp > 0) // Allow any number of components
			{
				// Bounds check
				int maxIndex = keyframe1 * comp + (comp - 1);
				if (maxIndex >= (int)keyframeSource.data.size())
				{
					LOG("[Warning] Translation keyframe index out of bounds: ", maxIndex, " >= ", keyframeSource.data.size());
					break;
				}

				glm::vec3 v0(0.0f), v1(0.0f);
				for (int c = 0; c < comp; ++c)
					v0[c] = keyframeSource.data[keyframe0 * comp + c];
				for (int c = 0; c < comp; ++c)
					v1[c] = keyframeSource.data[keyframe1 * comp + c];

				glm::vec3 result = interpolateVec3(v0, v1, t, sampler.interpolation);

				node->localTranslation = result;
			}
			break;
		}
		case ChannelType::Rotation:
		{
			int comp = sampler.outputComponentCount;
			if (comp > 0) // Allow any number of components
			{
				// Bounds check
				int maxIndex = keyframe1 * comp + (comp - 1);
				if (maxIndex >= (int)keyframeSource.data.size())
				{
					LOG("[Warning] Rotation keyframe index out of bounds: ", maxIndex, " >= ", keyframeSource.data.size());
					break;
				}

				glm::quat q0(1, 0, 0, 0), q1(1, 0, 0, 0);
				if (comp == 4)
				{
					q0 = glm::quat(keyframeSource.data[keyframe0 * comp + 3], keyframeSource.data[keyframe0 * comp + 0], keyframeSource.data[keyframe0 * comp + 1], keyframeSource.data[keyframe0 * comp + 2]);
					q1 = glm::quat(keyframeSource.data[keyframe1 * comp + 3], keyframeSource.data[keyframe1 * comp + 0], keyframeSource.data[keyframe1 * comp + 1], keyframeSource.data[keyframe1 * comp + 2]);
					q0.w = -q0.w;
					q1.w = -q1.w; // Apply negation
				}

				glm::quat result = interpolateQuat(q0, q1, t, sampler.interpolation);

				node->localRotation = result;
			}
			break;
		}
		case ChannelType::Scale:
		{
			int comp = sampler.outputComponentCount;
			if (comp > 0) // Allow any number of components
			{
				// Bounds check
				int maxIndex = keyframe1 * comp + (comp - 1);
				if (maxIndex >= (int)keyframeSource.data.size())
				{
					LOG("[Warning] Scale keyframe index out of bounds: ", maxIndex, " >= ", keyframeSource.data.size());
					break;
				}

				glm::vec3 v0(1.0f), v1(1.0f);
				for (int c = 0; c < comp; ++c)
					v0[c] = keyframeSource.data[keyframe0 * comp + c];
				for (int c = 0; c < comp; ++c)
					v1[c] = keyframeSource.data[keyframe1 * comp + c];

				glm::vec3 result = interpolateVec3(v0, v1, t, sampler.interpolation);

				node->localScale = result;
			}
			break;
		}
		default:
			break;
		}
	}
}

//! Interpolate between two floats.
float Model::interpolateFloat(float a, float b, float t, InterpolationType interp)
{
	switch (interp)
	{
	case InterpolationType::STEP:
		return a;
	case InterpolationType::LINEAR:
		return a + (b - a) * t;
	case InterpolationType::HERMITE:
		// Simplified hermite (cubic) interpolation
		return a + (b - a) * (t * t * (3.0f - 2.0f * t));
	default:
		return a;
	}
}

//! Interpolate between two vec3s.
glm::vec3 Model::interpolateVec3(const glm::vec3 &a, const glm::vec3 &b, float t, InterpolationType interp)
{
	return glm::vec3(
		interpolateFloat(a.x, b.x, t, interp),
		interpolateFloat(a.y, b.y, t, interp),
		interpolateFloat(a.z, b.z, t, interp));
}

//! Interpolate between two quaternions.
glm::quat Model::interpolateQuat(const glm::quat &a, const glm::quat &b, float t, InterpolationType interp)
{
	switch (interp)
	{
	case InterpolationType::STEP:
		return a;
	case InterpolationType::LINEAR:
	case InterpolationType::HERMITE:
		// Use spherical linear interpolation (slerp) for quaternions
		return glm::slerp(a, b, t);
	default:
		return a;
	}
}

//! Start playing animations.
void Model::playAnimation()
{
	animationPlaying = true;
	printf("\n[ANIMATION] ========== PLAY ANIMATION ==========\n");
	printf("[ANIMATION] Selected animation: %d\n", selectedAnimation);
	printf("[ANIMATION] Animation sets count: %zu\n", animationSets.size());
	printf("[ANIMATION] Loop mode: %s\n", loopMode == AnimationLoopMode::PINGPONG ? "PINGPONG" : "LOOP");
	printf("[ANIMATION] Animation speed: %.2f\n", animationSpeed);
	printf("[ANIMATION] Current time: %.4f\n", currentAnimationTime);
	printf("[ANIMATION] Reversed: %d\n", animationReversed);
	if (!animationSets.empty() && selectedAnimation >= 0 && selectedAnimation < (int)animationSets.size())
	{
		float maxDuration = 0.0f;
		for (const Animation &anim : animationSets[selectedAnimation])
		{
			if (anim.duration > maxDuration)
				maxDuration = anim.duration;
		}
		printf("[ANIMATION] Max duration: %.4f seconds\n", maxDuration);
	}
	printf("[ANIMATION] =====================================\n\n");
}

//! Pause animations.
void Model::pauseAnimation()
{
	animationPlaying = false;
}

//! Reset animation to beginning.
void Model::resetAnimation()
{
	currentAnimationTime = 0.0f;

	// Restore original transforms
	for (Node &node : nodes)
	{
		node.localTranslation = node.defaultTranslation;
		node.localRotation = node.defaultRotation;
		node.localScale = node.defaultScale;
	}

	// Recalculate world transforms
	for (int i = 0; i < nodes.size(); i++)
	{
		if (nodes[i].parentIndex == -1)
			updateNodesTransformationsRecursive(i, glm::mat4(1.0f));
	}
}

//! Update bone transformation matrices for skinning.
void Model::updateBoneTransforms()
{
	if (nodes.empty() || bindPoseMatrices.empty())
		return;

	int boneCount = (int)bindPoseMatrices.size();
	boneTransforms.resize(boneCount);

	// Compute bone transforms: boneTransform = worldTransform * inverseBindPose
	for (int boneIndex = 0; boneIndex < boneCount; boneIndex++)
	{
		if (boneIndex < (int)boneToNodeIdx.size() && boneToNodeIdx[boneIndex] != -1)
		{
			int nodeIndex = boneToNodeIdx[boneIndex];
			if (nodeIndex < (int)nodes.size())
			{
				// Use the world transform (already includes all parent transforms)
				boneTransforms[boneIndex] = nodes[nodeIndex].totalTransform * bindPoseMatrices[boneIndex];
			}
			else
			{
				boneTransforms[boneIndex] = glm::mat4(1.0f);
			}
		}
		else
		{
			// Unmapped bone - use identity or bind pose
			boneTransforms[boneIndex] = bindPoseMatrices[boneIndex];
		}
	}
}

//! Debug: Print detailed bone transform information
void Model::debugBoneTransforms(float animTime)
{
	LOG("\n\033[1m\033[95m[DEBUG] Bone Transforms at time ", animTime, "s\033[0m");

	// Show Bone001 root transform
	glm::vec3 bone001Pos = glm::vec3(bone001RootTransform[3]);
	LOG("  \033[93mBone001 Root Transform: pos=(", bone001Pos.x, ", ", bone001Pos.y, ", ", bone001Pos.z, ")\033[0m");

	int boneCount = (int)boneTransforms.size();
	for (int i = 0; i < boneCount && i < 5; i++) // Show first 5 bones
	{
		char boneName[32];
		snprintf(boneName, sizeof(boneName), "Bone%03d", i + 1);

		int nodeIndex = (i < (int)boneToNodeIdx.size()) ? boneToNodeIdx[i] : -1;

		LOG("  [", i, "] ", boneName, " -> Node[", nodeIndex, "]");

		if (nodeIndex != -1 && nodeIndex < (int)nodes.size())
		{
			glm::vec3 nodePos = glm::vec3(nodes[nodeIndex].totalTransform[3]);
			LOG("      Node world pos: (", nodePos.x, ", ", nodePos.y, ", ", nodePos.z, ")");
		}

		glm::vec3 invBindPos = glm::vec3(bindPoseMatrices[i][3]);
		LOG("      InvBind trans: (", invBindPos.x, ", ", invBindPos.y, ", ", invBindPos.z, ")");

		glm::vec3 finalPos = glm::vec3(boneTransforms[i][3]);
		LOG("      Final trans: (", finalPos.x, ", ", finalPos.y, ", ", finalPos.z, ")");
	}
}
