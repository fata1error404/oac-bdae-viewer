#version 330 core

out vec4 FragColor;

uniform int renderMode;

void main()
{
    if (renderMode == 1)
        FragColor = vec4(0.76f, 0.60f, 0.42f, 1.0f);
    else if (renderMode == 2)
        FragColor = vec4(0.4f, 0.2f, 0.1f, 1.0f);
    else if (renderMode == 3)
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    else if (renderMode == 4)
        FragColor = vec4(0.65f, 0.50f, 0.35f, 1.0f);
}