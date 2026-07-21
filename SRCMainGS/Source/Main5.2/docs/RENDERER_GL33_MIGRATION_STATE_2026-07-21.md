# Renderer OpenGL 3.3 Migration — State & Continuation Handoff

Date: 2026-07-21
Working branch: `claude/session-d70nxt`  (PR #28)

Purpose: single entry point for resuming the GL 3.3 migration in a new session.
Read this first, then `RENDERER_DEPRECATED_GL_AUDIT_2026-07-21.md` (the worklist),
and invoke the `mu-main52-renderer` skill before touching renderer code.

## 1. How to resume in a new session

Give the new session this context:

1. Repo `muserverge/src-mu-52`, branch `claude/session-d70nxt` (already pushed).
   Make sure the session starts from the latest branch tip, and commit + push any
   local edits first so nothing diverges. Keep one driver on the branch at a time.
2. Point it at this file and `docs/RENDERER_DEPRECATED_GL_AUDIT_2026-07-21.md`.
3. State the verification reality: this is a Win32 / MSBuild / OpenGL client that
   **cannot be built or run in the session container**. Build + in-game checks
   happen on the user's Windows machine (`SRCMainGS/Source/Main5.2/Main.sln`,
   `Win32`). The session writes small, additive slices; the user builds/runs and
   confirms; then the next slice.
4. The next concrete step is **Phase 13.2** (see §4).

## 2. What is done and verified

| Phase | Change | Build | Runtime / test |
|---|---|---|---|
| 12 | ImGui backend `opengl2` → `opengl3` | ✅ MSVC Win32 | ✅ ImGui panels render correctly (RenderMesh tools, effect-handle UI) |
| audit | `docs/RENDERER_DEPRECATED_GL_AUDIT_2026-07-21.md` | n/a (doc) | n/a |
| 13.1 | `RenderMatrix` CPU matrix backbone | ✅ MSVC Win32 | ✅ host unit test, 22 checks, `-Wall -Wextra` clean |
| 13.2 | CPU projection mirror `g_ProjectionMatrix` in `gluPerspective2` | ✅ Release Win32 | ✅ scene identical (default + `-gl33compat`) |
| 13.3 | CPU view mirror `g_ViewMatrix` in `BeginOpengl` | ✅ Release Win32 | ✅ scene identical (default + `-gl33compat`) |
| 13.4 | Feed `uProj` from `g_ProjectionMatrix` in `RenderMeshVBO` (first consumer) | ✅ Release Win32 | ✅ VBO world objects + `-vbotranslate` identical (proves CPU projection 1:1) |
| 13.5.1 | CPU MODELVIEW stack owner `g_ModelViewStack`; mirror world camera (`BeginOpengl`/`EndOpengl`); `g_ViewMatrix` derived from Top | ✅ Release x86 (0 warn/err) | invisible slice, no consumer — pending in-game scene-identical check |
| 14.1 | Authored Core terrain shaders `terrain_core.vs/.fs` (inert, not wired) | n/a (GLSL assets, no C++) | n/a — not referenced by `CShaderScene`, zero behavior change |
| 14.2 | `CShaderScene::Init` compile-link **probe** of `terrain_core` (logged, deleted, not bound); dropped non-portable uniform initializers from `terrain_core.fs` | ✅ Release x86 (0 warn/err) | pending: check log for `Core terrain probe ... compiled+linked OK`; scene must be identical |

All code phases are behavior-preserving so far: Phase 12 only swaps the UI
overlay backend; Phase 13.1 is purely additive (no existing call site changed);
Phase 13.2 / 13.3 are additive (build CPU copies of the projection and view
matrices; the fixed-function `gluPerspective` and MODELVIEW stay authoritative,
no draw changed).

Verification note: the user builds/runs the **Release** client only — Debug
builds are not usable. Do not gate slice verification behind `_DEBUG`; verify by
scene identity in the Release run.

## 3. Repo-specific gotchas a new session MUST know

