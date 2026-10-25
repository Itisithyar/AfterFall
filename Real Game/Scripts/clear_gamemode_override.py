import unreal
map_path = "/Game/FirstPerson/Lvl_FirstPerson"
world = unreal.EditorLevelLibrary.load_level(map_path)
if world:
    ws = world.get_world_settings()
    ws.set_editor_property("default_game_mode", None)
    unreal.EditorLevelLibrary.save_current_level()
    print("CLEARED GameModeOverride on Lvl_FirstPerson")
else:
    print("FAILED to load level")
