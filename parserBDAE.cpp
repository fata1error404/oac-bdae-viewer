#include "parserBDAE.h"
#include <filesystem>
#include "libs/glad/glad.h"
#include "Logger.h"
#include "PackPatchReader.h"

//! Parses .bdae file and sets up model mesh data.
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

	// 2. allocate memory
	unsigned int sizeOffsetTable;
	unsigned int sizeStringTable;
	unsigned int sizeUnRemovable;

	sizeOffsetTable = header->numOffsets * sizeof(BDAEint);
	sizeStringTable = header->offsetData - header->offsetStringTable;
	sizeUnRemovable = fileSize - sizeOffsetTable - sizeStringTable - header->sizeOfDynamic;

	char *stringBuffer = new char[sizeStringTable]; // temp buffer for string table
	DataBuffer = (char *)malloc(sizeUnRemovable);	// main buffer

	// 3. write header, data, removable sections to buffer storage as a raw binary data
	memcpy(DataBuffer, header, headerSize); // copy header

	file->seek(header->offsetStringTable);

	if (sizeStringTable > 0)
	{
		LOG("\n\033[37m[Init] At position ", file->getPos(), ", reading string table section..\033[0m");
		file->read(stringBuffer, sizeStringTable);
	}

	LOG("\n\033[37m[Init] At position ", file->getPos(), ", reading data and removable sections..\033[0m");
	file->read(DataBuffer + headerSize, sizeUnRemovable - headerSize); // insert after header

	// 4. parse string data section: retrieve strings
	if (stringBuffer && sizeStringTable > 0)
	{
		unsigned int pos = 0;

		while (pos < sizeStringTable)
		{
			// skip null bytes ([TODO] this is not perfect compared to original code)
			if (stringBuffer[pos] == '\0')
			{
				++pos;
				continue;
			}

			unsigned int stringStart = pos;

			// find end of string
			while (pos < sizeStringTable && stringBuffer[pos] != '\0')
				++pos;

			unsigned int stringLength = pos - stringStart;

			if (stringLength > 0)
				StringStorage.push_back(std::string(stringBuffer + stringStart, stringLength)); // construct string from buffer
		}
	}

	delete header;
	delete[] stringBuffer;

	LOG("_____________________");
	LOG("\nExtracted String Data\n");

	for (int i = 0, n = (int)StringStorage.size(); i < n; i++)
		LOG("[", std::setw(2), i + 1, "] \"", StringStorage[i], "\"");

	LOG("_____________________\n");

	// 5. parse data section: retrieve mesh info
	int meshCount, meshInfoOffset;

#ifdef BETA_GAME_VERSION
	char *ptr = DataBuffer + sizeof(BDAEFileHeader) + 100; // points to mesh info in the Data section
#else
	char *ptr = DataBuffer + sizeof(BDAEFileHeader) + 120;
#endif

	memcpy(&meshCount, ptr, sizeof(int));
	memcpy(&meshInfoOffset, ptr + 4, sizeof(int));

	LOG("\nMESHES: ", meshCount);

	int meshVertexCount[meshCount];
	int meshMetadataOffset[meshCount];
	int meshVertexDataOffset[meshCount];
	int bytesPerVertex[meshCount];
	int submeshCount[meshCount];
	int submeshMetadataOffset[meshCount];
	std::vector<int> submeshTriangleCount[meshCount];
	std::vector<int> submeshIndexDataOffset[meshCount];

	for (int i = 0; i < meshCount; i++)
	{
#ifdef BETA_GAME_VERSION
		memcpy(&meshMetadataOffset[i], DataBuffer + (meshInfoOffset - sizeOffsetTable - sizeStringTable + 12) + i * 16, sizeof(int));
		memcpy(&meshVertexCount[i], DataBuffer + (meshMetadataOffset[i] - sizeOffsetTable - sizeStringTable + 4), sizeof(int));
		memcpy(&submeshCount[i], DataBuffer + (meshMetadataOffset[i] - sizeOffsetTable - sizeStringTable + 12), sizeof(int));
		memcpy(&submeshMetadataOffset[i], DataBuffer + (meshMetadataOffset[i] - sizeOffsetTable - sizeStringTable + 16), sizeof(int));
		memcpy(&bytesPerVertex[i], DataBuffer + (meshMetadataOffset[i] - sizeOffsetTable - sizeStringTable + 44), sizeof(int));
		memcpy(&meshVertexDataOffset[i], DataBuffer + (meshMetadataOffset[i] - sizeOffsetTable - sizeStringTable + 80), sizeof(int));
#else
		memcpy(&meshMetadataOffset[i], ptr + 4 + meshInfoOffset + 20 + i * 24, sizeof(int));
		memcpy(&meshVertexCount[i], ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 4, sizeof(int));
		memcpy(&submeshCount[i], ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 12, sizeof(int));
		memcpy(&submeshMetadataOffset[i], ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 16, sizeof(int));
		memcpy(&bytesPerVertex[i], ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 48, sizeof(int));
		memcpy(&meshVertexDataOffset[i], ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 88, sizeof(int));
#endif

		LOG("[", i + 1, "]  ", meshVertexCount[i], " vertices, ", submeshCount[i], " submeshes");

		for (int k = 0; k < submeshCount[i]; k++)
		{
			int val1, val2;

#ifdef BETA_GAME_VERSION
			memcpy(&val1, DataBuffer + (submeshMetadataOffset[i] - sizeOffsetTable - sizeStringTable + 40) + k * 56, sizeof(int));
			memcpy(&val2, DataBuffer + (submeshMetadataOffset[i] - sizeOffsetTable - sizeStringTable + 44) + k * 56, sizeof(int));
#else
			memcpy(&val1, ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 16 + submeshMetadataOffset[i] + k * 80 + 48, sizeof(int));
			memcpy(&val2, ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 16 + submeshMetadataOffset[i] + k * 80 + 56, sizeof(int));
#endif

			submeshTriangleCount[i].push_back(val1 / 3);
			submeshIndexDataOffset[i].push_back(val2);
		}

		totalSubmeshCount += submeshCount[i];
	}

	indices.resize(totalSubmeshCount);
	int currentSubmeshIndex = 0;

	// 6. parse removable section: build vertex and index data for each mesh; all vertex data is stored in a single flat vector, while index data is stored in separate vectors for each submesh
	for (int i = 0; i < meshCount; i++)
	{
		int vertexBase = (int)(vertices.size() / 8); // [FIX] to convert vertex indices from local to global range

		char *meshVertexDataPtr = DataBuffer + (meshVertexDataOffset[i] - sizeOffsetTable - sizeStringTable + 4);

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
			char *submeshIndexDataPtr = DataBuffer + (submeshIndexDataOffset[i][k] - sizeOffsetTable - sizeStringTable + 4);

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

	memcpy(&textureCount, ptr - 24, sizeof(int));
	LOG("\nTEXTURES: ", ((textureCount != 0) ? std::to_string(textureCount) : "0, file name will be used as a texture name"));

	return 0;
}

