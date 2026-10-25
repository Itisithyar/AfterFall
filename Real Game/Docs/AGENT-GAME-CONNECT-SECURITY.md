# AGENT-GAME-CONNECT — Security (v1, 2026-10-24)

## Trust boundaries

1. **Orchestration (A) is untrusted input.** Every request is validated
   before use; nothing it sends is executed, dereferenced, or forwarded to
   gameplay except through the allowlist.
2. **Interface (B) is the choke point.** One function validates, one function
   answers. No other entry exists in v1.
3. **Gameplay (C) never trusts B.** In v1, C does not call B at all, so there
   is no path — direct or indirect — from an agent request to world state.

## Controls implemented in v1

- **Allowlist, exact match:** only `Op == "status"` passes. Prefixes,
  patterns, and unknown ops are refused (`unknown-op`, tested).
- **Input bounds:** empty op refused; payloads over 4096 TCHARs refused.
- **Fail closed:** null registry, shutting-down registry, and Shipping builds
  all refuse with explicit codes. No silent success path exists.
- **No secrets in scope:** the interface carries no credentials, keys, or
  player data; the audit log records op + outcome code only, never payloads.
- **No new attack surface in shipping:** `UE_BUILD_SHIPPING` short-circuits
  both entry points, so the packaged game contains no reachable agent logic.
- **Isolation:** tests use a local registry; the interface holds no global or
  static mutable state, so there is nothing to leak across worlds/sessions.

## Failure isolation

- Agent tooling crashing, timing out, or disconnecting cannot affect gameplay:
  gameplay never calls it, and it installs no tick, timer, delegate, or
  network listener in v1 (verified by code inspection + mirror, see TEST-REPORT).
- There is no queue in v1, hence no queue to bound, cancel, or drain. If a
  transport is added later, it must bring bounded queues, timeouts, and
  cancellation before any write op is accepted (see INTEGRATION-PLAN stage 4).

## Residual risks / non-goals for v1

- No authentication exists because no remote channel exists. Authentication
  becomes mandatory the moment a transport is proposed — it is a stage-4
  prerequisite, not a retrofit.
- The audit log goes to the game log; if payloads are ever logged in future
  stages, they must be redacted first (current code cannot log them: AuditLog
  takes op + code only).
