#include "parserBDAE.h"
#include <filesystem>
#include "libs/glad/glad.h"
#include "Logger.h"
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

	LOG("\n\033[37m[Init] At position ", file->getPos(), ", reading offset, string, data and removable sections..\033[0m");
	file->read(DataBuffer + headerSize, sizeUnRemovable - headerSize); // insert after header

	// 3. parse data section: retrieve texture, material and mesh info
	int textureInfoOffset;
	int materialCount, materialInfoOffset;
	int meshCount, meshInfoOffset;

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

			LOG("[", i + 1, "]  ", textureNames[i]);
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

	for (int i = 0; i < meshCount; i++)
	{
#ifdef BETA_GAME_VERSION
		memcpy(&meshMetadataOffset[i], DataBuffer + meshInfoOffset + 12 + i * 16, sizeof(int));
		memcpy(&meshVertexCount[i], DataBuffer + meshMetadataOffset[i] + 4, sizeof(int));
		memcpy(&submeshCount[i], DataBuffer + meshMetadataOffset[i] + 12, sizeof(int));
		memcpy(&submeshMetadataOffset[i], DataBuffer + meshMetadataOffset[i] + 16, sizeof(int));
		memcpy(&bytesPerVertex[i], DataBuffer + meshMetadataOffset[i] + 44, sizeof(int));
		memcpy(&meshVertexDataOffset[i], DataBuffer + meshMetadataOffset[i] + 80, sizeof(int));
#else
		memcpy(&meshMetadataOffset[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24, sizeof(int));
		memcpy(&meshVertexCount[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 4, sizeof(int));
		memcpy(&submeshCount[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 12, sizeof(int));
		memcpy(&submeshMetadataOffset[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 16, sizeof(int));
		memcpy(&bytesPerVertex[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 48, sizeof(int));
		memcpy(&meshVertexDataOffset[i], DataBuffer + header->offsetData + 120 + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 88, sizeof(int));
#endif

		LOG("[", i + 1, "]  ", meshVertexCount[i], " vertices, ", submeshCount[i], " submeshes");

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
		}

		totalSubmeshCount += submeshCount[i];
	}

	indices.resize(totalSubmeshCount);
	int currentSubmeshIndex = 0;

	// 4. parse removable section: build vertex and index data for each mesh; all vertex data is stored in a single flat vector, while index data is stored in separate vectors for each submesh
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

			vertices.push_back(vertex[6]); // S
			vertices.push_back(vertex[7]); // T
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

	delete header;
	return 0;
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

	// 2. run the parser
	int result = init(bdaeFile);

	if (result != 0)
	{
		delete bdaeFile;
		delete bdaeArchive;
		return;
	}

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
		std::string modelPath(fpath);
		std::replace(modelPath.begin(), modelPath.end(), '\\', '/'); // normalize model path for cross-platform compatibility (Windows uses '\', Linux uses '/')

		// retrieve model subpath
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

		// set file info to be displayed in the settings panel
		fileName = modelPath.substr(modelPath.find_last_of("/\\") + 1); // file name is after the last path separator in the full path
		vertexCount = vertices.size() / 8;

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
	EBOs.resize(totalSubmeshCount);
	glGenVertexArrays(1, &VAO);					  // generate a Vertex Attribute Object to store vertex attribute configurations
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

	// 7. load texture(s)
	textures.resize(textureNames.size());
	glGenTextures(textureNames.size(), textures.data()); // generate and store texture ID(s)

	for (int i = 0; i < (int)textureNames.size(); i++)
	{
		glBindTexture(GL_TEXTURE_2D, textures[i]); // bind the texture ID so that all upcoming texture operations affect this texture

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // for s (x) axis
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // for t (y) axis

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

	modelLoaded = true;
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
		glDeleteBuffers(totalSubmeshCount, EBOs.data());
		EBOs.clear();
	}

	if (!textures.empty())
	{
		glDeleteTextures(textureCount + alternativeTextureCount, textures.data());
		textures.clear();
	}

	textureNames.clear();
	submeshTextureIndex.clear();
	vertices.clear();
	indices.clear();
	sounds.clear();

	fileSize = vertexCount = faceCount = textureCount = alternativeTextureCount = selectedTexture = totalSubmeshCount = 0;
}

//! Renders .bdae model.
void Model::draw(glm::mat4 model, glm::mat4 view, glm::mat4 projection, glm::vec3 cameraPos, bool lighting, bool simple)
{
	if (!modelLoaded)
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
	shader.setMat4("model", model);
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
			if (submeshTextureIndex[i] != -1)
			{
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, textures[submeshTextureIndex[i]]);
			}

			if (alternativeTextureCount > 0 && textureCount == 1)
				glBindTexture(GL_TEXTURE_2D, textures[selectedTexture]);

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
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
		}

		// second pass: render mesh faces
		shader.setInt("renderMode", 3);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		for (int i = 0; i < totalSubmeshCount; i++)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
			glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
		}
	}

	glBindVertexArray(0);
}
