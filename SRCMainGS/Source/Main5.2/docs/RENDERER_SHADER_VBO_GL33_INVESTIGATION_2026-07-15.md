# Renderer, Shader, VBO and OpenGL 3.3 Investigation

Date: 2026-07-15

Scope: `SRCMainGS\Source\Main5.2`, `Client\Shaders`, `Client\Data\Data\Shaders`, and `Client\Data\Effect`

Method: read-only static investigation of the authoritative local worktree

Build/run status: intentionally not built or run; runtime-dependent conclusions are identified explicitly

Line numbers in this report refer to the local, uncommitted worktree inspected on the date above. They can move as that worktree changes.

## 1. Executive summary

The active renderer is a hybrid compatibility renderer. It still depends on fixed-function matrices, immediate mode, client arrays, global OpenGL state, and CPU-transformed BMD data. Two GLSL owners are active under `SHADER_VERSION_TEST` and `SHADER_PIPELINE`: `CShaderScene` wraps broad terrain and character passes, while `CShaderGL` owns an experimental per-mesh VAO/VBO/GPU-skinning path. They overlap during world-character rendering: `CShaderScene` binds the character program, then an eligible BMD mesh can temporarily bind the `CShaderGL` Model program.

The current VBO path is narrower than its shader inventory suggests. Its gate accepts only plain, lit `RENDER_TEXTURE` meshes in `MAIN_SCENE`; chrome, metal, oil, blend, wave, bright/dark, shadow, light-map, and other special materials remain on the legacy path. Ten non-Model material programs are compiled but currently unreachable through that gate. The common BMD gate means the Model path is not intrinsically character-only: eligible world objects, monsters, equipment, items, and some 3D previews can also reach it.

The normal context path requests only 16 color bits and 16 depth bits and requests no stencil. It uses `ChoosePixelFormat`, `SetPixelFormat`, and legacy `wglCreateContext`; it does not request OpenGL 3.3 or a profile. The existing ARB multisample selector requests 24 color + 8 alpha, 16 depth, and 0 stencil, but is compiled behind an undefined macro and has no caller. Therefore 32-bit RGBA, 24-bit depth, and 8-bit stencil are not established by the source. The selected driver's actual format is obtained with `DescribePixelFormat` but is not logged.

The most important confirmed correctness/lifetime issue is shutdown order: `KillGLWindow` releases the current context during `WM_DESTROY`, while ImGui, model, shader-program, texture, and buffer teardown happens later or in static destructors. BMD VAO/VBO/EBO handles have no matching deletion at all, so model reload/reallocation leaks them. Cleanup must not simply be added to `BMD::Release` until shutdown is reordered so every GL deletion runs while the owning context is current.

GPU skinning currently duplicates CPU per-vertex transformation for the same eligible draw. `NeedsCpuVertexTransform` always returns true because shadows/effects and other legacy consumers still require `VertexTransform`; the vertex shader then transforms the bind-pose vertices again. Bone matrices use three `vec4` rows per bone and the shader reserves `u_Bones[600]`. The draw clamps uploads to 200 bones, but no hardware uniform-limit check exists, and release builds have no safe model fallback for bone indices at or above 200. This is not automatically broken on common GPUs, but it is not guaranteed by OpenGL 3.3's minimum vertex-uniform capacity.

The safest first implementation step is diagnostics only: log the actual selected pixel format, GLEW result, OpenGL/GLSL version/profile, required entry points and limits; add gated renderer counters to the existing local `RenderProfiler`; and establish reproducible baselines. The first code-consolidation slice after that should centralize program binding/restoration and cache uniform locations while keeping both public managers as adapters, shader selection unchanged, and the legacy fallback intact.

No production source, shader, project, asset, binary, or changelog file was changed for this investigation.

## 2. Actual OpenGL initialization/context flow

### Active flow

1. `CreateOpenglWindow` builds a base `PIXELFORMATDESCRIPTOR` in `source\Winmain.cpp:503-512` with `PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER`, `PFD_TYPE_RGBA`, `cColorBits = 16`, and `cDepthBits = 16`. The zero-initialized descriptor leaves alpha and stencil at zero.
2. It obtains the device context with `GetDC` at `source\Winmain.cpp:514`.
3. It calls `ChoosePixelFormat` at `source\Winmain.cpp:524`, then `DescribePixelFormat` at `source\Winmain.cpp:532`, replacing the local descriptor with the chosen format's actual description.
4. It calls `SetPixelFormat` at `source\Winmain.cpp:540`.
5. It creates a legacy WGL context with `wglCreateContext` at `source\Winmain.cpp:548` and makes it current at `source\Winmain.cpp:556`. There is no `wglCreateContextAttribsARB`, version request, profile mask, or context flags request.
6. It calls `glewInit()` at `source\Winmain.cpp:564`, but ignores the return code and does not validate the entry points later used by the VBO path.
7. It initializes `g_ShaderScene` at `source\Winmain.cpp:566-569` and enables `GL_MULTISAMPLE` at `source\Winmain.cpp:571`, whether or not the chosen pixel format contains multisample buffers.
8. Later initialization calls `gShaderGL->Init()` at `source\Winmain.cpp:1141-1143`, before `gmClientModels` construction and model loading at `source\Winmain.cpp:1191-1193` and later asset-load calls.

### Base format conclusion

| Property | Active request | Final actual value |
|---|---:|---|
| Color | 16 bits (`Winmain.cpp:510`) | Unknown without runtime `DescribePixelFormat` logging |
| Alpha | 0 requested | Unknown |
| Depth | 16 bits (`Winmain.cpp:511`) | Unknown |
| Stencil | 0 requested | Unknown |
| Double buffer | Requested | Selected format must satisfy the request |
| Multisample | Not selected by the active path | Unknown; merely enabling `GL_MULTISAMPLE` does not create samples |

The source does not prove 32-bit color, 24-bit depth, or 8-bit stencil. A driver may choose a richer compatible format, but that is runtime evidence, not a source guarantee.

### Dormant ARB/multisample path

`InitGLMultisample` is defined in `source\ZzzOpenglUtil.cpp:782-902` under `LDS_ADD_MULTISAMPLEANTIALIASING` (`source\ZzzOpenglUtil.cpp:748`). No definition of that macro and no call to `InitGLMultisample` exists in the inspected client tree. The function requests `WGL_COLOR_BITS_ARB = 24`, `WGL_ALPHA_BITS_ARB = 8`, `WGL_DEPTH_BITS_ARB = 16`, and `WGL_STENCIL_BITS_ARB = 0` at `source\ZzzOpenglUtil.cpp:826-835`, then calls `wglChoosePixelFormatARB` at `source\ZzzOpenglUtil.cpp:844` and `:863`.

This path is not the active base format selector and, even if activated unchanged, would still not request 24-bit depth or 8-bit stencil. It also does not provide the dummy-window/bootstrap sequence required to choose an ARB pixel format before creating the final context.

### Extension and capability handling

- GLEW is included by `source\StdAfx.h:88` and linked by `Main.vcxproj:100,166`. GLAD headers exist in the tree, but no active GLAD loader call was found.
- There is no `wglewInit` call and no centralized capability record.
- No checks were found for `GL_MAX_VERTEX_UNIFORM_COMPONENTS`, `GL_MAX_VERTEX_UNIFORM_VECTORS`, `GL_MAX_UNIFORM_BLOCK_SIZE`, or `GL_MAX_TEXTURE_BUFFER_SIZE`.
- `CErrorReport::WriteOpenGLInfo` logs vendor, renderer, version, maximum texture size, and viewport dimensions at `source\Utilities\Log\ErrorReport.cpp:244-255`; it omits GLSL version, context/profile flags, actual pixel-format fields, VAO/integer-attribute support, and bone-transport limits.
- `WGLExtensionSupported` obtains `wglGetExtensionsStringARB` and calls it without a null-pointer check at `source\ZzzOpenglUtil.cpp:2217-2222`, creating a separate compatibility hazard on drivers that do not expose that function.

### Fallback behavior

