#include <algorithm>
#include <filesystem>
#include <cmath>
#include "PackPatchReader.h"
#include "model.h"
#include "libs/stb_image.h"

//! Parses .bdae file: textures, materials, meshes, mesh skin (if exist), and node tree.
int Model::init(IReadResFile *file)
{
	LOG("\033[1m\033[38;2;200;200;200m[Init] Starting Model::init..\033[0m\n");

	// 1. read Header data as a structure
	fileSize = file->getSize();
	int headerSize = sizeof(struct BDAEFileHeader);
	struct BDAEFileHeader *header = new BDAEFileHeader;

	LOG("\033[37m[Init] Header size (size of struct): \033[0m", headerSize);
	LOG("\033[37m[Init] File size (length of file): \033[0m", fileSize);
	LOG("\033[37m[Init] File name: \033[0m", file->getFileName());
	LOG("\n\033[37m[Init] At position ", file->getPos(), ", reading header..\033[0m");

	file->read(header, headerSize);

	LOG("_________________");
	LOG("\nFile Header Data\n");
	std::ostringstream hexStream;
	hexStream << "Signature: " << std::hex << ((char *)&header->signature)[0] << ((char *)&header->signature)[1] << ((char *)&header->signature)[2] << ((char *)&header->signature)[3] << std::dec;
	LOG(hexStream.str());
	LOG("Endian check: ", header->endianCheck);
	LOG("Version: ", header->version);
	LOG("Header size: ", header->sizeOfHeader);
	LOG("File size: ", header->sizeOfFile);
	LOG("Number of offsets: ", header->numOffsets);
	LOG("Origin: ", header->origin);
	LOG("\nSection offsets  ");
	LOG("Offset Data:   ", header->offsetOffsetTable);
	LOG("String Data:   ", header->offsetStringTable);
	LOG("Data:          ", header->offsetData);
	LOG("Related files: ", header->offsetRelatedFiles);
	LOG("Removable:     ", header->offsetRemovable);
	LOG("\nSize of Removable Chunk: ", header->sizeOfRemovable);
	LOG("Number of Removable Chunks: ", header->numRemovableChunks);
	LOG("Use separated allocation: ", ((header->useSeparatedAllocationForRemovableBuffers > 0) ? "Yes" : "No"));
	LOG("Size of Dynamic Chunk: ", header->sizeOfDynamic);
	LOG("________________________\n");

	// 2. allocate memory and write header, offset, string, data, and removable sections to buffer storage as a raw binary data
	unsigned int sizeUnRemovable = fileSize - header->sizeOfDynamic;

	DataBuffer = (char *)malloc(sizeUnRemovable); // main buffer

	memcpy(DataBuffer, header, headerSize); // copy header

	LOG("\n\033[37m[Init] At position ", file->getPos(), ", reading offset, string, model info and model data sections..\033[0m");
	file->read(DataBuffer + headerSize, sizeUnRemovable - headerSize); // insert after header

	// 3. parse general model info: counts and metadata offsets for textures, materials, meshes, etc.

	LOG("\033[37m[Init] Parsing general model info: counts and metadata offsets for textures, materials, meshes, etc.\033[0m");

	int textureMetadataOffset;
	int materialCount, materialMetadataOffset;
	int meshCount, meshMetadataOffset;
	int meshSkinCount, meshSkinMetadataOffset;
	int nodeTreeCount, nodeTreeMetadataOffset;

#ifdef BETA_GAME_VERSION
	char *ptr = DataBuffer + header->offsetData + 76; // points to texture info in the Data section
#else
	char *ptr = DataBuffer + header->offsetData + 96;
#endif

	memcpy(&textureCount, ptr, sizeof(int));
	memcpy(&textureMetadataOffset, ptr + 4, sizeof(int));
	memcpy(&materialCount, ptr + 16, sizeof(int));
	memcpy(&materialMetadataOffset, ptr + 20, sizeof(int));
	memcpy(&meshCount, ptr + 24, sizeof(int));
	memcpy(&meshMetadataOffset, ptr + 28, sizeof(int));
	memcpy(&meshSkinCount, ptr + 32, sizeof(int));
	memcpy(&meshSkinMetadataOffset, ptr + 36, sizeof(int));
	memcpy(&nodeTreeCount, ptr + 72, sizeof(int));
	memcpy(&nodeTreeMetadataOffset, ptr + 76, sizeof(int));

	// 4. parse TEXTURES AND MATERIALS (materials allow to match submesh with texture)
	// ____________________

	LOG("\033[37m[Init] Parsing model metadata and data.\033[0m");

	LOG("\nTEXTURES: ", ((textureCount != 0) ? std::to_string(textureCount) : "0, file name will be used as a texture name"));

	BDAEint materialNameOffset[materialCount];
	int materialTextureIndex[materialCount];

	if (textureCount > 0)
	{
		textureNames.resize(textureCount);

		for (int i = 0; i < textureCount; i++)
		{
			BDAEint textureNameOffset;
			int textureNameLength;

#ifdef BETA_GAME_VERSION
			memcpy(&textureNameOffset, DataBuffer + textureMetadataOffset + 8 + i * 20, sizeof(BDAEint));
			memcpy(&textureNameLength, DataBuffer + textureNameOffset - 4, sizeof(int));
#else
			memcpy(&textureNameOffset, DataBuffer + header->offsetData + 100 + textureMetadataOffset + 16 + i * 40, sizeof(BDAEint));
			memcpy(&textureNameLength, DataBuffer + textureNameOffset - 4, sizeof(int));
#endif
			textureNames[i] = std::string((DataBuffer + textureNameOffset), textureNameLength);

			LOG("[", i + 1, "] \033[96m", textureNames[i], "\033[0m");
		}

		LOG("\nMATERIALS: ", materialCount);

		for (int i = 0; i < materialCount; i++)
		{
			int materialNameLength;
			int materialPropertyCount;
			int materialPropertyOffset;

#ifdef BETA_GAME_VERSION
			memcpy(&materialNameOffset[i], DataBuffer + materialMetadataOffset + i * 36, sizeof(BDAEint));
			memcpy(&materialPropertyCount, DataBuffer + materialMetadataOffset + 16 + i * 36, sizeof(int));
			memcpy(&materialPropertyOffset, DataBuffer + materialMetadataOffset + 20 + i * 36, sizeof(int));
#else
			memcpy(&materialNameOffset[i], DataBuffer + header->offsetData + 116 + materialMetadataOffset + i * 56, sizeof(BDAEint));
			memcpy(&materialNameLength, DataBuffer + materialNameOffset[i] - 4, sizeof(int));
			memcpy(&materialPropertyCount, DataBuffer + header->offsetData + 148 + materialMetadataOffset + i * 56, sizeof(int));
			memcpy(&materialPropertyOffset, DataBuffer + header->offsetData + 152 + materialMetadataOffset + i * 56, sizeof(int));
#endif

			for (int k = 0; k < materialPropertyCount; k++)
			{
				int propertyType = 0;

#ifdef BETA_GAME_VERSION
				memcpy(&propertyType, DataBuffer + materialPropertyOffset[i] + 8 + k * 24, sizeof(int));
#else
				memcpy(&propertyType, DataBuffer + header->offsetData + 152 + materialMetadataOffset + i * 56 + materialPropertyOffset + 16 + k * 32, sizeof(int));
#endif

				if (propertyType == 11) // type = 11 is 'SAMPLER2D' (normal texture)
				{
					int offset1, offset2;

#ifdef BETA_GAME_VERSION
					memcpy(&offset1, DataBuffer + materialPropertyOffset[i] + 20 + k * 24, sizeof(int));
					memcpy(&offset2, DataBuffer + offset1, sizeof(int));
					memcpy(&materialTextureIndex[i], DataBuffer + offset2, sizeof(int));
#else
					memcpy(&offset1, DataBuffer + header->offsetData + 152 + materialMetadataOffset + i * 56 + materialPropertyOffset + 28 + k * 32, sizeof(int));
					memcpy(&offset2, DataBuffer + header->offsetData + 152 + materialMetadataOffset + i * 56 + materialPropertyOffset + 28 + offset1 + k * 32, sizeof(int));
					memcpy(&materialTextureIndex[i], DataBuffer + header->offsetData + 152 + materialMetadataOffset + i * 56 + materialPropertyOffset + 28 + offset1 + offset2 + k * 32, sizeof(int));
#endif

					break;
				}
			}

			LOG("[", i + 1, "] \033[96m", std::string((DataBuffer + materialNameOffset[i]), materialNameLength), "\033[0m  texture index [", materialTextureIndex[i] + 1, "]");
		}
	}

	// 5. parse MESHES and match submeshes with textures
	// ____________________

	LOG("\nMESHES: ", meshCount);

	int meshVertexCount[meshCount];
	int meshVertexDataOffset[meshCount];
	int bytesPerVertex[meshCount];
	int submeshCount[meshCount];
	std::vector<int> submeshTriangleCount[meshCount];
	std::vector<int> submeshIndexDataOffset[meshCount];

	std::vector<std::string> meshName(meshCount);

	for (int i = 0; i < meshCount; i++)
	{
		BDAEint nameOffset;
		int meshDataOffset;
		int submeshDataOffset;
		int nameLength;

#ifdef BETA_GAME_VERSION
		memcpy(&nameOffset, DataBuffer + meshMetadataOffset + i * 16 + 4, sizeof(BDAEint));
		memcpy(&meshDataOffset, DataBuffer + meshMetadataOffset + 12 + i * 16, sizeof(int));
		memcpy(&meshVertexCount[i], DataBuffer + meshDataOffset + 4, sizeof(int));
		memcpy(&submeshCount[i], DataBuffer + meshDataOffset + 12, sizeof(int));
		memcpy(&submeshDataOffset, DataBuffer + meshDataOffset + 16, sizeof(int));
		memcpy(&bytesPerVertex[i], DataBuffer + meshDataOffset + 44, sizeof(int));
		memcpy(&meshVertexDataOffset[i], DataBuffer + meshDataOffset + 80, sizeof(int));
#else
		memcpy(&nameOffset, DataBuffer + header->offsetData + 120 + 4 + meshMetadataOffset + 8 + i * 24, sizeof(BDAEint));
		memcpy(&meshDataOffset, DataBuffer + header->offsetData + 120 + 4 + meshMetadataOffset + 20 + i * 24, sizeof(int));
		memcpy(&meshVertexCount[i], DataBuffer + header->offsetData + 120 + 4 + meshMetadataOffset + 20 + i * 24 + meshDataOffset + 4, sizeof(int));
		memcpy(&submeshCount[i], DataBuffer + header->offsetData + 120 + 4 + meshMetadataOffset + 20 + i * 24 + meshDataOffset + 12, sizeof(int));
		memcpy(&submeshDataOffset, DataBuffer + header->offsetData + 120 + 4 + meshMetadataOffset + 20 + i * 24 + meshDataOffset + 16, sizeof(int));
		memcpy(&bytesPerVertex[i], DataBuffer + header->offsetData + 120 + 4 + meshMetadataOffset + 20 + i * 24 + meshDataOffset + 48, sizeof(int));
		memcpy(&meshVertexDataOffset[i], DataBuffer + header->offsetData + 120 + 4 + meshMetadataOffset + 20 + i * 24 + meshDataOffset + 88, sizeof(int));
#endif
		memcpy(&nameLength, DataBuffer + nameOffset - 4, sizeof(int));
		meshName[i] = std::string(DataBuffer + nameOffset, nameLength);

		LOG("[", i + 1, "] \033[96m", meshName[i], "\033[0m  ", submeshCount[i], " submeshes, ", meshVertexCount[i], " vertices - ", bytesPerVertex[i], " bytes / vertex");

		for (int k = 0; k < submeshCount[i]; k++)
		{
			int val1, val2, submeshMaterialNameOffset, textureIndex = -1;

#ifdef BETA_GAME_VERSION
			memcpy(&submeshMaterialNameOffset, DataBuffer + submeshDataOffset + 4 + k * 56, sizeof(int));
			memcpy(&val1, DataBuffer + submeshDataOffset + 40 + k * 56, sizeof(int));
			memcpy(&val2, DataBuffer + submeshDataOffset + 44 + k * 56, sizeof(int));
#else
			memcpy(&submeshMaterialNameOffset, ptr + 24 + 4 + meshMetadataOffset + 20 + i * 24 + meshDataOffset + 16 + submeshDataOffset + k * 80 + 8, sizeof(int));
			memcpy(&val1, ptr + 24 + 4 + meshMetadataOffset + 20 + i * 24 + meshDataOffset + 16 + submeshDataOffset + k * 80 + 48, sizeof(int));
			memcpy(&val2, ptr + 24 + 4 + meshMetadataOffset + 20 + i * 24 + meshDataOffset + 16 + submeshDataOffset + k * 80 + 56, sizeof(int));
#endif
			submeshTriangleCount[i].push_back(val1 / 3);
			submeshIndexDataOffset[i].push_back(val2);

			if (textureCount > 0)
			{
				/* map submeshes to textures
				   each submesh should (and can) be mapped to only one texture
				   each texture can be reused by multiple submeshes */

				for (int l = 0; l < materialCount; l++)
				{
					if (submeshMaterialNameOffset == materialNameOffset[l])
					{
						textureIndex = materialTextureIndex[l];
						LOG("    submesh [", i + 1, "][", k + 1, "] --> texture index [", textureIndex + 1, "], ", val1 / 3, " triangles");
						break;
					}

					if (l == materialCount - 1)
						LOG("    submesh [", i + 1, "][", k + 1, "] --> texture not found, ", val1 / 3, " triangles");
				}
			}

			submeshTextureIndex.push_back(textureIndex);
			int submeshIndex = submeshToMeshIdx.size();
			submeshToMeshIdx[submeshIndex] = i;
		}

		totalSubmeshCount += submeshCount[i];
	}

	// 6. parse NODES (nodes allow to position meshes within a model) and match nodes with meshes
	// ____________________

	if (nodeTreeCount != 1)
	{
		LOG("[Error] Model::init unhandled node tree count (this value is always expected to be equal to 1).");
		return -1;
	}

	int rootNodeCount, nodeTreeDataOffset;

	memcpy(&rootNodeCount, DataBuffer + header->offsetData + 168 + 4 + nodeTreeMetadataOffset + 16, sizeof(int));
	memcpy(&nodeTreeDataOffset, DataBuffer + header->offsetData + 168 + 4 + nodeTreeMetadataOffset + 20, sizeof(int));

	for (int i = 0; i < rootNodeCount; i++)
	{
		int rootNodeDataOffset = header->offsetData + 168 + 4 + nodeTreeMetadataOffset + 20 + nodeTreeDataOffset + i * 96;
		parseNodesRecursive(rootNodeDataOffset, -1); // -1 = root node (no parent)
	}

	/* map nodes to meshes
	   each mesh should (and can) be mapped to only one node
	   each node may be mapped to only one mesh, or in many cases, not mapped at all (for example, if it's mapped to a bone instead) */

	int node2meshCount = 0;

	for (int i = 0; i < nodes.size(); i++)
	{
		Node &node = nodes[i];

		auto it = std::find(std::begin(meshName), std::end(meshName), node.mainName);

		if (it != meshName.end())
		{
			int meshIndex = std::distance(meshName.begin(), it);
			meshToNodeIdx[meshIndex] = i;
			node2meshCount++;

			// stop early if every mesh already has exactly one node assigned
			if (node2meshCount == meshCount)
				break;
		}
	}

	// compute PIVOT offset (origin around which a mesh transforms) only for nodes that have meshes attached
	// node tree may have '_PIVOT' helper nodes, which are always terminal nodes and don't have meshes attached. they influence all their parent nodes that are linked to meshes.
	// at the time we recursively parse the node tree top-down, PIVOT nodes are not yet found since they are leaves of the tree, that's why we calculate their effect here only after parsing; alternatively, we could parse the tree bottom-up
	for (auto it = meshToNodeIdx.begin(); it != meshToNodeIdx.end(); it++)
	{
		int nodeIndex = it->second;
		Node &node = nodes[nodeIndex];

		node.pivotTransform = getPIVOTNodeTransformationRecursive(nodeIndex); // get local transformation matrix of the '_PIVOT' node, if it exists in child subtrees (we assume there is at most one PIVOT node)
	}

	// PIVOT transformation is now stored, so we can compute the total transformation matrix for each node
	// for animated models it is node's "starting position" and we will have to call this function every frame
	for (int i = 0; i < nodes.size(); i++)
	{
		if (nodes[i].parentIndex == -1)
			updateNodesTransformationsRecursive(i, glm::mat4(1.0f));
	}

	LOG("\nROOT NODES: ", rootNodeCount, ", nodes in total: ", nodes.size());
	LOG("Node tree illustration. Root nodes are on the left.\n");

	for (int i = 0; i < nodes.size(); i++)
	{
		if (nodes[i].parentIndex == -1)
		{
			printNodesRecursive(i, "", false);
			LOG("");
		}
	}

	// 7. parse VERTICES and INDICES
	// all vertex data is stored in a single flat vector, while index data is stored in separate vectors for each submesh
	// ____________________

	LOG("\n\033[37m[Init] Parsing vertex and index data.\033[0m");

	indices.resize(totalSubmeshCount);
	int currentSubmeshIndex = 0;

	for (int i = 0; i < meshCount; i++)
	{
		int vertexBase = (int)(vertices.size() / 8); // [FIX] to convert vertex indices from local to global range

		char *meshVertexDataPtr = DataBuffer + meshVertexDataOffset[i] + 4;

		for (int j = 0; j < meshVertexCount[i]; j++)
		{
			float vertex[8]; // each vertex has 3 position, 3 normal, and 2 texture coordinates (total of 8 float components; in fact, in the .bdae file there are more than 8 variables per vertex, that's why bytesPerVertex is more than 8 * sizeof(float))
			memcpy(vertex, meshVertexDataPtr + j * bytesPerVertex[i], sizeof(vertex));

			vertices.push_back(vertex[0]); // X
			vertices.push_back(vertex[1]); // Y
			vertices.push_back(vertex[2]); // Z

			vertices.push_back(vertex[3]); // Nx
			vertices.push_back(vertex[4]); // Ny
			vertices.push_back(vertex[5]); // Nz

			vertices.push_back(vertex[6]); // U
			vertices.push_back(vertex[7]); // V
		}

		for (int k = 0; k < submeshCount[i]; k++)
		{
			char *submeshIndexDataPtr = DataBuffer + submeshIndexDataOffset[i][k] + 4;

			for (int l = 0; l < submeshTriangleCount[i][k]; l++)
			{
				unsigned short triangle[3];
				memcpy(triangle, submeshIndexDataPtr + l * sizeof(triangle), sizeof(triangle));

				indices[currentSubmeshIndex].push_back(triangle[0] + vertexBase);
				indices[currentSubmeshIndex].push_back(triangle[1] + vertexBase);
				indices[currentSubmeshIndex].push_back(triangle[2] + vertexBase);
				faceCount++;
			}

			currentSubmeshIndex++;
		}
	}

	vertexCount = vertices.size() / 8;

	// 8. parse BONES and match bones with nodes
	// bone is a "job" given to a node in animated models, allowing it to influence specific vertices rather than the entire mesh; bones form the model’s skeleton, while the influenced (or “skinned”) vertices act as the model’s skin
	// ____________________

	// [TODO] parse bind shape transformation matrix, maybe it will fix mismatch between meshes and nodes for some models

	if (meshSkinCount == 0)
		LOG("[Init] Skipping bones parsing. This is a non-skinned model.\033[0m");
	else if (meshSkinCount != 1)
	{
		LOG("[Error] Model::init unhandled mesh skin count (this value is always expected to be equal to 1).");
		return -1;
	}
	else
	{
		LOG("\n\033[37m[Init] Mesh skinning detected. Parsing bones data.\033[0m");

		hasSkinningData = true;

		int meshSkinDataOffset;

		memcpy(&meshSkinDataOffset, DataBuffer + header->offsetData + 128 + 4 + meshSkinMetadataOffset + 16, sizeof(int));

		int bindPoseDataOffset; // bone count * 16 floats (4 x 4 matrix for each bone)
		int boneCount, boneNamesOffset;
		int boneInfluenceFloatCount, boneInfluenceDataOffset; // vertex count * (4 bytes for bone indices + maxInfluence floats for bone weights)
		int maxInfluence;									  // how many bones can influence one vertex

		memcpy(&bindPoseDataOffset, DataBuffer + header->offsetData + 128 + 4 + meshSkinMetadataOffset + 16 + meshSkinDataOffset + 4, sizeof(int));
		memcpy(&boneCount, DataBuffer + header->offsetData + 128 + 4 + meshSkinMetadataOffset + 16 + meshSkinDataOffset + 120, sizeof(int));
		memcpy(&boneNamesOffset, DataBuffer + header->offsetData + 128 + 4 + meshSkinMetadataOffset + 16 + meshSkinDataOffset + 124, sizeof(int));
		memcpy(&boneInfluenceFloatCount, DataBuffer + header->offsetData + 128 + 4 + meshSkinMetadataOffset + 16 + meshSkinDataOffset + 128, sizeof(int));
		memcpy(&boneInfluenceDataOffset, DataBuffer + header->offsetData + 128 + 4 + meshSkinMetadataOffset + 16 + meshSkinDataOffset + 136, sizeof(int));
		memcpy(&maxInfluence, DataBuffer + header->offsetData + 128 + 4 + meshSkinMetadataOffset + 16 + meshSkinDataOffset + 176, sizeof(int));

		if (maxInfluence < 1 || maxInfluence > 4)
		{
			LOG("[Error] Model::init invalid max influence value: ", maxInfluence);
			return -1;
		}

		LOG("One vertex can be influenced by up to ", maxInfluence, " bones.");

		LOG("\nBONES: ", boneCount);

		if (boneCount > 0)
		{
			boneNames.resize(boneCount);
			bindPoseMatrices.resize(boneCount);

			for (int i = 0; i < boneCount; i++)
			{
				BDAEint boneNameOffset;
				int boneNameLength;

				memcpy(&boneNameOffset, DataBuffer + header->offsetData + 128 + 4 + meshSkinMetadataOffset + 16 + meshSkinDataOffset + 124 + boneNamesOffset + i * 8, sizeof(BDAEint));
				memcpy(&boneNameLength, DataBuffer + boneNameOffset - 4, sizeof(int));

				boneNames[i] = std::string((DataBuffer + boneNameOffset), boneNameLength);

				LOG("[", i + 1, "] \033[96m", boneNames[i], "\033[0m");

				memcpy(&bindPoseMatrices[i], DataBuffer + header->offsetData + 128 + 4 + meshSkinMetadataOffset + 16 + meshSkinDataOffset + 4 + bindPoseDataOffset + i * 64, sizeof(glm::mat4));
			}
		}

		/* map bones to nodes
		   each bone should (and can) be mapped to only one node
		   each node may be mapped to only one bone, or not mapped at all */

		// previously, when recursively parsing the node tree, we mapped bone names to node indices, however we didn't know the actual number of bones (so unmapped bones couldn’t be determined)
		// now we build a hash table that includes all bones and stores bone indices instead of names

		for (int i = 0; i < boneCount; i++)
		{
			auto it = boneNameToNodeIdx.find(boneNames[i]);

			if (it != boneNameToNodeIdx.end())
			{
				int nodeIndex = it->second;
				boneToNodeIdx[i] = nodeIndex;
			}
			else
			{
				LOG("[Warning] Model::init bone [", i + 1, "] ", boneNames[i], " is unmapped.");
				boneToNodeIdx[i] = -1;
			}
		}

		boneInfluences.resize(vertexCount * 4); // the actual size of this vector should be boneInfluenceFloatCount, but we resize to 4 influence pairs (bone index, bone weight) per vertex for convenience: to further create a fixed-size buffer that aligns with the GPU's expectation, which simplifies shader logic

		// loop through each vertex (because boneInfluenceFloatCount / (maxInfluence + 1) = vertexCount)
		for (int i = 0; i < boneInfluenceFloatCount / (maxInfluence + 1); i++)
		{
			char boneIndices[4] = {0};				  // the number of bone indices is always 4 (effectively only maxInfluence indices are stored, and the remaining elements are reserved to align to 4 bytes)
			float boneWeights[maxInfluence] = {0.0f}; // the number of bone weights equals maxInfluence

			memcpy(boneIndices, DataBuffer + boneInfluenceDataOffset + 4 + i * (maxInfluence + 1) * 4, 4);
			memcpy(boneWeights, DataBuffer + boneInfluenceDataOffset + 4 + i * (maxInfluence + 1) * 4 + 4, maxInfluence * sizeof(float));

			for (int j = 0; j < maxInfluence; j++)
			{
				boneInfluences[i * 4 + j].first = boneIndices[j];
				boneInfluences[i * 4 + j].second = boneWeights[j];
			}
		}
	}

	LOG("\n\033[1m\033[38;2;200;200;200m[Init] Finishing Model::init..\033[0m\n");

	delete header;
	return 0;
}

