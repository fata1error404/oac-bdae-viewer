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

out vec3 barycentric;
out vec3 Normal;
out vec2 TexCoord1;
out vec2 TexCoord2;
out vec4 BlendWeights;
out vec3 PosWorldSpace;

out float texIdx1;
out float texIdx2;
out float texIdx3;

void main()
{
    PosWorldSpace = vec3(model * vec4(aPos, 1.0));
    vec4 clipPos = projection * view * model * vec4(aPos, 1.0);
    texIdx1 = aTexIdx.x;
    texIdx2 = aTexIdx.y; 
    texIdx3 = aTexIdx.z; 
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord1 = aTexCoord1;
    TexCoord2 = aTexCoord2;
    BlendWeights  = aColor;
    barycentric = aBary * clipPos.w;
    gl_Position = clipPos;
}