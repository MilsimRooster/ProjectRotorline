#pragma once

#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/Pawn.h"
#include "RotorlineAircraftCatalog.h"
#include "RotorlineMissionCatalog.h"
#include "RotorlineHelicopterPawn.generated.h"

class UAnimSequence;
class UAudioComponent;
class UBoxComponent;
class UCameraComponent;
class UCurveFloat;
class UExponentialHeightFogComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USpotLightComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USoundBase;
class ARotorlineMissionObjectiveActor;
class ARotorlineCabinSupplyConvoyActor;
class ARotorlineKiowaStrikeMissionActor;
class ARotorlineFinalCinematicActor;
class AStaticMeshActor;
class ARotorlineRocketProjectile;
class ARotorlineCannonProjectile;
enum class ERotorlineEnemyWeaponType : uint8;
enum class ERotorlineAudioChannel : uint8;

enum class ERotorlineBellWeaponMode : uint8
{
    Safe,
    Chain50,
    Cannon40,
    Linked,
    Aim9,
    Hellfire,
    Maverick
};

struct FRotorlineCockpitHUDState
{
    FString AircraftName;
    FString MissionCallsign;
    FString StartupPhase;
    float SpeedKph = 0.0f;
    float AltitudeAglMeters = 0.0f;
    float DescentRateMps = 0.0f;
    float SafeSkidSpeedKph = 45.0f;
    float SafeDescentRateMps = 4.0f;
    float HeadingDegrees = 0.0f;
    float HullPercent = 100.0f;
    float FuelPercent = 100.0f;
    float RotorPercent = 0.0f;
    float EngineReadySeconds = 0.0f;
    int32 RocketAmmo = 0;
    int32 RocketCapacity = 0;
    int32 CannonAmmo = 0;
    int32 CannonCapacity = 0;
    float CannonHeatPercent = 0.0f;
    int32 Countermeasures = 0;
    int32 CountermeasureCapacity = 0;
    float CountermeasureCooldown = 0.0f;
    bool bEngineReady = false;
    bool bMissionBriefActive = false;
    bool bArmed = false;
    bool bMissileLockMode = false;
    bool bCombatZoom = false;
    bool bCannonOverheated = false;
    bool bPlayerAircraftDying = false;
    bool bBellWeaponSystem = false;
    bool bStealthActive = false;
    float StealthSecondsRemaining = 0.0f;
    float StealthCooldownSecondsRemaining = 0.0f;
    FString SelectedWeapon;
    FString WeaponSystemState;
    FString WeaponTarget;
    FString WeaponLockState;
    int32 SelectedWeaponAmmo = 0;
    int32 SelectedWeaponCapacity = 0;
    float WeaponLockProgress = 0.0f;
    bool bReconStrikeSensor = false;
    float ReconDesignationProgress = 0.0f;
    FString ReconSensorStatus;
    FString AlliedStrikeStatus;
};

struct FRotorlineAwardsFlightState
{
    FVector Velocity = FVector::ZeroVector;
    float AltitudeAglMeters = 0.0f;
    float PitchDegrees = 0.0f;
    float RollDegrees = 0.0f;
    float Health = 0.0f;
    float MaxHealth = 0.0f;
    float FuelRemainingPercent = 100.0f;
    bool bEnginePowerAvailable = false;
    bool bMissionFailed = false;
    bool bAircraftDying = false;
};

UCLASS()
class ROTORLINE_API ARotorlineHelicopterPawn : public APawn
{
    GENERATED_BODY()

public:
    ARotorlineHelicopterPawn();