//! Recursively parses a node and its children.
void Model::parseNodesRecursive(int nodeOffset, int parentIndex)
{
	// read node data
	BDAEint name1Offset, name2Offset, name3Offset;
	int name1Length, name2Length, name3Length;

	memcpy(&name1Offset, DataBuffer + nodeOffset, sizeof(BDAEint));
	memcpy(&name2Offset, DataBuffer + nodeOffset + 8, sizeof(BDAEint));
	memcpy(&name3Offset, DataBuffer + nodeOffset + 16, sizeof(BDAEint));

	memcpy(&name1Length, DataBuffer + name1Offset - 4, sizeof(int));
	memcpy(&name2Length, DataBuffer + name2Offset - 4, sizeof(int));
	memcpy(&name3Length, DataBuffer + name3Offset - 4, sizeof(int));

	std::string name1(DataBuffer + name1Offset, name1Length);
	std::string name2(DataBuffer + name2Offset, name2Length);

	std::string name3 = "";

	if (name3Length > 0)
		name3 = std::string(DataBuffer + name3Offset, name3Length);

	float transX, transY, transZ;
	float rotX, rotY, rotZ, rotW;
	float scaleX, scaleY, scaleZ;
	int childrenCount, childrenOffset;

	memcpy(&transX, DataBuffer + nodeOffset + 24, sizeof(float));
	memcpy(&transY, DataBuffer + nodeOffset + 28, sizeof(float));
	memcpy(&transZ, DataBuffer + nodeOffset + 32, sizeof(float));
	memcpy(&rotX, DataBuffer + nodeOffset + 36, sizeof(float));
	memcpy(&rotY, DataBuffer + nodeOffset + 40, sizeof(float));
	memcpy(&rotZ, DataBuffer + nodeOffset + 44, sizeof(float));
	memcpy(&rotW, DataBuffer + nodeOffset + 48, sizeof(float));
	memcpy(&scaleX, DataBuffer + nodeOffset + 52, sizeof(float));
	memcpy(&scaleY, DataBuffer + nodeOffset + 56, sizeof(float));
	memcpy(&scaleZ, DataBuffer + nodeOffset + 60, sizeof(float));
	memcpy(&childrenCount, DataBuffer + nodeOffset + 68, sizeof(int));
	memcpy(&childrenOffset, DataBuffer + nodeOffset + 72, sizeof(int));

	// create new node
	Node node;
	node.ID = name1;
	node.mainName = name2;
	node.boneName = name3; // some nodes are not mapped to bones, though this name may be empty
	node.parentIndex = parentIndex;
	node.localTranslation = glm::vec3(transX, transY, transZ);
	node.localRotation = glm::quat(-rotW, rotX, rotY, rotZ);
	node.localScale = glm::vec3(scaleX, scaleY, scaleZ);

	// save original transformation for each node for correct animation reset
	node.defaultTranslation = node.localTranslation;
	node.defaultRotation = node.localRotation;
	node.defaultScale = node.localScale;

	nodes.push_back(node);

	int nodeIndex = nodes.size() - 1; // this new node is the last element in the node list

	// add to hash table, allowing to instantly get the node index by any of its 3 names
	nodeNameToIdx[node.ID] = nodeIndex;
	nodeNameToIdx[node.mainName] = nodeIndex;

	if (node.boneName != "")
	{
		nodeNameToIdx[node.boneName] = nodeIndex;
		boneNameToNodeIdx[node.boneName] = nodeIndex;
	}

	// update parent's children list
	if (parentIndex != -1)
		nodes[parentIndex].childIndices.push_back(nodeIndex);

	if (childrenCount > 0 && childrenOffset > 0)
	{
		for (int i = 0; i < childrenCount; i++)
			parseNodesRecursive(nodeOffset + 72 + childrenOffset + i * 96, nodeIndex);
	}
}

