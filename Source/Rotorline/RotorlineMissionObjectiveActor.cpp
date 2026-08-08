#include "RotorlineMissionObjectiveActor.h"
#include "RotorlineGroundingComponent.h"
#include "RotorlineGroundingLibrary.h"

#include "Animation/AnimSequence.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RotorlineEnemyProjectile.h"
#include "RotorlineHelicopterPawn.h"
#include "RotorlineOperationsPlayerController.h"
#include "RotorlineRocketProjectile.h"
#include "RotorlineRocketTrailSegment.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineMissionVisuals
{
    const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
    const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
    const TCHAR* SpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
    const TCHAR* YellowMaterialPath = TEXT("/Game/Environment/Materials/Blockout/M_Marking_Yellow.M_Marking_Yellow");
    const TCHAR* RedMaterialPath = TEXT("/Game/Environment/Materials/Blockout/M_Roof_Red.M_Roof_Red");
    const TCHAR* AmberGlowPath = TEXT("/Game/Missions/Presentation/M_ObjectiveAmberGlow.M_ObjectiveAmberGlow");
    const TCHAR* RedGlowPath = TEXT("/Game/Missions/Presentation/M_TargetRedGlow.M_TargetRedGlow");
    const TCHAR* GreenGlowPath = TEXT("/Game/Missions/Presentation/M_SuccessGreenGlow.M_SuccessGreenGlow");
    const TCHAR* PadMaterialPath = TEXT("/Game/Environment/Materials/Blockout/M_Asphalt.M_Asphalt");
    const TCHAR* CompactHeliportPath = TEXT("/Game/Environment/Imported/Heliports/Compact/SM_Heliport_Compact/StaticMeshes/SM_Heliport_Compact.SM_Heliport_Compact");
}

namespace RotorlineEnemyFlightSafety
{
    // Keep AI aircraft outside each other's rotor envelopes during attack
    // passes. Actual contact is handled separately as a fatal mid-air crash.
    constexpr float CollisionEnvelopeCm = 800.0f;
    constexpr float HardSeparationCm = 6500.0f;
    constexpr float AvoidanceStartCm = 20000.0f;
    constexpr float PredictedConflictCm = 12000.0f;
    constexpr float PredictionSeconds = 3.0f;
    constexpr float AttackPassOffsetCm = 8000.0f;
    constexpr float MD500AttackPassOffsetCm = 7500.0f;
}

namespace RotorlineEnemyRotorVisuals
{
    // Enemy rotors need to read as flight-loaded at gameplay camera distances.
    // Keep per-frame angular travel below 180 degrees at 30 fps so the rotor
    // cannot alias into an apparent slow reverse. The much faster main-disc
    // rates below also lift the tail rate without crossing that limit.
    constexpr float TailRateMultiplier = 1.55f;
}

namespace RotorlineHawkLauncher
{
    // PCA inspection of the supplied missile geometry puts the authored rail
    // axis 10.9 degrees above horizontal. Keep gameplay and the visible rack
    // on that physical axis instead of manufacturing a vertical launch arc.
    constexpr float RailElevationDegrees = 11.0f;
    constexpr float StraightBoostSeconds = 1.50f;
    constexpr float MissileSpeedCmPerSecond = 10000.0f;
    constexpr float StraightBoostDistanceCm = StraightBoostSeconds * MissileSpeedCmPerSecond;
}