- `CShaderScene::Init` loads individual programs and preserves program `0` for failed programs (`source\CShaderScene.cpp:127-154`). Callers check availability before wrapping their passes.
- `CShaderGL::loadVBOProgram` deletes and returns program `0` on link failure (`source\CShaderGL.cpp:78-115`). `BMD::RenderMeshVBO` returns false if `UseVBO` fails, and `BMD::RenderMesh` falls through to the legacy draw (`source\ZzzBMD.cpp:1615-1641`, `:3590-3597`).
- The loaders do not protect calls to VAO, integer attribute, and shader entry points when GLEW initialization or capabilities are insufficient. The fallback is therefore program-level, not a complete context/capability fallback.

### Shutdown flow and ordering

`WM_DESTROY` calls `KillGLWindow` in `source\WINHANDLE.cpp:618-628`. `KillGLWindow` clears the current context at `source\Winmain.cpp:211`, deletes it at `:217`, and releases the DC at `:226`. After the message loop returns, `gwinhandle->Destroyer()` is called at `source\Winmain.cpp:1273-1275`. `WINHANDLE::Destroyer` shuts down ImGui and calls `DestroyWindow` at `source\WINHANDLE.cpp:165-178`; `DestroyWindow` releases characters, water, models, and bitmaps at `source\Winmain.cpp:410-461`.

That order is inverted for OpenGL ownership. `DestroyImGuiWindow` invokes the OpenGL2 backend shutdown at `source\Winmain.cpp:597-605`, model release occurs at `source\Winmain.cpp:456-459`, `CShaderScene` releases from its global destructor (`source\CShaderScene.cpp:32-35,201-216`), and `CShaderGL` deletes programs from its singleton destructor (`source\CShaderGL.cpp:15-23`)—all after, or potentially after, the context is gone. The required order is renderer/UI/model/program/texture/buffer release while current, then unbind, delete context, and release DC.

## 3. Full shader and asset inventory

### Compile-time controls and owners

- `SHADER_VERSION_TEST` and `SHADER_PIPELINE` are enabled at `source\Defined_Global.h:77-78`.
- `CShaderScene` declares `Default`, `Terrain`, `Glow`, `Character`, and `Colorize` in `source\CShaderScene.h:32-40`; global `g_ShaderScene` is declared at `:80`.
- `CShaderGL` declares `eVBO_Model`, `BlendMesh`, `Metal`, `Oil`, and `Chrome1` through `Chrome7` at `source\CShaderGL.h:15-29`; `gShaderGL` is a singleton macro at `:81`.
- `CShaderScene::CompileShader` and `LinkProgram` are at `source\CShaderScene.cpp:61-106`; `CShaderGL::run_shader` and `loadVBOProgram` are at `source\CShaderGL.cpp:78-125,207-225`.

The normal runtime working directory must be `Client` because the client consistently resolves assets below `Data\...`. Consequently, `Shaders\...` resolves to `Client\Shaders`. `CShaderScene`'s alternate `Data\Shaders\...` lookup (`source\CShaderScene.cpp:37-58`) would resolve to `Client\Data\Shaders`, not the existing `Client\Data\Data\Shaders`. The latter is an asset tree, but it is not reached by the inspected loader under the normal working directory.

### Program inventory

State expectation abbreviations: `compat` means it consumes fixed-function matrices/built-ins or legacy surrounding state; `core` means explicit attributes/outputs but still depends on the surrounding renderer for blend/depth/cull. All current programs use texture unit 0 unless stated otherwise.

| Logical program | Owner and source | Runtime VS / FS | Initialization and flag | Confirmed consumers | Vertex input; uniforms/samplers | State, cleanup, fallback | Status |
|---|---|---|---|---|---|---|---|
| Scene Default | `CShaderScene`; `source\CShaderScene.cpp:127-154` | `Client\Shaders\shader.vs` / `shader.fs` | `CreateOpenglWindow`, `Winmain.cpp:566-569`; `SHADER_PIPELINE` | No direct render caller found | Compatibility built-ins; `texture1` | Surrounding legacy state; global destructor; caller can remain legacy | Loaded but no confirmed draw; duplicated |
| Scene Terrain | `CShaderScene`; enum `CShaderScene.h:32-40` | `Client\Shaders\terrain.vs` / `terrain.fs` | Same | `RenderTerrain`, `ZzzLodTerrain.cpp:3072,3102-3129` | Compatibility built-ins; `texture1`, `brightness`, `contrast` | Wraps existing terrain state; destructor; pass runs legacy if unavailable | Confirmed active when program links |
| Scene Glow | `CShaderScene` | `Client\Shaders\glow.vs` / `glow.fs` | Same | No direct caller found | VS is 330 core explicit input; FS is 330 compatibility; sampler/color inputs | No documented pass state; destructor; no caller means legacy unchanged | Loaded, unconsumed, link/runtime result uncertain |
| Scene Character | `CShaderScene` | `Client\Shaders\character.vs` / `character.fs` | Same | Main-world `RenderCharactersClient`, `ZzzScene.cpp:2648-2661` | Compatibility built-ins; `texture1` | Outer character-pass state; temporarily overridden by VBO Model; destructor; legacy if unavailable | Confirmed conditional active |
| Scene Colorize | `CShaderScene` | `Client\Shaders\colorize.vs` / `colorize.fs` | Same | No direct caller found | Compatibility built-ins plus palette/color uniforms and sampler | No documented pass state; destructor; legacy remains | Loaded, unconsumed |
| GL legacy shader | `CShaderGL`; `source\CShaderGL.cpp:25-68` | `Client\Shaders\shader.vs` / `shader.fs` | `gShaderGL->Init`, `Winmain.cpp:1141-1143`; `SHADER_VERSION_TEST` | `SetPerspective` is called by `Perspective`, `ZzzOpenglUtil.cpp:208-210`; old `RenderVertexBuffer` can use ID but has no caller | Compatibility built-ins; declared program lacks the projection/view/model uniforms queried by `SetPerspective` | `CShaderGL` destructor after context loss; fixed-function behavior survives | Compiled duplicate; draw use not proven; state side effect active |
| VBO Model | `CShaderGL`; `source\CShaderGL.cpp:78-125` | `Client\Data\Effect\VBO\Model.vs` / `Model.fs` | `CShaderGL::InitVBOShaders`; `SHADER_VERSION_TEST` | Eligible common `BMD::RenderMesh`, `ZzzBMD.cpp:1615-1639,3552-3643` | attr 0 position f3, 1 normal f3, 2 UV f2, 3 bone uint; `uView`, `uProj`, `u_Bones[600]`, body/light/UV/settings, `uTexture` | Depth/blend/cull inherited; draw restores prior program but not full VAO/buffer state; legacy on unavailable program | Confirmed active conditionally |
| VBO BlendMesh | `CShaderGL` | `...\VBO\BlendMesh.vs` / `.fs` | Same | Material switch exists in `RenderMeshVBO`, but current eligibility gate excludes it | Same layout and common uniforms | Same; legacy current path | Compiled but currently unreachable |
| VBO Metal | `CShaderGL` | `...\VBO\Metal.vs` / `.fs` | Same | Switch exists; excluded by current gate | Same | Same | Compiled but currently unreachable |
| VBO Oil | `CShaderGL` | `...\VBO\Oil.vs` / `.fs` | Same | Switch exists; excluded by current gate | Same | Same | Compiled but currently unreachable |
| VBO Chrome1 | `CShaderGL` | `...\VBO\Chrome1.vs` / `.fs` | Same | Switch at `ZzzBMD.cpp:3567-3587`; excluded by gate | Same; material UV settings | Same | Compiled but currently unreachable |
| VBO Chrome2 | `CShaderGL` | `...\VBO\Chrome2.vs` / `.fs` | Same | Same | Same | Same | Compiled but currently unreachable |
| VBO Chrome3 | `CShaderGL` | `...\VBO\Chrome3.vs` / `.fs` | Same | Same | Same | Same | Compiled but currently unreachable |
| VBO Chrome4 | `CShaderGL` | `...\VBO\Chrome4.vs` / `.fs` | Same | Same | Same | Same | Compiled but currently unreachable |
| VBO Chrome5 | `CShaderGL` | `...\VBO\Chrome5.vs` / `.fs` | Same | Same | Same | Same | Compiled but currently unreachable |
| VBO Chrome6 | `CShaderGL` | `...\VBO\Chrome6.vs` / `.fs` | Same | Same | Same | Same | Compiled but currently unreachable |
| VBO Chrome7 | `CShaderGL` | `...\VBO\Chrome7.vs` / `.fs` | Same | Same | Same | Same | Compiled but currently unreachable |

