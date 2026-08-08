#include "RotorlineFinalCinematicActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RotorlineOperationsPlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

ARotorlineFinalCinematicActor::ARotorlineFinalCinematicActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    CinematicCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CarrierCinematicCamera"));
    CinematicCamera->SetupAttachment(SceneRoot);
    CinematicCamera->SetFieldOfView(58.0f);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Hot(TEXT("/Game/Missions/Presentation/M_ExplosionHot.M_ExplosionHot"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Smoke(TEXT("/Game/Missions/Presentation/M_RocketSmoke.M_RocketSmoke"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> GasBombSystem(
        TEXT("/Game/Missions/Presentation/M25/NS_M25_MOAB_GasBomb.NS_M25_MOAB_GasBomb"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> CoreExplosionSystem(
        TEXT("/Game/Missions/Presentation/M25/NS_M25_MOAB_Core.NS_M25_MOAB_Core"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DustShockwaveSystem(
        TEXT("/Game/Missions/Presentation/M25/NS_M25_MOAB_Dust.NS_M25_MOAB_Dust"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SparksSystem(
        TEXT("/Game/Missions/Presentation/M25/NS_M25_MOAB_Sparks.NS_M25_MOAB_Sparks"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PersistentSmokeSystem(
        TEXT("/Game/Missions/Presentation/M25/NS_M25_MOAB_Smoke.NS_M25_MOAB_Smoke"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> VolumetricExplosionSystem(
        TEXT("/Game/Missions/Presentation/M25/NS_M25_Grid3D_Gas_Explosion.NS_M25_Grid3D_Gas_Explosion"));
    Missile = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeavyInboundMissile"));
    Missile->SetupAttachment(SceneRoot); Missile->SetStaticMesh(Cylinder.Object);
    Missile->SetRelativeScale3D(FVector(6.0f, 6.0f, 30.0f)); Missile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (Hot.Succeeded()) Missile->SetMaterial(0, Hot.Object);
    Fireball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IslandFireball"));
    Fireball->SetupAttachment(SceneRoot); Fireball->SetStaticMesh(Sphere.Object); Fireball->SetVisibility(false);
    Fireball->SetCollisionEnabled(ECollisionEnabled::NoCollision); Fireball->SetCastShadow(false);
    if (Hot.Succeeded()) Fireball->SetMaterial(0, Hot.Object);
    Shockwave = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IslandShockwave"));
    Shockwave->SetupAttachment(SceneRoot); Shockwave->SetStaticMesh(Cylinder.Object); Shockwave->SetVisibility(false);
    Shockwave->SetCollisionEnabled(ECollisionEnabled::NoCollision); Shockwave->SetCastShadow(false);
    if (Hot.Succeeded()) Shockwave->SetMaterial(0, Hot.Object);
    SmokeColumn = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IslandSmokeColumn"));
    SmokeColumn->SetupAttachment(SceneRoot); SmokeColumn->SetStaticMesh(Sphere.Object); SmokeColumn->SetVisibility(false);
    SmokeColumn->SetCollisionEnabled(ECollisionEnabled::NoCollision); SmokeColumn->SetCastShadow(false);
    if (Smoke.Succeeded()) SmokeColumn->SetMaterial(0, Smoke.Object);
    HeroGasBombFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("M25HeroGasBombFX"));
    HeroGasBombFX->SetupAttachment(SceneRoot); HeroGasBombFX->SetAutoActivate(false); HeroGasBombFX->SetBoundsScale(100.0f);
    HeroGasBombFX->SetAllowScalability(false); HeroGasBombFX->SetForceSolo(true);
    HeroGasBombFX->SetRelativeScale3D(FVector(220.0f)); if (GasBombSystem.Succeeded()) HeroGasBombFX->SetAsset(GasBombSystem.Object);
    CoreExplosionFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("M25CoreExplosionFX"));
    CoreExplosionFX->SetupAttachment(SceneRoot); CoreExplosionFX->SetAutoActivate(false); CoreExplosionFX->SetBoundsScale(100.0f);
    CoreExplosionFX->SetAllowScalability(false); CoreExplosionFX->SetForceSolo(true);
    CoreExplosionFX->SetRelativeScale3D(FVector(180.0f)); if (CoreExplosionSystem.Succeeded()) CoreExplosionFX->SetAsset(CoreExplosionSystem.Object);
    DustShockwaveFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("M25DustShockwaveFX"));
    DustShockwaveFX->SetupAttachment(SceneRoot); DustShockwaveFX->SetAutoActivate(false); DustShockwaveFX->SetBoundsScale(100.0f);
    DustShockwaveFX->SetAllowScalability(false); DustShockwaveFX->SetForceSolo(true);
    DustShockwaveFX->SetRelativeScale3D(FVector(340.0f, 340.0f, 90.0f)); if (DustShockwaveSystem.Succeeded()) DustShockwaveFX->SetAsset(DustShockwaveSystem.Object);
    SparksFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("M25SparksFX"));
    SparksFX->SetupAttachment(SceneRoot); SparksFX->SetAutoActivate(false); SparksFX->SetBoundsScale(100.0f);
    SparksFX->SetAllowScalability(false); SparksFX->SetForceSolo(true);
    SparksFX->SetRelativeScale3D(FVector(190.0f)); if (SparksSystem.Succeeded()) SparksFX->SetAsset(SparksSystem.Object);
    PersistentSmokeFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("M25PersistentSmokeFX"));
    PersistentSmokeFX->SetupAttachment(SceneRoot); PersistentSmokeFX->SetAutoActivate(false); PersistentSmokeFX->SetBoundsScale(100.0f);
    PersistentSmokeFX->SetAllowScalability(false); PersistentSmokeFX->SetForceSolo(true);
    PersistentSmokeFX->SetRelativeScale3D(FVector(260.0f, 260.0f, 360.0f)); if (PersistentSmokeSystem.Succeeded()) PersistentSmokeFX->SetAsset(PersistentSmokeSystem.Object);
    VolumetricExplosionFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("M25VolumetricExplosionFX"));
    VolumetricExplosionFX->SetupAttachment(SceneRoot); VolumetricExplosionFX->SetAutoActivate(false); VolumetricExplosionFX->SetBoundsScale(200.0f);
    VolumetricExplosionFX->SetAllowScalability(false); VolumetricExplosionFX->SetForceSolo(true);
    VolumetricExplosionFX->SetRelativeScale3D(FVector::OneVector); if (VolumetricExplosionSystem.Succeeded()) VolumetricExplosionFX->SetAsset(VolumetricExplosionSystem.Object);
    BlastLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IslandBlastLight"));
    BlastLight->SetupAttachment(SceneRoot); BlastLight->SetVisibility(false); BlastLight->SetIntensity(8000000.0f);
    BlastLight->SetAttenuationRadius(1400000.0f); BlastLight->SetLightColor(FLinearColor(1.0f, 0.28f, 0.04f));
}

void ARotorlineFinalCinematicActor::StartFinale()
{
    if (bStarted) return;
    bStarted = true;
    if (IConsoleVariable* NiagaraQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("fx.Niagara.QualityLevel")))
    {
        PreviousNiagaraQualityLevel = NiagaraQuality->GetInt();
        NiagaraQuality->Set(3, ECVF_SetByCode);
        bNiagaraQualityOverridden = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_M25_FINALE|state=NIAGARA_HERO_QUALITY|previous=%d|effective=%d|scalability=DISABLED"),
            PreviousNiagaraQualityLevel,
            NiagaraQuality->GetInt());
    }
    ImpactLocation = FVector(0.0f, 0.0f, 6500.0f);
    FHitResult ImpactHit;
    FCollisionQueryParams ImpactQuery(SCENE_QUERY_STAT(RotorlineM25FinaleImpact), false, this);
    const FVector ImpactTraceStart(0.0f, 0.0f, 300000.0f);
    const FVector ImpactTraceEnd(0.0f, 0.0f, -100000.0f);
    if (GetWorld()->LineTraceSingleByChannel(
        ImpactHit,
        ImpactTraceStart,
        ImpactTraceEnd,
        ECC_Visibility,
        ImpactQuery))
    {
        ImpactLocation = ImpactHit.ImpactPoint + FVector(0.0f, 0.0f, 150.0f);
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_M25_FINALE|state=IMPACT_GROUNDED|location=%s|surface=%s"),
        *ImpactLocation.ToCompactString(),
        ImpactHit.GetActor() ? *ImpactHit.GetActor()->GetName() : TEXT("FALLBACK"));
    MissileStart = ImpactLocation + FVector(180000.0f, 120000.0f, 155000.0f);
    Missile->SetWorldLocation(MissileStart);
    Missile->SetWorldRotation((ImpactLocation - MissileStart).Rotation() + FRotator(0.0f, 0.0f, 90.0f));
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        PreviousViewTarget = PC->GetViewTarget();
        // The finale is always witnessed from Pacific Dawn. Quick-deploy
        // previews begin at the airfield, so deriving this from the pawn would
        // validate the wrong view and repeat the original off-camera failure.
        const FVector ViewOrigin(530000.0f, 405000.0f, 3000.0f);
        FVector AwayFromIsland = ViewOrigin - ImpactLocation;
        AwayFromIsland.Z = 0.0f;
        if (!AwayFromIsland.Normalize())
        {
            AwayFromIsland = FVector::ForwardVector;
        }
        const FVector CameraLocation =
            ViewOrigin + AwayFromIsland * 3500.0f + FVector(0.0f, 0.0f, 2600.0f);
        const FVector CameraAimPoint = ImpactLocation + FVector(0.0f, 0.0f, 12000.0f);
        CinematicCamera->SetWorldLocation(CameraLocation);
        CinematicCamera->SetWorldRotation((CameraAimPoint - CameraLocation).Rotation());
        CameraBaseLocation = CinematicCamera->GetComponentLocation();
        CameraBaseRotation = CinematicCamera->GetComponentRotation();
        CinematicCamera->Activate(true);
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        PC->SetViewTargetWithBlend(this, 0.8f);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_M25_FINALE|state=CAMERA_LOCKED|location=%s|aim=%s"),
            *CameraLocation.ToCompactString(),
            *CameraAimPoint.ToCompactString());
    }
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            7250,
            7.0f,
            FColor(255, 120, 50),
            TEXT("FINAL STRIKE INBOUND  //  HOLD POSITION"));
    }

    ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(GetWorld()->GetFirstPlayerController());
    bEndingMovieStarted = OperationsController && OperationsController->BeginM25FinalCreditsSequence();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_M25_FINALE|state=FINAL_VIDEO_HANDOFF|movie=Movies/M25_FinalEnd.mp4|started=%d|credits=HUD_DELAYED|skippable=0|deliveries=1"),
        bEndingMovieStarted ? 1 : 0);
    if (!bEndingMovieStarted)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("ROTORLINE_M25_FINALE|state=FINAL_VIDEO_FALLBACK|reason=PLAYBACK_START_FAILED|fallback=REALTIME"));
    }
}

