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
| 14.2 | `CShaderScene::Init` compile-link **probe** of `terrain_core` (logged, deleted, not bound); dropped non-portable uniform initializers from `terrain_core.fs` | ✅ Release x86 (0 warn/err) | ✅ NVIDIA RTX 3050 Ti (GL 4.6): log `Core terrain probe 'terrain_core' compiled+linked OK`, scene identical (AMD/Intel still to check) |
| 14.3 | Promote `terrain_core` to a kept program: new `eShaderS_TerrainCore` enum slot + `s_ShaderBaseName` entry; removed the throwaway probe (loop loads/keeps it, not bound) | ✅ Release x86 (0 warn/err) | ✅ NVIDIA RTX 3050 Ti: log `Loaded 'terrain_core' (program 9)`, `Init OK`, scene identical |
| 14.4 | First Core-drawn geometry: grass `GL_QUADS` → VBO/VAO + `terrain_core` behind `-gl33terrain`; `CShaderScene::SetMat4/SetMat3` added; default path untouched | ✅ Release x86 (0 warn/err) | ✅ NVIDIA: `Core terrain path active ... this-draw glGetError=0x0000`; UI regression from a program-stack leak fixed in 14.4.4 |
| 14.5 | ~~Ground base tile only~~ → **full terrain Core pass**: single-bind `terrain_core` (`TerrainCoreBegin/End`) + emit helpers (`tTexCoord/tColor/tVertex`) + fan collector; all ~25 `Vertex*` helpers and every ground/grass fan site routed through it; old per-quad `RenderTerrainQuadCore` removed | ✅ Release x86 (0/0) | ✅ NVIDIA: log `Core terrain pass active (terrain_core program 9, single bind)`; ground+grass identical, **no z-fight** (base-only 14.5 was reverted first — coplanar mix; full conversion fixes it) |
| 15.1 | Authored Core character shaders `character_core.vs/.fs` (inert, not wired): explicit `aPos/aTex/aColor` + `uProj/uModelView` | n/a (GLSL assets, no C++) | n/a — not referenced by `CShaderScene`, zero behavior change |

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
- **14.3 (done, compiles):** promoted `terrain_core` from a throwaway probe to a
  **kept** `CShaderScene` program (`eShaderS_TerrainCore`), loaded and logged like
  terrain/character, but not bound for drawing. Safe: its load is independent
  (`Use()`/`GetProgram` fall back per program; `Init()`'s return is unused), so a
  GPU that rejects it only zeroes its own slot. Runtime check: log
  `Loaded 'terrain_core' (program N)`, scene identical.
- **14.4 (done, compiles — awaiting visual check):** grass `GL_QUADS` draw
  (`ZzzLodTerrain.cpp` `RenderTerrainGrassQuadCore`) now routes through a
  lazily-created static VAO/VBO/EBO + `terrain_core` when `-gl33terrain` is passed
  and the program loaded, else the legacy client-array path. Interleaved
  `pos3/normal3(0)/tex2/color4`, `GL_QUADS`→2 triangles, uniforms
  `uProj`=`g_ProjectionMatrix`, `uModelView`=`g_ViewMatrix`, `uNormalMatrix`=id,
  `texture1=0`, `brightness=contrast=1`; switches to `eShaderS_TerrainCore` for the
  draw and restores `eShaderS_Terrain` after. Added `CShaderScene::SetMat4/SetMat3`.
  Default (no flag) is byte-for-byte the old path. **Verify with `-gl33terrain`:
  grass must match the no-flag run** (Karutan wind, PK-field alpha blend). Known
  limitations to watch: per-quad program switch (perf only, this is a proof path),
  and the static VAO/VBO/EBO are not deleted at shutdown (acceptable for the gated
  experiment; give them an owner before the path becomes default).
- **14.5 (done, compiles):** the grass helper was generalized to
  `RenderTerrainQuadCore(colors)` (reads `TerrainVertex`/`TerrainTextureCoord`
  globals + 4 colors) and the **ground base tile** fan (`RenderFace`, the
  `Vertex0..3` `GL_TRIANGLE_FAN`) now routes through it under `-gl33terrain`, with
  ground colors from `PrimaryTerrainLight[TerrainIndex1..4]`. Grass and the base
  ground tile are both Core-drawn now; alpha-layer/blend/special tiles still use
  the legacy fan. Verify visually: with `-gl33terrain` the ground must match the
  no-flag run (no color/brightness patchwork between Core base tiles and legacy
  alpha tiles). Perf note: still one program switch per quad (proof path).
- **14 DONE + DEFAULT ON (2026-07-23):** terrain (ground base/alpha/blend + grass)
  fully renders through the single-bind `terrain_core` pass, verified in-game on
  NVIDIA (no z-fight, identical). `GL33TerrainEnabled()` now defaults ON with an
  opt-OUT safety valve (`-nogl33terrain` / `gl33terrain.disable`); the old opt-in
  `gl33terrain.enable` marker is obsolete. Follow-ups when convenient: fold fog +
  alpha test into the shader, batch tiles, validate AMD/Intel.

### Phase 15 — Character / BMD (ACTIVE)

Structurally the same as terrain: `character.vs/.fs` are `330 compatibility`
(`gl_ModelViewProjectionMatrix * gl_Vertex`, `gl_MultiTexCoord0`, `gl_Color`), fed
by the legacy immediate-mode BMD draw `BMD::RenderMesh` (`ZzzBMD.cpp:1327`,
`glBegin(GL_TRIANGLES)` at ~2155, per-vertex `glTexCoord/glColor/glVertex`,
CPU-skinned via `VertexTransform`). Key difference from terrain: the modelview is
**per-character** (each character pushes its own translate/rotate), so `uModelView`
must be set per object (read back `GL_MODELVIEW_MATRIX` per character, as the
`CShaderGL` `RenderMeshVBO` path already does), not once per pass.

Plan (mirror the terrain emit-collector; keep CPU skinning — do NOT chase the
GPU-skinning VBO path, which the FPS memory documents as flicker-prone on
effect/blend meshes):

- **15.1 (done):** authored `character_core.vs/.fs` (inert).
- **15.2 (done, verified):** added `eShaderS_CharacterCore` to `CShaderScene`
  (load + keep + log, not bound) — mirrors 14.3. Log confirmed on NVIDIA:
  `Loaded 'character_core' (program 12)`, `Init OK`, scene identical.
- **15.3 (done, superseded by 15.4):** added a Core character emit-collector in
  `ZzzBMD.cpp`. NOTE: the plan's premise was wrong — the live character draw is
  NOT immediate mode. `RenderMeshInternal` builds `RenderArrayVertices/TexCoords/
  Colors` from the CPU-skinned transforms and submits them with `glVertexPointer`
  + `glDrawArrays(GL_TRIANGLES)`. The `glBegin(GL_TRIANGLES)` block the plan
  pointed at is in `RenderMeshTranslate`, whose only caller `RenderBodyTranslate`
  has no callers anywhere — dead code. So the conversion is client-arrays -> VBO,
  and the emit-collector had no producer; 15.4 replaced it with a direct array
  upload.
- **15.4 (done, verified):** `CharacterCoreDrawTriangles` uploads the three CPU
  arrays into a VBO/VAO and draws via `character_core`; wired at the
  `RenderCharactersClient` pass (`ZzzScene.cpp`). Gate `Client\gl33char.enable` /
  `-gl33char`. Per-mesh `uModelView` and (no-color-array) `uConstColor` are read
  back from `GL_MODELVIEW_MATRIX` / `GL_CURRENT_COLOR`.
  **Crash + fix:** the first 15.4 bound `character_core` for the WHOLE pass, but
  the pass also draws shadows (`RenderBodyShadow`/`AddMeshShadowTriangles`) and
  part-effects that still use fixed-function client arrays — under the
  explicit-attribute Core program those crashed the NVIDIA driver (minidump:
  fault in `nvoglv32.dll` via `RenderBodyShadow`). Fix: `character_core` is bound
  PER BODY MESH only (Use/Unuse around each `CharacterCoreDrawTriangles`), the
  pass keeps the compatibility `eShaderS_Character` bound for shadows/effects.
  Same per-mesh bind/restore discipline as `RenderMeshVBO`. Never mix core +
  immediate-mode on one pass. Verified in-game (log `Core character path active
  ... per-body bind`, no crash).
- **15.5 (done, compiles — awaiting ON==OFF check):** made `character_core`
  pixel-identical to the compatibility `character.fs` it replaces. There is NO
  fog/tex-env to fold in — neither scene character shader has any. The only
  divergence was texturing: `character.fs` (the OFF path, the verified reference)
  samples `texture1` unconditionally (a bound fragment shader ignores
  `DisableTexture()`), so the 15.3 `uUseTexture` skip for RENDER_BRIGHT/COLOR was
  removed. `uUseVertexColor`/`uConstColor` stay (they reproduce `gl_Color`). The
  two programs are now semantically identical, so the Core path is a byte-for-byte
  substitute. **Verify: with `gl33char.enable` present vs absent the character
  render must be identical** (players, monsters, equipment, wings, chrome/metal
  set armor, alpha/transparent, bright/color meshes). Then make it default.
- **15.6 (done, default ON):** validated in-game across maps (parity, no crash),
  so `GL33CharEnabled()` now defaults ON. Legacy fallback kept as an opt-OUT
  safety valve: `-nogl33char` / a `gl33char.disable` marker file restores the
  compatibility character path without a rebuild. The old opt-in `gl33char.enable`
  marker is obsolete. **Phase 15 (Character/BMD) is complete.**

Do NOT convert per-mesh blend/chrome/effect logic blindly — those are the parts
that historically flickered; verify each in the Release run.
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

### Phase 16 — Effects / Sprites / Hair (ACTIVE)

The highest-flicker-risk surface (FPS memory documents effect/blend meshes as the
ones that historically flickered). Unlike terrain/character, these draws use NO
scene shader today — they are pure fixed-function, and they depend on a real
**texture-env** (`glTexEnvi(GL_TEXTURE_ENV_MODE, GL_ADD)` for additive glows and
`GL_MODULATE`, `ZzzEffectParticle.cpp:9041/9044`), plus per-draw blend modes.

Immediate-mode inventory: `ZzzEffectNoUse.cpp` (8 glBegin: GL_QUADS + GL_TRIANGLES),
`ZzzEffectJoint.cpp` (4), `ZzzEffectBlurSpark.cpp` (2), `Sprite.cpp` (2,
GL_TRIANGLE_FAN, 2D screen verts, textured + untextured branches),
`SideHair.cpp` (4). Emit shape: `glTexCoord2f` + `glColor3fv`/`glColor4ub` +
`glVertex3fv` (world) or `glVertex2f` (screen). Some untextured, some no colour.

Plan (mirror terrain/character: inert shader first, then per-pass emit-collector
behind an opt-in marker, verify, default):

- **16.1 (done, inert):** authored `effect_core.vs/.fs`. Explicit attributes
  (`aPos/aTex/aColor`), `uProj/uModelView`; the fragment shader reproduces the
  fixed-function tex-env combine (`uTexEnvMode` 0=MODULATE/1=ADD/2=REPLACE),
  `uUseTexture` for the untextured branches, and an optional `uAlphaRef` discard.
  Alpha **blend** stays fixed-function (glBlendFunc is program-independent) — only
  the texel*primary combine and alpha-test move into the shader. Not referenced by
  `CShaderScene` → zero behavior change.
- **16.2 (next):** add `eShaderS_EffectCore` to `CShaderScene` (load + keep + log,
  not bound) — mirror 14.3/15.2. Runtime check: log `Loaded 'effect_core'`.
- **16.3:** convert ONE low-risk, self-contained effect draw (candidate: a simple
  world GL_QUADS billboard in `ZzzEffectNoUse.cpp`) to a VBO + `effect_core` emit
  path behind a new marker `Client\gl33effect.enable`, single bind per effect
  pass, `uTexEnvMode`/blend set from the draw's state. Verify identical.
- **16.4+:** extend per effect family — joints/trails, blur/spark, hair — one at a
  time, verifying each in the Release run (these are the flicker-prone ones; do
  NOT batch-convert). Sprite.cpp 2D is arguably Phase 17 (UI); defer unless a 3D
  effect needs it.
- **16.N:** default on once every effect family validates. The character shadow
  (`RenderBodyShadow`, translucent+stencil, currently off) rides along here.

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
