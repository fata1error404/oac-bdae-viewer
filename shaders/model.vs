// .vs = vertex shader; executed on each vertex
#version 330 core

// input from GPU vertex buffer (per-vertex attributes defined with glVertexAttribPointer)
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

// input from application (same for all vertices within single draw call)
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

// output to fragment shader
out vec3 PosWorldSpace;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    Normal = aNormal;
    TexCoord = aTexCoord;
    PosWorldSpace = vec3(model * vec4(aPos, 1.0));             // transform vertex position from object to world space (for lighting calculations in fragment shader)
    gl_Position = projection * view * model * vec4(aPos, 1.0); // transform vertex position from object to clip space (gl_Position is built-in GLSL variable that only exists in vertex shader and required by GPU; it is 4D coordinate (x, y, z, w) where w is clipping value later used to discard vertices that are not in camera view via perspective division)
}