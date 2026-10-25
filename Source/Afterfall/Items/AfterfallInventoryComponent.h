#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/ItemStack.h"
#include "Items/ItemRegistry.h"
#include "AfterfallInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAfterfallInventoryChanged);

UCLASS(ClassGroup=(Afterfall), meta=(BlueprintSpawnableComponent))
class AFTERFALL_API UAfterfallInventoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsWorldCache = false;
	UPROPERTY(BlueprintAssignable) FOnAfterfallInventoryChanged OnInventoryChanged;
	UPROPERTY() TArray<FItemStack> Slots;
	int32 AddItem(FName ID, int32 Qty);
	bool RemoveItem(FName ID, int32 Qty);
	bool HasItem(FName ID, int32 Qty) const;
	bool TryCombineItems(FName A, FName B);
	/** Exchange two slots in place (inventory mat click-to-swap) and notify. */
	void SwapSlots(int32 IndexA, int32 IndexB);
	TArray<FItemStack> GetAllStacks() const { return Slots; }
};
