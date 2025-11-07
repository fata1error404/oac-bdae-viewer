#ifndef PARSER_BDAE_H
#define PARSER_BDAE_H

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include "IReadResFile.h"
#include "libs/glm/glm.hpp"
#include "libs/glm/gtc/quaternion.hpp"
#include "shader.h"
#include "sound.h"
#include "light.h"

// if defined, viewer prints detailed model info in terminal
#define CONSOLE_DEBUG_LOG

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
	unsigned short version;									// 2 bytes  byte order mark
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

struct Vertex
{
	glm::vec3 PosCoords;
	glm::vec3 Normal;
	glm::vec2 TexCoords;
	char BoneIndices[4];  // indices into boneNames array (up to 4 bones can influence 1 vertex)
	float BoneWeights[4]; // influence weights in [0, 1] range
};

struct Node
{
	std::string ID;				   // 1st node name (e.g. 'Bip001_Head-node')
	std::string mainName;		   // 2nd node name; used for mesh mapping (e.g. 'Bip001_Head')
	std::string boneName;		   // 3rd node name; used for bone mapping (e.g. 'Bone3')
	int parentIndex;			   // index of parent node into nodes array (-1 = root)
	std::vector<int> childIndices; // indices of child nodes into nodes array

	glm::mat4 pivotTransform; // transformation of the child helper PIVOT node (if it exists)

	// original transformation; used for animation reset
	glm::vec3 defaultTranslation;
	glm::quat defaultRotation;
	glm::vec3 defaultScale;

	// own current transformation; updated each frame during animation
	glm::vec3 localTranslation;
	glm::quat localRotation;
	glm::vec3 localScale;

	glm::mat4 totalTransform; // parent * local * pivot
};

struct BaseAnimation
{
	// channel data
	std::string targetNodeName; // model node that this animation affects
	int animationType;			// 1 = translation | 5 = rotation | 10 = scale

	// sampler data
	int interpolationType;							 // 0 = step | 1 = linear | 2 = hermite
	std::vector<float> timestamps;					 // time values in seconds
	std::vector<std::vector<float>> transformations; // transformation values (vectors or quaternions)
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
	unsigned int VAO;				// Vertex Attribute Object ID (stores vertex attribute configuration on GPU)
	unsigned int VBO;				// Vertex Buffer Object ID (stores vertex data on GPU)
	std::vector<unsigned int> EBOs; // Element Buffer Object ID for each submesh (stores index data on GPU)

	std::vector<Vertex> vertices;					  // vertex data
	std::vector<std::vector<unsigned short>> indices; // index data for each submesh (triangles)
	std::vector<unsigned int> textures;				  // texture ID(s)
	std::vector<std::string> sounds;				  // sound file name(s)

	glm::vec3 modelCenter; // geometric center of the model

	float meshPitch = 0.0f;
	float meshYaw = 0.0f;

	bool modelLoaded;

	char *DataBuffer; // raw binary content of .bdae file

	std::vector<Node> nodes; // node tree
	Shader defaultShader;	 // for nodes visualization
	unsigned int nodeVAO, nodeVBO, nodeEBO;

	// skinning data
	bool hasSkinningData;
	std::vector<std::string> boneNames;
	glm::mat4 bindShapeMatrix;					// correction matrix that transforms all model vertices from their raw positions defined in the .bdae file (mesh local space) to skeleton (or "bind pose") space (coordinate system where the skeleton was defined)
	std::vector<glm::mat4> bindPoseMatrices;	// inverse bind pose matrix for each bone; transforms vertices from their bind positions in skeleton space to the bone's local space
	std::vector<glm::mat4> boneTotalTransforms; // skinning matrix for each bone (it is node transform * inverse bind pose matrix); this matrix transforms a vertex to node's animated position

	// animation data
	std::vector<std::pair<float, std::vector<BaseAnimation>>> animations; // all loaded animation files data; for each file: {duration, set of base animations}
	bool animationsLoaded;												  // whether at least one animation file is loaded
	bool animationPlaying;												  // whether animation is playing
	int animationCount;													  // number of animation files found
	int selectedAnimation;												  // currently selected animation file index
	float currentAnimationTime;											  // current playback time

