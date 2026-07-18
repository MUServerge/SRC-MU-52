# Events and rewards architecture map

## Verified shared entry points

- `source/CSEventMatch.h/.cpp`: `CSBaseMatch` countdown, event type, match time, kill counts, result list, and derived Devil Square/Cursed Temple/DoppelGanger result UI behavior.
- `source/wsclientinline.h`: event game-state/result packets and reward-related presentation paths; inspect exact packet handlers for each event.
- `source/NewBloodCastleSystem.*`, `source/NewChaosCastleSystem.*`: event-specific client state.
- `source/MatchEvent.*`, `source/CSChaosCastle.*`: shared or older event paths.
- `source/Event.h`: seasonal event model/effect/monster presentation.
- Entry/result UI families: `NewUIBloodCastleEnter.*`, `NewUIDevilSquareEnter.*`, `NewUICursedTemple*`, `NewUIDoppelGanger*`, `NewUIKanturuEvent.*`.
- Map/event families: `GMDoppelGanger*`, `GMEmpireGuardian*`, `GM_Raklion*`, Kanturu direction/map code, and related `GM*` modules.

## Verified server-side reference (`MUServerge/SRCMainGS`)

- `Source/GameServer/GameServer/BloodCastle.*`, `DevilSquare.*`, `ChaosCastle.*`, `Kanturu*`, and `Raklion*`: authoritative event state and event-specific rules.
- `EventStart.*`, `CEventName.*`, `CustomEventTime.*`: scheduling and shared/custom event timing candidates.
- `ItemBag.*`, `ItemBagManager.*`, `ItemBagEx.*`, `CustomEventDrop.*`, `ItemDrop.*`: authoritative reward/drop selection and item creation paths.
- `QuestReward.*`, `QuestWorldReward.*`, `BotReward.*`: other authoritative reward implementations that establish reusable patterns.
- `Protocol.cpp`: client request dispatch and server response entry points; use it to pair client packets with authoritative handlers.
- `Map.*`, `MapManager.*`, `MonsterSetBase.*`, and event monster modules: map, spawn, and phase dependencies.

The server tree also contains an older `Source/Main5.2` client copy. Use it only for historical comparison; treat `MUServerge/main-5.2` as the current client unless the user states otherwise.

## Event audit matrix

For each event, record: event/map ID | entry request/response | phases | transition packet | timer source | score/kill state | monster specialization | UI | result packet | reward display | server owner.

Cover at least:

- Blood Castle
- Devil Square
- Chaos Castle
- Cursed Temple
- DoppelGanger
- Empire Guardian
- Kanturu
- Raklion
- Seasonal/custom events used by the project

## Reward rules

- Reward eligibility, selection, ranking, item/currency creation, inventory mutation, and drops belong to the server.
- Client code may render a result or newly received inventory state but must not synthesize a successful grant.
- If a reward table is absent here, request the matching server repository/files rather than inventing it.
- Validate result counts against fixed arrays such as the match-result storage before copying or rendering.

## Mandatory transitions

- Not entered/denied -> waiting -> countdown -> active -> success/failure/timeout -> result -> exit/cleanup.
- Reconnect during every phase, late or duplicated packets, map exit, repeated event sessions, missing result, and clock drift.

## Unresolved until traced

- Similar event UI does not prove identical state or reward rules.
- Text containing “reward” in inventory/UI files may only describe display; trace its packet and server source.
