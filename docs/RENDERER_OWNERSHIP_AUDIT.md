# Renderer ownership audit workflow

Run `tools/audit/Export-MU52RendererOwnership.ps1` against the exact revision under review. Each row is an ownership question, not an automatic defect.

## Required ownership record

| Resource/state | Trace |
|---|---|
| GL context/GLEW | capability load → checked entry points/limits → valid-context lifetime → shutdown |
| Shader/program | source asset → compile/link diagnostics → owner → bind/previous-state restore → delete |
| VAO/VBO/EBO | CPU/model owner → create/upload → bind/update → reload/context behavior → delete |
| Uniform/attribute | link generation → location cache → invalidation/relink → upload cadence |
| Global GL state | previous value → mutation → nested draw behavior → restoration |

## Consolidation gates

- One resource must have one authoritative owner and symmetric release under a valid context.
- Manager caches must not disagree with `GL_CURRENT_PROGRAM` or other real GL state.
- Nested draws restore the previous state, not guessed defaults.
- GPU skinning replaces equivalent CPU work; it must not duplicate it.
- Runtime shader assets and feature macros must be checked before declaring a program unused.
- Preserve compatibility renderer, fixed-function-dependent paths, dynamic terrain, effects/shadows, UI/NewUI and ImGui OpenGL2 until equivalent output and fallback are proven.

Validate login/select/world, characters/monsters/equipment, terrain/water/grass, effects/wings/blur, UI previews, map change, reload, focus/context behavior, and shutdown. Compare stable screenshots and frame-time data on available NVIDIA/AMD/Intel systems.

