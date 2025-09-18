#version 330 core

in vec3 PosWorldSpace;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 cameraPos;
uniform sampler2D modelTexture;

void main()
{
    FragColor = texture(modelTexture, TexCoord);
}