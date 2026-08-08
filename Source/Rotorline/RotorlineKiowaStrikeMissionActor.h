#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineKiowaStrikeMissionActor.generated.h"

class ARotorlineHelicopterPawn;
class ARotorlineMissionObjectiveActor;
class UAudioComponent;
class USceneComponent;
class UStaticMeshComponent;
class USoundBase;
class UTextRenderComponent;

UENUM()
enum class ERotorlineKiowaStrikeState : uint8
{
    Initializing,
    TravelToReconZone,
    SearchingForTarget,
    IdentifyingTarget,
    DesignatingTarget,
    TargetLockAudio,
    BellAcceptsMission,
    BellAcknowledgesTarget,
    BellArrival,
    BellAttackRun,
    WeaponsReleased,
    TargetExplosion,
    BellConfirmsKill,
    BellGoodbye,
    BellAfterburnerEgress,
    MissionAccomplished,
    Complete,
    Failed
};

UCLASS()
class ROTORLINE_API ARotorlineKiowaStrikeMissionActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineKiowaStrikeMissionActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void Configure(ARotorlineHelicopterPawn* InPlayer, bool bInAlliedStrikeSequence);
    void BeginReconnaissance(ARotorlineMissionObjectiveActor* InPriorityTarget);
    void NotifyTargetLockPressed();

    bool IsComplete() const { return State == ERotorlineKiowaStrikeState::Complete; }
    bool IsFailed() const { return State == ERotorlineKiowaStrikeState::Failed; }
    bool IsTargetRevealed() const;
    bool IsSensorMissionActive() const;
    bool IsDialogueAudioPlaying() const;
    float GetDesignationProgress() const;
    FString GetSensorStatus() const;
    FString GetAlliedStrikeStatus() const;

private:
    void SetState(ERotorlineKiowaStrikeState NewState);
    void PlaySequenceCue(USoundBase* Sound, const TCHAR* CueName, bool bSpatialAtTarget = false);
    float CueDurationOrFallback(const USoundBase* Sound, float Fallback) const;
    bool HasValidSensorSolution() const;
    void UpdateSensor(float DeltaSeconds);
    void ActivateBell222();
    void UpdateBellFlight(float DeltaSeconds);
    void MoveBellToward(const FVector& Destination, float DesiredSpeed, float DeltaSeconds);
    void FireStrikeMissile();
    void LoadMissionAudio();
    void StopAllAudio();
    static const TCHAR* StateName(ERotorlineKiowaStrikeState Value);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> BellModelRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> BellBody;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> BellMainRotorPivot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> BellMainRotor;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> BellTailRotorPivot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> BellTailRotor;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UTextRenderComponent> AlliedLabel;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAudioComponent> DialogueAudio;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAudioComponent> BellEngineAudio;

    UPROPERTY()
    TObjectPtr<UAudioComponent> SpatialExplosionAudio;

    UPROPERTY()
    TObjectPtr<USoundBase> StartupAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> TargetLockedAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> BellAcceptsMissionAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> BellAcknowledgesTargetAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> BellArrivalAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> BellFiresMissilesAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> TargetExplosionAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> BellConfirmsTargetDestroyedAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> BellSaysGoodbyeAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> BellAfterburnerAudio;
    UPROPERTY()
    TObjectPtr<USoundBase> MissionAccomplishedAudio;

    TWeakObjectPtr<ARotorlineHelicopterPawn> PlayerHelicopter;
    TWeakObjectPtr<ARotorlineMissionObjectiveActor> PriorityTarget;
    ERotorlineKiowaStrikeState State = ERotorlineKiowaStrikeState::Initializing;
    float StateElapsed = 0.0f;
    float IdentificationProgress = 0.0f;
    float DesignationProgress = 0.0f;
    float LockLossElapsed = 0.0f;
    float BellCurrentSpeed = 0.0f;
    float BellRotorDegrees = 0.0f;
    int32 StrikeMissilesFired = 0;
    bool bLockRequested = false;
    bool bAlliedStrikeSequence = true;
    bool bStartupPlayed = false;
    bool bBellActivated = false;
    bool bBellFinalApproachStarted = false;
    bool bTargetExplosionObserved = false;

    FVector BellSpawnPoint = FVector::ZeroVector;
    FVector BellEntryPoint = FVector::ZeroVector;
    FVector BellApproachPoint = FVector::ZeroVector;
    FVector BellReleasePoint = FVector::ZeroVector;
    FVector BellEgressPoint = FVector::ZeroVector;
};
