# Project context

## Authority order

`AGENTS.md` at the project root is authoritative. Where it and these skills disagree, `AGENTS.md` wins — notably its Golden Rule #7 (do not modernize; keep x86), which narrows the "incremental modernization" direction described here and in `$mu-main52-refactor` to changes the user explicitly asks for.

## Local checkout

This is the standalone `MUServerge/SRC-MU-52` checkout at `C:\Users\hatim\Desktop\SRC-MU-52`. The client and server suite share one repository root, with paths mapped as follows (verified 2026-07-18):

| Skill path | Local path |
|---|---|
| `Main.sln` | `SRCMainGS\Source\Main5.2\Main.sln` |
| `source/Main.vcxproj` | `SRCMainGS\Source\Main5.2\source\Main.vcxproj` |
| `dependencies/` | `SRCMainGS\Source\Main5.2\dependencies\` |
| `Source/<Service>/` | `SRCMainGS\Source\<Service>\` |

Build output goes to `Build\Client\Main.exe`, while the **live client the user runs is `Client\`**. Copy a verified build into the live client only as an explicit deployment step. Confirmed local toolchain: v143, Win32/x86, static CRT, `stdcpp17`/`stdcpp14` per configuration.

`origin` is `https://github.com/MUServerge/SRC-MU-52.git`. This desktop checkout is canonical; the older `SRC 5.2 BASE` tree is backup/reference only.

## Identity

- Repository: `MUServerge/SRC-MU-52`
- Product: MU Online Season 5.2 client
- Language: C++17
- Platform: Windows Win32/x86
- Toolchain: Visual Studio v143
- Primary project: `source/Main.vcxproj`
- Rendering: legacy OpenGL compatibility pipeline with GLEW, fixed-function code, partial Shader/VAO/VBO work, and ImGui OpenGL2 backend

## Core preservation goals

Preserve unless explicitly changed:

- combat, animation, timing, scene/map, and camera semantics;
- visual identity, blend modes, materials, effects, terrain, water, and UI;
- packet layouts, ordering, and client/server version compatibility;
- BMD and other game data formats;
- Win32/x86 requirements of old integrations;
- startup/shutdown and global construction order;
- controlled fallback for older player hardware.

## High-risk areas

- Structures shared with files, packets, hooks, or external libraries
- Packing, raw casts, manual allocation, fixed arrays, and hardcoded limits
- Global renderer state and render order
- Model/resource lifecycle and scene transitions
- Timing, FPS, animation advancement, and VSync
- Function pointers, callbacks, Lua bridges, exports, and runtime-name lookup
- Protection, encryption, and networking code

## Reuse-first checklist

Before adding code, search exact symbols and behaviors, nearby modules, project-wide helpers, build dependencies, disabled flags, upstream/original implementations, teardown, and fallback.

Prefer direct reuse, small extension of an owner, adapter around proven code, focused new component, then a new dependency.

## Living knowledge

Client shutdown is performed inside `CWINHANDLE::winLoop`'s `DispatchMessage`
path for `WM_CLOSE`/`WM_DESTROY`. Once the global `Destroy` flag is set, the loop
must exit before its protocol tail because socket/chat owners have already been
released.

When a fact is verified from code or runtime, update the relevant reference. Separate verified facts from hypotheses and include the evidence file/symbol.

