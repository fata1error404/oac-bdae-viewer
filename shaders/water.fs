#version 330 core

in vec3 PosWorldSpace;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 cameraPos;
uniform sampler2D waterTexture;

uniform bool lighting;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;

void main()
{
    // base water
    vec4 baseColor = texture(waterTexture, TexCoord);
    vec3 result = baseColor.rgb;

    vec3 N = normalize(Normal);
    vec3 I = normalize(cameraPos - PosWorldSpace); // view direction vector
    vec3 R = reflect(-I, N);                       // reflection vector

    // lighting (Phong lighting model: ambient + diffuse + specular)
    if (lighting)
    {
        vec3 L = normalize(lightPos - PosWorldSpace); // light direction vector
        float diff = max(dot(N, L), 0.0);             // measure how aligned the surface is with the light (cos = 1 means the light hits water surface directly)

        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(I, R), 0.0), 64.0); // measure how aligned the view direction is with the reflected light (cos = 1 means the light reflection hits the camera)

        vec3 ambient = ambientStrength  * lightColor;
        vec3 diffuse = diffuseStrength  * lightColor * diff; // apply diffuse lighting only if the fragment is not in shadow (this allows for rendering shadowed areas)

        vec3 specular = specularStrength * lightColor * spec;

        result = (ambient + diffuse + specular) * result;
    }

    FragColor = vec4(result, 1.0);
}