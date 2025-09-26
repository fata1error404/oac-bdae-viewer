// .fs = fragment shader; executed on each fragment 
#version 330 core

/* 
 fragment comes from 1 triangle (or other geometric primitive passed in glDrawElements): 
  - 3 vertices of the triangle provide attributes
  - GPU interpolates attributes across triangle’s surface
  - for each pixel the triangle covers, rasterizer produces one fragment
*/

// input from vertex shader
in vec3 PosWorldSpace;
in vec3 Normal;
in vec2 TexCoord;

// input from application (same for all fragments within single draw call)
uniform vec3 cameraPos;
uniform sampler2D modelTexture;
uniform int renderMode; // 1 = textured, 2 = wireframe (mesh edges), 3 = mesh faces

uniform bool lighting;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;

// output
out vec4 FragColor;

void main()
{
    if (renderMode == 1)
    {
        // sample base color from texture using interpolated u-v coordinates
        vec4 baseColor = texture(modelTexture, TexCoord);

        // discard nearly transparent fragments
        if (baseColor.a < 0.1)
            discard;

        vec3 result = baseColor.rgb;

        // lighting (Phong lighting model: ambient + diffuse + specular)
        if (lighting)
        {
            vec3 N = normalize(Normal);                   // surface normal
            vec3 L = normalize(lightPos - PosWorldSpace); // light direction vector
            float diff = max(dot(N, L), 0.0);             // measure how aligned the surface is with the light (cos = 1 means the light hits water surface directly)

            vec3 I = normalize(cameraPos - PosWorldSpace); // view direction vector
            vec3 R = reflect(-L, N);                       // reflection vector
            float spec = pow(max(dot(I, R), 0.0), 64.0);   // measure how aligned the view direction is with the reflected light (cos = 1 means the light reflection hits the camera)

            vec3 ambient = ambientStrength  * lightColor;
            vec3 diffuse = diffuseStrength  * lightColor * diff;
            vec3 specular = specularStrength * lightColor * spec;

            result = (ambient + diffuse + specular) * result;
        }

        FragColor = vec4(result, baseColor.a);
    }
    else if (renderMode == 2)
        FragColor = vec4(0.3, 0.3, 0.3, 1.0);
    else if (renderMode == 3)
        FragColor = vec4(0.75, 0.75, 0.75, 1.0);
}