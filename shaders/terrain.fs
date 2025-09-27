#version 330 core
// [TODO] annotate

in vec3 PosWorldSpace;
in vec3 barycentric;
in vec2 TexCoord;
flat in ivec3 texIdx;
in vec4 BlendWeights;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 cameraPos;
uniform sampler2DArray terrainArray;
uniform int renderMode;

uniform bool lighting;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform int terrainBlendMode; // 0 = average, 1 = weighted, 2 = dominant

void main()
{
    // if (renderMode == 1)
    // {
    //     vec3 texCol = vec3(0.0);
    //     float count = 0.0;
    //     vec3 weights = max(vec3(0.0), BlendWeights.rgb);

    //     if (texIdx.x >= 0) { texCol += texture(terrainArray, vec3(TexCoord, texIdx.x)).rgb; count += 1.0; }
    //     if (texIdx.y >= 0) { texCol += texture(terrainArray, vec3(TexCoord, texIdx.y)).rgb; count += 1.0; }
    //     if (texIdx.z >= 0) { texCol += texture(terrainArray, vec3(TexCoord, texIdx.z)).rgb; count += 1.0; }
    //     if (count > 0.0) texCol /= count; else texCol = vec3(0.2);

    //     vec3 tinted = texCol * clamp(BlendWeights.rgb, 0.0, 1.0);

    //     vec3 litCol = tinted;
    //     if (lighting)
    //     {
    //         vec3 N = Normal;
    //         vec3 L = normalize(lightPos - PosWorldSpace);
    //         float diff = max(dot(N, L), 0.0);

    //         vec3 ambient = 0.5  * lightColor;
    //         vec3 diffuse = 0.4  * lightColor * diff;

    //         litCol = (ambient + diffuse) * litCol;
    //     }

    //     FragColor = vec4(litCol * 0.85, 1.0);
    // }
    if (renderMode == 1)
    {
        // vec3 texCol = vec3(0.2); // fallback color

        vec3 weights = clamp(BlendWeights.rgb, 0.0, 1.0);
        vec3 texCol = vec3(0.0);
        if (texIdx.x >= 0) texCol += texture(terrainArray, vec3(TexCoord, texIdx.x)).rgb * weights.x;
        if (texIdx.y >= 0) texCol += texture(terrainArray, vec3(TexCoord, texIdx.y)).rgb * weights.y;
        if (texIdx.z >= 0) texCol += texture(terrainArray, vec3(TexCoord, texIdx.z)).rgb * weights.z;


        vec3 litCol = texCol;

        if (lighting)
        {
            vec3 N = normalize(Normal);
            vec3 L = normalize(lightPos - PosWorldSpace);
            float diff = max(dot(N, L), 0.0);

            vec3 ambient = 0.5 * lightColor;
            vec3 diffuse = 0.4 * lightColor * diff;

            litCol *= (ambient + diffuse);
        }

        FragColor = vec4(litCol * 0.85, 1.0);
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