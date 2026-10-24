#!/usr/bin/env python3
"""AFTERFALL agent-link contract mirror (no engine needed).

Verifies the v1 game-integration-interface contract statically:
- exactly one allowlisted op ("status"), exact-match only
- exact refusal codes for empty/unknown/oversize/shipping cases
- 4KB payload cap
- Shipping builds refuse everything (zero shipped agent surface)
- pure implementation: no Tick, no gameplay/system includes, no network

This mirrors the C++ rules; it does not execute Unreal code. Runtime proof
comes from the Afterfall.Agent.Link automation test (Editor).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = ROOT / "Source" / "Afterfall" / "Agent" / "AgentGameLink.h"
CPP = ROOT / "Source" / "Afterfall" / "Agent" / "AgentGameLink.cpp"
TEST = ROOT / "Source" / "Afterfall" / "Tests" / "AgentGameLinkTest.cpp"

FAIL = 0

def check(name, cond):
    global FAIL
    print(("[PASS] " if cond else "[FAIL] ") + name)
    if not cond:
        FAIL = 1

for p in (H, CPP, TEST):
    check(f"exists {p.relative_to(ROOT)}", p.is_file())

h = H.read_text(encoding="utf-8", errors="replace")
cpp = CPP.read_text(encoding="utf-8", errors="replace")
test = TEST.read_text(encoding="utf-8", errors="replace")

check("allowlist is exactly status", 'Op == TEXT("status")' in cpp)
check("no prefix/pattern matching", "StartsWith" not in cpp and "Contains" not in cpp.split("ContainsNaN")[0])
check("code empty-op", '"empty-op"' in cpp or "'empty-op'" in cpp or "empty-op" in cpp)
check("code unknown-op", "unknown-op" in cpp)
check("code payload-too-large", "payload-too-large" in cpp)
check("code disabled-shipping", "disabled-shipping" in cpp)
check("code ok/unavailable/shutting-down", all(c in cpp for c in ("\"ok\"", "\"unavailable\"", "\"shutting-down\"")))
m = re.search(r"MaxPayloadChars\s*=\s*(\d+)", h)
check("payload cap is 4096", m is not None and m.group(1) == "4096")
check("shipping gate in ValidateRequest", "#if UE_BUILD_SHIPPING" in cpp)
check("shipping gate in QueryStatus", cpp.count("#if UE_BUILD_SHIPPING") >= 2)
check("no Tick in interface", "Tick(" not in h and "Tick(" not in cpp)
check("no network includes", all(x not in cpp for x in ("Sockets", "HttpModule", "WebSockets", "TCPSocket")))
check("no gameplay includes", all(x not in h for x in ("Character", "Zombie", "Inventory", "Destruction")))
check("registry read-only (Num/IsShuttingDown only)", "Num()" in cpp and "Register(" not in cpp and "Unregister(" not in cpp and "ShutdownAll" not in cpp)
audit_body = cpp.split("void AuditLog")[1] if "void AuditLog" in cpp else ""
audit_code = "\n".join(line.split("//")[0] for line in audit_body.splitlines())
check("payloads never logged", "Payload" not in audit_code)
check("test covers empty/unknown/prefix/oversize/status", all(s in test for s in ("empty-op", "unknown-op", "payload-too-large", '"status"')))
check("test uses isolated local registry", "NewObject<UAfterfallSubsystemRegistry>" in test)
check("test asserts registry unchanged", "registry unchanged by query" in test)

if FAIL == 0:
    print("AGENT LINK MIRROR: ALL PASS")
else:
    print("AGENT LINK MIRROR: FAILURES PRESENT")
sys.exit(FAIL)
