#!/usr/bin/env python3
"""Mirror of InventoryComponent weights + CraftingService (no engine)."""
WEIGHTS = {"res_wood_log": 0.5, "res_stone": 0.8, "res_hemp_fiber": 0.1,
           "res_scrap_metal": 1.0, "mat_plank": 1.2, "mat_rope": 0.4,
           "res_cloth": 0.3, "mat_nail": 0.05, "AF_R_Wood": 0.5,
           "res_med_bandage": 0.1}
def weight(item):
    return WEIGHTS.get(item, 1.0)

RECIPES = {
    "plank": ({"res_wood_log": 2}, "mat_plank"),
    "wall": ({"mat_plank": 4, "mat_nail": 4}, "bld_wall_wood"),
    "bandage": ({"res_cloth": 2}, "res_med_bandage"),
}
def has_inputs(wallet, recipe):
    for i, (k, need) in enumerate(recipe[0].items()):
        if wallet.get(k, 0) < need:
            return (False, i)
    return (True, -1)

def dismantle(recipe):
    return {k: v // 2 for k, v in recipe[0].items() if v // 2 > 0}

CASES = [
    ("wood 0.5", weight("res_wood_log") == 0.5),
    ("nail 0.05", weight("mat_nail") == 0.05),
    ("unknown 1.0", weight("nope") == 1.0),
    ("weight cap: 10 wood = 5kg", sum(weight("res_wood_log") for _ in range(10)) == 5.0),
    ("rich crafts wall", has_inputs({"mat_plank": 4, "mat_nail": 4}, RECIPES["wall"]) == (True, -1)),
    ("poor blocked at nails", has_inputs({"mat_plank": 4, "mat_nail": 1}, RECIPES["wall"]) == (False, 1)),
    ("dismantle wall = 2+2", dismantle(RECIPES["wall"]) == {"mat_plank": 2, "mat_nail": 2}),
    ("no profit: dismantle < craft", all(v < RECIPES["wall"][0][k] for k, v in dismantle(RECIPES["wall"]).items())),
]

def main():
    fails = 0
    for name, ok in CASES:
        print(f"[{'PASS' if ok else 'FAIL'}] {name}")
        fails += 0 if ok else 1
    print("INVENTORY/CRAFT MIRROR: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)

if __name__ == "__main__":
    main()
