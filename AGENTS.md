# SRC-MU-52 — Project Guide

MU Online (MuEmu-style emulator), **Season 5.2**. Windows, C++, **32-bit (x86)**.
A **long-running, legacy** project: every change prioritizes **stability, compatibility,
and reuse** over rewriting or modernization.

> **Prime directive:** _Search first → reuse second → extend third → write new code last._
> Every change should look like the original author wrote it.

Primary project workflow: invoke **`mu-main52-engineering`**. Route architecture inventories to
**`mu-main52-codebase-audit`**, PR/commit-range checks to **`mu-main52-pr-regression`**, and then
load every affected specialist `mu-main52-*` skill.

---

# Skill routing and verification debt

- Treat this repository as the canonical source. Use separate `main-5.2` or `SRCMainGS`
  repositories only for historical comparison.
- Scattered code, ownership, duplication, dead-code or unused-asset claims:
  `mu-main52-codebase-audit`.
- Pull requests, commit ranges, or a chain of changes merged without a desktop build:
  `mu-main52-pr-regression`.
- FPS/renderer changes always require `mu-main52-performance`, `mu-main52-renderer`,
  `mu-main52-build-release-qa`, and `mu-main52-pr-regression`; add
  `mu-main52-refactor` when ownership or structure changes.
- Mark a change **build-unverified** until a clean Release|Win32 build exists, and
  **runtime-unverified** until the affected behavior and fallback/error/reload/shutdown path run.
  A later edit to the same path invalidates older verification.
- Before removing a symbol, manager, library, asset, or fallback, check declarations,
  definitions, every caller, callbacks/exports, feature macros, project files, config/scripts,
  runtime lookup, initialization, reload, and teardown.
- Treat default-branch code search as discovery only. Prove symbol/reference claims against the
  exact base and head SHA. A conflicted or updated PR must be re-audited on its resulting head.
- When project/dependency/configuration files change, build both clean Release|Win32 and
  Debug|Win32; Release remains the runtime and delivery gate.

Audit helpers live under `tools/audit/`; they are read-only and create reports only when an
output path is explicitly supplied.

---

# Golden Rules

### 1. Additive, not destructive
Extend, don't replace. Prefer a new branch / helper / wrapper / virtual handler / config flag
over rewriting working code. Keep old behavior unless removal is explicitly requested.
**Never delete or silently replace code you don't fully understand.**
_This governs adding features/systems. **Bug fixes are the carve-out** — see Rule #11: fix the
broken code at its source, cleanly, instead of bolting a compensating patch on top of it._

### 2. Reuse before reinvent
Before writing anything new, **search** (Grep/Glob) in order: current file → folder → project
→ shared utils → managers → helper classes → packet handlers → encryption/file systems → UI
helpers. Use existing macros, helpers, allocators, packet builders, bitmap helpers. Never
create a parallel implementation of something that exists.

### 3. Match existing style
Naming, tabs, brace style, spacing, comments, macros, logging, file organization — mirror the
file you're editing. No new style.

### 4. Improve carefully
While touching code, small safe wins are welcome: de-duplicate, readability, fewer
allocations, perf. **Not allowed:** risky refactors, architecture rewrites, touching unrelated
systems. Never fold a refactor into a bug fix without saying so.
_(For the buggy unit itself, Rule #11 goes further: rewrite it cleanly at the source rather than
patching over it — still scoped, still compatibility-safe.)_

### 5. Preserve compatibility (source · binary · packet)
Compatibility outranks cleaner code. Unless explicitly requested, **never**:
rename classes/methods/members/macros/enums/packet structs/exported functions ·
change memory layout (class/struct/vtable/packing/packet size) · reorder packet members.
Packet structs must stay **byte-compatible with the server**.

### 6. Respect existing hooks & switches
Don't remove or bypass hooks, detours, feature flags, `ENABLE_*` macros, compile switches,
compatibility layers, or patch systems. Assume each exists for a reason.

### 7. Don't modernize; keep x86
Legacy Win32/x86 codebase. Don't auto-introduce C++17, `std::filesystem`, `std::optional`,
smart pointers, "auto everywhere", range-for, or STL rewrites unless asked. No 64-bit or
pointer-size assumptions.

### 8. Explain risky changes first
If a change touches **rendering, networking, encryption, memory layout, file formats, or
engine init**, explain the likely impact before implementing.

### 9. Confirm before destructive/asset actions
Ask first (and recommend a backup) before: overwriting `Client*/Data` assets, replacing
maps/textures, modifying encrypted files, bulk edits, deleting code, or changing packet/class
layouts.

