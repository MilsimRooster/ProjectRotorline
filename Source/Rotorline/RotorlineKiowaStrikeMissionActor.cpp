#include "RotorlineKiowaStrikeMissionActor.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RotorlineHelicopterPawn.h"
#include "RotorlineMissionObjectiveActor.h"
#include "RotorlineRocketProjectile.h"
#include "Sound/SoundBase.h"

namespace RotorlineKiowaStrike
{
    constexpr float RadarRangeCm = 420000.0f;
    constexpr float RadarConeDot = 0.9659258f; // 15 degrees.
    constexpr float IdentificationSeconds = 3.5f;
    constexpr float DesignationSeconds = 4.0f;
    constexpr float LockLossGraceSeconds = 0.40f;
    constexpr float ApproachSpeedCmPerSecond = 15000.0f;
    constexpr float AttackSpeedCmPerSecond = 22000.0f;
    constexpr float EgressSpeedCmPerSecond = 28500.0f;
    constexpr float BellAccelerationCmPerSecondSquared = 4800.0f;
    constexpr float BellEntryPhaseSeconds = 12.0f;
    constexpr float BellApproachPhaseSeconds = 24.0f;
    constexpr float BellFinalApproachStartSeconds = 26.0f;
    constexpr float BellFinalApproachTimeoutSeconds = 36.0f;
}

ARotorlineKiowaStrikeMissionActor::ARotorlineKiowaStrikeMissionActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    BellModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BellModelRoot"));
    BellModelRoot->SetupAttachment(Root);
    // The imported Bell body faces 90 degrees right of its authored forward
    // axis. M18 moves the actor along the attack path, so rotate the visual an
    // additional 90 degrees left to make the nose follow that path.
    BellModelRoot->SetRelativeRotation(FRotator(0.0f, -180.0f, 0.0f));

    BellBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BellBody"));
    BellBody->SetupAttachment(BellModelRoot);
    BellBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BellBody->SetCastShadow(true);

    BellMainRotorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BellMainRotorPivot"));
    BellMainRotorPivot->SetupAttachment(BellModelRoot);
    BellMainRotorPivot->SetRelativeLocation(FVector(-83.1f, 0.0f, 172.799f));
    BellMainRotor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BellMainRotor"));
    BellMainRotor->SetupAttachment(BellMainRotorPivot);
    BellMainRotor->SetRelativeLocation(FVector(83.1f, 0.0f, -172.799f));
    BellMainRotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BellTailRotorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BellTailRotorPivot"));
    BellTailRotorPivot->SetupAttachment(BellModelRoot);
    BellTailRotorPivot->SetRelativeLocation(FVector(734.8f, 46.1f, 65.9f));
    BellTailRotor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BellTailRotor"));
    BellTailRotor->SetupAttachment(BellTailRotorPivot);
    BellTailRotor->SetRelativeLocation(FVector(-734.8f, -46.1f, -65.9f));
    BellTailRotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    AlliedLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("AlliedLabel"));
    AlliedLabel->SetupAttachment(Root);
    AlliedLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 430.0f));
    AlliedLabel->SetHorizontalAlignment(EHTA_Center);
    AlliedLabel->SetText(FText::FromString(TEXT("ALLY // BELL 222")));
    AlliedLabel->SetTextRenderColor(FColor(90, 255, 165));
    AlliedLabel->SetWorldSize(52.0f);
    AlliedLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    DialogueAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("DialogueAudio"));
    DialogueAudio->SetupAttachment(Root);
    DialogueAudio->bAutoActivate = false;
    DialogueAudio->bIsUISound = true;

    BellEngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BellEngineAudio"));
    BellEngineAudio->SetupAttachment(Root);
    BellEngineAudio->bAutoActivate = false;
    BellEngineAudio->bAllowSpatialization = true;
    BellEngineAudio->AttenuationOverrides.bAttenuate = true;
    BellEngineAudio->AttenuationOverrides.FalloffDistance = 90000.0f;
    BellEngineAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(1800.0f);

    BellBody->SetVisibility(false, true);
    BellMainRotor->SetVisibility(false, true);
    BellTailRotor->SetVisibility(false, true);
    AlliedLabel->SetVisibility(false, true);
}