ARotorlineMissionObjectiveActor::ARotorlineMissionObjectiveActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Grounding = CreateDefaultSubobject<URotorlineGroundingComponent>(TEXT("Grounding"));
    Grounding->PlacementOwner = TEXT("MissionObjective");

    ModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ModelRoot"));
    ModelRoot->SetupAttachment(Root);

    FlakGunPivot = CreateDefaultSubobject<USceneComponent>(TEXT("FlakGunPivot"));
    FlakGunPivot->SetupAttachment(ModelRoot);

    HimarsLauncherPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HimarsLauncherPivot"));
    HimarsLauncherPivot->SetupAttachment(ModelRoot);

    ApacheMainRotorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ApacheMainRotorPivot"));
    ApacheMainRotorPivot->SetupAttachment(ModelRoot);

    ApacheTailRotorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ApacheTailRotorPivot"));
    ApacheTailRotorPivot->SetupAttachment(ModelRoot);

    MarkerRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerRing"));
    MarkerRing->SetupAttachment(Root);
    MarkerRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MarkerRing->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
    MarkerRing->SetCastShadow(false);
    MarkerRing->ComponentTags.Add(TEXT("RotorlineGroundingIgnore"));

    MarkerPulseRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerPulseRing"));
    MarkerPulseRing->SetupAttachment(Root);
    MarkerPulseRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MarkerPulseRing->SetRelativeLocation(FVector(0.0f, 0.0f, 42.0f));
    MarkerPulseRing->SetCastShadow(false);
    MarkerPulseRing->ComponentTags.Add(TEXT("RotorlineGroundingIgnore"));

    MarkerCenter = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerCenter"));
    MarkerCenter->SetupAttachment(Root);
    MarkerCenter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MarkerCenter->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f));
    MarkerCenter->SetCastShadow(false);
    MarkerCenter->ComponentTags.Add(TEXT("RotorlineGroundingIgnore"));

    MarkerBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerBeam"));
    MarkerBeam->SetupAttachment(Root);
    MarkerBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MarkerBeam->SetCastShadow(false);
    MarkerBeam->ComponentTags.Add(TEXT("RotorlineGroundingIgnore"));

    MarkerHLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerHLeft"));
    MarkerHRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerHRight"));
    MarkerHCross = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerHCross"));
    for (UStaticMeshComponent* Marking : { MarkerHLeft.Get(), MarkerHRight.Get(), MarkerHCross.Get() })
    {
        Marking->SetupAttachment(Root);
        Marking->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Marking->SetCastShadow(false);
        Marking->SetVisibility(false, true);
        Marking->ComponentTags.Add(TEXT("RotorlineGroundingIgnore"));
    }

    CabinLandingClearing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinLandingClearing"));
    CabinLandingClearing->SetupAttachment(Root);
    CabinLandingClearing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CabinLandingClearing->SetVisibility(false, true);
    CabinLandingClearing->ComponentTags.Add(TEXT("RotorlineGroundingIgnore"));

    LandingPadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LandingPadMesh"));
    LandingPadMesh->SetupAttachment(Root);
    LandingPadMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    LandingPadMesh->SetVisibility(false, true);

    PrimaryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrimaryMesh"));
    PrimaryMesh->SetupAttachment(ModelRoot);
    PrimaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    SecondaryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SecondaryMesh"));
    SecondaryMesh->SetupAttachment(ModelRoot);
    SecondaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    TertiaryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TertiaryMesh"));
    TertiaryMesh->SetupAttachment(ModelRoot);
    TertiaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // The supplied HIMARS is a multipart Sketchfab GLB. Its imported parts
    // preserve a shared source origin, so overlaying them reconstructs the
    // complete textured truck without baking another duplicate mesh asset.
    constexpr int32 HimarsPartCount = 19;
    for (int32 PartIndex = 0; PartIndex < HimarsPartCount; ++PartIndex)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(
            FName(*FString::Printf(TEXT("HimarsPart%02d"), PartIndex)));
        Part->SetupAttachment(ModelRoot);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetVisibility(false, true);
        HimarsMeshParts.Add(Part);
    }

    EnemyMainRotor = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EnemyMainRotor"));
    EnemyMainRotor->SetupAttachment(ModelRoot);
    EnemyMainRotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EnemyMainRotor->SetVisibility(false, true);

    EnemyTailRotor = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EnemyTailRotor"));
    EnemyTailRotor->SetupAttachment(ModelRoot);
    EnemyTailRotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EnemyTailRotor->SetVisibility(false, true);

    MuzzleFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlash"));
    MuzzleFlash->SetupAttachment(ModelRoot);
    MuzzleFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MuzzleFlash->SetVisibility(false, true);
    MuzzleFlash->ComponentTags.Add(TEXT("RotorlineGroundingIgnore"));

    MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
    MuzzleLight->SetupAttachment(MuzzleFlash);
    MuzzleLight->SetLightColor(FLinearColor(1.0f, 0.18f, 0.01f));
    MuzzleLight->SetIntensity(110000.0f);
    MuzzleLight->SetAttenuationRadius(2600.0f);
    MuzzleLight->SetVisibility(false, true);

    GroundFireFlameA = CreateDefaultSubobject<UNiagaraComponent>(TEXT("GroundFireFlameA"));
    GroundFireFlameA->SetupAttachment(ModelRoot);
    GroundFireFlameA->bAutoActivate = false;
    GroundFireFlameA->SetAutoDestroy(false);
    GroundFireFlameA->SetVisibility(false, true);

    GroundFireFlameB = CreateDefaultSubobject<UNiagaraComponent>(TEXT("GroundFireFlameB"));
    GroundFireFlameB->SetupAttachment(ModelRoot);
    GroundFireFlameB->bAutoActivate = false;
    GroundFireFlameB->SetAutoDestroy(false);
    GroundFireFlameB->SetVisibility(false, true);

    GroundFireSmoke = CreateDefaultSubobject<UNiagaraComponent>(TEXT("GroundFireSmoke"));
    GroundFireSmoke->SetupAttachment(ModelRoot);
    GroundFireSmoke->bAutoActivate = false;
    GroundFireSmoke->SetAutoDestroy(false);
    GroundFireSmoke->SetVisibility(false, true);

    GroundFireEmbers = CreateDefaultSubobject<UNiagaraComponent>(TEXT("GroundFireEmbers"));
    GroundFireEmbers->SetupAttachment(ModelRoot);
    GroundFireEmbers->bAutoActivate = false;
    GroundFireEmbers->SetAutoDestroy(false);
    GroundFireEmbers->SetVisibility(false, true);

    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> GroundFlameFinder(
        TEXT("/Game/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Splash_Flame_01.Niagara_Splash_Flame_01"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> GroundSmokeFinder(
        TEXT("/Game/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Smoke_Wind_01.Niagara_Smoke_Wind_01"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> GroundEmberFinder(
        TEXT("/Game/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Sparks_Explosion_01.Niagara_Sparks_Explosion_01"));
    if (GroundFlameFinder.Succeeded())
    {
        GroundFireFlameA->SetAsset(GroundFlameFinder.Object);
        GroundFireFlameB->SetAsset(GroundFlameFinder.Object);
    }
    if (GroundSmokeFinder.Succeeded()) GroundFireSmoke->SetAsset(GroundSmokeFinder.Object);
    if (GroundEmberFinder.Succeeded()) GroundFireEmbers->SetAsset(GroundEmberFinder.Object);

    GroundFireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GroundFireLight"));
    GroundFireLight->SetupAttachment(ModelRoot);
    GroundFireLight->SetLightColor(FLinearColor(1.0f, 0.17f, 0.008f));
    GroundFireLight->SetIntensity(28000.0f);
    GroundFireLight->SetAttenuationRadius(2600.0f);
    GroundFireLight->SetSourceRadius(45.0f);
    GroundFireLight->SetCastShadows(false);
    GroundFireLight->SetVisibility(false, true);

    FireAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("FireAudio"));
    FireAudio->SetupAttachment(ModelRoot);
    FireAudio->bAutoActivate = false;
    FireAudio->bAllowSpatialization = true;

    EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EnemyEngineAudio"));
    EngineAudio->SetupAttachment(ModelRoot);
    EngineAudio->bAutoActivate = false;
    EngineAudio->bAllowSpatialization = true;
    EngineAudio->SetVolumeMultiplier(0.44f);

    // Enemy vehicle loops must remain local to the vehicle. Without an
    // attenuation override every spawned tank is effectively mixed beside the
    // cockpit, and the loops stack into a full-volume wall of engine noise.
    EngineAudio->bOverrideAttenuation = true;
    EngineAudio->AttenuationOverrides.bAttenuate = true;
    EngineAudio->AttenuationOverrides.bSpatialize = true;
    EngineAudio->AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
    EngineAudio->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
    EngineAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(1200.0f, 0.0f, 0.0f);
    EngineAudio->AttenuationOverrides.FalloffDistance = 13800.0f;
    EngineAudio->AttenuationOverrides.dBAttenuationAtMax = -60.0f;

    CabinBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinBody"));
    CabinRoofLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinRoofLeft"));
    CabinRoofRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinRoofRight"));
    CabinDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinDoor"));
    CabinChimney = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinChimney"));
    for (UStaticMeshComponent* CabinPart : { CabinBody.Get(), CabinRoofLeft.Get(), CabinRoofRight.Get(), CabinDoor.Get(), CabinChimney.Get() })
    {
        CabinPart->SetupAttachment(ModelRoot);
        CabinPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CabinPart->SetVisibility(false, true);
    }

    SkeletalSubject = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalSubject"));
    SkeletalSubject->SetupAttachment(ModelRoot);
    SkeletalSubject->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ObjectiveLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ObjectiveLabel"));
    ObjectiveLabel->SetupAttachment(Root);
    ObjectiveLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    ObjectiveLabel->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    ObjectiveLabel->SetWorldSize(65.0f);
    ObjectiveLabel->SetTextRenderColor(FColor(255, 195, 55));
    ObjectiveLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 550.0f));

    BeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLight"));
    BeaconLight->SetupAttachment(Root);
    BeaconLight->SetRelativeLocation(FVector(0.0f, 0.0f, 500.0f));
    BeaconLight->SetLightColor(FLinearColor(1.0f, 0.43f, 0.05f));
    BeaconLight->SetIntensity(8000.0f);
    BeaconLight->SetAttenuationRadius(2200.0f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(RotorlineMissionVisuals::CylinderPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(RotorlineMissionVisuals::CubePath);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AmberGlowFinder(RotorlineMissionVisuals::AmberGlowPath);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PadFinder(RotorlineMissionVisuals::PadMaterialPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CompactHeliportFinder(RotorlineMissionVisuals::CompactHeliportPath);
    if (CylinderFinder.Succeeded())
    {
        MarkerRing->SetStaticMesh(CylinderFinder.Object);
        MarkerPulseRing->SetStaticMesh(CylinderFinder.Object);
        MarkerCenter->SetStaticMesh(CylinderFinder.Object);
        MarkerBeam->SetStaticMesh(CylinderFinder.Object);
        CabinLandingClearing->SetStaticMesh(CylinderFinder.Object);
    }
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(RotorlineMissionVisuals::SpherePath);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedGlowFinder(RotorlineMissionVisuals::RedGlowPath);
    if (SphereFinder.Succeeded()) MuzzleFlash->SetStaticMesh(SphereFinder.Object);
    if (RedGlowFinder.Succeeded()) MuzzleFlash->SetMaterial(0, RedGlowFinder.Object);
    MuzzleFlash->SetRelativeScale3D(FVector(0.24f, 0.24f, 0.55f));
    if (CubeFinder.Succeeded())
    {
        MarkerHLeft->SetStaticMesh(CubeFinder.Object);
        MarkerHRight->SetStaticMesh(CubeFinder.Object);
        MarkerHCross->SetStaticMesh(CubeFinder.Object);
    }
    if (AmberGlowFinder.Succeeded())
    {
        MarkerRing->SetMaterial(0, AmberGlowFinder.Object);
        MarkerPulseRing->SetMaterial(0, AmberGlowFinder.Object);
        MarkerBeam->SetMaterial(0, AmberGlowFinder.Object);
        MarkerHLeft->SetMaterial(0, AmberGlowFinder.Object);
        MarkerHRight->SetMaterial(0, AmberGlowFinder.Object);
        MarkerHCross->SetMaterial(0, AmberGlowFinder.Object);
    }
    if (PadFinder.Succeeded()) MarkerCenter->SetMaterial(0, PadFinder.Object);
    if (CompactHeliportFinder.Succeeded()) LandingPadMesh->SetStaticMesh(CompactHeliportFinder.Object);
    MarkerRing->SetRelativeScale3D(FVector(18.0f, 18.0f, 0.045f));
    MarkerRing->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
    MarkerPulseRing->SetRelativeScale3D(FVector(19.5f, 19.5f, 0.025f));
    MarkerPulseRing->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
    MarkerCenter->SetRelativeScale3D(FVector(15.8f, 15.8f, 0.052f));
    MarkerCenter->SetRelativeLocation(FVector(0.0f, 0.0f, 15.0f));
    MarkerCenter->SetVisibility(false, true);
    MarkerBeam->SetRelativeScale3D(FVector(0.10f, 0.10f, 35.0f));
    MarkerBeam->SetRelativeLocation(FVector(0.0f, 0.0f, 1750.0f));
    MarkerHLeft->SetRelativeScale3D(FVector(0.55f, 4.8f, 0.055f));
    MarkerHLeft->SetRelativeLocation(FVector(-300.0f, 0.0f, 22.0f));
    MarkerHRight->SetRelativeScale3D(FVector(0.55f, 4.8f, 0.055f));
    MarkerHRight->SetRelativeLocation(FVector(300.0f, 0.0f, 22.0f));
    MarkerHCross->SetRelativeScale3D(FVector(3.5f, 0.55f, 0.055f));
    MarkerHCross->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
}

void ARotorlineMissionObjectiveActor::SetGroundedLocation(const FVector& WorldLocation)
{
    SetActorLocation(WorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
    if (!GetWorld() || !Grounding)
    {
        bGroundPlacementReady = false;
        return;
    }

    const bool bAlreadyStable = Grounding->bGroundingApplied &&
        GetActorTransform().Equals(Grounding->LastAppliedTransform, 0.5f);
    if (!bAlreadyStable)
    {
        Grounding->Exclusion = ERotorlineGroundingExclusion::None;
        Grounding->ExclusionDetail.Reset();
        Grounding->bGroundingApplied = false;
    }

    if (IsAircraftThreat())
    {
        Grounding->SetExplicitExclusion(
            ERotorlineGroundingExclusion::Airborne,
            TEXT("Enemy aircraft intentionally starts above terrain"));
        FRotorlineGroundingProfile TerrainProbe = URotorlineGroundingLibrary::MakeProfile(
            ERotorlineGroundingMode::LinearPoint, TEXT("AirborneTerrainClearance"));
        TerrainProbe.bRejectObstructionsAboveGround = false;
        TerrainProbe.bCheckCollisionPenetration = false;
        FRotorlineGroundingResult ProbeResult;
        if (URotorlineGroundingLibrary::SolveGroundContact(
            this, WorldLocation, FVector2D::ZeroVector, this, TerrainProbe, ProbeResult))
        {
            FVector AirborneLocation = WorldLocation;
            AirborneLocation.Z = FMath::Max(WorldLocation.Z, ProbeResult.ContactPoint.Z + 3000.0f);
            SetActorLocation(AirborneLocation, false, nullptr, ETeleportType::TeleportPhysics);
        }
        return;
    }

    const bool bGroundThreat = ThreatType == ERotorlineThreatType::Tank ||
        ThreatType == ERotorlineThreatType::Flak ||
        ThreatType == ERotorlineThreatType::RadarMissile ||
        ThreatType == ERotorlineThreatType::RocketArtillery;
    FRotorlineGroundingProfile Profile;
    if (bGroundThreat)
    {
        const bool bMortar = TargetId.Contains(TEXT("mortar"), ESearchCase::IgnoreCase);
        const bool bVehicle = ThreatType == ERotorlineThreatType::Tank ||
            (ThreatType == ERotorlineThreatType::RocketArtillery && !bMortar);
        Profile = URotorlineGroundingLibrary::MakeProfile(
            bVehicle ? ERotorlineGroundingMode::Vehicle : ERotorlineGroundingMode::MultiPointFootprint,
            bMortar ? TEXT("MortarEmplacement") :
                (bVehicle ? TEXT("GroundCombatVehicle") : TEXT("GroundCombatEmplacement")));
        Profile.MaximumRelocationRadiusCm = 5200.0f;
        Profile.RelocationStepCm = 1200.0f;
        Profile.MaximumSlopeDegrees = bVehicle ? 18.0f : 10.0f;
        Profile.MaximumFootprintRoughnessCm = bVehicle ? 100.0f : 135.0f;
        if (bMortar)
        {
            Profile.MaximumSlopeDegrees = 12.0f;
            Profile.MaximumFootprintRoughnessCm = 75.0f;
            Profile.ContactComponentNames = { TEXT("PrimaryMesh") };
        }
        else if (ThreatType == ERotorlineThreatType::RocketArtillery)
        {
            Profile.ContactComponentNames = { TEXT("HimarsPart02"), TEXT("HimarsPart03"), TEXT("HimarsPart04") };
        }
        else
        {
            Profile.ContactComponentNames = { TEXT("PrimaryMesh") };
        }
    }
    else if (bLandingObjective && !LandingPadMesh->IsVisible())
    {
        // Home and other persistent service pads already provide the physical
        // landing surface. Ground only the guidance actor to that approved
        // surface instead of spawning and solving a second overlapping pad.
        Profile = URotorlineGroundingLibrary::MakeProfile(
            ERotorlineGroundingMode::LinearPoint, TEXT("PersistentMissionHelipadMarker"));
        Profile.bAllowPreparedGround = true;
        Profile.bRejectObstructionsAboveGround = false;
        FRotorlineGroundingResult PersistentPadResult;
        if (URotorlineGroundingLibrary::SolveGroundContact(
            this, WorldLocation, FVector2D::ZeroVector, this, Profile, PersistentPadResult))
        {
            FVector MarkerLocation = WorldLocation;
            MarkerLocation.Z = PersistentPadResult.ContactPoint.Z;
            SetActorLocation(MarkerLocation, false, nullptr, ETeleportType::TeleportPhysics);
            return;
        }
        bGroundPlacementReady = false;
        return;
    }
    else if (bLandingObjective)
    {
        Profile = URotorlineGroundingLibrary::MakeProfile(
            ERotorlineGroundingMode::MultiPointFootprint, TEXT("MissionHelipad"));
        Profile.bAllowPreparedGround = true;
        Profile.MaximumFootprintRoughnessCm = 160.0f;
        Profile.ContactComponentNames = { TEXT("LandingPadMesh") };
    }
    else
    {
        const bool bCabin = CabinBody && CabinBody->IsVisible() && CabinBody->GetStaticMesh();
        const bool bStaticProp = PrimaryMesh && PrimaryMesh->IsVisible() && PrimaryMesh->GetStaticMesh();
        const bool bPerson = SkeletalSubject && SkeletalSubject->IsVisible() && SkeletalSubject->GetSkeletalMeshAsset();
        const bool bReconObservationVehicle = ObjectiveKind.Equals(TEXT("designate-recon"), ESearchCase::IgnoreCase) &&
            (TargetId.Contains(TEXT("unarmed Ural"), ESearchCase::IgnoreCase) ||
                TargetId.Contains(TEXT("observation truck"), ESearchCase::IgnoreCase));
        if (bReconObservationVehicle)
        {
            Profile = URotorlineGroundingLibrary::MakeProfile(
                ERotorlineGroundingMode::Vehicle, TEXT("ReconObservationVehicle"));
            Profile.MaximumRelocationRadiusCm = 5200.0f;
            Profile.RelocationStepCm = 1200.0f;
            Profile.MaximumSlopeDegrees = 16.0f;
            Profile.MaximumFootprintRoughnessCm = 95.0f;
            Profile.ContactComponentNames = { TEXT("PrimaryMesh") };
        }
        else if (bCabin || bStaticProp || bPerson)
        {
            Profile = URotorlineGroundingLibrary::MakeProfile(
                bStaticProp ? ERotorlineGroundingMode::SurfaceAligned : ERotorlineGroundingMode::Upright,
                bCabin ? TEXT("MissionCabin") : (bPerson ? TEXT("RescueSubject") : TEXT("MissionGroundProp")));
            Profile.MaximumSlopeDegrees = bStaticProp ? 22.0f : 12.0f;
            Profile.ContactComponentNames = bCabin
                ? TArray<FName>{ TEXT("CabinBody") }
                : (bPerson ? TArray<FName>{ TEXT("SkeletalSubject") } : TArray<FName>{ TEXT("PrimaryMesh") });
        }
        else
        {
            // Pure navigation/interact markers have no physical base. Ground
            // their authored contact point through the same approved-surface
            // resolver without pretending the visual beam is a structure.
            Profile = URotorlineGroundingLibrary::MakeProfile(
                ERotorlineGroundingMode::LinearPoint, TEXT("MissionMarker"));
            Profile.bAllowPreparedGround = ObjectiveKind == TEXT("interact");
            Profile.bRejectObstructionsAboveGround = false;
            FRotorlineGroundingResult MarkerResult;
            if (URotorlineGroundingLibrary::SolveGroundContact(
                this, WorldLocation, FVector2D::ZeroVector, this, Profile, MarkerResult))
            {
                FVector MarkerLocation = WorldLocation;
                MarkerLocation.Z = MarkerResult.ContactPoint.Z;
                SetActorLocation(MarkerLocation, false, nullptr, ETeleportType::TeleportPhysics);

                // Keep the gameplay anchor and vertical guidance elements at
                // their authored transform, but conform the ground-level POI
                // visuals to the traced surface. A horizontal 36 m annulus
                // placed at the center contact height is mostly buried on any
                // slope, which made valid objectives appear missing.
                FVector SurfaceNormal = MarkerResult.SurfaceNormal.GetSafeNormal();
                if (SurfaceNormal.IsNearlyZero() || SurfaceNormal.Z <= 0.0f)
                {
                    SurfaceNormal = FVector::UpVector;
                }

                const FRotator SurfaceRotation = FRotationMatrix::MakeFromZ(SurfaceNormal).Rotator();
                const FVector SurfaceAnchor(MarkerLocation.X, MarkerLocation.Y, MarkerResult.ContactPoint.Z);
                const auto PlaceMarkerVisual = [&SurfaceAnchor, &SurfaceNormal, &SurfaceRotation](
                    UStaticMeshComponent* Component, const float ClearanceCm)
                {
                    if (!Component)
                    {
                        return;
                    }

                    Component->SetWorldLocation(SurfaceAnchor + (SurfaceNormal * ClearanceCm));
                    Component->SetWorldRotation(SurfaceRotation);
                };

                PlaceMarkerVisual(MarkerCenter, 48.0f);
                PlaceMarkerVisual(MarkerRing, 55.0f);
                PlaceMarkerVisual(MarkerPulseRing, 62.0f);

                UE_LOG(LogTemp, Log,
                    TEXT("ROTORLINE_POI_GROUNDING|target=%s|slope=%.1f|clearance_cm=55|normal=%s"),
                    *TargetId, MarkerResult.SurfaceSlopeDegrees, *SurfaceNormal.ToCompactString());
                return;
            }
            bGroundPlacementReady = false;
            return;
        }
    }

    Grounding->Profile = Profile;
    FRotorlineGroundingResult GroundingResult;
    bGroundPlacementReady = Grounding->ApplyGrounding(false, GroundingResult);
    if (!bGroundPlacementReady)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_GROUNDING_V2|target=%s|state=NO_SAFE_PLACEMENT|failure=%d|detail=%s|weapon_safe=1"),
            *TargetId, static_cast<int32>(GroundingResult.Failure), *GroundingResult.Detail);
    }
}

void ARotorlineMissionObjectiveActor::Configure(const FRotorlineObjectiveDefinition& Objective, const FVector& WorldLocation)
{
    ObjectiveKind = Objective.Kind;
    ObjectiveText = Objective.Text;
    TargetId = Objective.Target;
    bDestroyObjective = Objective.Kind == TEXT("destroy") ||
        Objective.Kind.Equals(TEXT("designate-strike"), ESearchCase::IgnoreCase);
    bLandingObjective = Objective.Kind == TEXT("land");
    const bool bUsesPersistentLandingPad = bLandingObjective &&
        (Objective.Site.Equals(TEXT("field-hospital"), ESearchCase::IgnoreCase) ||
            Objective.Site.Equals(TEXT("home"), ESearchCase::IgnoreCase) ||
            Objective.Site.Equals(TEXT("base"), ESearchCase::IgnoreCase) ||
            Objective.Site.Equals(TEXT("airfield"), ESearchCase::IgnoreCase));
    if (bLandingObjective)
    {
        Tags.AddUnique(TEXT("RotorlineMissionPad"));
    }
    else
    {
        Tags.Remove(TEXT("RotorlineMissionPad"));
    }
    bDestroyedTarget = false;
    PlayerDamageEventCount = 0;
    bWorldCombatMarkerEnabled = true;
    BeaconLight->SetVisibility(true, true);
    TargetHealth = 100.0f;
    bThreatVisualReady = true;
    bGroundPlacementReady = true;
    // Configure the visible/physical mission model before solving contact.
    // Grounding must measure the actual wheels, skids, pad, person, or prop;
    // running it here used to solve against an empty objective shell.
    SetActorLocation(WorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
    LandingPadMesh->SetVisibility(false, true);
    LandingPadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Keep every objective marker readable as a glowing annulus instead of a
    // flat white/colored disc.  The dark center also gives landing markers a
    // believable pad surface while preserving the emissive outer ring.
    MarkerCenter->SetVisibility(true, true);
    ObjectiveLabel->SetText(FText::FromString(Objective.Text.ToUpper()));
    if (bDestroyObjective)
    {
        if (UMaterialInterface* RedMaterial = LoadObject<UMaterialInterface>(nullptr, RotorlineMissionVisuals::RedGlowPath))
        {
            MarkerRing->SetMaterial(0, RedMaterial);
            MarkerPulseRing->SetMaterial(0, RedMaterial);
            MarkerBeam->SetMaterial(0, RedMaterial);
            MarkerHLeft->SetMaterial(0, RedMaterial);
            MarkerHRight->SetMaterial(0, RedMaterial);
            MarkerHCross->SetMaterial(0, RedMaterial);
        }
        MarkerBaseScale = 24.0f;
        MarkerRing->SetRelativeScale3D(FVector(MarkerBaseScale, MarkerBaseScale, 0.06f));
        MarkerPulseRing->SetRelativeScale3D(FVector(MarkerBaseScale * 1.10f, MarkerBaseScale * 1.10f, 0.035f));
        MarkerCenter->SetRelativeScale3D(FVector(MarkerBaseScale * 0.87f, MarkerBaseScale * 0.87f, 0.065f));
        MarkerCenter->SetVisibility(true, true);
        // Combat contacts are identified by the HUD target box, world label,
        // and close-range ground ring. Never attach a vertical beam to an
        // aircraft or vehicle in the production game.
        MarkerBeam->SetVisibility(false, true);
        MarkerHLeft->SetRelativeScale3D(FVector(0.65f, 7.2f, 0.065f));
        MarkerHRight->SetRelativeScale3D(FVector(0.65f, 7.2f, 0.065f));
        MarkerHLeft->SetRelativeLocation(FVector::ZeroVector + FVector(0.0f, 0.0f, 25.0f));
        MarkerHRight->SetRelativeLocation(FVector::ZeroVector + FVector(0.0f, 0.0f, 25.0f));
        MarkerHLeft->SetRelativeRotation(FRotator(0.0f, 45.0f, 0.0f));
        MarkerHRight->SetRelativeRotation(FRotator(0.0f, -45.0f, 0.0f));
        MarkerHLeft->SetVisibility(true, true);
        MarkerHRight->SetVisibility(true, true);
        MarkerHCross->SetVisibility(false, true);
        BeaconLight->SetLightColor(FLinearColor(1.0f, 0.03f, 0.01f));
        BeaconLight->SetIntensity(42000.0f);
        BeaconLight->SetAttenuationRadius(4200.0f);
        ObjectiveLabel->SetTextRenderColor(FColor(255, 70, 45));
    }
    else if (bLandingObjective)
    {
        MarkerBaseScale = 18.0f;
        // Use the supplied 27 m compact heliport as the physical touchdown
        // surface. The glow is guidance around a real structure, not the pad.
        LandingPadMesh->SetVisibility(!bUsesPersistentLandingPad, true);
        LandingPadMesh->SetCollisionEnabled(bUsesPersistentLandingPad
            ? ECollisionEnabled::NoCollision
            : ECollisionEnabled::QueryAndPhysics);
        LandingPadMesh->SetRelativeLocation(FVector::ZeroVector);
        LandingPadMesh->SetRelativeRotation(FRotator::ZeroRotator);
        MarkerCenter->SetVisibility(false, true);
        const float GuidanceHeightCm = bUsesPersistentLandingPad ? 38.0f : 326.0f;
        MarkerRing->SetRelativeLocation(FVector(0.0f, 0.0f, GuidanceHeightCm));
        MarkerPulseRing->SetRelativeLocation(FVector(0.0f, 0.0f, GuidanceHeightCm + 7.0f));
        MarkerHLeft->SetRelativeLocation(FVector(-300.0f, 0.0f, GuidanceHeightCm + 4.0f));
        MarkerHRight->SetRelativeLocation(FVector(300.0f, 0.0f, GuidanceHeightCm + 4.0f));
        MarkerHCross->SetRelativeLocation(FVector(0.0f, 0.0f, GuidanceHeightCm + 4.0f));
        MarkerHLeft->SetVisibility(true, true);
        MarkerHRight->SetVisibility(true, true);
        MarkerHCross->SetVisibility(true, true);
    }
    ConfigureMissionModel(Objective);
    SetGroundedLocation(WorldLocation);
    EnsureThreatVisualReady();
    GroundWeaponAimRotation = GetActorRotation();
    if (ThreatType == ERotorlineThreatType::Tank || ThreatType == ERotorlineThreatType::Flak ||
        ThreatType == ERotorlineThreatType::RadarMissile || ThreatType == ERotorlineThreatType::RocketArtillery)
    {
        // Ground enemies are physical units, not landing objectives. The old
        // 48 m emissive discs clipped through terrain and obscured the models;
        // targeting now stays in the HUD and the close-range world label.
        bWorldCombatMarkerEnabled = false;
        MarkerRing->SetVisibility(false, true);
        MarkerPulseRing->SetVisibility(false, true);
        MarkerCenter->SetVisibility(false, true);
        MarkerBeam->SetVisibility(false, true);
        MarkerHLeft->SetVisibility(false, true);
        MarkerHRight->SetVisibility(false, true);
        MarkerHCross->SetVisibility(false, true);
        BeaconLight->SetVisibility(false, true);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_MISSION_WORLD|SPAWN|kind=%s|target=%s|location=%.0f,%.0f,%.0f"),
        *Objective.Kind,
        Objective.Target.IsEmpty() ? TEXT("navigation") : *Objective.Target,
        GetActorLocation().X,
        GetActorLocation().Y,
        GetActorLocation().Z);
}

bool ARotorlineMissionObjectiveActor::SetStaticModel(
    UStaticMeshComponent* Component,
    const TCHAR* AssetPath,
    const FVector& Scale,
    const FVector& Offset,
    const FRotator& Rotation)
{
    if (!Component)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_ENEMY_VISUAL|state=LOAD_FAILED|reason=NULL_COMPONENT|asset=%s"), AssetPath);
        return false;
    }
    if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, AssetPath))
    {
        Component->SetStaticMesh(Mesh);
        Component->SetRelativeScale3D(Scale);
        Component->SetRelativeLocation(Offset);
        Component->SetRelativeRotation(Rotation);
        Component->SetVisibility(true, true);
        Component->SetHiddenInGame(false, true);
        return true;
    }
    UE_LOG(LogTemp, Error, TEXT("ROTORLINE_ENEMY_VISUAL|state=LOAD_FAILED|asset=%s"), AssetPath);
    return false;
}

void ARotorlineMissionObjectiveActor::EnsureThreatVisualReady()
{
    if (ThreatType == ERotorlineThreatType::None) return;

    const auto HasStaticGeometry = [](const UStaticMeshComponent* Component)
    {
        return Component && Component->GetStaticMesh() && Component->IsVisible();
    };
    const auto HasSkeletalGeometry = [](const USkeletalMeshComponent* Component)
    {
        return Component && Component->GetSkeletalMeshAsset() && Component->IsVisible();
    };
    const auto CountResolvedGeometry = [&]()
    {
        int32 Count = 0;
        Count += HasStaticGeometry(PrimaryMesh) ? 1 : 0;
        Count += HasStaticGeometry(SecondaryMesh) ? 1 : 0;
        Count += HasStaticGeometry(TertiaryMesh) ? 1 : 0;
        Count += HasSkeletalGeometry(EnemyMainRotor) ? 1 : 0;
        Count += HasSkeletalGeometry(EnemyTailRotor) ? 1 : 0;
        for (const UStaticMeshComponent* Part : HimarsMeshParts)
        {
            Count += HasStaticGeometry(Part) ? 1 : 0;
        }
        return Count;
    };

    int32 GeometryCount = CountResolvedGeometry();
    bool bUsedFallback = false;
    if (GeometryCount == 0)
    {
        // A threat may never keep firing as a ghost just because an imported
        // package was omitted or renamed. Engine primitives are hard-referenced
        // by this actor and therefore remain available in every cooked level.
        ModelRoot->SetRelativeRotation(FRotator::ZeroRotator);
        if (IsAircraftThreat())
        {
            SetStaticModel(PrimaryMesh, RotorlineMissionVisuals::CubePath,
                FVector(5.4f, 1.45f, 0.95f), FVector(0.0f, 0.0f, 30.0f));
            SetStaticModel(SecondaryMesh, RotorlineMissionVisuals::CubePath,
                FVector(0.20f, 7.0f, 0.10f), FVector(0.0f, 0.0f, 145.0f));
        }
        else
        {
            SetStaticModel(PrimaryMesh, RotorlineMissionVisuals::CubePath,
                FVector(4.6f, 2.15f, 0.75f), FVector(0.0f, 0.0f, 38.0f));
            SetStaticModel(SecondaryMesh, RotorlineMissionVisuals::CubePath,
                FVector(2.0f, 1.45f, 0.42f), FVector(35.0f, 0.0f, 100.0f));
        }
        GeometryCount = CountResolvedGeometry();
        bUsedFallback = GeometryCount > 0;
    }

    bThreatVisualReady = GeometryCount > 0 && bGroundPlacementReady;
    if (!bThreatVisualReady)
    {
        // Last-resort safety: an unresolved actor remains inert instead of
        // damaging the player from an invisible surface layer.
        FireAudio->Stop();
        EngineAudio->Stop();
    }
    if (bThreatVisualReady)
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_ENEMY_VISUAL|target=%s|threat=%d|geometry=%d|state=%s"),
            *TargetId,
            static_cast<int32>(ThreatType),
            GeometryCount,
            bUsedFallback ? TEXT("FALLBACK_VISIBLE") : TEXT("READY"));
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_ENEMY_VISUAL|target=%s|threat=%d|geometry=0|state=DISARMED"),
            *TargetId,
            static_cast<int32>(ThreatType));
    }
}