### 10. Definition of done
A change is done only when: **(a)** the affected solution builds clean in **Release / x86**,
**(b)** behavior is verified by actually running the component (a clean build does not by
itself prove correct behavior), **(c)** a `CHANGELOG.txt` entry is added, and **(d)** if the change
(or the user) **confirmed a new durable, non-obvious fact** about a subsystem — or changed how one
works — the relevant docs are updated the same way as the changelog: the matching `mu-main52-*` skill
and/or this file's **Project facts**. Verified facts only, kept general (not example-bound); skip trivial one-offs.

### 11. Bug fixes: fix at the source, clean — not bolted on
When fixing a **bug**, don't stack a patch, compensating hack, workaround, or extra layer on top
of the broken code. Fix the **root cause in place**: rewrite the specific function / unit that owns
the bug so it reads clean, organized, and **more optimized than it was** — as if it had been written
correctly from scratch. No dead weight bolted onto hot paths, no "fix on top of a fix".
**Bounds (never cross these):**
- **Stay scoped** to the unit that actually owns the bug — don't rewrite neighbours or unrelated systems.
- **Fully understand it first** — if you can't explain the old code, you're not ready to replace it (Rule #1's "never replace what you don't understand" still holds).
- **Preserve the external contract absolutely** — signatures, class/struct/vtable/packet layout, callers, and the behavior of every _other_ code path stay identical, so nothing breaks later. **Rule #5 (compatibility) and "no regressions" always win over cleanliness.**
This sharpens Rules #1/#4 for the bug-fix case: a clean root-cause rewrite of the affected unit beats
an additive bolt-on — but only inside these bounds. When in doubt about scope or layout impact, ask first.

---

# Before writing code — answer first
1. Does this already exist?  2. Can I reuse it?  3. Can I extend it?
4. Will this break compatibility?  5. Can I implement it additively?
Any uncertainty → **search the project first.**

---

# Project layout

| Path | Description |
|---|---|
| `SRCMainGS/Source/Main5.2/` | Client source (`Main.sln`) |
| `SRCMainGS/Source/GameServer/` | GameServer |
| `SRCMainGS/Source/ConnectServer/` | ConnectServer |
| `SRCMainGS/Source/DataServer/` | DataServer |
| `SRCMainGS/Source/JoinServer/` | JoinServer |
| `SRCMainGS/Source/Encoder/` | Encoder tool |
| `MainInfo/` | Editor for `av-code45.pak` |
| `Build/Client/` | Generated client build output |
| `Client/` | Live client runtime and Data assets |
| `MuServer/`, `MuServerTK/` | Runtime servers |
| `CHANGELOG.txt` | Change history |

# Build
Client: `MSBuild "SRCMainGS/Source/Main5.2/Main.sln" /p:Configuration=Release /p:Platform=x86`
→ output `Build/Client/Main.exe`. **Never switch to x64.** Build only the solution for the
component you changed.

# CHANGELOG format
Newest block on top. Use the **real current date** (don't copy an old one):
```
------------------------------------------------------------
 YYYY-MM-DD
------------------------------------------------------------

- Client: <description> (<function/area>)
- GameServer: <description>
```

---

# Project facts
_Confirmed in real work only. Detailed workflows live in the matching `mu-main52-*` skills._

- **All client UI must be resolution-aware (standing rule):** the UI is authored in a virtual **640×480**
  base scaled by `g_fScreenRate` (`WINHANDLE.cpp::InitSize`; `ScreenType` 0=stretch, 1=aspect-lock,
  2=fixed-rate). `GetScreenX()/GetScreenY()` (= `GetWindowsX/Y`) return the VIRTUAL `iWinWidth/iWinHight`,
  which is **not** a fixed 640 on widescreen/ScreenType 1–2. Position via the `pos_*`/`Position_*` macros
  (`CGMFrame.h`) or relative to `GetScreenX()/GetScreenY()`; keep a fill bar's width tied to the frame
  element it matches. **Never hardcode pixels assuming one resolution** — it drifts/breaks on others.
- **Maps get their look per-map, not by default:** the client hard-codes specific effects / terrain
  transparency / object blend / camera / fog / BGM / ambient effects **per map**, keyed on `WD_*`, scattered
  across `ZzzLodTerrain.cpp` (RenderFace), `MapManager.cpp` (LoadWorld), `ZzzObject.cpp`, `ZzzScene.cpp`,
  `GOBoid.cpp`, etc. A map not wired in → objects as cubes/black, no effects/water. This — **not** the index —
  is where map bugs live. Deep guide: `mu-main52-maps-worlds` skill.
- **Canonical source tree:** edit `SRCMainGS\`; client builds go to `Build\Client\Main.exe`.
  The older `SRC 5.2 BASE` desktop tree is backup/reference only.
- **Header change → clean rebuild** (Rebuild, not incremental — a widely-included header like `MapManager.h`
  can leave stale `.obj`s and produce inconsistent behavior).
- _Index naming (trivia, rarely the bug):_ client `Data\WorldK`/`ObjectK` (`EncTerrainK`) = internal/server
  map index `K−1`.