void ARotorlineKiowaStrikeMissionActor::Configure(
    ARotorlineHelicopterPawn* InPlayer,
    bool bInAlliedStrikeSequence)
{
    PlayerHelicopter = InPlayer;
    bAlliedStrikeSequence = bInAlliedStrikeSequence;
    if (bAlliedStrikeSequence)
    {
        LoadMissionAudio();
        BellBody->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Vehicles/UserAdded/CombatReady/Bell222X/Bell222X_Body/Bell222X_Body/StaticMeshes/Object_28.Object_28")));
        BellMainRotor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Vehicles/UserAdded/CombatReady/Bell222X/Bell222X_MainRotor/Bell222X_MainRotor/StaticMeshes/Bell222X_MainRotor.Bell222X_MainRotor")));
        BellTailRotor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Vehicles/UserAdded/CombatReady/Bell222X/Bell222X_TailRotor/Bell222X_TailRotor/StaticMeshes/Bell222X_TailRotor.Bell222X_TailRotor")));
        BellEngineAudio->SetSound(LoadObject<USoundBase>(nullptr,
            TEXT("/Game/Audio/Vehicles/Bell222X/SC_Bell222X_EngineInFlight_Loop.SC_Bell222X_EngineInFlight_Loop")));
    }
    else
    {
        // Silent Watch owns a dedicated briefing cue. Keep it on the Kiowa
        // mission coordinator so it starts once per deployment as ignition
        // begins, without changing Mission 19's later engine-ready callout.
        StartupAudio = LoadObject<USoundBase>(nullptr,
            TEXT("/Game/Audio/Missions/KiowaRecon/M6_startup_audio.M6_startup_audio"));
        if (StartupAudio)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_KIOWA_RECON|STARTUP_AUDIO_ASSET|asset=%s|result=PASS"),
                TEXT("/Game/Audio/Missions/KiowaRecon/M6_startup_audio.M6_startup_audio"));
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_KIOWA_RECON|STARTUP_AUDIO_ASSET|asset=%s|result=FAIL"),
                TEXT("/Game/Audio/Missions/KiowaRecon/M6_startup_audio.M6_startup_audio"));
        }
    }
    SetState(ERotorlineKiowaStrikeState::TravelToReconZone);
}

void ARotorlineKiowaStrikeMissionActor::BeginReconnaissance(ARotorlineMissionObjectiveActor* InPriorityTarget)
{
    if (!IsValid(InPriorityTarget) || (bAlliedStrikeSequence && PriorityTarget.IsValid())) return;

    // Silent Watch reuses the Kiowa sensor coordinator for each observation
    // point. Reset only the sensor state; the one-shot M19 strike sequence
    // remains terminal and cannot be restarted.
    if (!bAlliedStrikeSequence)
    {
        IdentificationProgress = 0.0f;
        DesignationProgress = 0.0f;
        LockLossElapsed = 0.0f;
        bLockRequested = false;
        State = ERotorlineKiowaStrikeState::TravelToReconZone;
        StateElapsed = 0.0f;
    }
    PriorityTarget = InPriorityTarget;
    PriorityTarget->SetMissionMarkerVisibility(!bAlliedStrikeSequence);
    const FVector Target = PriorityTarget->GetAimLocation();
    const FVector AttackDirection = FVector(0.82f, -0.57f, 0.0f).GetSafeNormal();
    const FVector AttackRight = FVector::CrossProduct(FVector::UpVector, AttackDirection).GetSafeNormal();
    BellSpawnPoint = Target - AttackDirection * 480000.0f + AttackRight * 120000.0f + FVector::UpVector * 52000.0f;
    BellEntryPoint = Target - AttackDirection * 300000.0f + AttackRight * 65000.0f + FVector::UpVector * 40000.0f;
    BellApproachPoint = Target - AttackDirection * 160000.0f + AttackRight * 30000.0f + FVector::UpVector * 24000.0f;
    BellReleasePoint = Target - AttackDirection * 72000.0f + FVector::UpVector * 12500.0f;
    BellEgressPoint = Target + AttackDirection * 480000.0f - AttackRight * 190000.0f + FVector::UpVector * 65000.0f;
    SetActorLocation(BellSpawnPoint);
    SetState(ERotorlineKiowaStrikeState::SearchingForTarget);
}

void ARotorlineKiowaStrikeMissionActor::NotifyTargetLockPressed()
{
    if (State == ERotorlineKiowaStrikeState::SearchingForTarget ||
        State == ERotorlineKiowaStrikeState::IdentifyingTarget ||
        State == ERotorlineKiowaStrikeState::DesignatingTarget)
    {
        bLockRequested = true;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_KIOWA_STRIKE|LOCK_REQUEST|target=%s|solution=%d|priority=MISSION_SENSOR_CONTACT"),
            PriorityTarget.IsValid() ? *PriorityTarget->GetTargetLabel() : TEXT("NONE"),
            HasValidSensorSolution() ? 1 : 0);
    }
}

void ARotorlineKiowaStrikeMissionActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    StateElapsed += DeltaSeconds;

    if (!PlayerHelicopter.IsValid())
    {
        SetState(ERotorlineKiowaStrikeState::Failed);
        return;
    }

    const bool bEngineReady = PlayerHelicopter->IsEngineReadyForMission();
    const bool bStartupCueReady = bAlliedStrikeSequence ? bEngineReady : true;
    if (!bStartupPlayed && StartupAudio && bStartupCueReady)
    {
        bStartupPlayed = true;
        if (!bAlliedStrikeSequence)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_KIOWA_RECON|STARTUP_AUDIO_TRIGGER|phase=ENGINE_STARTUP|engine_ready=%d"),
                bEngineReady ? 1 : 0);
        }
        PlaySequenceCue(StartupAudio,
            bAlliedStrikeSequence ? TEXT("STARTUP") : TEXT("M6_STARTUP"));
    }

    UpdateSensor(DeltaSeconds);
    UpdateBellFlight(DeltaSeconds);

    switch (State)
    {
    case ERotorlineKiowaStrikeState::TargetLockAudio:
        if (StateElapsed >= CueDurationOrFallback(TargetLockedAudio, 1.0f))
        {
            SetState(ERotorlineKiowaStrikeState::BellAcceptsMission);
            PlaySequenceCue(BellAcceptsMissionAudio, TEXT("BELL_ACCEPTS_MISSION"));
        }
        break;
    case ERotorlineKiowaStrikeState::BellAcceptsMission:
        if (StateElapsed >= CueDurationOrFallback(BellAcceptsMissionAudio, 1.0f) + 0.35f)
        {
            SetState(ERotorlineKiowaStrikeState::BellAcknowledgesTarget);
            PlaySequenceCue(BellAcknowledgesTargetAudio, TEXT("BELL_ACKNOWLEDGES_TARGET"));
        }
        break;
    case ERotorlineKiowaStrikeState::BellAcknowledgesTarget:
        if (StateElapsed >= CueDurationOrFallback(BellAcknowledgesTargetAudio, 0.8f) + 0.25f)
        {
            ActivateBell222();
            SetState(ERotorlineKiowaStrikeState::BellArrival);
        }
        break;
    case ERotorlineKiowaStrikeState::BellArrival:
        if (bBellFinalApproachStarted &&
            (FVector::Dist(GetActorLocation(), BellReleasePoint) <= 500.0f ||
             StateElapsed >= RotorlineKiowaStrike::BellFinalApproachTimeoutSeconds))
        {
            DialogueAudio->Stop();
            BellEngineAudio->FadeOut(0.18f, 0.0f);
            SetState(ERotorlineKiowaStrikeState::BellAttackRun);
            PlaySequenceCue(BellFiresMissilesAudio, TEXT("BELL_FIRES_MISSILES"));
        }
        break;
    case ERotorlineKiowaStrikeState::BellAttackRun:
        if (StrikeMissilesFired == 0 && StateElapsed >= 0.18f) FireStrikeMissile();
        if (StrikeMissilesFired == 1 && StateElapsed >= 0.62f) FireStrikeMissile();
        if (StrikeMissilesFired >= 2 && StateElapsed >= CueDurationOrFallback(BellFiresMissilesAudio, 1.0f))
        {
            SetState(ERotorlineKiowaStrikeState::WeaponsReleased);
        }
        break;
    case ERotorlineKiowaStrikeState::WeaponsReleased:
        if (PriorityTarget.IsValid() && PriorityTarget->IsDestroyedTarget())
        {
            bTargetExplosionObserved = true;
            SetState(ERotorlineKiowaStrikeState::TargetExplosion);
            PlaySequenceCue(TargetExplosionAudio, TEXT("TARGET_EXPLOSION"), true);
        }
        else if (StateElapsed > 12.0f && PriorityTarget.IsValid())
        {
            float AppliedDamage = 0.0f;
            PriorityTarget->ApplyCombatDamage(1000.0f, TEXT("ALLIED_BELL222_FAILSAFE"), AppliedDamage);
            UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_KIOWA_STRIKE|CONTROLLED_RETRY|damage=%.1f"), AppliedDamage);
        }
        break;
    case ERotorlineKiowaStrikeState::TargetExplosion:
        if (StateElapsed >= CueDurationOrFallback(TargetExplosionAudio, 1.5f))
        {
            SetState(ERotorlineKiowaStrikeState::BellConfirmsKill);
            PlaySequenceCue(BellConfirmsTargetDestroyedAudio, TEXT("BELL_CONFIRMS_TARGET_DESTROYED"));
        }
        break;
    case ERotorlineKiowaStrikeState::BellConfirmsKill:
        if (StateElapsed >= CueDurationOrFallback(BellConfirmsTargetDestroyedAudio, 0.8f) + 0.2f)
        {
            SetState(ERotorlineKiowaStrikeState::BellGoodbye);
            PlaySequenceCue(BellSaysGoodbyeAudio, TEXT("BELL_GOODBYE"));
        }
        break;
    case ERotorlineKiowaStrikeState::BellGoodbye:
        if (StateElapsed >= CueDurationOrFallback(BellSaysGoodbyeAudio, 1.0f))
        {
            SetState(ERotorlineKiowaStrikeState::BellAfterburnerEgress);
            PlaySequenceCue(BellAfterburnerAudio, TEXT("BELL_AFTERBURNER_EGRESS"));
        }
        break;
    case ERotorlineKiowaStrikeState::BellAfterburnerEgress:
        if (StateElapsed >= CueDurationOrFallback(BellAfterburnerAudio, 3.0f))
        {
            SetState(ERotorlineKiowaStrikeState::MissionAccomplished);
            PlaySequenceCue(MissionAccomplishedAudio, TEXT("MISSION_ACCOMPLISHED"));
        }
        break;
    case ERotorlineKiowaStrikeState::MissionAccomplished:
        if (StateElapsed >= CueDurationOrFallback(MissionAccomplishedAudio, 2.5f) &&
            (!PriorityTarget.IsValid() || FVector::Dist2D(GetActorLocation(), PriorityTarget->GetActorLocation()) > 300000.0f))
        {
            SetState(ERotorlineKiowaStrikeState::Complete);
            BellEngineAudio->FadeOut(1.5f, 0.0f);
        }
        break;
    default:
        break;
    }
}

