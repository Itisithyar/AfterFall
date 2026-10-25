# AGENT-GAME-CONNECT — Architecture (v1, 2026-10-24)

Status: implemented slice verified in Editor; shipped game exposes zero agent surface.

## 0. Source discovery

`C:\Users\mckee\Desktop\Agent Game Connect` is an **empty directory** (0 files,
created 2026-10-24). There is no plugin, SDK, service, or documentation to
reuse. The interface below was therefore designed from the Afterfall side,
following existing Afterfall subsystem conventions. Nothing was preserved
because nothing existed; nothing from gameplay was replaced.

## 1. Three parts and their boundaries

```
A. Agent orchestration (EXTERNAL — not in this repo)
   Routes requests, runs dev workflows, proposes patches.
   Never linked into the game. No SDK added to any game target.

B. Game integration interface (NEW — Source/Afterfall/Agent/AgentGameLink.*)
   Pure functions: ValidateRequest / QueryStatus / AuditLog.
   Allowlist: exactly one read-only op ("status"). Failures are values
   (explicit Codes), never silent, never exceptions.
   Reads the existing world-subsystem registry; mutates nothing.

C. Gameplay and simulation (UNTOUCHED)
   Physics, damage, destruction, inventory, persistence, replication and
   their server authority stay exactly as they were. No gameplay file was
   modified; no gameplay system calls into B; B is never called from C.
```

Data flows one way for reads: A -> B -> registry snapshot -> structured
result + audit log. There is no write path in v1 and no transport: requests
arrive as function calls from dev tooling / console, not over a network.

## 2. Why this shape

- The existing `Subsystem/` layer already solved the hard problems this
  interface needs: lifecycle states, Ready-only resolution, failures as
  values (`AfterfallService::EError`), isolation via local registries in
  tests. B reuses that layer instead of inventing a second one.
- A pure, tickless, networkless slice has no frame cost and no new
  dependencies (`Afterfall.Build.cs` unchanged), so it cannot regress the
  game loop by construction. Measurements in TEST-REPORT confirm ~zero cost.
- The Shipping gate (`UE_BUILD_SHIPPING` refuses everything) keeps the
  shipped game free of agent surface even if dev tooling grows later.

## 3. What v1 deliberately excludes

Write operations, transport (HTTP/sockets/pipes), authentication, remote
execution, runtime NPC/event hooks, and any call from gameplay into B.
Each exclusion is a completion-criteria item (§8 of the task) enforced by
code (allowlist + shipping gate), not by policy text.