void ARotorlineFinalCinematicActor::CompleteFinale(const TCHAR* CompletionMode)
{
    if (bFinished) return;
    bFinished = true;
    CleanupFinaleState();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_M25_FINALE|state=COMPLETE|mode=%s"),
        CompletionMode);
}

void ARotorlineFinalCinematicActor::CleanupFinaleState()
{
    if (bCleanupComplete) return;
    bCleanupComplete = true;

    for (UNiagaraComponent* Effect :
        { HeroGasBombFX.Get(), CoreExplosionFX.Get(), DustShockwaveFX.Get(),
          SparksFX.Get(), PersistentSmokeFX.Get(), VolumetricExplosionFX.Get() })
    {
        if (Effect)
        {
            Effect->DeactivateImmediate();
        }
    }
    if (BlastLight) BlastLight->SetVisibility(false);
    if (CinematicCamera) CinematicCamera->Deactivate();

    if (bNiagaraQualityOverridden)
    {
        if (IConsoleVariable* NiagaraQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("fx.Niagara.QualityLevel")))
        {
            NiagaraQuality->Set(PreviousNiagaraQualityLevel, ECVF_SetByCode);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("ROTORLINE_M25_FINALE|state=NIAGARA_QUALITY_RESTORED|effective=%d"),
                NiagaraQuality->GetInt());
        }
        bNiagaraQualityOverridden = false;
    }
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            PC->SetIgnoreMoveInput(false);
            PC->SetIgnoreLookInput(false);
            if (PreviousViewTarget.IsValid())
            {
                PC->SetViewTargetWithBlend(PreviousViewTarget.Get(), 0.6f);
            }
        }
    }
}

void ARotorlineFinalCinematicActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CleanupFinaleState();
    Super::EndPlay(EndPlayReason);
}

void ARotorlineFinalCinematicActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bStarted || bFinished) return;
    Elapsed += DeltaSeconds;
    if (bEndingMovieStarted)
    {
        ARotorlineOperationsPlayerController* OperationsController =
            Cast<ARotorlineOperationsPlayerController>(GetWorld()->GetFirstPlayerController());
        if (OperationsController && OperationsController->HasM25FinalCreditsSequenceCompleted())
        {
            CompleteFinale(TEXT("FINAL_END_VIDEO_AND_CREDITS"));
            return;
        }
        if (OperationsController && !OperationsController->HasM25FinalCreditsSequenceFailed())
        {
            return;
        }
        if (!OperationsController && Elapsed < 3.0f)
        {
            return;
        }
        bEndingMovieStarted = false;
        Elapsed = 0.0f;
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("ROTORLINE_M25_FINALE|state=FINAL_VIDEO_FALLBACK|reason=PLAYBACK_FAILED_OR_CONTROLLER_LOST|fallback=REALTIME"));
    }

    constexpr float ImpactTime = 7.0f;
    if (!bImpacted)
    {
        const float Alpha = FMath::Clamp(Elapsed / ImpactTime, 0.0f, 1.0f);
        Missile->SetWorldLocation(FMath::Lerp(MissileStart, ImpactLocation, Alpha * Alpha));
        if (Alpha >= 1.0f)
        {
            bImpacted = true;
            Missile->SetVisibility(false);
            Fireball->SetVisibility(false);
            Shockwave->SetVisibility(false);
            SmokeColumn->SetVisibility(false);
            BlastLight->SetVisibility(true);
            HeroGasBombFX->SetWorldLocation(ImpactLocation); HeroGasBombFX->Activate(true);
            CoreExplosionFX->SetWorldLocation(ImpactLocation); CoreExplosionFX->Activate(true);
            DustShockwaveFX->SetWorldLocation(ImpactLocation + FVector(0.0f, 0.0f, 300.0f)); DustShockwaveFX->Activate(true);
            SparksFX->SetWorldLocation(ImpactLocation + FVector(0.0f, 0.0f, 1200.0f)); SparksFX->Activate(true);
            PersistentSmokeFX->SetWorldLocation(ImpactLocation + FVector(0.0f, 0.0f, 1600.0f)); PersistentSmokeFX->Activate(true);
            const FVector VolumetricWorldSize(180000.0f, 180000.0f, 260000.0f);
            const FVector VolumetricSourceOffset(0.0f, 0.0f, -90000.0f);
            constexpr int32 VolumetricResolutionMaxAxis = 96;
            constexpr float VolumetricSourceRadius = 26000.0f;
            VolumetricExplosionFX->SetVariableVec3(TEXT("User.WorldSpaceSize"), VolumetricWorldSize);
            VolumetricExplosionFX->SetVariableInt(TEXT("User.ResolutionMaxAxis"), VolumetricResolutionMaxAxis);
            VolumetricExplosionFX->SetVariableVec3(TEXT("User.SourceOffset"), VolumetricSourceOffset);
            VolumetricExplosionFX->SetVariableFloat(TEXT("Emitter.SphereRadius"), VolumetricSourceRadius);
            VolumetricExplosionFX->SetWorldLocation(ImpactLocation + FVector(0.0f, 0.0f, 95000.0f));
            VolumetricExplosionFX->Activate(true);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("ROTORLINE_M25_FINALE|state=GRID3D_OVERRIDES|world_size_cm=%.0f,%.0f,%.0f|resolution=%d|source_offset_cm=%.0f,%.0f,%.0f|source_radius_cm=%.0f"),
                VolumetricWorldSize.X,
                VolumetricWorldSize.Y,
                VolumetricWorldSize.Z,
                VolumetricResolutionMaxAxis,
                VolumetricSourceOffset.X,
                VolumetricSourceOffset.Y,
                VolumetricSourceOffset.Z,
                VolumetricSourceRadius);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("ROTORLINE_M25_FINALE|state=NIAGARA_ACTIVATED|gas=%d|core=%d|dust=%d|sparks=%d|smoke=%d"),
                HeroGasBombFX->IsActive() ? 1 : 0,
                CoreExplosionFX->IsActive() ? 1 : 0,
                DustShockwaveFX->IsActive() ? 1 : 0,
                SparksFX->IsActive() ? 1 : 0,
                PersistentSmokeFX->IsActive() ? 1 : 0);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("ROTORLINE_M25_FINALE|state=VOLUMETRIC_ACTIVATED|active=%d|primitive_fallback=DISABLED"),
                VolumetricExplosionFX->IsActive() ? 1 : 0);
            CinematicCamera->PostProcessBlendWeight = 1.0f;
            CinematicCamera->PostProcessSettings.bOverride_BloomIntensity = true;
            CinematicCamera->PostProcessSettings.BloomIntensity = 2.2f;
            CinematicCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
            CinematicCamera->PostProcessSettings.AutoExposureBias = 0.8f;
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(
                    7251,
                    8.0f,
                    FColor::White,
                    TEXT("IMPACT  //  ROTORLINE EVACUATION COMPLETE"));
            }
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_M25_FINALE|state=ISLAND_IMPACT"));
        }
        return;
    }
    const float T = Elapsed - ImpactTime;
    const float FlashFade = 1.0f - FMath::Clamp(T / 2.4f, 0.0f, 1.0f);
    BlastLight->SetIntensity(8000000.0f * FlashFade);
    CinematicCamera->PostProcessSettings.BloomIntensity = 1.0f + 1.2f * FlashFade;
    CinematicCamera->PostProcessSettings.AutoExposureBias = 0.8f * FlashFade;
    const float ShakeFade = 1.0f - FMath::Clamp(T / 6.0f, 0.0f, 1.0f);
    const FVector CameraJitter(
        FMath::PerlinNoise1D(T * 7.1f) * 65.0f,
        FMath::PerlinNoise1D(T * 8.3f + 19.0f) * 65.0f,
        FMath::PerlinNoise1D(T * 6.7f + 43.0f) * 42.0f);
    const FRotator RotationJitter(
        FMath::PerlinNoise1D(T * 5.9f + 71.0f) * 0.22f,
        FMath::PerlinNoise1D(T * 6.5f + 89.0f) * 0.22f,
        FMath::PerlinNoise1D(T * 5.3f + 101.0f) * 0.12f);
    CinematicCamera->SetWorldLocation(CameraBaseLocation + CameraJitter * ShakeFade);
    CinematicCamera->SetWorldRotation(CameraBaseRotation + RotationJitter * ShakeFade);
    if (T >= 14.0f)
    {
        CompleteFinale(TEXT("REALTIME_FALLBACK"));
    }
}