//! Recursively computes total transformation matrix for a node and its children.
void Model::updateNodesTransformationsRecursive(int nodeIndex, const glm::mat4 &parentTransform)
{
	Node &currNode = nodes[nodeIndex];

	// translate -> rotate -> scale
	glm::mat4 localTransform(1.0f);
	localTransform = glm::translate(glm::mat4(1.0f), currNode.localTranslation);
	localTransform *= glm::mat4_cast(currNode.localRotation);
	localTransform *= glm::scale(glm::mat4(1.0f), currNode.localScale);

	// total transformation of a node within a .bdae model = parent * local * pivot
	nodes[nodeIndex].totalTransform = parentTransform * localTransform * currNode.pivotTransform;

	for (int i = 0; i < currNode.childIndices.size(); i++)
	{
		int childIndex = currNode.childIndices[i];
		updateNodesTransformationsRecursive(childIndex, parentTransform * localTransform); // pass without PIVOT transformation to prevent its accumulation effect (each child and grandchild store the same PIVOT matrix)
	}
}

//! Recursively searches down the tree starting from a given node for the first node with '_PIVOT' in its ID and returns its local transformation matrix.
glm::mat4 Model::getPIVOTNodeTransformationRecursive(int nodeIndex)
{
	Node &currNode = nodes[nodeIndex];

	for (int i = 0; i < currNode.childIndices.size(); i++)
	{
		int childIndex = currNode.childIndices[i];
		Node &childNode = nodes[childIndex];

		if (childNode.ID.find("_PIVOT") != std::string::npos)
		{
			glm::mat4 pivotTransform(1.0f);
			pivotTransform = glm::translate(glm::mat4(1.0f), childNode.localTranslation);
			pivotTransform *= glm::mat4_cast(childNode.localRotation);
			pivotTransform *= glm::scale(glm::mat4(1.0f), childNode.localScale);

			return pivotTransform;
		}

		glm::mat4 pivotTransform = getPIVOTNodeTransformationRecursive(childIndex);

		if (pivotTransform != glm::mat4(1.0f))
			return pivotTransform;
	}

	return glm::mat4(1.0f);
}