void ARotorlineMissionObjectiveActor::SetMissionMarkerVisibility(bool bVisible)
{
    bWorldCombatMarkerEnabled = bVisible;
    MarkerRing->SetVisibility(bVisible, true);
    MarkerPulseRing->SetVisibility(bVisible, true);
    MarkerCenter->SetVisibility(bVisible, true);
    MarkerBeam->SetVisibility(bVisible, true);
    MarkerHLeft->SetVisibility(bVisible, true);
    MarkerHRight->SetVisibility(bVisible, true);
    MarkerHCross->SetVisibility(bVisible, true);
    ObjectiveLabel->SetVisibility(bVisible, true);
    BeaconLight->SetVisibility(bVisible, true);
}

void ARotorlineMissionObjectiveActor::ConfigureMissionModel(const FRotorlineObjectiveDefinition& Objective)
{
    // Rotor meshes are reused by other objective types. Restore their default
    // attachment before applying an airframe-specific pivot hierarchy.
    SecondaryMesh->AttachToComponent(ModelRoot, FAttachmentTransformRules::KeepRelativeTransform);
    TertiaryMesh->AttachToComponent(ModelRoot, FAttachmentTransformRules::KeepRelativeTransform);
    HimarsLauncherPivot->SetRelativeLocation(FVector::ZeroVector);
    HimarsLauncherPivot->SetRelativeRotation(FRotator::ZeroRotator);
    ApacheMainRotorPivot->SetRelativeLocation(FVector::ZeroVector);
    ApacheMainRotorPivot->SetRelativeRotation(FRotator::ZeroRotator);
    ApacheTailRotorPivot->SetRelativeLocation(FVector::ZeroVector);
    ApacheTailRotorPivot->SetRelativeRotation(FRotator::ZeroRotator);
    ModelRoot->SetRelativeLocation(FVector::ZeroVector);
    ModelRoot->SetRelativeRotation(FRotator::ZeroRotator);
    PrimaryMesh->SetVisibility(false, true);
    SecondaryMesh->SetVisibility(false, true);
    TertiaryMesh->SetVisibility(false, true);
    for (UStaticMeshComponent* HimarsPart : HimarsMeshParts)
    {
        HimarsPart->SetVisibility(false, true);
    }
    EnemyMainRotor->SetVisibility(false, true);
    EnemyTailRotor->SetVisibility(false, true);
    SkeletalSubject->SetVisibility(false, true);
    MuzzleFlash->SetVisibility(false, true);
    MuzzleLight->SetVisibility(false, true);
    FireAudio->Stop();
    EngineAudio->Stop();
    FireAudioBaseVolume = 0.42f;
    EngineAudioBaseVolume = 0.34f;
    FireSmokeMaterial = nullptr;
    bFireScene = false;
    bUseNiagaraFire = false;
    for (UNiagaraComponent* FireComponent :
        { GroundFireFlameA.Get(), GroundFireFlameB.Get(), GroundFireSmoke.Get(), GroundFireEmbers.Get() })
    {
        FireComponent->DeactivateImmediate();
        FireComponent->SetVisibility(false, true);
    }
    GroundFireLight->SetVisibility(false, true);
    NextFireFlameATime = 0.0f;
    NextFireFlameBTime = 0.0f;
    NextFireEmberTime = 0.0f;
    ThreatType = ERotorlineThreatType::None;
    EnemyAirframe = ERotorlineEnemyAirframe::None;
    AircraftVelocity = FVector::ZeroVector;
    AircraftBreakawayWaypoint = FVector::ZeroVector;
    AircraftForwardSpeed = 0.0f;
    AircraftAttackSpeed = 0.0f;
    AircraftBreakSpeed = 0.0f;
    AircraftAcceleration = 0.0f;
    AircraftTurnRate = 0.0f;
    AircraftRotorRate = 0.0f;
    AircraftManeuverTime = 0.0f;
    LastAircraftAuditLogTime = -1000.0f;
    AircraftLastTurnRate = 0.0f;
    AircraftLastAcceleration = 0.0f;
    AircraftLastVelocityDot = 1.0f;
    AircraftLastTargetDot = 1.0f;
    AircraftQualificationElapsed = 0.0f;
    AircraftQualificationNextShotTime = 1.5f;
    AircraftQualificationMinVelocityDot = 1.0f;
    AircraftQualificationMaxTurnRate = 0.0f;
    AircraftQualificationMaxAcceleration = 0.0f;
    AircraftQualificationSamples = 0;
    AircraftQualificationShots = 0;
    AircraftQualificationMilestone = 0;
    AircraftDamageQualificationStage = 0;
    AircraftDeathElapsed = 0.0f;
    AircraftDeathSmokeAccumulator = 0.0f;
    AircraftFallSpeed = 0.0f;
    AircraftRotorSpinScale = 1.0f;
    AircraftPassSide = FMath::FRand() < 0.5f ? -1 : 1;
    bAircraftAttackRun = true;
    bAircraftDying = false;
    bEnemyFlightQualificationMode = false;
    CabinLandingClearing->SetVisibility(false, true);
    CabinLandingClearing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const FString SearchText = (Objective.Text + TEXT(" ") + Objective.Target + TEXT(" ") + Objective.Site).ToLower();

    if (SearchText.Contains(TEXT("rooftop-extraction-team")))
    {
        // Mission 21's extraction unit is visual-only. Keep the center of the
        // hospital rooftop clear for the MD-500 and disable every character's
        // collision so skids, rotors, and mission interaction cannot snag on
        // imported physics bodies.
        struct FRooftopTeamMember
        {
            const TCHAR* Name;
            const TCHAR* AssetPath;
            FVector Offset;
            float Yaw;
        };
        const FRooftopTeamMember Team[] = {
            {
                TEXT("RooftopTeamLead"),
                TEXT("/Game/Characters/RooftopExtraction/Runtime/RT_PolishSoldier/StaticMeshes/RT_PolishSoldier.RT_PolishSoldier"),
                FVector(-350.0f, -250.0f, 0.0f),
                45.0f
            },
            {
                TEXT("RooftopTeamRiflemanA"),
                TEXT("/Game/Characters/RooftopExtraction/Runtime/RT_UkrainianDMR/StaticMeshes/RT_UkrainianDMR.RT_UkrainianDMR"),
                FVector(350.0f, -250.0f, 0.0f),
                135.0f
            },
            {
                TEXT("RooftopTeamRiflemanB"),
                TEXT("/Game/Characters/RooftopExtraction/Runtime/RT_UkrainianDMR/StaticMeshes/RT_UkrainianDMR.RT_UkrainianDMR"),
                FVector(-350.0f, 250.0f, 0.0f),
                -45.0f
            },
            {
                TEXT("RooftopTeamSupport"),
                TEXT("/Game/Characters/RooftopExtraction/Runtime/RT_UkrainianDMR/StaticMeshes/RT_UkrainianDMR.RT_UkrainianDMR"),
                FVector(350.0f, 250.0f, 0.0f),
                -135.0f
            }
        };

        int32 ReadyMembers = 0;
        for (const FRooftopTeamMember& Member : Team)
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Member.AssetPath);
            if (!Mesh) continue;

            UStaticMeshComponent* TeamMember =
                NewObject<UStaticMeshComponent>(this, FName(Member.Name));
            TeamMember->SetupAttachment(ModelRoot);
            TeamMember->SetStaticMesh(Mesh);
            TeamMember->SetRelativeLocation(Member.Offset);
            TeamMember->SetRelativeRotation(FRotator(0.0f, Member.Yaw, 0.0f));
            TeamMember->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            TeamMember->SetGenerateOverlapEvents(false);
            TeamMember->SetCanEverAffectNavigation(false);
            TeamMember->SetCastShadow(true);
            AddInstanceComponent(TeamMember);
            TeamMember->RegisterComponent();
            ++ReadyMembers;
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_ROOFTOP_EXTRACTION|TEAM_READY|members=%d|collision=DISABLED|pickup=CENTER_CLEAR"),
            ReadyMembers);
    }
    else if (SearchText.Contains(TEXT("unarmed ural")) || SearchText.Contains(TEXT("observation truck")))
    {
        // Mission 6 reconnaissance contacts are parked logistics vehicles,
        // not threats. Resolve this explicit mission role before generic words
        // such as "radar" or "summit" can classify the truck as a weapon.
        ThreatType = ERotorlineThreatType::None;
        ModelRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Vehicles/UserAdded/CombatReady/Ural4320_Body/Ural4320_Body/StaticMeshes/Ural4320_Body.Ural4320_Body"),
            FVector(1.0f));
    }
    else if (SearchText.Contains(TEXT("jaguar")) || SearchText.Contains(TEXT("ebrc")))
    {
        // Mission 19's hero target. The supplied GLB imports nose-forward on +Y
        // and with its wheel contact 171 cm below the shared source origin.
        // It intentionally remains passive until the allied strike arrives.
        ThreatType = ERotorlineThreatType::None;
        TargetHealth = 260.0f;
        ModelRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 171.1f));
        ModelRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Vehicles/Hostile/HeroTargets/EBRCJaguar/ebrc_jaguar_infantry_fighting_vehicle/StaticMeshes/ebrc_jaguar_infantry_fighting_vehicle.ebrc_jaguar_infantry_fighting_vehicle"),
            FVector(1.0f));
    }
    else if (SearchText.Contains(TEXT("parked aircraft flight line")))
    {
        // Mission 22 attacks the staged aircraft while they are still on the
        // apron. Keep this objective stationary and destructible rather than
        // classifying any of its silhouettes as an airborne combat encounter.
        ThreatType = ERotorlineThreatType::None;
        EnemyAirframe = ERotorlineEnemyAirframe::None;
        TargetHealth = 280.0f;
        ModelRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyApacheMk1_Body/EnemyApacheMk1_Body/StaticMeshes/EnemyApacheMk1_Body.EnemyApacheMk1_Body"),
            FVector(1.0f),
            FVector(-1350.0f, -900.0f, 40.0f));
        SetStaticModel(
            SecondaryMesh,
            TEXT("/Game/Vehicles/Hostile/EnemyHind_Body/EnemyHind_Body/StaticMeshes/EnemyHind_Body.EnemyHind_Body"),
            FVector(0.01f),
            FVector(0.0f, 900.0f, 40.0f));
        SetStaticModel(
            TertiaryMesh,
            TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/StaticMeshes/Helicopter_Helicopter_Material_0.Helicopter_Helicopter_Material_0"),
            FVector(1.0f),
            FVector(1350.0f, -900.0f, 40.0f));
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_M22_FLIGHT_LINE|READY|aircraft=3|state=PARKED|airborne_threat=0"));
    }
    else if (SearchText.Contains(TEXT("hind")))
    {
        ThreatType = ERotorlineThreatType::RocketGunship;
        EnemyAirframe = ERotorlineEnemyAirframe::Hind;
        TargetHealth = 160.0f;
        // Processed Hind and MD-500 assets both import nose-forward on +Y.
        // Rotate +Y onto Unreal's actor-forward +X once at the shared model root.
        ModelRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Vehicles/Hostile/EnemyHind_Body/EnemyHind_Body/StaticMeshes/EnemyHind_Body.EnemyHind_Body"),
            FVector(0.01f));
        SetStaticModel(
            SecondaryMesh,
            TEXT("/Game/Vehicles/Hostile/EnemyHind_MainRotor/EnemyHind_MainRotor/StaticMeshes/EnemyHind_MainRotor.EnemyHind_MainRotor"),
            FVector(0.01f));
    }
    else if (SearchText.Contains(TEXT("apache")))
    {
        ThreatType = ERotorlineThreatType::RocketGunship;
        EnemyAirframe = ERotorlineEnemyAirframe::Apache;
        TargetHealth = 140.0f;
        // The supplied Mk.1 was regrouped by material primitives into three
        // runtime meshes that preserve the original shared pivot.
        ModelRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyApacheMk1_Body/EnemyApacheMk1_Body/StaticMeshes/EnemyApacheMk1_Body.EnemyApacheMk1_Body"),
            FVector(1.0f));
        SetStaticModel(
            SecondaryMesh,
            TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyApacheMk1_MainRotor/EnemyApacheMk1_MainRotor/StaticMeshes/EnemyApacheMk1_MainRotor.EnemyApacheMk1_MainRotor"),
            FVector(1.0f));
        SetStaticModel(
            TertiaryMesh,
            TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyApacheMk1_TailRotor/EnemyApacheMk1_TailRotor/StaticMeshes/EnemyApacheMk1_TailRotor.EnemyApacheMk1_TailRotor"),
            FVector(1.0f));

        // The GLB parts share the source model origin. Rotate the rotor pivot,
        // not the full tail-rotor mesh around that distant origin.
        const FVector MainRotorCenter(-0.117f, -2.527f, 192.580f);
        ApacheMainRotorPivot->SetRelativeLocation(MainRotorCenter);
        SecondaryMesh->AttachToComponent(ApacheMainRotorPivot, FAttachmentTransformRules::KeepRelativeTransform);
        SecondaryMesh->SetRelativeLocation(-MainRotorCenter);
        SecondaryMesh->SetRelativeRotation(FRotator::ZeroRotator);

        const FVector TailRotorCenter(69.163f, -922.332f, 102.102f);
        ApacheTailRotorPivot->SetRelativeLocation(TailRotorCenter);
        TertiaryMesh->AttachToComponent(ApacheTailRotorPivot, FAttachmentTransformRules::KeepRelativeTransform);
        TertiaryMesh->SetRelativeLocation(-TailRotorCenter);
        TertiaryMesh->SetRelativeRotation(FRotator::ZeroRotator);
    }
    else if (SearchText.Contains(TEXT("gunship")) || SearchText.Contains(TEXT("md500")))
    {
        ThreatType = ERotorlineThreatType::MachineGunship;
        EnemyAirframe = ERotorlineEnemyAirframe::MD500;
        TargetHealth = 110.0f;
        // This source model is authored with +Y as the nose.  Match the same
        // -90 degree alignment used by the playable MD-500 so actor +X is
        // visibly the aircraft's forward direction.
        ModelRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        SetStaticModel(PrimaryMesh, TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/StaticMeshes/Helicopter_Helicopter_Material_0.Helicopter_Helicopter_Material_0"), FVector(1.0f));
        SetStaticModel(SecondaryMesh, TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/StaticMeshes/Cockpit_Cockpit_Material_0.Cockpit_Cockpit_Material_0"), FVector(1.0f));
        SetStaticModel(TertiaryMesh, TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/StaticMeshes/Glass_Glass_Material_0.Glass_Glass_Material_0"), FVector(1.0f));
        if (USkeletalMesh* MainRotor = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/SkeletalMeshes/Top_rotor_Helicopter_Material_0.Top_rotor_Helicopter_Material_0")))
        {
            EnemyMainRotor->SetSkeletalMeshAsset(MainRotor);
            EnemyMainRotor->SetVisibility(true, true);
        }
        if (USkeletalMesh* TailRotor = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/SkeletalMeshes/Tail_Rotor_Helicopter_Material_0.Tail_Rotor_Helicopter_Material_0")))
        {
            EnemyTailRotor->SetSkeletalMeshAsset(TailRotor);
            EnemyTailRotor->SetVisibility(true, true);
        }
        if (UAnimSequence* RotorAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/SkeletalMeshes/md-500_defender_helicopter_Anim.md-500_defender_helicopter_Anim")))
        {
            EnemyMainRotor->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            EnemyTailRotor->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            EnemyMainRotor->SetAnimation(RotorAnim);
            EnemyTailRotor->SetAnimation(RotorAnim);
            EnemyMainRotor->Play(true);
            EnemyTailRotor->Play(true);
            EnemyMainRotor->SetPlayRate(2.4f);
            EnemyTailRotor->SetPlayRate(2.4f);
        }
    }
    else if (SearchText.Contains(TEXT("hawk")))
    {
        ThreatType = ERotorlineThreatType::RadarMissile;
        TargetHealth = 130.0f;
        // The prepared HAWK source is authored nose-forward on +Y and was
        // normalized to real-world centimetres with its wheel/contact plane at
        // Z=0. Rotate +Y onto actor-forward +X so the ridge deployment can face
        // the valley corridor without pitching or rolling the chassis.
        ModelRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Vehicles/UserAdded/CombatReady/MIM23Hawk_Body/MIM23Hawk_Body/StaticMeshes/MIM23Hawk_Body.MIM23Hawk_Body"),
            FVector(1.0f));
        SetStaticModel(
            SecondaryMesh,
            TEXT("/Game/Vehicles/UserAdded/CombatReady/MIM23Hawk_Launcher/MIM23Hawk_Launcher/StaticMeshes/MIM23Hawk_Launcher.MIM23Hawk_Launcher"),
            FVector(1.0f));
    }
    else if (SearchText.Contains(TEXT("radar")))
    {
        ThreatType = ERotorlineThreatType::RadarMissile;
        TargetHealth = 120.0f;
        ModelRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Missions/Assets/Radar/1s91_straight_flushlow_poly/StaticMeshes/Hull_Material__26_0.Hull_Material__26_0"),
            FVector(1.25f));
        SetStaticModel(
            SecondaryMesh,
            TEXT("/Game/Missions/Assets/Radar/1s91_straight_flushlow_poly/StaticMeshes/radar_3_Material__25_0.radar_3_Material__25_0"),
            FVector(1.25f),
            FVector(0.0f, 0.0f, 105.0f));
    }
    else if (SearchText.Contains(TEXT("tank")) || SearchText.Contains(TEXT("armor")))
    {
        ThreatType = ERotorlineThreatType::Tank;
        TargetHealth = 150.0f;
        ModelRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 18.0f));
        ModelRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        const FVector TankScale(1.0f);
        SetStaticModel(PrimaryMesh, TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyChallengerMk3_Body/EnemyChallengerMk3_Body/StaticMeshes/EnemyChallengerMk3_Body.EnemyChallengerMk3_Body"), TankScale);
        SetStaticModel(SecondaryMesh, TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyChallengerMk3_Turret/EnemyChallengerMk3_Turret/StaticMeshes/EnemyChallengerMk3_Turret.EnemyChallengerMk3_Turret"), TankScale);
        SetStaticModel(TertiaryMesh, TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyChallengerMk3_Barrel/EnemyChallengerMk3_Barrel/StaticMeshes/EnemyChallengerMk3_Barrel.EnemyChallengerMk3_Barrel"), TankScale);
    }
    else if (SearchText.Contains(TEXT("flak")))
    {
        ThreatType = ERotorlineThreatType::Flak;
        TargetHealth = 100.0f;
        ModelRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Missions/Assets/Flak/flak_cannon_with_adats_missiles/StaticMeshes/Object_3.Object_3"),
            FVector(0.35f));
        SetStaticModel(
            SecondaryMesh,
            TEXT("/Game/Missions/Assets/Flak/flak_cannon_with_adats_missiles/StaticMeshes/Object_2.Object_2"),
            FVector(0.35f));
        // Object_2 is the complete upper gun assembly. Its source pivot sits at
        // the model origin, so move the component back by the scaled trunnion
        // height and rotate the dedicated pivot around that physical joint.
        constexpr float FlakSourceScale = 0.35f;
        const FVector FlakTrunnion(0.0f, 0.0f, 385.0f * FlakSourceScale);
        FlakGunPivot->SetRelativeLocation(FlakTrunnion);
        SecondaryMesh->AttachToComponent(FlakGunPivot, FAttachmentTransformRules::KeepRelativeTransform);
        SecondaryMesh->SetRelativeLocation(-FlakTrunnion);
        SecondaryMesh->SetRelativeRotation(FRotator::ZeroRotator);
    }
    else if (SearchText.Contains(TEXT("mortar")))
    {
        ThreatType = ERotorlineThreatType::RocketArtillery;
        TargetHealth = 75.0f;
        // Build a recognizable 120 mm mortar from cooked engine primitives:
        // base plate, elevated tube, and rear bipod. It remains a compact
        // emplacement rather than borrowing the large HIMARS truck model.
        SetStaticModel(PrimaryMesh, RotorlineMissionVisuals::CylinderPath,
            FVector(1.25f, 1.25f, 0.08f), FVector(0.0f, 0.0f, 8.0f));
        SetStaticModel(SecondaryMesh, RotorlineMissionVisuals::CylinderPath,
            FVector(0.13f, 0.13f, 1.20f), FVector(0.0f, 0.0f, 108.0f),
            FRotator(0.0f, 0.0f, -35.0f));
        SetStaticModel(TertiaryMesh, RotorlineMissionVisuals::CubePath,
            FVector(0.10f, 0.72f, 0.10f), FVector(-46.0f, 0.0f, 52.0f),
            FRotator(0.0f, 0.0f, 22.0f));
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MORTAR|CONFIG|target=%s|weapon=BALLISTIC_120MM|visual=BASEPLATE_TUBE_BIPOD"),
            *TargetId);
    }
    else if (SearchText.Contains(TEXT("himars")) || SearchText.Contains(TEXT("rocket artillery")) ||
        SearchText.Contains(TEXT("rocket battery")))
    {
        ThreatType = ERotorlineThreatType::RocketArtillery;
        TargetHealth = 130.0f;

        // Import inspection measured the source wheel bottoms at Z=-0.150 cm.
        // The GLB root also carries a 0.001 scale, so a 1000x component scale
        // restores real-world dimensions and +150 cm places the tire contact
        // plane exactly on the actor's terrain-snapped origin.
        ModelRoot->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
        constexpr float HimarsSourceScale = 1000.0f;
        constexpr float HimarsWheelContactOffsetCm = 150.0f;
        static const TCHAR* HimarsPartPaths[] = {
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/chassis_Material__147_0.chassis_Material__147_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/chassis_Material__148_0.chassis_Material__148_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/wheels_1_Material__147_0.wheels_1_Material__147_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/wheels_2_Material__147_0.wheels_2_Material__147_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/wheels_3_Material__147_0.wheels_3_Material__147_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/glass_03_-_Default_0.glass_03_-_Default_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/Object01_Material__63_0.Object01_Material__63_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/Object01_Material__62_0.Object01_Material__62_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/Object02_03_-_Default_0.Object02_03_-_Default_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/Object03_03_-_Default_0.Object03_03_-_Default_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/Object04_03_-_Default_0.Object04_03_-_Default_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/Object05_03_-_Default_0.Object05_03_-_Default_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/door_rf_ok_Material__147_0.door_rf_ok_Material__147_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/door_rf_ok_Material__148_0.door_rf_ok_Material__148_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/door_lf_ok_Material__147_0.door_lf_ok_Material__147_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/door_lf_ok_Material__148_0.door_lf_ok_Material__148_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/bonnet_ok_Material__147_0.bonnet_ok_Material__147_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/bonnet_ok_Material__148_0.bonnet_ok_Material__148_0"),
            TEXT("/Game/Vehicles/Hostile/HIMARS/ukrainian_m142_himars/StaticMeshes/misc_a_Material__147_0.misc_a_Material__147_0")
        };
        check(HimarsMeshParts.Num() == UE_ARRAY_COUNT(HimarsPartPaths));

        const FVector CommonOffset(0.0f, 0.0f, HimarsWheelContactOffsetCm);
        const FVector LauncherPivotLocation(0.0f, 155.0f, 245.0f);
        HimarsLauncherPivot->SetRelativeLocation(LauncherPivotLocation);
        for (int32 PartIndex = 0; PartIndex < HimarsMeshParts.Num(); ++PartIndex)
        {
            const bool bLauncherAssembly = PartIndex == HimarsMeshParts.Num() - 1;
            UStaticMeshComponent* Part = HimarsMeshParts[PartIndex];
            Part->AttachToComponent(
                bLauncherAssembly ? HimarsLauncherPivot.Get() : ModelRoot.Get(),
                FAttachmentTransformRules::KeepRelativeTransform);
            SetStaticModel(
                Part,
                HimarsPartPaths[PartIndex],
                FVector(HimarsSourceScale),
                bLauncherAssembly ? CommonOffset - LauncherPivotLocation : CommonOffset);
        }

        float WheelBottomWorldZ = TNumericLimits<float>::Max();
        for (int32 WheelIndex = 2; WheelIndex <= 4; ++WheelIndex)
        {
            HimarsMeshParts[WheelIndex]->UpdateBounds();
            const FBoxSphereBounds& WheelBounds = HimarsMeshParts[WheelIndex]->Bounds;
            WheelBottomWorldZ = FMath::Min(WheelBottomWorldZ, WheelBounds.Origin.Z - WheelBounds.BoxExtent.Z);
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_HIMARS_PLACEMENT|wheel_bottom_z=%.1f|terrain_origin_z=%.1f|contact_error_cm=%.1f|upright_dot=%.3f|scale=%.0f"),
            WheelBottomWorldZ,
            GetActorLocation().Z,
            WheelBottomWorldZ - GetActorLocation().Z,
            FVector::DotProduct(ModelRoot->GetUpVector(), FVector::UpVector),
            HimarsSourceScale);
    }
    else if (SearchText.Contains(TEXT("cabin")))
    {
        // The authored map already contains the imported cabin GLB and its
        // adjacent service pad. This mission actor supplies only the objective
        // marker; never overlay procedural cabin geometry or another clearing.
        for (UStaticMeshComponent* CabinPart : {
            CabinBody.Get(), CabinRoofLeft.Get(), CabinRoofRight.Get(),
            CabinDoor.Get(), CabinChimney.Get(), CabinLandingClearing.Get(),
            LandingPadMesh.Get() })
        {
            if (!CabinPart) continue;
            CabinPart->SetVisibility(false, true);
            CabinPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSION_PROP|site=%s|asset=AUTHORED_CABIN_AND_PAD|state=REFERENCE_ONLY"),
            *Objective.Site);
    }
    else if (SearchText.Contains(TEXT("crash")) || SearchText.Contains(TEXT("wreck")) || SearchText.Contains(TEXT("downed")) || SearchText.Contains(TEXT("broken bird")))
    {
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Missions/Assets/CrashedPlane/low_poly_-_crashed_plane/StaticMeshes/low_poly_-_crashed_plane.low_poly_-_crashed_plane"),
            FVector(0.35f),
            FVector(0.0f, 0.0f, 80.0f),
            FRotator(-8.0f, 25.0f, 4.0f));
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSION_PROP|site=%s|asset=CRASHED_PLANE_GLB|state=VISIBLE"),
            *Objective.Site);
    }
    else if (SearchText.Contains(TEXT("pilot")) || SearchText.Contains(TEXT("survivor")) || SearchText.Contains(TEXT("civilian")) || SearchText.Contains(TEXT("liaison")))
    {
        if (USkeletalMesh* PilotMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Missions/Assets/Pilot/pilot_low_poly_character/SkeletalMeshes/pilot_low_poly_character.pilot_low_poly_character")))
        {
            SkeletalSubject->SetSkeletalMeshAsset(PilotMesh);
            SkeletalSubject->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
            SkeletalSubject->SetVisibility(true, true);
        }
    }
    else if (SearchText.Contains(TEXT("relay")))
    {
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Missions/Assets/RelayTower/mobile_tower_free_low_poly/StaticMeshes/Object_4.Object_4"),
            FVector(0.045f));
    }
    else if (SearchText.Contains(TEXT("load")) || SearchText.Contains(TEXT("supply")) || SearchText.Contains(TEXT("cargo")) || SearchText.Contains(TEXT("equipment")) || SearchText.Contains(TEXT("medicine")) || SearchText.Contains(TEXT("canister")))
    {
        SetStaticModel(
            PrimaryMesh,
            TEXT("/Game/Missions/Assets/SupplyCrate/military_supply_crate/StaticMeshes/military_supply_crate.military_supply_crate"),
            FVector(2.8f),
            FVector(0.0f, 0.0f, 35.0f));
    }
    else if (SearchText.Contains(TEXT("fire")) || SearchText.Contains(TEXT("suppressant")))
    {
        bFireScene = true;
        bUseNiagaraFire =
            GroundFireFlameA->GetAsset() &&
            GroundFireFlameB->GetAsset() &&
            GroundFireSmoke->GetAsset() &&
            GroundFireEmbers->GetAsset();
        if (bUseNiagaraFire)
        {
            PrimaryMesh->SetVisibility(false, true);
            SecondaryMesh->SetVisibility(false, true);
            TertiaryMesh->SetVisibility(false, true);

            GroundFireFlameA->SetRelativeLocation(FVector(-150.0f, -65.0f, 35.0f));
            GroundFireFlameA->SetRelativeRotation(FRotator(0.0f, -18.0f, 0.0f));
            GroundFireFlameA->SetRelativeScale3D(FVector(1.35f, 1.10f, 1.25f));
            GroundFireFlameB->SetRelativeLocation(FVector(175.0f, 95.0f, 20.0f));
            GroundFireFlameB->SetRelativeRotation(FRotator(0.0f, 37.0f, 0.0f));
            GroundFireFlameB->SetRelativeScale3D(FVector(0.92f, 0.82f, 1.05f));
            GroundFireSmoke->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
            GroundFireSmoke->SetRelativeScale3D(FVector(1.55f));
            GroundFireEmbers->SetRelativeLocation(FVector(20.0f, 0.0f, 120.0f));
            GroundFireEmbers->SetRelativeScale3D(FVector(0.85f));

            for (UNiagaraComponent* FireComponent :
                { GroundFireFlameA.Get(), GroundFireFlameB.Get(), GroundFireSmoke.Get(), GroundFireEmbers.Get() })
            {
                FireComponent->SetVisibility(true, true);
                FireComponent->Activate(true);
            }
            GroundFireLight->SetRelativeLocation(FVector(0.0f, 0.0f, 170.0f));
            GroundFireLight->SetVisibility(true, true);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_MISSION_FIRE|site=%s|layers=NIAGARA_FLAME_A,NIAGARA_FLAME_B,WIND_SMOKE,EMBERS,LOCAL_LIGHT|state=ACTIVE"),
                *Objective.Site);
        }
        else
        {
            SetStaticModel(PrimaryMesh, RotorlineMissionVisuals::SpherePath, FVector(1.25f, 1.25f, 4.8f), FVector(-120.0f, -40.0f, 330.0f));
            SetStaticModel(SecondaryMesh, RotorlineMissionVisuals::SpherePath, FVector(0.95f, 0.95f, 3.8f), FVector(150.0f, 75.0f, 260.0f));
            SetStaticModel(TertiaryMesh, RotorlineMissionVisuals::SpherePath, FVector(2.0f, 2.0f, 2.5f), FVector(20.0f, 0.0f, 820.0f));
            if (UMaterialInterface* HotMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Missions/Presentation/M_ExplosionHot.M_ExplosionHot")))
            {
                PrimaryMesh->SetMaterial(0, HotMaterial);
            }
            if (UMaterialInterface* FlameMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Missions/Presentation/M_RocketFlameGlow.M_RocketFlameGlow")))
            {
                SecondaryMesh->SetMaterial(0, FlameMaterial);
            }
            if (UMaterialInterface* SmokeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Missions/Presentation/M_RocketSmoke.M_RocketSmoke")))
            {
                TertiaryMesh->SetMaterial(0, SmokeMaterial);
                FireSmokeMaterial = TertiaryMesh->CreateDynamicMaterialInstance(0);
                if (FireSmokeMaterial)
                {
                    FireSmokeMaterial->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.16f, 0.13f, 0.10f, 1.0f));
                    FireSmokeMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.42f);
                }
            }
            UE_LOG(LogTemp, Warning,
                TEXT("ROTORLINE_MISSION_FIRE|site=%s|layers=LEGACY_FALLBACK|state=ACTIVE"),
                *Objective.Site);
        }
        BeaconLight->SetLightColor(FLinearColor(1.0f, 0.24f, 0.015f));
        BeaconLight->SetIntensity(52000.0f);
    }

    USoundBase* FireSound = nullptr;
    const ARotorlineOperationsPlayerController* OperationsController =
        GetWorld() ? Cast<ARotorlineOperationsPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
    const float WeaponMix = OperationsController
        ? OperationsController->GetEffectiveAudioVolume(ERotorlineAudioChannel::WeaponsExplosions) : 1.0f;
    const float EngineMix = OperationsController
        ? OperationsController->GetEffectiveAudioVolume(ERotorlineAudioChannel::Engine) : 1.0f;
    if (ThreatType == ERotorlineThreatType::Flak)
    {
        FireSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Weapons/SFX_Enemy_Cannon.SFX_Enemy_Cannon"));
        FireAudioBaseVolume = 0.48f;
    }
    else if (ThreatType == ERotorlineThreatType::Tank)
    {
        FireSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Combat/SFX_TankCannon.SFX_TankCannon"));
        FireAudioBaseVolume = 0.55f;
        EngineAudioBaseVolume = 0.13f;
        EngineAudio->SetSound(LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Combat/AMB_TankDriving.AMB_TankDriving")));
        EngineAudio->SetVolumeMultiplier(EngineAudioBaseVolume * EngineMix);
        EngineAudio->Play();
    }
    else if (ThreatType == ERotorlineThreatType::RocketArtillery)
    {
        const bool bMortar = TargetId.Contains(TEXT("mortar"), ESearchCase::IgnoreCase);
        FireSound = LoadObject<USoundBase>(nullptr, bMortar
            ? TEXT("/Game/Audio/Weapons/SFX_Enemy_Cannon.SFX_Enemy_Cannon")
            : TEXT("/Game/Audio/Combat/SFX_MissileFlyby.SFX_MissileFlyby"));
        FireAudioBaseVolume = bMortar ? 0.36f : 0.46f;
    }
    else if (ThreatType == ERotorlineThreatType::MachineGunship)
    {
        FireSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Vehicles/AH64/SFX_AH64_30mm_Autocannon.SFX_AH64_30mm_Autocannon"));
        FireAudioBaseVolume = 0.46f;
    }
    else if (ThreatType == ERotorlineThreatType::RadarMissile || ThreatType == ERotorlineThreatType::RocketGunship)
    {
        FireSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Combat/SFX_MissileFlyby.SFX_MissileFlyby"));
        FireAudioBaseVolume = 0.50f;
    }
    FireAudio->SetVolumeMultiplier(FireAudioBaseVolume * WeaponMix);
    if (FireSound) FireAudio->SetSound(FireSound);

    if (IsAircraftThreat())
    {
        SetActorLocation(GetActorLocation() + FVector(0.0f, 0.0f, 5800.0f));
        PatrolCenter = GetActorLocation();
        PatrolPhase = FMath::Fmod(FMath::Abs(GetActorLocation().X + GetActorLocation().Y) * 0.000013f, 2.0f * PI);
        switch (EnemyAirframe)
        {
        case ERotorlineEnemyAirframe::MD500:
            AircraftAttackSpeed = 4500.0f;
            AircraftBreakSpeed = 5100.0f;
            AircraftAcceleration = 700.0f;
            AircraftTurnRate = 34.0f;
            AircraftRotorRate = 3240.0f;
            AircraftManeuverTime = 7.0f;
            break;
        case ERotorlineEnemyAirframe::Apache:
            AircraftAttackSpeed = 4100.0f;
            AircraftBreakSpeed = 4700.0f;
            AircraftAcceleration = 620.0f;
            AircraftTurnRate = 30.0f;
            AircraftRotorRate = 2880.0f;
            AircraftManeuverTime = 8.0f;
            break;
        case ERotorlineEnemyAirframe::Hind:
            AircraftAttackSpeed = 3700.0f;
            AircraftBreakSpeed = 4300.0f;
            AircraftAcceleration = 540.0f;
            AircraftTurnRate = 25.0f;
            AircraftRotorRate = 2520.0f;
            AircraftManeuverTime = 9.0f;
            break;
        default:
            break;
        }

        // Aircraft should survive a deliberate firing pass without becoming
        // bullet sponges. Apply the same modest durability lift to every
        // airframe, including mission-specific variants configured above.
        TargetHealth *= 1.12f;

        // Spawn already pointed into the fight instead of drifting away on
        // world +X while waiting through a 90-180 degree initial turn.
        if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        {
            if (const APawn* PlayerPawn = PC->GetPawn())
            {
                const FRotator InitialLook = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), PlayerPawn->GetActorLocation());
                SetActorRotation(FRotator(0.0f, InitialLook.Yaw, 0.0f));
            }
        }
        AircraftForwardSpeed = AircraftAttackSpeed * 0.72f;
        AircraftVelocity = GetActorForwardVector() * AircraftForwardSpeed;
        MarkerRing->SetVisibility(false, true);
        MarkerPulseRing->SetVisibility(false, true);
        MarkerCenter->SetVisibility(false, true);
        MarkerHLeft->SetVisibility(false, true);
        MarkerHRight->SetVisibility(false, true);
        MarkerHCross->SetVisibility(false, true);
        ObjectiveLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 1200.0f));
        EngineAudio->SetSound(LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Vehicles/MD500/SC_MD500_EngineInFlight_Loop.SC_MD500_EngineInFlight_Loop")));
        EngineAudioBaseVolume = 0.34f;
        EngineAudio->SetVolumeMultiplier(EngineAudioBaseVolume * EngineMix);
        EngineAudio->Play();
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_ENEMY_FLIGHT|CONFIG|airframe=%s|attack_kmh=%.1f|break_kmh=%.1f|accel_mps2=%.1f|turn_dps=%.1f|visual_forward=MODEL_POSITIVE_Y_TO_ACTOR_POSITIVE_X"),
            GetAirframeName(), AircraftAttackSpeed * 0.036f, AircraftBreakSpeed * 0.036f,
            AircraftAcceleration / 100.0f, AircraftTurnRate);
    }
    else
    {
        PatrolCenter = GetActorLocation();
    }

    EnsureThreatVisualReady();
    TargetMaxHealth = FMath::Max(1.0f, TargetHealth);
    if (IsAircraftThreat())
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COMBAT_LOOP_TEST|ENEMY_CONFIG|target=%s|airframe=%s|health=%.1f|hit_radius_cm=%.0f"),
            *TargetId, GetAirframeName(), TargetMaxHealth, GetProjectileHitRadius());
    }
}

