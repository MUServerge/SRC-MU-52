# Measurement plan

## Stable scenes

Keep resolution, VSync/FPS cap, camera, map, character/effect count, client data, and build configuration identical.

Use login/select, empty Lorencia, crowded town, many monsters/skills, chrome/wings/effect-heavy characters, terrain/water/grass, inventory 3D preview/UI, and repeated map/model load/unload.

## Metrics

- average FPS and 1% low;
- CPU and GPU frame time;
- update/render/pass durations;
- draw calls, triangles, program/texture/state switches;
- buffer uploads and bytes per frame;
- allocations/deallocations per frame;
- RAM/VRAM before and after reload;
- load time and I/O;
- packet rate when relevant.

## Instrumentation

- Use `std::chrono::steady_clock` for CPU scopes.
- Aggregate samples; do not log every hot-loop iteration.
- Retrieve GPU timer-query results later to avoid stalls.
- Separate CPU submission from GPU completion.
- Record hardware, driver, GL/GLSL, build, resolution, and settings.
- Warm caches unless cold start is the target.

## Acceptance

Require a repeatable improvement outside normal variance and no correctness, visual, stability, memory, or compatibility regression. Prefer frame-time distribution over a single FPS snapshot.

