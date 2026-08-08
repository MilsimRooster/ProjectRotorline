#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineCannonProjectile.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Short-lived physical 30 mm round used by the playable Apache.
 *
 * The projectile deliberately performs both a world trace and a swept check
 * against Rotorline mission targets. Most mission models are presentation
 * meshes with collision disabled, so relying on mesh collision alone would
 * make a visibly accurate burst pass straight through them.
 */
UCLASS()
class ROTORLINE_API ARotorlineCannonProjectile : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineCannonProjectile();
    virtual void Tick(float DeltaSeconds) override;

    void Launch(const FVector& Start, const FVector& Direction, float Damage);
    void LaunchAdvanced(
        const FVector& Start,
        const FVector& Direction,
        float Damage,
        float Speed,
        float BlastRadius,
        const FString& DamageSource,
        float TracerScale = 1.0f);

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Tracer;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPointLightComponent> TracerLight;

    FVector Velocity = FVector::ZeroVector;
    FVector LaunchOrigin = FVector::ZeroVector;
    float HitDamage = 28.0f;
    float LifeSeconds = 0.0f;
    float BlastRadius = 0.0f;
    FString DamageSource = TEXT("30MM_DIRECT");
};