const TCHAR* ARotorlineMissionObjectiveActor::GetAirframeName() const
{
    switch (EnemyAirframe)
    {
    case ERotorlineEnemyAirframe::MD500: return TEXT("MD500");
    case ERotorlineEnemyAirframe::Apache: return TEXT("APACHE");
    case ERotorlineEnemyAirframe::Hind: return TEXT("HIND");
    default: return TEXT("NONE");
    }
}

bool ARotorlineMissionObjectiveActor::TraceAircraftGround(const FVector& Location, float& OutGroundHeight) const
{
    if (!GetWorld()) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RotorlineEnemyAircraftGround), false, this);
    TArray<FHitResult> Hits;
    const FVector Start(Location.X, Location.Y, Location.Z + 12000.0f);
    const FVector End(Location.X, Location.Y, Location.Z - 30000.0f);
    if (!GetWorld()->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, Params) || Hits.IsEmpty()) return false;
    OutGroundHeight = Hits[0].ImpactPoint.Z;
    return true;
}

bool ARotorlineMissionObjectiveActor::TraceTerrainSurface(
    const FVector& Location,
    float& OutGroundHeight,
    FVector& OutGroundNormal) const
{
    FRotorlineGroundingProfile Profile = URotorlineGroundingLibrary::MakeProfile(
        ERotorlineGroundingMode::LinearPoint, TEXT("GroundVehicleSurface"));
    Profile.bAllowPreparedGround = false;
    Profile.bRejectObstructionsAboveGround = true;
    Profile.bCheckCollisionPenetration = false;
    FRotorlineGroundingResult Result;
    if (!URotorlineGroundingLibrary::SolveGroundContact(
        const_cast<ARotorlineMissionObjectiveActor*>(this), Location,
        FVector2D::ZeroVector, const_cast<ARotorlineMissionObjectiveActor*>(this),
        Profile, Result)) return false;
    OutGroundHeight = Result.ContactPoint.Z;
    OutGroundNormal = Result.SurfaceNormal;
    return true;
}

