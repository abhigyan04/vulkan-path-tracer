#version 460
#extension GL_EXT_ray_tracing : require

struct Vertex
{
    vec4 position;
    vec4 normal;
    vec2 uv;
    vec2 pad;
    uvec4 materialID;
};

struct Material
{
    vec4 diffuse;
    vec4 specular;
    vec4 emissive;
    uvec4 textureInfo; // x: hasTexture, y: textureIndex, zw: padding
};

layout(std430, set = 0, binding = 3) buffer VertexBuffer
{
    Vertex vertices[];
};

layout(std430, set = 0, binding = 4) buffer IndexBuffer
{
    uint indices[];
};

layout(std430, set = 0, binding = 5) buffer MaterialBuffer
{
    Material materials[];
};

#define MAX_TEXTURES 64
layout(set = 0, binding = 7) uniform sampler2D textures[MAX_TEXTURES];

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;

struct RayPayload
{
    vec3 color;
    int depth;
};

layout(push_constant) uniform PushConstants
{
    uint frame;
} pushConstants;

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT RayPayload secondaryPayload;
layout(location = 2) rayPayloadEXT float shadowPayload;

hitAttributeEXT vec2 attributes;

const float PI = 3.14159265359;
const float RAY_BIAS = 0.001;

uint pcg(uint inputValue)
{
    uint state = inputValue * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand(inout uint rngState)
{
    rngState = pcg(rngState);
    return float(rngState) / 4294967295.0;
}

uint initRng(uvec2 pixel, uint frame, uint primitiveID, uint bounce)
{
    uint seed = pixel.x;
    seed ^= pixel.y * 1664525u;
    seed ^= frame * 1013904223u;
    seed ^= primitiveID * 2246822519u;
    seed ^= bounce * 3266489917u;
    return pcg(seed);
}

vec3 buildTangent(vec3 n)
{
    return normalize(abs(n.x) > 0.1 ? cross(vec3(0, 1, 0), n) : cross(vec3(1, 0, 0), n));
}

vec3 cosineWeightedHemisphere(vec3 normal, vec2 xi)
{
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt(1.0 - xi.y);
    float sinTheta = sqrt(xi.y);

    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float z = sqrt(max(0.0, 1.0 - xi.y));

    vec3 tangent = buildTangent(normal);
    vec3 bitangent = cross(normal, tangent);

    return normalize(tangent * x + bitangent * y + normal * z);
}

void main()
{
    uint triIndex = gl_PrimitiveID;

    uint i0 = indices[triIndex * 3 + 0];
    uint i1 = indices[triIndex * 3 + 1];
    uint i2 = indices[triIndex * 3 + 2];

    Vertex v0 = vertices[i0];
    Vertex v1 = vertices[i1];
    Vertex v2 = vertices[i2];

    float u = attributes.x;
    float v = attributes.y;
    float w = 1.0 - u - v;

    vec2 uv = w * v0.uv + u * v1.uv + v * v2.uv;

    uint matID = v0.materialID.x;
    Material mat = materials[matID];

    vec3 baseColor = clamp(mat.diffuse.xyz, 0.0, 1.0);

    if(mat.textureInfo.x == 1 && mat.textureInfo.y < MAX_TEXTURES)
    {
        baseColor = texture(textures[mat.textureInfo.y], uv).rgb;
    }

    vec3 emissiveColor = clamp(mat.emissive.xyz, 0.0, 1.0);

    if(length(emissiveColor) > 0.0)
    {
        payload.color = emissiveColor * 10.0;
        return;
    }

    vec3 hitPosition = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

    vec3 faceNormal = normalize(cross(v1.position.xyz - v0.position.xyz, v2.position.xyz - v0.position.xyz));

    faceNormal = faceforward(faceNormal, gl_WorldRayDirectionEXT, faceNormal);

    vec3 shadingNormal = normalize(w * v0.normal.xyz + u * v1.normal.xyz + v * v2.normal.xyz);

    shadingNormal = faceforward(shadingNormal, gl_WorldRayDirectionEXT, faceNormal);

    //RNG Seed
    uint rngState = initRng(uvec2(gl_LaunchIDEXT.xy), pushConstants.frame, triIndex, uint(payload.depth));

    //Area light
    vec3 lightCenter = vec3(0, 1.9, 0);
    // vec3 lightCenter = vec3(-2.8, 102, 29);
    vec3 lightU = vec3(0.25, 0, 0);
    vec3 lightV = vec3(0, 0, 0.25);
    vec3 lightNormal = vec3(0, -1, 0);

    vec2 lightXi = vec2(rand(rngState), rand(rngState));

    vec3 sampledLightPos = lightCenter + (lightXi.x * 2.0 - 1.0) * lightU + (lightXi.y * 2.0 - 1.0) * lightV;

    vec3 toLight = sampledLightPos - hitPosition;

    float lightDistance = length(toLight);
    
    vec3 lightDirection = normalize(toLight);

    //Shadow Ray
    vec3 shadowOrigin = hitPosition + faceNormal * RAY_BIAS;

    shadowPayload = 0.0;

    traceRayEXT(
        tlas,
        gl_RayFlagsTerminateOnFirstHitEXT |
        gl_RayFlagsOpaqueEXT,
        0xFF,
        1, 0, 1,
        shadowOrigin,
        RAY_BIAS,
        lightDirection,
        max(lightDistance - RAY_BIAS, RAY_BIAS),
        2
    );

    float visibility = clamp(shadowPayload, 0.0, 1.0);

    float NdotL = max(dot(shadingNormal, lightDirection), 0.0);

    float LdotNl = max(dot(-lightDirection, lightNormal), 0.0);

    float lightAttenuation = 1.0 / max(lightDistance * lightDistance, 1e-4);

    vec3 directLight = baseColor * NdotL * visibility * LdotNl * lightAttenuation * 8.0;

    vec3 ambient = baseColor * 0.005;

    // Reflection Ray
    vec3 reflectedColor = vec3(0.0);

    float reflectivity = 0.05;

    if(payload.depth < 1)
    {
        vec3 reflectedDirection = normalize(reflect(gl_WorldRayDirectionEXT, faceNormal));

        vec3 reflectedOrigin = hitPosition + faceNormal * RAY_BIAS;

        secondaryPayload.depth = payload.depth + 1;
        secondaryPayload.color = vec3(0.0);

        traceRayEXT(
            tlas,
            gl_RayFlagsOpaqueEXT,
            0xFF,
            0, 0, 0,
            reflectedOrigin,
            0.001,
            reflectedDirection,
            10000.0,
            1
        );

        reflectedColor = secondaryPayload.color;
    }

    //Indirect ray
    vec3 indirectColor = vec3(0.0);

    if(payload.depth < 2)
    {
        vec2 bounceXi = vec2(rand(rngState), rand(rngState));

        vec3 bouncedDirection = cosineWeightedHemisphere(shadingNormal, bounceXi);

        vec3 indirectOrigin = hitPosition + shadingNormal * RAY_BIAS;

        RayPayload indirectPayload;
        indirectPayload.depth = payload.depth + 1;
        indirectPayload.color = vec3(0.0);

        traceRayEXT(
            tlas,
            gl_RayFlagsOpaqueEXT,
            0xFF,
            0, 0, 0,
            indirectOrigin,
            RAY_BIAS,
            bouncedDirection,
            10000.0,
            1
        );

        indirectColor = indirectPayload.color * baseColor * 1.2;
    }
    
    vec3 color = ambient + directLight + indirectColor * 0.35 + reflectedColor * reflectivity;

    color = clamp(color, vec3(0.0), vec3(5.0));
 
    payload.color = color;
    return;
}
