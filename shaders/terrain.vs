#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord1;
layout (location = 3) in vec2 aTexCoord2;
layout (location = 4) in vec3 aTexIdx;      // indices for 3 chunk textures into texture array
layout (location = 5) in vec4 aColor;       // vertex color
layout (location = 6) in vec3 aBary;        // barycentric coord (for wireframe mode optimization)

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 PosWorldSpace;
out vec3 Normal;
out vec2 TexCoord1;
out vec2 TexCoord2;
out float TexIdx1;
out float TexIdx2;
out float TexIdx3;
out vec4 TexBlendWeights;
out vec3 Barycentric;

void main()
{
    PosWorldSpace = vec3(model * vec4(aPos, 1.0));
    Normal = aNormal;
    TexCoord1 = aTexCoord1;
    TexCoord2 = aTexCoord2;
    TexIdx1 = aTexIdx.x;
    TexIdx2 = aTexIdx.y; 
    TexIdx3 = aTexIdx.z; 
    TexBlendWeights = aColor;

    vec4 clipPos = projection * view * model * vec4(aPos, 1.0);
    Barycentric = aBary * clipPos.w;
    gl_Position = clipPos;
}