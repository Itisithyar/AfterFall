# Agent 1 — Afterfall AgentGameLink verification (2026-10-24)

- **Role/scope taken:** verification-only pass over the existing AgentGameLink
  implementation. No source file was modified, no cache cleared, no build
  configuration changed, nothing committed.
- **Working tree verified:** `C:\Users\mckee\Desktop\Come Up Game` (branch
  `master`, HEAD `7f2bc2d`), project `Real Game/RealGame.uproject`.
- **Raw evidence:** `Tony/agent1-evidence-2026-10-24/` (unmodified command
  output; every claim below cites a file there or a log line inside one).
- **Coordination note:** no handoff or ownership record existed in the Tony
  folder when this pass began. Five other agent reports appeared in that folder
  **while this pass was running** (17:32–17:35); they are reconciled in §8. This
  file is the handoff, not an acknowledgment.
- **Post-verification update (18:3x):** two items in this report have since
  changed. The shutting-down gap (§5.2) is closed by a test addition with
  positive runtime evidence, and the Shipping string check (§5.3) used a method
  that was wrong — corrected below, conclusion unchanged. Details: §9.

Labels used strictly: **VERIFIED** (ran, observed), **FAILED** (ran, observed a
failure), **BLOCKED** (could not run, with the reason), **NOT RUN** (never
attempted), **UNVERIFIED** (inspectable only, or contradicted by evidence).

---

## 1. Repository state (measured, not assumed)

| Fact | Observation |
|---|---|
| Root | `C:\Users\mckee\Desktop\Come Up Game` |
| Branch / HEAD | `master` / `7f2bc2d6897d84ce925959da5b136a747a76f61a` ("Point cook at the real map; quarantine broken template anim assets", 2026-10-23) |
| Tracked modifications | 14 files, `1855 insertions(+), 34 deletions(-)` — same total before and after this pass |
| Untracked entries | 50 |
| **Provenance of the slice** | `Real Game/` is **entirely untracked** (`?? "Real Game/"`). `git ls-files` returns nothing for `Real Game/Source/Afterfall/Agent`, `Real Game/Tests/AgentGameLinkTest.cpp`, `Real Game/Scripts/validate_agent_link.py`, or the four `AGENT-GAME-CONNECT-*.md` docs. |

Consequence: the AgentGameLink work exists **only as working-tree files**. It has
no commit, no author trail, and no `git revert`/`git stash` rollback path. Any
statement of the form "the implementation was delivered" can only mean "present
on disk", not "merged".

## 2. What exists (file inventory)

| Path | Bytes | mtime (local) |
|---|---|---|
| `Real Game/Source/Afterfall/Agent/AgentGameLink.h` | 2373 | 16:51 |
| `Real Game/Source/Afterfall/Agent/AgentGameLink.cpp` | 1954 | 16:51 |
| `Real Game/Source/Afterfall/Tests/AgentGameLinkTest.cpp` | 4025 | 16:52 |
| `Real Game/Scripts/validate_agent_link.py` | 3174 | 16:52 |
| `Real Game/Docs/AGENT-GAME-CONNECT-{ARCHITECTURE,INTEGRATION-PLAN,SECURITY,TEST-REPORT}.md` | — | 16:52 |
| `Real Game/Scripts/run_tests.sh` additions | lines 90–93 (`check` for the four files), line 122 (`[PASS] agent link contract`) | — |

The header claims "pure functions only, no UObject, no Tick"; the code matches:
it reads `UAfterfallSubsystemRegistry::Num()` / `IsShuttingDown()` and writes one
`UE_LOG` line. No mutation of the registry exists in the interface.

## 3. Verification matrix

