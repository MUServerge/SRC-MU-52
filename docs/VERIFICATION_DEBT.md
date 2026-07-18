# Verification debt

This register tracks changes that lack reproducible Windows build or runtime evidence. It is not a defect list and does not claim a regression exists.

## Status definitions

- `build-unverified`: no clean build evidence exists for the affected revision/configuration.
- `runtime-unverified`: the affected behavior and nearest fallback/error/reload/shutdown path have not run.
- `partially-verified`: some required checks passed, but the complete gate is open.
- `verified`: evidence is recorded for the exact revision; later edits to the same path reopen the debt.

## Open register

| Change | Area | Build | Runtime | Required evidence | Owner/closure |
|---|---|---|---|---|---|
| PRs #13–#23 | FPS, frame pacing, scheduler, renderer refactors | build-unverified | runtime-unverified | SHA-pinned range audit; clean Main Release/Debug Win32; 60/120 FPS empty+dense scenes; map change, reconnect, shutdown | Pending desktop validation |
| PR #25 | dangling timing/shader symbols and `glprocs.lib` linker cleanup | build-unverified | runtime-unverified | clean Main Release/Debug Win32; login/select/world/map/reconnect/exit; shader/VBO visual smoke | Pending desktop validation |
| PR #27 | workflow documentation and read-only PowerShell audit tools | not product-code applicable | runtime-unverified | PowerShell parser/runtime execution from a clean checkout; confirm reports only and no source mutation | Pending desktop validation |

## Evidence record

When closing debt, append:

| Date | Change/SHA | Environment | Evidence path/link | Result | Remaining debt |
|---|---|---|---|---|---|

Never mark a row verified using evidence from a different commit, dirty worktree, configuration, runtime Data/config set, or later-conflicted PR head.