//! For 3D Model Viewer. Loads .bdae file from disk, calls the parser and sets up model textures and sounds.
void Model::load(const char *fpath, Sound &sound)
{
	reset();

	std::vector<std::string> textureNames;

	// 1. load .bdae file
	CPackPatchReader *bdaeArchive = new CPackPatchReader(fpath, true, false);			// open outer .bdae archive file
	IReadResFile *bdaeFile = bdaeArchive->openFile("little_endian_not_quantized.bdae"); // open inner .bdae file

	if (bdaeFile)
	{
		// 2. run the parser
		int result = init(bdaeFile);

		if (result != 0)
			return;

		// compute the mesh's center in world space for its correct rotation (instead of always rotating around the origin (0, 0, 0))
		meshCenter = glm::vec3(0.0f);

		for (int i = 0, n = vertices.size() / 8; i < n; i++)
		{
			meshCenter.x += vertices[i * 8 + 0];
			meshCenter.y += vertices[i * 8 + 1];
			meshCenter.z += vertices[i * 8 + 2];
		}

		meshCenter /= (vertices.size() / 8);

		// 3. search for texture names in strings retrieved from .bdae
		std::string modelPath(fpath);
		std::replace(modelPath.begin(), modelPath.end(), '\\', '/'); // normalize model path for cross-platform compatibility (Windows uses '\', Linux uses '/')

		// retrieve model subpath
		const char *subpathStart = std::strstr(modelPath.c_str(), "/model/") + 7; // subpath starts after '/model/' (texture and model files have the same subpath, e.g. 'creature/pet/')
		const char *subpathEnd = std::strrchr(modelPath.c_str(), '/') + 1;		  // last '/' before the file name
		std::string textureSubpath(subpathStart, subpathEnd);

		// bool isAlphaRef = false;       // for debugging textures
		bool isUnsortedFolder = false; // for 'unsorted' folder

		if (textureSubpath.rfind("unsorted/", 0) == 0)
			isUnsortedFolder = true;

		// [TODO] implement a more robust approach
		// loop through each retrieved string and find those that are texture names
		for (int i = 0, n = StringStorage.size(); i < n; i++)
		{
			std::string s = StringStorage[i];

			// if (s == "alpharef")
			//     isAlphaRef = true;

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

			// a string is a texture file name if it ends with '.tga' and doesn't start with '_'
			if (s.length() >= 4 && s.compare(s.length() - 4, 4, ".tga") == 0 && s[0] != '_' && s.substr(0, 3) != "e:/")
			{
				// replace the ending with '.png'
				s.replace(s.length() - 4, 4, ".png");

				// build final path
				if (!isUnsortedFolder)
					s = "data/texture/" + textureSubpath + s;
				else
					s = "data/texture/unsorted/" + s;

				// ensure it is a unique texture name
				if (std::find(textureNames.begin(), textureNames.end(), s) == textureNames.end())
					textureNames.push_back(s);
			}
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

		for (int i = 0; i < (int)textureNames.size(); i++)
			LOG("[", i + 1, "]  ", textureNames[i]);

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

		free(DataBuffer);
	}

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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // for t (y) axis R

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

//! For Terrain Viewer. Loads .bdae file from disk, performs in-memory initialization and parsing, sets up model mesh data and textures (does not search for associated textures and sounds).
void Model::load(const char *fpath, glm::mat4 modelMatrix)
{
}

//! Clears GPU memory and resets viewer state.
void Model::reset()
{
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

	StringStorage.clear();
	vertices.clear();
	indices.clear();
	sounds.clear();

	fileSize = vertexCount = faceCount = textureCount = alternativeTextureCount = selectedTexture = totalSubmeshCount = 0;

	modelLoaded = false;
}

//! Renders .bdae model.
void Model::draw(glm::mat4 view, glm::mat4 projection, glm::vec3 cameraPos, bool lighting, bool simple)
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
			if (textureCount == totalSubmeshCount)
			{
				glActiveTexture(GL_TEXTURE0); // [TODO] textures are assigned to the wrong submeshes
				glBindTexture(GL_TEXTURE_2D, textures[i]);
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
}
