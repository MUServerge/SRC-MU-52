# Renderer Phase 0 Baseline Runbook

Date: 2026-07-15

This runbook captures renderer baselines only. It does not define an optimization target and must not be used to claim an improvement without a like-for-like before/after run.

## Build and runtime identity

Record these values with every capture:

- solution: `SRCMainGS\Source\Main5.2\Main.sln`;
- project: `SRCMainGS\Source\Main5.2\source\Main.vcxproj`;
- configuration/platform: `Release|Win32` (the project is the x86 client);
- executable under test and SHA-256;
- complete `Client\Data` and shader-asset manifest/hash;
- `MAIN_UPDATE`, renderer feature macros, and whether the worktree is dirty;
- Windows version, CPU, RAM, GPU, driver version, monitor refresh rate, and foreground/background state;
- the startup `<Renderer pixel-format diagnostics>` and `<Renderer OpenGL capability diagnostics>` log blocks.

Run from the normal `Client` working directory so `Data\...` and `Shaders\...` resolve exactly as they do for players. Do not copy or deploy the executable as part of a baseline capture.

## Enabling the profiler

The profiler is disabled by default. The protected runtime can remove inherited environment variables, so the preferred activation is the explicit command-line switch:

```powershell
Set-Location 'C:\Users\hatim\Desktop\SRC-MU-52\Client'
& '.\Main.exe' -renderprofiler
```

`MU_RENDER_PROFILER=1` remains supported for launchers that preserve the environment. Successful activation writes:

```text
[RenderProfiler] enabled
```

Output is aggregated approximately every two seconds. No per-draw log lines, `glGetError` polling, or GPU timer query is performed. `FrameTimeStable` and `FrameTimeLoading` are separate fixed-capacity sample groups. Immediate-mode draw calls are counted, but their per-vertex submissions are intentionally not intercepted; `SubmittedVerticesKnown` and `SubmittedTrianglesKnown` are therefore lower bounds covering `glDrawArrays`/`glDrawElements` calls.

## Fixed test controls

Use the same settings for every comparative run:

- primary resolution: 1280×720, identical `ScreenType`/aspect mode;
- optional compatibility pass: 640×480, reported separately rather than mixed;
- VSync: enabled, monitor fixed at 60 Hz;
- client frame cap: 60 FPS;
- camera: fixed position, yaw, pitch, distance, and zoom; disable camera input after placement and save a reference screenshot;
- window state: foreground, unobscured, same full-screen/windowed mode;
- warm-up: 60 seconds after the scene is fully loaded;
- measured duration: 120 seconds per stable scene;
- repeats: five complete runs per scene, restarting the client between runs unless the scenario explicitly tests transition/reconnect;
- network/server population, character, equipment, skills, effects, weather, time, and map coordinates: fixed and recorded;
- background software, overlays, driver overrides, and power plan: fixed;
- loading/transition frames: retained but reported separately from stable gameplay frames.

For each repeat, retain the complete log, one scene-start screenshot, one scene-end screenshot, and any crash report. Do not average unlike scenes or mix cold-start and warm-cache results.

## Scene procedures

### 1. Login

1. Launch from a clean process and wait until the login UI and background are fully settled.
2. Warm for 60 seconds without mouse movement, text input, or window focus changes.
3. Capture 120 seconds with the same server-list state and background animation.
4. Record whether any shader, entry-point, or capability warning appeared.

### 2. Character selection

1. Log in to the same account and server.
2. Select the same character slot and leave the camera untouched.
3. Warm for 60 seconds, then capture 120 seconds.
4. Run separate named captures for each of the seven supported classes; do not combine their samples.

### 3. Normal map

1. Enter the same quiet map, coordinate, direction, and camera pose.
2. Use the same character, equipment, wings, weapon, buffs, weather, and nearby NPC state.
3. Warm for 60 seconds, then capture 120 seconds idle and a separate 120-second fixed movement route.

### 4. Dense player/monster scene

1. Use a controlled server population or repeatable spawn setup with recorded player, monster, and NPC counts.
2. Fix camera pose and prevent entities outside the test plan from entering/leaving view.
3. Warm for 60 seconds and capture 120 seconds without combat.
4. Record visible mesh count assumptions and any population drift.

### 5. Inventory and character preview

1. Use the same character/equipment and open the same inventory/character windows.
2. Keep the same hovered item, tooltip state, preview rotation, and UI layout.
3. Warm for 30 seconds, then capture 120 seconds.
4. Repeat with the same representative 3D item preview and record its item identifier.

### 6. Effects-heavy combat

1. Use a repeatable skill sequence, targets, attack rate, buffs, wings, and effect density.
2. Fix camera and combat duration; avoid unrelated players/effects.
3. Warm with a non-measured rehearsal, reset the encounter, then capture 120 seconds.
4. Preserve video or screenshots for alpha order, blur/trails, particles, joints, and lighting comparison.

### 7. Terrain: grass, water, and dynamic lighting

1. Use a recorded map coordinate and camera that simultaneously shows grass and animated water.
2. Fix weather, world time/light state where the server/client permits it.
3. Warm for 60 seconds and capture 120 seconds.
4. Preserve reference screenshots at matching timestamps for grass displacement, water UV motion, and terrain color.

### 8. Map transition

1. Begin from the same source map/coordinate and use the same gate/warp to the same destination.
2. Start capture 30 seconds before the transition and stop 60 seconds after the destination becomes stable.
3. Repeat five times without mixing the loading samples with stable scene statistics.
4. Record world-entry duration and shader/program/VAO/buffer live counts before and after each transition.

### 9. Reconnect

1. Enter the same normal-map scene and warm for 60 seconds.
2. Trigger the same controlled disconnect method, allow the normal reconnect flow, and return to the same character/map.
3. Capture from 30 seconds before disconnect through 60 seconds of stable post-reconnect rendering.
4. Record duplicate initialization messages, shader/resource count changes, and visual/state anomalies.

### 10. Repeated clean shutdown

1. Launch, reach the same normal-map scene, warm for 30 seconds, then exit through the normal UI.
2. Repeat ten times from a fresh process.
3. For each iteration record exit result, crash report, final profiler block, process termination time, and live resource counts.
4. This phase observes the known shutdown/lifetime risk; it does not certify resource cleanup and must not force-kill the process unless separately labeled.

## Result sheet

For every scene and repeat record:

- stable/loading sample counts;
- frame-time minimum, median, p95, p99, maximum, and arithmetic mean;
- CPU render and named section times;
- draw calls and known submitted vertices/indices/triangles;
- program, VAO, buffer, texture, uniform, CPU-transform, GPU-skinning, and resource counters;
- visual/reference outcome and any GL/shader warning;
- whether the run completed, crashed, or was invalidated.

GPU time remains “not measured” in Phase 0 even when timer-query support is reported. Actual GL values and all baseline numbers remain pending until this runbook is executed on the target hardware.
