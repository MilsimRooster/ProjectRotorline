#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineMissionCatalog.h"
#include "RotorlineMissionObjectiveActor.generated.h"

class UPointLightComponent;
class APawn;
class UAudioComponent;
class UAnimSequence;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;
class UTextRenderComponent;
class ARotorlineHelicopterPawn;
class URotorlineGroundingComponent;
enum class ERotorlineEnemyWeaponType : uint8;

UENUM(BlueprintType)
enum class ERotorlineThreatType : uint8
{
    None,
    Flak,
    RadarMissile,
    Tank,
    RocketArtillery,
    MachineGunship,
    RocketGunship
};

enum class ERotorlineEnemyAirframe : uint8
{
    None,
    MD500,
    Apache,
    Hind
};

UCLASS()
class ROTORLINE_API ARotorlineMissionObjectiveActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineMissionObjectiveActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void Configure(const FRotorlineObjectiveDefinition& Objective, const FVector& WorldLocation);
    void ApplyRocketDamage(float Damage);
    bool ApplyCombatDamage(float RequestedDamage, const TCHAR* DamageSource, float& OutAppliedDamage);
    bool IsDestroyedTarget() const { return bDestroyedTarget; }
    bool IsDestroyObjective() const { return bDestroyObjective; }
    void SetMissionMarkerVisibility(bool bVisible);
    FVector GetAimLocation() const;
    FVector GetMuzzleLocation() const;
    FVector GetWeaponAimDirection() const;
    void NotifyWeaponFired(ERotorlineEnemyWeaponType WeaponType);
    bool RefreshAudioMix();
    ERotorlineThreatType GetThreatType() const { return ThreatType; }
    bool IsAircraftThreat() const;
    bool IsAircraftCombatActive() const { return IsAircraftThreat() && !bAircraftDying && !bDestroyedTarget; }
    float GetProjectileHitRadius() const;
    float GetHealthFraction() const { return TargetHealth <= 0.0f ? 0.0f : FMath::Clamp(TargetHealth / FMath::Max(1.0f, TargetMaxHealth), 0.0f, 1.0f); }
    float GetCurrentHealth() const { return FMath::Clamp(TargetHealth, 0.0f, FMath::Max(1.0f, TargetMaxHealth)); }
    float GetMaximumHealth() const { return FMath::Max(1.0f, TargetMaxHealth); }
    int32 GetPlayerDamageEventCount() const { return PlayerDamageEventCount; }
    bool HasWeaponSolution(const FVector& TargetLocation) const;
    bool HasCurrentWeaponSolution() const;
    FVector GetCurrentWeaponTargetLocation() const { return CurrentCombatTargetLocation; }
    FString GetTargetLabel() const { return ObjectiveText; }
    void SetEnemyFlightQualificationMode();