void ARotorlineKiowaStrikeMissionActor::UpdateSensor(float DeltaSeconds)
{
    if (!IsSensorMissionActive() || !PriorityTarget.IsValid()) return;
    const bool bReconQualification = !bAlliedStrikeSequence &&
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineKiowaReconTest"));
    if (bReconQualification)
    {
        // FinishSpawning reasserts the normal airfield deployment after the
        // mission actor is configured. Hold the test aircraft at the contact
        // during the sensor tick so qualification measures the real cone/LOS
        // solution after that production deployment step has completed.
        const FVector TargetAim = PriorityTarget->GetAimLocation();
        // Approach from a high oblique mast-sensor orbit. Several valid island
        // sites sit beside buildings or steep relief, so a low west-only test
        // position could put the qualification aircraft inside that scenery
        // even though the surveyed truck itself was correctly grounded.
        const FVector QualificationLocation = TargetAim + FVector(-5000.0f, 0.0f, 7000.0f);
        PlayerHelicopter->SetActorLocation(
            QualificationLocation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        const FVector SensorOrigin = QualificationLocation + FVector::UpVector * 180.0f;
        const FRotator SensorRotation = (TargetAim - SensorOrigin).Rotation();
        // Keep the nose outside the production 15-degree acquisition cone.
        // The qualification therefore passes only when the mast sensor honors
        // the operator's view/reticle instead of aircraft or enemy-target aim.
        PlayerHelicopter->SetActorRotation(SensorRotation + FRotator(0.0f, 35.0f, 0.0f));
        if (APlayerController* PlayerController = Cast<APlayerController>(PlayerHelicopter->GetController()))
        {
            PlayerController->SetControlRotation(SensorRotation);
        }
        bLockRequested = true;
        if (StateElapsed <= DeltaSeconds * 1.5f)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_KIOWA_RECON_TEST|SENSOR_POSITIONED|target=%s|range_m=%.1f|nose_offset_deg=35|aim_source=PLAYER_VIEW"),
                *PriorityTarget->GetTargetLabel(),
                FVector::Dist(QualificationLocation, TargetAim) / 100.0f);
        }
    }
    const bool bValidSolution = HasValidSensorSolution();
    if (!bLockRequested)
    {
        IdentificationProgress = 0.0f;
        DesignationProgress = 0.0f;
        return;
    }
    if (!bValidSolution)
    {
        LockLossElapsed += DeltaSeconds;
        if (LockLossElapsed > RotorlineKiowaStrike::LockLossGraceSeconds)
        {
            IdentificationProgress = 0.0f;
            DesignationProgress = 0.0f;
            SetState(ERotorlineKiowaStrikeState::SearchingForTarget);
        }
        return;
    }
    LockLossElapsed = 0.0f;
    if (IdentificationProgress < 1.0f)
    {
        if (State != ERotorlineKiowaStrikeState::IdentifyingTarget)
            SetState(ERotorlineKiowaStrikeState::IdentifyingTarget);
        IdentificationProgress = FMath::Min(1.0f,
            IdentificationProgress + DeltaSeconds / RotorlineKiowaStrike::IdentificationSeconds);
        if (IdentificationProgress >= 1.0f)
        {
            PriorityTarget->SetMissionMarkerVisibility(true);
            SetState(ERotorlineKiowaStrikeState::DesignatingTarget);
        }
        return;
    }
    if (State != ERotorlineKiowaStrikeState::DesignatingTarget)
        SetState(ERotorlineKiowaStrikeState::DesignatingTarget);
    DesignationProgress = FMath::Min(1.0f,
        DesignationProgress + DeltaSeconds / RotorlineKiowaStrike::DesignationSeconds);
    if (DesignationProgress >= 1.0f)
    {
        if (bAlliedStrikeSequence)
        {
            SetState(ERotorlineKiowaStrikeState::TargetLockAudio);
            PlaySequenceCue(TargetLockedAudio, TEXT("TARGET_LOCKED"));
        }
        else
        {
            SetState(ERotorlineKiowaStrikeState::Complete);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_KIOWA_RECON|CONTACT_RECORDED|target=%s|hold_seconds=%.1f"),
                *PriorityTarget->GetTargetLabel(),
                RotorlineKiowaStrike::IdentificationSeconds + RotorlineKiowaStrike::DesignationSeconds);
        }
    }
}

