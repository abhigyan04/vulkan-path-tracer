#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT vec3 secondaryPayload;

void main()
{
    secondaryPayload = vec3(0.1, 0.1, 0.2);
}