void ARotorlineMissionObjectiveActor::AnimateAircraftRotors(float DeltaSeconds, float SpinScale)
{
    const float MainRotorRadians = FMath::DegreesToRadians(
        AircraftRotorRate * FMath::Max(0.0f, SpinScale) * DeltaSeconds);
    if (EnemyAirframe == ERotorlineEnemyAirframe::MD500)
    {
        // Imported skeletal animation supplies blade flex; component rotation
        // guarantees readable disc motion even when animation sampling aliases.
        EnemyMainRotor->AddLocalRotation(FQuat(FVector::UpVector, MainRotorRadians));
        EnemyTailRotor->AddLocalRotation(FQuat(FVector::ForwardVector, MainRotorRadians * RotorlineEnemyRotorVisuals::TailRateMultiplier));
    }
    else if (EnemyAirframe == ERotorlineEnemyAirframe::Apache)
    {
        ApacheMainRotorPivot->AddLocalRotation(FQuat(FVector::UpVector, MainRotorRadians));
        ApacheTailRotorPivot->AddLocalRotation(FQuat(FVector::ForwardVector, MainRotorRadians * RotorlineEnemyRotorVisuals::TailRateMultiplier));
    }
    else if (EnemyAirframe == ERotorlineEnemyAirframe::Hind)
    {
        SecondaryMesh->AddLocalRotation(FQuat(FVector::UpVector, MainRotorRadians));
        // The supplied processed Hind body contains its tail rotor baked into
        // the fuselage mesh, so only the independently supplied main rotor can
        // animate until a separated tail-rotor asset is imported.
    }
}

bool ARotorlineMissionObjectiveActor::HasWeaponSolution(const FVector& TargetLocation) const
{
    if (bDestroyedTarget || !bThreatVisualReady || !GetWorld()) return false;

    // Aiming alignment is not a firing solution when solid cover is between
    // the weapon and player. Trace from the actual muzzle and only accept a
    // blocking hit when it is effectively at the aircraft endpoint.
    const FVector MuzzleLocation = GetMuzzleLocation();
    const bool bMortar = ThreatType == ERotorlineThreatType::RocketArtillery &&
        TargetId.Contains(TEXT("mortar"), ESearchCase::IgnoreCase);
    if (bMortar)
    {
        // Indirect fire deliberately ignores intervening terrain and buildings;
        // range and cadence are enforced by the mission combat director.
        return FVector::Dist2D(MuzzleLocation, TargetLocation) <= 90000.0f;
    }
    const bool bHawkLauncher = ThreatType == ERotorlineThreatType::RadarMissile &&
        TargetId.Contains(TEXT("hawk"), ESearchCase::IgnoreCase);
    // A HAWK first travels straight down the launcher rail. Validate that real
    // boosted segment before checking the guided leg; there is no artificial
    // vertical teleport or scripted loft in this firing solution.
    const FVector RailClearLocation = bHawkLauncher
        ? MuzzleLocation + GetWeaponAimDirection() * RotorlineHawkLauncher::StraightBoostDistanceCm
        : MuzzleLocation;
    FHitResult CoverHit;
    FCollisionQueryParams CoverParams(SCENE_QUERY_STAT(RotorlineEnemyLineOfSight), false, this);
    CoverParams.AddIgnoredActor(this);
    FHitResult RailHit;
    const bool bRailBlocked = bHawkLauncher && GetWorld()->LineTraceSingleByChannel(
        RailHit,
        MuzzleLocation,
        RailClearLocation,
        ECC_Visibility,
        CoverParams);
    const bool bCoverBlocked = GetWorld()->LineTraceSingleByChannel(
        CoverHit,
        RailClearLocation,
        TargetLocation,
        ECC_Visibility,
        CoverParams);
    // A trace into a large helicopter can strike its collision hull more than
    // five metres before the actor origin. That is the target, not intervening
    // cover. This matters for the larger support airframes such as the Dhruv.
    const AActor* CoverActor = bCoverBlocked ? CoverHit.GetActor() : nullptr;
    const bool bHitTargetActor = CoverActor &&
        FVector::DistSquared(CoverActor->GetActorLocation(), TargetLocation) <= FMath::Square(1500.0f);
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineHawkRidgeTest")) &&
        TargetId.Contains(TEXT("hawk"), ESearchCase::IgnoreCase))
    {
        static double LastHawkSolutionAuditTime = -1000.0;
        const double Now = GetWorld()->GetTimeSeconds();
        if (Now - LastHawkSolutionAuditTime >= 1.0)
        {
            LastHawkSolutionAuditTime = Now;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_HAWK_RIDGE_TEST|WEAPON_SOLUTION|site=%s|muzzle=%.0f,%.0f,%.0f|target=%.0f,%.0f,%.0f|rail_pitch=%.1f|rail_blocked=%d|guided_blocked=%d|hit=%s|endpoint_error_m=%.1f|hit_target=%d"),
                *GetTargetLabel(),
                MuzzleLocation.X, MuzzleLocation.Y, MuzzleLocation.Z,
                TargetLocation.X, TargetLocation.Y, TargetLocation.Z,
                GetWeaponAimDirection().Rotation().Pitch,
                bRailBlocked ? 1 : 0,
                bCoverBlocked ? 1 : 0,
                CoverActor ? *CoverActor->GetActorNameOrLabel() : TEXT("NONE"),
                bCoverBlocked ? FVector::Dist(CoverHit.ImpactPoint, TargetLocation) / 100.0f : 0.0f,
                bHitTargetActor ? 1 : 0);
        }
    }
    if (bRailBlocked)
    {
        return false;
    }
    if (bCoverBlocked && !bHitTargetActor &&
        FVector::DistSquared(CoverHit.ImpactPoint, TargetLocation) > FMath::Square(500.0f))
    {
        return false;
    }
    if (ThreatType == ERotorlineThreatType::Tank || ThreatType == ERotorlineThreatType::Flak ||
        ThreatType == ERotorlineThreatType::RocketArtillery)
    {
        const FVector ToTarget = (TargetLocation - MuzzleLocation).GetSafeNormal();
        const float AllowedErrorDegrees = ThreatType == ERotorlineThreatType::Tank ? 6.5f :
            (ThreatType == ERotorlineThreatType::RocketArtillery ? 12.0f : 8.5f);
        return FVector::DotProduct(GetWeaponAimDirection(), ToTarget) >=
            FMath::Cos(FMath::DegreesToRadians(AllowedErrorDegrees));
    }
    if (bHawkLauncher)
    {
        const FVector FlatAim = FVector(GetWeaponAimDirection().X, GetWeaponAimDirection().Y, 0.0f).GetSafeNormal();
        const FVector FlatTarget = FVector(TargetLocation.X - MuzzleLocation.X, TargetLocation.Y - MuzzleLocation.Y, 0.0f).GetSafeNormal();
        return FVector::DotProduct(FlatAim, FlatTarget) >= FMath::Cos(FMath::DegreesToRadians(5.0f));
    }
    if (!IsAircraftThreat()) return true;
    if (bDestroyedTarget || bAircraftDying || !bAircraftAttackRun) return false;
    const FVector ToTarget = (TargetLocation - MuzzleLocation).GetSafeNormal();
    const float RequiredForwardDot = EnemyAirframe == ERotorlineEnemyAirframe::MD500 ? 0.45f : 0.52f;
    return FVector::DotProduct(GetActorForwardVector(), ToTarget) >= RequiredForwardDot;
}

bool ARotorlineMissionObjectiveActor::HasCurrentWeaponSolution() const
{
    if (CurrentCombatTargetLocation.IsNearlyZero()) return false;
    return HasWeaponSolution(CurrentCombatTargetLocation);
}

void ARotorlineMissionObjectiveActor::UpdateGroundCombat(float DeltaSeconds, const APawn* PlayerPawn)
{
    if (!PlayerPawn || !GetWorld() || !bThreatVisualReady ||
        (ThreatType != ERotorlineThreatType::Tank && ThreatType != ERotorlineThreatType::Flak &&
            ThreatType != ERotorlineThreatType::RocketArtillery &&
            !(ThreatType == ERotorlineThreatType::RadarMissile &&
                TargetId.Contains(TEXT("hawk"), ESearchCase::IgnoreCase))))
    {
        return;
    }

    if (ThreatType == ERotorlineThreatType::Tank)
    {
        // Roughly 23-30 km/h around a broad scouting loop instead of circling a
        // single marker. All network spawn points sit outside the airfield
        // sanctuary, so this loop cannot wander into the player start.
        const float CandidatePhase = PatrolPhase + PatrolDirection * DeltaSeconds * 0.108f;
        const float LookAheadPhase = PatrolPhase + PatrolDirection * 0.050f;
        // Offset cosine by its phase-zero value so the route begins at the
        // authored spawn instead of teleporting one full patrol radius on the
        // first frame.
        FVector DriveLocation = PatrolCenter + FVector(
            (FMath::Cos(CandidatePhase) - 1.0f) * 11200.0f,
            FMath::Sin(CandidatePhase) * 6400.0f,
            0.0f);
        const FVector LookAheadLocation = PatrolCenter + FVector(
            (FMath::Cos(LookAheadPhase) - 1.0f) * 11200.0f,
            FMath::Sin(LookAheadPhase) * 6400.0f,
            0.0f);
        float CurrentGroundHeight = 0.0f;
        float CandidateGroundHeight = 0.0f;
        float LookAheadGroundHeight = 0.0f;
        FVector CurrentGroundNormal = FVector::UpVector;
        FVector CandidateGroundNormal = FVector::UpVector;
        FVector LookAheadGroundNormal = FVector::UpVector;
        const bool bCurrentGround = TraceTerrainSurface(GetActorLocation(), CurrentGroundHeight, CurrentGroundNormal);
        const bool bCandidateGround = TraceTerrainSurface(DriveLocation, CandidateGroundHeight, CandidateGroundNormal);
        const bool bLookAheadGround = TraceTerrainSurface(LookAheadLocation, LookAheadGroundHeight, LookAheadGroundNormal);
        const float LookAheadDistance = FVector::Dist2D(GetActorLocation(), LookAheadLocation);
        const float LookAheadGrade = LookAheadDistance > 1.0f
            ? FMath::Abs(LookAheadGroundHeight - CurrentGroundHeight) / LookAheadDistance
            : 0.0f;
        constexpr float MaximumTankGrade = 0.325f; // approximately 18 degrees
        const bool bUnsafeSlope = !bCurrentGround || !bCandidateGround || !bLookAheadGround ||
            LookAheadGrade > MaximumTankGrade ||
            CandidateGroundNormal.Z < 0.927f || LookAheadGroundNormal.Z < 0.927f;
        if (bUnsafeSlope)
        {
            const float Now = GetWorld()->GetTimeSeconds();
            if (Now - LastSlopeRejectTime >= 0.75f)
            {
                LastSlopeRejectTime = Now;
                PatrolDirection *= -1.0f;
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_GROUND_AI|type=TANK|state=SLOPE_REJECT|grade=%.3f|normal_z=%.3f|action=REVERSE"),
                    LookAheadGrade,
                    FMath::Min(CandidateGroundNormal.Z, LookAheadGroundNormal.Z));
            }
        }
        else
        {
            PatrolPhase = CandidatePhase;
            DriveLocation.Z = CandidateGroundHeight + 8.0f;
            SetActorLocation(DriveLocation);
        }
        const FVector Tangent(
            -FMath::Sin(PatrolPhase) * PatrolDirection,
            0.57f * FMath::Cos(PatrolPhase) * PatrolDirection,
            0.0f);
        SetActorRotation(FMath::RInterpConstantTo(
            GetActorRotation(),
            FRotator(0.0f, Tangent.Rotation().Yaw, 0.0f),
            DeltaSeconds,
            24.0f));
    }

    const ARotorlineHelicopterPawn* PlayerHelicopter = Cast<ARotorlineHelicopterPawn>(PlayerPawn);
    const bool bPlayerStealthed = PlayerHelicopter && PlayerHelicopter->IsBell222StealthActive();
    const FVector PlayerVelocity = PlayerHelicopter ? PlayerHelicopter->GetFlightVelocity() : PlayerPawn->GetVelocity();
    FVector SelectedTargetLocation = PlayerPawn->GetActorLocation();
    FVector SelectedTargetVelocity = PlayerVelocity;

    if (bPlayerStealthed)
    {
        CurrentCombatTargetLocation = FVector::ZeroVector;
        CurrentCombatTargetVelocity = FVector::ZeroVector;
        GroundTrackingErrorDegrees = 180.0f;
        return;
    }

    CurrentCombatTargetLocation = SelectedTargetLocation;
    CurrentCombatTargetVelocity = SelectedTargetVelocity;
    const float TargetDistanceCm = FVector::Dist(GetMuzzleLocation(), SelectedTargetLocation);
    const bool bHawkLauncher = ThreatType == ERotorlineThreatType::RadarMissile &&
        TargetId.Contains(TEXT("hawk"), ESearchCase::IgnoreCase);
    const bool bMortar = ThreatType == ERotorlineThreatType::RocketArtillery &&
        TargetId.Contains(TEXT("mortar"), ESearchCase::IgnoreCase);
    const float ProjectileSpeed = ThreatType == ERotorlineThreatType::Tank ? 11800.0f :
        (bMortar ? 10500.0f : (ThreatType == ERotorlineThreatType::RocketArtillery ? 19000.0f :
            (bHawkLauncher ? RotorlineHawkLauncher::MissileSpeedCmPerSecond : 12500.0f)));
    const float RawLeadSeconds = TargetDistanceCm / ProjectileSpeed;
    const float LeadScale = ThreatType == ERotorlineThreatType::Tank ? 0.58f :
        (bMortar ? 0.35f : (ThreatType == ERotorlineThreatType::RocketArtillery ? 0.22f : (bHawkLauncher ? 0.18f : 0.48f)));
    const float MaximumLead = ThreatType == ERotorlineThreatType::Tank ? 1.65f :
        (bMortar ? 1.25f : (ThreatType == ERotorlineThreatType::RocketArtillery ? 0.75f : (bHawkLauncher ? 0.65f : 1.10f)));
    const float LeadSeconds = FMath::Clamp(
        RawLeadSeconds * LeadScale,
        0.18f,
        MaximumLead);
    FVector DesiredAimPoint = SelectedTargetLocation + SelectedTargetVelocity * LeadSeconds;

    // Repeatable low-frequency aim drift keeps batteries dangerous without
    // making them perfect snipers. A pilot who changes vector after the muzzle
    // flash can evade the non-homing shell.
    const float ErrorAmplitude = ThreatType == ERotorlineThreatType::Tank ? 1.35f :
        (bMortar ? 3.4f : (ThreatType == ERotorlineThreatType::RocketArtillery ? 0.75f : (bHawkLauncher ? 0.0f : 2.15f)));
    const float YawError = FMath::Sin(PulseTime * 0.73f + PatrolPhase * 1.7f) * ErrorAmplitude;
    const float PitchError = FMath::Sin(PulseTime * 0.51f + PatrolPhase * 0.9f + 1.1f) * ErrorAmplitude * 0.55f;
    FRotator DesiredAim = UKismetMathLibrary::FindLookAtRotation(GetMuzzleLocation(), DesiredAimPoint);
    DesiredAim.Yaw += YawError;
    DesiredAim.Pitch += PitchError;
    if (bHawkLauncher)
    {
        DesiredAim.Pitch = RotorlineHawkLauncher::RailElevationDegrees;
    }
    else if (bMortar)
    {
        DesiredAim.Pitch = 58.0f;
    }
    else
    {
        const float MinimumPitch = ThreatType == ERotorlineThreatType::RocketArtillery ? 5.0f : -6.0f;
        DesiredAim.Pitch = FMath::Clamp(DesiredAim.Pitch, MinimumPitch,
            ThreatType == ERotorlineThreatType::Tank ? 34.0f : 62.0f);
    }

    const float TrackingRateDegreesPerSecond = ThreatType == ERotorlineThreatType::Tank ? 38.0f :
        (bMortar ? 32.0f : (ThreatType == ERotorlineThreatType::RocketArtillery ? 20.0f : (bHawkLauncher ? 28.0f : 54.0f)));
    GroundWeaponAimRotation = FMath::RInterpConstantTo(
        GroundWeaponAimRotation,
        DesiredAim,
        DeltaSeconds,
        TrackingRateDegreesPerSecond);
    GroundTrackingErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
        FVector::DotProduct(GroundWeaponAimRotation.Vector(), (DesiredAimPoint - GetMuzzleLocation()).GetSafeNormal()),
        -1.0f,
        1.0f)));

    if (ThreatType == ERotorlineThreatType::Tank)
    {
        // Both Challenger parts use source +Y as forward. Build rotations from
        // that axis directly so the barrel elevates as well as yaws.
        const FVector FlatAim = FVector(GetWeaponAimDirection().X, GetWeaponAimDirection().Y, 0.0f).GetSafeNormal();
        SecondaryMesh->SetWorldRotation(FRotationMatrix::MakeFromYZ(FlatAim, FVector::UpVector).Rotator());
        TertiaryMesh->SetWorldRotation(FRotationMatrix::MakeFromYZ(GetWeaponAimDirection(), FVector::UpVector).Rotator());
    }
    else if (ThreatType == ERotorlineThreatType::Flak)
    {
        // The inspected Object_2 gun assembly fires along source +Y. The base
        // stays fixed while the dedicated trunnion pivot yaws and elevates the
        // entire barrel/rack assembly toward the weapon solution.
        const FVector FlakSourceBarrelAxis = FVector::RightVector; // imported local +Y
        (void)FlakSourceBarrelAxis;
        FlakGunPivot->SetWorldRotation(
            FRotationMatrix::MakeFromYZ(GetWeaponAimDirection(), FVector::UpVector).Rotator());
    }
    else if (ThreatType == ERotorlineThreatType::RocketArtillery)
    {
        if (bMortar)
        {
            // Engine cylinders point along local +Z; slew the tube without
            // rotating its base plate or bipod off the terrain.
            SecondaryMesh->SetWorldRotation(
                FRotationMatrix::MakeFromZX(GetWeaponAimDirection(), FVector::UpVector).Rotator());
        }
        else
        {
            // The launcher pod's authored firing axis is source +Y. Track the
            // aircraft with the pod only; the six-wheel truck remains level.
            HimarsLauncherPivot->SetWorldRotation(
                FRotationMatrix::MakeFromYZ(GetWeaponAimDirection(), FVector::UpVector).Rotator());
        }
    }
    else if (bHawkLauncher)
    {
        // The supplied missiles point along local +Y and already carry their
        // physical 11-degree elevation in the mesh. Slew only the launcher
        // azimuth so the visible rails and gameplay launch vector agree.
        const FVector FlatAim = FVector(GetWeaponAimDirection().X, GetWeaponAimDirection().Y, 0.0f).GetSafeNormal();
        SecondaryMesh->SetWorldRotation(
            FRotationMatrix::MakeFromYZ(FlatAim, FVector::UpVector).Rotator());
    }

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastGroundCombatAuditLogTime >= 3.0f)
    {
        LastGroundCombatAuditLogTime = Now;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_GROUND_AI|type=%s|target=PLAYER|tracking_dps=%.0f|error_deg=%.1f|range_m=%.0f|solution=%d|patrol_kmh=%.1f"),
            ThreatType == ERotorlineThreatType::Tank ? TEXT("TANK") :
                (ThreatType == ERotorlineThreatType::RocketArtillery ? (bMortar ? TEXT("MORTAR") : TEXT("HIMARS")) :
                    (bHawkLauncher ? TEXT("HAWK") : TEXT("FLAK"))),
            TrackingRateDegreesPerSecond,
            GroundTrackingErrorDegrees,
            TargetDistanceCm / 100.0f,
            HasCurrentWeaponSolution() ? 1 : 0,
            ThreatType == ERotorlineThreatType::Tank ? 26.0f : 0.0f);
    }
}

