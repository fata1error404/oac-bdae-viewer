#version 330 core

in vec3 PosWorldSpace;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 cameraPos;
uniform sampler2D modelTexture;
uniform int renderMode;

uniform bool lighting;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;

void main()
{
    if (renderMode == 1)
    {
        vec4 baseColor = texture(modelTexture, TexCoord);

        if (baseColor.a < 0.1)
            discard;

        vec3 result = baseColor.rgb;

        if (lighting)
        {
            vec3 N = normalize(Normal);
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