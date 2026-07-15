# MU Online GameClient cleanup architecture plan

This project is a Season-6-era MU Online client. The goal is safer performance,
clearer ownership and less legacy noise without changing gameplay speed or
breaking visual compatibility.

## Rules

- Do not delete disabled code until it is classified as obsolete, unfinished, or
  still-needed feature backlog.
- Keep the 25 FPS gameplay contract. Render smoothness must not speed up
  movement, animation logic, packets, timers, or skill timing.
- Prefer measurement before optimization. Use the render profiler before changing
  `ZzzBMD`, `ZzzObject`, `ZzzCharacter`, effects, particles, physics cloth, or
  terrain.
- Make every phase buildable and reversible.

## Current hot spots

- `ZzzEffect.cpp`
- `ZzzObject.cpp`
- `ZzzCharacter.cpp`
- `WSclient.cpp`
- `ZzzOpenData.cpp`
- `ZzzBMD.cpp`
- `ZzzEffectParticle.cpp`

## Disabled or unfinished feature inventory

These are not safe delete candidates yet:

- `LauncherHelper.cpp`
- `GlobalPortalSystem.cpp`
- `GCCertification.cpp`
- `NewGatemanWindow.cpp`
- `CSResourceManager.cpp`
- `Utilities/Dump/*`

Classify each one as:

- `finish`: should become a supported feature
- `keep-disabled`: historical or region-specific feature
- `replace`: old implementation superseded by a new one
- `delete`: only after references, protocol, assets and server dependency are
  checked

## Target logical modules

- `Platform`: `Winmain`, window/input/timer, crash handling
- `Core`: shared definitions, math, utilities, configuration
- `Render`: `ZzzBMD`, OpenGL helpers, shader/VBO, terrain, bitmap loading
- `Game`: character, item, skill, party, guild, managers
- `Network`: `WSclient`, protocol receive/send, packet helpers
- `UI`: legacy UI and `NewUI`
- `Events`: map/event systems, battle/castle/crywolf/kanturu/etc.
- `Effects`: particles, joints, sprites, visual effects
- `ThirdParty`: vendored libraries such as pugixml

Start with Visual Studio filters. Physical file moves come later.

## Phase order

1. Repository hygiene: ignore generated VS/build artifacts and separate source
   changes from output noise.
2. Measurement: keep `RenderProfiler` active for `MAIN_SCENE`, especially the
   10-boss scene and zoom-out scenario.
3. FPS contract: keep 60 FPS cap for now, keep 25 FPS gameplay semantics, and
   continue separating visual step from real delta time.
4. VBO isolation: keep Login/Character scene on legacy path; enable only verified
   `MAIN_SCENE` material paths.
5. CPU transform contract: use `NeedsCpuVertexTransform` and opt in only paths
   that do not need `VertexTransform` for shadow, sidehair, mesh effects or
   attachments.
6. Header and define cleanup: fix missing guards and duplicate defines such as
   `MAX_ID_SIZE`.
7. VS filter reorganization by module.
8. Physical folder moves in small groups, only after filters and build are stable.
9. Large file extraction by real ownership boundaries, not arbitrary line count.

## Next render optimization checkpoint

Run the client in the heavy scene and inspect `ErrorReport` for:

- `BMD::TransformVertices`
- `BMD::TransformNormals`
- `BMD::RenderMesh`
- `BMD::RenderMeshVBO`
- `BMD::RenderMeshLegacy`
- `CPUVertexTransformRequired`
- `CPUVertexTransformSkipped`

Only after this data should the CPU transform skip be enabled for specific
render paths.

