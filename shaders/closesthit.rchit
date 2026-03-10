#version 460
#extension GL_EXT_ray_tracing : require

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;

layout(location = 0) rayPayloadInEXT vec3 payload;
layout(location = 1) rayPayloadEXT vec3 secondaryPayload;

hitAttributeEXT vec2 attributes;

void main()
{
    vec3 A = vec3(-1.0, -1.0, 0.0);
    vec3 B = vec3(1.0, -1.0, 0.0);
    vec3 C = vec3(0.0, 1.0, 0.0);

    vec3 edge1 = B- A;
    vec3 edge2 = C - A;

    vec3 normal = normalize(cross(edge1, edge2));

    vec3 hitPosition = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

    vec3 reflectedDirection = normalize(reflect(gl_WorldRayDirectionEXT, normal));

    secondaryPayload = vec3(0.0);

    traceRayEXT(
        tlas,
        gl_RayFlagsOpaqueEXT,
        0xFF,
        0, 0, 0,
        hitPosition + normal * 0.001,
        0.001,
        reflectedDirection,
        10000.0,
        0
    );

    //vec3 lightDirection = normalize(vec3(1.0, 1.0, 1.0));

    //float NdotL = max(dot(normal, lightDirection), 0.0);

    //vec3 baseColor = vec3(1.0, 0.3, 0.3);

    float u = attributes.x;
    float v = attributes.y;
    float w = 1.0 - u - v;

    vec3 barycentricColor = vec3(w, u, v);

    payload = 0.25 * barycentricColor + 0.75 * secondaryPayload;
}