	// utility hash tables
	std::unordered_map<int, int> submeshToMeshIdx;			// (index in EBOs array → ..)
	std::unordered_map<int, int> meshToNodeIdx;				// (index in meshNames array → index in nodes array)
	std::unordered_map<std::string, int> nodeNameToIdx;		// (node name → index in nodes array)
	std::unordered_map<std::string, int> boneNameToNodeIdx; // (bone name → index in nodes array)
	std::unordered_map<int, int> boneToNodeIdx;				// (index in boneNames array → index in nodes array)

	Model(const char *vertex, const char *fragment)
		: DataBuffer(NULL),
		  shader(vertex, fragment),
		  defaultShader("shaders/default.vs", "shaders/default.fs"),
		  VAO(0), VBO(0),
		  nodeVAO(0), nodeVBO(0), nodeEBO(0),
		  fileSize(0),
		  vertexCount(0), faceCount(0),
		  totalSubmeshCount(0),
		  modelCenter(glm::vec3(-1.0f)),
		  textureCount(0),
		  alternativeTextureCount(0),
		  selectedTexture(0),
		  hasSkinningData(false),
		  modelLoaded(false),
		  animationsLoaded(false),
		  animationPlaying(false),
		  animationCount(0),
		  selectedAnimation(0),
		  currentAnimationTime(0.0f)
	{
		shader.use();
		shader.setInt("modelTexture", 0);
		shader.setVec3("lightPos", lightPos);
		shader.setVec3("lightColor", lightColor);
		shader.setFloat("ambientStrength", ambientStrength);
		shader.setFloat("diffuseStrength", diffuseStrength);
		shader.setFloat("specularStrength", specularStrength);
	}

	// functions for parsing: implemented in parserBDAE.cpp
	// ____________________

	//! Parses .bdae model file: textures, materials, meshes, mesh skin (if exist), and node tree.
	int init(IReadResFile *file);

	//! Loads .bdae model file from disk, calls init function and searches for animations, sounds, and alternative colors.
	void load(const char *fpath, Sound &sound, bool isTerrainViewer);

	//! Loads .bdae animation file from disk and parses animation samplers, channels, and data (timestamps and transformations).
	void loadAnimation(const char *animationFilePath);

	//! Recursively parses a node and its children.
	void parseNodesRecursive(int nodeOffset, int parentIndex);

	//! Recursively computes total transformation matrix for a node and its children.
	void updateNodesTransformationsRecursive(int nodeIndex, const glm::mat4 &parentTransform);

	//! [debug] Recursively prints the node tree.
	void printNodesRecursive(int nodeIndex, const std::string &prefix, bool isLastChild);

	//! Recursively searches down the tree starting from a given node for the first node with '_PIVOT' in its ID and returns its local transformation matrix.
	glm::mat4 getPIVOTNodeTransformationRecursive(int nodeIndex);

	// functions for rendering: implemented in model.cpp
	// ____________________

	//! Renders .bdae model.
	void draw(glm::mat4 model, glm::mat4 view, glm::mat4 projection, glm::vec3 cameraPos, float dt, bool lighting, bool simple);

	//! Applies a base animation (translation / rotation / scale) at a specific time, targeting one node.
	void applyBaseAnimation(BaseAnimation &baseAnim, float time);

	//! Interpolates between two floats.
	float interpolateFloat(float a, float b, float t, int interpolationType);

	//! Interpolates between two vectors.
	glm::vec3 interpolateVec3(glm::vec3 &a, glm::vec3 &b, float t, int interpolationType);

	//! Interpolates between two quaternions.
	glm::quat interpolateQuat(glm::quat &a, glm::quat &b, float t, int interpolationType);

	//! Resets animation to beginning.
	void resetAnimation();

	//! Clears GPU memory and resets viewer state.
	void reset();
};

#endif
