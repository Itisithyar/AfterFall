# AGENT-GAME-CONNECT — Test report (2026-10-24)

Slice under test: v1 read-only status interface
(`Source/Afterfall/Agent/AgentGameLink.*` + `Tests/AgentGameLinkTest.cpp`).
No gameplay, config, content, or packaging file was modified.

## Results

| Check | Command | Result |
|---|---|---|
| Python contract mirror (no engine) | `python3 Scripts/validate_agent_link.py` (cwd `Real Game/`) | ALL PASS (22 checks: allowlist, codes, 4KB cap, shipping gates, no Tick/network/gameplay refs, read-only registry, isolated test) |
| Targeted automation test | `UnrealEditor-Cmd.exe RealGame.uproject ... "Automation RunTests Afterfall.Agent.Link; Quit"` | `Result={Success}`, `TEST COMPLETE. EXIT CODE: 0` (`Saved/Logs/RealGame.log`, 22:07:29Z) |
| Full automation regression | `... "Automation RunTests Afterfall; Quit"` | **37/37 `Result={Success}`, 0 fail**, `TEST COMPLETE. EXIT CODE: 0` (22:12:52Z). Includes the new `Afterfall.Agent.Link` alongside all 36 pre-existing tests — no regressions |
| Functional script suite | `bash Scripts/run_tests.sh` (cwd `Real Game/`) | ALL CHECKS PASS, incl. new `[PASS] agent link contract` |
| Editor build | `Build.bat AfterfallEditor Win64 Development RealGame.uproject` | `Result: Succeeded`, exit 0 (unity-blob rebuild + relink, ~53s incremental) |

Note: one `run_tests.sh` invocation from the repo root (wrong cwd) reported
`[FAIL] agent link contract` because that script uses tree-relative paths.
Re-running from `Real Game/` (its documented cwd) passes. No code fault.

## Failure-isolation checks (executed, not mocked)

- Unknown op (`spawn-zombie`), op prefix (`statu`), empty op, oversize payload
  (4097 chars): all refused with exact codes — automation test, passed.
- Null registry and shutting-down registry: clean refusals — automation test, passed.
- Shipping gate: verified statically (both entry points short-circuit under
  `UE_BUILD_SHIPPING`) — Editor automation cannot execute Shipping code, so
  this is a code-inspection result, stated as such.
- Disconnection/timeout: not applicable — v1 has no transport, no tick, no
  listeners. Nothing to disconnect; documented in SECURITY.md rather than emulated.

## Measurements

- Per-frame cost of the slice: zero — no Tick, no timers, no delegates, no
  gameplay references (verified by inspection + mirror). Runs only when called
  from dev tooling/tests.
- Memory: no state held (pure functions; audit line only). No new modules,
  plugins, or third-party dependencies (`Afterfall.Build.cs` unchanged).
- Request latency: single registry count read; full automation run containing
  the new test completed in the normal suite window (no outlier).

## Known limitations

- Runtime (NPC/event) capabilities: not implemented — no caller exists and no
  transport was built (INTEGRATION-PLAN stages 3–4, owner-gated).
- `Agent Game Connect` source folder remains empty; no orchestration side was
  delivered or faked.
- Physically observed UI/phone testing: none — all runs headless, as with the
  rest of this project's evidence.

## Rollback

Delete `Source/Afterfall/Agent/`, `Source/Afterfall/Tests/AgentGameLinkTest.cpp`,
`Scripts/validate_agent_link.py`, revert the two added blocks in
`Scripts/run_tests.sh`, optionally remove the four `AGENT-GAME-CONNECT-*.md`
docs, rebuild. Zero gameplay impact by construction.