void ARotorlineMissionObjectiveActor::UpdateAircraftFlight(float DeltaSeconds, const APawn* PlayerPawn)
{
    if (!PlayerPawn || !GetWorld()) return;
    AnimateAircraftRotors(DeltaSeconds);

    AircraftManeuverTime -= DeltaSeconds;
    const FVector PlayerLocation = PlayerPawn->GetActorLocation();
    const ARotorlineHelicopterPawn* PlayerHelicopter = Cast<ARotorlineHelicopterPawn>(PlayerPawn);
    const bool bPlayerStealthed = PlayerHelicopter && PlayerHelicopter->IsBell222StealthActive();
    const FVector PlayerVelocity = PlayerHelicopter ? PlayerHelicopter->GetFlightVelocity() : PlayerPawn->GetVelocity();
    const float PlayerDistance = FVector::Dist(PlayerLocation, GetActorLocation());
    // The light MD-500 turns more quickly than the heavier gunships, so begin
    // its pass break earlier instead of letting it repeatedly press the hard
    // separation boundary around the player.
    const float PassDistance = EnemyAirframe == ERotorlineEnemyAirframe::MD500 ? 17000.0f : 16000.0f;
    const double Now = GetWorld()->GetTimeSeconds();

    // Separation avoidance is the primary defense. If an aircraft still gets
    // inside the physical rotor/fuselage envelope, treat it as a real mid-air
    // collision instead of a harmless overlap or partial projectile hit.
    if (PlayerDistance < RotorlineEnemyFlightSafety::CollisionEnvelopeCm &&
        Now - LastAircraftCollisionTime > 1.25)
    {
        LastAircraftCollisionTime = Now;
        const float RelativeImpactSpeed = (AircraftVelocity - PlayerVelocity).Size();
        if (ARotorlineHelicopterPawn* MutablePlayer =
            Cast<ARotorlineHelicopterPawn>(const_cast<APawn*>(PlayerPawn)))
        {
            MutablePlayer->HandleEnemyAircraftCollision(
                FString(GetAirframeName()),
                RelativeImpactSpeed);
        }
        float AppliedEnemyDamage = 0.0f;
        ApplyCombatDamage(10000.0f, TEXT("AIRCRAFT_COLLISION"), AppliedEnemyDamage);
        UE_LOG(LogTemp, Warning,
            TEXT("ROTORLINE_AIRCRAFT_COLLISION|airframe=%s|relative_mps=%.1f|result=FATAL_MIDAIR"),
            GetAirframeName(), RelativeImpactSpeed / 100.0f);
        return;
    }

    const FVector RelativePosition = GetActorLocation() - PlayerLocation;
    const FVector RelativeVelocity = AircraftVelocity - PlayerVelocity;
    const float RelativeSpeedSquared = RelativeVelocity.SizeSquared();
    const float ClosestApproachTime = RelativeSpeedSquared > 1.0f
        ? FMath::Clamp(
            -FVector::DotProduct(RelativePosition, RelativeVelocity) / RelativeSpeedSquared,
            0.0f,
            RotorlineEnemyFlightSafety::PredictionSeconds)
        : 0.0f;
    const float PredictedSeparation = (RelativePosition + RelativeVelocity * ClosestApproachTime).Size();
    const float AvoidanceStartDistance = EnemyAirframe == ERotorlineEnemyAirframe::MD500
        ? 22000.0f
        : RotorlineEnemyFlightSafety::AvoidanceStartCm;
    const float PredictedConflictDistance = EnemyAirframe == ERotorlineEnemyAirframe::MD500
        ? 13000.0f
        : RotorlineEnemyFlightSafety::PredictedConflictCm;
    const bool bPredictedSeparationConflict =
        PlayerDistance < AvoidanceStartDistance &&
        PredictedSeparation < PredictedConflictDistance;

    if (bPlayerStealthed)
    {
        if (bAircraftAttackRun)
        {
            const FVector EscapeDirection = (GetActorLocation() - PlayerLocation).GetSafeNormal2D();
            AircraftBreakawayWaypoint = GetActorLocation() +
                (EscapeDirection.IsNearlyZero() ? GetActorForwardVector() : EscapeDirection) * 16000.0f +
                FVector::UpVector * 3000.0f;
        }
        bAircraftAttackRun = false;
        AircraftManeuverTime = FMath::Max(AircraftManeuverTime, 2.0f);
    }
    else if (bPredictedSeparationConflict)
    {
        FVector EscapeDirection = RelativePosition.GetSafeNormal2D();
        if (EscapeDirection.IsNearlyZero())
        {
            EscapeDirection = GetActorRightVector().GetSafeNormal2D() * static_cast<float>(AircraftPassSide);
        }
        const FVector EscapeRight = FVector::CrossProduct(FVector::UpVector, EscapeDirection).GetSafeNormal();
        AircraftBreakawayWaypoint = GetActorLocation() +
            EscapeDirection * 18000.0f +
            EscapeRight * (AircraftPassSide * 6500.0f) +
            FVector::UpVector * 5500.0f;
        bAircraftAttackRun = false;
        AircraftManeuverTime = FMath::Max(AircraftManeuverTime, 7.0f);
        if (!bAircraftSeparationAvoidanceActive)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_ENEMY_FLIGHT|SEPARATION|state=AVOID|airframe=%s|distance_m=%.1f|predicted_m=%.1f|minimum_m=%.1f"),
                GetAirframeName(),
                PlayerDistance / 100.0f,
                PredictedSeparation / 100.0f,
                RotorlineEnemyFlightSafety::HardSeparationCm / 100.0f);
        }
        bAircraftSeparationAvoidanceActive = true;
    }
    else if (bAircraftAttackRun && PlayerDistance < PassDistance && AircraftManeuverTime < 6.0f)
    {
        bAircraftAttackRun = false;
        AircraftManeuverTime = EnemyAirframe == ERotorlineEnemyAirframe::MD500 ? 4.0f : 5.0f;
        const FVector FlatForward = FRotator(0.0f, GetActorRotation().Yaw, 0.0f).Vector();
        const FVector FlatRight = FRotationMatrix(FRotator(0.0f, GetActorRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::Y);
        AircraftBreakawayWaypoint = GetActorLocation() + FlatForward * 12000.0f + FlatRight * (AircraftPassSide * 6000.0f) + FVector::UpVector * 3000.0f;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_ENEMY_FLIGHT|PASS|airframe=%s|state=BREAK|distance_m=%.1f|side=%d"),
            GetAirframeName(), PlayerDistance / 100.0f, AircraftPassSide);
    }
    else if (!bAircraftAttackRun &&
        AircraftManeuverTime <= 0.0f &&
        PlayerDistance > AvoidanceStartDistance * 1.25f)
    {
        bAircraftAttackRun = true;
        AircraftPassSide *= -1;
        AircraftManeuverTime = EnemyAirframe == ERotorlineEnemyAirframe::MD500 ? 5.0f : 6.5f;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_ENEMY_FLIGHT|PASS|airframe=%s|state=ATTACK|distance_m=%.1f|side=%d"),
            GetAirframeName(), PlayerDistance / 100.0f, AircraftPassSide);
    }
    if (bAircraftSeparationAvoidanceActive && PlayerDistance > AvoidanceStartDistance * 1.35f)
    {
        bAircraftSeparationAvoidanceActive = false;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_ENEMY_FLIGHT|SEPARATION|state=CLEAR|airframe=%s|distance_m=%.1f"),
            GetAirframeName(), PlayerDistance / 100.0f);
    }

    FVector DesiredTarget;
    float DesiredSpeed;
    if (bAircraftAttackRun)
    {
        const float LeadTime = FMath::Clamp(PlayerDistance / FMath::Max(AircraftAttackSpeed, 1.0f), 0.25f, 1.20f);
        const float HeightBias = EnemyAirframe == ERotorlineEnemyAirframe::MD500 ? 350.0f : 650.0f;
        FVector AttackAxis = (PlayerLocation - GetActorLocation()).GetSafeNormal2D();
        if (AttackAxis.IsNearlyZero())
        {
            AttackAxis = GetActorForwardVector().GetSafeNormal2D();
        }
        const FVector PassRight =
            FVector::CrossProduct(FVector::UpVector, AttackAxis).GetSafeNormal();
        const float AttackPassOffset = EnemyAirframe == ERotorlineEnemyAirframe::MD500
            ? RotorlineEnemyFlightSafety::MD500AttackPassOffsetCm
            : RotorlineEnemyFlightSafety::AttackPassOffsetCm;
        // Weapons continue to track the player, but the aircraft itself flies
        // an offset firing pass instead of steering through the player's center.
        DesiredTarget = PlayerLocation + PlayerVelocity * LeadTime +
            PassRight * (AircraftPassSide * AttackPassOffset) +
            FVector::UpVector * HeightBias;
        DesiredSpeed = AircraftAttackSpeed;
    }
    else
    {
        DesiredTarget = AircraftBreakawayWaypoint;
        DesiredSpeed = AircraftBreakSpeed;
    }

    // Look ahead more than two seconds at cruise speed and command a climb
    // before a ridge, instead of correcting altitude only after penetration.
    const FVector FlatForward = FRotator(0.0f, GetActorRotation().Yaw, 0.0f).Vector();
    float LookAheadGround = 0.0f;
    if (TraceAircraftGround(GetActorLocation() + FlatForward * 8500.0f, LookAheadGround))
    {
        DesiredTarget.Z = FMath::Max(DesiredTarget.Z, LookAheadGround + 2300.0f);
    }

    const FRotator DesiredHeading = (DesiredTarget - GetActorLocation()).Rotation();
    const FVector PreviousVelocity = AircraftVelocity;
    const float PreviousSpeed = PreviousVelocity.Size();
    const FVector PreviousDirection = PreviousSpeed > 1.0f
        ? PreviousVelocity / PreviousSpeed
        : GetActorForwardVector().GetSafeNormal();
    const FRotator PreviousVelocityHeading = PreviousDirection.Rotation();
    const float YawError = FMath::FindDeltaAngleDegrees(PreviousVelocityHeading.Yaw, DesiredHeading.Yaw);

    // Reduce power in a hard turn. The airframe can no longer maintain full
    // attack speed while slewing through a large heading error.
    const float AlignmentSpeedScale = FMath::GetMappedRangeValueClamped(
        FVector2D(20.0f, 100.0f), FVector2D(1.0f, 0.48f), FMath::Abs(YawError));
    DesiredSpeed *= AlignmentSpeedScale;
    AircraftForwardSpeed = FMath::FInterpConstantTo(AircraftForwardSpeed, DesiredSpeed, DeltaSeconds, AircraftAcceleration);

    // Turn the velocity vector under an explicit yaw/pitch rate limit before
    // moving. Actor rotation then follows that real velocity, so the model can
    // no longer translate sideways toward an independently computed target.
    const float VelocityYawStep = FMath::Clamp(
        YawError,
        -AircraftTurnRate * DeltaSeconds,
        AircraftTurnRate * DeltaSeconds);
    const float DesiredPitch = FMath::Clamp(DesiredHeading.Pitch, -11.0f, 12.0f);
    const float PitchTurnRate = AircraftTurnRate * 0.55f;
    const float VelocityPitchStep = FMath::Clamp(
        FMath::FindDeltaAngleDegrees(PreviousVelocityHeading.Pitch, DesiredPitch),
        -PitchTurnRate * DeltaSeconds,
        PitchTurnRate * DeltaSeconds);
    const FRotator VelocityHeading(
        PreviousVelocityHeading.Pitch + VelocityPitchStep,
        PreviousVelocityHeading.Yaw + VelocityYawStep,
        0.0f);
    const FVector VelocityDirection = VelocityHeading.Vector().GetSafeNormal();
    AircraftVelocity = VelocityDirection * AircraftForwardSpeed;

    const FRotator CurrentRotation = GetActorRotation();
    const float ActorYawStep = FMath::Clamp(
        FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, VelocityHeading.Yaw),
        -AircraftTurnRate * DeltaSeconds,
        AircraftTurnRate * DeltaSeconds);
    const float ActorPitchStep = FMath::Clamp(
        FMath::FindDeltaAngleDegrees(CurrentRotation.Pitch, VelocityHeading.Pitch),
        -PitchTurnRate * DeltaSeconds,
        PitchTurnRate * DeltaSeconds);
    const float NewYaw = CurrentRotation.Yaw + ActorYawStep;
    const float NewPitch = CurrentRotation.Pitch + ActorPitchStep;
    const float BankLimit = EnemyAirframe == ERotorlineEnemyAirframe::MD500 ? 28.0f : 24.0f;
    const float TurnDegreesPerSecond = VelocityYawStep / FMath::Max(DeltaSeconds, 0.001f);
    const float TargetBank = FMath::Clamp(-TurnDegreesPerSecond * 1.05f, -BankLimit, BankLimit);
    const float NewBank = FMath::FInterpTo(CurrentRotation.Roll, TargetBank, DeltaSeconds, 3.0f);
    SetActorRotation(FRotator(NewPitch, NewYaw, NewBank));

    FVector NewLocation = GetActorLocation() + AircraftVelocity * DeltaSeconds;

    float GroundHeight = 0.0f;
    float AboveGround = 999999.0f;
    if (TraceAircraftGround(NewLocation, GroundHeight))
    {
        const float MinimumAltitude = GroundHeight + 1800.0f;
        if (NewLocation.Z < MinimumAltitude)
        {
            NewLocation.Z = FMath::FInterpTo(NewLocation.Z, MinimumAltitude, DeltaSeconds, 6.0f);
        }
        AboveGround = NewLocation.Z - GroundHeight;
    }

    FVector PlayerToNewLocation = NewLocation - PlayerLocation;
    float NewSeparation = PlayerToNewLocation.Size();
    if (NewSeparation < RotorlineEnemyFlightSafety::HardSeparationCm)
    {
        FVector Outward = PlayerToNewLocation.GetSafeNormal();
        if (Outward.IsNearlyZero())
        {
            Outward = GetActorRightVector().GetSafeNormal();
        }
        NewLocation = PlayerLocation + Outward * RotorlineEnemyFlightSafety::HardSeparationCm;

        FVector RelativeFlightVelocity = AircraftVelocity - PlayerVelocity;
        const float ClosingVelocity = FVector::DotProduct(RelativeFlightVelocity, Outward);
        if (ClosingVelocity < 0.0f)
        {
            RelativeFlightVelocity -= Outward * ClosingVelocity;
        }
        AircraftVelocity = (PlayerVelocity + RelativeFlightVelocity + Outward * 1200.0f)
            .GetClampedToMaxSize(FMath::Max(AircraftBreakSpeed, 1.0f));
        AircraftForwardSpeed = AircraftVelocity.Size();
        bAircraftAttackRun = false;
        bAircraftSeparationAvoidanceActive = true;
        AircraftManeuverTime = FMath::Max(AircraftManeuverTime, 7.0f);
        NewSeparation = RotorlineEnemyFlightSafety::HardSeparationCm;
        UE_LOG(LogTemp, Warning,
            TEXT("ROTORLINE_ENEMY_FLIGHT|SEPARATION|state=HARD_BOUNDARY|airframe=%s|distance_m=%.1f|minimum_m=%.1f"),
            GetAirframeName(),
            NewSeparation / 100.0f,
            RotorlineEnemyFlightSafety::HardSeparationCm / 100.0f);
    }
    SetActorLocation(NewLocation);

    AircraftLastTurnRate = FMath::Abs(VelocityYawStep) / FMath::Max(DeltaSeconds, 0.001f);
    AircraftLastAcceleration = FMath::Abs(AircraftForwardSpeed - PreviousSpeed) /
        FMath::Max(DeltaSeconds * 100.0f, 0.001f);
    AircraftLastVelocityDot = FVector::DotProduct(GetActorForwardVector(), VelocityDirection);
    AircraftLastTargetDot = FVector::DotProduct(
        GetActorForwardVector(),
        (DesiredTarget - GetActorLocation()).GetSafeNormal());
    if (bEnemyFlightQualificationMode)
    {
        AircraftQualificationMinVelocityDot = FMath::Min(
            AircraftQualificationMinVelocityDot,
            AircraftLastVelocityDot);
        AircraftQualificationMaxTurnRate = FMath::Max(
            AircraftQualificationMaxTurnRate,
            AircraftLastTurnRate);
        AircraftQualificationMaxAcceleration = FMath::Max(
            AircraftQualificationMaxAcceleration,
            AircraftLastAcceleration);
        ++AircraftQualificationSamples;
    }

    if (Now - LastAircraftAuditLogTime >= 3.0f)
    {
        LastAircraftAuditLogTime = Now;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_ENEMY_FLIGHT|TELEMETRY|airframe=%s|state=%s|speed_kmh=%.1f|velocity_dot=%.3f|target_dot=%.3f|turn_dps=%.1f|accel_mps2=%.2f|yaw_error=%.1f|pitch=%.1f|bank=%.1f|agl_m=%.1f|weapon_solution=%d"),
            GetAirframeName(), bAircraftAttackRun ? TEXT("ATTACK") : TEXT("BREAK"), AircraftForwardSpeed * 0.036f,
            AircraftLastVelocityDot, AircraftLastTargetDot, AircraftLastTurnRate, AircraftLastAcceleration,
            YawError, NewPitch, NewBank, AboveGround / 100.0f,
            !bPlayerStealthed && HasWeaponSolution(PlayerLocation) ? 1 : 0);
    }
}

