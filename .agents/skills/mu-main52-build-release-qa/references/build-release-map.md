# Build, Release, and QA Source Map

## Client build

- Solution: `SRCMainGS/Source/Main5.2/Main.sln`
- Project: `source/Main.vcxproj`
- Inspected public branch: Win32, MultiByte, v143, static CRT (`MultiThreaded`), Release and Debug configurations.
- Important macros include `MAIN_UPDATE=603`, `PROTO_EXTRA`, language/debug flags, and `FT2_BUILD_LIBRARY`.
- Dependencies are under `dependencies/` and include OpenGL/GLU/GLEW-related libraries, Lua/Sol, FreeType, Crypto++, TurboJPEG, ImGui, audio/shared-memory/modulus libraries, and Windows system libraries.
- Generated client output is isolated under repository-root `Build/Client/`; the live `Client/` runtime is updated only through an explicit deployment step.

## Server suite

| Service/tool | Solution/project root | Main responsibility in release |
|---|---|---|
| ConnectServer | `Source/ConnectServer/ConnectServer.sln` | Server list/routing endpoint |
| JoinServer | `Source/JoinServer/JoinServer.sln` | Account/login/session service |
| DataServer | `Source/DataServer/DataServer.sln` | Persistence and cross-server data services |
| GameServer | `Source/GameServer/GameServer.sln` | Authoritative world/gameplay process |
| Encoder | `Source/Encoder/Encoder.sln` | Build/config packaging utility; verify exact operational use |

The server projects contain multiple update/product configurations (603, 401, 803 and normal/CS variants). Choose explicitly; similarly named Release outputs are not interchangeable.

## Confirmed build-system cautions

- Several server project configurations contain developer-machine absolute output paths such as `D:\NewSource5.2Tkm\Build` or deployment-tree-relative outputs.
- GameServer configurations mix `v142` and legacy `v100` toolsets.
- The inspected GameServer Debug 603 configuration uses `MultiThreadedDebugDLL`, while Release 603 uses `MultiThreaded`. Verify every linked library's CRT/configuration before changing or diagnosing heap/iterator/runtime issues.
- This standalone checkout's canonical client is `SRCMainGS/Source/Main5.2`; the older desktop BASE tree is backup/reference only.

## Compatibility fingerprint

Record these values for every test/release set:

- client revision, configuration, `MAIN_UPDATE`, protocol/language macros;
- each service revision, configuration, update/type/license macros;
- packet/header/layout version or hash when available;
- SQL schema/procedure migration version;
- authoritative server config/script hash;
- client Data/asset manifest hash;
- executable/DLL hashes and PDB archive key.

## Domain-focused regression routing

| Change | Required focused skills/checks |
|---|---|
| Shader/VBO/GL/context | `$mu-main52-renderer`, `$mu-main52-performance`; visual parity, GL errors, driver matrix, world/UI/effect smoke |
| LookAndFeel5/resolution | `$mu-main52-ui-lookandfeel5`; resolution/aspect matrix, anchors, hit boxes, text, scissor, 3D previews |
| Character/items/combat | Corresponding character/items/combat skills; save/reload, stats, packets, death/reconnect |
| Economy | `$mu-main52-economy-transactions`; conservation, duplicate packets, disconnect/failure injection, DB verification |
| Maps/events/monsters | Corresponding domain skills; entry/exit/state phases, world resources, spawn/drop/reward parity |
| Social | `$mu-main52-social-systems`; invite/rank/state lifecycle, disconnect, persistence, cross-server presence |
| Assets/data | `$mu-main52-data-assets`; missing/corrupt/duplicate resources, load time/memory, ID parity |
| Refactor only | `$mu-main52-refactor`; behavior-characterization tests and minimal diff proof |

## Evidence template

For each delivered change capture:

1. Build commands or exact IDE configuration/platform and tool versions.
2. Clean/incremental build results and warning delta.
3. Focused test cases with expected/actual outcome.
4. Smoke test coverage and tests not run.
5. Logs, screenshots/video, packet/DB evidence, or profiler captures as appropriate.
6. Binary/config/data compatibility set.
7. Deployment and rollback instructions.