bool ARotorlineKiowaStrikeMissionActor::HasValidSensorSolution() const
{
    if (!PlayerHelicopter.IsValid() || !PriorityTarget.IsValid() || PriorityTarget->IsDestroyedTarget()) return false;
    const FVector Origin = PlayerHelicopter->GetActorLocation() + FVector::UpVector * 180.0f;
    const FVector ToTarget = PriorityTarget->GetAimLocation() - Origin;
    if (ToTarget.SizeSquared() > FMath::Square(RotorlineKiowaStrike::RadarRangeCm)) return false;
    FVector SensorDirection = PlayerHelicopter->GetActorForwardVector();
    if (const APlayerController* PlayerController = Cast<APlayerController>(PlayerHelicopter->GetController()))
    {
        FVector ViewLocation;
        FRotator ViewRotation;
        PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
        SensorDirection = ViewRotation.Vector();
    }
    if (FVector::DotProduct(SensorDirection.GetSafeNormal(), ToTarget.GetSafeNormal()) < RotorlineKiowaStrike::RadarConeDot) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RotorlineKiowaDesignationLOS), true);
    Params.AddIgnoredActor(PlayerHelicopter.Get());
    Params.AddIgnoredActor(PriorityTarget.Get());
    return !GetWorld()->LineTraceSingleByChannel(Hit, Origin, PriorityTarget->GetAimLocation(), ECC_Visibility, Params);
}

void ARotorlineKiowaStrikeMissionActor::ActivateBell222()
{
    if (bBellActivated) return;
    bBellActivated = true;
    SetActorLocation(BellSpawnPoint);
    BellBody->SetVisibility(true, true);
    BellMainRotor->SetVisibility(true, true);
    BellTailRotor->SetVisibility(true, true);
    AlliedLabel->SetVisibility(true, true);
    BellEngineAudio->SetVolumeMultiplier(PlayerHelicopter.IsValid() ? PlayerHelicopter->GetMissionEngineVolume() * 0.62f : 0.62f);
    BellEngineAudio->Stop();
    bBellFinalApproachStarted = false;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_KIOWA_STRIKE|BELL222|state=ACTIVATED|faction=ALLIED_STRIKE"));
}

