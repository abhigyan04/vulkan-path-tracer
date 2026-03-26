#version 460
#extension GL_EXT_ray_tracing : require

struct RayPayload
{
    vec3 color;
    int depth;
};

layout(location = 0) rayPayloadInEXT RayPayload secondaryPayload;

void main()
{
    vec3 d = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (d.y + 1.0);
    secondaryPayload.color = mix(vec3(1.0, 1.0, 1.0), vec3(0.4, 0.6, 1.0), t);
    // payload.color = vec3(0.0);
}
