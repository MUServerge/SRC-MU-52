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

Phase 13.4 is the first slice that **uses** them, and therefore the first with
real visual risk. The safe candidate is the `CShaderGL` BMD Model VBO path
(`Client\Data\Effect\VBO\Model.vs/.fs`, already `330 core` with explicit
uniforms), which currently composes its own matrices on the CPU. Plan:

1. Identify the Model path's proj/view/model uniforms and where they are set.
2. Feed `g_ProjectionMatrix` and `g_ViewMatrix` into its `uProj`/`uView` (keep the
   per-object model matrix as is), behind the existing opt-in gate
   (`-vbotranslate`) so the default path is untouched until proven.
3. Verify with `-gl33compat -vbotranslate`: translated meshes render identically.

Keep the fixed-function matrix stack authoritative for every other path. Do not
remove any `gluPerspective`/`glRotatef` yet — removal is a later phase, only
after the shader path fully covers the same materials.

Note: `ZzzOpenglUtil.cpp` is UTF-8; `ZzzOpenglUtil.h` is ISO-8859 — declare the
externs for `g_ProjectionMatrix`/`g_ViewMatrix` byte-safe or in a UTF-8 header.

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
