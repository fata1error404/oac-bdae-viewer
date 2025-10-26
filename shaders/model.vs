// .vs = vertex shader; executed on each vertex
#version 330 core

// input from GPU vertex buffer (per-vertex attributes defined with glVertexAttribPointer)
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aBoneIDs;      // Bone IDs (4 influences per vertex) - stored as floats
layout (location = 4) in vec4 aBoneWeights;  // Bone weights (4 influences per vertex)

// input from application (same for all vertices within single draw call)
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

// Skeletal animation uniforms
const int MAX_BONES = 128;  // Maximum number of bones supported
uniform mat4 boneTransforms[MAX_BONES];  // Bone transformation matrices
uniform bool useSkinning;  // Whether to apply skeletal animation

// output to fragment shader
out vec3 PosWorldSpace;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    vec4 finalPosition = vec4(aPos, 1.0);
    vec3 finalNormal = aNormal;
    
    // Apply skeletal animation if enabled
    if (useSkinning)
    {
        // Convert float bone IDs to integers
        int boneID0 = int(aBoneIDs.x);
        int boneID1 = int(aBoneIDs.y);
        int boneID2 = int(aBoneIDs.z);
        int boneID3 = int(aBoneIDs.w);
        
        // Calculate skinned position by blending bone transformations
        mat4 boneTransform = mat4(0.0);
        boneTransform += boneTransforms[boneID0] * aBoneWeights.x;
        boneTransform += boneTransforms[boneID1] * aBoneWeights.y;
        boneTransform += boneTransforms[boneID2] * aBoneWeights.z;
        boneTransform += boneTransforms[boneID3] * aBoneWeights.w;
        
        // Transform position and normal by blended bone matrix
        finalPosition = boneTransform * vec4(aPos, 1.0);
        finalNormal = mat3(boneTransform) * aNormal;
    }
    
    Normal = finalNormal;
    TexCoord = aTexCoord;
    PosWorldSpace = vec3(model * finalPosition);             // transform vertex position from object to world space (for lighting calculations in fragment shader)
    gl_Position = projection * view * model * finalPosition; // transform vertex position from object to clip space
}
