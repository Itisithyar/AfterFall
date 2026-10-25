#!/usr/bin/env bash
# AFTERFALL Stage A — baseline capture (no gameplay change).
# Usage: bash Scripts/perf_baseline.sh [scenario]
# Requires: editor build + TestNeighborhood map (blocked until MSVC).
# Output: Saved/Perf/<date>-<scenario>/ (stat logs + trace + checklist).
set -euo pipefail
SCENARIO="${1:-empty_field}"
OUT="Saved/Perf/$(date +%Y%m%d_%H%M%S)-$SCENARIO"
mkdir -p "$OUT"
cat > "$OUT/scenario.txt" <<EOF
scenario=$SCENARIO
map=/Game/Maps/TestNeighborhood
ref_hw=TBD (RTX4070/7800X3D/32GB/NVMe proposed)
preset=competitive 1080p (record exact settings here)
accept=avg>=165, 1% low>=120, hitches(>12ms)<5/min, server tick p95<=33ms
commands: stat unit | stat fps | stat game | stat gpu | stat net
  trace: UnrealInsights -trace=cpu,gpu,net,bookmarks
EOF
cat > "$OUT/COMMANDS.txt" <<'EOF'
# Paste into editor console (or -ExecCmds) per scenario:
stat unit
stat fps
stat game
stat gpu
stat net
stat streaming
stat hlod
# Insights: start trace, run 120s scenario, stop, save .utrace here.
# Worst busy moment: sprint village + zombies + PvP + base + streaming.
EOF
echo "Prepared $OUT"
echo "Scenarios: empty_field dense_forest village sprint_crossing fortified_base zombie24 combat_gunfire reload_region restart_restore combined_worst"
echo "No capture run (needs MSVC build). Fill scenario.txt + run when unblocked."