All eleven VBO fragment files are byte-identical in the inspected tree. The VBO vertex programs differ primarily in material/UV calculations. `Model.vs` declares its attributes and `u_Bones[600]` at `Client\Data\Effect\VBO\Model.vs:1-17` and performs skinning at `:23-44`. `Model.fs` multiplies sampled texture and interpolated color.

### Asset-only inventory

| Asset pair/tree | Code reference | Assessment |
|---|---|---|
| `Client\Data\Data\Shaders\shader.vs/.fs` | No path resolves here under normal `Client` CWD | Secondary/mirrored variant; not proven active |
| `Client\Data\Data\Shaders\terrain.vs/.fs` | Same | Byte-identical to root terrain pair; not proven active |
| `Client\Data\Data\Shaders\glow.vs/.fs` | Same | VS matches root copy; not proven active |
| `Client\Data\Data\Shaders\character.vs/.fs` | Same | Variant differs from the authoritative root copy; not proven active |
| `Client\Data\Data\Shaders\colorize.vs/.fs` | Same | Variant differs from the authoritative root copy; not proven active |
| `Client\Shaders\skin.vs/.fs` | No loader/caller found | UBO skinning proof-of-concept; `BoneBlock` contains 600 `vec4` (9,600 bytes), but ownership and binding are absent |
| `Client\Data\Effect\Shader\vertex_shader.glsl` / `fragment_shader.glsl` | No source-path reference found | Orphan or external-tool asset; not safe to remove without packaging/runtime trace |

The root `character` pair is byte-identical to the root `shader` pair. This is confirmed asset duplication, although separate logical programs can still be useful while passes diverge. Asset presence alone does not prove runtime use.

## 4. `CShaderScene` versus `CShaderGL`

| Concern | `CShaderScene` | `CShaderGL` | Consequence |
|---|---|---|---|
| Primary role | Broad pass wrapper for terrain/characters | Per-mesh VBO/material programs plus a duplicate legacy program | They overlap during character meshes |
| Initialization | Early, after GLEW call (`Winmain.cpp:566-569`) | Later, before model loads (`Winmain.cpp:1141-1143`) | Both compile root shaders independently |
| Compile/link behavior | Explicit compile and link status checks (`CShaderScene.cpp:61-106`) | `run_shader` logs compile failure but returns shader; link failure is handled (`CShaderGL.cpp:78-125,207-225`) | Scene owner is the stronger reusable base |
| Current-state cache | `m_CurrentProgram` enum (`CShaderScene.cpp:163-178`) | `m_CurrentVBOProgram` GLuint (`CShaderGL.cpp:130-139`) | Each cache can be invalidated by the other or direct GL calls |
| Unbind | `Unuse` binds program 0 (`CShaderScene.cpp:174-178`) | VBO caller manually restores `GL_CURRENT_PROGRAM`; other helpers bind 0 | Nested rendering is not uniformly safe |
| Uniform lookup | `glGetUniformLocation` on every setter call (`CShaderScene.cpp:180-199`) | Same (`CShaderGL.cpp:141-167,267-300`) | Cache after successful link; invalidate on relink/delete/context recreation |
| Cleanup | Explicit `Release`, but only destructor calls it | Destructor deletes all IDs; no explicit pre-context release | Both are destroyed too late |
| Fallback | Pass-level program availability | Per-mesh boolean fall-through | Both correctly preserve a legacy rendering route at a high level |

Reusable unchanged: `CShaderScene`'s file loading, compile/link diagnostics pattern, program-ID arrays, and caller-side availability checks. Adapt: binding, nested restoration, uniform caching, capability checks, and explicit shutdown. Confirmed duplicate: root `shader.vs/.fs` is compiled once by each owner, and the root `character` pair has identical contents. Keep distinct logical techniques even when their present source happens to match.

## 5. Render-path call graph

```text
WinMain / CreateOpenglWindow
  -> legacy pixel format + wglCreateContext
  -> glewInit (unchecked)
  -> CShaderScene::Init
  -> later CShaderGL::Init
       -> compile duplicate Shaders/shader pair
       -> compile 11 Effect/VBO pairs
  -> model load (BMD::Open2)
       -> CPU arrays
       -> BMD::CreateVertexBuffer per mesh

Main scene render
  -> RenderTerrain
       -> CShaderScene::Use(Terrain)
       -> fixed-function/immediate/client-array terrain, grass and water-related work
       -> CShaderScene::Unuse -> program 0
  -> CShaderScene::Use(Character)
       -> RenderCharactersClient
            -> player / monster / NPC RenderCharacter
                 -> BMD::Transform (CPU bone and always CPU vertex/normal transform)
                 -> BMD::RenderMesh
                      -> if plain lit texture + MAIN_SCENE + eligible + VAO:
                           BMD::RenderMeshVBO
                             -> save GL_CURRENT_PROGRAM
                             -> CShaderGL::UseVBO(Model)
                             -> upload matrices, bones and material state
                             -> glDrawElements
                             -> restore previous program
                      -> otherwise legacy CPU-expanded glDrawArrays path
       -> CShaderScene::Unuse -> program 0
  -> world objects / items / effects
       -> common BMD path can use VBO Model if its gate is satisfied
       -> sprites, joints, blurs and many effects remain legacy
  -> NewUI / LookAndFeel5 / overlays
       -> fixed-function 2D and client arrays
       -> 3D item/character previews can enter common BMD path
       -> ImGui OpenGL2 backend assumes compatibility-era state
```

Login and character-selection calls to `RenderCharactersClient` at `source\ZzzScene.cpp:1242,1400,1675,1743` are outside the main-world `CShaderScene` wrapper, and `IsVboSceneEnabled` restricts the VBO gate to `MAIN_SCENE` (`source\ZzzBMD.cpp:58-61`). They therefore remain legacy.

## 6. VBO/VAO/EBO ownership and lifecycle

`Mesh_t` owns GLuint-like fields for `VAO`, vertex, normal, texcoord, color, bone buffers, EBO, and element count in `source\ZzzBMD.h:135-185`; its constructor zeroes the relevant handles at `:175-182`.

| GPU object | Creator / upload | Owner and consumer | Update frequency | Deletion and context requirement | Reload behavior |
|---|---|---|---|---|---|
| Mesh VAO | `BMD::CreateVertexBuffer`, `ZzzBMD.cpp:3509-3514` | `Mesh_t`; bound by `BMD::RenderMeshVBO` at `:3622-3624` | Once per `Open2` load | No `glDeleteVertexArrays` found; deletion would require current owning context | `Open2` can call `Release` on reallocation (`:2788-2793`) and overwrite handles, leaking old VAO |
| Position VBO | Created/uploaded `ZzzBMD.cpp:3518-3521`, `GL_STATIC_DRAW` | attr 0 | Load only | No `glDeleteBuffers` found | Leaks on reallocation/reload |
| Normal VBO | `:3523-3527`, static | attr 1 | Load only | None | Same |
| Texcoord VBO | `:3529-3533`, static | attr 2 | Load only | None | Same |
| Bone-index VBO | `:3535-3539`, static | attr 3 via `glVertexAttribIPointer` | Load only | None | Same |
| EBO | `:3541-3544`, static | sequential `GL_UNSIGNED_SHORT`; `glDrawElements` at `:3623` | Load only | None | Same |
| `VBO_Colors` | Existing `Mesh_t` field; old path references it | No creator in the current `CreateVertexBuffer` | Not active | No deletion | Leftover from old `RenderVertexBuffer` path |
| Scene programs | `CShaderScene::Init` | Global manager | Once per context | `Release` deletes, but destructor is after context loss | No context recreation support |
| GL/VBO programs | `CShaderGL::Init` | Singleton | Once per context | Destructor deletes after context loss | No explicit recreation/release pair |

