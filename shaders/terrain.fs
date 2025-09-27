#version 330 core

in vec3 PosWorldSpace;
in vec3 barycentric;
in vec2 TexCoord1;         // Detail UVs
in vec2 TexCoord2;         // Mask UVs
in float texIdx1;
in float texIdx2;
in float texIdx3;
in vec4 BlendWeights;      // Optional baked blend weights
in vec3 Normal;

out vec4 FragColor;

uniform vec3 cameraPos;
uniform sampler2DArray baseTextureArray;
uniform sampler2D maskTexture;
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
        vec4 color1 = vec4(0.0, 0.0, 0.0, 0.0);
        vec4 color2 = vec4(0.0, 0.0, 0.0, 0.0);
        vec4 color3 = vec4(0.0, 0.0, 0.0, 0.0);

        color1 = texture(baseTextureArray, vec3(TexCoord1, texIdx1));
        color2 = texture(baseTextureArray, vec3(TexCoord1, texIdx2));
        color3 = texture(baseTextureArray, vec3(TexCoord1, texIdx3));

        vec4 mask = texture(maskTexture, TexCoord2);
        vec3 baseColor =
            color1.rgb * BlendWeights.b * (1.0 - mask.r - mask.g) +
            color2.rgb * BlendWeights.r * mask.r +
            color3.rgb * BlendWeights.g * mask.g;

        if (lighting)
        {
            vec3 N = normalize(Normal);
            vec3 L = normalize(lightPos - PosWorldSpace);
            float diff = max(dot(N, L), 0.0);

            vec3 ambient = 0.5 * lightColor;
            vec3 diffuse = 0.4 * lightColor * diff;

            baseColor = (ambient + diffuse) * baseColor;
        }

        FragColor = vec4(baseColor, 1.0);
    }
    else if (renderMode == 2)
        FragColor = vec4(0.4f, 0.2f, 0.1f, 1.0f);
    else if (renderMode == 3)
    {
        float minB = min(min(barycentric.x, barycentric.y), barycentric.z);
        float w = fwidth(minB) * 0.2;
        float edgeFactor = smoothstep(w * 0.5, w, minB);

        FragColor = mix(vec4(0.4f, 0.2f, 0.1f, 1.0f), vec4(0.76f, 0.60f, 0.42f, 1.0f), edgeFactor);
    }
    else if (renderMode == 4)
        FragColor = vec4(0.73f, 0.58f, 0.40f, 0.9f);
    else if (renderMode == 5)
        FragColor = vec4(0.59f, 0.29f, 0.0f, 0.5f);
}