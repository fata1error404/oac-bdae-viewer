#ifndef PARSER_BDAE_H
#define PARSER_BDAE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "IReadResFile.h"
#include "libs/glm/fwd.hpp"
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

// Animation structures
// ____________________

enum class AnimationLoopMode
{
	LOOP = 0,	 // Normal loop: play forward, restart at beginning
	PINGPONG = 1 // Ping-pong: play forward, then reverse, then forward again
};

enum class InterpolationType
{
	STEP = 0,
	LINEAR = 1,
	HERMITE = 2
};

enum class ChannelType
{
	Transform = 0,
	Translation = 1,
	Position_X = 2,
	Position_Y = 3,
	Position_Z = 4,
	Rotation = 5,
	Rotation_X = 6,
	Rotation_Y = 7,
	Rotation_Z = 8,
	Rotation_W = 9,
	Scale = 10,
	Scale_X = 11,
	Scale_Y = 12,
	Scale_Z = 13
};

enum class DataType
{
	BYTE = 0,
	UBYTE = 1,
	SHORT = 2,
	USHORT = 3,
	INT = 4,
	UINT = 5,
	FLOAT = 6
};

// Keyframe source data
struct KeyframeSource
{
	DataType dataType;
	int componentCount;
	std::vector<float> data; // All data converted to float for simplicity
};

// Animation sampler (links input time to output keyframes)
struct AnimationSampler
{
	InterpolationType interpolation;
	int inputSourceIndex;  // Index into sources array (time values)
	int outputSourceIndex; // Index into sources array (keyframe values)
	DataType inputType;
	int inputComponentCount;
	DataType outputType;
	int outputComponentCount;
};

// Animation channel (links sampler to target node and property)
struct AnimationChannel
{
	std::string targetNodeName; // Name of the node to animate
	ChannelType channelType;	// What property to animate (translation, rotation, scale)
	int samplerIndex;			// Index into samplers array
};

// Complete animation
struct Animation
{
	std::string name;
	std::vector<AnimationSampler> samplers;
	std::vector<AnimationChannel> channels;
	float duration; // Calculated from max time value
};

// Node structure for hierarchy
// ____________________

struct Node
{
	std::string id;				   // Node ID (e.g., "Bone001-node")
	std::string name;			   // Node name (e.g., "Bone001")
	int parentIndex;			   // Index of parent node (-1 = root)
	std::vector<int> childIndices; // Indices of child nodes
	int dataOffset;				   // Offset in DataBuffer where node data is stored

	// Local transform (relative to parent)
	glm::vec3 localPosition;
	glm::quat localRotation;
	glm::vec3 localScale;

	// Original transform (for animation reset)
	glm::vec3 originalPosition;
	glm::quat originalRotation;
	glm::vec3 originalScale;

	// World transform (accumulated from root)
	glm::mat4 worldTransform;

	Node()
		: parentIndex(-1),
		  dataOffset(0),
		  localPosition(0),
		  localRotation(1, 0, 0, 0),
		  localScale(1),
		  originalPosition(0),
		  originalRotation(1, 0, 0, 0),
		  originalScale(1),
		  worldTransform(1.0f)
	{
	}
};

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
	std::vector<glm::mat4> meshPivotOffset;			  // pivot offset for each mesh (for animation updates)
	std::vector<int> submeshToMesh;
	glm::vec3 meshCenter;

	float meshPitch = 0.0f;
	float meshYaw = 0.0f;
	bool modelLoaded;

	char *DataBuffer; // raw binary content of .bdae file

	// Node hierarchy
	std::vector<Node> nodes;					// All nodes in the model
	std::map<std::string, int> nodeNameToIndex; // Quick lookup: node name -> index
	std::map<int, int> nodeToMeshIndex;			// Maps node index to mesh index (for nodes that have corresponding meshes)
	int nodeCollectionCount;					// Number of node collections in the model

	// Node visualization
	Shader nodeShader;						// Shader for rendering nodes
	unsigned int nodeVAO, nodeVBO, nodeEBO; // OpenGL buffers for node sphere
	int nodeSphereVertexCount;				// Number of vertices in unit sphere
	int nodeSphereIndexCount;				// Number of indices in unit sphere

	// Animation data
	std::vector<Animation> animations;			 // All animations
	std::vector<KeyframeSource> keyframeSources; // All keyframe source data
	bool animationsLoaded;						 // Whether animations have been loaded
	float currentAnimationTime;					 // Current playback time
	bool animationPlaying;						 // Whether animation is playing
	float animationSpeed;						 // Playback speed multiplier
	AnimationLoopMode loopMode;					 // How animation should loop
	bool animationReversed;						 // Whether animation is playing in reverse (for ping-pong)

	Model(const char *vertex, const char *fragment)
		: shader(vertex, fragment),
		  nodeShader("shaders/default.vs", "shaders/default.fs"),
		  totalSubmeshCount(0),
		  VAO(0), VBO(0),
		  nodeVAO(0), nodeVBO(0), nodeEBO(0),
		  nodeSphereVertexCount(0),
		  nodeSphereIndexCount(0),
		  modelLoaded(false),
		  meshCenter(glm::vec3(-1.0f)),
		  DataBuffer(NULL),
		  nodeCollectionCount(0),
		  animationsLoaded(false),
		  currentAnimationTime(0.0f),
		  animationPlaying(false),
		  animationSpeed(1.0f),
		  loopMode(AnimationLoopMode::PINGPONG),
		  animationReversed(false)
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
	void getPivotOffset(int nodeInfoOffset, int nodeMeshIdx, bool skipFirst);

	//! Loads .bdae file from disk, calls the parser and searches for alternative textures and sounds.
	void load(const char *fpath, Sound &sound, bool isTerrainViewer);

	//! Clears GPU memory and resets viewer state.
	void reset();

	//! Renders .bdae model.
	void draw(glm::mat4 model, glm::mat4 view, glm::mat4 projection, glm::vec3 cameraPos, bool lighting, bool simple);

	//! Node hierarchy functions
	void parseNodeHierarchy();
	void parseNodeRecursive(int nodeOffset, int parentIndex);
	void updateNodeTransforms();
	void updateNodeTransformsRecursive(int nodeIndex);
	void updateMeshTransformsFromNodes();
	void printNodeHierarchy(int nodeIndex, int depth);

	//! Node visualization functions
	void generateUnitSphere();
	void drawNodes(glm::mat4 model, glm::mat4 view, glm::mat4 projection);

	//! Animation functions
	void loadAnimations(const char *animationFilePath);
	void updateAnimations(float deltaTime);
	void applyAnimation(const Animation &anim, float time);
	float interpolateFloat(float a, float b, float t, InterpolationType interp);
	glm::vec3 interpolateVec3(const glm::vec3 &a, const glm::vec3 &b, float t, InterpolationType interp);
	glm::quat interpolateQuat(const glm::quat &a, const glm::quat &b, float t, InterpolationType interp);
	void playAnimation();
	void pauseAnimation();
	void resetAnimation();
};

#endif
