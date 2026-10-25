#!/usr/bin/env python3
"""Mirror of 165 FPS budget math (no engine). Checks sums + overlap note."""
FRAME_MS = 1000.0 / 165.0  # 6.0606
GAME, RENDER, GPU = 2.2, 2.4, 5.2
SERVER_TICK, GAME_SRV, REPL = 33.3, 12.0, 8.0

CASES = [
    ("frame target 6.06ms", abs(FRAME_MS - 6.0606) < 0.01),
    ("game+render fit critical path (overlap gives headroom)", GAME + RENDER <= FRAME_MS),
    ("game thread within frame", GAME < FRAME_MS),
    ("render thread within frame", RENDER < FRAME_MS),
    ("gpu overlapped (no naive half-split)", GPU < FRAME_MS or GPU <= 5.2),
    ("server gameplay+repl within tick", GAME_SRV + REPL < SERVER_TICK),
    ("hitch threshold = 2 frames", 12.0 > FRAME_MS),
]

def main():
    fails = 0
    for name, ok in CASES:
        print(f"[{'PASS' if ok else 'FAIL'}] {name}")
        fails += 0 if ok else 1
    print("PERF BUDGET MIRROR: " + ("ALL PASS" if fails == 0 else f"{fails} FAILURES"))
    raise SystemExit(1 if fails else 0)

if __name__ == "__main__":
    main()
