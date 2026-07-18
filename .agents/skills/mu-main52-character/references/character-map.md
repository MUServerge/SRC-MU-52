# Character architecture map

## Verified entry points

- `source/ZzzInfomation.h`: `CHARACTER_MACHINE`, `Character`, `Equipment`, input/output combat fields, `CalculateAll`, individual stat calculations, equipment checks, and global `CharacterMachine` / `CharacterAttribute` pointers.
- `source/CharacterManager.cpp`: character-machine implementation and calculations; verify each formula and caller before changes.
- `source/Winmain.cpp`: creation and initialization of the global character-machine state.
- `source/ZzzCharacter.h`: character and monster creation plus client character rendering entry points.
- `source/ZzzScene.cpp`: scene/update/render integration.
- `source/SkillManager.cpp`, `source/CAIController.cpp`: skill and action consumers.
- `source/ZzzInventory.cpp`, `source/NewUIMyInventory.cpp`, `source/NewUICharacterInfoWindow.cpp`: equipment/inventory/stat UI consumers.
- `source/wsclientinline.h`: packet-driven character state; inspect exact handlers and structs for the requested path.
- `source/NewUIMasterSkillTree.*`, `source/CGMResetManager.cpp`, `source/CSPetSystem.cpp`, `source/GIPetManager.cpp`, `source/MonkSystem.cpp`: specialized progression or state consumers.

## Mandatory audit

- Lifecycle: allocation -> init -> login/select -> world enter -> map change -> death/respawn -> logout/teardown.
- State: server-authoritative, local base, equipment modifiers, buffs/skills, master-level modifiers, derived totals, render/UI copies.
- Recalculation: callers, order, frequency, dirty triggers, integer overflow/underflow, class-specific formulas.
- Presentation: action/animation, model/equipment appearance, name/status UI, stat window, comparison values.
- Protocol: packet structs, lengths, signedness, endianness assumptions, serial/order handling, reconnect hydration.

## Verified server-side reference (`MUServerge/SRCMainGS`)

- `Source/GameServer/GameServer/User.*`: authoritative player object fields and lifecycle state.
- `ObjectManager.*`: `CharacterCalcAttribute` and related authoritative recalculation entry points; trace callers and formula dependencies.
- `DefaultClassInfo.*`, `CharacterAdvance.*`, `ServerInfo.*`: class bases, progression, and configured character rules.
- `Protocol.cpp`: character selection/info, level-up point requests, and `GCNewCharacterInfoSend` / `GCNewCharacterCalcSend` views sent to the client.
- `DSProtocol.*` plus `Source/DataServer/DataServer/CharacterManager.*`: persistence load/save boundary.
- `ItemManager.*`, `ItemOption.*`, `SetItemOption.*`, `SocketItemOption.*`, `JewelOfHarmonyOption.*`, `380ItemOption.*`: equipment-derived authoritative modifiers.

Use the server-calculated view packets to compare client display calculations, but preserve server authority for effective combat and persisted state.

## Unresolved until traced

- Do not assume every `CHARACTER_ATTRIBUTE` field is authoritative or every `CHARACTER_MACHINE` output is used.
- Do not assume UI-displayed values and combat-effective values share the same rounding path.
- Server formula and anti-cheat behavior require the matching server source or protocol evidence.