void ARotorlineMissionObjectiveActor::SetEnemyFlightQualificationMode()
{
    bEnemyFlightQualificationMode = true;
    AircraftPassSide = 1;
    AircraftQualificationElapsed = 0.0f;
    AircraftQualificationNextShotTime = 1.5f;
    AircraftQualificationMinVelocityDot = 1.0f;
    AircraftQualificationMaxTurnRate = 0.0f;
    AircraftQualificationMaxAcceleration = 0.0f;
    AircraftQualificationSamples = 0;
    AircraftQualificationShots = 0;
    AircraftQualificationMilestone = 0;
    AircraftDamageQualificationStage = 0;
    AircraftQualificationMainRotorDegrees = 0.0f;
    AircraftQualificationTailRotorDegrees = 0.0f;
    AircraftQualificationLastMainRotorRotation = (EnemyAirframe == ERotorlineEnemyAirframe::MD500
        ? EnemyMainRotor->GetRelativeRotation()
        : EnemyAirframe == ERotorlineEnemyAirframe::Apache
            ? ApacheMainRotorPivot->GetRelativeRotation()
            : SecondaryMesh->GetRelativeRotation()).Quaternion();
    AircraftQualificationLastTailRotorRotation = (EnemyAirframe == ERotorlineEnemyAirframe::MD500
        ? EnemyTailRotor->GetRelativeRotation()
        : EnemyAirframe == ERotorlineEnemyAirframe::Apache
            ? ApacheTailRotorPivot->GetRelativeRotation()
            : TertiaryMesh->GetRelativeRotation()).Quaternion();
    bAircraftQualificationRotorSampleInitialized = true;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_ENEMY_FLIGHT_TEST|READY|airframe=%s"), GetAirframeName());
}

void ARotorlineMissionObjectiveActor::UpdateEnemyFlightQualification(
    float DeltaSeconds,
    ARotorlineHelicopterPawn* PlayerHelicopter)
{
    if (!bEnemyFlightQualificationMode || !PlayerHelicopter || !GetWorld()) return;

    AircraftQualificationElapsed += DeltaSeconds;
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineEnemyDamageTest")))
    {
        if (AircraftDamageQualificationStage == 0 && AircraftQualificationElapsed >= 2.0f)
        {
            AircraftDamageQualificationStage = 1;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_COMBAT_LOOP_TEST|ENEMY_TEST_SHOT|sequence=1|weapon=30MM|damage=56.0"));
            ApplyRocketDamage(56.0f);
        }
        else if (AircraftDamageQualificationStage == 1 && AircraftQualificationElapsed >= 3.5f)
        {
            AircraftDamageQualificationStage = 2;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_COMBAT_LOOP_TEST|ENEMY_TEST_SHOT|sequence=2|weapon=ROCKET|damage=250.0"));
            ApplyRocketDamage(250.0f);
            return;
        }
    }
    const FQuat MainRotorRotation = (EnemyAirframe == ERotorlineEnemyAirframe::MD500
        ? EnemyMainRotor->GetRelativeRotation()
        : EnemyAirframe == ERotorlineEnemyAirframe::Apache
            ? ApacheMainRotorPivot->GetRelativeRotation()
            : SecondaryMesh->GetRelativeRotation()).Quaternion();
    if (bAircraftQualificationRotorSampleInitialized)
    {
        AircraftQualificationMainRotorDegrees += FMath::RadiansToDegrees(
            MainRotorRotation.AngularDistance(AircraftQualificationLastMainRotorRotation));
        AircraftQualificationLastMainRotorRotation = MainRotorRotation;
        if (EnemyAirframe != ERotorlineEnemyAirframe::Hind)
        {
            const FQuat TailRotorRotation = (EnemyAirframe == ERotorlineEnemyAirframe::MD500
                ? EnemyTailRotor->GetRelativeRotation()
                : EnemyAirframe == ERotorlineEnemyAirframe::Apache
                    ? ApacheTailRotorPivot->GetRelativeRotation()
                    : TertiaryMesh->GetRelativeRotation()).Quaternion();
            AircraftQualificationTailRotorDegrees += FMath::RadiansToDegrees(
                TailRotorRotation.AngularDistance(AircraftQualificationLastTailRotorRotation));
            AircraftQualificationLastTailRotorRotation = TailRotorRotation;
        }
    }
    if (AircraftQualificationElapsed >= AircraftQualificationNextShotTime)
    {
        if (HasWeaponSolution(PlayerHelicopter->GetActorLocation()))
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector MuzzleLocation = GetMuzzleLocation();
            ARotorlineEnemyProjectile* Projectile = GetWorld()->SpawnActor<ARotorlineEnemyProjectile>(
                ARotorlineEnemyProjectile::StaticClass(),
                MuzzleLocation,
                GetActorForwardVector().Rotation(),
                SpawnParams);
            if (Projectile)
            {
                const ERotorlineEnemyWeaponType WeaponType = EnemyAirframe == ERotorlineEnemyAirframe::MD500
                    ? ERotorlineEnemyWeaponType::MachineGun
                    : ERotorlineEnemyWeaponType::GuidedMissile;
                Projectile->Launch(MuzzleLocation, PlayerHelicopter, 0.0f, WeaponType);
                NotifyWeaponFired(WeaponType);
                ++AircraftQualificationShots;
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_ENEMY_FLIGHT_TEST|SHOT|airframe=%s|count=%d|state=%s|turn_dps=%.1f|bank=%.1f|velocity_dot=%.3f"),
                    GetAirframeName(), AircraftQualificationShots,
                    bAircraftAttackRun ? TEXT("ATTACK") : TEXT("BREAK"),
                    AircraftLastTurnRate, GetActorRotation().Roll, AircraftLastVelocityDot);
            }
            AircraftQualificationNextShotTime = AircraftQualificationElapsed +
                (EnemyAirframe == ERotorlineEnemyAirframe::MD500 ? 1.0f : 2.8f);
        }
        else
        {
            AircraftQualificationNextShotTime = AircraftQualificationElapsed + 0.25f;
        }
    }

    // The final gate is deliberately a full minute. Short smoke tests did not
    // expose the old sideways-translation and cadence failures reliably.
    constexpr float Milestones[] = { 10.0f, 30.0f, 60.0f };
    if (AircraftQualificationMilestone < UE_ARRAY_COUNT(Milestones) &&
        AircraftQualificationElapsed >= Milestones[AircraftQualificationMilestone])
    {
        const bool bLimitsPass =
            AircraftQualificationMinVelocityDot >= 0.96f &&
            AircraftQualificationMaxTurnRate <= AircraftTurnRate + 0.1f &&
            AircraftQualificationMaxAcceleration <= AircraftAcceleration / 100.0f + 0.05f;
        const bool bRotorPass =
            AircraftQualificationMainRotorDegrees >= AircraftRotorRate * AircraftQualificationElapsed * 0.70f &&
            (EnemyAirframe == ERotorlineEnemyAirframe::Hind ||
                AircraftQualificationTailRotorDegrees >= AircraftRotorRate * RotorlineEnemyRotorVisuals::TailRateMultiplier * AircraftQualificationElapsed * 0.70f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_ENEMY_FLIGHT_TEST|PROOF|airframe=%s|t=%.1f|samples=%d|min_velocity_dot=%.3f|max_turn_dps=%.2f|turn_limit_dps=%.2f|max_accel_mps2=%.2f|accel_limit_mps2=%.2f|shots=%d|main_rotor_deg=%.0f|tail_rotor_deg=%.0f|state=%s|result=%s"),
            GetAirframeName(), AircraftQualificationElapsed, AircraftQualificationSamples,
            AircraftQualificationMinVelocityDot, AircraftQualificationMaxTurnRate, AircraftTurnRate,
            AircraftQualificationMaxAcceleration, AircraftAcceleration / 100.0f,
            AircraftQualificationShots, AircraftQualificationMainRotorDegrees, AircraftQualificationTailRotorDegrees,
            bAircraftAttackRun ? TEXT("ATTACK") : TEXT("BREAK"),
            bLimitsPass && bRotorPass && (AircraftQualificationElapsed < 59.5f || AircraftQualificationShots > 0)
                ? TEXT("PASS") : TEXT("FAIL"));
        ++AircraftQualificationMilestone;
    }

    if (AircraftQualificationElapsed >= 65.0f &&
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineEnemyTestAutoExit")))
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_ENEMY_FLIGHT_TEST|AUTO_EXIT|airframe=%s|elapsed=%.1f"),
            GetAirframeName(), AircraftQualificationElapsed);
        FPlatformMisc::RequestExit(false);
    }
}

void ARotorlineMissionObjectiveActor::UpdateAircraftDeath(float DeltaSeconds)
{
    if (!bAircraftDying || !GetWorld()) return;
    AircraftDeathElapsed += DeltaSeconds;
    AircraftDeathSmokeAccumulator += DeltaSeconds;
    if (AircraftDeathSmokeAccumulator >= 0.10f)
    {
        AircraftDeathSmokeAccumulator = 0.0f;
        const FVector SmokeLocation = GetAimLocation() - GetActorForwardVector() * 180.0f;
        if (ARotorlineRocketTrailSegment* Smoke = GetWorld()->SpawnActor<ARotorlineRocketTrailSegment>(
            ARotorlineRocketTrailSegment::StaticClass(), SmokeLocation, FRotator::ZeroRotator))
        {
            const FVector TravelDirection = AircraftVelocity.IsNearlyZero()
                ? GetActorForwardVector()
                : AircraftVelocity.GetSafeNormal();
            Smoke->InitializeTrail(SmokeLocation, TravelDirection);
        }
    }
    AircraftRotorSpinScale = FMath::FInterpTo(AircraftRotorSpinScale, 0.12f, DeltaSeconds, 0.65f);
    AnimateAircraftRotors(DeltaSeconds, AircraftRotorSpinScale);
    AircraftForwardSpeed = FMath::FInterpTo(AircraftForwardSpeed, 650.0f, DeltaSeconds, 0.8f);
    AircraftFallSpeed = FMath::Min(4200.0f, AircraftFallSpeed + 1050.0f * DeltaSeconds);
    const FVector FlatForward = FRotator(0.0f, GetActorRotation().Yaw, 0.0f).Vector();
    SetActorLocation(GetActorLocation() + (FlatForward * AircraftForwardSpeed - FVector::UpVector * AircraftFallSpeed) * DeltaSeconds);
    const float SpinDirection = AircraftPassSide >= 0 ? 1.0f : -1.0f;
    SetActorRotation(GetActorRotation() + FRotator(-7.0f * DeltaSeconds, 62.0f * SpinDirection * DeltaSeconds, 78.0f * SpinDirection * DeltaSeconds));

    float GroundHeight = 0.0f;
    const bool bGroundImpact = TraceAircraftGround(GetActorLocation(), GroundHeight) && GetActorLocation().Z <= GroundHeight + 240.0f;
    if (bGroundImpact || AircraftDeathElapsed >= 5.0f)
    {
        const FVector ExplosionLocation = bGroundImpact
            ? FVector(GetActorLocation().X, GetActorLocation().Y, GroundHeight + 180.0f)
            : GetAimLocation();
        SpawnAircraftDeathExplosion(ExplosionLocation);
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_ENEMY_FLIGHT|DEATH|airframe=%s|phase=%s|elapsed=%.2f"),
            GetAirframeName(), bGroundImpact ? TEXT("GROUND_IMPACT") : TEXT("TERMINAL_BREAKUP"), AircraftDeathElapsed);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COMBAT_LOOP_TEST|ENEMY_CRASH|airframe=%s|phase=%s|elapsed=%.2f"),
            GetAirframeName(), bGroundImpact ? TEXT("GROUND_IMPACT") : TEXT("TERMINAL_BREAKUP"), AircraftDeathElapsed);
        FinishDestroyedTarget();
    }
}

void ARotorlineMissionObjectiveActor::SpawnAircraftDeathExplosion(const FVector& Location) const
{
    if (!GetWorld()) return;
    if (ARotorlineRocketProjectile* Explosion = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
        ARotorlineRocketProjectile::StaticClass(), Location, FRotator::ZeroRotator))
    {
        Explosion->DetonateVisualOnly(Location);
    }
}

void ARotorlineMissionObjectiveActor::FinishDestroyedTarget()
{
    bAircraftDying = false;
    bDestroyedTarget = true;
    TargetHealth = 0.0f;
    FireAudio->Stop();
    EngineAudio->Stop();
    FireAudioStopTime = -1.0f;
    MuzzleFlashStopTime = -1.0f;
    PrimaryMesh->SetVisibility(false, true);
    SecondaryMesh->SetVisibility(false, true);
    TertiaryMesh->SetVisibility(false, true);
    for (UStaticMeshComponent* HimarsPart : HimarsMeshParts)
    {
        if (!HimarsPart) continue;
        HimarsPart->SetVisibility(false, true);
        HimarsPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    EnemyMainRotor->SetVisibility(false, true);
    EnemyTailRotor->SetVisibility(false, true);
    SkeletalSubject->SetVisibility(false, true);
    MuzzleFlash->SetVisibility(false, true);
    MuzzleLight->SetVisibility(false, true);
    MarkerBeam->SetVisibility(false, true);
    if (UMaterialInterface* SuccessMaterial = LoadObject<UMaterialInterface>(nullptr, RotorlineMissionVisuals::GreenGlowPath))
    {
        MarkerRing->SetMaterial(0, SuccessMaterial);
        MarkerPulseRing->SetMaterial(0, SuccessMaterial);
        MarkerHLeft->SetMaterial(0, SuccessMaterial);
        MarkerHRight->SetMaterial(0, SuccessMaterial);
    }
    ObjectiveLabel->SetText(FText::FromString(TEXT("TARGET DESTROYED")));
    ObjectiveLabel->SetTextRenderColor(FColor(120, 255, 150));
    BeaconLight->SetLightColor(FLinearColor(1.0f, 0.02f, 0.0f));
    BeaconLight->SetIntensity(55000.0f);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_MISSION_WORLD|TARGET_DESTROYED|target=%s"), *TargetId);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_GROUND_TARGET|REMOVED|target=%s|threat=%d|visible_himars_parts=0|fire_audio=STOPPED|future_fire=DISABLED"),
        *TargetId,
        static_cast<int32>(ThreatType));
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_COMBAT_LOOP_TEST|ENEMY_DESTROYED|target=%s|airframe=%s"),
        *TargetId, GetAirframeName());
    if (bEnemyFlightQualificationMode &&
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineEnemyDamageTest")))
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COMBAT_LOOP_TEST|COMPLETE|target=%s|airframe=%s|result=PASS"),
            *TargetId, GetAirframeName());
        FString CombatLoopScenario;
        if (!FParse::Value(FCommandLine::Get(), TEXT("RotorlineCombatLoopTest="), CombatLoopScenario))
        {
            FPlatformMisc::RequestExit(false);
        }
    }
    // Leave the destroyed flag available long enough for mission scoring to
    // observe it, then remove the dead emplacement from world iteration and
    // the controller's weak references.
    SetLifeSpan(1.5f);
}

void ARotorlineMissionObjectiveActor::ApplyRocketDamage(float Damage)
{
    float AppliedDamage = 0.0f;
    ApplyCombatDamage(Damage, TEXT("LEGACY"), AppliedDamage);
}

bool ARotorlineMissionObjectiveActor::ApplyCombatDamage(
    float RequestedDamage,
    const TCHAR* DamageSource,
    float& OutAppliedDamage)
{
    OutAppliedDamage = 0.0f;
    if (!bDestroyObjective || bDestroyedTarget || bAircraftDying)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("ROTORLINE_DAMAGE_INTEGRITY|IGNORED|target=%s|source=%s|reason=INACTIVE|health=%.1f"),
            *TargetId, DamageSource ? DamageSource : TEXT("UNKNOWN"), TargetHealth);
        return false;
    }

    const float SanitizedDamage = FMath::IsFinite(RequestedDamage)
        ? FMath::Max(0.0f, RequestedDamage)
        : 0.0f;
    if (SanitizedDamage <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_DAMAGE_INTEGRITY|IGNORED|target=%s|source=%s|reason=NON_POSITIVE|requested=%.1f|health=%.1f"),
            *TargetId, DamageSource ? DamageSource : TEXT("UNKNOWN"), RequestedDamage, TargetHealth);
        return false;
    }

    const float PreviousHealth = TargetHealth;
    TargetHealth = FMath::Clamp(PreviousHealth - SanitizedDamage, 0.0f, FMath::Max(1.0f, TargetMaxHealth));
    OutAppliedDamage = PreviousHealth - TargetHealth;
    if (OutAppliedDamage <= KINDA_SMALL_NUMBER)
    {
        return false;
    }
    ++PlayerDamageEventCount;
    const bool bFatalDamage = PreviousHealth > 0.0f && TargetHealth <= 0.0f;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_ENEMY_DAMAGE|target=%s|airframe=%s|source=%s|hit_index=%d|requested=%.1f|damage=%.1f|health_before=%.1f|health=%.1f|max_health=%.1f|health_pct=%.0f"),
        *TargetId, GetAirframeName(), DamageSource ? DamageSource : TEXT("UNKNOWN"), PlayerDamageEventCount,
        RequestedDamage, OutAppliedDamage, PreviousHealth, TargetHealth, TargetMaxHealth, GetHealthFraction() * 100.0f);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_COMBAT_LOOP_TEST|ENEMY_DAMAGE|target=%s|airframe=%s|source=%s|hit_index=%d|damage=%.1f|health_before=%.1f|health_after=%.1f|fatal=%d"),
        *TargetId, GetAirframeName(), DamageSource ? DamageSource : TEXT("UNKNOWN"), PlayerDamageEventCount,
        OutAppliedDamage, PreviousHealth, TargetHealth, bFatalDamage ? 1 : 0);
    if (!bFatalDamage)
    {
        return false;
    }

    if (IsAircraftThreat())
    {
        bAircraftDying = true;
        AircraftDeathElapsed = 0.0f;
        AircraftDeathSmokeAccumulator = 0.0f;
        AircraftFallSpeed = 200.0f;
        AircraftRotorSpinScale = 1.0f;
        FireAudio->Stop();
        EngineAudio->Stop();
        MuzzleFlash->SetVisibility(false, true);
        MuzzleLight->SetVisibility(false, true);
        ObjectiveLabel->SetText(FText::FromString(TEXT("AIRCRAFT DISABLED")));
        SpawnAircraftDeathExplosion(GetAimLocation());
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_ENEMY_FLIGHT|DEATH|airframe=%s|phase=FATAL_DAMAGE|speed_kmh=%.1f"),
            GetAirframeName(), AircraftForwardSpeed * 0.036f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COMBAT_LOOP_TEST|ENEMY_FATAL|target=%s|airframe=%s|speed_kmh=%.1f"),
            *TargetId, GetAirframeName(), AircraftForwardSpeed * 0.036f);
        return true;
    }

    // Ground threats need an unambiguous kill response. The old path hid only
    // the three generic meshes (not the multipart HIMARS model), so the unit
    // looked untouched even though its health and firing state were dead.
    SpawnAircraftDeathExplosion(GetAimLocation());
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_GROUND_TARGET|FATAL|target=%s|threat=%d|health=0|explosion=1"),
        *TargetId,
        static_cast<int32>(ThreatType));
    FinishDestroyedTarget();
    return true;
}

