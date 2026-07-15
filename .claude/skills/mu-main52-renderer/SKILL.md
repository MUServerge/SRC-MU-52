---
name: mu-main52-renderer
description: OpenGL renderer workflow for MUServerge/main-5.2, including shaders, VAO/VBO/EBO, BMD models, GPU skinning, terrain, effects, render state, context creation, GLEW, ImGui backends, and staged OpenGL 3.3 migration. Use for rendering bugs, visual differences, driver crashes, shader/VBO optimization, GPU resource lifetime, or GL compatibility/core modernization.
---

# MU Main 5.2 Renderer

Read [references/renderer-baseline.md](references/renderer-baseline.md) before changing renderer code.

Treat rendering as ordered global state until ownership is centralized. Preserve blend, depth, alpha, texture, matrix, culling, shader, framebuffer, buffer, and pixel-store state across subsystem boundaries.

## Renderer rules

1. Map the render pass and state transitions before editing a draw call.
2. Reuse the existing texture/model/effect/state wrapper when correct; do not create a parallel renderer or shader manager.
3. Give every GPU object one owner and symmetric creation/destruction under a valid context.
4. Save and restore state changed inside nested draws, especially program, VAO, active texture, buffers, blend/depth/cull, and pixel-store state.
5. Cache uniform/attribute locations after link. Avoid per-mesh state queries and redundant uploads.
6. Keep a tested legacy fallback until the new path covers the same materials, deformation, lighting, alpha, and hardware behavior.
7. GPU skinning must replace equivalent CPU render transforms; duplicated CPU/GPU work is not an optimization.
8. Validate shader assets, compile/link results, capabilities, entry points, and limits.
9. Compare reference screenshots and frame time in stable scenes.
10. Do not request a Core Profile while required fixed-function paths remain.

## Shader state ownership

Use one authoritative shader/program state owner. Nested draws must restore the previous program rather than blindly binding `0`. Manager state must match `GL_CURRENT_PROGRAM`.

Avoid compiling the same program through two managers. Remove unused programs only after checking runtime assets, callbacks, and feature flags.

## VBO and resources

- Validate streams and indices before partial allocation.
- Use an intentional vertex layout; use an EBO only when indices provide reuse.
- Check index width and hardware limits.
- Delete every VAO/VBO/EBO in model release before CPU mesh memory is discarded.
- Cache view/projection at pass level and material/bone state at the narrowest valid level.
- Design bone transport against real uniform/UBO/TBO limits.
- Split terrain data by update frequency: keep stable height/topology in static buffers, while grass wind, water motion, and dynamic light/color remain shader inputs or intentionally updated streams.
- Verify reload and context-loss behavior.

## OpenGL 3.3 migration

1. Stabilize the compatibility renderer.
2. Request a 3.3 Compatibility context with legacy fallback.
3. Modernize BMD, terrain, effects/sprites, UI, and ImGui incrementally.
4. Remove fixed-function calls.
5. Request 3.3 Core only after a repository-wide deprecated API audit passes.

Changing only the reported context version is not a performance optimization.

## Validation

Test login/select, characters, monsters, equipment materials, animation, terrain/water/grass, effects/wings/blur, UI previews, ImGui/Scaleform/NewUI, map change, reload, shutdown, and NVIDIA/AMD/Intel when available.
