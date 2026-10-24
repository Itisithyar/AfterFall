#!/usr/bin/env bash
# AFTERFALL build — editor + Windows client + Windows dedicated server.
# Usage: bash Scripts/build.sh [editor|client|server|all]
set -euo pipefail
MODE="${1:-all}"
UE_ROOT="/c/Program Files/Epic Games/UE_5.8"
UBT="$UE_ROOT/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe"
UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"

bash Scripts/check_prereqs.sh

build_editor() {
  echo ">> Building editor (AfterfallEditor Win64 Development) ..."
  "$UBT" AfterfallEditor Win64 Development -Project="$PWD/Afterfall.uproject" -WaitMutex -FromMsBuild
}

build_client() {
  echo ">> Packaging Windows client (Shipping) ..."
  bash "$UAT" BuildCookRun -project="$PWD/Afterfall.uproject" -platform=Win64 -clientconfig=Shipping -build -cook -pak -stage -archive -archivedirectory="$PWD/Build/WindowsClient"
}

build_server() {
  echo ">> Building dedicated server (Win64 Shipping) ..."
  "$UBT" AfterfallServer Win64 Shipping -Project="$PWD/Afterfall.uproject" -WaitMutex -FromMsBuild
  echo ">> Packaging server ..."
  bash "$UAT" BuildCookRun -project="$PWD/Afterfall.uproject" -platform=Win64 -serverconfig=Shipping -server -cook -pak -stage -archive -archivedirectory="$PWD/Build/WindowsServer" -serverplatform=Win64 -noclient
}

case "$MODE" in
  editor) build_editor ;;
  client) build_client ;;
  server) build_server ;;
  all) build_editor; build_server ;;
  *) echo "usage: build.sh [editor|client|server|all]"; exit 2 ;;
esac
echo "BUILD DONE ($MODE)"
