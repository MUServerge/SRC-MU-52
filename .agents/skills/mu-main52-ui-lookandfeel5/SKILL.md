---
name: mu-main52-ui-lookandfeel5
description: Responsive UI architecture workflow for the MUServerge/main-5.2 Win32 C++ client, with priority on LookAndFeel5. Use for HUDs, windows, textures and atlases, layout, anchors, resolution and aspect-ratio scaling, safe areas, fonts, text measurement, mouse/input hit testing, tooltips, scissor and viewport state, 3D item previews, NewUI, UI controls, and UI modernization without changing gameplay.
---

# MU Main 5.2 UI and LookAndFeel5

## Establish one coordinate contract

1. Read `references/ui-lookandfeel5-map.md` before changing UI.
2. Trace physical window pixels -> UI scale/viewport -> virtual design coordinates -> layout/anchors -> rendering/text -> inverse input transform/hit test.
3. Use one authoritative transform for textures, text, controls, mouse, tooltip bounds, scissor, viewport, and embedded 3D previews.
4. Do not mix physical pixels, legacy 640x480 units, and Look5 design units inside one component without explicit conversion.

## Responsive layout rules

- Define an explicit Look5 design canvas and scale policy. Derive scale from the current drawable size; do not lock it to a constant such as `1.8f`.
- Prefer uniform scale to preserve texture and circle proportions. Represent remaining widescreen space as safe-area offsets/virtual extent.
- Anchor HUD elements to left, center, right, top, or bottom according to their intent. Do not simulate anchoring with scattered resolution checks.
- Separate position, layout size, render size, and atlas UV size. Resolution scales geometry, never UV coordinates.
- Use nine-slice/tiled frames for resizable windows; do not stretch decorative borders or low-resolution atlas corners.
- Apply min/max sizes and readable font-scale limits where pure proportional scaling would make content unusable.
- Pixel-snap thin lines and text origins only after the common transform is applied.

## Preserve input and state parity

- Convert physical mouse coordinates to the same virtual space used by layout before hit testing.
- Ensure button visual bounds, hover bounds, click bounds, tooltip anchors, and drag bounds are derived from one rectangle.
- Transform OpenGL scissor rectangles with Y inversion and DPI/scale exactly once.
- Save and restore viewport, projection, scissor, blend, depth, and camera state around UI 3D previews.
- Recompute layout on resolution/fullscreen/window-size changes; never retain stale cached rectangles.

## Migrate incrementally

1. Preserve legacy LookAndFeel1-4 behavior unless explicitly included.
2. Introduce or repair the transform at the Look5 boundary first.
3. Migrate the main HUD, buttons, EXP/gauges, chat/notifications, modal windows, inventory-family windows, tooltips, and previews in small groups.
4. Remove per-resolution branches only after screenshot and input parity is proven.
5. Reuse existing NewUI controls and render helpers when they obey the coordinate contract; adapt them rather than creating a parallel UI framework.

## Validate

- Test the matrix in `references/ui-lookandfeel5-map.md`.
- Capture screenshots with identical game state and compare alignment, size, clipping, text, UVs, and safe areas.
- Verify every migrated control at corners and edges, not only its visual appearance.
- Measure UI draw calls, texture binds, allocations, and layout work; cache only data that is invalidated correctly.
- Preserve gameplay, packet behavior, inventory slot semantics, and original Look5 visual identity.

## Report evidence

- Name the coordinate space of every changed value and the owner of each conversion.
- Provide before/after layout and input evidence across representative aspect ratios.
- Record verified paths and remaining fixed-coordinate islands in `references/ui-lookandfeel5-map.md`.