void ARotorlineKiowaStrikeMissionActor::UpdateBellFlight(float DeltaSeconds)
{
    if (!bBellActivated) return;
    const float MainRotorDelta = DeltaSeconds * 1980.0f;
    const float TailRotorDelta = DeltaSeconds * 5940.0f;
    BellRotorDegrees = FMath::Fmod(BellRotorDegrees + MainRotorDelta, 360.0f);
    BellMainRotorPivot->AddLocalRotation(FRotator(0.0f, MainRotorDelta, 0.0f));
    BellTailRotorPivot->AddLocalRotation(FRotator(TailRotorDelta, 0.0f, 0.0f));

    if (State == ERotorlineKiowaStrikeState::BellArrival)
    {
        if (StateElapsed < RotorlineKiowaStrike::BellEntryPhaseSeconds)
        {
            MoveBellToward(BellEntryPoint, RotorlineKiowaStrike::ApproachSpeedCmPerSecond, DeltaSeconds);
        }
        else if (StateElapsed < RotorlineKiowaStrike::BellApproachPhaseSeconds)
        {
            MoveBellToward(BellApproachPoint, RotorlineKiowaStrike::ApproachSpeedCmPerSecond, DeltaSeconds);
        }
        else if (StateElapsed >= RotorlineKiowaStrike::BellFinalApproachStartSeconds)
        {
            if (!bBellFinalApproachStarted)
            {
                bBellFinalApproachStarted = true;
                BellCurrentSpeed = 0.0f;
                PlaySequenceCue(BellArrivalAudio, TEXT("BELL_ARRIVAL"));
                BellEngineAudio->Play();
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_KIOWA_STRIKE|BELL222|state=FINAL_APPROACH|flight_audio=STARTED|target=FIRING_POSITION"));
            }
            MoveBellToward(BellReleasePoint, RotorlineKiowaStrike::AttackSpeedCmPerSecond, DeltaSeconds);
        }
    }
    else if (State == ERotorlineKiowaStrikeState::BellAttackRun || State == ERotorlineKiowaStrikeState::WeaponsReleased)
    {
        MoveBellToward(BellReleasePoint, RotorlineKiowaStrike::AttackSpeedCmPerSecond, DeltaSeconds);
    }
    else if (State >= ERotorlineKiowaStrikeState::BellAfterburnerEgress &&
        State < ERotorlineKiowaStrikeState::Complete)
    {
        MoveBellToward(BellEgressPoint, RotorlineKiowaStrike::EgressSpeedCmPerSecond, DeltaSeconds);
    }
}

void ARotorlineKiowaStrikeMissionActor::MoveBellToward(const FVector& Destination, float DesiredSpeed, float DeltaSeconds)
{
    const FVector ToDestination = Destination - GetActorLocation();
    if (ToDestination.SizeSquared() < FMath::Square(150.0f)) return;
    BellCurrentSpeed = FMath::FInterpConstantTo(BellCurrentSpeed, DesiredSpeed, DeltaSeconds,
        RotorlineKiowaStrike::BellAccelerationCmPerSecondSquared);
    const FVector Direction = ToDestination.GetSafeNormal();
    SetActorLocation(GetActorLocation() + Direction * FMath::Min(BellCurrentSpeed * DeltaSeconds, ToDestination.Size()), false);
    const FRotator DesiredRotation = Direction.Rotation();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(DesiredRotation.Pitch * 0.20f, DesiredRotation.Yaw, 0.0f), DeltaSeconds, 1.8f));
}

void ARotorlineKiowaStrikeMissionActor::FireStrikeMissile()
{
    if (!PriorityTarget.IsValid() || PriorityTarget->IsDestroyedTarget()) return;
    const float Side = StrikeMissilesFired == 0 ? -1.0f : 1.0f;
    const FVector Start = GetActorLocation() + GetActorForwardVector() * 270.0f + GetActorRightVector() * 135.0f * Side - FVector::UpVector * 55.0f;
    const FVector Direction = (PriorityTarget->GetAimLocation() - Start).GetSafeNormal();
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    ARotorlineRocketProjectile* Missile = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
        ARotorlineRocketProjectile::StaticClass(), Start, Direction.Rotation(), SpawnParams);
    if (!Missile) return;
    Missile->LaunchPlayerWeapon(Start, Direction, PriorityTarget.Get(), TEXT("ALLIED_BELL222_HELLFIRE"),
        165.0f, 35.0f, 900.0f, 34500.0f, FString());
    ++StrikeMissilesFired;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_KIOWA_STRIKE|WEAPON_RELEASE|missile=%d|target=%s"),
        StrikeMissilesFired, *PriorityTarget->GetTargetLabel());
}

