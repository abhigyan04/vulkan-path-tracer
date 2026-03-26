#version 460
#extension GL_EXT_ray_tracing : require

struct Vertex
{
    vec3 position;
    float pad0;
    vec3 normal;
    float pad1;
};

struct Material
{
    vec3 diffuse;
    vec3 specular;
    vec3 emissive;
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

layout(std430, set = 0, binding = 6) buffer MaterialIDBuffer
{
    uint materialIDs[];
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

    vec3 normal = normalize(w * v0.normal + u * v1.normal + v * v2.normal);

    uint matID = materialIDs[triIndex];
    Material mat = materials[matID];

    vec3 baseColor = clamp(mat.diffuse.xyz, 0.0, 1.0);
    vec3 emissiveColor = clamp(mat.emissive.xyz, 0.0, 1.0);

    if(length(emissiveColor) > 0.0)
    {
        payload.color = emissiveColor;
        return;
    }

    vec3 lightDirection = normalize(vec3(1,1,1));
    float NdotL = max(dot(normal, lightDirection), 0.0);

    vec3 color = baseColor * NdotL;

    payload.color = color;

    // vec3 c0 = vertices[triIndex + 0].color;
    // vec3 c1 = vertices[triIndex + 1].color;
    // vec3 c2 = vertices[triIndex + 2].color;

    // vec3 baseColor = w * c0 + u * c1 + v * c2;

    // baseColor = clamp(baseColor, 0.0, 1.0);

    // if (baseColor.r > 0.9 && baseColor.g > 0.9 && baseColor.b > 0.9 && normal.y < -0.9)  payload.color = vec3(12.0);

    // int matID = vertices[triIndex].materialID;
    // Material mat = materials[matID];

    // vec3 baseColor = clamp(mat.diffuse, 0.0, 1.0);

    // vec3 hitPosition = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

    // vec3 shadingNormal = normal; // for lighting
    // vec3 geometricNormal = faceforward(normal, -gl_WorldRayDirectionEXT, normal);

    // // normal = faceforward(normal, -gl_WorldRayDirectionEXT, normal);

    // vec3 origin = hitPosition + geometricNormal * max(0.01, 0.001 * gl_HitTEXT);

    // // vec3 lightPosition = vec3(5.0, 6.0, 3.0);
    // vec3 lightPosition = vec3(0.0, 1.9, 0.0);

    // vec3 toLight = lightPosition - hitPosition;

    // float lightDistance = length(toLight);
    
    // vec3 lightDirection = normalize(toLight);

    // if(abs(hitPosition.y - 1.9) < 0.05) { payload.color = vec3(10.0); return; }

    // // if(length(mat.emissive) > 0.0)
    // // {
    // //     payload.color = mat.emissive;
    // //     return;
    // // }

    // shadowPayload = 0.0;

    // traceRayEXT(
    //     tlas,
    //     gl_RayFlagsTerminateOnFirstHitEXT |
    //     gl_RayFlagsOpaqueEXT |
    //     gl_RayFlagsSkipClosestHitShaderEXT,
    //     0xFF,
    //     0, 0, 1,
    //     origin,
    //     0.001,
    //     lightDirection,
    //     max(lightDistance - 0.01, 0.001),
    //     2
    // );

    // shadowPayload = clamp(shadowPayload, 0.0, 1.0);

    // float NdotL = max(dot(shadingNormal, lightDirection), 0.0);

    // vec3 ambient = baseColor * 0.05;

    // ambient = clamp(ambient, 0.0, 1.0);

    // float lightAttenuation = 1.0 / (lightDistance * lightDistance);
    // vec3 diffuse = baseColor * NdotL * shadowPayload * lightAttenuation * 10.0;

    // diffuse = clamp(diffuse, 0.0, 1.0);

    // vec3 color = ambient + diffuse * 0.8;

    // color = clamp(color, 0.0, 1.0);

    // vec3 reflectedColor = vec3(0.0);
    // float reflectivity = 0.3;

    // if(payload.depth < 1)
    // {
    //     vec3 reflectedDirection = reflect(gl_WorldRayDirectionEXT, geometricNormal);

    //     float rLen = length(reflectedDirection);
    //     if(rLen > 1e-5)
    //         reflectedDirection /= rLen;
    //     else
    //         reflectedDirection = normal;

    //     vec3 reflectedOrigin = hitPosition + geometricNormal * 0.05;

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

    // // vec3 reflectedColor = payload;

    // // secondaryPayload = clamp(secondaryPayload, 0.0, 1.0);

    // // secondaryPayload *= 0.5;

    // if(any(isnan(ambient)) || any(isnan(diffuse)))
    // {
    //     payload.color = vec3(1.0, 0.0, 1.0); // magenta debug
    //     return;
    // }

    // payload.color = clamp(color + reflectivity * reflectedColor, 0.0, 5.0);
    // payload.color = color + 0.05 * reflectedColor;
    // payload.color = reflectedColor;

    //payload = mix(color, secondaryPayload, reflectivity);
    //payload = diffuse + secondaryPayload;
    // payload = clamp(color + secondaryPayload * reflectivity, 0.0, 1.0);
    // payload = secondaryPayload;
    //payload = diffuse;
    //payload = baseColor;
    //payload = normal * 0.5 + 0.5;
    // payload = color;
    //payload = ambient;
    //payload = vec3(shadowPayload);
}
