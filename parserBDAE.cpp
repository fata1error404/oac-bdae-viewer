#include "parserBDAE.h"
#include <algorithm>
#include <filesystem>
#include <cmath>
#include "PackPatchReader.h"

//! Parses .bdae file and sets up model mesh and texture data.
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

	// 3. parse model info section: retrieve texture, material, mesh and node info

	// 3.1 TEXTURES AND MATERIALS (materials are parsed to match submesh with texture)
	// ____________________

	int textureInfoOffset;
	int materialCount, materialInfoOffset;
	int meshCount, meshInfoOffset;
	int nodeCollectionCount, nodeCollectionInfoOffset;

#ifdef BETA_GAME_VERSION
	char *ptr = DataBuffer + header->offsetData + 76; // points to texture info in the Data section
#else
	char *ptr = DataBuffer + header->offsetData + 96;
#endif

	memcpy(&textureCount, ptr, sizeof(int));
	memcpy(&textureInfoOffset, ptr + 4, sizeof(int));
	memcpy(&materialCount, ptr + 16, sizeof(int));
	memcpy(&materialInfoOffset, ptr + 20, sizeof(int));
	memcpy(&meshCount, ptr + 24, sizeof(int));
	memcpy(&meshInfoOffset, ptr + 28, sizeof(int));
	memcpy(&nodeCollectionCount, ptr + 72, sizeof(int));
	memcpy(&nodeCollectionInfoOffset, ptr + 76, sizeof(int));

	LOG("\033[37m[Init] Parsing model info section. Retrieving texture, material, mesh and node info.\033[0m");
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
			memcpy(&textureNameOffset, DataBuffer + textureInfoOffset + 8 + i * 20, sizeof(BDAEint));
			memcpy(&textureNameLength, DataBuffer + textureNameOffset - 4, sizeof(int));
#else
			memcpy(&textureNameOffset, DataBuffer + header->offsetData + 100 + textureInfoOffset + 16 + i * 40, sizeof(BDAEint));
			memcpy(&textureNameLength, DataBuffer + textureNameOffset - 4, sizeof(int));
