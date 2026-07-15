---
name: mu-main52-build-release-qa
description: "Build, release, and regression workflow for MUServerge/main-5.2 and MUServerge/SRCMainGS. Use for Visual Studio/MSBuild setup, Win32 toolsets and CRT linkage, dependencies, compile flags and product variants, build failures, packaging, client-server compatibility, deployment manifests, symbols and crash diagnostics, smoke tests, regression matrices, performance baselines, release gates, rollback, or validating any project change before delivery."
---

# MU Main 5.2 Build, Release, and QA

Make every result reproducible: exact revision, project/configuration/platform, toolchain, dependencies, compile definitions, runtime data/config, database migration, test evidence, and rollback path. Read [references/build-release-map.md](references/build-release-map.md) before substantial work.

## Establish the build identity

Before compiling or comparing binaries, record:

1. Client and server repository revisions and whether the desktop tree has uncommitted changes.
2. Exact solution, project, configuration, and platform; never assume the IDE's selected defaults.
3. Visual Studio/MSVC toolset, Windows SDK, architecture, character set, runtime library, and language/update/type/license macros.
4. Include/library search paths, third-party library architecture/configuration, output path, working directory, and required DLL/data files.
5. Database schema/procedures and runtime INI/script/Data versions.

## Build rules

- Preserve Win32/x86 ABI unless an explicit coordinated migration is approved. Verify pointer-size assumptions, packed packets, binary file layouts, inline assembly, and third-party libraries.
- Do not fix a linker mismatch by suppressing default libraries or forcing symbols until `/MT`, `/MTd`, `/MD`, `/MDd`, iterator/debug settings, architecture, and toolset compatibility are understood.
- Keep client `MAIN_UPDATE`, protocol feature macros, language/encoding, and server `*_UPDATE`/`GAMESERVER_TYPE` variants compatible.
- Replace developer-specific absolute output/include/library paths with repository/build-root properties incrementally; do not redirect production output silently.
- Treat warnings introduced by changed code as defects. Do not enable a repository-wide warning-as-error migration in the same change unless baseline cleanup is the task.
- Never commit generated binaries, PDBs, intermediate files, user settings, secrets, connection strings, or machine-specific paths unless the repository explicitly owns an artifact.
- Keep Release symbols privately available and bind crash reports to binary revision/configuration. Do not distribute sensitive symbols with the client package.

## Change verification workflow

1. Define observable behavior and risk before editing.
2. Build the smallest affected target, then all directly compatible client/server services.
3. Run static checks appropriate to the change: compiler warnings, `git diff --check`, packet/layout assertions, duplicate ID scans, config/schema validation, and focused searches.
4. Run a clean-build check when project files, headers, compile flags, generated data, or dependencies changed.
5. Execute focused runtime tests plus the cross-domain smoke suite.
6. Compare behavior, logs, packets/DB where relevant, frame-time/memory for performance-sensitive changes, and screenshots for visual changes.
7. Record unrun tests explicitly; source inspection is not runtime proof.

## Minimum smoke suite

- Start ConnectServer, JoinServer, DataServer, and GameServer in dependency order; verify ports/connections and clean logs.
- Start client, render login/character/world, authenticate, select/create character, enter and change map, reconnect, and exit cleanly.
- Verify movement, combat/skill, item pickup/use/equip/drop, NPC shop, inventory/warehouse, chat/whisper, party, guild, and one representative event.
- Exercise LookAndFeel5 at supported resolutions/aspect ratios, including hit testing, text, tooltips, 3D previews, and window bounds.
- Exercise one dense scene and one world switch while observing FPS/frame-time, CPU/RAM/VRAM, GL errors, and leaks.
- Verify save/reload of character, items, currency, social state, and affected feature data.

Tailor this suite using the relevant domain skill; do not run destructive economy/database tests against production data.

## Protocol and persistence gate

For packet, structure, serialization, or database changes:

- build and deploy compatible client and all affected services as one version set;
- assert packed sizes, widths, header/subcode, encryption/serial path, bounds, and old-version rejection/compatibility behavior;
- back up schema/data, make migrations restart-safe and idempotent where practical, and define downgrade/rollback limitations;
- test duplicate, reordered, truncated, late, and old-client packets plus service restart and reconnect;
- use `$mu-main52-protocol-persistence` for the detailed trace.

## Performance and visual gate

- Compare like-for-like Release builds with the same data, resolution, driver, scene, character count, and settings.
- Report median and tail frame time, not only FPS; include loading time and memory when relevant.
- Use screenshots/video and stable camera/state for renderer/UI comparisons.
- Route renderer benchmarks through `$mu-main52-performance` and `$mu-main52-renderer`; route LookAndFeel5 checks through `$mu-main52-ui-lookandfeel5`.

## Release package

Create a manifest containing binaries, revision/configuration, hashes, required runtime DLLs, configs/scripts/assets, schema migrations, compatibility requirements, known issues, and test evidence. Keep secrets and environment-specific endpoints outside the distributable template.

Deploy in a reversible order, preserve the previous known-good package and database backup, run post-deploy health/smoke checks, and define objective rollback triggers. Do not call a release successful solely because compilation passed.

## Handoff

Report exact build identity, changed targets and artifacts, warnings/errors resolved, tests passed/failed/not run, runtime environment, performance/visual evidence, compatibility and migration requirements, known risks, deployment order, and rollback steps.
