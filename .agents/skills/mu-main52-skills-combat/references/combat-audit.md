# Combat parity and audit worksheet

## Per-skill parity

| Contract | Client | GameServer | Evidence/result |
|---|---|---|---|
| Skill/base/master IDs | | | |
| Effective build guards | | | |
| Learn/replace prerequisites | | | |
| Packet head/subhead/layout | | | |
| Target source and max count | | | |
| Range/radius/frustum | | | |
| Delay and anti-speed | | | |
| Mana/BP/ammo | | | |
| Class/equipment/map/event rules | | | |
| Damage type and formula path | | | |
| Buff/effect ID and group | | | |
| Action/animation/sound/effect | | | |
| Result/damage packets | | | |
| Save/reconnect behavior | | | |

## Formula ledger

Record integer values at each stage for a fixed attacker, target, equipment set, random seed or forced roll, and configuration:

| Stage | Before | Operation/config | After |
|---|---:|---|---:|
| Base min/max | | | |
| Skill/master additions | | | |
| Item/effect modifiers | | | |
| Weapon/durability | | | |
| Roll/critical/excellent | | | |
| PvP/PvE/skill rate | | | |
| Defense/reduction | | | |
| Double/combo | | | |
| Shield/HP split | | | |
| Reflect/absorb/DOT | | | |
| Final HP/SD/death | | | |

## Confirmed audit candidates

Do not silently fix these; reproduce them in the selected build and add regression vectors first.

1. `Attack.cpp::MissCheckPvP` checks a level difference of `>=100` before `>=200` and `>=300`, making the latter branches unreachable. It also needs an explicit zero-denominator policy when both PvP rates are zero.
2. `SkillManager.cpp::CGMultiSkillAttackRecv` clamps `count` to five, but `ProtocolCore` does not pass the received byte size to this handler. Validate the variable packet structurally before iterating target entries.
3. `MasterSkillTree.cpp::GetMasterSkillValue(index, level)` indexes the configured value array directly. Validate stored and requested levels centrally, especially after DataServer load.
4. Client skill-list and other receive paths use count/index fields to access arrays. Require packet-size and index validation before updating `CharacterAttribute` or UI state.
5. `HackSkillCheck.cpp` includes absolute `GetTickCount()` addition comparisons, while canonical skill delay uses elapsed subtraction. Audit long-uptime wrap behavior and keep the two responsibilities separate.
6. Client skill, master-tree, and buff loaders rely on fixed/raw binary layouts. Check every `fread` result, file length, count, checksum, allocation bound, and struct version before consuming data.

## Balance-change acceptance

Before accepting a combat formula change, provide:

- exact old and new formulas with integer/rounding order;
- at least one low, mid, and high-stat vector per affected class;
- PvE and PvP vectors where applicable;
- critical/excellent/double/combo and buffed/unbuffed variants;
- maximum-value/overflow checks;
- expected packet-visible HP/SD and client display;
- confirmation that unrelated skills remain byte-for-byte or value-for-value equivalent.
