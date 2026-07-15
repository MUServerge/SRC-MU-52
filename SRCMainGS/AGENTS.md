# SRCMainGS Project Guidance

## Scope and priorities

- These instructions apply to the entire repository.
- Preserve the existing client architecture and extend established systems before introducing a parallel subsystem.
- Prioritize, in order: correctness, compatibility with existing game behavior, render/runtime performance, maintainability, and visual quality.
- Keep changes narrowly scoped to the user's request. Do not perform unrelated cleanup or broad modernization without explicit approval.
- Communicate with the user in Georgian unless they request another language. Keep code identifiers and technical names in English.

## Repository map

- Main solution: `Source/Main5.2/Main.sln`.
- Main client project and source: `Source/Main5.2/source/Main.vcxproj` and `Source/Main5.2/source/`.
- Shared/support code may also live under `Util/`.
- The real runnable client and authoritative runtime-data tree is the sibling `../Client/` directory (`C:/Users/hatim/Desktop/SRC 5.2 BASE/Client`). When code refers to runtime textures, fonts, XML/INI configuration, models, or other `Data/` files, inspect that client tree first.
- `Client_2/` is a build-output location, not the authoritative live client or runtime-data source. Do not infer live assets or configuration from it.

## Working-tree safety

- Assume the worktree may already contain important user edits and generated build changes.
- Before editing a tracked file, inspect its targeted `git diff`. Preserve all unrelated user changes, including changes in the same file.
- Never use `git reset --hard`, destructive checkout commands, or bulk cleanup unless the user explicitly requests them.
- Do not modify or intentionally include generated artifacts such as `.vs/`, `Debug/`, `Release/`, `*.obj`, `*.pdb`, `*.tlog`, build logs, or other compiler output.
- Use targeted `git status`, `git diff`, and `git diff --check`; avoid treating the entire noisy worktree as part of the task.
- Do not stage, commit, push, or create a PR unless the user explicitly asks.

## Architecture rules

- Reuse existing managers, facades, resource loaders, and render helpers. Do not create a second implementation of an existing service merely to avoid understanding the current one.
- Treat any external or comparison project strictly as a behavioral, mathematical, architectural, and visual reference. Do not copy its source code or assets directly into this project; derive a project-native implementation that respects this repository's ownership, APIs, data formats, and performance constraints.
- LookAndFeel 5 (Look5) is the active production interface and the default target only for UI/interface work. Keep Look1-Look4 UI behavior isolated and unchanged unless the user explicitly includes those skins in scope.
- Do not apply the Look5-only restriction to shared non-UI systems such as camera, world/terrain rendering, gameplay, networking, animation, or general engine code. Those changes should remain interface-independent unless the user explicitly scopes them to one look.
- Maintain public interfaces where they have many call sites. Prefer improving the implementation behind an existing interface over rewriting callers across the UI or game code.
- Keep ownership and lifetime explicit. Match existing initialization and shutdown order, especially for Windows handles, OpenGL resources, global managers, and static singletons.
- Add new `.cpp` or `.h` files to `Main.vcxproj` and `Main.vcxproj.filters` only when new files are actually necessary.
- Do not add new production dependencies without explaining the need and receiving user approval. Prefer already-linked libraries and project-native facilities.

## Rendering and performance

- Treat rendering code as a hot path unless proven otherwise.
- Avoid new per-frame or per-object heap allocations, font creation, file I/O, texture loading, shader compilation, logging, `glGet*` calls, or full texture uploads in hot loops.
- Cache stable resources and derived data. Reuse textures, glyph/font objects, buffers, and state descriptors.
- Minimize redundant OpenGL state changes. Set state at the narrowest sensible pass boundary rather than once per face, glyph, or object when behavior is identical.
- OpenGL state is global. Any local change to alpha test, blending, depth mask/test, culling, texture state, color, shader binding, or client arrays must leave the expected state for subsequent rendering. Audit every early return.
- Prefer batching and existing vertex-array paths over adding new immediate-mode loops. Preserve compatibility-profile behavior unless a broader renderer migration is explicitly requested.
- Do not claim a performance improvement from intuition alone. Compare the relevant call frequency/allocation/state-change path, and use the existing profiler or measurements when practical.
- Preserve graceful fallback behavior when shader or optional rendering paths fail.
- Keep world-overlay coordinate spaces explicit. `Projection2`/world projection and `CGMFontLayer` use physical screen pixels, while scaled UI helpers such as `RenderImageF` and `g_pRenderText` consume logical UI coordinates and apply `g_fScreenRate`. Convert exactly once at the renderer boundary; never feed projected physical coordinates directly into a scaled UI helper.
- Use one normalized projected anchor for every part of a world overlay (frame, fill, name, marker, and hit-test). Use `MouseRenderX/Y` only with physical-pixel geometry and `MouseX/Y` only with logical UI geometry. Verify overlays at default camera, zoom extremes, camera pitch, and multiple resolutions.
- Keep screen-space overlay size independent from camera zoom. Camera projection may move the anchor, but must not scale health bars, names, NPC icons, or their spacing.
- Resource ownership follows the feature, not the directory where a reference asset was first found. Boss-health-bar texture IDs, paths, loading, and configuration must belong to the health-bar subsystem and must not depend on Quest System resources.
- Production texture calls use the source `.tga` name and the runtime loader resolves the converted `.ozt` in the live `Client/Data` tree. Before adding a call, verify the OZT exists and whether it decodes as RGB or RGBA; the loader and cleanup path must support both valid formats without leaking decoded buffers.

