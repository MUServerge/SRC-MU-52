# Verified skills and combat topology

## Client

| Area | Verified files and role |
|---|---|
| Input and cast selection | `source/ZzzInterface.cpp`; selects targets and calls request helpers |
| Packet construction | `source/wsclientinline.h`; attack `0x11`, skill `0x19`, cancel `0x1B`, teleport `0x1C`, multi-target `0x1D/DB`, duration `0x1E` paths |
| Receive and presentation | `source/WSclient.cpp/.h`; skill lists, skill actions, damage, buffs, finish/status, master-skill responses |
| Client skill data | `source/ZzzInfomation.cpp/.h`; fixed `MAX_SKILLS` `Skill.bmd` loader and `SkillAttribute` |
| Client eligibility/cooldown | `source/SkillManager.cpp`; presentation-side requirements, distance, and delay |
| UI | `source/NewUIMainFrameWindow.cpp`, `NewUIHotKey.cpp`, `NewUIMasterSkillTree.cpp` |
| Buff presentation | `source/w_BuffStateSystem.*`, `w_BuffScriptLoader.cpp`, `w_BuffTimeControl.*`, `w_BuffStateValueControl.*` |
| Animation/effects | `ZzzInterface.cpp`, `WSclient.cpp`, character/object/effect modules, and map/monster specializations |

The client applies a local visual cooldown and contains extensive skill-to-action/effect switches. These are responsiveness and presentation layers; they do not authorize a hit or damage. Normal damage reception uses server-supplied current HP/SD when `PROTO_EXTRA` is active, while some special map paths retain separate presentation logic and require regression checks.

## GameServer

| Area | Verified files and role |
|---|---|
| Dispatch | `Protocol.cpp/.h`; attack/skill/multi/duration/master requests |
| Normal combat | `Attack.cpp/.h`; target legality, hit/miss, damage, defense, shield, reflect, durability, death handoff |
| Skill execution | `SkillManager.cpp/.h`; ownership, requirements, delay, range/radius, mana/BP, `RunningSkill`, individual skills and packets |
| Skill definitions | `Data/Skill/Skill.txt` path loaded by `ServerInfo.cpp`; `SkillDamage.txt` applies user/monster rates |
| Effects | `EffectManager.cpp/.h`, `Effect.cpp/.h`; group/stacking, derived options, expiry, DOT, view/party packets, serialization |
| Master skills | `MasterSkillTree.cpp/.h`; prerequisites, replacement, values, upgrade, packets, DataServer load/save |
| Anti-speed | `HackSkillCheck.cpp/.h`; configurable speed/count heuristic in addition to canonical skill delay |
| Load/rebuild | `ObjectManager.cpp`, `DSProtocol.cpp`; skills/effects hydrate and derived attributes rebuild |
| Automation | `CustomAttack.cpp` and monster/event callers; must converge on the same authoritative managers |

## Configuration evidence

The client project defines `MAIN_UPDATE=603` and `PROTO_EXTRA`. GameServer and DataServer projects contain multiple configurations. `Release_EX603`/`Debug_EX603` define update 603; other configurations define 401 or 803. Select and record the actual configuration before interpreting conditional packet structs or maximum skill/effect layouts.

For the 603 pairing, client `MAX_SKILLS` is 650 while GameServer `MAX_SKILL` is 622. This need not be a bug by itself, but it makes explicit ID-range and data-parity checks mandatory.

## Persistence boundaries

- Normal skill list and saveable effects are serialized in the character save packet to DataServer.
- Effects use 13-byte records; server configuration determines whether an effect is saveable and how its time is interpreted.
- Master level/tree uses a separate DataServer request/save path.
- One logical character save fans out into multiple persistence messages; master skill/effect durability across crashes must be described from the actual save timing, not assumed.
