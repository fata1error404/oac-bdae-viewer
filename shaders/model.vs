// .vs = vertex shader; executed on each vertex
#version 330 core

// input from GPU vertex buffer (per-vertex attributes defined with glVertexAttribPointer)
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in ivec4 aBoneIndices;
layout (location = 4) in vec4 aBoneWeights;

// input from application (same for all vertices within single draw call)
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

const int MAX_BONES = 128;
uniform mat4 boneTotalTransforms[MAX_BONES];
uniform bool useSkinning;

// output to fragment shader
out vec3 PosWorldSpace;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    vec4 position = vec4(aPos, 1.0);
    vec3 normal = aNormal;
    
    if (useSkinning)
    {   
        /* linear blend skinning (LBS):

           v' = Σ_i ( w_i * (B_i * v) )
           where:
           v' — final skinned vertex position
           v  — original vertex in bind pose
           w_i — bone weight (Σ_i w_i = 1)
           B_i — bone skinning matrix  */

        mat4 blendedBoneMatrix = mat4(0.0);
        blendedBoneMatrix += boneTotalTransforms[aBoneIndices.x] * aBoneWeights.x;
        blendedBoneMatrix += boneTotalTransforms[aBoneIndices.y] * aBoneWeights.y;
        blendedBoneMatrix += boneTotalTransforms[aBoneIndices.z] * aBoneWeights.z;
        blendedBoneMatrix += boneTotalTransforms[aBoneIndices.w] * aBoneWeights.w;
        
        position = blendedBoneMatrix * vec4(aPos, 1.0); // apply skinning (move vertex to the weighted position)
        normal = mat3(blendedBoneMatrix) * aNormal;
    }
    
    Normal = normal;
    TexCoord = aTexCoord;
    PosWorldSpace = vec3(model * position);             // transform vertex position from object to world space (for lighting calculations in fragment shader)
    gl_Position = projection * view * model * position; // transform vertex position from object to clip space
}