| # | Check | Command / method | Result | Label |
|---|---|---|---|---|
| 1 | UBT discovers the new sources | `Intermediate/Build/Win64/x64/AfterfallEditor/Development/Makefile.bin` contains `AgentGameLink`; `.../UnrealEditor/Development/Afterfall/Module.Afterfall.cpp` references it twice | Source is in the module's makefile, not merely on disk | **VERIFIED** |
| 2 | Editor build | `Build.bat AfterfallEditor Win64 Development "…/RealGame.uproject" -WaitMutex` → `editor-build.log` | `Result: Succeeded`, `BUILD_EXIT:0`, shell exit 0, 1.32 s (up to date: nothing to recompile) | **VERIFIED** (see note) |
| 3 | Targeted test `Afterfall.Agent.Link` | `UnrealEditor-Cmd.exe … -ExecCmds="Automation RunTests Afterfall.Agent.Link; Quit" -nullrhi` → `targeted-agentlink.log` | `Test Completed. Result={Success} Name={Link} Path={Afterfall.Agent.Link}`; `**** TEST COMPLETE. EXIT CODE: 0 ****`; `TARGETED_EXIT:0` | **VERIFIED** |
| 4 | Full automation regression | same, `RunTests Afterfall` → `full-suite.log` | 37 started / 37 completed / **37 Success / 0 Fail**; `**** TEST COMPLETE. EXIT CODE: 0 ****`; `FULLSUITE_EXIT:0` | **VERIFIED** |
| 5 | New test inside the suite | grep `Path={Afterfall.Agent.Link}` in `full-suite.log` | `Test Completed. Result={Success}` at 22:35:27, alongside 36 others — matches the claimed 37 total | **VERIFIED** |
| 6 | Python contract mirror | `python3 Scripts/validate_agent_link.py` (cwd `Real Game/`) | `AGENT LINK MIRROR: ALL PASS`, exit 0, **21 checks** | **VERIFIED** (count differs from doc — §5.1) |
| 7 | Functional script suite | `bash Scripts/run_tests.sh` (cwd `Real Game/`) → `script-suite-from-realgame.txt` | exit 0, **321 `[PASS]` lines, 0 `[FAIL]`**, incl. `[PASS] agent link contract` | **VERIFIED** |
| 8 | Audit log actually emitted | `targeted-agentlink.log` | `[2026.10.24-22.33.40:507][588]LogAfterfall: AGENTLINK op=status result=ok` | **VERIFIED** |
| 9 | Invalid requests refused with exact codes | test source + passing run | empty / unknown / op-prefix `statu` / 4097-char payload each asserted against an exact code | **VERIFIED** |
| 10 | Null registry | test source + passing run | `unavailable`, `bAccepted=false` | **VERIFIED** |
| 11 | Registry not mutated by a query | test source + passing run | `Providers=1` after registering a probe; `Num()` restored after `Unregister` | **VERIFIED** |
| 12 | Shutting-down registry refusal | none | Code path exists (`IsShuttingDown()` → `shutting-down`), but **no test calls `ShutdownAll()` before `QueryStatus`**. The test only asserts a fresh registry is *not* shutting down (line 87) | **UNVERIFIED** — see §5.2 |
| 13 | Shipping refusal | `#if UE_BUILD_SHIPPING` in both entry points; binary string search | Both entry points short-circuit by inspection. String `disabled-shipping` is absent from the editor DLL (expected — that branch is compiled out) **and absent from the shipping exe**, because that exe is stale (§5.3). The branch was never executed | **UNVERIFIED** — source inspection only |
| 14 | "Afterfall.Build.cs unchanged" | mtime `14:39` vs slice files `16:51`; content read | No agent-link dependency; timestamp predates the slice | **VERIFIED** (as "untouched by this slice") |
| 15 | Documented "source is empty" claim | `C:\Users\mckee\Desktop\Agent Game Connect` | exists, **0 entries**, created 16:49 | **VERIFIED** |
| 16 | Subsystem registration anomaly | `grep -i unexpected` in both logs | **zero matches**; but see §5.4 for the duplicate registration line | **VERIFIED** (no anomaly string) |

### Note on check 2 (build)

The build was already up to date — 1.32 s, no compiler invocation. That proves
the target is consistent, **not** that a fresh compile of the slice succeeds on
its own. The compile evidence that does exist is indirect but strong: the test
executed at runtime, so the binary contains the new code; `UnrealEditor-Afterfall.dll`
is dated `17:01`, after every slice file (`16:51`/`16:52`).