void ARotorlineKiowaStrikeMissionActor::PlaySequenceCue(USoundBase* Sound, const TCHAR* CueName, bool bSpatialAtTarget)
{
    if (!Sound)
    {
        UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_KIOWA_STRIKE|AUDIO_MISSING|cue=%s|fallback=1"), CueName);
        return;
    }
    const float RadioMix = PlayerHelicopter.IsValid() ? PlayerHelicopter->GetMissionRadioVolume() : 1.0f;
    const bool bBellArrival = FCString::Stricmp(CueName, TEXT("BELL_ARRIVAL")) == 0;
    const float Volume = RadioMix * (bBellArrival ? 0.34f : 1.0f);
    if (bSpatialAtTarget && PriorityTarget.IsValid())
    {
        if (SpatialExplosionAudio) SpatialExplosionAudio->Stop();
        SpatialExplosionAudio = UGameplayStatics::SpawnSoundAtLocation(this, Sound, PriorityTarget->GetAimLocation(), FRotator::ZeroRotator, Volume);
    }
    else
    {
        DialogueAudio->Stop();
        DialogueAudio->SetSound(Sound);
        DialogueAudio->SetVolumeMultiplier(Volume);
        DialogueAudio->Play();
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_KIOWA_STRIKE|AUDIO|cue=%s|duration=%.2f|volume_scale=%.2f|one_shot=1"),
        CueName, Sound->GetDuration(), bBellArrival ? 0.34f : 1.0f);
}

void ARotorlineKiowaStrikeMissionActor::LoadMissionAudio()
{
    StartupAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/startupAudio.startupAudio"));
    TargetLockedAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/TargetlockedAudio.TargetlockedAudio"));
    BellAcceptsMissionAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/Bell222AcceptsMission.Bell222AcceptsMission"));
    BellAcknowledgesTargetAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/Bell222AcknoledgesTarget.Bell222AcknoledgesTarget"));
    BellArrivalAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/Bell222arrivalAudio.Bell222arrivalAudio"));
    BellFiresMissilesAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/Bell222firesmissiles.Bell222firesmissiles"));
    TargetExplosionAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/TargetexplosionAudio.TargetexplosionAudio"));
    BellConfirmsTargetDestroyedAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/Bell222ConfirmsTargetDestroyed.Bell222ConfirmsTargetDestroyed"));
    BellSaysGoodbyeAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/Bell222SaysGoodby.Bell222SaysGoodby"));
    BellAfterburnerAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/bell-222-afterburner-scream.bell-222-afterburner-scream"));
    MissionAccomplishedAudio = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Missions/KiowaReconStrike/MissionAccomplished.MissionAccomplished"));
    const int32 LoadedCueCount =
        (StartupAudio ? 1 : 0) +
        (TargetLockedAudio ? 1 : 0) +
        (BellAcceptsMissionAudio ? 1 : 0) +
        (BellAcknowledgesTargetAudio ? 1 : 0) +
        (BellArrivalAudio ? 1 : 0) +
        (BellFiresMissilesAudio ? 1 : 0) +
        (TargetExplosionAudio ? 1 : 0) +
        (BellConfirmsTargetDestroyedAudio ? 1 : 0) +
        (BellSaysGoodbyeAudio ? 1 : 0) +
        (BellAfterburnerAudio ? 1 : 0) +
        (MissionAccomplishedAudio ? 1 : 0);
    if (LoadedCueCount == 11)
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_KIOWA_STRIKE|AUDIO_ASSET_CHECK|loaded=%d|expected=11|result=PASS"),
            LoadedCueCount);
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_KIOWA_STRIKE|AUDIO_ASSET_CHECK|loaded=%d|expected=11|result=FAIL"),
            LoadedCueCount);
    }
}

void ARotorlineKiowaStrikeMissionActor::SetState(ERotorlineKiowaStrikeState NewState)
{
    if (State == NewState || State == ERotorlineKiowaStrikeState::Complete || State == ERotorlineKiowaStrikeState::Failed) return;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_KIOWA_STRIKE|STATE|from=%s|to=%s"), StateName(State), StateName(NewState));
    State = NewState;
    StateElapsed = 0.0f;
}

float ARotorlineKiowaStrikeMissionActor::CueDurationOrFallback(const USoundBase* Sound, float Fallback) const
{
    return Sound ? FMath::Max(0.1f, Sound->GetDuration()) : Fallback;
}

bool ARotorlineKiowaStrikeMissionActor::IsTargetRevealed() const
{
    return IdentificationProgress >= 1.0f || State >= ERotorlineKiowaStrikeState::TargetLockAudio;
}

bool ARotorlineKiowaStrikeMissionActor::IsSensorMissionActive() const
{
    return State == ERotorlineKiowaStrikeState::SearchingForTarget ||
        State == ERotorlineKiowaStrikeState::IdentifyingTarget ||
        State == ERotorlineKiowaStrikeState::DesignatingTarget;
}

float ARotorlineKiowaStrikeMissionActor::GetDesignationProgress() const
{
    return IdentificationProgress < 1.0f ? IdentificationProgress * 0.45f : 0.45f + DesignationProgress * 0.55f;
}

