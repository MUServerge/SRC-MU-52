---
name: mu-main52-character
description: Character architecture workflow for the MUServerge/main-5.2 MU Online Season 5.2 Win32 C++ client. Use for character creation and selection, CHARACTER_MACHINE, CharacterAttribute, classes, stats, equipment, skills, buffs, master level, movement, animation, model state, character UI, and character-related packets or calculations.
---

# MU Main 5.2 Character

## Establish the state flow

1. Read `references/character-map.md` before changing character behavior.
2. Trace the complete path: server packet or local input -> authoritative client state -> derived calculations -> animation/render state -> UI.
3. Identify the single source of truth for every affected stat. Do not create a second calculation beside `CHARACTER_MACHINE` or an existing subsystem.
4. Search existing class, equipment, skill, buff, master-level, inventory, and UI code before adding a helper or field.

## Preserve contracts

- Preserve class IDs, equipment slot meanings, stat widths, caps, packet layouts, action IDs, model IDs, and animation timing.
- Treat server-supplied identity, progression, inventory, combat outcomes, and eligibility as authoritative.
- Keep base state, derived state, and presentation state distinct even when legacy code stores them together.
- Maintain x86 ABI and serialization compatibility. Do not casually change packed structs, enum values, field order, or integer widths.
- Preserve original MU gameplay and visual behavior unless the request explicitly changes it.

## Change safely

- Reuse the existing character lifecycle and calculation entry points.
- Refactor only the touched path, in small reviewable steps. Add an adapter or extraction seam before moving global state.
- Centralize repeated stat or equipment logic only after proving the formulas and call order are equivalent.
- Validate initialization, packet hydration, recalculation triggers, equipment changes, death/respawn, map transitions, character switching, and teardown.
- Compare before/after values for representative classes and equipment sets. Verify UI values and world behavior against the same state.

## Report evidence

- Name the files and functions that own each state transition.
- Separate verified behavior from hypotheses and unresolved server-side dependencies.
- Record newly verified paths in `references/character-map.md` so the map evolves with the project.
