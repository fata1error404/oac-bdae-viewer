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
        FragColor = vec4(0.73f, 0.58f, 0.40f, 1.0f);
}