## Font and text system

- The primary UI API is `g_pRenderText` / `CUIRenderText`; preserve it because it has many call sites.
- Extend the current font renderer instead of pasting a separate `CGlFont`-style global font system.
- Preserve UTF-8/ANSI-to-UTF-16 conversion through `CMultiLanguage`; do not regress Chinese, Korean, Vietnamese, Georgian, or other Unicode text.
- `CUIRenderTextOriginal` uses cached antialiased logical-font clones and grayscale-to-alpha coverage. Keep `GetFont()`/`SetFont()` restoration semantics correct when changing font caching.
- `CGMFontLayer` is an existing FreeType path but is not yet a drop-in global replacement for all styles and call sites. Audit style caching, alignment, clipping, long-string limits, resource lifetime, and fallback fonts before expanding its use.
- Keep native input-box caret and selection rendering behavior intact when changing ordinary UI text rendering.
- Font texture filtering and alpha thresholds affect visual quality. Verify text at normal, bold, big, and fixed sizes rather than judging one screen only.

## Legacy source encoding

- Some large legacy `.cpp` files are not valid UTF-8 and may contain ANSI or historical multibyte text.
- Preserve the file's existing bytes and line endings. Never re-encode an entire legacy source file as a side effect of a small edit.
- Use `apply_patch` for normal text files. If it cannot parse a legacy file, use a byte-preserving, exact, uniquely validated replacement and inspect the resulting diff immediately.
- Avoid bulk formatters on legacy source files unless the user explicitly requests a controlled formatting migration.

## Implementation workflow

1. Locate definitions and call sites with `rg`/`rg --files`.
2. Inspect the surrounding render/state/lifetime context, not only the requested line.
3. Inspect targeted existing diffs before modifying files.
4. Implement the smallest architecture-consistent change.
5. Check all error paths and early returns for resource and state restoration.
6. Run targeted diff validation, then build/test in proportion to risk.
7. Report what changed, what was verified, and any runtime-only checks still needed.

## Build and verification

- Canonical release build from `Source/Main5.2/`:

  ```powershell
  & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'Main.sln' /m /p:Configuration=Release /p:Platform=x86 /verbosity:minimal /nologo
  ```

- The solution maps `Release|x86` to the project's `Release|Win32` configuration.
- A successful build currently produces `Client_2/Main.exe` relative to the repository's parent layout; this is a build artifact and is distinct from the real runnable `Client/` tree.
- Existing warnings are not automatically caused by the current change. Distinguish baseline warnings from new warnings/errors.
- For rendering/UI changes, perform or request focused runtime smoke checks: normal/bold/big text, chat, tooltips, input selection/caret, resolution or font-size changes, alpha edges, and at least one affected map/UI screen.
- Do not rebuild for documentation-only changes unless verification specifically requires it.

## Definition of done

- The requested behavior is implemented without replacing an established subsystem unnecessarily.
- Unrelated user edits remain intact.
- Hot-path cost, global render state, resource lifetime, Unicode behavior, and legacy encoding were considered.
- Relevant source diffs pass `git diff --check`.
- The appropriate build/tests pass, or any blocker and unverified runtime behavior are clearly reported.

## Maintaining this guidance

- Keep this file concise and durable. Do not add task history, temporary debugging notes, or one-off preferences.
- When the user corrects a recurring architectural or workflow assumption, update the nearest applicable `AGENTS.md` rule so future tasks inherit the correction.
- Use a nested `AGENTS.override.md` only when a subtree genuinely needs rules that differ from these repository-wide instructions.
