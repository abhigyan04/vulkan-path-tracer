#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT vec3 secondaryPayload;

void main()
{
    vec3 d = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (d.y + 1.0);
    secondaryPayload = mix(vec3(1.0, 1.0, 1.0), vec3(0.4, 0.6, 1.0), t);
}
