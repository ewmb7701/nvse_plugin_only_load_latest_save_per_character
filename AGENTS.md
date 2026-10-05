# Project context for coding agents

This is **Only Load Latest Save Per Character**, a native Win32 xNVSE plugin for standard Fallout New Vegas 1.4.0.525. Repository/project name: `nvse_plugin_only_load_latest_save_per_character`. Runtime DLL/log basename: `only_load_latest_save_per_character`. Current version: 0.5.1.

## Ownership and layout

- The user owns the public `README.md`. Do not recreate the former generated README or overwrite the user's documentation.
- Keep agent notes, disassembly captures, investigative scripts, and historical artifacts in `.codex/`, which is gitignored. Never ship or publish game-code captures.
- `.codex/README.md` contains the previous generated project overview. `.codex/hook-notes.md` records verified addresses, layouts, ABIs, and investigation evidence. Consult these when changing hooks.
- `.codex/read_game_code.py` is an optional read-only process inspection tool for the locally packed game executable. It is not a build or runtime dependency.
- `source/` contains the DLL and Visual Studio project; `tests/integration.cpp` is the native test harness. `scripts/build.ps1` and `scripts/package.ps1` are public development tools. `target/` and `dist/` are generated and gitignored.
- Historical packages/captures are under `.codex/dist/` and `.codex/analysis/`; they are not current releases.

## Intended behavior

Both title-menu Load and pause-menu Load share the StartMenu save list and hooks. Permit the first **visible** save for each character, using the smallest valid listindex. Match Stewie's Character Save Selector with case-sensitive save-header `pcName` comparisons. Same-name playthroughs share a group; missing/empty names share an unknown group. There is no fixed character limit or requirement for Stewie to be installed.

Older visible saves remain targetable, scrollable, selectable, and deletable. Block their load activation before the confirmation/Stewie skip-confirmation hook. Replace their normal click sound with `UIMenuCancel` and dim the save text to half its original opacity. Save mode remains unrestricted. Continue, quickload, console loading, and direct loads from other plugins are outside this menu-only feature.

Filtering or deleting a first save promotes the next visible save for that character. Keep the policy used by load interception, sound selection, and appearance consistent. Do not introduce a per-frame scan: attach opacity during row setup and refresh after native filtering/rebuild events. Preserve existing XML fade/brightness/color expressions and all input targets.

## Implementation map and constraints

- `main.cpp`: xNVSE exports, runtime validation, PostPostLoad installation.
- `game_ui.h` / `nvse_api.h`: minimal verified game layouts and public ABI.
- `load_policy.cpp`: shared per-character grouping and eligibility; no cached live pointers across events.
- `load_guard.cpp`: activation lookup at `007CEDA2`, sound-name lookup at `00A0B158`. Leave Stewie's `007CEDE5` hook intact.
- `row_appearance.cpp`: setup `007D6917`, mode `007D3C88`, rebuild/filter `0071A72A`, `0072A1E6`, `007D8FCA`. Leave highlighting at `007D6931` intact. Native refresh runs before our appearance update.
- Validate instruction signatures before patching, preserve calling conventions and stack cleanup, restore page protection, and flush the instruction cache. On conflicts, log and avoid overwriting unknown code.
- Supported save text children: `lb_saveload_prefix` and `lb_saveload_location` (vanilla and Vanilla UI Plus).

## Build, verification, and packaging

From the project root, run `./scripts/build.ps1 -Test` using Visual Studio's C++ workload (currently v145, Win32) and Windows SDK. The tests load the real DLL into a separate process with synthetic native call sites. Exit 77 means Windows occupied a required fixed address; the runner retries only that startup condition, never failed assertions.

Tests cover mixed characters, title/pause contexts, filtering/deletion promotion, sound, opacity, native ABI forwarding, and patch conflicts. Stubbed engine services do not prove actual rendering or in-game compatibility. The user has confirmed the working behavior through the 0.5.1 shared title/pause implementation; later changes still need appropriate verification. A historical bug confused equality (`7DEh`) with inequality (`7DFh`) in both implementation and test emulator: check SDK/operator definitions independently.

Run `./scripts/package.ps1` after a Release build. It packages the DLL and, if present, the user's public README. Exclude AGENTS.md, `.codex`, tests, captures, and agent notes from the player package. Never modify historical archives to imply they contain new builds.

In the surrounding development workspace, the installed MO2 mod is `mods/Only Load Latest Save Per Character`, enabled in `profiles/Default/modlist.txt`. These paths are outside this standalone project's root and may not exist in another checkout. Close the game before replacing a locked DLL; do not terminate the user's game. When deploying, verify the installed DLL matches the build and avoid leaving duplicate plugin DLLs under old names. The older script-based save restrictions must stay disabled; the pause-menu Load-button disabling mod must be disabled to access that screen.

Reference projects in this workspace: sibling `ewmb-nvse-reference` and workspace `agents/Stewie Tweaks 10.00 Source`. They are references, not dependencies or files to include in this repository. Do not change runtime behavior as part of documentation or repository cleanup.