#endif
			textureNames[i] = std::string((DataBuffer + textureNameOffset), textureNameLength);

			LOG("[", i + 1, "] ", textureNames[i]);
		}

		int materialPropertyCount[materialCount];
		int materialPropertyOffset[materialCount];
		std::vector<int> materialPropertyValueOffset[materialCount];

		for (int i = 0; i < materialCount; i++)
		{
#ifdef BETA_GAME_VERSION
			memcpy(&materialNameOffset[i], DataBuffer + materialInfoOffset + i * 36, sizeof(BDAEint));
			memcpy(&materialPropertyCount[i], DataBuffer + materialInfoOffset + 16 + i * 36, sizeof(int));
			memcpy(&materialPropertyOffset[i], DataBuffer + materialInfoOffset + 20 + i * 36, sizeof(int));
#else
			memcpy(&materialNameOffset[i], DataBuffer + header->offsetData + 116 + materialInfoOffset + i * 56, sizeof(BDAEint));
			memcpy(&materialPropertyCount[i], DataBuffer + header->offsetData + 148 + materialInfoOffset + i * 56, sizeof(int));
			memcpy(&materialPropertyOffset[i], DataBuffer + header->offsetData + 152 + materialInfoOffset + i * 56, sizeof(int));
#endif

			int propertyType = 0;

			for (int k = 0; k < materialPropertyCount[i]; k++)
			{
#ifdef BETA_GAME_VERSION
				memcpy(&propertyType, DataBuffer + materialPropertyOffset[i] + 8 + k * 24, sizeof(int));
#else
				memcpy(&propertyType, DataBuffer + header->offsetData + 152 + materialInfoOffset + i * 56 + materialPropertyOffset[i] + 16 + k * 32, sizeof(int));
#endif

				if (propertyType == 11)
				{
					int offset1, offset2, textureIndex;

#ifdef BETA_GAME_VERSION
					memcpy(&offset1, DataBuffer + materialPropertyOffset[i] + 20 + k * 24, sizeof(int));
					memcpy(&offset2, DataBuffer + offset1, sizeof(int));
					memcpy(&materialTextureIndex[i], DataBuffer + offset2, sizeof(int));
#else
					memcpy(&offset1, DataBuffer + header->offsetData + 152 + materialInfoOffset + i * 56 + materialPropertyOffset[i] + 28 + k * 32, sizeof(int));
					memcpy(&offset2, DataBuffer + header->offsetData + 152 + materialInfoOffset + i * 56 + materialPropertyOffset[i] + 28 + offset1 + k * 32, sizeof(int));
					memcpy(&materialTextureIndex[i], DataBuffer + header->offsetData + 152 + materialInfoOffset + i * 56 + materialPropertyOffset[i] + 28 + offset1 + offset2 + k * 32, sizeof(int));
#endif

					break;
				}
			}
		}
	}

	// 3.2 MESHES
	// ____________________

	LOG("\nMESHES: ", meshCount);

	int meshVertexCount[meshCount];
	int meshMetadataOffset[meshCount];
	int meshVertexDataOffset[meshCount];
	int bytesPerVertex[meshCount];
	int submeshCount[meshCount];
	int submeshMetadataOffset[meshCount];
	std::vector<int> submeshMaterialNameOffset[meshCount];
	std::vector<int> submeshTriangleCount[meshCount];
	std::vector<int> submeshIndexDataOffset[meshCount];

	std::vector<std::string> meshName(meshCount);
	meshTransform.assign(meshCount, glm::mat4(1.0f));
	meshPivotOffset.assign(meshCount, glm::mat4(1.0f));

	for (int i = 0; i < meshCount; i++)
	{
		BDAEint nameOffset;
		int nameLength;

#ifdef BETA_GAME_VERSION
		memcpy(&nameOffset, DataBuffer + meshInfoOffset + i * 16 + 4, sizeof(BDAEint));
		memcpy(&meshMetadataOffset[i], DataBuffer + meshInfoOffset + 12 + i * 16, sizeof(int));
		memcpy(&meshVertexCount[i], DataBuffer + meshMetadataOffset[i] + 4, sizeof(int));
		memcpy(&submeshCount[i], DataBuffer + meshMetadataOffset[i] + 12, sizeof(int));
		memcpy(&submeshMetadataOffset[i], DataBuffer + meshMetadataOffset[i] + 16, sizeof(int));
		memcpy(&bytesPerVertex[i], DataBuffer + meshMetadataOffset[i] + 44, sizeof(int));
		memcpy(&meshVertexDataOffset[i], DataBuffer + meshMetadataOffset[i] + 80, sizeof(int));
#else
		memcpy(&nameOffset, DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + i * 24, sizeof(BDAEint));
		memcpy(&meshMetadataOffset[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24, sizeof(int));
		memcpy(&meshVertexCount[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 4, sizeof(int));
		memcpy(&submeshCount[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 12, sizeof(int));
		memcpy(&submeshMetadataOffset[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 16, sizeof(int));
		memcpy(&bytesPerVertex[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 48, sizeof(int));
		memcpy(&meshVertexDataOffset[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 88, sizeof(int));
#endif
		memcpy(&nameLength, DataBuffer + nameOffset - 4, sizeof(int));
		meshName[i] = std::string(DataBuffer + nameOffset, nameLength);

		LOG("[", i + 1, "] ", meshName[i], " ", meshVertexCount[i], " vertices, ", submeshCount[i], " submeshes");

		for (int k = 0; k < submeshCount[i]; k++)
		{
			int val1, val2, submeshMaterialNameOffset, textureIndex = -1;

#ifdef BETA_GAME_VERSION
			memcpy(&submeshMaterialNameOffset, DataBuffer + submeshMetadataOffset[i] + 4 + k * 56, sizeof(int));
			memcpy(&val1, DataBuffer + submeshMetadataOffset[i] + 40 + k * 56, sizeof(int));
			memcpy(&val2, DataBuffer + submeshMetadataOffset[i] + 44 + k * 56, sizeof(int));
#else
			memcpy(&submeshMaterialNameOffset, ptr + 24 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 16 + submeshMetadataOffset[i] + k * 80 + 8, sizeof(int));
			memcpy(&val1, ptr + 24 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 16 + submeshMetadataOffset[i] + k * 80 + 48, sizeof(int));
			memcpy(&val2, ptr + 24 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 16 + submeshMetadataOffset[i] + k * 80 + 56, sizeof(int));
#endif
			submeshTriangleCount[i].push_back(val1 / 3);
			submeshIndexDataOffset[i].push_back(val2);

			if (textureCount > 0)
			{
				for (int l = 0; l < materialCount; l++)
				{
					if (submeshMaterialNameOffset == materialNameOffset[l])
					{
						textureIndex = materialTextureIndex[l];
						break;
					}
				}
			}

			submeshTextureIndex.push_back(textureIndex);
			submeshToMesh.push_back(i);
		}

		totalSubmeshCount += submeshCount[i];
	}

	// 3.3 NODES (parse node hierarchy with proper root node and parent-child relationships)
	// ____________________

	parseNodeHierarchy();

	// 3.4 MAP NODES TO MESHES (apply node transforms to meshes for correct positioning)
	// ____________________

	LOG("\n\033[37m[Init] Mapping nodes to meshes.\033[0m");

	int node2meshCount = 0;

	for (size_t i = 0; i < nodes.size(); i++)
	{
		const Node &node = nodes[i];

		// Try to find matching mesh by converting node name to mesh name
		std::string nodeMeshName = node.id;
		int pos = (int)nodeMeshName.find("-node");
		if (pos != -1)
			nodeMeshName.replace(pos, 5, "-mesh");

		// Find mesh with matching name
		auto it = std::find(std::begin(meshName), std::end(meshName), nodeMeshName);

		if (it != meshName.end())
		{
			LOG("[", i + 1, "] node --> [", std::distance(meshName.begin(), it) + 1, "] mesh");

			int meshIdx = std::distance(meshName.begin(), it);

			// Get PIVOT offset (child PIVOT transforms only, without node's own transform)
			meshTransform[meshIdx] = glm::mat4(1.0f);
			getPivotOffset(node.dataOffset, meshIdx, true);	   // skipFirst=true to skip node's own transform
			meshPivotOffset[meshIdx] = meshTransform[meshIdx]; // Store pivot offset for animation updates

			// Compute full mesh transform: node world transform * PIVOT offset
			// node.worldTransform already contains: parent world * node local
			// pivotOffset contains: PIVOT child transforms only
			// Result: parent world * node local * PIVOT children
			meshTransform[meshIdx] = node.worldTransform * meshPivotOffset[meshIdx];

			// Store mapping from node to mesh for rendering
			nodeToMeshIndex[i] = meshIdx;

			node2meshCount++;

			// stop early if every mesh already has exactly one node assigned ([TODO] questionable approach)
			if (nodeCollectionCount == 1 && node2meshCount == meshCount)
				break;
		}
		else
			LOG("[Warning] Model::init: mesh name ", nodeMeshName, " for node [", i + 1, "] not found!");
	}

	// Debug: print mesh transforms
	LOG("\n\033[37m[Debug] Mesh transforms:\033[0m");
	for (int i = 0; i < meshCount; i++)
	{
		glm::vec3 pos = glm::vec3(meshTransform[i][3]);
		LOG("[", i + 1, "] ", meshName[i], " pos=(", pos.x, ", ", pos.y, ", ", pos.z, ")");
	}

	// 4. parse model data section: build vertex and index data for each mesh; all vertex data is stored in a single flat vector, while index data is stored in separate vectors for each submesh
	LOG("\n\033[37m[Init] Parsing model data section. Building vertex and index data.\033[0m");

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

	LOG("\033[1m\033[38;2;200;200;200m[Init] Finishing Model::init..\033[0m\n");

	delete header;
	return 0;
}

//! Get only the PIVOT child transforms (without the node's own local transform).
void Model::getPivotOffset(int nodeInfoOffset, int nodeMeshIdx, bool skipFirst)
{
	int childrenOffset = 0;
	float transX, transY, transZ;
	float rotX, rotY, rotZ, rotW;
	float scaleX, scaleY, scaleZ;

#ifdef BETA_GAME_VERSION
	memcpy(&transX, DataBuffer + nodeInfoOffset + 12, sizeof(float));
	memcpy(&transY, DataBuffer + nodeInfoOffset + 16, sizeof(float));
	memcpy(&transZ, DataBuffer + nodeInfoOffset + 20, sizeof(float));
	memcpy(&rotX, DataBuffer + nodeInfoOffset + 24, sizeof(float));
	memcpy(&rotY, DataBuffer + nodeInfoOffset + 28, sizeof(float));
	memcpy(&rotZ, DataBuffer + nodeInfoOffset + 32, sizeof(float));
	memcpy(&rotW, DataBuffer + nodeInfoOffset + 36, sizeof(float));
	memcpy(&scaleX, DataBuffer + nodeInfoOffset + 40, sizeof(float));
	memcpy(&scaleY, DataBuffer + nodeInfoOffset + 44, sizeof(float));
	memcpy(&scaleZ, DataBuffer + nodeInfoOffset + 48, sizeof(float));
	memcpy(&childrenOffset, DataBuffer + nodeInfoOffset + 60, sizeof(int));
#else
	memcpy(&transX, DataBuffer + nodeInfoOffset + 24, sizeof(float));
	memcpy(&transY, DataBuffer + nodeInfoOffset + 28, sizeof(float));
	memcpy(&transZ, DataBuffer + nodeInfoOffset + 32, sizeof(float));
	memcpy(&rotX, DataBuffer + nodeInfoOffset + 36, sizeof(float));
	memcpy(&rotY, DataBuffer + nodeInfoOffset + 40, sizeof(float));
	memcpy(&rotZ, DataBuffer + nodeInfoOffset + 44, sizeof(float));
	memcpy(&rotW, DataBuffer + nodeInfoOffset + 48, sizeof(float));
	memcpy(&scaleX, DataBuffer + nodeInfoOffset + 52, sizeof(float));
	memcpy(&scaleY, DataBuffer + nodeInfoOffset + 56, sizeof(float));
	memcpy(&scaleZ, DataBuffer + nodeInfoOffset + 60, sizeof(float));
	memcpy(&childrenOffset, DataBuffer + nodeInfoOffset + 72, sizeof(int));

	if (childrenOffset != 0)
		childrenOffset += nodeInfoOffset + 72;

	// [TODO] yeah you see it
	if (fileName == "pvp_flag03.bdae" || fileName == "pvp_flag04.bdae")
		transY = 3.0f;
#endif

	// Only apply transform if not skipping first (i.e., for PIVOT children only)
	if (!skipFirst)
	{
		// translate -> rotate -> scale
		glm::mat4 nodeLocalTransform(1.0f);
		nodeLocalTransform = glm::translate(glm::mat4(1.0f), glm::vec3(transX, transY, transZ));
		nodeLocalTransform *= glm::mat4_cast(glm::quat(-rotW, rotX, rotY, rotZ));
		nodeLocalTransform *= glm::scale(glm::mat4(1.0f), glm::vec3(scaleX, scaleY, scaleZ));

		// combine with parent node transformation matrix
		meshTransform[nodeMeshIdx] *= nodeLocalTransform;
	}

	// if has child node, recursive call (don't skip children)
	if (childrenOffset != 0)
		getPivotOffset(childrenOffset, nodeMeshIdx, false);
}

//! Recursively parses child nodes to accumulate local transformation matrix for a single mesh (called in init).
void Model::getNodeTransformation(int nodeInfoOffset, int nodeMeshIdx)
{
	int childrenOffset = 0;
	float transX, transY, transZ;
	float rotX, rotY, rotZ, rotW;
	float scaleX, scaleY, scaleZ;

#ifdef BETA_GAME_VERSION
	memcpy(&transX, DataBuffer + nodeInfoOffset + 12, sizeof(float));
	memcpy(&transY, DataBuffer + nodeInfoOffset + 16, sizeof(float));
	memcpy(&transZ, DataBuffer + nodeInfoOffset + 20, sizeof(float));
	memcpy(&rotX, DataBuffer + nodeInfoOffset + 24, sizeof(float));
	memcpy(&rotY, DataBuffer + nodeInfoOffset + 28, sizeof(float));
	memcpy(&rotZ, DataBuffer + nodeInfoOffset + 32, sizeof(float));
	memcpy(&rotW, DataBuffer + nodeInfoOffset + 36, sizeof(float));
	memcpy(&scaleX, DataBuffer + nodeInfoOffset + 40, sizeof(float));
	memcpy(&scaleY, DataBuffer + nodeInfoOffset + 44, sizeof(float));
	memcpy(&scaleZ, DataBuffer + nodeInfoOffset + 48, sizeof(float));
	memcpy(&childrenOffset, DataBuffer + nodeInfoOffset + 60, sizeof(int));
#else
	memcpy(&transX, DataBuffer + nodeInfoOffset + 24, sizeof(float));
	memcpy(&transY, DataBuffer + nodeInfoOffset + 28, sizeof(float));
	memcpy(&transZ, DataBuffer + nodeInfoOffset + 32, sizeof(float));
	memcpy(&rotX, DataBuffer + nodeInfoOffset + 36, sizeof(float));
	memcpy(&rotY, DataBuffer + nodeInfoOffset + 40, sizeof(float));
	memcpy(&rotZ, DataBuffer + nodeInfoOffset + 44, sizeof(float));
	memcpy(&rotW, DataBuffer + nodeInfoOffset + 48, sizeof(float));
	memcpy(&scaleX, DataBuffer + nodeInfoOffset + 52, sizeof(float));
	memcpy(&scaleY, DataBuffer + nodeInfoOffset + 56, sizeof(float));
	memcpy(&scaleZ, DataBuffer + nodeInfoOffset + 60, sizeof(float));
	memcpy(&childrenOffset, DataBuffer + nodeInfoOffset + 72, sizeof(int));

	if (childrenOffset != 0)
		childrenOffset += nodeInfoOffset + 72;

	// [TODO] yeah you see it
	if (fileName == "pvp_flag03.bdae" || fileName == "pvp_flag04.bdae")
		transY = 3.0f;
#endif

	// translate -> rotate -> scale
	glm::mat4 nodeLocalTransform(1.0f);
	nodeLocalTransform = glm::translate(glm::mat4(1.0f), glm::vec3(transX, transY, transZ));
	nodeLocalTransform *= glm::mat4_cast(glm::quat(-rotW, rotX, rotY, rotZ));
	nodeLocalTransform *= glm::scale(glm::mat4(1.0f), glm::vec3(scaleX, scaleY, scaleZ));

	// combine with parent node transformation matrix
	meshTransform[nodeMeshIdx] *= nodeLocalTransform;

	// if has child node, recursive call
	if (childrenOffset != 0)
		getNodeTransformation(childrenOffset, nodeMeshIdx);
}

//! Parse node hierarchy with proper root node and parent-child relationships.
void Model::parseNodeHierarchy()
{
	LOG("\n\033[37m[Init] Parsing node hierarchy.\033[0m");

	// Find node collection info
	BDAEFileHeader *header = (BDAEFileHeader *)DataBuffer;
	char *ptr = DataBuffer + header->offsetData + 96;

	int nodeCollectionInfoOffset;
	memcpy(&nodeCollectionCount, ptr + 72, sizeof(int));
	memcpy(&nodeCollectionInfoOffset, ptr + 76, sizeof(int));

	if (nodeCollectionCount == 0 || nodeCollectionInfoOffset == 0)
	{
		LOG("[Warning] No node collections found.");
		return;
	}

	LOG("Node collections: ", nodeCollectionCount);

	// Parse each node collection
	for (int i = 0; i < nodeCollectionCount; i++)
	{
		int nodeCount, nodeMetadataOffset;

#ifdef BETA_GAME_VERSION
		memcpy(&nodeCount, DataBuffer + nodeCollectionInfoOffset + 8 + i * 16, sizeof(int));
		memcpy(&nodeMetadataOffset, DataBuffer + nodeCollectionInfoOffset + 12 + i * 16, sizeof(int));
#else
		memcpy(&nodeCount, DataBuffer + header->offsetData + 168 + 4 + nodeCollectionInfoOffset + 16 + i * 24, sizeof(int));
		memcpy(&nodeMetadataOffset, DataBuffer + header->offsetData + 168 + 4 + nodeCollectionInfoOffset + 20 + i * 24, sizeof(int));

		if (nodeMetadataOffset != 0)
			nodeMetadataOffset += header->offsetData + 168 + 4 + nodeCollectionInfoOffset + 20 + i * 24;
#endif

		LOG("Collection [", i, "]: ", nodeCount, " nodes");

		if (nodeMetadataOffset == 0)
			continue;

		// Parse each root node in this collection
		int nodeSize = 80;
#ifndef BETA_GAME_VERSION
		nodeSize = 96;
#endif

		for (int k = 0; k < nodeCount; k++)
		{
			int nodeOffset = nodeMetadataOffset + k * nodeSize;
			parseNodeRecursive(nodeOffset, -1); // -1 = root node (no parent)
		}
	}

	LOG("Total nodes parsed: ", nodes.size());

	// Update world transforms
	updateNodeTransforms();

	// Save original transforms for animation reset
	for (Node &node : nodes)
	{
		node.originalPosition = node.localPosition;
		node.originalRotation = node.localRotation;
		node.originalScale = node.localScale;
	}
}

//! Recursively parse a node and its children.
void Model::parseNodeRecursive(int nodeOffset, int parentIndex)
{
	// Read node name
	BDAEint nameOffset;
	int nameLength = 0;

#ifdef BETA_GAME_VERSION
	memcpy(&nameOffset, DataBuffer + nodeOffset + 4, sizeof(BDAEint));
#else
	memcpy(&nameOffset, DataBuffer + nodeOffset, sizeof(BDAEint));
#endif

	if (nameOffset <= 4 || nameOffset >= fileSize)
		return;

	memcpy(&nameLength, DataBuffer + nameOffset - 4, sizeof(int));
	if (nameLength <= 0 || nameLength > 200)
		return;

	std::string nodeId(DataBuffer + nameOffset, nameLength);
	std::string nodeName = nodeId;

	// Remove "-node" suffix if present
	size_t pos = nodeName.find("-node");
	if (pos != std::string::npos)
		nodeName = nodeName.substr(0, pos);

	// Read transform data
	float transX, transY, transZ;
	float rotX, rotY, rotZ, rotW;
	float scaleX, scaleY, scaleZ;
	int childrenCount, childrenOffset;

#ifdef BETA_GAME_VERSION
	memcpy(&transX, DataBuffer + nodeOffset + 12, sizeof(float));
	memcpy(&transY, DataBuffer + nodeOffset + 16, sizeof(float));
	memcpy(&transZ, DataBuffer + nodeOffset + 20, sizeof(float));
	memcpy(&rotX, DataBuffer + nodeOffset + 24, sizeof(float));
	memcpy(&rotY, DataBuffer + nodeOffset + 28, sizeof(float));
	memcpy(&rotZ, DataBuffer + nodeOffset + 32, sizeof(float));
	memcpy(&rotW, DataBuffer + nodeOffset + 36, sizeof(float));
	memcpy(&scaleX, DataBuffer + nodeOffset + 40, sizeof(float));
	memcpy(&scaleY, DataBuffer + nodeOffset + 44, sizeof(float));
	memcpy(&scaleZ, DataBuffer + nodeOffset + 48, sizeof(float));
	memcpy(&childrenCount, DataBuffer + nodeOffset + 56, sizeof(int));
	memcpy(&childrenOffset, DataBuffer + nodeOffset + 60, sizeof(int));

	if (childrenOffset != 0)
		childrenOffset += nodeOffset + 60;
#else
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

	if (childrenOffset != 0)
		childrenOffset += nodeOffset + 72;
#endif

	// Create node
	Node node;
	node.id = nodeId;
	node.name = nodeName;
	node.parentIndex = parentIndex;
	node.dataOffset = nodeOffset;
	node.localPosition = glm::vec3(transX, transY, transZ);
	node.localRotation = glm::quat(-rotW, rotX, rotY, rotZ);
	node.localScale = glm::vec3(scaleX, scaleY, scaleZ);

	// Add to list
	int nodeIndex = (int)nodes.size();
	nodes.push_back(node);
	nodeNameToIndex[nodeId] = nodeIndex;
	nodeNameToIndex[nodeName] = nodeIndex;

	// Update parent's children list
	if (parentIndex >= 0 && parentIndex < (int)nodes.size())
		nodes[parentIndex].childIndices.push_back(nodeIndex);

	// Parse children recursively
	if (childrenCount > 0 && childrenOffset > 0)
	{
		int nodeSize = 80;
#ifndef BETA_GAME_VERSION
		nodeSize = 96;
#endif

		for (int i = 0; i < childrenCount; i++)
		{
			int childOffset = childrenOffset + i * nodeSize;
			parseNodeRecursive(childOffset, nodeIndex);
		}
	}
}

//! Update world transforms for all nodes (traverse from root).
void Model::updateNodeTransforms()
{
	// Find root nodes and update their transforms recursively
	for (size_t i = 0; i < nodes.size(); i++)
	{
		if (nodes[i].parentIndex == -1)
		{
			// Root node: world transform = local transform
			glm::mat4 localTransform(1.0f);
			localTransform = glm::translate(glm::mat4(1.0f), nodes[i].localPosition);
			localTransform *= glm::mat4_cast(nodes[i].localRotation);
			localTransform *= glm::scale(glm::mat4(1.0f), nodes[i].localScale);
			nodes[i].worldTransform = localTransform;

			// Update children recursively
			updateNodeTransformsRecursive(i);
		}
	}
}

//! Helper function to recursively update child node transforms.
void Model::updateNodeTransformsRecursive(int nodeIndex)
{
	const Node &node = nodes[nodeIndex];

	for (int childIdx : node.childIndices)
	{
		// Calculate local transform
		glm::mat4 localTransform(1.0f);
		localTransform = glm::translate(glm::mat4(1.0f), nodes[childIdx].localPosition);
		localTransform *= glm::mat4_cast(nodes[childIdx].localRotation);
		localTransform *= glm::scale(glm::mat4(1.0f), nodes[childIdx].localScale);

		// World transform = parent's world transform * local transform
		nodes[childIdx].worldTransform = node.worldTransform * localTransform;

		// Recursively update this child's children
		updateNodeTransformsRecursive(childIdx);
	}
}

//! Update mesh transforms from animated node transforms.
void Model::updateMeshTransformsFromNodes()
{
	// For each node that has a mesh, update the mesh transform
	for (const auto &pair : nodeToMeshIndex)
	{
		int nodeIdx = pair.first;
		int meshIdx = pair.second;

		if (nodeIdx >= 0 && nodeIdx < (int)nodes.size() &&
			meshIdx >= 0 && meshIdx < (int)meshTransform.size())
		{
			// Update mesh transform: animated node world transform * pivot offset
			meshTransform[meshIdx] = nodes[nodeIdx].worldTransform * meshPivotOffset[meshIdx];
		}
	}
}

//! Print node hierarchy for debugging.
void Model::printNodeHierarchy(int nodeIndex, int depth)
{
	if (nodeIndex < 0 || nodeIndex >= (int)nodes.size())
		return;

	const Node &node = nodes[nodeIndex];

	// Print indentation
	for (int i = 0; i < depth; i++)
	{
		if (i == depth - 1)
			std::cout << "├─ ";
		else
			std::cout << "│  ";
	}

	// Print node info
	glm::vec3 pos = node.localPosition;
	std::cout << node.name << " [" << nodeIndex << "] pos=("
			  << pos.x << ", " << pos.y << ", " << pos.z << ") children="
			  << node.childIndices.size() << "\n";

	// Print children
	for (int childIdx : node.childIndices)
		printNodeHierarchy(childIdx, depth + 1);
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

		// Get node transform (mesh transform if available, otherwise world transform)
		glm::mat4 nodeTransform = node.worldTransform;
		auto meshIt = nodeToMeshIndex.find(i);
		if (meshIt != nodeToMeshIndex.end())
			nodeTransform = meshTransform[meshIt->second];

		// Apply baseModel transformation first, then extract position
		// This matches how meshes are rendered: model * meshTransform
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

	// Automatically try to load animation file
	// Pattern: if model is "dragon_model.bdae", try "dragon_animation_idle.bdae"
	if (!isTerrainViewer)
	{
		std::string animationPath = std::string(fpath);

		// Check if filename ends with "_model.bdae"
		size_t modelPos = animationPath.rfind(".bdae");
		if (modelPos != std::string::npos)
		{
			// Replace "_model.bdae" with "_animation_idle.bdae"
			animationPath.replace(modelPos, 11, "_anim.bdae");

			// Check if animation file exists
			if (std::filesystem::exists(animationPath))
			{
				LOG("\n\033[37m[Load] Found animation file: ", animationPath, "\033[0m");
				loadAnimations(animationPath.c_str());
			}
			else
			{
				LOG("\n\033[37m[Load] No animation file found at: ", animationPath, "\033[0m");
			}
		}
	}
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
	meshTransform.clear();
	submeshToMesh.clear();
	nodes.clear();
	nodeNameToIndex.clear();
	nodeToMeshIndex.clear();

	fileSize = vertexCount = faceCount = textureCount = alternativeTextureCount = selectedTexture = totalSubmeshCount = 0;
	nodeSphereVertexCount = nodeSphereIndexCount = 0;
}

//! Renders .bdae model.
void Model::draw(glm::mat4 model, glm::mat4 view, glm::mat4 projection, glm::vec3 cameraPos, bool lighting, bool simple)
{
	if (!modelLoaded || EBOs.empty())
		return;

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

	// render model
	glBindVertexArray(VAO);

	if (!simple)
	{
		shader.setInt("renderMode", 1);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		for (int i = 0; i < totalSubmeshCount; i++)
		{
			int meshIdx = submeshToMesh[i];

			if (meshIdx < 0 || meshIdx >= (int)meshTransform.size())
			{
				std::cout << "[Warning] Model::draw: skipping submesh [" << i << "] --> invalid mesh index [" << meshIdx << "]" << std::endl;
				continue;
			}

			shader.setMat4("model", model * meshTransform[meshIdx]);
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
			int meshIdx = submeshToMesh[i];
			shader.setMat4("model", model * meshTransform[meshIdx]);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
		}

		// second pass: render mesh faces
		shader.setInt("renderMode", 3);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		for (int i = 0; i < totalSubmeshCount; i++)
		{
			int meshIdx = submeshToMesh[i];
			shader.setMat4("model", model * meshTransform[meshIdx]);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
		}

		// render node visualization (bones/skeleton)
		drawNodes(glm::mat4(1.0f), view, projection);
	}

	glBindVertexArray(0);
}
// Animation functions to be added to parserBDAE.cpp

//! Load animations from a separate .bdae animation file.
void Model::loadAnimations(const char *animationFilePath)
{
	LOG("\n\033[1m\033[97mLoading animations from ", animationFilePath, "\033[0m");

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

	for (uint32_t animIdx = 0; animIdx < animCount; animIdx++)
	{
		BDAEint animPos = animArrayOffset + animIdx * ANIM_STRUCT_SIZE;

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

		LOG("\n[", animIdx, "] ", animName);
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
				// Ensure keyframeSources has enough space
				while ((int)keyframeSources.size() <= inSrc)
				{
					keyframeSources.push_back(KeyframeSource());
				}

				// Parse if not already parsed
				if (keyframeSources[inSrc].data.empty())
				{
					keyframeSources[inSrc].dataType = (DataType)inType;
					keyframeSources[inSrc].componentCount = inComp;
					keyframeSources[inSrc].data = parseSourceData(sourceDescs[inSrc].dataOffset, (DataType)inType, sourceDescs[inSrc].count);

					// Convert time values from frames to seconds (BDAE uses 30 fps)
					// Time values in BDAE are stored as frame numbers, need to divide by 30
					const float BDAE_FPS = 30.0f;
					for (float &timeValue : keyframeSources[inSrc].data)
					{
						timeValue /= BDAE_FPS;
					}

					// Update animation duration from time values
					if (!keyframeSources[inSrc].data.empty())
					{
						float maxTime = *std::max_element(keyframeSources[inSrc].data.begin(), keyframeSources[inSrc].data.end());
						if (maxTime > anim.duration)
							anim.duration = maxTime;
					}
				}
			}

			if (outSrc >= 0 && outSrc < (int)sourcesCount)
			{
				while ((int)keyframeSources.size() <= outSrc)
				{
					keyframeSources.push_back(KeyframeSource());
				}

				if (keyframeSources[outSrc].data.empty())
				{
					keyframeSources[outSrc].dataType = (DataType)outType;
					keyframeSources[outSrc].componentCount = outComp;
					keyframeSources[outSrc].data = parseSourceData(sourceDescs[outSrc].dataOffset, (DataType)outType, sourceDescs[outSrc].count);
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

		animations.push_back(anim);
		LOG("  Duration: ", anim.duration, " seconds");
	}

	free(animBuffer);
	delete animFile;
	delete animArchive;

	animationsLoaded = true;
	LOG("\n\033[1m\033[38;2;200;200;200m[Load] Loaded ", animations.size(), " animations.\033[0m\n");
}

//! Update animations based on elapsed time.
void Model::updateAnimations(float deltaTime)
{
	if (!animationPlaying || animations.empty())
		return;

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

	// Update time based on direction (forward or reverse for ping-pong)
	float timeStep = deltaTime * animationSpeed;
	if (animationReversed)
		timeStep = -timeStep;

	currentAnimationTime += timeStep;

	// Handle looping based on mode
	if (loopMode == AnimationLoopMode::PINGPONG)
	{
		// Ping-pong: reverse direction at boundaries
		if (currentAnimationTime >= maxDuration)
		{
			currentAnimationTime = maxDuration;
			animationReversed = true; // Start playing in reverse
		}
		else if (currentAnimationTime <= 0.0f)
		{
			currentAnimationTime = 0.0f;
			animationReversed = false; // Start playing forward
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

	// Recalculate world transforms after animation
	updateNodeTransforms();

	// Update mesh transforms from animated nodes
	updateMeshTransformsFromNodes();
}

//! Apply an animation at a specific time.
void Model::applyAnimation(const Animation &anim, float time)
{
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

		if (keyframeCount < 2)
		{
			// Not enough keyframes to interpolate
			continue;
		}

		int keyframe0 = 0, keyframe1 = 0;
		float t = 0.0f;

		// Find keyframes
		for (int i = 0; i < keyframeCount - 1; i++)
		{
			if (time >= timeSource.data[i] && time <= timeSource.data[i + 1])
			{
				keyframe0 = i;
				keyframe1 = i + 1;
				float t0 = timeSource.data[i];
				float t1 = timeSource.data[i + 1];
				t = (time - t0) / (t1 - t0);
				break;
			}
		}

		// If time is past the last keyframe, use the last keyframe
		if (time >= timeSource.data[keyframeCount - 1])
		{
			keyframe0 = keyframe1 = keyframeCount - 1;
			t = 0.0f;
		}

		// Find target node
		auto nodeIt = nodeNameToIndex.find(channel.targetNodeName);
		if (nodeIt == nodeNameToIndex.end())
			continue;

		int nodeIndex = nodeIt->second;
		if (nodeIndex < 0 || nodeIndex >= (int)nodes.size())
			continue;

		Node &node = nodes[nodeIndex];

		// Apply animation based on channel type
		switch (channel.channelType)
		{
		case ChannelType::Translation:
		{
			int comp = sampler.outputComponentCount;
			if (comp == 3)
			{
				// Bounds check
				int maxIndex = keyframe1 * 3 + 2;
				if (maxIndex >= (int)keyframeSource.data.size())
				{
					LOG("[Warning] Translation keyframe index out of bounds: ", maxIndex, " >= ", keyframeSource.data.size());
					break;
				}

				glm::vec3 v0(keyframeSource.data[keyframe0 * 3 + 0],
							 keyframeSource.data[keyframe0 * 3 + 1],
							 keyframeSource.data[keyframe0 * 3 + 2]);
				glm::vec3 v1(keyframeSource.data[keyframe1 * 3 + 0],
							 keyframeSource.data[keyframe1 * 3 + 1],
							 keyframeSource.data[keyframe1 * 3 + 2]);
				node.localPosition = interpolateVec3(v0, v1, t, sampler.interpolation);
			}
			break;
		}
		case ChannelType::Rotation:
		{
			int comp = sampler.outputComponentCount;
			if (comp == 4)
			{
				// Bounds check
				int maxIndex = keyframe1 * 4 + 3;
				if (maxIndex >= (int)keyframeSource.data.size())
				{
					LOG("[Warning] Rotation keyframe index out of bounds: ", maxIndex, " >= ", keyframeSource.data.size());
					break;
				}

				glm::quat q0(keyframeSource.data[keyframe0 * 4 + 3],  // w
							 keyframeSource.data[keyframe0 * 4 + 0],  // x
							 keyframeSource.data[keyframe0 * 4 + 1],  // y
							 keyframeSource.data[keyframe0 * 4 + 2]); // z
				glm::quat q1(keyframeSource.data[keyframe1 * 4 + 3],
							 keyframeSource.data[keyframe1 * 4 + 0],
							 keyframeSource.data[keyframe1 * 4 + 1],
							 keyframeSource.data[keyframe1 * 4 + 2]);

				// Apply negation to w component (same as in node parsing)
				q0.w = -q0.w;
				q1.w = -q1.w;

				node.localRotation = interpolateQuat(q0, q1, t, sampler.interpolation);
			}
			break;
		}
		case ChannelType::Scale:
		{
			int comp = sampler.outputComponentCount;
			if (comp == 3)
			{
				// Bounds check
				int maxIndex = keyframe1 * 3 + 2;
				if (maxIndex >= (int)keyframeSource.data.size())
				{
					LOG("[Warning] Scale keyframe index out of bounds: ", maxIndex, " >= ", keyframeSource.data.size());
					break;
				}

				glm::vec3 v0(keyframeSource.data[keyframe0 * 3 + 0],
							 keyframeSource.data[keyframe0 * 3 + 1],
							 keyframeSource.data[keyframe0 * 3 + 2]);
				glm::vec3 v1(keyframeSource.data[keyframe1 * 3 + 0],
							 keyframeSource.data[keyframe1 * 3 + 1],
							 keyframeSource.data[keyframe1 * 3 + 2]);
				node.localScale = interpolateVec3(v0, v1, t, sampler.interpolation);
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
		node.localPosition = node.originalPosition;
		node.localRotation = node.originalRotation;
		node.localScale = node.originalScale;
	}

	// Recalculate world transforms
	updateNodeTransforms();
}
