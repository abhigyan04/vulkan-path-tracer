#version 460
#extension GL_EXT_ray_tracing : require

struct Vertex
{
    vec4 position;
    vec4 normal;
    uvec4 materialID;
};

struct Material
{
    vec4 diffuse;
    vec4 specular;
    vec4 emissive;
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

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;

struct RayPayload
{
    vec3 color;
    int depth;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT RayPayload secondaryPayload;
layout(location = 2) rayPayloadEXT float shadowPayload;

hitAttributeEXT vec2 attributes;

const float RAY_BIAS = 0.001;

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

    uint matID = v0.materialID.x;
    Material mat = materials[matID];

    vec3 baseColor = clamp(mat.diffuse.xyz, 0.0, 1.0);
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

    vec3 geometricNormal = faceNormal;

    //Shadow Ray
    vec3 origin = hitPosition + faceNormal * RAY_BIAS;

    vec3 lightPosition = vec3(0, 1.9, 0);

    vec3 toLight = lightPosition - hitPosition;

    float lightDistance = length(toLight);
    
    vec3 lightDirection = normalize(toLight);

    shadowPayload = 0.0;

    traceRayEXT(
        tlas,
        gl_RayFlagsTerminateOnFirstHitEXT |
        gl_RayFlagsOpaqueEXT,
        0xFF,
        1, 0, 1,
        origin,
        RAY_BIAS,
        lightDirection,
        max(lightDistance - RAY_BIAS, RAY_BIAS),
        2
    );

    float visibility = clamp(shadowPayload, 0.0, 1.0);

    float NdotL = max(dot(shadingNormal, lightDirection), 0.0);

    float lightAttenuation = 1.0 / max(lightDistance * lightDistance, 0.2);

    vec3 ambient = baseColor * 0.08;

    vec3 diffuse = baseColor * NdotL * visibility * lightAttenuation * 2.0;

    // Reflection Ray
    // vec3 reflectedColor = vec3(0.0);

    // float reflectivity = 0.0;

    // if(payload.depth < 1)
    // {
    //     vec3 reflectedDirection = normalize(reflect(gl_WorldRayDirectionEXT, faceNormal));

    //     vec3 reflectedOrigin = hitPosition + faceNormal * RAY_BIAS;

    //     secondaryPayload.depth = payload.depth + 1;
    //     secondaryPayload.color = vec3(0.0);

    //     traceRayEXT(
    //         tlas,
    //         gl_RayFlagsOpaqueEXT,
    //         0xFF,
    //         0, 0, 0,
    //         reflectedOrigin,
    //         0.001,
    //         reflectedDirection,
    //         10000.0,
    //         1
    //     );

    //     reflectedColor = secondaryPayload.color;
    // }
    
    // vec3 color = ambient + diffuse + reflectedColor * reflectivity;
 
    payload.color = ambient + diffuse;
    return;
}
