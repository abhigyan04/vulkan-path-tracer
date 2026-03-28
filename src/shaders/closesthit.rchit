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

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

layout(push_constant) uniform PushConstants
{
    uint frame;
} pushConstants;

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT RayPayload secondaryPayload;
layout(location = 2) rayPayloadEXT float shadowPayload;

hitAttributeEXT vec2 attributes;

const float RAY_BIAS = 0.001;

vec3 randomHemisphere(vec3 normal, vec2 xi)
{
    float phi = 2.0 * 3.14159265359 * xi.x;
    float cosTheta = sqrt(1.0 - xi.y);
    float sinTheta = sqrt(xi.y);

    vec3 tangent = normalize(abs(normal.x) > 0.1 ? cross(vec3(0, 1, 0), normal) : cross(vec3(1, 0, 0), normal));
    vec3 bitangent = cross(normal, tangent);

    return normalize(tangent * cos(phi) * sinTheta + bitangent * sin(phi) * sinTheta + normal * cosTheta);
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
    vec3 lightU = vec3(0.25, 0, 0);
    vec3 lightV = vec3(0, 0, 0.25);
    vec3 lightNormal = vec3(0, -1, 0);

    vec2 seed = vec2(gl_LaunchIDEXT.xy) + vec2(float(pushConstants.frame), float(gl_PrimitiveID));

    vec2 xi = vec2(rand(seed + vec2(0.13, 0.71)), rand(seed + vec2(0.57, 0.29)));

    vec3 sampledLightPos = lightPosition + (xi.x * 2.0 - 1.0) * lightU + (xi.y * 2.0 - 1.0) * lightV;

    vec3 toLight = sampledLightPos - hitPosition;

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

    float LdotNl = max(dot(-lightDirection, lightNormal), 0.0);

    float lightAttenuation = 1.0 / max(lightDistance * lightDistance, 1e-4);

    vec3 ambient = baseColor * 0.02;

    vec3 diffuse = baseColor * NdotL * visibility * LdotNl * lightAttenuation * 8.0;

    // Reflection Ray
    vec3 reflectedColor = vec3(0.0);

    float reflectivity = 0.1;

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

    if(payload.depth < 1)
    {
        vec2 xi = vec2(rand(seed + vec2(2.1, 3.7)), rand(seed + vec2(4.3, 1.9)));

        vec3 bouncedDirection = randomHemisphere(shadingNormal, xi);

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

        float NdotB = max(dot(shadingNormal, bouncedDirection), 0.0);

        indirectColor = indirectPayload.color * baseColor * NdotB;
    }
    
    vec3 color = ambient + diffuse + reflectedColor * reflectivity + indirectColor * 0.3;
 
    payload.color = color;
    return;
}
