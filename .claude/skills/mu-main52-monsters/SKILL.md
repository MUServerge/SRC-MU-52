---
name: mu-main52-monsters
description: Monster architecture workflow for the MUServerge/main-5.2 MU Online Season 5.2 Win32 C++ client. Use for monster creation, class and model mapping, client AI or movement presentation, animations, effects, sounds, map and event specializations, combat visuals, resource loading, update loops, and monster-related packets.
---

# MU Main 5.2 Monsters

## Trace the lifecycle

1. Read `references/monster-map.md` before changing monster behavior.
2. Trace spawn packet/data -> monster class lookup -> character/object creation -> model/resources -> update/movement -> render/effects/sound -> deletion.
3. Separate generic monster behavior from map- or event-specific overrides.
4. Search `CreateMonster`, `CGMMonsterMng`, `ZzzCharacter`, AI/controller code, map processors, and event modules before adding a special case.

## Preserve authority and identity

- Treat spawn, despawn, AI decisions, targeting, combat outcomes, drops, and rewards as server-authoritative.
- Keep client prediction, interpolation, animation selection, effects, and sound presentation subordinate to received state.
- Preserve monster IDs, class/model mapping, action enums, object keys, map gates, animation timing, and resource lifetime.
- Avoid broad switch growth. Reuse an existing generic path or explicit extension point before introducing another map-specific branch.
- Preserve original MU gameplay and appearance unless explicitly requested otherwise.

## Change safely

- Verify resource loading occurs once at the correct lifetime and deletion releases or reuses resources safely.
- Make data-driven conversions only after mapping current exceptional behavior and fallback rules.
- Refactor the touched specialization incrementally; do not merge visually similar monsters until behavior and packets are proven equivalent.
- Test normal spawn/despawn, off-screen transitions, map changes, death, respawn, crowded scenes, unknown classes, and event monsters.
- Measure frame-time impact for changes in per-monster update or render loops.

## Report evidence

- Identify the generic factory, any override, packet/data source, model mapping, and render/update consumers.
- Distinguish client presentation logic from server behavior that is absent from this repository.
- Add verified lifecycle and specialization paths to `references/monster-map.md`.
