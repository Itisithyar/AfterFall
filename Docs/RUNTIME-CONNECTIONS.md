# RUNTIME CONNECTIONS — Afterfall

| Source | Destination | Interface | Purpose | Status |
|---|---|---|---|---|
| AAfterfallCharacter | UInventoryComponent | direct pointer | inventory state | VERIFIED |
| AAfterfallCharacter | UAfterfallMovementComponent | GetAfterfallMovement | load mass | VERIFIED |
| UInventoryComponent | OnInventoryChanged | delegate | refresh load | VERIFIED |
| Game runtime | Dev services | none found | N/A | DISCONNECTED |
