# Verified persistence and service map

## Ownership

| Component | Primary responsibility |
|---|---|
| Client | Input and presentation; never authoritative for gameplay state |
| ConnectServer | Server list, address routing, version/update responses |
| JoinServer | Account authentication, duplicate-session coordination, map-move authorization |
| GameServer | Live character/gameplay authority, validation, mutation, orchestration |
| DataServer | Character, inventory, warehouse, quest, master, event, guild, shop, and custom-system persistence |

## Character save fan-out

`GDCharacterInfoSaveSend` in GameServer `DSProtocol.cpp` fans one logical character save into multiple DataServer messages, including core character data plus several conditional systems such as inventory/pets, warehouse-related state, quests, master data, cash/shop state, lucky/pentagram data, personal-shop values, event inventory, and Muun data.

Do not assume this fan-out is one database transaction. For a touched feature, list every emitted sub-save, its acknowledgement behavior, failure visibility, retry behavior, and load-time reconciliation. Define the acceptable crash invariant before refactoring.

## Active identity registries

- DataServer `CharacterManager.*` keeps an in-process character-name registry tied to account, user index, and server code.
- JoinServer maintains account session ownership and duplicate-account behavior.
- GameServer `Reconnect.*` keeps temporary reconnect data in memory keyed by character identity.

Audit canonicalization, timeouts, duplicate live sessions, stale cleanup, server moves, process restarts, and multi-instance behavior together. None of these registries alone proves global ownership.

## Warehouse evidence

DataServer `Warehouse.cpp` supports normal, extended, and guild warehouse flows. Item blobs use binary parameter binding in places, while account/guild identifiers and other values are also assembled through formatted SQL. Guild warehouse exclusion includes process-local state, and some database `InUse` logic is commented in the inspected source. Items/money and password updates can be separate statements.

For warehouse work:

1. Parameterize every identifier and value.
2. Define a database-backed or otherwise cross-instance lease if multiple DataServers can serve the same warehouse.
3. Couple item, money, password, and ownership changes transactionally where the invariant requires it.
4. Make open/save/close retries idempotent and ensure disconnect releases or expires ownership.

## High-risk consistency paths

- trade accept/cancel and disconnect;
- chaos mix and material consumption;
- shop buy/sell and currency updates;
- reward/event claim and reconnect;
- inventory move plus item save;
- warehouse or guild warehouse open/save/close;
- character save during map-server transfer;
- login/load while an older session or save is still active.

For each path, identify the commit point, durable evidence, duplicate-prevention key or state precondition, compensation behavior, and client response timing.