//! [debug] Recursively prints the node tree.
void Model::printNodesRecursive(int nodeIndex, const std::string &prefix, bool isLastChild)
{
	std::ostringstream ss;
	ss << prefix;

	if (!prefix.empty())
		ss << (isLastChild ? "└── " : "├── ");

	ss << "[" << nodeIndex + 1 << "] " << "\033[96m" << nodes[nodeIndex].ID << "\033[0m";

	int meshIndex = -1;

	for (auto it = meshToNodeIdx.begin(); it != meshToNodeIdx.end(); it++)
	{
		if (it->second == nodeIndex)
		{
			meshIndex = it->first;
			break;
		}
	}

	if (meshIndex != -1)
		ss << " --> [" << (meshIndex + 1) << "] mesh";

	if (!nodes[nodeIndex].boneName.empty())
		ss << " --> " << nodes[nodeIndex].boneName;

	LOG(ss.str());

	std::vector<int> &children = nodes[nodeIndex].childIndices;

	for (int i = 0; i < children.size(); i++)
		printNodesRecursive(children[i], prefix + (isLastChild ? "    " : "│   "), (i + 1 == children.size()));
}

//! Loads .bdae file from disk, calls the parser and searches for alternative textures and sounds.
void Model::load(const char *fpath, Sound &sound, bool isTerrainViewer)
{
	reset();

	// 1. load .bdae file
	CPackPatchReader *bdaeArchive;

	if (isTerrainViewer)
		bdaeArchive = new CPackPatchReader((std::string("data/model/unsorted/") + (fpath + 6)).c_str(), true, false); // open outer .bdae archive file
	else
		bdaeArchive = new CPackPatchReader(fpath, true, false);

	if (!bdaeArchive)
		return;

	IReadResFile *bdaeFile = bdaeArchive->openFile("little_endian_not_quantized.bdae"); // open inner .bdae file

	if (!bdaeFile)
	{
		delete bdaeArchive;
		return;
	}

	LOG("\033[1m\033[97mLoading ", fpath, "\033[0m");

	std::string modelPath(fpath);
	std::replace(modelPath.begin(), modelPath.end(), '\\', '/');	// normalize model path for cross-platform compatibility (Windows uses '\', Linux uses '/')
	fileName = modelPath.substr(modelPath.find_last_of("/\\") + 1); // file name is after the last path separator in the full path

	// 2. run the parser
	int result = init(bdaeFile);

	if (result != 0)
	{
		delete bdaeFile;
		delete bdaeArchive;
		return;
	}

	LOG("\n\033[37m[Load] BDAE initialization success.\033[0m");

	if (!isTerrainViewer) // 3D model viewer
	{
		// compute the mesh's center in world space for its correct rotation (instead of always rotating around the origin (0, 0, 0))
		meshCenter = glm::vec3(0.0f);

		for (int i = 0, n = vertices.size() / 8; i < n; i++)
		{
			meshCenter.x += vertices[i * 8 + 0];
			meshCenter.y += vertices[i * 8 + 1];
			meshCenter.z += vertices[i * 8 + 2];
		}

		meshCenter /= (vertices.size() / 8);

		// 3. process strings retrieved from .bdae
		const char *subpathStart = std::strstr(modelPath.c_str(), "/model/") + 7; // subpath starts after '/model/' (texture and model files have the same subpath, e.g. 'creature/pet/')
		const char *subpathEnd = std::strrchr(modelPath.c_str(), '/') + 1;		  // last '/' before the file name
		std::string textureSubpath(subpathStart, subpathEnd);

		bool isUnsortedFolder = false; // for 'unsorted' folder

		if (textureSubpath.rfind("unsorted/", 0) == 0)
			isUnsortedFolder = true;

		// post-process retrieved texture names
		for (int i = 0, n = (int)textureNames.size(); i < n; i++)
		{
			std::string &s = textureNames[i];

			if (s.length() <= 4)
				continue;

			// convert to lowercase
			for (char &c : s)
				c = std::tolower(c);

			// remove 'avatar/' if it exists
			int avatarPos = s.find("avatar/");
			if (avatarPos != (int)std::string::npos && !isUnsortedFolder)
				s.erase(avatarPos, 7);

			// remove 'texture/' if it exists
			if (s.rfind("texture/", 0) == 0)
				s.erase(0, 8);

			// replace the ending with '.png'
			s.replace(s.length() - 4, 4, ".png");

			// build final path
			if (!isUnsortedFolder)
				s = "data/texture/" + textureSubpath + s;
			else
				s = "data/texture/unsorted/" + s;
		}

		// if a texture file matching the model file name exists, override the parsed texture (for single-texture models only)
		std::string s = "data/texture/" + textureSubpath + fileName;
		s.replace(s.length() - 5, 5, ".png");

		if (textureCount == 1 && std::filesystem::exists(s))
		{
			textureNames.clear();
			textureNames.push_back(s);
		}

		// if a texture name is missing in the .bdae file, use this file's name instead (assuming the texture file was manually found and named)
		if (textureNames.empty())
		{
			textureNames.push_back(s);
			textureCount++;
		}

		// 4. search for alternative color texture files
		// [TODO] handle for multi-texture models
		if (textureNames.size() == 1 && std::filesystem::exists(textureNames[0]) && !isUnsortedFolder)
		{
			std::filesystem::path texturePath("data/texture/" + textureSubpath);
			std::string baseTextureName = std::filesystem::path(textureNames[0]).stem().string(); // texture file name without extension or folder (e.g. 'boar_01' or 'puppy_bear_black')

			std::string groupName; // name shared by a group of related textures

			// naming rule #1
			if (baseTextureName.find("lvl") != std::string::npos && baseTextureName.find("world") != std::string::npos)
				groupName = baseTextureName;

			// naming rule #2
			for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(texturePath))
			{
				if (!entry.is_regular_file())
					continue;

				std::filesystem::path entryPath = entry.path();

				if (entryPath.extension() != ".png")
					continue;

				std::string baseEntryName = entryPath.stem().string();

				if (baseEntryName.rfind(baseTextureName + '_', 0) == 0 &&								   // starts with '<baseTextureName>_'
					baseEntryName.size() > baseTextureName.size() + 1 &&								   // has at least one character after the underscore
					std::isdigit(static_cast<unsigned char>(baseEntryName[baseTextureName.size() + 1])) && // first character after '_' is a digit
					entryPath.string() != textureNames[0])												   // not the original base texture itself
				{
					groupName = baseTextureName;
					break;
				}
			}

			// for a numeric suffix (e.g. '_01', '_2'), remove it if exists (to derive a group name for searching potential alternative textures, e.g 'boar')
			if (groupName.empty())
			{
				auto lastUnderscore = baseTextureName.rfind('_');

				if (lastUnderscore != std::string::npos)
				{
					std::string afterLastUnderscore = baseTextureName.substr(lastUnderscore + 1);

					if (!afterLastUnderscore.empty() && std::all_of(afterLastUnderscore.begin(), afterLastUnderscore.end(), ::isdigit))
						groupName = baseTextureName.substr(0, lastUnderscore);
				}
			}

			// for a non numeric‑suffix (e.g. '_black'), use the “max‑match” approach to find the best group name
			if (groupName.empty())
			{
				// build a list of all possible prefixes (e.g. 'puppy_black_bear', 'puppy_black', 'puppy')
				std::vector<std::string> prefixes;
				std::string s = baseTextureName;

				while (true)
				{
					prefixes.push_back(s);
					auto pos = s.rfind('_');

					if (pos == std::string::npos)
						break;

					s.resize(pos); // remove the last '_suffix'
				}

				// try each prefix and find the one that gives the highest number of matching texture files
				int bestCount = 0;

				for (int i = 0, n = prefixes.size(); i < n; i++)
				{
					int count = 0;
					std::string pref = prefixes[i];

					// skip single-word prefixes ('puppy' cannot be a group name, otherwise puppy_wolf.png could be an alternative)
					if (pref.find('_') == std::string::npos)
						continue;

					// loop through each file in the texture directory and count how many .png files start with '<pref>_'
					for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(texturePath))
					{
						if (!entry.is_regular_file())
							continue;

						std::filesystem::path entryPath = entry.path();

						if (entryPath.extension() != ".png")
							continue;

						if (entryPath.stem().string().rfind(pref + '_', 0) == 0)
							count++;
					}

					// compare and update the best count; if two prefixes match the same number of textures, prefer the longer one
					if (count > bestCount || (count == bestCount && pref.length() > groupName.length()))
					{
						bestCount = count;
						groupName = pref;
					}
				}
			}

			// finally, collect textures based on the best group name
			if (!groupName.empty())
			{
				std::vector<std::string> found;

				for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(texturePath))
				{
					if (!entry.is_regular_file())
						continue;

					std::filesystem::path entryPath = entry.path();

					if (entryPath.extension() != ".png")
						continue;

					// skip the file if its name doesn't exactly match the group name, and doesn’t start with the group name followed by an underscore
					if (!(entryPath.stem().string() == groupName || entryPath.stem().string().rfind(groupName + '_', 0) == 0))
						continue;

					std::string alternativeTextureName = "data/texture/" + textureSubpath + entryPath.filename().string();

					// skip the original base texture (already in textureNames[0])
					if (alternativeTextureName == textureNames[0])
						continue;

					// ensure it is a unique texture name
					if (std::find(textureNames.begin(), textureNames.end(), alternativeTextureName) == textureNames.end())
					{
						found.push_back(alternativeTextureName);
						alternativeTextureCount++;
					}
				}

				if (!found.empty())
				{
					// append and report
					textureNames.insert(textureNames.end(), found.begin(), found.end());

					LOG("Found ", found.size(), " alternative(s) for '", groupName, "':");

					for (int i = 0; i < (int)found.size(); i++)
						LOG("  ", found[i]);
				}
				else
					LOG("No alternatives found for group '", groupName, "'");
			}
			else
				LOG("No valid grouping name for '", baseTextureName, "'");
		}

		// 5. search for sounds
		sound.searchSoundFiles(fileName, sounds);

		LOG("\nSOUNDS: ", ((sounds.size() != 0) ? sounds.size() : 0));

		for (int i = 0; i < (int)sounds.size(); i++)
			LOG("[", i + 1, "]  ", sounds[i]);
	}
	else // terrain viewer
	{
		LOG("\033[37m[Load] Terrain viewer mode. Post-processing texture names.\033[0m");

		for (int i = 0, n = (int)textureNames.size(); i < n; i++)
		{
			std::string &s = textureNames[i];

			if (s.length() <= 4)
				continue;

			for (char &c : s)
				c = std::tolower(c);

			if (s.rfind("texture/", 0) == 0)
				s.erase(0, 8);

			s.replace(s.length() - 4, 4, ".png");
			s = "data/texture/unsorted/" + s;
		}
	}

	free(DataBuffer);

	delete bdaeFile;
	delete bdaeArchive;

	// 6. setup buffers
	if (!isTerrainViewer)
	{
		LOG("\n\033[37m[Load] Uploading vertex data to GPU.\033[0m");
		EBOs.resize(totalSubmeshCount);
		glGenVertexArrays(1, &VAO);					  // generate a Vertex Array Object to store vertex attribute configurations
		glGenBuffers(1, &VBO);						  // generate a Vertex Buffer Object to store vertex data
		glGenBuffers(totalSubmeshCount, EBOs.data()); // generate an Element Buffer Object for each submesh to store index data

		glBindVertexArray(VAO); // bind the VAO first so that subsequent VBO bindings and vertex attribute configurations are stored in it correctly

		glBindBuffer(GL_ARRAY_BUFFER, VBO);																 // bind the VBO
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW); // copy vertex data into the GPU buffer's memory

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0); // define the layout of the vertex data (vertex attribute configuration): index 0, 3 components per vertex, type float, not normalized, with a stride of 8 * sizeof(float) (next vertex starts after 8 floats), and an offset of 0 in the buffer
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		for (int i = 0; i < totalSubmeshCount; i++)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices[i].size() * sizeof(unsigned short), indices[i].data(), GL_STATIC_DRAW);
		}

		// Update vertex buffer with bone data if skinning is present
		updateVertexBufferWithBoneData();

		// Automatically search and load all animation files
		searchAnimationFiles(fpath, false); // isUnsortedFolder will be recalculated inside
	}

	// 7. load texture(s)
	LOG("\033[37m[Load] Uploading textures to GPU.\033[0m");

	textures.resize(textureNames.size());
	glGenTextures(textureNames.size(), textures.data()); // generate and store texture ID(s)

	for (int i = 0; i < (int)textureNames.size(); i++)
	{
		glBindTexture(GL_TEXTURE_2D, textures[i]); // bind the texture ID so that all upcoming texture operations affect this texture

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // for u (x) axis
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // for v (y) axis

		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		int width, height, nrChannels, format;
		unsigned char *data = stbi_load(textureNames[i].c_str(), &width, &height, &nrChannels, 0); // load the image and its parameters

		if (!data)
		{
			std::cerr << "Failed to load texture: " << textureNames[i] << "\n";
			continue;
		}

		format = (nrChannels == 4) ? GL_RGBA : GL_RGB;											  // image format
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data); // create and store texture image inside the texture object (upload to GPU)
		glGenerateMipmap(GL_TEXTURE_2D);
		stbi_image_free(data);
	}

	// Generate unit sphere for node visualization
	generateUnitSphere();

	modelLoaded = true;
	LOG("\033[1m\033[38;2;200;200;200m[Load] BDAE model loaded.\033[0m\n");
}

