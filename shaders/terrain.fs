#version 330 core

in vec3 PosWorldSpace;
in vec3 Normal;
in vec2 TexCoord1;
in vec2 TexCoord2;
in float TexIdx1;
in float TexIdx2;
in float TexIdx3;
in vec4 TexBlendWeights;
in vec3 Barycentric;

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

out vec4 FragColor;

void main()
{
if (renderMode == 1)
{
        vec4 color1 = texture(baseTextureArray, vec3(TexCoord1, TexIdx1));
        vec4 color2 = texture(baseTextureArray, vec3(TexCoord1, TexIdx2));
        vec4 color3 = texture(baseTextureArray, vec3(TexCoord1, TexIdx3));
        vec4 mask = texture(maskTexture, TexCoord2);

        // clamp mask channels to safe range
        float mr = clamp(mask.r, 0.0, 1.0);
        float mg = clamp(mask.g, 0.0, 1.0);
        float mb = clamp(mask.b, 0.0, 1.0);

        float w1 = TexBlendWeights.b * max(0.0, 1.0 - mr - mg); // base layer
        float w2 = TexBlendWeights.r * mr;                     // red layer
        float w3 = TexBlendWeights.g * mg;                     // green layer

        // normalize weights to avoid division by zero and preserve color energy
        float sumW = w1 + w2 + w3;
        const float EPS = 1e-6;
        vec3 blended;
        if (sumW > EPS)
        {
            blended = (color1.rgb * w1 + color2.rgb * w2 + color3.rgb * w3) / sumW;
        }
        else
        {
            // fallback: use color1 if weights are effectively zero
            blended = color1.rgb;
        }

        if (lighting)
        {
            vec3 N = normalize(Normal);
            vec3 L = normalize(lightPos - PosWorldSpace);
            float diff = max(dot(N, L), 0.0);

            vec3 ambient = 0.2 * lightColor;   // use your uniforms
            vec3 diffuse = 0.5 * lightColor * diff;

            // shadow application: reduce brightness based on mask.b but do not zero-out
            // shadowStrength in [0,1] controls how strong the shadow channel is
            float shadowStrength = 1.0; // tweak: 1.0 = full effect, 0.5 = half
            float shadow = mix(1.0, 1.0 - mb * shadowStrength, 1.0); // result in (0,1]
            vec3 lightingTerm = ambient + diffuse;

            blended = lightingTerm * shadow * blended;
        }

        FragColor = vec4(blended, 1.0);
    }
    else if (renderMode == 2)
        FragColor = vec4(0.4f, 0.2f, 0.1f, 1.0f);
    else if (renderMode == 3)
    {
        float minB = min(min(Barycentric.x, Barycentric.y), Barycentric.z);
        float w = fwidth(minB) * 0.2;
        float edgeFactor = smoothstep(w * 0.5, w, minB);

        FragColor = mix(vec4(0.4f, 0.2f, 0.1f, 1.0f), vec4(0.76f, 0.60f, 0.42f, 1.0f), edgeFactor);
    }
    else if (renderMode == 4)
        FragColor = vec4(0.73f, 0.58f, 0.40f, 0.9f);
    else if (renderMode == 5)
        FragColor = vec4(0.59f, 0.29f, 0.0f, 0.5f);
}