# Vulkan Path Tracer

A physically-based path tracer built from scratch using Vulkan ray tracing extensions, implementing global illumination, soft shadows, and Monte Carlo light transport on the GPU.

![Render](assets/vulkan_pt.gif)

---

## Features

### Vulkan Ray Tracing Pipeline
- Built using `VK_KHR_ray_tracing_pipeline`
- Hardware-accelerated BVH traversal (TLAS / BLAS)
- Custom Shader Binding Table (SBT)

### Physically-Based Rendering
- Global illumination (indirect diffuse bounces)
- Soft shadows via area light sampling
- Recursive reflections

### Monte Carlo Path Tracing
- Cosine-weighted hemisphere sampling
- Progressive frame accumulation
- Stochastic sampling with per-pixel random seeds

### Materials & Assets
- OBJ + MTL loading
- Per-vertex material system
- Texture support via descriptor arrays

---

## Technical Highlights

- Multi-ray system (primary, shadow, indirect rays)
- Multiple payloads and miss shaders
- Correct SBT alignment (`shaderGroupHandleAlignment`, `shaderGroupBaseAlignment`)
- GPU-friendly memory layout (`std430`-safe structs)
- Explicit Vulkan synchronization and image layout management

---

## Tech Stack

| Component     | Technology                        |
|---------------|-----------------------------------|
| Language      | C++                               |
| Graphics API  | Vulkan (Ray Tracing Extensions)   |
| Shaders       | GLSL → SPIR-V                     |
| Math          | GLM                               |

---

## Build & Run

### Requirements
- Vulkan SDK 1.3+
- GPU with ray tracing support (RTX / RDNA2+)
- CMake

### Build
```bash
git clone https://github.com/abhigyan04/vulkan-path-tracer.git
cd vulkan-path-tracer
mkdir build && cd build
cmake ..
make
```

### Run
```bash
./build/vulkan-path-tracer.exe
```

---

## Challenges & Learnings

- Debugging GPU-side issues with minimal feedback (black frames, device loss)
- Matching CPU ↔ GPU memory layouts (struct alignment bugs)
- Correctly configuring Shader Binding Table regions and indexing
- Managing descriptor sets and texture bindings in Vulkan
- Implementing unbiased sampling while controlling noise

---

## Future Improvements

- [ ] Multiple Importance Sampling (MIS)
- [ ] Physically-based BRDFs (GGX / PBR)
- [ ] HDR environment lighting
- [ ] Denoising & temporal stability
- [ ] Performance optimizations (sampling + traversal)

---

## Acknowledgements

Inspired by real-time rendering techniques and physically-based path tracing principles.