FString ARotorlineKiowaStrikeMissionActor::GetSensorStatus() const
{
    switch (State)
    {
    case ERotorlineKiowaStrikeState::SearchingForTarget: return bLockRequested ? TEXT("SEARCHING // NO VALID CONTACT") : TEXT("RADAR READY // PRESS B1, X, OR T");
    case ERotorlineKiowaStrikeState::IdentifyingTarget: return TEXT("SCANNING // UNKNOWN CONTACT");
    case ERotorlineKiowaStrikeState::DesignatingTarget: return TEXT("PRIORITY TARGET // DESIGNATING");
    default: return IsComplete() ? TEXT("DESIGNATION COMPLETE") : TEXT("RADAR STANDBY");
    }
}

FString ARotorlineKiowaStrikeMissionActor::GetAlliedStrikeStatus() const
{
    if (!bAlliedStrikeSequence)
    {
        return IsComplete() ? TEXT("RECON RECORD // COMPLETE") : TEXT("RECON RECORD // AWAITING SENSOR LOCK");
    }
    if (State < ERotorlineKiowaStrikeState::TargetLockAudio) return TEXT("ALLIED STRIKE // STANDING BY");
    if (State < ERotorlineKiowaStrikeState::BellArrival) return TEXT("ALLIED STRIKE // TASKING");
    if (State < ERotorlineKiowaStrikeState::BellAttackRun) return TEXT("ALLY BELL 222 // INBOUND");
    if (State < ERotorlineKiowaStrikeState::TargetExplosion) return TEXT("ALLY BELL 222 // ATTACKING");
    if (State < ERotorlineKiowaStrikeState::Complete) return TEXT("ALLY BELL 222 // EGRESSING");
    return TEXT("ALLIED STRIKE // COMPLETE");
}

const TCHAR* ARotorlineKiowaStrikeMissionActor::StateName(ERotorlineKiowaStrikeState Value)
{
    switch (Value)
    {
    case ERotorlineKiowaStrikeState::Initializing: return TEXT("INITIALIZING");
    case ERotorlineKiowaStrikeState::TravelToReconZone: return TEXT("TRAVEL_TO_RECON_ZONE");
    case ERotorlineKiowaStrikeState::SearchingForTarget: return TEXT("SEARCHING_FOR_TARGET");
    case ERotorlineKiowaStrikeState::IdentifyingTarget: return TEXT("IDENTIFYING_TARGET");
    case ERotorlineKiowaStrikeState::DesignatingTarget: return TEXT("DESIGNATING_TARGET");
    case ERotorlineKiowaStrikeState::TargetLockAudio: return TEXT("TARGET_LOCK_AUDIO");
    case ERotorlineKiowaStrikeState::BellAcceptsMission: return TEXT("BELL_ACCEPTS_MISSION");
    case ERotorlineKiowaStrikeState::BellAcknowledgesTarget: return TEXT("BELL_ACKNOWLEDGES_TARGET");
    case ERotorlineKiowaStrikeState::BellArrival: return TEXT("BELL_ARRIVAL");
    case ERotorlineKiowaStrikeState::BellAttackRun: return TEXT("BELL_ATTACK_RUN");
    case ERotorlineKiowaStrikeState::WeaponsReleased: return TEXT("WEAPONS_RELEASED");
    case ERotorlineKiowaStrikeState::TargetExplosion: return TEXT("TARGET_EXPLOSION");
    case ERotorlineKiowaStrikeState::BellConfirmsKill: return TEXT("BELL_CONFIRMS_KILL");
    case ERotorlineKiowaStrikeState::BellGoodbye: return TEXT("BELL_GOODBYE");
    case ERotorlineKiowaStrikeState::BellAfterburnerEgress: return TEXT("BELL_AFTERBURNER_EGRESS");
    case ERotorlineKiowaStrikeState::MissionAccomplished: return TEXT("MISSION_ACCOMPLISHED");
    case ERotorlineKiowaStrikeState::Complete: return TEXT("COMPLETE");
    case ERotorlineKiowaStrikeState::Failed: return TEXT("FAILED");
    default: return TEXT("UNKNOWN");
    }
}

void ARotorlineKiowaStrikeMissionActor::StopAllAudio()
{
    if (DialogueAudio) DialogueAudio->Stop();
    if (BellEngineAudio) BellEngineAudio->Stop();
    if (SpatialExplosionAudio) SpatialExplosionAudio->Stop();
}

void ARotorlineKiowaStrikeMissionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopAllAudio();
    Super::EndPlay(EndPlayReason);
}

bool ARotorlineKiowaStrikeMissionActor::IsDialogueAudioPlaying() const
{
    return DialogueAudio && DialogueAudio->IsPlaying();
}