## 4. The reported UnrealEditor-Cmd crash and missing TEST_EXIT

**Not reproduced.** Two independent runs (targeted, then full suite) both ended
with `**** TEST COMPLETE. EXIT CODE: 0 ****`, wrote the full log, and returned
exit 0 to the shell.

Likely origin of the concern, and worth stating because it will recur: every
clean UE log contains the strings

```
LogWindows: Started CrashReportClient (pid=…)
LogWindows: Custom abort handler registered for crash reporting.
LogInit: Session CrashGUID >====…
```

A grep for `crash` matches those three boilerplate lines in **both** passing
runs (7 hits per log, 0 real faults). There is no `Fatal error`, no `Assertion
failed`, no `Ensure condition failed`, and no `LogAfterfall: Error/Warning` line
in either log. If the earlier report was produced by grepping for "crash", that
grep alone cannot distinguish a crash from normal startup.

The earlier "missing TEST_EXIT evidence" is consistent with a run whose log was
never written to a path the reporter checked (this pass used `-abslog` to pin it
to `Tony/agent1-evidence-2026-10-24/`), rather than with a crash.

**Label: VERIFIED — no crash under this configuration.** It does not prove no
crash exists under a different configuration (different map, cooked content,
Shipping, second client).

## 5. Discrepancies between the docs and observed behaviour

These are documentation/evidence mismatches, not gameplay defects. All four are
recorded here rather than fixed, because this pass was verification-only.

### 5.1 Mirror check count: claimed 22, actual 21

`AGENT-GAME-CONNECT-TEST-REPORT.md` says "22 checks". Actual output is 21
`[PASS]` lines (3 existence + 18 contract checks), `ALL PASS`. Off-by-one in the
report text; the checks themselves pass.

### 5.2 Shutting-down refusal is claimed as tested; it is not

TEST-REPORT: *"Null registry and shutting-down registry: clean refusals —
automation test, passed."* The automation test covers the null registry but
never reaches the shutting-down branch: no `ShutdownAll()` call exists in
`AgentGameLinkTest.cpp` (grep: `shut` matches only a probe's `Shutdown()` override
and the "not shutting down" assertion on a fresh registry). `ShutdownAll` /
`IsShuttingDown` are exercised only by `Tests/SubsystemRegistryTest.cpp` — a
different test, on a different contract.

So: the refusal code exists and reads correctly, and the registry's own shutdown
behaviour is tested elsewhere, but **"AgentGameLink refuses a shutting-down
registry" has never been executed**. Closing it is a ~6-line addition to the
existing test (call `Reg->ShutdownAll()`, then assert `QueryStatus(Reg).Code ==
"shutting-down"`), which is a source change and therefore outside this pass.

### 5.3 There is a Shipping binary, and it is stale relative to the slice

`Real Game/Binaries/Win64/Afterfall-Win64-Shipping.exe` exists, dated **16:16** —
35 minutes *before* `AgentGameLink.cpp` (16:51). Neither `disabled-shipping` nor
`AgentGameLink` appears in it (UTF-16LE search, see §9 on the correction), which
is expected for a binary built before the gate existed, and it means the shipping
artefact in the folder **cannot** be used as evidence for the shipping refusal in
either direction.

The correct reading: shipping refusal is **source-inspected only**. The report
already admits this ("verified statically"), which is accurate; the extra fact
worth carrying forward is that a stale Shipping binary sitting in `Binaries/`
is easy to mistake for proof.

To actually verify, from `Real Game/`:
`Build.bat AfterfallShipping Win64 Shipping "…/RealGame.uproject" -WaitMutex`
then confirm `disabled-shipping` appears in the new exe. Not run here: it is a
multi-minute full shipping build, and building a packaging target was out of
scope for a verification-only pass.

### 5.4 The "unexpected subsystem registration" is duplicate init, not corruption

The only registration lines in the targeted log are:

```
22:33:32.128  SubsystemRegistry: registered Voxel (1 total)
22:33:33.273  SubsystemRegistry: registered Voxel (1 total)
22:33:40.508  SubsystemRegistry: registered LinkProbe (1 total)
```