`BMD::Open2` reads mesh, bone, and action counts at `source\ZzzBMD.cpp:2870-2873`, allocates CPU mesh data, and calls `CreateVertexBuffer` for each mesh at `:2943-2945`. `CreateVertexBuffer` expands each triangle corner into position, normal, UV, and bone arrays (`:3449-3499`), triangulates quads (`:3458-3465`), validates referenced source indices (`:3471-3477`), builds sequential indices, rejects an empty stream or more than 65,000 elements (`:3503-3505`), then uploads static buffers.

`BMD::Release` at `source\ZzzBMD.cpp:2407-2476` frees textures and CPU allocations, including per-mesh arrays at `:2445-2448` and the mesh array at `:2463`, but does not delete any VAO, VBO, or EBO. No double-deletion path exists because deletion is absent. The defect is both a reload leak and a missing symmetric lifetime; shutdown leakage may be reclaimed by the OS, but that does not make reallocation safe.

### Vertex/index correctness

The current VAO uses four separate, tightly packed buffers rather than one interleaved structure. No BMD file structure or shared ABI layout is reused as a GPU declaration.

| Attribute | Source field | GL declaration | Stride / offset | Normalization | Shader mapping |
|---|---|---|---|---|---|
| 0 | `Vertex_t::Position[3]` copied to float stream | 3 × `GL_FLOAT` | 12 bytes / 0 | `GL_FALSE` | `layout(location=0) in vec3 aPosition` |
| 1 | `Normal_t::Normal[3]` copied to float stream | 3 × `GL_FLOAT` | 12 bytes / 0 | `GL_FALSE` | `layout(location=1) in vec3 aNormal` |
| 2 | `TexCoord_t::{U,V}` copied to float stream | 2 × `GL_FLOAT` | 8 bytes / 0 | `GL_FALSE` | `layout(location=2) in vec2 aTexCoord` |
| 3 | `Vertex_t::Node * 3` copied to unsigned-int stream | 1 × `GL_UNSIGNED_INT` through `glVertexAttribIPointer` | 4 bytes / 0 | Integer input; normalization not applicable | `layout(location=3) in uint aBone` |

The declarations are created at `source\ZzzBMD.cpp:3518-3536`; matching shader inputs are at `Client\Data\Effect\VBO\Model.vs:3-6` and the equivalent VBO material vertex shaders. The draw uses `GL_UNSIGNED_SHORT` indices at `source\ZzzBMD.cpp:3622-3623` and rejects more than 65,000 expanded elements at `:3503-3505`. Source vertex, normal, and texture indices are bounds-checked at `:3471-3477`. Quads are split into `(0,1,2)` and `(0,2,3)` at `:3458-3465`; other polygon counts are treated as one triangle, so malformed polygon values are not independently rejected.

Every triangle corner is expanded, and the EBO value is exactly the next stream position (`source\ZzzBMD.cpp:3498`). Therefore `glDrawArrays(GL_TRIANGLES, 0, count)` would be geometrically equivalent for the current expanded stream and would remove the two-byte sequential index per corner. It would not create vertex reuse. Actual indexed reuse requires deduplicating the complete corner tuple `(position index, normal index, texcoord index, bone index/material-relevant data)` within each mesh; deduplicating position indices alone would be incorrect because BMD corners can have different normals or UVs. Mesh/material separation is retained because each `Mesh_t` owns a separate VAO and draw.

## 7. CPU versus GPU skinning analysis

### CPU path

`BMD::Transform` begins at `source\ZzzBMD.cpp:331`. It selects a bone matrix and records GPU eligibility only when translation/body-scale conditions permit at `:359-366`. `NeedsCpuVertexTransform` evaluates downstream requirements at `:295-321` but currently returns true unconditionally at `:319`. Consequently, eligible draws still calculate every `VertexTransform` at `:386-437`, normal transforms at `:439-460`, and legacy per-normal lighting intensity at `:452-456`.

This CPU work must not simply be removed. `BMD::RenderBodyShadow` consumes CPU-transformed vertices at `source\ZzzBMD.cpp:2213` and legacy effects, hair, attachments, blur/trails, picking, and other consumers may depend on the same arrays. The per-vertex transform can only be skipped after each consumer has an equivalent path or after usage is proven absent for that draw.

### GPU path and bone transport

`Client\Data\Effect\VBO\Model.vs:8-17` declares matrices, lighting/material settings, and `uniform vec4 u_Bones[600]`. Three rows represent one 3x4 bone transform. Position and normal skinning occurs at `:23-44`. `BMD::RenderMeshVBO` obtains `NumBones`, clamps it to `MAX_BONES`, and uploads `boneCount * 3` `vec4` values at `source\ZzzBMD.cpp:3605-3612`. The per-object `BoneTransform` allocation is sized from the model's real bone count in `source\w_ObjectInfo.cpp:218`.

The local clamp correctly prevents reading 200 bones from a smaller allocation. It does not establish hardware capacity, and it does not safely handle a model whose valid vertex bone index is 200 or greater. `Open2` only has a debug assertion for the maximum around `source\ZzzBMD.cpp:2870-2873`; `CreateVertexBuffer` accepts a vertex node when it is below `NumBones`, not below 200 (`:3494-3496`). Such an asset could index beyond the shader array after an upload clamped to 200. Whether any current asset does so requires an asset/runtime inventory.

OpenGL 3.3 guarantees a minimum number of vertex-uniform components, not that this particular 600-`vec4` array plus all other uniforms will link. The current code performs none of the four relevant limit queries. A program link failure should produce program 0 and the legacy mesh fallback, but unsupported GL entry points or a shader that links on a lower limit with unexpected behavior are not centrally handled.

### Seven-class compatibility

The seven-class selection logic remains in the existing character subsystem (`source\ZzzCharacter.cpp:11836-11915`). The renderer gate is class-agnostic and uses common BMD data, so no class is intentionally added, removed, or redesigned. Static source proves a common route for bodies, equipment, wings, weapons, monsters, and effects; it does not prove that every current asset stays below 200 bones, uses matching vertex/normal bone nodes, or is visually identical. Those are runtime/asset verification questions.

### Future transport

The existing unused `Client\Shaders\skin.vs` UBO experiment demonstrates a 600-`vec4` block (9,600 bytes), which is below the OpenGL 3.3 minimum uniform-block size of 16 KiB. It is not ready to activate: block layout, binding ownership, supported entry points, per-draw update policy, and program integration are absent. A UBO is a plausible first transport when capabilities permit; a texture buffer is another option for larger or shared palettes. Both require a limit-tested legacy/uniform-array/CPU fallback. Bone transport should be changed only after measurements identify uniform uploads as material and visual parity tests exist.

## 8. State-conflict findings

The two managers do track separate ideas of the current program. `CShaderScene::Use/Unuse` maintains an enum and binds zero on unuse (`source\CShaderScene.cpp:163-178`). `CShaderGL::UseVBO` maintains a GLuint (`source\CShaderGL.cpp:130-139`). Neither cache is reconciled when the other manager or a direct `glUseProgram` changes actual state.

The local worktree contains a targeted fix in `BMD::RenderMeshVBO`: it queries `GL_CURRENT_PROGRAM` before the VBO bind (`source\ZzzBMD.cpp:3590-3597`) and restores that exact program at `:3638`. The local diff confirms this was added specifically to avoid forcing program 0 after a mesh nested inside the scene-character pass. It is valuable and must not be overwritten.

Every `glUseProgram` call found in the client source is accounted for below:

| Call site | Owner/symbol | Behavior and status |
|---|---|---|
| `source\CShaderScene.cpp:169` | `CShaderScene::Use` | Binds selected scene program; confirmed terrain/character use |
| `source\CShaderScene.cpp:176` | `CShaderScene::Unuse` | Forces program 0 rather than restoring a nested predecessor |
| `source\CShaderGL.cpp:136` | `CShaderGL::UseVBO` | Binds selected VBO program and updates only its own cache |
| `source\CShaderGL.cpp:173` | `CShaderGL::RenderShader` | Binds duplicate legacy program; no caller confirmed |
| `source\CShaderGL.cpp:232,251` | `CShaderGL::run_projection` | Binds legacy program, uploads state, then forces 0; old VBO helper can call it |
| `source\CShaderGL.cpp:259,262` | `CShaderGL::SetPerspective` | Binds legacy program, attempts projection upload, then forces 0; called from `Perspective` |
| `source\ZzzBMD.cpp:3638` | `BMD::RenderMeshVBO` | Restores exact saved predecessor; confirmed local fix |
| `source\ZzzBMD.cpp:3669,3673` | old `BMD::RenderVertexBuffer` | Binds legacy program then forces 0; no caller confirmed |

