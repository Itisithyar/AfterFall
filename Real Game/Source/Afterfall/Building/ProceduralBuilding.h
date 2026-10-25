#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Math/RandomStream.h"
#include "Math/AfterfallMath.h"
#include "ProceduralBuilding.generated.h"

UCLASS()
class AFTERFALL_API AProceduralBuilding : public AActor
{
	GENERATED_BODY()
public:
	AProceduralBuilding();
	UPROPERTY(EditAnywhere, Category="Building") int32 FloorCount = 3;
	UPROPERTY(EditAnywhere, Category="Building") FVector2D FootprintSize = FVector2D(900,900);
	UPROPERTY(EditAnywhere, Category="Building") float WallThickness = 20.f;
	UPROPERTY(EditAnywhere, Category="Building") float FloorHeight = 300.f;
	UPROPERTY(EditAnywhere, Category="Building") int32 RandomSeed = 12345;
	// ---- Fragmentation (impulse destruction) ----
	/** Blast speed given to sections at the epicenter, in cm/s (velocity-change
	 *  impulse, so a 900cm concrete-ish slab and a thin wall scatter alike). */
	UPROPERTY(EditAnywhere, Category="Building|Fragment") float FragmentStrength = 900.f;
	/** Distance over which the blast falls off to zero (cm). */
	UPROPERTY(EditAnywhere, Category="Building|Fragment") float FragmentFalloffRadius = 1600.f;
	/** Random per-section kick as a fraction of FragmentStrength (0 = pure radial). */
	UPROPERTY(EditAnywhere, Category="Building|Fragment") float FragmentJitter = 0.25f;
	/** Latches after the first blast so a second call cannot re-kick settled debris. */
	UPROPERTY(VisibleAnywhere, Transient, Category="Building|Fragment") bool bFragmented = false;

	UFUNCTION(BlueprintCallable, Category="Building") void RebuildBuilding();
	/** Detaches every section, switches it to simulated physics and kicks it with a
	 *  distance-scaled radial impulse. Idempotent (bFragmented latch). */
	UFUNCTION(BlueprintCallable, Category="Building") void FragmentBuilding();

	/** Pure impulse math for one section — exposed so the blast rule is unit-testable
	 *  without a world. Returns a velocity-change impulse (cm/s) pointing away from
	 *  Epicenter, scaled by Strength and the 1 - dist/FalloffRadius falloff, plus a
	 *  deterministic jitter from Stream scaled by JitterFraction. Degenerate case
	 *  (section at the epicenter) blasts straight up. */
	static FVector FragmentImpulseFor(const FVector& SectionCenter, const FVector& Epicenter,
		float Strength, float FalloffRadius, float JitterFraction, FRandomStream& Stream);
protected:
	virtual void BeginPlay() override;
private:
	void BuildFloor(int32 FloorIndex);
	void AddWallSegment(const TArray<FVector>& Triangles, const FString& Tag);
};