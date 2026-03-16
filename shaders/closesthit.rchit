#version 460
#extension GL_EXT_ray_tracing : require

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;

struct Vertex
{
    vec3 position;
    float pad0;
    vec3 normal;
    float pad1;
};

layout(set = 0, binding = 3) buffer VertexBuffer
{
    Vertex vertices[];
};

layout(location = 0) rayPayloadInEXT vec3 payload;
layout(location = 1) rayPayloadEXT vec3 secondaryPayload;
layout(location = 2) rayPayloadEXT float shadowPayload;

hitAttributeEXT vec2 attributes;

void main()
{
    float u = attributes.x;
    float v = attributes.y;
    float w = 1.0 - u - v;

    uint triIndex = gl_PrimitiveID * 3;

    vec3 n0 = vertices[triIndex + 0].normal;
    vec3 n1 = vertices[triIndex + 1].normal;
    vec3 n2 = vertices[triIndex + 2].normal;

    vec3 normal = normalize(w * n0 + u * n1 + v * n2);

    vec3 baseColor = vec3(0.8, 0.8, 0.8);

    float reflectivity = 0.2;

    vec3 hitPosition = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

    vec3 origin = hitPosition + normal * 0.01;

    vec3 lightPosition = vec3(5.0, 6.0, 3.0);

    vec3 toLight = lightPosition - hitPosition;

    float lightDistance = length(toLight);
    
    vec3 lightDirection = normalize(toLight);

    shadowPayload = 0.0;

    traceRayEXT(
        tlas,
        gl_RayFlagsTerminateOnFirstHitEXT |
        gl_RayFlagsOpaqueEXT |
        gl_RayFlagsSkipClosestHitShaderEXT,
        0xFF,
        0, 0, 1,
        origin,
        0.001,
        lightDirection,
        lightDistance - 0.01,
        2
    );

    float NdotL = max(dot(normal, lightDirection), 0.0);

    vec3 diffuse = baseColor * NdotL * shadowPayload;

    vec3 reflectedDirection = normalize(reflect(gl_WorldRayDirectionEXT, normal));

    secondaryPayload = vec3(0.0);

    traceRayEXT(
        tlas,
        gl_RayFlagsOpaqueEXT,
        0xFF,
        0, 0, 0,
        origin,
        0.001,
        reflectedDirection,
        10000.0,
        1
    );

    payload = mix(diffuse, secondaryPayload, reflectivity);

    //if(gl_PrimitiveID == 0)
    //{
        //Main Triangle
    //    vec3 A = vec3(-1.0, -1.0, 0.0);
    //   vec3 B = vec3(1.0, -1.0, 0.0);
    //    vec3 C = vec3(0.0, 1.0, 0.0);

    //    vec3 edge1 = B- A;
    //    vec3 edge2 = C - A;

    //    normal = normalize(cross(edge1, edge2));

    //    baseColor = vec3(1.0, 0.2, 0.2);
    //    reflectivity = 0.75;
    //}
    //else
    //{
    //    normal = vec3(0.0, 1.0, 0.0);

    //    baseColor = vec3(0.7, 0.7, 0.7);
    //    reflectivity = 0.35;
    //}

    //payload = vec3(shadowPayload);
}