No other direct `glUseProgram` call was found in `source`. The ImGui OpenGL2 backend has commented-out program-state code rather than an active bind.

The fix is not a complete state model:

- After restoration, `CShaderGL::m_CurrentVBOProgram` still says the VBO program is current even though GL now has the previous scene program.
- `CShaderScene` can similarly believe a program is current after `CShaderGL::SetPerspective`, `run_projection`, or another direct `glUseProgram` binds and then forces zero (`source\CShaderGL.cpp:228-263`).
- Uniform setters query a location for their manager's program but do not verify that program is currently bound. The current VBO draw happens to call `UseVBO` immediately before its setters, so that path is ordered safely; the abstraction is not safe for other nested calls.
- The VBO draw binds VAO zero, disables attributes 0-3 on VAO zero, and binds array/element buffers zero at `source\ZzzBMD.cpp:3624-3636`. It does not restore a caller's previous VAO, buffers, or generic attribute state.
- It does not snapshot texture unit/binding, blend, depth, alpha test, cull, or fixed-function matrix state. Most of those are inherited rather than changed by `RenderMeshVBO`, but the function is not a complete isolated render scope.
- `BindTexture` maintains its own `CachTexture` and activates texture unit zero only when the logical texture changes (`source\ZzzOpenglUtil.cpp:311-330`). Direct texture binds can invalidate this cache, and active texture is not restored.
- The ImGui OpenGL2 backend explicitly does not save/restore a GLSL program (`source\imgui_impl_opengl2.cpp:21,143-146`) and renders client arrays at `:180-244`. It is safe only while called with program 0 or a compatible program; expanding shader coverage would make this a conflict.

No source-only investigation can prove that every blend/depth/cull/alpha/matrix transition is balanced across all render callers. The existing renderer intentionally uses global state and relies on render order. The consolidation must preserve that order and add narrow state guards at shader/VBO boundaries rather than attempting a global state rewrite.

## 9. Per-draw overhead findings

For each successful VBO mesh draw, `BMD::RenderMeshVBO` currently performs:

- one `glGetIntegerv(GL_CURRENT_PROGRAM)` at `source\ZzzBMD.cpp:3594`;
- two `glGetFloatv` calls for model-view and projection at `:3598-3601`;
- program binding through `CShaderGL::UseVBO`;
- up to ten uniform-location queries and corresponding uploads for view, projection, bones, body light, light position, mesh UV, two setting vectors, light enable, and sampler at `:3602-3620`, because each `vboSet*` calls `glGetUniformLocation` in `source\CShaderGL.cpp:141-167`;
- one VAO bind, one `glDrawElements`, and explicit VAO/buffer cleanup at `source\ZzzBMD.cpp:3622-3636`.

Confirmed/rejected claims:

| Candidate cost | Finding |
|---|---|
| `glGetUniformLocation` per draw | Confirmed, up to ten lookups per successful mesh draw |
| `glGetAttribLocation` per draw | Rejected; layouts are explicit and no call was found |
| `glGetFloatv` per draw | Confirmed twice per mesh |
| Repeated view/projection upload | Confirmed per mesh, even when unchanged during a pass |
| Repeated model query | No separate model matrix query; fixed-function model-view is queried |
| Shader compile/link per draw | Rejected; compilation occurs at initialization |
| Per-frame VBO upload | Rejected for the active path; buffers use `GL_STATIC_DRAW` at model load |
| Per-draw allocation | Rejected in `RenderMeshVBO`; temporary vectors are load-time in `CreateVertexBuffer` |
| Redundant buffer binds | The VAO and explicit zeroing are repeated per mesh; impact requires counters/timing |
| Redundant texture binds | Existing `CachTexture` avoids some repeats, but its correctness under direct binds needs instrumentation |
| Sequential EBO | Confirmed; indices are assigned in stream order at `ZzzBMD.cpp:3498` |
| Duplicated CPU/GPU skinning | Confirmed for VBO-eligible draws because CPU transform remains unconditional |

Caching uniform locations after a successful link is safe if the cache belongs to the program record and is invalidated before deletion, after relink, and on context recreation. Stable per-pass matrices and sampler bindings can then be uploaded on a verified program-generation/pass change. Bone palettes and per-object lighting remain object-dependent. Do not remove any call solely because it looks redundant; first count calls and measure the current CPU/GPU cost in reproducible scenes.

The existing local `source\RenderProfiler.h:3-25` and `source\RenderProfiler.cpp:53-144` already time movement, render, physics, swap, sleep, BMD transform, VBO, and legacy mesh work, and count CPU-transform requests/skips. It should be extended rather than replaced. Its current two-second average is not a median/tail baseline, and its scope destructor queries performance frequency (`RenderProfiler.cpp:127-144`), so profiling overhead itself must be measured or the frequency cached.

## 10. Renderer coverage matrix

| Domain | Current active path | Shader/VBO coverage | Fallback / missing coverage | Evidence |
|---|---|---|---|---|
| Terrain base | Immediate/fixed-function geometry inside terrain pass | `CShaderScene::Terrain` wraps it | Full legacy when shader unavailable; no terrain VBO | `ZzzLodTerrain.cpp:1626-1721,3072-3129` |
| Grass | CPU-updated client arrays | Inside Terrain program wrapper | Legacy client arrays; dynamic wind/color prevents naive static VBO | `ZzzLodTerrain.cpp:1850-1973` |
| Water | Terrain UV animation and separate legacy water terrain | Partly within Terrain wrapper; `CSWaterTerrain` remains separate | Legacy | `ZzzLodTerrain.cpp:1826-1838,3074-3089`; `source\CSWaterTerrain.cpp` |
| Player bodies, seven classes | Common BMD CPU transform; eligible plain-lit meshes use GPU Model | Character outer program plus nested VBO Model | Special/unlit materials legacy; login/select legacy | `ZzzScene.cpp:2648-2661`; `ZzzCharacter.cpp:11836-11915`; `ZzzBMD.cpp:1615-1641` |
| Equipment, wings, weapons | Common BMD parts path | Same eligibility as above | Chrome/metal/oil/wave/bright/effect materials currently legacy | `ZzzBMD.cpp:1615-1753,3552-3643` |
| Monsters and NPCs | `RenderCharactersClient` / `RenderCharacter`, common BMD | Same character/VBO overlap in main world | Noneligible meshes legacy | `source\ZzzCharacter.cpp:8467,11121`; `ZzzScene.cpp:2648-2661` |
| World objects | Common BMD and object-specific legacy effects | Eligible Model VBO possible, without Character outer program | Special render flags/effects legacy | `source\ZzzObject.cpp`; common `BMD::RenderMesh` gate |
| Ground items | Common BMD plus item effects | Eligible Model VBO possible in main world | Effects/material variants legacy | Common BMD gate; item render callers |
| Inventory/character previews | Fixed-function UI framing plus common 3D BMD render | Can reach Model VBO during `MAIN_SCENE` if eligible | Login/select previews remain legacy; 2D UI legacy | `source\ZzzInventory.cpp:4393,4867-4869`; `source\NewUIMyInventory.cpp:1069,1143`; `source\NewUIInventoryCtrl.cpp:154,1788` |
| Model effects | Common BMD for model-based effects plus legacy special logic | Eligible plain-lit model mesh may use Model VBO | Most special materials legacy | Common BMD gate; `source\ZzzEffect.cpp:18633` |
| Particles, joints, sprites | Immediate/client arrays and fixed-function transforms | No confirmed shader-owner coverage beyond surrounding pass | Legacy only | `source\ZzzOpenglUtil.cpp:1038`; effect/joint renderers |
| Blur and trails | CPU-generated legacy arrays | None | Legacy only | `source\ZzzEffectBlurSpark.cpp:134` and blur callers |
| Dynamic lighting | CPU terrain/normal colors; Model VS lighting for eligible VBO meshes | Split implementation | Legacy is visual reference | `ZzzLodTerrain.cpp:2830+`; `ZzzBMD.cpp:439-460`; `Model.vs:52-65` |
| Shadows | CPU `VertexTransform` legacy shadow path | No VBO shadow program | Legacy only | `ZzzBMD.cpp:2213` |
| NewUI / LookAndFeel5 | Fixed-function 2D, client arrays, bitmap helpers | No general shader migration | Must remain legacy initially | `source\ZzzInterface.cpp:8067`; `ZzzScene.cpp:2760` |
| ImGui | OpenGL2 backend/client arrays | No safe nested GLSL state owner | Must be invoked with compatible state or later migrate backend | `imgui_impl_opengl2.cpp:21,180-244`; `CGMEffectHandle.cpp:134-175`; `CGMRenderGroupMesh.cpp:687-704` |
| Login/character selection | Legacy character rendering | No Scene Character wrapper and VBO gate disabled | Legacy is required fallback/reference | `ZzzScene.cpp:1242,1400,1675,1743`; `ZzzBMD.cpp:58-61` |

