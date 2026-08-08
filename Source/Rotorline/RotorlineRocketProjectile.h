#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineRocketProjectile.generated.h"

class ARotorlineMissionObjectiveActor;
class ARotorlineHelicopterPawn;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS()
class ROTORLINE_API ARotorlineRocketProjectile : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineRocketProjectile();

    virtual void Tick(float DeltaSeconds) override;

    void Launch(const FVector& Start, const FVector& InitialDirection, ARotorlineMissionObjectiveActor* Target);
    void LaunchPlayerWeapon(
        const FVector& Start,
        const FVector& InitialDirection,
        ARotorlineMissionObjectiveActor* Target,
        const FString& WeaponId,
        float DirectDamage,
        float MinimumBlastDamage,
        float BlastRadius,
        float ProjectileSpeed,
        const FString& ProjectileAsset);
    void LaunchEnemy(const FVector& Start, const FVector& InitialDirection, ARotorlineHelicopterPawn* Target, float Damage);
    void LaunchEnemyArtillery(const FVector& Start, const FVector& InitialDirection, ARotorlineHelicopterPawn* Target, float Damage);
    void LaunchEnemyAirDefense(const FVector& Start, const FVector& InitialDirection, ARotorlineHelicopterPawn* Target, float Damage);
    void DetonateVisualOnly(const FVector& Location);
    bool DivertEnemyGuidance(const FVector& DecoyLocation);

private:
    void Explode(bool bDamageTarget);
    void ApplyPlayerBlastDamage();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> RocketMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> TrailNear;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> TrailMid;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> TrailFar;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> ImpactFlash;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> ExplosionCore;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> ExplosionFlameA;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> ExplosionFlameB;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> ExplosionSmoke;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> ExplosionSmokeMaterial;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> ExplosionSparks;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPointLightComponent> RocketLight;

    UPROPERTY()
    TObjectPtr<ARotorlineMissionObjectiveActor> TargetActor;

    UPROPERTY()
    TObjectPtr<ARotorlineMissionObjectiveActor> DirectImpactTarget;

    UPROPERTY()
    TObjectPtr<ARotorlineHelicopterPawn> EnemyTargetActor;

    UPROPERTY()
    TObjectPtr<USoundBase> PlayerLaunchSound;

    UPROPERTY()
    TObjectPtr<USoundBase> EnemyLaunchSound;

    UPROPERTY()
    TObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY()
    TObjectPtr<UNiagaraSystem> ImpactExplosionSystem;

    UPROPERTY()
    TObjectPtr<UNiagaraSystem> PlayerHitExplosionSystem;

    UPROPERTY()
    TObjectPtr<UNiagaraSystem> PlayerHitSparksSystem;

    UPROPERTY()
    TObjectPtr<UAudioComponent> LaunchAudioComponent;

    float LaunchAudioUnduckedVolume = 0.0f;
    FVector Velocity = FVector::ZeroVector;
    FVector PlayerLaunchOrigin = FVector::ZeroVector;
    float LifeSeconds = 0.0f;
    float ExplosionSeconds = 0.0f;
    float TrailSpawnAccumulator = 0.0f;
    bool bExploding = false;
    bool bUsingNiagaraExplosion = false;
    bool bPlayerDamageEnabled = false;
    bool bPlayerGuidedWeapon = false;
    bool bEnemyRocket = false;
    bool bEnemyArtilleryRocket = false;
    bool bEnemyAirDefenseMissile = false;
    bool bCountermeasureDiverted = false;
    FVector CountermeasureTarget = FVector::ZeroVector;
    float CountermeasureElapsed = 0.0f;
    float EnemyDamage = 0.0f;
    FString PlayerWeaponId = TEXT("ROCKET");
    float PlayerDirectDamage = 125.0f;
    float PlayerMinimumBlastDamage = 28.0f;
    float PlayerBlastRadius = 900.0f;
    float PlayerProjectileSpeed = 36000.0f;
};
