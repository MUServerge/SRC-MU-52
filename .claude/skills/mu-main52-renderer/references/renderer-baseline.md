# Renderer baseline and known findings

## Current stack

- Legacy context is created by `wglCreateContext`; OpenGL 3.3 is not explicitly requested.
- The base `PIXELFORMATDESCRIPTOR` in `Winmain.cpp` requests 16-bit color and 16-bit depth.
- An ARB multisample path already exists in `ZzzOpenglUtil.cpp::InitGLMultisample` through `wglChoosePixelFormatARB`; it requests 24-bit color, 8-bit alpha, 16-bit depth, no stencil, and MSAA. Modernizing the base/fallback format must account for this existing path rather than describing MSAA as absent.
- GLEW is active and `glewInit()` result is ignored.
- Fixed-function and client-array code remains widespread.
- ImGui uses the OpenGL2 backend.
- `SHADER_PIPELINE` and experimental `SHADER_VERSION_TEST` are enabled.

## Shader systems

### CShaderScene

Loads Default, Terrain, Glow, Character, and Colorize. Verified call sites currently use Terrain and Character only.

### CShaderGL

Loads a legacy shared `shader_id` plus VBO programs Model, BlendMesh, Metal, Oil, and Chrome1–7. The legacy `RenderShader`/`RenderVertexBuffer` path has no verified caller; the material VBO path is conditionally active.

Runtime GLSL assets are outside the source repository and must be audited from `Shaders`, optional `Data/Shaders`, and `Data/Effect/VBO`.

## Verified risks

1. `CShaderScene` may bind Character while `RenderMeshVBO` replaces it and ends with `glUseProgram(0)`, corrupting state ownership for later draws.
2. `BMD::Release` does not delete mesh VAO/VBO/EBO resources.
3. `BMD::Transform` performs full CPU vertex/normal work before GPU skinning.
4. Legacy chrome UV work runs before the VBO shader recomputes it.
5. `BMD::RenderMeshVBO` calls `glGetFloatv` for model-view and projection on every mesh draw, then each `vboSet*` call performs `glGetUniformLocation`. This per-draw query cost is verified in current `ZzzBMD.cpp` and `CShaderGL.cpp`; cache locations after link and supply pass-level matrices without changing matrix semantics.
6. Expanded triangle vertices use a sequential EBO with no reuse.
7. The maximum upload is 200 bones * 3 `vec4` = 600 `vec4` = 2400 components. This exceeds the OpenGL 3.3 guaranteed minimum `GL_MAX_VERTEX_UNIFORM_COMPONENTS` of 1024, although typical desktop drivers often expose more and may run it successfully. Treat it as non-portable rather than automatically broken: query runtime limits and provide a UBO/TBO or equivalent fallback after auditing GLSL limits.
8. The legacy base format is 16/16, while the existing ARB MSAA selector requests 24/8/16/0. A 32-color/24-depth/8-stencil modernization should preserve ARB selection and provide a compatible fallback, then verify the actual chosen format at runtime.
9. GLAD appears unused while GLEW and `glprocs.lib` remain; verify before cleanup.

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
