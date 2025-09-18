#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 PosWorldSpace;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    TexCoord = aTexCoord;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww; // a trick to force z = w so that after perspective division, depth is always 1.0, which is the max depth value at the far plane
}