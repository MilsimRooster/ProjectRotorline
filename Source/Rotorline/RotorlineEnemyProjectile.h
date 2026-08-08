#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineEnemyProjectile.generated.h"

class ARotorlineHelicopterPawn;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class USoundBase;

UENUM()
enum class ERotorlineEnemyWeaponType : uint8
{
    Flak,
    TankShell,
    MortarShell,
    ArtilleryRocket,
    MachineGun,
    AutoCannon,
    GuidedMissile
};

UCLASS()
class ROTORLINE_API ARotorlineEnemyProjectile : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineEnemyProjectile();
    virtual void Tick(float DeltaSeconds) override;

    void Launch(
        const FVector& Start,
        ARotorlineHelicopterPawn* Target,
        float Damage,
        ERotorlineEnemyWeaponType WeaponType,
        const FVector& InitialAimDirection = FVector::ZeroVector);
private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Tracer;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPointLightComponent> TracerLight;

    UPROPERTY()
    TObjectPtr<ARotorlineHelicopterPawn> TargetActor;

    FVector Velocity = FVector::ZeroVector;
    float HitDamage = 5.0f;
    float LifeSeconds = 0.0f;
    float ProjectileSpeed = 12500.0f;
    float MaximumLife = 7.0f;
    float HitRadius = 300.0f;
    float HomingAcceleration = 0.0f;
    float BallisticGravityCmPerSecondSquared = 0.0f;
    ERotorlineEnemyWeaponType ActiveWeapon = ERotorlineEnemyWeaponType::Flak;
};