//! Recreates the vertex buffer to include bone IDs and weights.
void Model::updateVertexBufferWithBoneData()
{
	if (!hasSkinningData || boneInfluences.empty())
		return;

	LOG("\n\033[1m\033[96m[UpdateVBO] Updating vertex buffer with bone data...\033[0m");

	int oldVertexCount = vertices.size() / 8;
	int boneDataVertexCount = boneInfluences.size() / 4;

	if (oldVertexCount != boneDataVertexCount)
	{
		LOG("\033[31m[UpdateVBO] ERROR: Vertex count mismatch! (", oldVertexCount, " != ", boneDataVertexCount, ")\033[0m");
		return;
	}

	// Create new interleaved data: Pos(3), Normal(3), TexCoord(2), BoneIDs(4), BoneWeights(4)
	std::vector<float> newVertexData;
	newVertexData.reserve(oldVertexCount * 16);

	for (int i = 0; i < oldVertexCount; i++)
	{
		// Copy old data (position, normal, texcoord)
		newVertexData.insert(newVertexData.end(), vertices.begin() + i * 8, vertices.begin() + i * 8 + 8);

		// Add bone data from the vector of pairs
		for (int j = 0; j < 4; j++)
		{
			const auto &influence = boneInfluences[i * 4 + j];
			newVertexData.push_back(static_cast<float>(influence.first)); // boneID
		}
		for (int j = 0; j < 4; j++)
		{
			const auto &influence = boneInfluences[i * 4 + j];
			newVertexData.push_back(influence.second); // weight
		}
	}

	vertices = newVertexData;

	// Re-upload data to GPU and update attribute pointers
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	// New stride is 16 floats
	const int stride = 16 * sizeof(float);
	// Position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
	glEnableVertexAttribArray(0);
	// Normal
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// TexCoord
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	// Bone IDs (as floats)
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void *)(8 * sizeof(float)));
	glEnableVertexAttribArray(3);
	// Bone Weights
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void *)(12 * sizeof(float)));
	glEnableVertexAttribArray(4);

	glBindVertexArray(0);

	LOG("  \033[32m✓ Vertex buffer updated with skinning data. New stride: ", stride, " bytes.\033[0m");
}