Two `Voxel` registrations are two **different registries**, one per world context
in an editor-Cmd run (the same duplication appears as two `ROOMTEST subsystem
init` lines). Each registry reports `(1 total)`, i.e. no duplicate entries within
a registry — `Register()` refuses duplicates, and the string `Unexpected` appears
nowhere in either log.

The third line is the important one: the test's probe registers into the **local**
registry created by the test (`NewObject<UAfterfallSubsystemRegistry>`), which is
the isolation the docs promise, observed at runtime.

## 6. Safety boundaries honoured

- No source, config, content, or packaging file modified. `git status` totals are
  identical before and after: 14 tracked files / 1855 insertions / 34 deletions.
- No caches cleared, no build configuration changed, no commit, no push.
- All new files written by this pass are under
  `Tony/agent1-evidence-2026-10-24/` (4 wrapper `.bat` files + logs). The game
  tree only received its normal `Saved/Logs` output.
- **Observed concurrent activity:** `_vpass1_full_out.txt` (repo root) was
  written at **17:35**, during this pass, by `Real Game/Builds/vpass1_full_tmp.bat`
  (a UBT `-Mode=ValidatePlatforms` run). It is not mine; I neither touched nor
  interrupted it. My runs completed normally alongside it, but the concurrent
  UBT activity is a caveat on the build-up-to-date result in check 2.

## 7. Reconciliation with the five concurrent reports

These files were written into `Tony/` during this pass by other agents:
`AGENT1_AFTERFALL_VERIFICATION_REPORT.md` (17:33),
`AGENT2_COORDINATION_REPORT.md` (17:32),
`AGENT2_COORDINATION_AND_INTEGRATION_REPORT.md` (17:34),
`AGENT_A_AFTERFALL_BUILD_VERIFICATION.md` (17:35),
`AGENT_B_TONY_AGENTGAMECONNECT_CONTRACT.md` (17:35). None of them references
this report; none is an acknowledgment of it.

Two of them (`AGENT1_…`, `AGENT_A_…`) label the runtime checks **BLOCKED** on the
premise that the environment cannot run them:

| Their stated blocker | Observed in this pass | Verdict |
|---|---|---|
| "bash not available natively; would need WSL/cygwin" | `bash Scripts/run_tests.sh` ran in cwd `Real Game/`: exit 0, 321 `[PASS]`, 0 `[FAIL]` | **contradicted** — Git Bash is present |
| "UE automation not executed… requires MSVC + generated project files" | `Build.bat AfterfallEditor` → `Result: Succeeded`, exit 0; `UnrealEditor-Cmd` ran both suites, exit 0 | **contradicted** — toolchain and generated files are present |
| "Python mirrors: NOT RUN" | mirror ran: `ALL PASS`, exit 0, 21 checks | **contradicted** |
| "No TEST_EXIT found in inspected logs" | TEST_EXIT present in both runs (`TEST COMPLETE. EXIT CODE: 0`) | **caused by the artefact inspected**, see below |

On the last row: both reports drew their "missing TEST_EXIT" from
`C:\Users\mckee\Desktop\Afterfall\out.txt` (mtime **09:35**). That file is a UBT
`-Mode=ValidatePlatforms` log for the **root** `Afterfall.uproject` — an SDK
check. It can never contain automation output. The packaged folder around it is
a packaged build dated 16:26, not the source project `Real Game/RealGame.uproject`
under verification. So the "silent crash / missing TEST_EXIT" concern traced back
to a log that was never an automation log — which also explains why no crash was
reproducible.

Two further corrections to those reports:

- `AGENT1_…` states the tests confirm "null/shutdown cases". Null: verified.
  Shutdown: **not exercised by any AgentGameLink test** (§5.2). The claim was
  inherited from the docs, not observed.
- `AGENT1_…` lists the untracked set as `"AfterFall 5.8/"` only. The material
  fact is that **`Real Game/` itself is untracked** (§1), so the slice has no
  commit provenance and no git rollback path.

