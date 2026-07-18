# Social Systems Source Map

Use this as an orientation map, then verify the active branch and compile-time flags before changing code.

## Client: `MUServerge/main-5.2`

| Domain | Primary files and entry points |
|---|---|
| Party | `source/PartyManager.*`, `NewUIPartyInfoWindow.*`, `NewUIPartyListWindow.*`; senders in party UI/commands; receives in `source/WSclient.cpp` around the party handlers |
| Guild/union | `source/GuildCache.*`, `GuildManager.*`, `UIGuild*`, `NewUIGuildInfoWindow.*`, `NewUIGuildMakeWindow.*`; guild, relation, and war receives in `WSclient.cpp` |
| Chat/whisper | `source/NewUIChatInputBox.*`, `NewUIChatLogWindow.*`, `CGMHeadChat.*`; request construction in chat input/commands and receives in `WSclient.cpp` |
| Friends/mail/chat rooms | `source/NewUIFriendWindow.*`, related flows in `source/UIWindows.cpp`; friend, memo, invitation, and room receives in `WSclient.cpp` |
| Duel | `source/DuelMgr.*`, `GMDuelArena.*`, `NewUIDuel*`; duel request/result/score/HP/watch receives in `WSclient.cpp` |
| Gens | `source/NewUIGensRanking.*`, Gens NPC/dialogue code, and Gens receives in `WSclient.cpp` |

`WSclient.cpp` is a transport/dispatch concentration point, not the desired owner of new domain policy. Reuse established send/receive shapes, but place new policy in the narrowest existing manager when safe.

## GameServer: `MUServerge/SRCMainGS`

| Domain | Primary files | Authority |
|---|---|---|
| Party | `Source/GameServer/GameServer/Party.*` | Live party create/destroy, add/delete, leader, list/life updates |
| Party matching | `.../PartyMatching.*` plus DataServer counterpart | GS validates player/session; DS stores and routes matching state |
| Guild | `.../Guild.*`, `GuildClass.*` | Live membership, status, list, war-facing state |
| Guild matching | `.../GuildMatching.*` plus DataServer counterpart | Application/recruitment workflow, separate from live membership |
| Alliance/union | `.../Union.*`, `UnionInfo.*` | Relationship cache/routing; verify DS/SQL acknowledgement for durable changes |
| Chat | `.../ChatManager.*` and protocol dispatch | Channel validation, routing, filtering, and announcements |
| Duel | `.../Duel.*` | Arena/fighter/spectator state machine, result and cleanup |
| Gens | `.../GensSystem.*` | Live family/rank/contribution, PvP/victim/reward interaction |

## DataServer and SQL

| Domain | Primary files/data |
|---|---|
| Guild | `Source/DataServer/DataServer/GuildManager.*`, `GuildMatching.*` and related guild/union protocol handlers |
| Party matching | `Source/DataServer/DataServer/PartyMatching.*` |
| Gens | `Source/DataServer/DataServer/GensSystem.*`; `Gens_Rank`, `Gens_Reward`, and rank procedures |
| Friends/mail | `Source/DataServer/DataServer/CSProtocol.*`; `T_FriendMain`, `T_FriendList`, `T_FriendMail` and `WZ_*Friend*`/mail procedures |

JoinServer handles account/login concerns in this suite. Do not move friends or mail there merely because some MU distributions use a separate friend/chat service; this repository routes them through DataServer `CSProtocol`.

## Required trace records

For substantial work, record:

1. Packet header/subcode, direction, packed size, and every fixed-width name/text field.
2. UI/cache source and all affected client recipients.
3. GameServer owner, permission checks, live-state mutation, and cleanup hook.
4. DataServer request/reply and tables/procedures when persistent.
5. Stable identity versus transient connection/server indices.
6. Failure behavior for disconnect, timeout, duplicate, late callback, and service restart.
7. Cross-domain effects on combat, events, rewards, maps, UI, or economy.

## Reuse-first search terms

Search both repositories before implementing: feature class/function name, packet struct, header/subcode, UI message ID, SQL procedure/table, `CloseClient`, map-server move, connect/disconnect callbacks, member status constants, broadcast helpers, and existing bounds/text-copy utilities.