// Animation functions to be added to parserBDAE.cpp

//! Search for and load all animation files based on model path.
void Model::searchAnimationFiles(const char *modelPath, bool isUnsortedFolder)
{
	std::string modelPathStr(modelPath);
	std::replace(modelPathStr.begin(), modelPathStr.end(), '\\', '/');

	// Detect if this is an unsorted folder model
	bool isUnsorted = (modelPathStr.find("/model/unsorted/") != std::string::npos);

	// Get the directory containing the model
	size_t lastSlash = modelPathStr.find_last_of('/');
	if (lastSlash == std::string::npos)
		return;

	std::string modelDir = modelPathStr.substr(0, lastSlash);

	// Get model name without extension
	std::string modelFileName = modelPathStr.substr(lastSlash + 1);
	size_t dotPos = modelFileName.rfind(".bdae");
	if (dotPos == std::string::npos)
		return;

	std::string modelName = modelFileName.substr(0, dotPos);

	std::string animDir;

	if (isUnsorted)
	{
		// For unsorted models, look in "anim" subfolder
		animDir = modelDir + "/anim";
	}
	else
	{
		// For regular models, look in "animations/model_name" folder
		animDir = modelDir + "/animations/" + modelName;
	}

	LOG("\n\033[37m[Load] Searching for animations in: ", animDir, "\033[0m");
	LOG("\033[37m[Load] Model path: ", modelPathStr, "\033[0m");
	LOG("\033[37m[Load] Is unsorted: ", (isUnsorted ? "YES" : "NO"), "\033[0m");

	// Check if animation directory exists
	if (!std::filesystem::exists(animDir) || !std::filesystem::is_directory(animDir))
	{
		LOG("\033[37m[Load] Animation directory not found: ", animDir, "\033[0m");
		return;
	}

	// Search for all .bdae files in the animation directory
	std::vector<std::string> animationFiles;

	for (const auto &entry : std::filesystem::directory_iterator(animDir))
	{
		if (entry.is_regular_file())
		{
			std::string filePath = entry.path().string();
			if (filePath.size() >= 5 && filePath.substr(filePath.size() - 5) == ".bdae")
			{
				animationFiles.push_back(filePath);
			}
		}
	}

	if (animationFiles.empty())
	{
		LOG("\033[37m[Load] No animation files found.\033[0m");
		return;
	}

	// Sort animation files for consistent ordering
	std::sort(animationFiles.begin(), animationFiles.end());

	LOG("\033[37m[Load] Found ", animationFiles.size(), " animation file(s).\033[0m");

	// Load all animation files
	for (const std::string &animFile : animationFiles)
	{
		animationFileNames.push_back(animFile);
		loadAnimations(animFile.c_str());
	}

	animationCount = (int)animationFiles.size();

	if (animationCount > 0)
	{
		selectedAnimation = 0; // Select first animation by default
	}
}

