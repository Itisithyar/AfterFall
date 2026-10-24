#!/usr/bin/env bash
# AFTERFALL vertical-slice验收: 1 dedicated server + 2 clients on one PC.
# Usage: bash Scripts/launch_2client_server.sh [MapName]
# Requires: editor build done (Scripts/build.sh editor), UE 5.8 installed.
set -euo pipefail
MAP="${1:-TestNeighborhood}"
UE_ROOT="/c/Program Files/Epic Games/UE_5.8"
EDITOR="$UE_ROOT/Engine/Binaries/Win64/UnrealEditor.exe"
PROJECT="$PWD/Afterfall.uproject"

bash Scripts/check_prereqs.sh

echo ">> Starting dedicated server ($MAP, -log, port 7777) ..."
"$EDITOR" "$PROJECT" "/Game/Maps/$MAP" -server -log -port=7777 &
SERVER_PID=$!
sleep 8

echo ">> Starting client 1 (127.0.0.1) ..."
"$EDITOR" "$PROJECT" "127.0.0.1" -game -log -windowed -ResX=1280 -ResY=720 &
C1_PID=$!
sleep 3

echo ">> Starting client 2 (127.0.0.1) ..."
"$EDITOR" "$PROJECT" "127.0.0.1" -game -log -windowed -ResX=1280 -ResY=720 &
C2_PID=$!

echo "Server PID=$SERVER_PID clients=$C1_PID,$C2_PID"
echo "Acceptance: both clients see same wall after ServerRequestPlacement; zombie attacks barricade; disconnect/reconnect keeps progress."
echo "To stop: kill $SERVER_PID $C1_PID $C2_PID"
wait