    virtual void Tick(float DeltaSeconds) override;
    bool IsSpokenDialogueActive() const;
    bool IsBellLairAuthorizedAircraft() const;
    const FString& GetSelectedAircraftId() const { return SelectedAircraftId; }
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void ConfigureDeployment(ERotorlineCraftType Craft, const FRotorlineMissionDefinition& Mission);
    void ConfigureDeployment(const FRotorlineAircraftDefinition& Aircraft, const FRotorlineMissionDefinition& Mission);
    void SetFleetQualificationMode(bool bSkipStartup);
    void SkipStartupForPlaytest();
    void BeginTransitionSpawnHold(float MinimumSeconds = 0.85f);
    void SetEnemyFlightTestMode(bool bEnabled) { bEnemyFlightTestMode = bEnabled; }
    void SetCombatPreviewMode(bool bEnabled) { bCombatPreviewMode = bEnabled; }
    bool GetMissionNavigationData(FVector& OutWorldLocation, FString& OutLabel, int32& OutObjectiveIndex, int32& OutObjectiveCount) const;
    bool GetKiowaSensorTargetData(FVector& OutWorldLocation, FString& OutLabel) const;
    bool GetThreatNavigationData(FVector& OutWorldLocation, FString& OutLabel) const;
    bool GetRadioChatter(FString& OutMessage) const;
    float GetRadioChatterFadeAlpha() const;
    bool GetBaseRearmStatus(FString& OutStatus, bool& bOutServicing) const;
    FVector GetMissionWorldLocation(const FVector& BrowserLocation) const;
    FVector GetMissionWorldLocation(const FRotorlineObjectiveDefinition& Objective) const;
    FVector ResolveMissionObjectiveWorld(const FRotorlineObjectiveDefinition& Objective) const { return ResolveObjectiveWorld(Objective); }
    bool IsMissionFailed() const { return bMissionFailed; }
    bool IsMissionFailureMenuReady() const { return bMissionFailed && (!bPlayerAircraftDying || bPlayerCrashImpact || PlayerDeathElapsed >= 12.0f); }
    bool IsPlayerAircraftDying() const { return bPlayerAircraftDying; }
    const FString& GetMissionFailureReason() const { return MissionFailureReason; }
    bool GetApacheWeaponAimSolution(
        FVector& OutMuzzleLocation,
        FVector& OutAimDirection,
        FVector& OutImpactLocation,
        bool& bOutBlockingHit) const;
    bool HasAttackCombatPackage() const { return bSelectedAircraftArmed; }
    const FString& GetMissionWeather() const { return ActiveMission.Weather; }
    bool IsApacheMissileLockMode() const { return IsBell222SpecialOperations() ? IsBell222MissileMode() : bApacheMissileLockMode; }
    bool IsApacheCombatZoomEnabled() const { return bApacheCombatZoomEnabled; }
    bool GetBellWeaponTargetData(FVector& OutWorldLocation, FString& OutLabel) const;
    int32 GetCountermeasureCharges() const { return CountermeasureCharges; }
    float GetCountermeasureCooldownRemaining() const;
    bool IsBell222StealthActive() const { return bStealthActive; }
    FRotorlineCockpitHUDState GetCockpitHUDState() const;
    FRotorlineAwardsFlightState GetAwardsFlightState() const;
    void ApplyEnemyProjectileHit(float Damage, const FVector& ImpactImpulse = FVector::ZeroVector);
    void HandleEnemyAircraftCollision(const FString& EnemyAirframe, float RelativeImpactSpeed);
    void RefreshAudioMix();
    void ToggleNightVision();
    void LogMissionLoopResetSnapshot(int32 Generation) const;
    FVector GetFlightVelocity() const { return CurrentVelocity; }
    bool IsEngineReadyForMission() const { return bEngineReady; }
    float GetMissionRadioVolume() const;
    float GetMissionEngineVolume() const;

