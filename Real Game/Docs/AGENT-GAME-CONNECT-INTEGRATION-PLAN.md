# AGENT-GAME-CONNECT — Integration plan (2026-10-24)

## Stages

| # | Stage | State |
|---|-------|-------|
| 0 | Inventory source + target | DONE — source empty; target mapped (registry, typed services, automation tests, python mirrors) |
| 1 | v1 read-only status interface | DONE — `Agent/AgentGameLink.*`, validated + tested (see TEST-REPORT) |
| 2 | Dev-workflow wiring (optional next) | NOT STARTED — e.g. console `afAgentStatus` for headless runs; only if an operator asks |
| 3 | Scoped write ops (proposals, never direct mutation) | NOT STARTED — requires owner-approved op design + per-op tests first |
| 4 | Transport / auth (only if a real remote caller exists) | NOT STARTED — no caller exists today; do not build speculatively |

## Dependency analysis

- v1 depends only on: `UAfterfallSubsystemRegistry` (existing), `LogAfterfall`
  (existing), automation-test framework (existing). No new modules, plugins,
  packages, or engine features. `Afterfall.Build.cs` unchanged.
- Blockers: none for v1. Stage 2+ blockers are product decisions (who calls,
  over what channel, with what credentials) — not technical unknowns.

## Minimal-slice rationale

The smallest slice that proves the boundary is real: an interface that
accepts exactly one safe op, refuses everything else with explicit codes,
cannot mutate state, compiles into the game module, and is covered by both
an automation test and a python mirror. Anything smaller (docs only) would
not satisfy completion criterion 6 (verifiable build + tests).

## Rollback

Delete `Source/Afterfall/Agent/`, `Source/Afterfall/Tests/AgentGameLinkTest.cpp`,
`Scripts/validate_agent_link.py`, revert the two `run_tests.sh` additions, and
(optionally) remove these four docs. No gameplay, config, content, or packaging
file is touched by the slice, so rollback cannot regress the game.
Rebuild after removal to restore the prior binary.
