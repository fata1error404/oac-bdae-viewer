#ifndef PARSER_BDAE_H
#define PARSER_BDAE_H

#include <string>
#include <vector>
#include <cstdint>
#include "IReadResFile.h"
#include "libs/glm/fwd.hpp"
#include "shader.h"
#include "sound.h"
#include "light.h"

// if defined, viewer prints detailed model info in terminal
// #define CONSOLE_DEBUG_LOG

template <typename... Args>
inline void LOG(Args &&...args)
{
#ifdef CONSOLE_DEBUG_LOG
	(std::cout << ... << args) << std::endl;
#endif
}

// if defined, viewer works with .bdae version from oac 1.0.3; if undefined, with oac 4.2.5
// #define BETA_GAME_VERSION

#ifdef BETA_GAME_VERSION
typedef uint32_t BDAEint;
#else
typedef uint64_t BDAEint;
#endif

const float meshRotationSensitivity = 0.3f;

// 60 or 80 bytes (depends on .bdae version)
struct BDAEFileHeader
{
	unsigned int signature;									// 4 bytes  file signature – 'BRES' for .bdae file
	unsigned short endianCheck;								// 2 bytes  byte order mark
	unsigned short version;									// 2 bytes  version (?)
	unsigned int sizeOfHeader;								// 4 bytes  header size in bytes
	unsigned int sizeOfFile;								// 4 bytes  file size in bytes
	unsigned int numOffsets;								// 4 bytes  number of entries in the offset table
	unsigned int origin;									// 4 bytes  file origin – always '0' for standalone .bdae files (?)
	BDAEint offsetOffsetTable;								// 8 bytes  offset to Offset Data section (in bytes, from the beginning of the file)
	BDAEint offsetStringTable;								// 8 bytes  offset to String Data section
	BDAEint offsetData;										// 8 bytes  offset to Data section
	BDAEint offsetRelatedFiles;								// 8 bytes  offset to related file names (?)
	BDAEint offsetRemovable;								// 8 bytes  offset to Removable section
	unsigned int sizeOfRemovable;							// 4 bytes  size of Removable section in bytes
	unsigned int numRemovableChunks;						// 4 bytes  number of removable chunks
	unsigned int useSeparatedAllocationForRemovableBuffers; // 4 bytes  1: each removable chunk is loaded into its own separately allocated buffer, 0: all chunks in one shared buffer
	unsigned int sizeOfDynamic;								// 4 bytes  size of dynamic chunk (?)
};

// Class for loading and rendering 3D model.
// _________________________________________

class Model
{
  public:
	Shader shader;
	std::string fileName;
	std::vector<std::string> textureNames;
	std::vector<int> submeshTextureIndex;
	int fileSize, vertexCount, faceCount, totalSubmeshCount;
	int textureCount, alternativeTextureCount, selectedTexture;
	unsigned int VAO;								  // Vertex Attribute Object ID (stores vertex attribute configuration on GPU)
	unsigned int VBO;								  // Vertex Buffer Object ID (stores vertex data on GPU)
	std::vector<unsigned int> EBOs;					  // Element Buffer Object ID for each submesh (stores index data on GPU)
	std::vector<float> vertices;					  // vertex data for whole model (x1, y1, z1, x2, y2, z2, .. )
	std::vector<std::vector<unsigned short>> indices; // index data for each submesh (triangles)
	std::vector<unsigned int> textures;				  // texture ID(s)
	std::vector<std::string> sounds;				  // sound file name(s)
	std::vector<glm::mat4> meshTransform;			  // local transformation matrix for each mesh
	std::vector<int> submeshToMesh;
	glm::vec3 meshCenter;

	float meshPitch = 0.0f;
	float meshYaw = 0.0f;
	bool modelLoaded;

	char *DataBuffer; // raw binary content of .bdae file

	Model(const char *vertex, const char *fragment)
		: shader(vertex, fragment),
		  totalSubmeshCount(0),
		  VAO(0), VBO(0),
		  modelLoaded(false),
		  meshCenter(glm::vec3(-1.0f)),
		  DataBuffer(NULL)
	{
		shader.use();
		shader.setInt("modelTexture", 0);
		shader.setVec3("lightPos", lightPos);
		shader.setVec3("lightColor", lightColor);
		shader.setFloat("ambientStrength", ambientStrength);
		shader.setFloat("diffuseStrength", diffuseStrength);
		shader.setFloat("specularStrength", specularStrength);
	}

	//! Parses .bdae file and sets up model mesh and texture data.
	int init(IReadResFile *file);

	//! Recursively parses child nodes to accumulate local transformation matrix for a single mesh (called in init).
	void getNodeTransformation(int nodeInfoOffset, int nodeMeshIdx);

	//! Loads .bdae file from disk, calls the parser and searches for alternative textures and sounds.
	void load(const char *fpath, Sound &sound, bool isTerrainViewer);

	//! Clears GPU memory and resets viewer state.
	void reset();

	//! Renders .bdae model.
	void draw(glm::mat4 model, glm::mat4 view, glm::mat4 projection, glm::vec3 cameraPos, bool lighting, bool simple);
};

#endif