    UFUNCTION(BlueprintCallable, Category = "Rotorline|Audit")
    bool RunAlertIsolationAudit();

protected:
    virtual void BeginPlay() override;

private:
    void SetMouseCaptured(bool bCaptured);
    void UpdateCamera(float DeltaSeconds);
    void BeginRotorSpoolAudio();
    void BeginTakeoffEngineAudio();
    void BeginFlightEngineAudio();
    void PlayFinalMissionCallout(const TCHAR* AssetPath, const TCHAR* EventName);
    void UpdateFinalMissionCargo(const FRotorlineObjectiveDefinition& CompletedObjective);
    void UpdateFinalMissionCargoSling(float DeltaSeconds);
    void SpawnFinalMissionSetPieces();
    void ClearFinalMissionPressureActors();
    void ClearFinalMissionSetPieces();
    void StartMissionBriefPlayback(const TCHAR* Trigger);
    void FinishMissionBrief();
    void ReleaseMissionRadioHold();
    bool IsMissionRadioHoldActive() const;
    void UpdateEngineStartup(float DeltaSeconds);
    void UpdateFlight(float DeltaSeconds);
    void UpdateBellLairTransitCollision();
    void HandleFlightCollision(const FHitResult& Hit, const FVector& PreImpactVelocity);
    void UpdateFuel(float DeltaSeconds);
    void UpdatePlayerDestruction(float DeltaSeconds);
    void BeginPlayerDestruction();
    void UpdateEngineAudio(float DeltaSeconds);
    void InitializeRotorDownwash();
    void UpdateRotorDownwash(float DeltaSeconds);
    void UpdateControllerVibration();
    void UpdateNightOpsLights();
    void UpdateMissionRuntime();
    void UpdateBaseRearm(float DeltaSeconds);
    void DiscoverMapHelipads();
    void UpdateWeaponModeInput();
    void FireCountermeasures();
    void UpdateFlightReadout();
    void RefreshMissionObjectiveActor();
    void FireMissionRocket();
    ARotorlineMissionObjectiveActor* FindBestMissileLockTarget(float& OutDistanceMeters) const;
    void UpdateApacheCannon(float DeltaSeconds);
    void FireApacheCannon();
    void StartApacheCannonAudio(const TCHAR* Reason);
    void StopApacheCannonAudio(const TCHAR* Reason, float FadeOutSeconds = 0.035f);
    void UpdateMissionCombat(float DistanceMeters);
    void UpdateMissionPacing(float DistanceMeters);
    void UpdateEnemyHelicopterEncounterGate();
    void UpdateEnemyHelicopterEncounterGateQualification();
    void UpdateTutorialHelicopterKillQualification();
    void LogMissionLoopExpectedStats(float ElapsedSeconds) const;
    bool CanSpawnEnemyHelicopterEncounter(const TCHAR* Source);
    void RegisterEnemyHelicopterEncounter(ARotorlineMissionObjectiveActor* Actor, const TCHAR* Source);
    void RetireEnemyHelicopterEncounter(ARotorlineMissionObjectiveActor* Actor, const TCHAR* Reason, bool bDestroyActor);
    void StartEnemyHelicopterCooldown(const TCHAR* Reason);
    int32 CountActiveEnemyHelicopters() const;
    bool IsMissionAirObjective(const FRotorlineObjectiveDefinition& Objective) const;
    void LogEnemyHelicopterSpawnBlocked(const TCHAR* Source, const TCHAR* Reason, double CooldownRemaining);
    void UpdateMissionMusic();
    void LoadRadioCallouts();
    void BroadcastRadio(const FString& Message, float Duration = 6.0f, bool bPlaySquelch = false);
    void UpdateQueuedRadio();
    void PlayThreatAlert(USoundBase* AlertSound, float BaseVolume);
    void SpawnTransitThreat(const FVector& ObjectiveWorld);
    void SpawnKiowaReconGroundHarassment();
    void FireEnemyShot(ARotorlineMissionObjectiveActor* Site, ERotorlineEnemyWeaponType WeaponType, float Damage);
    void ResetCombatThreatState();
    void ApplyMissionConditions();
    void ConfigureWeatherPrecipitation(bool bEnabled);
    void FailMission(const FString& Reason, bool bDestroyPlayerAircraft = true);
    float GetAboveGroundMeters() const;
    FVector MissionLocationToWorld(const FVector& BrowserLocation) const;
    FVector ResolveObjectiveWorld(const FRotorlineObjectiveDefinition& Objective) const;
    void CompleteCurrentObjective();
    void ApplyCraftConfiguration();
    void UpdateAircraftLightStations();
    void RouteMissionBriefAudio();
    void ApplyCatalogAircraftConfiguration();
    void ResetCatalogAircraftComponents();
    void AddCatalogFallbackMainRotor(const FBox& Bounds);
    void AddCatalogFallbackTailRotor(const FBox& Bounds);
    void UpdateCatalogRotorVisuals(float DeltaSeconds);
    void ApplyActiveRotorAnimationRates();
    void UpdateHueyRotorAnimation(float DeltaSeconds);
    void UpdateMD500RotorAnimation(float DeltaSeconds);
    void ConfigureAircraftExhaust();
    void ResetAircraftExhaust();
    void UpdateAircraftExhaust(float DeltaSeconds);
    void UpdateBell222LandingGear(float DeltaSeconds);
    void UpdateBell222BoostEffects(float DeltaSeconds);
    void ConfigureBell222WeaponComponents();
    void ResetBell222WeaponSystem();
    void UpdateBell222WeaponSystem(float DeltaSeconds);
    void UpdateBell222WeaponVisuals(float DeltaSeconds);
    void InitializeBell222StealthInput();
    void InitializeBell222StealthMaterials();
    void ResetBell222StealthMaterials();
    void ApplyBell222StealthMaterials();
    void RestoreBell222OriginalMaterials();
    void ToggleBell222Stealth();
    void SetBell222StealthActive(bool bActive, const TCHAR* Reason);
    void PlayBell222StealthTransitionAudio(bool bActivating);
    UFUNCTION()
    void UpdateBell222StealthTimeline(float Value);
    UFUNCTION()
    void FinishBell222StealthTimeline();
    void CycleBell222Weapon(int32 Direction);
    void FireBell222GunMode();
    void FireBell222MissileMode();
    void FireBell222ProjectilePair(const FRotorlineAircraftWeaponModeDefinition& Mode, bool bFortyMillimeter);
    ARotorlineMissionObjectiveActor* FindBestBell222Target(
        const FRotorlineAircraftWeaponModeDefinition& Mode,
        float& OutDistanceMeters,
        bool& bOutInsideArc) const;
    bool IsBell222TargetValid(
        const ARotorlineMissionObjectiveActor* Candidate,
        const FRotorlineAircraftWeaponModeDefinition& Mode,
        float& OutDistanceMeters,
        bool& bOutInsideArc) const;
    const FRotorlineAircraftWeaponModeDefinition* GetBell222WeaponDefinition() const;
    const FRotorlineAircraftWeaponModeDefinition* FindBell222WeaponDefinition(const TCHAR* Id) const;
    FString GetBell222WeaponSystemState() const;
    bool IsBell222SpecialOperations() const;
    bool IsBell222GunMode() const;
    bool IsBell222MissileMode() const;
    int32 GetBell222WeaponAmmo() const;
    int32 GetBell222WeaponCapacity() const;
    void TriggerBell222MuzzleFlash(int32 MuzzleIndex, float Duration);
    void UpdateFleetQualification(float DeltaSeconds);
    FString GetCraftDisplayName() const;
    TArray<USkeletalMeshComponent*> GetActiveRotors() const;
    float GetAudioMix(ERotorlineAudioChannel Channel) const;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UBoxComponent> CollisionBox;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USceneComponent> VisualRoot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USceneComponent> MeshAlignment;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> GlassMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Combat")
    TObjectPtr<UStaticMeshComponent> PlayerExplosionCore;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Combat")
    TObjectPtr<UStaticMeshComponent> PlayerExplosionSmoke;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Combat")
    TObjectPtr<UStaticMeshComponent> PlayerExplosionSparks;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Combat")
    TObjectPtr<UPointLightComponent> PlayerExplosionLight;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Night Ops")
    TObjectPtr<UPointLightComponent> LeftNavigationLight;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Night Ops")
    TObjectPtr<UPointLightComponent> RightNavigationLight;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Night Ops")
    TObjectPtr<UPointLightComponent> TailStrobeLight;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Night Ops")
    TObjectPtr<USpotLightComponent> LandingLight;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Night Ops")
    TObjectPtr<UStaticMeshComponent> LeftNavigationBulb;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Night Ops")
    TObjectPtr<UStaticMeshComponent> RightNavigationBulb;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Night Ops")
    TObjectPtr<UStaticMeshComponent> TailStrobeBulb;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USceneComponent>> ExhaustOutletRoots;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UNiagaraComponent>> ExhaustComponents;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraSystem> TurboshaftExhaustSystem;

