#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aBary;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTexIdx;
layout (location = 4) in vec4 aBlend;
layout (location = 5) in vec3 aNormal;

out vec3 barycentric;
out vec2 TexCoord;
flat out ivec3 texIdx;  
out vec4 BlendWeights;
out vec3 Normal;
out vec3 PosWorldSpace;
    
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    PosWorldSpace = vec3(model * vec4(aPos, 1.0));
    vec4 clipPos = projection * view * model * vec4(aPos, 1.0);
    texIdx = ivec3(aTexIdx); 
    gl_Position = clipPos;
    BlendWeights  = aBlend;
    Normal = mat3(transpose(inverse(model))) * aNormal;

    // store barycentric multiplied by clip-space w
    barycentric = aBary * clipPos.w;
    TexCoord = aTexCoord;
}