No standalone “VBO without shaders” active draw was found. The old `BMD::RenderVertexBuffer` at `source\ZzzBMD.cpp:3645-3690` references buffer fields and the legacy `CShaderGL` ID, but has no confirmed caller. It must be treated as uncertain, not deleted.

## 11. Terrain static/dynamic safety analysis

Terrain is not one update-frequency class. The current code mixes stable map data with frame- and event-dependent values:

- Stable candidates: height samples, tile topology, base UVs, terrain texture mapping, and a stable index topology for a loaded map.
- Grass wind: `TerrainGrassWind` and related globals are declared at `source\ZzzLodTerrain.cpp:48-58`; wind values are computed at `:2875-2890`, and grass vertices are displaced per draw at `:1938-1946`.
- Grass/dynamic colors: `PrimaryTerrainLight` is copied into render colors at `source\ZzzLodTerrain.cpp:1949-1958`; dynamic light initialization and accumulation begins around `:2830` and `:2911`.
- Water: `WaterMove` is updated by map/time logic at `source\ZzzLodTerrain.cpp:64,3074-3089` and applied to UVs around `:1826-1838`.
- Weather/map effects: remain map-specific across terrain, object, scene, and effect subsystems and cannot be collapsed into a single static terrain upload.

A safe eventual split is:

1. Upload stable position/topology/base UV/material-region data once per map, preserving the current tile/face ordering and map conditions.
2. Represent water offset/time and grass-wind phase/amplitude as uniforms or a small dynamic field, after 1:1 visual proof.
3. Keep terrain light/color as an intentionally updated texture/stream or shader-readable field; do not bake `PrimaryTerrainLight` into an immutable buffer.
4. Keep weather, blend, transparency, map object, and special-effect decisions in their existing per-map owners until separately migrated.
5. Retain the legacy terrain path as fallback and comparison throughout.

Freezing the complete current terrain into a static VBO would freeze grass displacement, water animation, and dynamic lighting or force broad per-frame rewrites. It is specifically not recommended.

## 12. Confirmed defects with severity P0-P3

No P0 data-loss, security, or always-fatal renderer defect was proven statically.

| Severity | Confirmed defect | Exact evidence | Impact |
|---|---|---|---|
| P1 | GL context is destroyed before renderer-owned GL resources are released | `WINHANDLE.cpp:618-628`; `Winmain.cpp:207-231,410-461,597-605,1273-1275`; shader destructors `CShaderScene.cpp:32-35`, `CShaderGL.cpp:15-23` | GL deletion is attempted without the owning current context; behavior is invalid/driver-dependent |
| P1 | Mesh VAO/VBO/EBO resources have no deletion path | Creation `ZzzBMD.cpp:3509-3544`; `BMD::Release` `:2407-2476`; no `glDeleteBuffers`/`glDeleteVertexArrays` in the client tree | Reallocation/model reload leaks GPU objects; symmetric ownership is absent |
| P1 | VBO code has no validated loader/entry-point/capability gate | Ignored `glewInit` result `Winmain.cpp:564`; immediate VAO/integer-attribute use `ZzzBMD.cpp:3509-3539`; no relevant limit queries | Unsupported/incompletely initialized contexts can fail before the program-level legacy fallback |
| P2 | Model VBO lighting deliberately darkens RGB by 15% relative to the legacy formula | `Client\Data\Effect\VBO\Model.vs:52-65`, especially the `0.85` multiplier | Eligible meshes cannot be 1:1 visual equivalents unless the legacy path has the same factor elsewhere; runtime extent still needs screenshots |
| P2 | Manager caches can disagree with `GL_CURRENT_PROGRAM` | caches and unbinds `CShaderScene.cpp:163-178`, `CShaderGL.cpp:130-167`; local manual restore `ZzzBMD.cpp:3590-3638` | Future/nested setters can upload to the wrong actual program or generate GL errors |
| P2 | Bone counts/indices beyond 200 have no explicit safe release-build fallback | shader `u_Bones[600]`; upload clamp `ZzzBMD.cpp:3605-3612`; asset index acceptance `:3494-3496`; debug-only bound near `:2870-2873` | A qualifying future/current asset can index beyond the uploaded shader palette |
| P2 | GPU skinning does not eliminate CPU per-vertex skinning | unconditional `NeedsCpuVertexTransform` return `ZzzBMD.cpp:295-321`; CPU loops `:386-460`; GPU draw `:3552-3643` | Confirmed duplicated work for eligible draws; performance impact requires measurement |
| P2 | VBO draw does not restore prior VAO/buffer/attribute state | `ZzzBMD.cpp:3622-3636` | A nested caller with its own VAO/buffers can be invalidated; current render order may mask it |
| P2 | WGL extension-string function is called without validating its pointer | `ZzzOpenglUtil.cpp:2217-2222` | Potential null call on unsupported WGL implementations |
| P2 | Existing shader fallback path does not reach the actual `Client\Data\Data\Shaders` mirror under normal CWD | lookup `CShaderScene.cpp:37-58`; runtime tree layout | Deployment ambiguity; fallback assets may never be used |
| P3 | Dormant multisample helper obtains a DC without a matched release inside the helper | `ZzzOpenglUtil.cpp:813-902` | Resource issue only if the currently dormant path is activated as-is |
| P3 | Eleven VBO programs and two copies of the root default shader are compiled although only Model has a confirmed mesh consumer | `CShaderGL.cpp:25-68,117-125`; gate `ZzzBMD.cpp:1615-1639` | Startup/maintenance overhead; size and timing require measurement |

## 13. Unverified hypotheses

These are not confirmed defects and must not be represented as such:

- Normal-bone mismatch: CPU normal transformation uses `Normal_t::Node` (`source\ZzzBMD.cpp:447-449`), while `CreateVertexBuffer` stores the corner's `Vertex_t::Node` and the shader uses the same bone for position and normal (`:3494-3496`; `Model.vs:23-44`). A visual mismatch exists only for assets where those nodes differ. Asset scanning or runtime capture is required.
- Current assets over 200 bones: source permits a problematic release-build case, but no current BMD inventory was decoded to prove that one of the seven classes, equipment, wings, monsters, or effects triggers it.
- Uniform lookups, matrix readbacks, sequential EBOs, and duplicated CPU skinning are likely CPU costs, but their percentage of frame time and visible FPS effect are unmeasured.
- The root glow vertex/core plus fragment/compatibility version combination may fail or link differently by driver. No runtime compile log was available.
- VBO Model may render every eligible class/equipment asset identically except the confirmed 0.85 light factor; normal-node differences, bone limits, alpha ordering, and driver shader behavior remain runtime questions.
- The `Client\Data\Data\Shaders` and `Client\Data\Effect\Shader` trees may be consumed by an external launcher, packer, tool, alternate CWD, or unpublished workflow.
- `RenderVertexBuffer` and `VBO_Colors` appear inactive, but indirect invocation, local experimental use, or future work has not been ruled out.
- Full state leakage outside the traced program/VAO/buffer boundaries requires a GL state capture across representative render passes.

## 14. Proposed unified architecture

Do not add a third public shader manager. Make the stronger existing `CShaderScene` compile/link/lifetime machinery the authoritative internal program registry, while preserving both existing public interfaces as adapters during migration.

### Target ownership

