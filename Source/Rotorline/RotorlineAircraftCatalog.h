#pragma once

#include "CoreMinimal.h"

struct FRotorlineAircraftStats
{
    int32 Speed = 1;
    int32 Maneuverability = 1;
    int32 Armor = 1;
    int32 Cargo = 1;
};

struct FRotorlineAircraftSuitability
{
    int32 Rescue = 1;
    int32 Medevac = 1;
    int32 Cargo = 1;
    int32 Recon = 1;
    int32 Attack = 1;
    int32 Escort = 1;
};

struct FRotorlineAircraftRotorGroup
{
    FString Role = TEXT("main");
    FString SpinAxis = TEXT("Z");
    TArray<FString> Assets;
    bool bHasExplicitPivot = false;
    FVector Pivot = FVector::ZeroVector;
    bool bHasExplicitMeshPivot = false;
    FVector MeshPivot = FVector::ZeroVector;
    FRotator AlignmentRotation = FRotator::ZeroRotator;
};

struct FRotorlineAircraftWeaponModeDefinition
{
    FString Id;
    FString DisplayName;
    FString TargetClass;
    FString ProjectileAsset;
    int32 Ammo = 0;
    float Damage = 0.0f;
    float MinimumBlastDamage = 0.0f;
    float BlastRadius = 0.0f;
    float FireInterval = 0.2f;
    float SpreadDegrees = 0.0f;
    float LockSeconds = 0.0f;
    float MaxRangeMeters = 5000.0f;
    float ProjectileSpeed = 36000.0f;
};

struct FRotorlineAircraftWeaponLoadout
{
    bool bEnabled = false;
    float GunDeploymentDuration = 0.85f;
    float GunDeploymentDistance = 135.0f;
    float PodDeploymentDuration = 0.95f;
    float PodDeploymentDistance = 105.0f;
    float PodArcDegrees = 90.0f;
    float RetractionDelay = 0.25f;
    float ConvergenceDistanceMeters = 1200.0f;
    FVector LeftGunMount = FVector(20.0f, -155.0f, -58.0f);
    FVector RightGunMount = FVector(20.0f, 155.0f, -58.0f);
    FVector BellyPodMount = FVector(-5.0f, 0.0f, -92.0f);
    TArray<FRotorlineAircraftWeaponModeDefinition> Modes;
};

struct FRotorlineAircraftExhaustOutlet
{
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator(0.0f, 180.0f, 0.0f);
    float Diameter = 26.0f;
};

struct FRotorlineAircraftExhaustConfig
{
    bool bEnabled = false;
    float PlumeWidth = 1.0f;
    float PlumeLength = 1.0f;
    float VelocityMultiplier = 1.0f;
    float VaporAmount = 1.0f;
    float DistortionIntensity = 1.0f;
    bool bStartupPuff = true;
    TArray<FRotorlineAircraftExhaustOutlet> Outlets;
};

struct FRotorlineAircraftDefinition
{
    FString Id;
    FString DisplayName;
    FString Manufacturer;
    FString Role;
    FString Summary;
    FString SpawnStatus;
    float FuelEnduranceSeconds = 600.0f;

    bool bHangarVisible = false;
    bool bAlphaSelectable = false;
    bool bDeploymentReady = false;
    bool bEnemyEligible = false;
    bool bReadyForHangar = false;

    FRotorlineAircraftStats Stats;
    FRotorlineAircraftSuitability MissionSuitability;

    FString PreferredGlb;
    TArray<FString> SourceVariants;
    FString License;
    FString Credit;
    FString SourceUrl;

    FString AssetRoot;
    FString BodyAsset;
    TArray<FString> BodyAssets;
    FString DeploymentClass;
    TArray<FString> RotorAssets;
    TArray<FRotorlineAircraftRotorGroup> RotorGroups;
    TArray<FString> StationaryRotorAssets;
    FString AnimationAsset;
    bool bAllowProceduralRotorFallback = false;
    float PresentationScale = 1.0f;
    float PresentationPitch = 0.0f;
    float PresentationYaw = 0.0f;
    float PresentationRoll = 0.0f;
    FVector PresentationOffset = FVector::ZeroVector;
    FVector SourceCenter = FVector::ZeroVector;
    float SourceMinimumZ = 0.0f;

    FString AudioStatus;
    FString PreIgnitionAudio;
    FString StartupAudio;
    FString TakeoffAudio;
    FString InflightAudio;
    FString AutocannonAudio;
    FRotorlineAircraftExhaustConfig Exhaust;
    FRotorlineAircraftWeaponLoadout WeaponLoadout;
    TArray<FString> Gaps;
};

class FRotorlineAircraftCatalog
{
public:
    static bool Load(TArray<FRotorlineAircraftDefinition>& OutAircraft, FString& OutError);
    static const FRotorlineAircraftDefinition* FindById(
        const TArray<FRotorlineAircraftDefinition>& Aircraft,
        const FString& Id);
    static int32 SuitabilityForMissionType(
        const FRotorlineAircraftDefinition& Aircraft,
        const FString& MissionType);
    static FString Stars(int32 Rating);
};