    UPROPERTY()
    TObjectPtr<USoundBase> PlayerExplosionSound;

    UPROPERTY()
    TObjectPtr<UNiagaraSystem> PlayerAirExplosionSystem;

    UPROPERTY()
    TObjectPtr<UNiagaraSystem> PlayerGroundExplosionSystem;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|FX")
    TObjectPtr<UStaticMeshComponent> DownwashGroundSheet;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|FX")
    TObjectPtr<UStaticMeshComponent> DownwashGroundSheetSecondary;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DownwashGroundMaterial;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DownwashGroundMaterialSecondary;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> DownwashPuffMeshes;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DownwashPuffMaterials;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> InteriorMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USkeletalMeshComponent> MainRotorMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USkeletalMeshComponent> TailRotorMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USceneComponent> HueyMainRotorPivot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USceneComponent> HueyTailRotorPivot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USceneComponent> MD500SuperiorRoot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> MD500BodyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> MD500AccessoryMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> MD500WeaponMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Weapons")
    TObjectPtr<USceneComponent> MD500LeftGunMuzzle;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Weapons")
    TObjectPtr<USceneComponent> MD500RightGunMuzzle;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Weapons")
    TObjectPtr<USceneComponent> MD500LeftRocketMuzzle;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Weapons")
    TObjectPtr<USceneComponent> MD500RightRocketMuzzle;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> MD500CockpitMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> MD500GlassMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USceneComponent> MD500MainRotorPivot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> MD500MainRotorMountMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> MD500MainRotorRotatingMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USceneComponent> MD500TailRotorPivot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> MD500TailRotorMountMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> MD500TailRotorRotatingMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<UStaticMeshComponent> CatalogBodyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USkeletalMeshComponent> CatalogSkeletalBodyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TArray<TObjectPtr<UStaticMeshComponent>> CatalogStaticRotors;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TArray<TObjectPtr<USkeletalMeshComponent>> CatalogSkeletalRotors;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TArray<TObjectPtr<UStaticMeshComponent>> CatalogDynamicStaticParts;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TArray<TObjectPtr<USkeletalMeshComponent>> CatalogDynamicSkeletalParts;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222")
    TObjectPtr<UStaticMeshComponent> Bell222LandingGearMesh;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222")
    TArray<TObjectPtr<UStaticMeshComponent>> Bell222CompactGearParts;

    TArray<FVector> Bell222CompactGearBaseLocations;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222")
    TObjectPtr<UStaticMeshComponent> Bell222LeftBoostPlume;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222")
    TObjectPtr<UStaticMeshComponent> Bell222RightBoostPlume;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222")
    TObjectPtr<UPointLightComponent> Bell222LeftBoostLight;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222")
    TObjectPtr<UPointLightComponent> Bell222RightBoostLight;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TObjectPtr<USceneComponent> Bell222LeftGunRoot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TObjectPtr<USceneComponent> Bell222RightGunRoot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TObjectPtr<UStaticMeshComponent> Bell222LeftSourceGun;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TObjectPtr<UStaticMeshComponent> Bell222RightSourceGun;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TArray<TObjectPtr<UStaticMeshComponent>> Bell222GunHousings;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TArray<TObjectPtr<UStaticMeshComponent>> Bell222GunDoors;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TArray<TObjectPtr<UStaticMeshComponent>> Bell222GunBarrels;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TArray<TObjectPtr<UStaticMeshComponent>> Bell222MuzzleFlashes;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TObjectPtr<USceneComponent> Bell222MissilePodRoot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TArray<TObjectPtr<UStaticMeshComponent>> Bell222MissilePodDoors;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TObjectPtr<UStaticMeshComponent> Bell222MissilePodBody;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Bell 222|Weapons")
    TArray<TObjectPtr<UStaticMeshComponent>> Bell222PodMissiles;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TArray<TObjectPtr<USceneComponent>> CatalogMainRotorParts;

    TArray<FVector> CatalogMainRotorAxes;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TArray<TObjectPtr<USceneComponent>> CatalogTailRotorParts;