```text
CShaderScene (existing authoritative owner, evolved internally)
  ProgramRecord[]
    id, logical name, source paths, link generation
    cached uniform/attribute locations
    capability requirements and diagnostics
  BindProgram / ScopedProgramBind
    query/verify actual GL_CURRENT_PROGRAM
    bind target
    restore exact previous program on scope exit
  ReleaseAll (explicit, while context current)

CShaderScene public calls ---------------> registry adapter
CShaderGL public calls ------------------> registry adapter
BMD / terrain / character callers ------> unchanged initially
legacy fixed-function path --------------> retained fallback and reference
```

Rules:

1. Actual `GL_CURRENT_PROGRAM` is the verifiable source of truth. A cache may avoid redundant calls only when debug verification and all mutations pass through the owner; it must never contradict GL silently.
2. A `ScopedProgramBind` stores and restores the exact prior program, supporting character-pass/VBO nesting. Existing paired `Use/Unuse` APIs can use an internal stack or adapter until call sites are converted safely.
3. Uniform/attribute locations are cached only after successful link and belong to a program generation. Delete/relink/context recreation invalidates the whole record before a new ID is exposed.
4. Compile/link logs, file paths, GLSL/version requirements, and capability failures are centralized.
5. Program release, BMD buffer release, texture release, and ImGui renderer release run explicitly while the context is current. Only then may WGL unbind/delete the context.
6. Scene techniques and VBO material techniques remain separate program records. Do not create one mega-shader or merge terrain, UI, character, and material state merely because one owner stores them.
7. Preserve shader assets, call order, fixed-function matrices, blend/alpha/depth/cull behavior, and legacy fall-through until each path has screenshot/counter/regression evidence.

Reuse unchanged initially: file readers, the successful `CShaderScene` compile/link checks, shader enums/public method signatures, current caller gates, the local `GL_CURRENT_PROGRAM` restore, BMD CPU data, and legacy draws. Adapt: program storage, bind/unbind, uniform setters, capability validation, and explicit teardown. Later, move mesh buffer deletion into a BMD-owned helper using existing handle fields; do not change BMD serialization or shared ABI layouts.

## 15. Staged OpenGL 3.3 migration roadmap

The proposed order moves pixel-format cleanup before the final 3.3 context request because a final window's pixel format must be selected before its context is created. Each phase ends in a releasable Release/x86 fallback point.

| Phase | Prerequisites and exact scope | Boundary / expected benefit | Risks and fallback | Validation / rollback / type |
|---|---|---|---|---|
| 0. Diagnostics and baseline | `Winmain.cpp`, `ErrorReport.cpp`, existing `RenderProfiler.*`; no render selection changes | Record actual PFD/context/GLSL/caps/limits and renderer counts; establish reproducible evidence | Logging/profiler overhead; compile-time/runtime gate and legacy behavior unchanged | Release/x86 build/run matrix; disable gate to rollback; correctness/measurement |
| 1. Stabilize compatibility lifetime | `WINHANDLE.cpp`, `Winmain.cpp`, ImGui shutdown, `CShaderScene`, `CShaderGL`, `BMD::Release` helper | Release GL resources while context current; add entry-point/capability fallback | Order-sensitive legacy globals; first add ordering, then deletion, with handle zeroing | Reconnect/map reload/shutdown loops and live-resource counts; rollback at ordered-shutdown boundary; correctness |
| 2. One shader owner | Internals of `CShaderScene`/`CShaderGL`; retain headers/call sites | One registry and scoped binding; no shader/material change | Nested state regressions; keep adapters and legacy program 0 fallback | Assert actual program at boundaries, all scenes/screenshots; rollback adapters; architecture/correctness |
| 3. Cache locations and stable state | Program records and uniform setters; profiler counters | Remove repeated location queries and measured redundant binds/uploads | Stale locations after relink/context recreation | Generation-based invalidation; before/after counters and GL errors; rollback per cache; measured performance |
| 4. Pixel-format selector cleanup | `CreateOpenglWindow`, dormant `InitGLMultisample`; temporary bootstrap window/context | Request preferred RGBA 24+8, depth 24, stencil 8 and optional samples, log actual; retain legacy PFD ladder | WGL bootstrap/window lifecycle and old-driver compatibility | Test preferred and forced-fallback paths; rollback to current selector; compatibility/correctness |
| 5. Request OpenGL 3.3 Compatibility | WGL extensions/capability record after Phase 4 | Explicit 3.3 Compatibility context where available; no performance claim | Old GPUs/remote desktop/context creation failures | Retry current legacy `wglCreateContext`; verify identical renderer matrix; compatibility |
| 6. Modernize BMD/character correctness | `ZzzBMD.cpp/.h`, VBO Model shader only after visual baselines | Make current eligible Model path 1:1, validate bone/normal bounds and buffer ownership | Seven-class/equipment/monster visual regressions | Per-mesh fallback, screenshots, animation/shadow/effect tests; rollback VBO feature gate; correctness/architecture |
| 7. Safe bone transport | Capability registry, BMD palette upload, new/confirmed shader variants | UBO or TBO only where supported; reduce measured upload cost; retain uniform-array/CPU path | Binding limits/layout/driver variation | Force each transport/fallback, >limit synthetic/known asset, identical poses; rollback transport selector; compatibility/measured performance |
| 8. Effects, sprites, previews | Effect, sprite, blur, inventory preview owners, one domain at a time | Broaden explicit buffers/shaders without disturbing ordering | Alpha/blend/order and CPU-transform consumers | Domain feature flag and golden scenes; rollback each domain independently; architecture/measured performance |
| 9. Terrain static/dynamic split | `ZzzLodTerrain.cpp`, terrain shaders, map-load lifecycle | Static stable topology/data; dynamic wind/water/light fields remain dynamic | Map-specific effects, seams, color/UV drift | Map matrix including grass/water/light/weather; full legacy terrain toggle; architecture/measured performance |
| 10. UI/LookAndFeel5 and overlays | NewUI, LookAndFeel5, font/bitmap helpers, replace ImGui GL2 backend only when ready | Explicit UI state and safe shader nesting, resolution-aware behavior unchanged | Scissor, text, 3D previews, aspect/resolution regressions | Resolution/aspect/input matrix; preserve GL2/UI fallback until complete; compatibility/architecture |
| 11. Deprecated API audit | Whole renderer after migrated-domain proof | Inventory remaining `glBegin`, matrix stack, client arrays, alpha test, quads | Removing a call still used by rare maps/events/tools | Runtime coverage + static caller map; no deletion without proof; architecture |
| 12. OpenGL 3.3 Core, conditional | Zero required compatibility calls, explicit replacements for UI/terrain/effects/tools, complete asset tests | Optional core context with clean explicit state | Highest compatibility risk; no inherent FPS guarantee | Keep Compatibility build/runtime option and revert context request; compatibility only unless measured |

Phase 6 and later must preserve the seven supported classes exactly. Later-season readiness means bounds/capability-driven data paths and graceful fallback, not enabling new classes in this work.

## 16. Measurement and regression-test plan

### Instrumentation

Extend the existing `RenderProfiler` behind a low-overhead compile/runtime switch. Cache the performance-counter frequency and collect per-frame samples rather than only two-second averages.

Record:

- frame time median, p95, p99, worst non-loading frame, and variance across runs;
- CPU total render time and named pass times;
- GPU elapsed time with timer queries only when supported, delayed/read without stalling;
- draw calls; rendered meshes, vertices, and triangles;
- program switches and attempted redundant program binds;
- VAO/array/EBO binds; texture-unit/binding changes;
- uniform-location queries and uniform uploads, split by matrix/bone/material;
- CPU vertex/normal skinning counts and time; GPU-skinned mesh/bone counts;
- created/live/deleted program, shader, buffer, VAO, and relevant texture counts;
- startup, model-load, and world-entry upload time;
- process RAM and, only where a trustworthy extension/OS counter exists, GPU memory;
- GLEW/capability failures and gated `glGetError`/debug callback messages outside hot production paths.

### Reproducible scene set

