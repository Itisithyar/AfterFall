# fix_pawn_map.py — one-shot headless map fix for the playable-pawn issue.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this> [-log]
# What it does (Lvl_FirstPerson only):
#   1. Sets WorldSettings DefaultGameMode to /Script/Afterfall.AfterfallGameMode
#      (was: BP_FirstPersonGameMode whose BP_FirstPersonCharacter_C fails to spawn).
#   2. Adds a PlayerStart at (-700, 0, 100) facing +X. The slice room seeds its
#      structural wall at Anchor+(-700,0,0) where Anchor follows the PlayerStart,
#      so the wall always lands 7 m west of spawn: inherently clear of it. Lab
#      blocks/items spawn +600..+1300 cm east of the anchor: also clear.
import unreal

MAP_PATH = "/Game/FirstPerson/Lvl_FirstPerson"
SPAWN_LOC = unreal.Vector(-700.0, 0.0, 100.0)
SPAWN_ROT = unreal.Rotator(0.0, 0.0, 0.0)  # yaw 0 faces +X, toward the lab area

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if world is None:
    raise RuntimeError("load_map failed for %s" % MAP_PATH)

gm_class = None
try:
    gm_class = unreal.AfterfallGameMode.static_class()
except Exception as e:
    unreal.log_warning("unreal.AfterfallGameMode lookup failed: %s" % e)
if gm_class is None:
    gm_class = unreal.load_class(None, "/Script/Afterfall.AfterfallGameMode")
if gm_class is None:
    raise RuntimeError("AfterfallGameMode class not found; is AfterfallEditor built?")

ws = world.get_world_settings()
old = ws.get_editor_property("default_game_mode")
ws.set_editor_property("default_game_mode", gm_class)
unreal.log("GameMode override: %s -> %s" % (old, gm_class.get_name()))

starts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerStart)
unreal.log("pre-existing PlayerStarts: %d" % len(starts))
ps = None
if len(starts) == 0:
    ps = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, SPAWN_LOC, SPAWN_ROT)
    if ps is None:
        raise RuntimeError("PlayerStart spawn failed")
    ps.set_actor_label("AfterfallPlayerStart")
    unreal.log("spawned PlayerStart at %s" % ps.get_actor_location())
else:
    ps = starts[0]
    unreal.log("reusing PlayerStart at %s" % ps.get_actor_location())

ok = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH)
unreal.log("save_map %s -> %s" % (MAP_PATH, ok))
if not ok:
    raise RuntimeError("save_map failed")
