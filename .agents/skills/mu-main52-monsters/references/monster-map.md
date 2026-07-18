# Monster architecture map

## Verified entry points

- `source/CGMMonsterMng.h/.cpp`: custom monster data, model loading, lookup, and `CreateMonster`.
- `source/ZzzCharacter.h`: generic character/monster creation and rendering entry points.
- `source/ZzzInfomation.h`: `MonsterScript`, monster script loading, conversion, and name lookup.
- `source/CAIController.cpp`: client action/movement presentation; verify authority per path.
- `source/w_MapProcess.*`, `source/w_BaseMap.h`: map-level monster and visual specialization.
- `source/Event.h`: seasonal event monster creation and visual movement overrides.
- `source/GMDoppelGanger*`, `source/GMEmpireGuardian*`, `source/GM_Raklion*`, `source/GMHellas*`, `source/GMSantaTown*`, `source/GM_PK_Field*`, `source/GMDuelArena*`: event/map specialization candidates.

## Verified server-side reference (`MUServerge/SRCMainGS`)

- `Source/GameServer/GameServer/MonsterManager.*`, `Monster.*`, `MonsterSetBase.*`: authoritative monster registration, spawn definitions, and base lifecycle.
- `MonsterAI*`, `MonsterAIRule*`, `MonsterAIAutomata*`, `MonsterAIMovePath*`, `MonsterSkill*`: authoritative AI, rules, paths, states, and skills.
- `CustomMonster.*`: custom monster definitions and behavior extensions.
- `Map.*`, `MapPath.*`, `Viewport.cpp`: authoritative movement attributes, pathing, and visibility.
- `ItemDrop.*`, `ItemBag*`, `CustomEventDrop.*`: drop selection and item creation; keep this authority server-side.
- Event families such as `BloodCastle.*`, `DevilSquare.*`, `ChaosCastle.*`, `Kanturu*`, and `Raklion*`: event-owned monster state and transitions.

## Lifecycle audit

- Data/script load and model/resource registration.
- Spawn packet/data, class/index lookup, object key allocation, position and initial action.
- Update/movement/interpolation and action/animation selection.
- Model, textures, effects, sound, shadows, attachments, and render-state changes.
- Hit/death presentation, despawn, map transition, object reuse, and cleanup.

## Architecture decisions

- Keep a generic creation path with narrow, documented specializations.
- Before making behavior data-driven, inventory every switch/default/fallback for the affected monster classes.
- Never move server AI, damage, drop, or reward authority into the client.
- Audit hot loops for per-frame lookup, allocation, state changes, and repeated resource loading.

## Unresolved until traced

- `CreateMonster` exists in multiple generic and event paths; the active dispatch order must be proven for each map/class.
- Client-named AI code may only animate received state; verify packets and callers before classifying it as decision logic.
