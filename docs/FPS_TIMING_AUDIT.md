# FPS and timing audit workflow

Run `tools/audit/Export-MU52TimingInventory.ps1` against the exact revision under review. The output is discovery evidence, not proof that a consumer is wrong.

## Classify every consumer

| Classification | Expected ownership |
|---|---|
| Gameplay simulation | fixed cadence or server-authoritative; preserve existing rate/order |
| Animation/effect | elapsed-time or bounded interpolation after visual parity proof |
| UI/input | normally once per rendered frame with correct mouse/layout transforms |
| Network timer | monotonic elapsed time; preserve packet cadence/order and wrap safety |
| Visual frame counter | migrate only after 25/60/120 behavior is understood |
| Compatibility path | retain until feature/fallback and runtime use are disproved |

For each match record entry point, owner, writers/readers, current cadence, expected cadence, stall behavior, wrap/precision behavior, and 25/60/120 observation. Do not move broad `MoveMainScene`, input, UI, physics, effects, or packet graphs into the fixed loop as one change.

## Required runtime matrix

- Same Release|Win32 binary, Data/config, driver, resolution, map, camera, and character count.
- 60 and 120 modes in an empty scene and representative dense scene.
- Normal frame, long frame/stall, map transition, reconnect, focus loss/restore, and shutdown.
- Record median, p95, p99 and maximum frame time; emitted/dropped fixed steps; visible animation/water/effect behavior; packet cadence; CPU/GPU/RAM/VRAM.
- Preserve gameplay speed, collision, input feel, animation rate, effect lifetime, and server interaction.