    TArray<FVector> CatalogTailRotorAxes;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TArray<TObjectPtr<USceneComponent>> CatalogRotorPivots;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USceneComponent> CatalogFallbackMainRotorPivot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Helicopter")
    TObjectPtr<USceneComponent> CatalogFallbackTailRotorPivot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Camera")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Weather")
    TObjectPtr<UExponentialHeightFogComponent> MissionFog;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraSystem> RainPrecipitationSystem;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UNiagaraComponent>> WeatherPrecipitationComponents;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Weapons")
    TObjectPtr<UStaticMeshComponent> KiowaLeftMinigun;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Weapons")
    TObjectPtr<UStaticMeshComponent> KiowaRightRocketPod;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> EngineStartupAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> EngineTakeoffAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> EngineFlightAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> Bell222BoostAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> Bell222StealthAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> MissionBriefAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> InstructorAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> MissionMusicAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> RadioAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> RadioSquelchAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> ThreatAlertAudio;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Audio")
    TObjectPtr<UAudioComponent> ApacheCannonAudio;

    UPROPERTY()
    TObjectPtr<USoundBase> EnginePreIgnitionSound;

    UPROPERTY()
    TObjectPtr<USoundBase> EngineStartupSound;

    UPROPERTY()
    TObjectPtr<USoundBase> EngineTakeoffSound;

    UPROPERTY()
    TObjectPtr<USoundBase> EngineFlightLoopSound;

    UPROPERTY()
    TObjectPtr<USoundBase> MissionBriefSound;

    UPROPERTY()
    TObjectPtr<USoundBase> MusicCombatSound;

    UPROPERTY()
    TObjectPtr<USoundBase> MusicMission1Sound;

    UPROPERTY()
    TObjectPtr<USoundBase> MusicMission2Sound;

    UPROPERTY()
    TObjectPtr<USoundBase> MusicMission3Sound;

    UPROPERTY()
    TObjectPtr<USoundBase> MusicRescueSound;

    UPROPERTY()
    TArray<TObjectPtr<USoundBase>> RotorlineGameplayMusic;

    UPROPERTY()
    TObjectPtr<USoundBase> KiowaReconMissionMusicSound;

    UPROPERTY()
    TObjectPtr<USoundBase> Bell222MissionMusicSound;

    UPROPERTY()
    TObjectPtr<USoundBase> Bell222FinalMissionBriefSound;

    UPROPERTY()
    TObjectPtr<USoundBase> Bell222BoostSound;

    UPROPERTY()
    TObjectPtr<USoundBase> Bell222StealthSound;

    UPROPERTY()
    TObjectPtr<USoundBase> Bell222DecloakSound;

    FTimerHandle Bell222BoostFadeTimer;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> Bell222StealthParentMaterial;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> Bell222OriginalMaterials;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> Bell222StealthMaterialInstances;

    TArray<TWeakObjectPtr<UMeshComponent>> Bell222StealthMeshComponents;
    TArray<int32> Bell222StealthMaterialSlots;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> Bell222ToggleStealthAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> Bell222StealthMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UCurveFloat> Bell222StealthCurve;

    FTimeline Bell222StealthTimeline;

    UPROPERTY()
    TObjectPtr<USoundBase> RadioSquelchSound;

    UPROPERTY()
    TMap<FString, TObjectPtr<USoundBase>> RadioCalloutSounds;

    UPROPERTY()
    TObjectPtr<USoundBase> ThreatWarningSound;

    UPROPERTY()
    TObjectPtr<USoundBase> ThreatLockedSound;

    UPROPERTY()
    TObjectPtr<USoundBase> RadarHomingSound;

    UPROPERTY()
    TObjectPtr<USoundBase> RadarLockedSound;

    UPROPERTY()
    TObjectPtr<USoundBase> ApacheCannonSound;

    UPROPERTY()
    TObjectPtr<USoundBase> CurrentMissionMusic;

    UPROPERTY()
    TObjectPtr<USoundBase> HueyEngineStartupSound;

    UPROPERTY()
    TObjectPtr<USoundBase> HueyEngineFlightLoopSound;

    UPROPERTY()
    TObjectPtr<USoundBase> HueyMissionBriefSound;

    UPROPERTY()
    TObjectPtr<USoundBase> Mission1BriefSound;

    UPROPERTY()
    TObjectPtr<USoundBase> MD500EngineStartupSound;

    UPROPERTY()
    TObjectPtr<USoundBase> MD500EngineTakeoffSound;

    UPROPERTY()
    TObjectPtr<USoundBase> MD500EngineFlightLoopSound;

    UPROPERTY()
    TObjectPtr<UAnimSequence> RotorAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> HueyRotorAnimation;

    UPROPERTY()
    TObjectPtr<ARotorlineMissionObjectiveActor> ActiveObjectiveActor;

    UPROPERTY()
    TObjectPtr<ARotorlineCabinSupplyConvoyActor> ActiveCabinSupplyConvoy;

    UPROPERTY()
    TObjectPtr<ARotorlineKiowaStrikeMissionActor> ActiveKiowaStrikeMission;

    UPROPERTY()
    TObjectPtr<ARotorlineFinalCinematicActor> ActiveFinalCinematic;

    UPROPERTY()
    TObjectPtr<AStaticMeshActor> ActiveFinalMissionCargo;

    UPROPERTY()
    TArray<TObjectPtr<ARotorlineMissionObjectiveActor>> FinalMissionPressureActors;

    UPROPERTY()
    TArray<TObjectPtr<AStaticMeshActor>> FinalMissionCrateActors;

    UPROPERTY()
    TArray<TObjectPtr<AStaticMeshActor>> FinalMissionDeliveredCrates;

    UPROPERTY()
    TObjectPtr<ARotorlineMissionObjectiveActor> TransitThreatActor;

    UPROPERTY()
    TObjectPtr<ARotorlineMissionObjectiveActor> BurstFireSite;

    UPROPERTY()
    TObjectPtr<ARotorlineMissionObjectiveActor> PendingMissileSite;

    TMap<TWeakObjectPtr<ARotorlineMissionObjectiveActor>, double> LastThreatAttackTimes;
    TMap<TWeakObjectPtr<ARotorlineMissionObjectiveActor>, int32> ThreatAttackSequences;
    TMap<FString, double> LastRadioMessageTimes;
    TMap<FString, double> LastTacticalRadioCategoryTimes;
    bool bRadarLockCalloutPlayed = false;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float MaxForwardSpeed = 5200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float MaxReverseSpeed = 2300.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float MaxStrafeSpeed = 2800.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float MaxVerticalSpeed = 2400.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float MaxYawRate = 55.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float MaxPitchAngle = 31.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float MaxRollAngle = 36.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float CyclicAcceleration = 1900.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float CollectiveAcceleration = 1500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float ForwardDrag = 0.16f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float LateralDrag = 0.42f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float VerticalDrag = 0.65f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float CyclicResponse = 2.8f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float YawResponse = 3.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float BankLiftGravity = 980.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float TranslationalLiftAcceleration = 85.0f;

    // Airframe-specific coupling terms. These remain zero for the existing
    // fleet and let conventional tail-rotor aircraft model translating
    // tendency and uncompensated main-rotor torque without changing controls.
    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float PedalSideforceAcceleration = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float CollectiveTorqueYawRate = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    FVector RotorDiscBiasAcceleration = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float CameraFollowResponse = 4.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float ApacheCombatZoomFOV = 36.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float ForwardViewFOV = 58.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float ApacheCombatZoomForwardClearance = 180.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float ApacheCombatZoomMinimumHeight = 45.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float ApacheCombatZoomCameraResponse = 1.1f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float ApacheCombatZoomInputExponent = 2.45f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float ApacheCombatZoomInputScale = 0.28f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float ApacheCombatZoomFineAimDeadZone = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
	float ApacheCombatZoomPitchFineAimDeadZone = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
	float ApacheCombatZoomPitchInputExponent = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
	float ApacheCombatZoomPitchInputScale = 0.16f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float ApacheCombatZoomFullAuthorityStart = 0.70f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
	float ApacheCombatZoomPitchAuthorityStart = 0.82f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
	float ApacheCombatZoomPitchAuthorityEnd = 0.98f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float VelocityResponse = 1.9f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float AttitudeResponse = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Flight")
    float BoostMultiplier = 1.35f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Camera")
    float LookSensitivity = 0.10f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Audio")
    float EngineCrossfadeSeconds = 2.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Audio")
    float EngineFlightVolume = 0.64f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Audio")
    float EngineStartupVolume = 0.72f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Audio")
    float EngineDuckedVolume = 0.30f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Audio")
    float MissionBriefVolume = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Audio")
    float MissionMusicVolume = 0.40f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Audio")
    float ThreatAlertBaseVolume = 0.40f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Audio")
    float RotorStartPlayRate = 0.05f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Audio")
    float RotorFlightPlayRate = 6.0f;

    FVector CurrentVelocity = FVector::ZeroVector;
    FVector CurrentObjectiveWorldLocation = FVector::ZeroVector;
    float CurrentYawRate = 0.0f;
    float CurrentPitchAngle = 0.0f;
    float CurrentRollAngle = 0.0f;
    TArray<FVector> DownwashPuffVelocities;
    TArray<float> DownwashPuffAges;
    TArray<float> DownwashPuffLifetimes;
    TArray<float> DownwashPuffStartScales;
    TArray<float> DownwashPuffBaseOpacities;
    float DownwashSpawnAccumulator = 0.0f;
    float DownwashAuditTime = 0.0f;
    int32 DownwashNextPuffIndex = 0;
    bool bDownwashInitialized = false;
    FVector MissionWindAcceleration = FVector::ZeroVector;
    float ForwardInput = 0.0f;
    float StrafeInput = 0.0f;
    float CollectiveInput = 0.0f;
    float YawInput = 0.0f;
    float TransitionSpawnHoldRemaining = 0.0f;
    bool bTransitionSpawnAwaitingNeutral = false;
    TSet<FName> ControllerAcceptanceAxesLogged;
    double LastControllerAcceptanceAxisTime = -1000.0;
    bool bControllerAcceptanceTakeoffLogged = false;
    bool bMouseCaptured = false;
    bool bEngineReady = false;
    bool bFuelStarved = false;
    bool bFuelLowWarningIssued = false;
    bool bFuelCriticalWarningIssued = false;
    bool bFuelFumesWarningIssued = false;
    float FuelRemainingPercent = 100.0f;
    float CurrentFuelBurnMultiplier = 1.0f;
    float Bell222LandingGearAlpha = 0.0f;
    bool bBell222LandingGearRetracted = false;
    float Bell222SmoothedForwardInput = 0.0f;
    float Bell222SmoothedCollectiveInput = 0.0f;
    float Bell222CurrentSpeedScale = 1.0f;
    bool bBoostActive = false;
    bool bBell222BoostEffectActive = false;
    bool bStealthActive = false;
    bool bBell222StealthMaterialsApplied = false;
    bool bBell222StealthInputBound = false;
    bool bBell222StealthB6WasPressed = false;
    bool bBell222DecloakAudioPrimed = false;
    float Bell222StealthAmount = 0.0f;
    double Bell222StealthExpiresAt = -1000.0;
    double Bell222StealthCooldownUntil = -1000.0;
    ERotorlineBellWeaponMode Bell222WeaponMode = ERotorlineBellWeaponMode::Safe;
    TMap<FString, int32> Bell222WeaponAmmo;
    TMap<FString, int32> Bell222WeaponCapacity;
    float Bell222GunDeploymentAlpha = 0.0f;
    float Bell222PodDeploymentAlpha = 0.0f;
    float Bell222WeaponLockProgress = 0.0f;
    float Bell222LinkedHeat = 0.0f;
    float Bell222PodYawDegrees = 0.0f;
    double Bell222ModeSelectedTime = -1000.0;
    double Bell222LastGunFireTime = -1000.0;
    double Bell222LastMissileFireTime = -1000.0;
    TArray<double> Bell222MuzzleFlashUntil;
    TWeakObjectPtr<ARotorlineMissionObjectiveActor> Bell222LockedTarget;
    mutable TWeakObjectPtr<ARotorlineMissionObjectiveActor> MissileLockedTarget;
    bool bFlightEngineAudioStarted = false;
    bool bMissionBriefActive = false;
    bool bMissionBriefPlaybackStarted = false;
    bool bMission1RememberCalloutPlayed = false;
    bool bDeploymentConfigured = false;
    bool bControllerVibrationActive = false;
    bool bLastInputWasGamepad = false;
    bool bApacheCombatZoomEnabled = false;
    float ZoomPitchCommandAngle = 0.0f;
    bool bZoomPitchCommandInitialized = false;
    bool bNightVisionEnabled = false;
    float NormalCameraArmLength = 1150.0f;
    FVector NormalCameraSocketOffset = FVector(0.0f, 0.0f, 230.0f);
    bool bMissionFailed = false;
    bool bPlayerAircraftDying = false;
    bool bPlayerCrashImpact = false;
    bool bMissionComplete = false;
    bool bLastAlertIsolationPassed = true;
    bool bTransitThreatAnnounced = false;
    ERotorlineCraftType SelectedCraft = ERotorlineCraftType::SupportHuey;
    FRotorlineAircraftDefinition SelectedAircraftDefinition;
    FString SelectedAircraftId;
    bool bUseCatalogAircraft = false;
    bool bSelectedAircraftArmed = false;
    bool bCatalogFallbackTailUsesXAxis = false;
    bool bFleetQualificationMode = false;
    bool bFleetQualificationSkipStartup = false;
    bool bEnemyFlightTestMode = false;
    bool bHomeSanctuaryActive = false;
    bool bCityServiceSanctuaryActive = false;
    bool bNightOperationLightsEnabled = false;
    bool bSearchlightEnabledByPlayer = true;
    bool bLandingGearExtended = true;
    bool bCockpitViewEnabled = false;
    bool bBell222CockpitViewEnabled = false;
    bool bBellLairWorldStaticPassThrough = false;
    bool bCombatPreviewMode = false;
    bool bFleetQualificationWeaponAttempted = false;
    bool bFleetQualificationWeaponPassed = false;
    bool bFleetQualificationCountermeasureAttempted = false;
    bool bFleetQualificationCountermeasurePassed = false;
    float FleetQualificationElapsed = 0.0f;
    float FleetQualificationMaxDisplacementMeters = 0.0f;
    float FleetQualificationMaxAttitudeDegrees = 0.0f;
    int32 FleetQualificationMilestone = 0;
    FRotorlineMissionDefinition ActiveMission;
    int32 CurrentObjectiveIndex = 0;
    int32 RocketAmmo = 0;
    int32 RocketAmmoCapacity = 0;
    int32 ApacheCannonAmmo = 0;
    int32 ApacheCannonAmmoCapacity = 0;
    int32 CountermeasureCharges = 0;
    int32 CountermeasureCapacity = 8;
    float BaseRearmProgress = 0.0f;
    bool bBaseRearmLatched = false;
    bool bInsideBaseServiceZone = false;
    bool bInsideCityServiceZone = false;
    bool bBaseRearmActive = false;
    double LastSafeServicePadContactTime = -1000.0;
    double LastServicePadContactLogTime = -1000.0;
    TArray<FVector> AdditionalServicePadLocations;
    mutable TMap<FString, FVector> SurveyedLandingSites;
    mutable TMap<FString, FVector> SurveyedGroundSites;
    bool bCombatLoopTestMode = false;
    bool bCombatLoopPlayerFatalTriggered = false;
    bool bHawkRidgeQualificationMode = false;
    bool bHawkRidgeQualificationLockObserved = false;
    bool bHawkRidgeQualificationLaunchObserved = false;
    float HawkRidgeQualificationElapsed = 0.0f;
    struct FEnemyHelicopterEncounter
    {
        TWeakObjectPtr<ARotorlineMissionObjectiveActor> Actor;
        FString Source;
        uint64 Generation = 0;
    };
    TArray<FEnemyHelicopterEncounter> ActiveEnemyHelicopterEncounters;
    double EnemyHelicopterCooldownUntil = -1000.0;
    double LastEnemyHelicopterSpawnAuthorizationTime = -1000.0;
    double LastEnemyHelicopterBlockLogTime = -1000.0;
    FString LastEnemyHelicopterBlockReason;
    uint64 EnemyHelicopterEncounterGeneration = 0;
    bool bEnemyHelicopterCooldownActive = false;
    bool bEnemyHelicopterTerminalShutdownLogged = false;
    bool bEnemyHelicopterEncounterGateTestMode = false;
    bool bMissionLoopTestMode = false;
    bool bTutorialHelicopterKillTestMode = false;
    int32 EnemyHelicopterEncounterGateTestStage = 0;
    double EnemyHelicopterEncounterGateTestStartTime = 0.0;
    double EnemyHelicopterEncounterGateTestCooldownStartTime = -1000.0;
    int32 TutorialHelicopterKillTestStage = 0;
    double TutorialHelicopterKillTestStartTime = 0.0;
    float ApacheCannonHeat = 0.0f;
    bool bApacheCannonOverheated = false;
    bool bApacheCannonAudioTriggerActive = false;
    bool bApacheMissileLockMode = true;
    float CurrentHealth = 100.0f;
    float MaxHealth = 100.0f;
    float PlayerDeathElapsed = 0.0f;
    float PlayerCrashImpactElapsed = 0.0f;
    float PlayerCrashFallSpeed = 0.0f;
    float PlayerCrashSpinDirection = 1.0f;
    FString MissionFailureReason;
    float MissionStartTime = 0.0f;
    float ObjectiveStartTime = 0.0f;
    double RadioMessageUntil = 0.0;
    double NextRadioPlaybackAllowedTime = -1000.0;
    double LastRadioChatterTime = -1000.0;
    double LastAmbientChatterTime = -1000.0;
    double MissionRadioHoldUntil = -1000.0;
    FString ActiveRadioMessage;
    TArray<FString> QueuedRadioMessages;
    TArray<float> QueuedRadioDurations;
    TSet<FString> PlayedRadioCalloutAssetsThisMission;
    int32 TransitThreatObjectiveIndex = INDEX_NONE;
    int32 TransitEncountersSpawned = 0;
    int32 TransitThreatAttackPasses = 0;
    double TransitThreatRetreatTime = -1000.0;
    double LastTransitEncounterTime = -1000.0;
    bool bTransitThreatHarmless = false;
    uint64 ControllerVibrationHandle = 0;
    float EngineStartupElapsed = 0.0f;
    float EngineSpoolDuration = 0.1f;
    double EngineReadyTime = -1.0;
    float CurrentRotorPlayRate = 0.0f;
    float MD500MainRotorIntegratedDegrees = 0.0f;
    float MD500TailRotorIntegratedDegrees = 0.0f;
    int32 LastSpoolMilestone = -1;
    bool bRotorSpoolStageActive = false;
    bool bExhaustWasRunning = false;
    bool bExhaustStartupPulsePending = false;
    float ExhaustStartupPulseRemaining = 0.0f;
    FTimerHandle EngineTransitionTimer;
    FTimerHandle EngineTakeoffTimer;
    FTimerHandle MissionBriefTimer;
    FTimerHandle MissionRadioHoldTimer;
    double LastWPress = -1000.0;
    double LastSPress = -1000.0;
    double LastAPress = -1000.0;
    double LastDPress = -1000.0;
    double LastSpacePress = -1000.0;
    double LastZPress = -1000.0;
    double LastCPress = -1000.0;
    double LastQPress = -1000.0;
    double LastEPress = -1000.0;
    double LastRocketFireTime = -1000.0;
    double LastApacheCannonFireTime = -1000.0;
    double LastCountermeasureFireTime = -1000.0;
    double LastEnemyAttackTime = -1000.0;
    double NextBurstShotTime = -1000.0;
    double PendingMissileLaunchTime = -1000.0;
    int32 BurstShotsRemaining = 0;
    uint8 BurstWeaponType = 0;
    float BurstDamage = 0.0f;
    bool bPendingHimarsLockedAlert = false;
    bool bPendingAircraftLockedAlert = false;
};
