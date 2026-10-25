# Playtest Checklist

| Test | How | Expected | Expected log line(s) | Result |
|---|---|---|---|---|
| 1. Press Play, walk, look around | Start PIE | No crash | none | |
| 2. Throw zombie at wall | Spawn/throw zombie into wall | One damage application | [IMPACT] ... then [IMPACT-DEDUP] not SKIPPED | |
| 3. Throw same body after 1s | Repeat throw | Damages again | [IMPACT-DEDUP] APPLIED | |
| 4. Melee wall once | Melee attack | One damage application | ServerApplyBoxImpact once | |
| 5. Hitscan wall, hold fire 3s | Fire repeatedly | Damage every round, none dropped | ServerApplyImpact per shot | |
| 6. Client hit prediction | Client fire | Predicted hit then confirmed/retracted | ClientResolveHitPrediction | |
| 7. Inventory UI | Open/close Tab | Widget shows/hides | none | |
| 8. Packaged build launches | Run packaged exe | Reaches map | no errors in 30s | |
