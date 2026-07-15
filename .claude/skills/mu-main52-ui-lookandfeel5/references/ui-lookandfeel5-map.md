# UI and LookAndFeel5 architecture map

## Verified current behavior

- `source/WINHANDLE.cpp::CWINHANDLE::InitSize` sets `g_fScreenRate_x` and `g_fScreenRate_y` to constant `1.8f` whenever `LookAndFeel == 5`.
- It then derives the virtual dimensions as physical `WindowWidth/WindowHeight` divided by those rates. Consequently Look5 geometry remains approximately constant in physical pixels across resolutions while the virtual extent changes.
- Other screen modes derive scale from 640x480 and may use uniform aspect-preserving scaling; do not change their behavior as a side effect of fixing Look5.
- Mouse messages arrive in physical coordinates as `MouseRenderX/Y`; `WINHANDLE.cpp` converts them to `MouseX/Y` by dividing by `g_fScreenRate_x/y`.
- `source/NewUICommon.cpp::RenderImageF` passes virtual position/size into bitmap rendering while converting atlas source rectangles to normalized UVs.
- `source/CGMFrame.h` contains legacy 640x480 anchor/position macros and scale helpers.
- `source/NewUIMainFrameWindow.cpp` contains the primary Look5 HUD branch, dedicated atlas constants, fixed design sizes, many `LookAndFeel == 5` branches, and button hit tests.
- `source/Resolutions.*` loads resolution metadata, but `interface_scale_x/y` parsing is currently commented out and therefore is not an active responsive Look5 policy.

## Root cause to verify on the desktop branch

The public branch explains the reported symptom: constant Look5 scale plus fixed virtual sizes means a window or texture does not grow/shrink with resolution. The user's desktop branch contains newer UI work, so re-run this audit there before implementing.

## Verified history across the reference clients

- `0kju0/MuOnline-Main-5.2` computes `g_fScreenRate_x = WindowWidth / 640` and `g_fScreenRate_y = WindowHeight / 480` directly in `Winmain.cpp` after the resolution switch. This scales with resolution but stretches UI non-uniformly on non-4:3 displays.
- The historical `Source/Main5.2` inside `MUServerge/SRCMainGS` moves scaling into `CWINHANDLE::InitSize`: `ScreenType == 0` keeps independent 640x480 scaling, `ScreenType == 1` chooses one uniform minimum scale, and the remaining mode uses fixed threshold values.
- Current `MUServerge/main-5.2` adds a higher-priority `LookAndFeel == 5` branch that forces both rates to `1.8f`, bypassing every `ScreenType` formula. This is the direct public-branch cause of constant physical Look5 size.
- Do not blindly restore the oldest independent X/Y formula: it fixes size responsiveness while introducing aspect distortion. Reuse the newer centralized `InitSize` boundary, but give Look5 an explicit uniform responsive policy and safe-area/anchor behavior.
- Compare the user's newer desktop branch before editing because the Look5 HUD and call order may have advanced beyond the public branch.

## Target transform

Keep these concepts explicit:

- `drawableWidth`, `drawableHeight`: actual framebuffer/window pixels.
- `designWidth`, `designHeight`: Look5 reference canvas chosen from the audited assets/layout, not guessed per widget.
- `uiScale`: uniform policy such as `min(drawableWidth/designWidth, drawableHeight/designHeight)` unless the product intentionally uses height-based widescreen extension.
- `viewportOffsetX/Y`: letterbox/safe-area origin when the aspect ratios differ.
- `virtualWidth/Height`: visible UI extent when widescreen extension is allowed.
- `ToScreen(rect/point)` and `ToVirtual(mouse)`: exact forward/inverse transforms.

Choose one of two documented policies for Look5:

1. Fit canvas: the entire reference canvas scales uniformly and is centered; safe bars may remain.
2. Height-fit widescreen: scale from usable height, keep reference-height geometry, expose extra virtual width, and anchor side elements to the expanded edges.

Do not combine both policies ad hoc. Height-fit widescreen is often appropriate for MU HUDs, but confirm intended Look5 screenshots and assets first.

## Layout primitives

- Anchors: top-left, top-center, top-right, center, bottom-left, bottom-center, bottom-right.
- Safe area: configurable physical/DPI margins converted once to virtual units.
- Rect: authoritative layout rectangle shared by render and input.
- Size policies: fixed design size, proportional, fill, content, min/max constrained.
- Window chrome: nine-slice corners/edges/center; atlas UVs remain in source-pixel space.
- Text: scaled font selection or raster size, measured in the target coordinate system; wrapping and clipping use the same content rect.

## Migration order

1. Instrument resolution, drawable size, current rates, virtual extent, mouse transform, and LookAndFeel mode.
2. Add the Look5 transform without changing Look1-4.
3. Main HUD frame and EXP/gauge geometry.
4. Look5 buttons, hover/click rectangles, hotkeys, helper/shop controls.
5. Chat, buffs, notifications, party/pet, minimap, tooltips.
6. Inventory/character/storage/trade/shop window families and their shared slot grids.
7. Modal/message boxes, scrollable lists, edit controls, and native Win32 child controls.
8. Scissor regions and embedded 3D item/character previews.
9. Remove redundant resolution checks and duplicated constants after parity.

## Test matrix

- 4:3: 800x600, 1024x768.
- 16:9: 1280x720, 1366x768, 1600x900, 1920x1080, 2560x1440.
- 16:10: 1280x800, 1680x1050, 1920x1200.
- Ultrawide where supported: 2560x1080 and 3440x1440.
- Windowed/fullscreen transitions, resize where supported, DPI 100/125/150%, and taskbar/border differences.
- Open zero, one, two, and three right-side inventory-family windows.
- Check HUD, chat input, buffs, minimap, tooltips, all buttons, drag/scroll, item slots, modal focus, text clipping, and 3D previews.

For every case record: scale, virtual size, safe offsets, screenshot, mouse hit parity, clipping/scissor result, and any legacy branch entered.

## Additional UI risks

- `GetScreenWidth()` is not simply physical screen width; it subtracts occupied right-side NewUI windows. Do not use it as a generic drawable-size API.
- Fixed `640`/`480` values may represent intentional design coordinates, viewport setup, or obsolete assumptions. Classify before replacing.
- Native edit controls use physical Win32 placement and need an explicit virtual-to-screen conversion.
- Texture quality and geometry scaling are separate: larger UI may require higher-resolution source assets or mip/filter decisions, not different atlas UVs.
