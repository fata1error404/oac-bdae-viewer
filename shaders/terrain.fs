#version 330 core

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

void main()
{
    if (renderMode == 1)
    {
    // 1) Equal‐weight blend of whatever layers you have
        vec3 texCol = vec3(0.0);
        float totalWeight = 0.0;

        // Sample only valid textures, and only use their corresponding weights
        if (texIdx.x >= 0) {
            texCol += texture(terrainArray, vec3(TexCoord, texIdx.x)).rgb * BlendWeights.x;
            totalWeight += BlendWeights.x;
        }
        if (texIdx.y >= 0) {
            texCol += texture(terrainArray, vec3(TexCoord, texIdx.y)).rgb * BlendWeights.y;
            totalWeight += BlendWeights.y;
        }
        if (texIdx.z >= 0) {
            texCol += texture(terrainArray, vec3(TexCoord, texIdx.z)).rgb * BlendWeights.z;
            totalWeight += BlendWeights.z;
        }

        // Avoid divide-by-zero and normalize only if needed
        if (totalWeight > 0.0)
            texCol /= totalWeight;
        else
            texCol = vec3(0.2); // fallback dull color to avoid black

        // Limit the tint effect strictly to balanced range
        vec3 vertexTint = clamp(mix(vec3(1.0), BlendWeights.rgb, 0.2), 0.8, 1.2);

        // Final dim and color correction
        vec3 litCol = texCol * vertexTint;

        // LIGHTING
        if (lighting)
        {
            vec3 N = normalize(Normal);
            vec3 L = normalize(lightPos - PosWorldSpace); // light direction vector
            float diff = max(dot(N, L), 0.0);             // measure how aligned the surface is with the light (cos = 1 means the light hits water surface directly)

            vec3 I = normalize(cameraPos - PosWorldSpace); // view direction vector
            vec3 R = reflect(-L, N);                       // reflection vector
            float spec = pow(max(dot(I, R), 0.0), 64.0);   // measure how aligned the view direction is with the reflected light (cos = 1 means the light reflection hits the camera)

            vec3 ambient = ambientStrength  * lightColor;
            vec3 diffuse = 0.7  * lightColor * diff;
            //vec3 specular = specularStrength * lightColor * spec;

            litCol = (1.0 + diffuse) * litCol;
        }

        FragColor = vec4(litCol * 0.85, 1.0); // slightly dimmed

                // int count = 0;
        // if(texIdx.x >= 0) count++;
        // if(texIdx.y >= 0) count++;
        // if(texIdx.z >= 0) count++;
        // float w = count > 0 ? 1.0/float(count) : 0.0;

        // vec4 col = vec4(0);
        // if(texIdx.x >= 0) col += w * texture(terrainArray, vec3(TexCoord, texIdx.x));
        // if(texIdx.y >= 0) col += w * texture(terrainArray, vec3(TexCoord, texIdx.y));
        // if(texIdx.z >= 0) col += w * texture(terrainArray, vec3(TexCoord, texIdx.z));

        // FragColor = col;  // or mix with edgeColor via bary

        // float minB = min(min(barycentric.x, barycentric.y), barycentric.z); // figure out how “close” we are to an edge
        // float w = fwidth(minB) * 0.2;      // tweak 1.5 for thicker/thinner
        // float edgeFactor = smoothstep(w * 0.5, w, minB);

        // FragColor = mix(vec4(0.4f, 0.2f, 0.1f, 1.0f), vec4(0.76f, 0.60f, 0.42f, 1.0f), edgeFactor);
        //FragColor = texture(modelTexture, TexCoord);
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
        FragColor = vec4(0.59f, 0.29f, 0.0f, 0.5f); // brown with alpha
}