FVector ARotorlineMissionObjectiveActor::GetAimLocation() const
{
    if (IsAircraftThreat()) return GetActorLocation() + FVector(0.0f, 0.0f, 240.0f);
    if (ThreatType == ERotorlineThreatType::Tank) return GetActorLocation() + FVector(0.0f, 0.0f, 260.0f);
    if (ThreatType == ERotorlineThreatType::RocketArtillery) return GetActorLocation() + FVector(0.0f, 0.0f,
        TargetId.Contains(TEXT("mortar"), ESearchCase::IgnoreCase) ? 90.0f : 210.0f);
    return GetActorLocation() + FVector(0.0f, 0.0f, ThreatType == ERotorlineThreatType::RadarMissile ? 360.0f : 220.0f);
}

bool ARotorlineMissionObjectiveActor::IsAircraftThreat() const
{
    return ThreatType == ERotorlineThreatType::MachineGunship || ThreatType == ERotorlineThreatType::RocketGunship;
}

float ARotorlineMissionObjectiveActor::GetProjectileHitRadius() const
{
    if (TargetId.Contains(TEXT("parked aircraft flight line"), ESearchCase::IgnoreCase))
    {
        return 1900.0f;
    }

    switch (EnemyAirframe)
    {
    case ERotorlineEnemyAirframe::MD500: return 520.0f;
    case ERotorlineEnemyAirframe::Apache: return 780.0f;
    case ERotorlineEnemyAirframe::Hind: return 850.0f;
    default: break;
    }

    switch (ThreatType)
    {
    case ERotorlineThreatType::Tank: return 380.0f;
    case ERotorlineThreatType::RadarMissile: return 420.0f;
    case ERotorlineThreatType::Flak: return 340.0f;
    case ERotorlineThreatType::RocketArtillery: return 500.0f;
    default: return 230.0f;
    }
}

FVector ARotorlineMissionObjectiveActor::GetMuzzleLocation() const
{
    const auto MeshBarrelTip = [&](const UStaticMeshComponent* BarrelMesh, float ExtraForward) -> FVector
    {
        const FVector AimDirection = GetWeaponAimDirection();
        if (!BarrelMesh || !BarrelMesh->GetStaticMesh())
        {
            return GetActorLocation() + AimDirection * ExtraForward;
        }
        const FBoxSphereBounds Bounds = BarrelMesh->Bounds;
        const float ExtentAlongAim = FMath::Abs(AimDirection.X) * Bounds.BoxExtent.X +
            FMath::Abs(AimDirection.Y) * Bounds.BoxExtent.Y +
            FMath::Abs(AimDirection.Z) * Bounds.BoxExtent.Z;
        return Bounds.Origin + AimDirection * (ExtentAlongAim + ExtraForward);
    };
    if (EnemyAirframe == ERotorlineEnemyAirframe::MD500) return GetActorLocation() + GetActorForwardVector() * 440.0f + FVector::UpVector * 120.0f;
    if (EnemyAirframe == ERotorlineEnemyAirframe::Apache) return GetActorLocation() + GetActorForwardVector() * 720.0f + FVector::UpVector * 145.0f;
    if (EnemyAirframe == ERotorlineEnemyAirframe::Hind) return GetActorLocation() + GetActorForwardVector() * 820.0f + FVector::UpVector * 165.0f;
    if (ThreatType == ERotorlineThreatType::Tank) return MeshBarrelTip(TertiaryMesh, 32.0f);
    if (ThreatType == ERotorlineThreatType::Flak) return MeshBarrelTip(SecondaryMesh, 28.0f);
    if (ThreatType == ERotorlineThreatType::RocketArtillery)
    {
        if (TargetId.Contains(TEXT("mortar"), ESearchCase::IgnoreCase))
        {
            return MeshBarrelTip(SecondaryMesh, 18.0f);
        }
        return HimarsLauncherPivot->GetComponentLocation() + GetWeaponAimDirection() * 310.0f;
    }
    if (ThreatType == ERotorlineThreatType::RadarMissile &&
        TargetId.Contains(TEXT("hawk"), ESearchCase::IgnoreCase))
    {
        return MeshBarrelTip(SecondaryMesh, 36.0f);
    }
    return GetActorLocation() + GetActorForwardVector() * 360.0f + FVector::UpVector * 430.0f;
}

FVector ARotorlineMissionObjectiveActor::GetWeaponAimDirection() const
{
    if (ThreatType == ERotorlineThreatType::Tank || ThreatType == ERotorlineThreatType::Flak ||
        ThreatType == ERotorlineThreatType::RocketArtillery ||
        (ThreatType == ERotorlineThreatType::RadarMissile &&
            TargetId.Contains(TEXT("hawk"), ESearchCase::IgnoreCase)))
    {
        return GroundWeaponAimRotation.Vector().GetSafeNormal();
    }
    return GetActorForwardVector().GetSafeNormal();
}

void ARotorlineMissionObjectiveActor::NotifyWeaponFired(ERotorlineEnemyWeaponType WeaponType)
{
    if (bDestroyedTarget || bAircraftDying || !bThreatVisualReady || ThreatType == ERotorlineThreatType::None || !GetWorld()) return;
    RefreshAudioMix();
    const FVector MuzzleLocation = GetMuzzleLocation();
    MuzzleFlash->SetWorldLocation(MuzzleLocation);
    MuzzleFlash->SetWorldRotation(GetWeaponAimDirection().Rotation());
    MuzzleFlash->SetVisibility(true, true);
    MuzzleLight->SetVisibility(true, true);
    MuzzleFlashStopTime = GetWorld()->GetTimeSeconds() + 0.11f;
    const bool bAircraftCannonBurst = IsAircraftThreat() &&
        (WeaponType == ERotorlineEnemyWeaponType::AutoCannon || WeaponType == ERotorlineEnemyWeaponType::MachineGun);
    if (!bAircraftCannonBurst)
    {
        FireAudio->Stop();
    }
    if (bAircraftCannonBurst)
    {
        FireAudio->SetSound(LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Vehicles/AH64/SFX_AH64_30mm_Autocannon.SFX_AH64_30mm_Autocannon")));
    }
    FireAudio->SetWorldLocation(MuzzleLocation);
    // Guided and artillery projectiles own their launch/flight audio so it
    // follows the moving rocket and stops at impact. Playing FireAudio here as
    // well stacked a second clip on every HIMARS round.
    if (WeaponType != ERotorlineEnemyWeaponType::GuidedMissile &&
        WeaponType != ERotorlineEnemyWeaponType::ArtilleryRocket)
    {
        if (!bAircraftCannonBurst || !FireAudio->IsPlaying())
        {
            FireAudio->Play();
        }
    }
    if (ThreatType == ERotorlineThreatType::Tank || ThreatType == ERotorlineThreatType::Flak ||
        ThreatType == ERotorlineThreatType::RocketArtillery)
    {
        const UStaticMeshComponent* BarrelComponent = ThreatType == ERotorlineThreatType::Tank
            ? TertiaryMesh.Get() : (ThreatType == ERotorlineThreatType::Flak ? SecondaryMesh.Get() : nullptr);
        const bool bMortar = ThreatType == ERotorlineThreatType::RocketArtillery &&
            TargetId.Contains(TEXT("mortar"), ESearchCase::IgnoreCase);
        const FVector BarrelCenter = ThreatType == ERotorlineThreatType::RocketArtillery
            ? (bMortar ? SecondaryMesh->Bounds.Origin : HimarsLauncherPivot->GetComponentLocation())
            : (BarrelComponent ? BarrelComponent->Bounds.Origin : GetActorLocation());
        const FVector CenterToMuzzle = (MuzzleLocation - BarrelCenter).GetSafeNormal();
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_GROUND_MUZZLE|type=%s|offset_cm=%.1f|aim_dot=%.4f|origin=%.0f,%.0f,%.0f"),
            ThreatType == ERotorlineThreatType::Tank ? TEXT("TANK") :
                (ThreatType == ERotorlineThreatType::RocketArtillery ? (bMortar ? TEXT("MORTAR") : TEXT("HIMARS")) : TEXT("FLAK")),
            FVector::Dist(BarrelCenter, MuzzleLocation),
            FVector::DotProduct(CenterToMuzzle, GetWeaponAimDirection()),
            MuzzleLocation.X,
            MuzzleLocation.Y,
            MuzzleLocation.Z);
    }
    const float ClipWindow = bAircraftCannonBurst ? 0.48f :
        (ThreatType == ERotorlineThreatType::MachineGunship ? 0.48f :
        (ThreatType == ERotorlineThreatType::Flak ? 0.32f :
        (ThreatType == ERotorlineThreatType::Tank ? 1.15f :
        (ThreatType == ERotorlineThreatType::RocketArtillery ? 0.70f : 1.40f))));
    FireAudioStopTime = GetWorld()->GetTimeSeconds() + ClipWindow;
}

bool ARotorlineMissionObjectiveActor::RefreshAudioMix()
{
    if (!GetWorld() || !FireAudio || !EngineAudio)
    {
        return false;
    }

    const ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(GetWorld()->GetFirstPlayerController());
    const float WeaponMix = OperationsController
        ? OperationsController->GetEffectiveAudioVolume(ERotorlineAudioChannel::WeaponsExplosions)
        : 1.0f;
    const ARotorlineHelicopterPawn* PlayerHelicopter = Cast<ARotorlineHelicopterPawn>(
        GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr);
    const float DialogueDuck = PlayerHelicopter && PlayerHelicopter->IsSpokenDialogueActive()
        ? 0.18f
        : 1.0f;
    const float EngineMix = OperationsController
        ? OperationsController->GetEffectiveAudioVolume(ERotorlineAudioChannel::Engine)
        : 1.0f;
    const float ExpectedFireVolume = FireAudioBaseVolume * WeaponMix * DialogueDuck;
    float DistanceGain = 1.0f;
    if (ThreatType == ERotorlineThreatType::Tank)
    {
        const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
        const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
        if (PlayerPawn)
        {
            // A nearby tank still has physical presence, but its engine becomes
            // background texture quickly enough that several patrols cannot
            // overpower the player's helicopter, radio, or weapon audio.
            const float DistanceCm = FVector::Dist(PlayerPawn->GetActorLocation(), GetActorLocation());
            const float FadeAlpha = FMath::Clamp((DistanceCm - 1200.0f) / 13800.0f, 0.0f, 1.0f);
            DistanceGain = FMath::Square(1.0f - FadeAlpha);
        }
    }
    const float ExpectedEngineVolume = EngineAudioBaseVolume * EngineMix * DistanceGain;
    FireAudio->SetVolumeMultiplier(ExpectedFireVolume);
    EngineAudio->SetVolumeMultiplier(ExpectedEngineVolume);
    return FMath::IsNearlyEqual(FireAudio->VolumeMultiplier, ExpectedFireVolume) &&
        FMath::IsNearlyEqual(EngineAudio->VolumeMultiplier, ExpectedEngineVolume);
}

void ARotorlineMissionObjectiveActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FireAudio->Stop();
    EngineAudio->Stop();
    Super::EndPlay(EndPlayReason);
}

void ARotorlineMissionObjectiveActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    PulseTime += DeltaSeconds;
    MarkerRing->AddLocalRotation(FRotator(0.0f, 18.0f * DeltaSeconds, 0.0f));
    const float Pulse = 1.0f + FMath::Sin(PulseTime * 3.4f) * 0.12f;
    const float BeamRadius = 0.10f;
    const float BeamHeight = 35.0f;
    MarkerBeam->SetRelativeScale3D(FVector(BeamRadius * Pulse, BeamRadius * Pulse, BeamHeight));
    const float PulseScale = MarkerBaseScale * (1.08f + FMath::Sin(PulseTime * 2.2f) * 0.10f);
    MarkerPulseRing->SetRelativeScale3D(FVector(PulseScale, PulseScale, bDestroyObjective ? 0.035f : 0.025f));
    BeaconLight->SetIntensity((bDestroyedTarget ? 26000.0f : (bDestroyObjective ? 42000.0f : 18000.0f)) * Pulse);
    if (bFireScene)
    {
        const float FlameA = 0.82f + FMath::Abs(FMath::Sin(PulseTime * 5.7f)) * 0.36f;
        if (bUseNiagaraFire && GetWorld())
        {
            const float Now = GetWorld()->GetTimeSeconds();
            if (Now >= NextFireFlameATime)
            {
                GroundFireFlameA->Activate(true);
                NextFireFlameATime = Now + 1.05f;
            }
            if (Now >= NextFireFlameBTime)
            {
                GroundFireFlameB->Activate(true);
                NextFireFlameBTime = Now + 1.37f;
            }
            if (Now >= NextFireEmberTime)
            {
                GroundFireEmbers->Activate(true);
                NextFireEmberTime = Now + 2.65f;
            }
            if (!GroundFireSmoke->IsActive()) GroundFireSmoke->Activate(true);
            GroundFireLight->SetIntensity(22000.0f + FlameA * 11000.0f);
        }
        else
        {
            const float FlameB = 0.78f + FMath::Abs(FMath::Sin(PulseTime * 7.1f + 1.4f)) * 0.42f;
            PrimaryMesh->SetRelativeScale3D(FVector(1.25f / FlameA, 1.25f * FlameA, 4.8f * FlameA));
            SecondaryMesh->SetRelativeScale3D(FVector(0.95f * FlameB, 0.95f / FlameB, 3.8f * FlameB));
            PrimaryMesh->SetRelativeLocation(FVector(-120.0f + FMath::Sin(PulseTime * 4.1f) * 55.0f, -40.0f, 300.0f + FlameA * 45.0f));
            SecondaryMesh->SetRelativeLocation(FVector(150.0f, 75.0f + FMath::Sin(PulseTime * 4.8f) * 45.0f, 235.0f + FlameB * 42.0f));
            const float SmokePulse = 0.88f + FMath::Sin(PulseTime * 1.3f) * 0.12f;
            TertiaryMesh->SetRelativeScale3D(FVector(2.0f * SmokePulse, 2.0f / SmokePulse, 2.5f + FMath::Fmod(PulseTime, 1.5f) * 0.35f));
            TertiaryMesh->AddLocalRotation(FRotator(9.0f, 18.0f, 5.0f) * DeltaSeconds);
            if (FireSmokeMaterial)
            {
                FireSmokeMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.34f + FMath::Abs(FMath::Sin(PulseTime * 1.8f)) * 0.12f);
            }
        }
        BeaconLight->SetIntensity(42000.0f + FlameA * 18000.0f);
    }

    const UWorld* World = GetWorld();
    if (World)
    {
        const float Now = World->GetTimeSeconds();
        if (FireAudioStopTime > 0.0f && Now >= FireAudioStopTime)
        {
            FireAudio->Stop();
            FireAudioStopTime = -1.0f;
        }
        if (MuzzleFlashStopTime > 0.0f && Now >= MuzzleFlashStopTime)
        {
            MuzzleFlash->SetVisibility(false, true);
            MuzzleLight->SetVisibility(false, true);
            MuzzleFlashStopTime = -1.0f;
        }
    }
    if (bAircraftDying)
    {
        UpdateAircraftDeath(DeltaSeconds);
    }

    const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
    if (APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr)
    {
        // Every live threat tracks the player unless mission-specific stealth
        // or sanctuary rules suppress its weapon solution.
        CurrentCombatTargetLocation = PlayerPawn->GetActorLocation();
        CurrentCombatTargetVelocity = PlayerPawn->GetVelocity();
        const float PlayerDistance = FVector::Dist(
            PlayerPawn->GetActorLocation(), GetActorLocation());
        if (ThreatType == ERotorlineThreatType::Tank && World)
        {
            const float Now = World->GetTimeSeconds();
            if (Now >= NextDistanceAudioRefreshTime)
            {
                RefreshAudioMix();
                NextDistanceAudioRefreshTime = Now + 0.15f;
            }
        }
        if (!bDestroyedTarget && !bAircraftDying)
        {
            if (ThreatType == ERotorlineThreatType::Tank || ThreatType == ERotorlineThreatType::Flak ||
                ThreatType == ERotorlineThreatType::RocketArtillery ||
                (ThreatType == ERotorlineThreatType::RadarMissile &&
                    TargetId.Contains(TEXT("hawk"), ESearchCase::IgnoreCase)))
            {
                // Static route threats do not need terrain probes, turret
                // tracking, or weapon-solution work while kilometres away.
                // The longest non-air engagement range is 900 m; 1.2 km gives
                // every site time to wake before the player enters range.
                if (PlayerDistance <= 120000.0f)
                {
                    UpdateGroundCombat(DeltaSeconds, PlayerPawn);
                }
            }
            else if (ThreatType == ERotorlineThreatType::RadarMissile)
            {
                SecondaryMesh->AddLocalRotation(FRotator(0.0f, FMath::RadiansToDegrees(0.7f) * DeltaSeconds, 0.0f));
            }
            else if (IsAircraftThreat())
            {
                UpdateAircraftFlight(DeltaSeconds, PlayerPawn);
                UpdateEnemyFlightQualification(
                    DeltaSeconds,
                    Cast<ARotorlineHelicopterPawn>(PlayerPawn));
            }
        }
        // Aircraft already have HUD target boxes and labels. The old distant
        // world marker rendered as a tall red debug beam above helicopters.
        MarkerBeam->SetVisibility(
            bWorldCombatMarkerEnabled &&
            !bDestroyObjective &&
            !IsAircraftThreat() &&
            !bDestroyedTarget &&
            PlayerDistance > 45000.0f,
            true);
        MarkerRing->SetVisibility(bWorldCombatMarkerEnabled && !IsAircraftThreat(), true);
        MarkerPulseRing->SetVisibility(bWorldCombatMarkerEnabled && !IsAircraftThreat(), true);
        MarkerCenter->SetVisibility(bWorldCombatMarkerEnabled && !IsAircraftThreat(), true);
        ObjectiveLabel->SetVisibility(PlayerDistance > 1800.0f && PlayerDistance < 220000.0f);
        const FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(ObjectiveLabel->GetComponentLocation(), PlayerPawn->GetActorLocation());
        ObjectiveLabel->SetWorldRotation(FRotator(0.0f, LookAt.Yaw, 0.0f));
    }
}