- **Source encoding is fragile.** Several files are ISO-8859 / CP949 (legacy
  Korean/Spanish comments), not UTF-8. Editors/tools that rewrite a file as UTF-8
  corrupt those bytes and produce spurious comment-only diffs (this already bit
  `CGMEffectHandle.cpp`; see PR #21 for a prior instance). Before editing such a
  file, check `file <path>`; if it is ISO-8859, apply changes with byte-safe
  ASCII-only edits (e.g. `LC_ALL=C sed`), and re-check the encoding after. Never
  let a mechanical re-encode into the diff.
- **`Main.vcxproj` / `.filters` are UTF-8 with BOM.** Preserve the BOM; verify
  with `head -c 3 <file> | od -An -tx1` (must be `ef bb bf`) and `xmllint --noout`.
- **Standalone (no-stdafx) .cpp files must be `NotUsing` PCH** in the vcxproj, or
  they fail C1010. `RenderMatrix.cpp` and the imgui backends follow this.
- **Two shader owners**: `CShaderScene` (`SHADER_PIPELINE`, terrain/character,
  `#version 330 compatibility`, fed by fixed-function draws) and `CShaderGL`
  (`SHADER_VERSION_TEST`, BMD Model VBO/GPU-skinning, already `330 core`).
- The 3.3 context is opt-in via `-gl33compat` and requests the **Compatibility**
  profile on purpose. Do not request Core until the audit gate (§5 of the audit
  doc) is clean.

## 4. Next step — Phase 13.4 (first real consumer of the CPU matrices)

Phases 13.2 (projection) and 13.3 (view) are done: `g_ProjectionMatrix` and
`g_ViewMatrix` (both extern-able, defined in `ZzzOpenglUtil.cpp`) now hold CPU
copies of the current projection and camera view every frame, matching the
fixed-function matrices 1:1, but nothing consumes them yet.

Phase 13.4 (done) fed **`uProj`** from `g_ProjectionMatrix` in `RenderMeshVBO`
(`ZzzBMD.cpp`). It is safe because projection is a single global value and every
camera site — the world camera (`BeginOpengl`) and the NewUI 3D preview
(`NewUI3DRenderMng.cpp:129`) — routes through `gluPerspective2`, so
`g_ProjectionMatrix` always holds the projection of the camera about to draw.

### `uView` is NOT a safe global swap (do not attempt a one-line `modelView = g_ViewMatrix`)

The MODELVIEW at `RenderMeshVBO` is **per-camera**, not a single global:

- World pass: MODELVIEW == pure camera view (bones carry world placement), which
  `g_ViewMatrix` mirrors. Fine on its own.
- **NewUI 3D previews** (`NewUI3DRenderMng.cpp:130-132`): set their own MODELVIEW
  with `glLoadIdentity()` + per-object `Render3D()` transforms, and **never call
  `BeginOpengl`**, so `g_ViewMatrix` is never updated for them. Feeding
  `g_ViewMatrix` here would render every inventory/character/shop preview with the
  world camera → broken previews.

This is exactly why the current code reads `GL_MODELVIEW_MATRIX` back per draw.
The view-matrix migration therefore requires a **CPU modelview stack**
(`RenderMatrix::Stack`) mirrored at *every* modelview site (`BeginOpengl`, the
NewUI preview, per-object `Render3D`), not a single global. That is Phase 15
(character/BMD) work, done additively the same way (mirror first, consume later),
not a 13.x one-liner.

### Modelview-stack backbone (PAUSED — scope deferred)

`13.5.1` (done, compiles): `g_ModelViewStack` owner added; world camera
(`BeginOpengl` push/identity/rotate/translate, `EndOpengl` pop) mirrored;
`g_ViewMatrix` is now a derived snapshot of `Top()`. No consumer yet.

Remaining work is **paused**: mirroring every modelview site (the NewUI preview
plus **16 `Render3D` implementations** and audit §2.2's 62 `glPush/PopMatrix`
across 15 files) is large, and hand-mirroring desyncs easily. When resumed, do it
via a **wrapper API** (`RM_LoadIdentity/Push/Pop/Rotate/Translate/…` that call the
`gl*` op *and* update `g_ModelViewStack` in one place), migrate sites
mechanically, then swap `RenderMeshVBO`'s `uView` from the `glGetFloatv` readback
to `Top()`. Belongs with Phase 15/17 (character/BMD + UI previews).

Do not remove any `gluPerspective`/`glRotatef` yet.

### Actual next step — Phase 14 (terrain), chosen path

**Terrain architecture (as-is).** `ZzzLodTerrain.cpp` draws ground/water/grass via
a **mix** of immediate mode (many `glBegin(GL_TRIANGLE_FAN)` + `Vertex*` helpers)
and **client arrays** (`glVertexPointer`/`glColorPointer`/`glTexCoordPointer` +
`glDrawArrays(GL_QUADS)`, ~line 1961). It binds `CShaderScene::Use(eShaderS_Terrain)`
(`terrain.vs/.fs`, `#version 330 compatibility`, reading `gl_ModelViewProjectionMatrix`,
`gl_Vertex`, `gl_NormalMatrix`, `gl_Normal`, `gl_MultiTexCoord0`, `gl_Color`).
`CShaderScene` loads programs by base name from `s_ShaderBaseName[]` and supports
a post-`#version` define via `BuildProgramFromFiles(..., vertexDefine)`.

**Phase 14 slice plan (each additive/behavior-preserving until the explicit switch):**

- **14.1 (done, inert):** authored Core-profile shaders `Client\Shaders\terrain_core.vs`
  / `terrain_core.fs` — explicit attributes (`aPos/aNormal/aTex/aColor`) and
  uniforms (`uProj/uModelView/uNormalMatrix/texture1/brightness/contrast`),
  semantically identical to `terrain.vs/.fs`. Not referenced by `CShaderScene`
  yet → zero behavior change, nothing to build.
- **14.2 (done, compiles):** `CShaderScene::Init` now runs a **compile-link probe**
  of `terrain_core` (via `LoadProgram`), logs `Core terrain probe ... OK`/`FAILED`,
  and deletes the probe — not stored in `m_Program`, not bound, scene unchanged.
  Also dropped the non-portable uniform initializers (`= 1.0`) from
  `terrain_core.fs` (invalid in strict `330 core`; the draw path sets them from
  C++). **Runtime check:** confirm the log shows the probe compiled+linked OK on
  the user's GPU, and the scene is identical. If it logs FAILED, read the GLSL
  error the shader compiler emitted and fix `terrain_core.*` before 14.3.
- **14.3:** build the terrain **VBO/VAO** (interleaved `aPos/aNormal/aTex/aColor`)
  alongside the existing feed, populated per frame from the same terrain vertex
  data, but not drawn. Additive.
- **14.4:** behind a new opt-in flag (e.g. `-gl33terrain`), draw the ground pass
  via the VBO + `terrain_core` program (feeding `uProj`=`g_ProjectionMatrix`,
  `uModelView` from the camera, `uNormalMatrix`), converting `GL_QUADS` → two
  triangles. Default path untouched. Validate per map.
- **14.5+:** extend to water, grass, and the alpha/blend passes; fold fog and
  alpha test into the fragment shader; then make the Core terrain path default
  once every map validates.

Highest visual risk in the whole migration — validate per map, and water / grass /
alpha specifically, on NVIDIA/AMD/Intel when available.

Build note: the MSBuild compile (Release/x86,
`MSBuild.exe Main.sln -p:Configuration=Release -p:Platform=x86 -m`) is run per
slice to confirm it compiles; the in-game Release run is the behavioral gate.

Note: `ZzzOpenglUtil.cpp`/`ZzzBMD.cpp` are UTF-8; `ZzzOpenglUtil.h` is ISO-8859 —
declare externs byte-safe (as `extern float g_ProjectionMatrix[16];` in
`ZzzBMD.cpp`) or in a UTF-8 header.

Later phases (from the audit worklist): 14 terrain, 15 character/BMD fallback,
16 effects/sprites/shadow/hair, 17 UI/3D previews, 18 Core switch.

## 5. RenderMatrix API quick reference

`source/RenderMatrix.h`, namespace `RenderMatrix`, column-major `float[16]`
(feed directly to `glUniformMatrix4fv(loc, 1, GL_FALSE, m)`):

- `Identity`, `Copy`, `Multiply(out,a,b)` (out=a*b), `MultiplyRight(m,b)` (m=m*b)
- `Translate/Scale/Rotate(m,...)` — post-multiply, GL semantics; Rotate in degrees
- `Frustum/Ortho/Perspective/LookAt` — load the matrix (exact GL/GLU formulas)
- `class Stack` — GL matrix-stack replacement: `LoadIdentity/Load/Push/Pop/Top`
  and `MultiplyRight/Translate/Scale/Rotate` on the top

The host test used to verify it lives in the session scratchpad (not committed);
recreate it from §5 checks if needed, or re-run the same semantic invariants.