1. Login idle.
2. Character selection with each of the seven classes represented across controlled runs.
3. Normal map idle/walk with a fixed camera.
4. Dense player/monster/NPC scene.
5. Inventory character and item preview with equipment/wings/weapons.
6. Effects-heavy combat with blur/trails/particles.
7. Terrain scene containing grass, water, dynamic light, and weather/map effects.
8. Map transition and model/world reload.
9. Disconnect/reconnect.
10. Normal shutdown, repeated launch/shutdown, and forced shader/capability fallback.

Control Release/x86 executable, exact client asset manifest, configuration, resolution/aspect mode, VSync/frame cap, camera, character/equipment state, scene duration, foreground state, driver settings, and hardware. Warm up each scene, collect a fixed interval, repeat at least five times, and report per-run plus aggregate distributions. Use identical builds/assets/scenes/hardware for before/after comparisons. Capture reference screenshots and, where practical, tolerant image diffs for poses, lighting, alpha ordering, terrain, UI, and previews. A clean build is not runtime proof, and no FPS claim is valid without these measurements.

## 17. Exact recommended first implementation slice

Safest first slice: diagnostics and baseline only, with no render-path selection or visual change.

1. In `CreateOpenglWindow`, capture and log the selected pixel-format index and actual `DescribePixelFormat` fields: color, red/green/blue/alpha, depth, stencil, double buffer, stereo, and sample buffers/samples when queryable.
2. Check and log the `glewInit` result; record OpenGL vendor/renderer/version, GLSL version, context profile/flags when available, required VAO/integer-attribute/shader entry points, maximum vertex attributes, the two vertex-uniform limits, UBO maximum size, and texture-buffer maximum size.
3. Extend the existing local `RenderProfiler` with gated counters for draw/program/buffer/texture/uniform/resource activity and per-frame samples. Do not replace it and do not turn instrumentation on in normal production by default.
4. Run the full baseline matrix before altering shader ownership, pixel format, skinning, or terrain.

The next narrowly bounded consolidation slice, after baseline evidence, is one internal program registry/binding scope and cached uniform locations. Keep `CShaderScene` and `CShaderGL` public call sites as adapters; preserve all shader files, enums, material selection, the local prior-program restore, and the legacy fallback. Do not combine this with context creation, bone transport, terrain, or UI changes.

## 18. Resolved shader/VBO experiment classification

This classification supersedes the earlier blanket retention list after Phases 1-9 established one program owner, audited repository call sites, isolated the production VBO gate, and inspected every uploaded GLSL asset.

### Production-owned programs

- `Shaders\\terrain.vs/.fs`: active terrain scene program.
- `Shaders\\character.vs/.fs`: active character scene program.
- `Data\\Effect\\VBO\\Model.vs/.fs`: the only BMD VBO program reachable through the current eligibility gate.
- The fixed-function/client-array BMD path remains the authoritative fallback and visual reference.

### Redundant or unowned experiments

- `Shaders\\shader.vs/.fs` duplicates the Character technique and its legacy adapter has no caller.
- Glow and Colorize have no render-pass owner, caller, state contract, resource owner, or runtime selection path. Glow is a visual feature prototype, not a performance path. Colorize is a potentially useful palette-recolouring idea but uses compatibility inputs and has no palette lifecycle.
- `Data\\Effect\\shader\\*.glsl` is an unowned screen-space text/shadow prototype. It is not connected to NewUI/LookAndFeel5 layout, resolution conversion, font texture ownership, batching, or correct alpha-composition policy.
- `BMD::RenderVertexBuffer`, the legacy `shader_id` adapter, and `VBO_Colors` form an older dynamic-upload experiment with no verified caller. They duplicate the active static bind-pose/GPU-skinning path.

These paths must not remain in the production runtime merely as dormant executable architecture. Any future feature must be reintroduced through the authoritative renderer owner with an explicit pass, lifecycle, fallback, and validation contract.

### Valuable unfinished design retained as requirements, not dormant programs

`Shaders\\skin.vs/.fs` contains a useful UBO bone-palette concept, but is not a drop-in replacement:

- its attribute layout differs from the active Model VBO layout;
- it represents the bone index as float rather than the active integer attribute;
- it expects per-frame CPU vertex-light colour uploads, while the active Model shader computes lighting from the transformed normal;
- its body transform contract differs from the currently isolated eligibility rules.

The next bone-transport phase should port only the validated std140 3x4 palette concept into the active Model technique, retain capability/size checks and a uniform-array/legacy fallback, and avoid activating the obsolete POC layout.

The inactive BlendMesh/Metal/Oil/Chrome1-7 shaders duplicate the complete skinning and lighting body across separate programs. Their intended legacy UV equations were inspected and are retained here as the parity specification for a future unified BMD material technique:

- Chrome1: `u = N.z*0.5 + Wave`, `v = N.y*0.5 + Wave*2`.
- Chrome2: `u = (N.z+N.x)*0.8 + Wave2*2`, `v = (N.y+N.x) + Wave2*3`.
- Chrome3: `u = dot(N, LightVector)`, `v = 1-u`.
- Chrome4: Chrome3-style animated `L`, then `u += N.y*0.5 + L.y*3`, `v -= N.z*0.5 + Wave*3`, plus mesh UV offset.
- Chrome5: animated `L`, then `u += N.y*3 + L.y*5`, `v -= N.z*2.5 + Wave`.
- Chrome6: both coordinates `(N.z+N.x)*0.8 + Wave2*2`.
- Chrome7: both coordinates `(N.z+N.x)*0.8 + WorldTime*0.00006`.
- Metal: `u = N.z*0.5 + 0.2`, `v = N.y*0.5 + 0.5`.
- Oil: `uv = N.xy * sourceUV + meshUVOffset`.
- BlendMesh/stream: `uv = sourceUV + meshUVOffset`; it remains excluded while wave/stream semantics are not proven.
- Chrome8 has no experimental VBO counterpart and remains legacy-only.

A future material expansion should share one BMD skinning/bone transport implementation and select a verified material mode (or centrally generated variants), rather than restoring eleven independently owned shader files. Each material remains legacy until screenshot/state/blend/alpha parity is proven.

### Performance interpretation

The current VBO path still executes CPU vertex/normal transformation because `NeedsCpuVertexTransform` deliberately returns true for unresolved shadow, side-hair, attachment, effect, and preview consumers. Consequently GPU skinning currently duplicates part of the CPU work. Shader cleanup reduces startup compilation, file I/O, program/resource ownership, and maintenance risk; it is not proof of higher FPS.

The largest plausible CPU improvement comes only after consumer-specific proof allows CPU transforms to be skipped for an explicitly safe draw contract. UBO bone transport primarily improves portability and may reduce upload/driver overhead; both require the existing profiler and identical runtime scenes before any performance claim.


## 19. Questions that require runtime evidence

1. What color/alpha/depth/stencil/sample values does `DescribePixelFormat` report on each supported machine and remote/virtual environment?
2. What OpenGL and GLSL versions/profile does legacy `wglCreateContext` actually yield, and does `glewInit` succeed everywhere?
3. Do all five scene programs and all eleven VBO programs compile/link on supported drivers? What are their complete logs?
4. Which real meshes reach the Model VBO gate per frame, broken down by player class, body/equipment/wing/weapon, monster, NPC, object, item, effect, and preview?
5. Does any shipped/runtime BMD exceed 200 bones or contain a vertex/normal node mismatch on a rendered corner?
6. Does the Model shader's 0.85 multiplier visibly darken current eligible meshes, and are there other pixel differences in light, fog, alpha, or material ordering?
7. How much CPU time is spent in duplicate CPU transformation, uniform lookup/upload, matrix readback, buffer binding, and the profiler itself?
8. Are there reload paths that exercise `Open2(..., bReAlloc)` during normal map transitions, reconnect, character changes, or tools, and how quickly do live GPU-object counts grow?
9. Does any nested preview, ImGui overlay, or special render pass encounter a nonzero GLSL program unexpectedly or rely on a prior VAO/buffer state?
10. Which shaders/assets in `Client\Data\Data\Shaders`, `Client\Shaders\skin.*`, and `Client\Data\Effect\Shader` are consumed by launchers, packers, tools, or alternate working directories?
11. Does an explicit 3.3 Compatibility context preserve every map, event, UI, font, effect, and capture/tool integration on the supported hardware set?
12. Which stable terrain data actually benefits from GPU residency after draw count, upload cost, and dynamic light/wind/water behavior are measured?
