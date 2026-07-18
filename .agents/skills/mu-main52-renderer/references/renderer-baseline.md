# Renderer baseline and known findings

## Current stack

- The default remains the legacy `wglCreateContext` path. The opt-in
  `-gl33compat` path first creates that bootstrap context, then requests an
  explicit OpenGL 3.3 Compatibility context through
  `wglCreateContextAttribsARB`; creation, activation, or GLEW failure restores
  the legacy context. Core Profile is not requested.
- The base `PIXELFORMATDESCRIPTOR` in `Winmain.cpp` requests 16-bit color and 16-bit depth.
- An ARB multisample path already exists in `ZzzOpenglUtil.cpp::InitGLMultisample` through `wglChoosePixelFormatARB`; it requests 24-bit color, 8-bit alpha, 16-bit depth, no stencil, and MSAA. Modernizing the base/fallback format must account for this existing path rather than describing MSAA as absent.
- GLEW is active; its result is logged and an opt-in context that fails GLEW
  initialization falls back to the bootstrap legacy context.
- Fixed-function and client-array code remains widespread.
- ImGui uses the OpenGL2 backend.
- `SHADER_PIPELINE` and experimental `SHADER_VERSION_TEST` are enabled.

## Shader systems

### CShaderScene

Loads only Terrain and Character from `Client/Shaders`, with `Client/Data/Shaders` retained as the loader fallback. `CShaderScene::BindProgram` is the shared client program-binding owner and uses its synchronized program cache instead of querying `GL_CURRENT_PROGRAM` on every request. A 2026-07-18 A/B runtime check reproduced the same pre-existing 27 FPS drop with both the cache and the GL query, excluding this cache change as the cause.

### CShaderGL

Loads only the GPU-skinned BMD Model program. It prefers one std140 UBO bone palette, recompiles the same source for a validated uniform-array fallback, and leaves the fixed-function/client-array mesh renderer authoritative if either GPU transport is unavailable.

The tracked production GLSL set is exactly `Shaders/terrain.*`, `Shaders/character.*`, and `Data/Effect/VBO/Model.*`. Dormant Glow, Colorize, Skin, material variants, nested `Data/Data/Shaders`, and `Data/Effect/Shader` experiments are not runtime owners.

## Verified risks

1. Standard world objects may defer CPU vertex/normal transforms, but translated character/monster passes and every special-material/CPU consumer still materialize or retain the legacy path. Extend eligibility only after profiler and screenshot parity.
2. Expanded triangle vertices use a sequential EBO with no index reuse; measure memory/bandwidth before changing the validated layout.
3. The UBO path is portable only where the validated 9,600-byte block and required entry points exist; the uniform-array and legacy fallbacks must remain.
4. The legacy base format is 16/16, while the existing ARB MSAA selector requests 24/8/16/0. A 32-color/24-depth/8-stencil modernization must preserve ARB selection and fallback.
5. Terrain, effects and much of the client remain immediate-mode/fixed-function; they are likely a larger crowded-scene cost than the already-isolated Model shader, but require runtime measurements.
6. GLAD appears unused while GLEW and `glprocs.lib` remain; verify packaging and call sites before cleanup.

## 2026-07-18 profiler baseline

The protected runtime accepted `-renderprofiler` and produced main-scene evidence.
The captured worst crowded-scene window reproduced 27 FPS, 35.600 ms mean frame
time, and 33.951 ms in `Render`. BMD mesh work accounted for 17.512 ms: 499.0
successful VBO draws/frame used 1.981 ms while 757.9 legacy mesh draws/frame used
14.696 ms. The first-failure gate attributed 659.9 meshes/frame to translated
rendering, far above the 49.0 unlit and 49.0 missing-VAO gates. Deferred
transforms were 310.0 per frame and only 43.0 were later materialized. The same
window also exposed 5,750.8 draw calls/frame and 2,635.2 immediate-mode draws/frame,
confirming a separate fixed-function/draw-granularity blocker before Core Profile.

This proves the existing Model VBO path is healthy and that translated
character/equipment rendering is the largest measured eligibility blocker. It
does not authorize blindly restoring the earlier translated prototype: that test
showed set-effect flicker, so effect/material ownership must be mapped and parity
validated before expanding the gate.

## 2026-07-18 OpenGL 3.3 Compatibility validation

The opt-in `-gl33compat` launch requested an OpenGL 3.3 Compatibility context
through `wglCreateContextAttribsARB` and selected it without invoking the legacy
fallback. NVIDIA exposed that compatible request as OpenGL 4.6 with profile mask
`0x2` (Compatibility), which is permitted because the returned context satisfies
the requested minimum while retaining deprecated entry points. GLEW 2.2, Terrain,
Character and Model programs linked successfully; the Model program selected its
validated 200-bone UBO transport.

The captured worst crowded main-scene window reached 25 FPS with 38.173 ms mean
frame time and 36.250 ms in `Render`. BMD work remained split the same way as the
legacy-context baseline: 513.0 successful VBO draws/frame consumed 2.220 ms,
while 737.9 legacy draws/frame consumed 15.675 ms. Translated rendering remained
the dominant first-failure gate at 636.9 meshes/frame, and the scene still issued
5,558.8 total and 2,499.2 immediate-mode draws/frame. Context selection therefore
preserved the measured renderer architecture and did not remove the pre-existing
crowded-scene bottleneck. It is a compatibility milestone, not an FPS claim.

## First actions

1. Audit actual GLSL assets.
2. Capture GL vendor/renderer/version/GLSL and limits.
3. Check GLEW and entry points.
4. Fix program save/restore and unify ownership.
5. Add resource cleanup.
6. Establish screenshot/frame-time baselines.
7. Remove duplicated CPU render work after equivalence tests.

## Terrain modernization safety

- Do not freeze the entire terrain vertex payload into a static VBO.
- `ZzzLodTerrain.cpp` mutates grass vertex positions from `TerrainGrassWind` / `g_fTerrainGrassWind1`, computes per-draw `colors[]` from `PrimaryTerrainLight`, and animates water texture coordinates from `WaterMove` and wind values.
- Buffer stable base height/topology, indices, and invariant coordinates as static data.
- Move grass wind, water motion, and dynamic lighting/color to shader uniforms/textures/attributes or explicitly updated dynamic streams.
- Establish visual parity for grass, water, changing terrain light, event maps, and map transitions before removing the legacy path.

## GL 3.3 distinction

- Compatibility Profile is the practical intermediate target and retains legacy functions.
- Core Profile requires replacing matrix stack, immediate mode, client states, fixed-function alpha/lighting/texture behavior, GLU, and the ImGui OpenGL2 backend.