//! Load animations from a separate .bdae animation file.
void Model::loadAnimations(const char *animationFilePath)
{
	LOG("\n\033[1m\033[97mLoading animations from ", animationFilePath, "\033[0m");

	// Create temporary vectors for this animation file
	std::vector<Animation> currentAnimations;
	std::vector<KeyframeSource> currentKeyframeSources;

	// Open animation file
	CPackPatchReader *animArchive = new CPackPatchReader(animationFilePath, true, false);
	if (!animArchive)
	{
		LOG("[Error] Failed to open animation archive");
		return;
	}

	IReadResFile *animFile = animArchive->openFile("little_endian_not_quantized.bdae");
	if (!animFile)
	{
		LOG("[Error] Failed to open animation file inside archive");
		delete animArchive;
		return;
	}

	// Read header
	BDAEFileHeader header;
	animFile->read(&header, sizeof(header));

	// Read entire file into buffer
	int animFileSize = animFile->getSize();
	char *animBuffer = (char *)malloc(animFileSize);
	animFile->seek(0);
	animFile->read(animBuffer, animFileSize);

	// Helper function to read string at offset
	auto readString = [&](uint32_t offset) -> std::string
	{
		if (offset < 4 || offset >= animFileSize)
			return "";
		int length;
		memcpy(&length, animBuffer + offset - 4, sizeof(int));
		if (length <= 0 || length > 200)
			return "";
		return std::string(animBuffer + offset, length);
	};

	// Helper function to parse source data
	auto parseSourceData = [&](BDAEint sourceOffset, DataType dataType, int elementCount) -> std::vector<float>
	{
		std::vector<float> result;
		if (sourceOffset >= animFileSize)
			return result;

		for (int i = 0; i < elementCount; i++)
		{
			float value = 0.0f;
			switch (dataType)
			{
			case DataType::BYTE:
			{
				int8_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 1, 1);
				value = (float)val;
				break;
			}
			case DataType::UBYTE:
			{
				uint8_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 1, 1);
				value = (float)val;
				break;
			}
			case DataType::SHORT:
			{
				int16_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 2, 2);
				value = (float)val;
				break;
			}
			case DataType::USHORT:
			{
				uint16_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 2, 2);
				value = (float)val;
				break;
			}
			case DataType::INT:
			{
				int32_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 4, 4);
				value = (float)val;
				break;
			}
			case DataType::UINT:
			{
				uint32_t val;
				memcpy(&val, animBuffer + sourceOffset + i * 4, 4);
				value = (float)val;
				break;
			}
			case DataType::FLOAT:
			{
				memcpy(&value, animBuffer + sourceOffset + i * 4, 4);
				break;
			}
			}
			result.push_back(value);
		}
		return result;
	};

	// Find SLibraryAnimations (at offsetData + 56)
	BDAEint libAnimOffset = header.offsetData + 56;
	uint32_t animCount, animOffsetRel;
	memcpy(&animCount, animBuffer + libAnimOffset, sizeof(uint32_t));
	memcpy(&animOffsetRel, animBuffer + libAnimOffset + 4, sizeof(uint32_t));

	BDAEint animArrayOffset = libAnimOffset + 4 + animOffsetRel;

	LOG("Animation count: ", animCount);

	// Find SAnimationData (sources array) - search for size field = animCount * 2
	BDAEint sourcesOffset = 0;
	uint32_t expectedSourcesCount = animCount * 2;

	for (BDAEint pos = header.offsetData; pos < animFileSize - 8; pos++)
	{
		uint32_t count;
		memcpy(&count, animBuffer + pos, sizeof(uint32_t));
		if (count == expectedSourcesCount)
		{
			sourcesOffset = pos;
			break;
		}
	}

	if (sourcesOffset == 0)
	{
		LOG("[Warning] Could not find SAnimationData structure");
		free(animBuffer);
		delete animFile;
		delete animArchive;
		return;
	}

	LOG("Found SAnimationData at offset: 0x", std::hex, sourcesOffset, std::dec);

	// Parse sources array
	uint32_t sourcesCount;
	memcpy(&sourcesCount, animBuffer + sourcesOffset, sizeof(uint32_t));
	BDAEint sourcesArrayOffset = sourcesOffset + 4;

	LOG("Sources count: ", sourcesCount);

	// Read source descriptors
	struct SourceDesc
	{
		uint32_t count;
		BDAEint dataOffset;
	};

	std::vector<SourceDesc> sourceDescs;
	for (uint32_t i = 0; i < sourcesCount; i++)
	{
		BDAEint descPos = sourcesArrayOffset + i * 8;
		SourceDesc desc;
		uint32_t offsetRel;
		memcpy(&desc.count, animBuffer + descPos, sizeof(uint32_t));
		memcpy(&offsetRel, animBuffer + descPos + 4, sizeof(uint32_t));
		desc.dataOffset = descPos + 4 + offsetRel;
		sourceDescs.push_back(desc);
	}

	// Parse each animation
	const int ANIM_STRUCT_SIZE = 40;

	for (uint32_t animIndex = 0; animIndex < animCount; animIndex++)
	{
		BDAEint animPos = animArrayOffset + animIndex * ANIM_STRUCT_SIZE;

		// Read SAnimation structure
		uint32_t idOffset, idPadding;
		uint32_t samplersCount, samplersOffsetRel;
		uint32_t channelsCount, channelsOffsetRel;

		memcpy(&idOffset, animBuffer + animPos, sizeof(uint32_t));
		memcpy(&idPadding, animBuffer + animPos + 4, sizeof(uint32_t));

		BDAEint samplersBase = animPos + 8;
		memcpy(&samplersCount, animBuffer + samplersBase, sizeof(uint32_t));
		memcpy(&samplersOffsetRel, animBuffer + samplersBase + 4, sizeof(uint32_t));
		BDAEint samplersOffset = samplersBase + 4 + samplersOffsetRel;

		BDAEint channelsBase = animPos + 16;
		memcpy(&channelsCount, animBuffer + channelsBase, sizeof(uint32_t));
		memcpy(&channelsOffsetRel, animBuffer + channelsBase + 4, sizeof(uint32_t));
		BDAEint channelsOffset = channelsBase + 4 + channelsOffsetRel;

		std::string animName = readString(idOffset);

		Animation anim;
		anim.name = animName;
		anim.duration = 0.0f;

		LOG("\n[", animIndex, "] ", animName);
		LOG("  Samplers: ", samplersCount);
		LOG("  Channels: ", channelsCount);

		// Parse samplers
		for (uint32_t s = 0; s < samplersCount; s++)
		{
			BDAEint samplerPos = samplersOffset + s * 28;

			AnimationSampler sampler;
			int32_t interp, inType, inComp, inSrc, outType, outComp, outSrc;

			memcpy(&interp, animBuffer + samplerPos, sizeof(int32_t));
			memcpy(&inType, animBuffer + samplerPos + 4, sizeof(int32_t));
			memcpy(&inComp, animBuffer + samplerPos + 8, sizeof(int32_t));
			memcpy(&inSrc, animBuffer + samplerPos + 12, sizeof(int32_t));
			memcpy(&outType, animBuffer + samplerPos + 16, sizeof(int32_t));
			memcpy(&outComp, animBuffer + samplerPos + 20, sizeof(int32_t));
			memcpy(&outSrc, animBuffer + samplerPos + 24, sizeof(int32_t));

			sampler.interpolation = (InterpolationType)interp;
			sampler.inputSourceIndex = inSrc;
			sampler.outputSourceIndex = outSrc;
			sampler.inputType = (DataType)inType;
			sampler.inputComponentCount = inComp;
			sampler.outputType = (DataType)outType;
			sampler.outputComponentCount = outComp;

			// Parse keyframe source data if not already parsed
			if (inSrc >= 0 && inSrc < (int)sourcesCount)
			{
				// Ensure currentKeyframeSources has enough space
				while ((int)currentKeyframeSources.size() <= inSrc)
				{
					currentKeyframeSources.push_back(KeyframeSource());
				}

				// The number of timestamps must equal the number of keyframes in the output data.
				int requiredTimeCount = sourceDescs[outSrc].count / sampler.outputComponentCount;

				// Re-parse if the cached source is empty or has the wrong count for this specific track.
				if (currentKeyframeSources[inSrc].data.empty() || currentKeyframeSources[inSrc].data.size() != requiredTimeCount)
				{
					currentKeyframeSources[inSrc].dataType = (DataType)inType;
					currentKeyframeSources[inSrc].componentCount = inComp;
					currentKeyframeSources[inSrc].data = parseSourceData(sourceDescs[inSrc].dataOffset, (DataType)inType, requiredTimeCount);

					// Convert time values from frames to seconds (BDAE uses 30 fps)
					// Time values in BDAE are stored as frame numbers, need to divide by 30
					const float BDAE_FPS = 30.0f;
					for (float &timeValue : currentKeyframeSources[inSrc].data)
					{
						timeValue /= BDAE_FPS;
					}

					// Update animation duration from time values
					if (!currentKeyframeSources[inSrc].data.empty())
					{
						float maxTime = *std::max_element(currentKeyframeSources[inSrc].data.begin(), currentKeyframeSources[inSrc].data.end());
						if (maxTime > anim.duration)
							anim.duration = maxTime;
					}
				}
			}

			if (outSrc >= 0 && outSrc < (int)sourcesCount)
			{
				while ((int)currentKeyframeSources.size() <= outSrc)
				{
					currentKeyframeSources.push_back(KeyframeSource());
				}

				if (currentKeyframeSources[outSrc].data.empty())
				{
					currentKeyframeSources[outSrc].dataType = (DataType)outType;
					currentKeyframeSources[outSrc].componentCount = outComp;
					currentKeyframeSources[outSrc].data = parseSourceData(sourceDescs[outSrc].dataOffset, (DataType)outType, sourceDescs[outSrc].count);
				}
			}

			anim.samplers.push_back(sampler);
		}

		// Parse channels
		for (uint32_t c = 0; c < channelsCount; c++)
		{
			BDAEint channelPos = channelsOffset + c * 24;

			AnimationChannel channel;
			uint32_t sourceOffset, sourcePadding, channelType, typePadding, reserved1, reserved2;

			memcpy(&sourceOffset, animBuffer + channelPos, sizeof(uint32_t));
			memcpy(&sourcePadding, animBuffer + channelPos + 4, sizeof(uint32_t));
			memcpy(&channelType, animBuffer + channelPos + 8, sizeof(uint32_t));
			memcpy(&typePadding, animBuffer + channelPos + 12, sizeof(uint32_t));
			memcpy(&reserved1, animBuffer + channelPos + 16, sizeof(uint32_t));
			memcpy(&reserved2, animBuffer + channelPos + 20, sizeof(uint32_t));

			channel.targetNodeName = readString(sourceOffset);
			channel.channelType = (ChannelType)channelType;
			channel.samplerIndex = c; // Channels and samplers are paired by index

			anim.channels.push_back(channel);
		}

		// DEBUG: Show ALL keyframe times for Bone022-translation
		if (anim.name.find("Bone022-node-translation") != std::string::npos ||
			anim.name.find("Bone22-node-translation") != std::string::npos)
		{
			LOG("\n  ========== DEBUG: ", anim.name, " ==========");
			if (!anim.samplers.empty() && anim.samplers[0].inputSourceIndex >= 0 &&
				anim.samplers[0].inputSourceIndex < (int)currentKeyframeSources.size())
			{
				const auto &timeData = currentKeyframeSources[anim.samplers[0].inputSourceIndex].data;
				const auto &valueData = currentKeyframeSources[anim.samplers[0].outputSourceIndex].data;
				LOG("  Input source index: ", anim.samplers[0].inputSourceIndex);
				LOG("  Output source index: ", anim.samplers[0].outputSourceIndex);
				LOG("  Keyframe count: ", timeData.size());
				LOG("  Value count: ", valueData.size());
				LOG("  Output component count: ", anim.samplers[0].outputComponentCount);
				LOG("  Actual keyframe count: ", valueData.size() / anim.samplers[0].outputComponentCount);
				LOG("\n  ALL KEYFRAME TIMES:");
				for (size_t i = 0; i < timeData.size(); i++)
				{
					size_t valueIndex = i * anim.samplers[0].outputComponentCount;
					if (valueIndex + 2 < valueData.size())
					{
						LOG("    [", i, "] time=", timeData[i], "s  value=(",
							valueData[valueIndex], ", ", valueData[valueIndex + 1], ", ", valueData[valueIndex + 2], ")");
					}
				}
			}
			LOG("  ==========================================\n");
		}

		currentAnimations.push_back(anim);
		LOG("  Duration: ", anim.duration, " seconds");
	}

	free(animBuffer);
	delete animFile;
	delete animArchive;

	// Add the loaded animations and keyframe sources to the sets
	animationSets.push_back(currentAnimations);
	keyframeSourceSets.push_back(currentKeyframeSources);

	animationsLoaded = true;
	LOG("\n\033[1m\033[38;2;200;200;200m[Load] Loaded ", currentAnimations.size(), " animations.\033[0m\n");
}
