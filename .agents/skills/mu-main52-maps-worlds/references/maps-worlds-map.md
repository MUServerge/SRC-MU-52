# Maps and worlds architecture map

## Current client (`MUServerge/main-5.2`)

- `source/MapManager.*`: client map metadata/selection entry point.
- `source/w_BaseMap.h`, `source/w_MapProcess.*`, `source/w_MapHeaders.h`: generic and specialized world processing.
- `source/ZzzScene.cpp`, terrain/world loaders, and `GM*` modules: scene integration, resources, lighting, weather, models, and effects; trace per requested map.
- `source/CSMapServer.*`: map-server/client routing support.
- `source/NewUIMiniMap.*`, `source/UIMapName.*`: minimap and map-name presentation.
- `source/CKANTURUDirection.*`, `GM_Kanturu_*`, `GM_Raklion.*`, and other `GM*` families: explicit map/event specializations.
- `source/ZzzInfomation.h`: gate script entry point and shared map-dependent data.

## Server reference (`MUServerge/SRCMainGS`)

- `Source/GameServer/GameServer/Map.*`: authoritative map grid/items and attribute checks.
- `MapPath.*`: authoritative pathfinding and movement-grid behavior.
- `MapManager.*`: per-map configuration including PK/outlaw rules, view range, experience rates, normal/excellent/set/socket drop rates, helper/custom actions, PK drops, death gate, and name.
- `MapServerManager.*`: map-to-server ownership and routing.
- `Gate.*`, move/teleport code, and `gObjMoveGate` callers: authoritative gate destination and access path.
- `MonsterSetBase.*`, `MonsterManager.*`: map spawn definitions and monster ownership.
- `Viewport.cpp`: map-aware visibility using configured view range.
- `ItemDrop.*`, `CustomEventDrop.*`: map restrictions and map-aware drop placement/attributes.
- Event modules (`BloodCastle`, `DevilSquare`, `ChaosCastle`, `Kanturu`, `Raklion`, custom events): event-map transitions and restrictions.

The server repository contains an older `Source/Main5.2` client snapshot. Use it to understand historical pairing, not as the current client source of truth.

## Mandatory parity matrix

For each changed world record:

- Numeric map/world ID and display name.
- Server registration and map-server group.
- Client loader/base-map or specialization.
- Login/default spawn, move commands, gates, death gate, and exit route.
- Attribute files/bits: walkable, blocked, safe-zone, water/height or special flags.
- Monster/NPC spawn sources and event ownership.
- Experience, PK, helper, view-range, and item-drop rules.
- Terrain, objects, textures, sky/light/weather, effects, music/sound, minimap.
- Packet fields and reconnect/map-change sequence.

## Risk checks

- ID collision or client/server ID disagreement.
- Valid gate targeting an unloaded or unauthorized map.
- Coordinate outside the loaded grid or on blocked/special attributes.
- Client visual walkability disagreeing with server collision.
- Event map accidentally inheriting normal-map PK/drop/teleport rules.
- Repeated resource loading, leaked world resources, excessive draw calls, or map-specific per-frame allocations.