What those reports get right, and this pass agrees with: the design description
of the interface, the absence of transport/auth/orchestration, and the shipping
gate being source-inspection-only. Their `BLOCKED` labels are a property of the
sessions that wrote them, not of this tree.

## 8. Unresolved / not verified

| Item | State | Why |
|---|---|---|
| Shutting-down registry refusal | UNVERIFIED | no test executes it (§5.2) |
| Shipping-build refusal at runtime | UNVERIFIED | source-inspected only; existing Shipping exe is stale (§5.3) |
| Fresh full compile of the slice | NOT RUN | build was up to date (1.32 s); compile proven indirectly via the executing test |
| World-thread-only assumption for `QueryStatus` | UNVERIFIED | documented in the header; no test asserts threading, and none can from an automation test |
| Commit provenance | BLOCKED | `Real Game/` is untracked; there is nothing to attribute or revert |
| Any runtime beyond editor-Cmd automation | NOT RUN | PIE, packaged build, and two-client sessions were out of scope for this pass |

**Stop condition reached.** No further checks performed in the verification
pass itself. The single highest-value next action is in the Agent 2 report
(`agent2-coordination-and-architecture-2026-10-24.md`).

## 9. Changes made after this verification (18:3x)

Authorised follow-up work, kept separate from the verification results above.

### 9.1 Shutting-down refusal — gap CLOSED, with positive evidence

`Real Game/Source/Afterfall/Tests/AgentGameLinkTest.cpp` now calls
`Reg->ShutdownAll()` and asserts four things against `QueryStatus(Reg)`:
refused, code `shutting-down`, `bRegistryShuttingDown == true`, `Providers == 0`.
It also emits `AddInfo("shutting-down refusal exercised: code=<code>")` so
passing is not the only evidence.

Observed in `fix-evidence-2026-10-24/suite-final.log`:

```
LogAutomationController: shutting-down refusal exercised: code=shutting-down
37 started / 37 Success / 0 Fail
TEST COMPLETE. EXIT CODE: 0
```

The §5.2 gap and the TEST-REPORT sentence it contradicted are now accurate.

### 9.2 Caller surface added (D1)

`Real Game/Source/Afterfall/Agent/AgentStatusCommand.cpp`: console command
`afAgentStatus [op]` plus a `-afAgentStatus` command-line flag, both calling
`ValidateRequest` → `QueryStatus` → `AuditLog`. Headless evidence
(`fix-evidence-2026-10-24/agentstatus-flag.log`):

```
AGENTLINK op=status result=ok
AGENTSTATUS op=status providers=1 code=ok registry_shutting_down=0
```

Measured limitation: `-ExecCmds="afAgentStatus"` does **not** dispatch console
commands in this build (tried editor-Cmd, `-game`, `-server`, and after
`Automation RunTests`; the `Cmd:` line is logged and nothing runs). The flag path
is the working headless invocation.

### 9.3 Methodology correction — the Shipping string search was invalid

§5.3 originally cited a byte search for `disabled-shipping`. UE string literals
are `TCHAR` = **UTF-16LE** on Windows, so a UTF-8 `grep` could never have found
them: the "absent" result was an artifact of the method, not evidence.

Re-run with both encodings (`UTF-16LE`):

| Binary | `AGENTLINK` | `AGENTSTATUS` | `afAgentStatus` | `disabled-shipping` |
|---|---|---|---|---|
| `UnrealEditor-Afterfall.dll` (17:53) | 1 | 2 | 2 | **0** |
| `Afterfall-Win64-Shipping.exe` (16:16) | 0 | 0 | 0 | **0** |

The corrected numbers support the same conclusions: the editor build contains
the interface and excludes the shipping branch (as `#if UE_BUILD_SHIPPING`
requires), and the stale shipping exe contains none of the slice. Shipping
refusal therefore remains **source-inspection only** — unchanged, but now stated
on a valid measurement.

### 9.4 Still open

Shipping-build refusal at runtime (needs a Shipping build, not attempted);
world-thread-only claim; interactive PIE verification; the untracked status of
`Real Game/`.