private:
    void ConfigureMissionModel(const FRotorlineObjectiveDefinition& Objective);
    bool SetStaticModel(UStaticMeshComponent* Component, const TCHAR* AssetPath, const FVector& Scale, const FVector& Offset = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator);
    void EnsureThreatVisualReady();
    void SetGroundedLocation(const FVector& WorldLocation);
    bool TraceTerrainSurface(const FVector& Location, float& OutGroundHeight, FVector& OutGroundNormal) const;
    bool TraceAircraftGround(const FVector& Location, float& OutGroundHeight) const;
    void AnimateAircraftRotors(float DeltaSeconds, float SpinScale = 1.0f);
    void UpdateAircraftFlight(float DeltaSeconds, const APawn* PlayerPawn);
    void UpdateEnemyFlightQualification(float DeltaSeconds, ARotorlineHelicopterPawn* PlayerHelicopter);
    void UpdateAircraftDeath(float DeltaSeconds);
    void SpawnAircraftDeathExplosion(const FVector& Location) const;
    void UpdateGroundCombat(float DeltaSeconds, const APawn* PlayerPawn);
    void FinishDestroyedTarget();
    const TCHAR* GetAirframeName() const;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<URotorlineGroundingComponent> Grounding;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> ModelRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> FlakGunPivot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> HimarsLauncherPivot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> ApacheMainRotorPivot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> ApacheTailRotorPivot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MarkerRing;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MarkerPulseRing;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MarkerCenter;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MarkerBeam;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MarkerHLeft;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MarkerHRight;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MarkerHCross;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> CabinLandingClearing;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> LandingPadMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> PrimaryMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> SecondaryMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> TertiaryMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> FireSmokeMaterial;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UNiagaraComponent> GroundFireFlameA;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UNiagaraComponent> GroundFireFlameB;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UNiagaraComponent> GroundFireSmoke;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UNiagaraComponent> GroundFireEmbers;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPointLightComponent> GroundFireLight;

    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UStaticMeshComponent>> HimarsMeshParts;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> EnemyMainRotor;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> EnemyTailRotor;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MuzzleFlash;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPointLightComponent> MuzzleLight;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAudioComponent> FireAudio;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAudioComponent> EngineAudio;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> CabinBody;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> CabinRoofLeft;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> CabinRoofRight;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> CabinDoor;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> CabinChimney;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> SkeletalSubject;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UTextRenderComponent> ObjectiveLabel;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPointLightComponent> BeaconLight;

    FString ObjectiveKind;
    FString ObjectiveText;
    FString TargetId;
    ERotorlineThreatType ThreatType = ERotorlineThreatType::None;
    ERotorlineEnemyAirframe EnemyAirframe = ERotorlineEnemyAirframe::None;
    float TargetHealth = 100.0f;
    float TargetMaxHealth = 100.0f;
    int32 PlayerDamageEventCount = 0;
    float PulseTime = 0.0f;
    float FireAudioStopTime = -1.0f;
    float MuzzleFlashStopTime = -1.0f;
    float FireAudioBaseVolume = 0.42f;
    float EngineAudioBaseVolume = 0.34f;
    float NextFireFlameATime = 0.0f;
    float NextFireFlameBTime = 0.0f;
    float NextFireEmberTime = 0.0f;
    float NextDistanceAudioRefreshTime = 0.0f;
    FVector PatrolCenter = FVector::ZeroVector;
    FVector CurrentCombatTargetLocation = FVector::ZeroVector;
    FVector CurrentCombatTargetVelocity = FVector::ZeroVector;
    float PatrolPhase = 0.0f;
    float PatrolDirection = 1.0f;
    FRotator GroundWeaponAimRotation = FRotator::ZeroRotator;
    float GroundTrackingErrorDegrees = 180.0f;
    float LastGroundCombatAuditLogTime = -1000.0f;
    float LastSlopeRejectTime = -1000.0f;
    FVector AircraftVelocity = FVector::ZeroVector;
    FVector AircraftBreakawayWaypoint = FVector::ZeroVector;
    float AircraftForwardSpeed = 0.0f;
    float AircraftAttackSpeed = 0.0f;
    float AircraftBreakSpeed = 0.0f;
    float AircraftAcceleration = 0.0f;
    float AircraftTurnRate = 0.0f;
    float AircraftRotorRate = 0.0f;
    float AircraftManeuverTime = 0.0f;
    float LastAircraftAuditLogTime = -1000.0f;
    float LastGroundingAuditLogTime = -1000.0f;
    float AircraftLastTurnRate = 0.0f;
    float AircraftLastAcceleration = 0.0f;
    float AircraftLastVelocityDot = 1.0f;
    float AircraftLastTargetDot = 1.0f;
    float AircraftQualificationElapsed = 0.0f;
    float AircraftQualificationNextShotTime = 1.5f;
    float AircraftQualificationMinVelocityDot = 1.0f;
    float AircraftQualificationMaxTurnRate = 0.0f;
    float AircraftQualificationMaxAcceleration = 0.0f;
    float AircraftQualificationMainRotorDegrees = 0.0f;
    float AircraftQualificationTailRotorDegrees = 0.0f;
    FQuat AircraftQualificationLastMainRotorRotation = FQuat::Identity;
    FQuat AircraftQualificationLastTailRotorRotation = FQuat::Identity;
    float AircraftDeathElapsed = 0.0f;
    float AircraftDeathSmokeAccumulator = 0.0f;
    float AircraftFallSpeed = 0.0f;
    float AircraftRotorSpinScale = 1.0f;
    int32 AircraftPassSide = 1;
    int32 AircraftQualificationSamples = 0;
    int32 AircraftQualificationShots = 0;
    int32 AircraftQualificationMilestone = 0;
    int32 AircraftDamageQualificationStage = 0;
    bool bAircraftAttackRun = true;
    bool bWorldCombatMarkerEnabled = true;
    bool bAircraftSeparationAvoidanceActive = false;
    double LastAircraftCollisionTime = -1000.0;
    bool bAircraftDying = false;
    bool bEnemyFlightQualificationMode = false;
    bool bAircraftQualificationRotorSampleInitialized = false;
    bool bDestroyObjective = false;
    bool bLandingObjective = false;
    bool bDestroyedTarget = false;
    bool bFireScene = false;
    bool bUseNiagaraFire = false;
    bool bThreatVisualReady = true;
    bool bGroundPlacementReady = true;
    float MarkerBaseScale = 18.0f;
};
