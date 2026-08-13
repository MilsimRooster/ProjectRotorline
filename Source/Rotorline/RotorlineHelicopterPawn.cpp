#include "RotorlineHelicopterPawn.h"

#include "RotorlineBuildingClusterActor.h"
#include "RotorlineHelipadBeaconActor.h"
#include "RotorlineOperationsPlayerController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformMisc.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Curves/CurveFloat.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RotorlineExplosionFx.h"
#include "RotorlineMissionObjectiveActor.h"
#include "RotorlineKiowaStrikeMissionActor.h"
#include "RotorlineGroundingLibrary.h"
#include "RotorlineCabinSupplyConvoyActor.h"
#include "RotorlineFinalCinematicActor.h"
#include "Engine/StaticMeshActor.h"
#include "RotorlineCannonProjectile.h"
#include "RotorlineEnemyProjectile.h"
#include "RotorlineOperationsPlayerController.h"
#include "RotorlineFlightControllerSubsystem.h"
#include "RotorlineCombatTuning.h"
#include "RotorlineRocketProjectile.h"
#include "RotorlineRocketTrailSegment.h"
#include "RotorlineSupportLocations.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineHelicopter
{
    struct FRadioCalloutDefinition
    {
        const TCHAR* Message;
        const TCHAR* AssetName;
    };

    const FRadioCalloutDefinition RadioCalloutDefinitions[] =
    {
        { TEXT("COMMAND: Rotorline, cleared outbound. Execute the mission."), TEXT("RADIO_001_COMMAND_ClearedOutbound") },
        { TEXT("INSTRUCTOR: Trainer crossing the route. Hold altitude and maintain separation."), TEXT("RADIO_002_INSTRUCTOR_TrainerCrossing") },
        { TEXT("CREW: Armed scout closing from the right. Stay low, keep moving, and make it miss."), TEXT("RADIO_003_CREW_ArmedScoutClosing") },
        { TEXT("COMMAND: Hind inbound. Rockets hot. Break now or destroy it."), TEXT("RADIO_004_COMMAND_HindInbound") },
        { TEXT("COMMAND: Apache inbound. Break its rocket lock or destroy it."), TEXT("RADIO_005_COMMAND_ApacheInbound") },
        { TEXT("GUNNER: Hostile light helicopter closing fast. Machine guns, eleven o'clock!"), TEXT("RADIO_006_GUNNER_LightHelicopterClosing") },
        { TEXT("INSTRUCTOR: Trainer clear. Continue the route."), TEXT("RADIO_007_INSTRUCTOR_TrainerClear") },
        { TEXT("CREW: Scout is breaking away. Route ahead is open."), TEXT("RADIO_008_CREW_ScoutBreakingAway") },
        { TEXT("COMMAND: Threat is down. Continue the mission."), TEXT("RADIO_009_COMMAND_ThreatDown") },
        { TEXT("COMMAND: Enemy radar is painting the valley. Use the terrain."), TEXT("RADIO_010_COMMAND_RadarPaintingValley") },
        { TEXT("GUNNER: Tracers ahead. Keep moving and do not hover."), TEXT("RADIO_011_GUNNER_TracersAhead") },
        { TEXT("COMMAND: Confirm the target before you loose a rocket."), TEXT("RADIO_012_COMMAND_ConfirmTarget") },
        { TEXT("GUNNER: I have the site. Bring the nose around."), TEXT("RADIO_013_GUNNER_HaveTheSite") },
        { TEXT("CREW: Beacon strength is changing. Check the next relay and watch for smoke."), TEXT("RADIO_014_CREW_BeaconStrengthChanging") },
        { TEXT("COMMAND: Survivor clock is active. Keep the shoreline on your right."), TEXT("RADIO_015_COMMAND_SurvivorClockActive") },
        { TEXT("CREW: I have a weak return ahead. Could be the missing hiker."), TEXT("RADIO_016_CREW_WeakReturnAhead") },
        { TEXT("COMMAND: Hospital team is standing by. Bring the survivor straight in."), TEXT("RADIO_017_COMMAND_HospitalStandingBy") },
        { TEXT("MEDIC: Patient is unstable. Give us the smoothest approach you can."), TEXT("RADIO_018_MEDIC_PatientUnstable") },
        { TEXT("COMMAND: Trauma team has the pad. Do not lose time in the circuit."), TEXT("RADIO_019_COMMAND_TraumaTeamHasPad") },
        { TEXT("CREW: Landing zone ahead. Checking wires and slope."), TEXT("RADIO_020_CREW_LandingZoneAhead") },
        { TEXT("MEDIC: We are ready to transfer as soon as the skids settle."), TEXT("RADIO_021_MEDIC_ReadyToTransfer") },
        { TEXT("CREW: Cargo is secure. Avoid abrupt pedal turns."), TEXT("RADIO_022_CREW_CargoSecure") },
        { TEXT("COMMAND: Cabin team has a flare out for your approach."), TEXT("RADIO_023_COMMAND_CabinFlareOut") },
        { TEXT("CREW: Watching the load and the rising ground."), TEXT("RADIO_024_CREW_WatchingLoad") },
        { TEXT("COMMAND: Deliver, confirm the handoff, and clear the site."), TEXT("RADIO_025_COMMAND_DeliverConfirmClear") },
        { TEXT("INSTRUCTOR: Hold the route and keep scanning outside."), TEXT("RADIO_026_INSTRUCTOR_HoldRoute") },
        { TEXT("CREW: Ridgeline is clear. Watching the low ground."), TEXT("RADIO_027_CREW_RidgelineClear") },
        { TEXT("COMMAND: Keep the landing zone in sight and check your approach."), TEXT("RADIO_028_COMMAND_KeepLZInSight") },
        { TEXT("CREW: Wind is moving across the valley. Correcting right."), TEXT("RADIO_029_CREW_WindAcrossValley") },
        { TEXT("GUNNER: Missile away! Break hard and get behind terrain!"), TEXT("RADIO_030_GUNNER_MissileAway") },
        { TEXT("COMMAND: Radar lock. Use the terrain before it launches."), TEXT("RADIO_031_COMMAND_RadarLock") },
        { TEXT("GUNNER: Apache rocket launch! Break and use terrain!"), TEXT("RADIO_032_GUNNER_ApacheRocketLaunch") },
        { TEXT("GUNNER: Apache cannon! Jink now!"), TEXT("RADIO_033_GUNNER_ApacheCannon") },
        { TEXT("GUNNER: Gunship firing! Keep moving!"), TEXT("RADIO_034_GUNNER_GunshipFiring") },
        { TEXT("GUNNER: Incoming fire. Break now!"), TEXT("RADIO_035_GUNNER_IncomingFire") },
        { TEXT("CREW: People are counting on us."), TEXT("RADIO_036_CREW_PeopleAreCounting") },
        { TEXT("CREW: Keep it steady."), TEXT("RADIO_037_CREW_KeepItSteady") },
        { TEXT("CREW: Check your gauges."), TEXT("RADIO_038_CREW_CheckYourGauges") },
        { TEXT("CREW: I remember this time."), TEXT("RADIO_039_CREW_RememberThisTime") },
        { TEXT("GUNNER: HIND rocket launch! Break and use terrain!"), TEXT("RADIO_040_GUNNER_HindRocketLaunch") },
    };

    constexpr double RepeatedRadioMessageCooldownSeconds = 150.0;
    constexpr double TacticalRadioCooldownSeconds = 45.0;
    constexpr double MinimumRadioCalloutSeparationSeconds = 2.25;

    FString GetTacticalRadioCategory(const FString& Message)
    {
        if (Message.Contains(TEXT("radar"), ESearchCase::IgnoreCase))
        {
            return TEXT("RADAR_LOCK");
        }
        if (Message.Contains(TEXT("missile away"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("rocket launch"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("salvo inbound"), ESearchCase::IgnoreCase))
        {
            return TEXT("MISSILE_WARNING");
        }
        if (Message.Contains(TEXT("cannon"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("gunship firing"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("incoming fire"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("tracers ahead"), ESearchCase::IgnoreCase))
        {
            return TEXT("GUNFIRE_WARNING");
        }
        if (Message.Contains(TEXT("flares out"), ESearchCase::IgnoreCase))
        {
            return TEXT("COUNTERMEASURE_CONFIRM");
        }
        return FString();
    }

    double GetTacticalRadioCooldown(const FString& Category)
    {
        // Command announces a radar lock once per sortie. Threat tones and
        // physical missile audio remain independent and repeat normally.
        return Category == TEXT("RADAR_LOCK")
            ? TNumericLimits<double>::Max()
            : TacticalRadioCooldownSeconds;
    }

    constexpr int32 MaximumCatalogMeshParts = 220;
    constexpr float TargetCatalogAircraftSpanCm = 980.0f;
    constexpr float MinimumCatalogAircraftSpanCm = 620.0f;
    constexpr float MaximumCatalogAircraftSpanCm = 1380.0f;

    bool IsCatalogRotorPath(const FString& Path)
    {
        const FString Lower = Path.ToLower();
        return Lower.Contains(TEXT("rotor"))
            || Lower.Contains(TEXT("blade"))
            || Lower.Contains(TEXT("prop"));
    }

    bool IsCatalogTailRotorPath(const FString& Path)
    {
        const FString Lower = Path.ToLower();
        return Lower.Contains(TEXT("tail"))
            || Lower.Contains(TEXT("backrotor"))
            || Lower.Contains(TEXT("back_rotor"))
            || Lower.Contains(TEXT("backprop"));
    }

    FVector CatalogRotorAxis(const FString& AxisName, bool bTailRotor)
    {
        if (AxisName.Equals(TEXT("-X"), ESearchCase::IgnoreCase)) return -FVector::ForwardVector;
        if (AxisName.Equals(TEXT("-Y"), ESearchCase::IgnoreCase)) return -FVector::RightVector;
        if (AxisName.Equals(TEXT("-Z"), ESearchCase::IgnoreCase)) return -FVector::UpVector;
        if (AxisName.Equals(TEXT("X"), ESearchCase::IgnoreCase)) return FVector::ForwardVector;
        if (AxisName.Equals(TEXT("Y"), ESearchCase::IgnoreCase)) return FVector::RightVector;
        if (AxisName.Equals(TEXT("Z"), ESearchCase::IgnoreCase)) return FVector::UpVector;
        return bTailRotor ? FVector::ForwardVector : FVector::UpVector;
    }

    constexpr double SpawnX = -236194.1;
    constexpr double SpawnY = -193027.5;
    constexpr double SpawnZ = 3595.0;
    constexpr float SpawnYaw = 8.0f;
    constexpr float BaseServiceRadiusCm = 9000.0f;
    constexpr float BaseServiceResetRadiusCm = 14000.0f;
    constexpr float BaseRearmDurationSeconds = 3.0f;
    constexpr float ServiceRepairMissingHealthFraction = 0.5f;
    constexpr float DefaultFuelEnduranceSeconds = 600.0f;
    constexpr float BoostFuelBurnMultiplier = 1.25f;
    constexpr float FuelWarningLowPercent = 25.0f;
    constexpr float FuelWarningCriticalPercent = 10.0f;
    constexpr float FuelWarningFumesPercent = 5.0f;
    constexpr float ServiceFuelThresholdPercent = 99.0f;
    constexpr float CountermeasureCooldownSeconds = 2.75f;
    constexpr float CountermeasureMissileRangeCm = 250000.0f;
    constexpr double MinimumEnemyHelicopterCooldownSeconds = 60.0;
    constexpr double DuplicateEnemyHelicopterTriggerWindowSeconds = 0.25;
    constexpr double OpeningCombatGraceSeconds = 40.0;

    const TCHAR* BodyPath = TEXT("/Game/Vehicles/Candidates/BellHuey/bell_huey_helicopter/StaticMeshes/Bell_Huey_lambert1_0.Bell_Huey_lambert1_0");
    const TCHAR* GlassPath = TEXT("/Game/Vehicles/Candidates/BellHuey/bell_huey_helicopter/StaticMeshes/Bell_Huey_Glass_0.Bell_Huey_Glass_0");
    const TCHAR* InteriorPath = TEXT("/Game/Vehicles/Candidates/BellHuey/bell_huey_helicopter/StaticMeshes/Inside1_Huey_inside_material_0.Inside1_Huey_inside_material_0");
    const TCHAR* MainRotorPath = TEXT("/Game/Vehicles/Candidates/BellHuey/bell_huey_helicopter/SkeletalMeshes/Top_Rotor_lambert1_0.Top_Rotor_lambert1_0");
    const TCHAR* TailRotorPath = TEXT("/Game/Vehicles/Candidates/BellHuey/bell_huey_helicopter/SkeletalMeshes/Back_Rotor_lambert1_0.Back_Rotor_lambert1_0");
    const TCHAR* RotorAnimationPath = TEXT("/Game/Vehicles/Candidates/BellHuey/bell_huey_helicopter/SkeletalMeshes/bell_huey_helicopter_Anim.bell_huey_helicopter_Anim");
    const TCHAR* EngineStartupPath = TEXT("/Game/Audio/Vehicles/BellHuey/SFX_BellHuey_EngineStart.SFX_BellHuey_EngineStart");
    const TCHAR* EngineFlightLoopPath = TEXT("/Game/Audio/Vehicles/BellHuey/SC_BellHuey_EngineFlight_Loop.SC_BellHuey_EngineFlight_Loop");
    const TCHAR* MissionBriefPath = TEXT("/Game/Audio/Missions/Startup/VO_MissionBrief_Startup.VO_MissionBrief_Startup");
    const TCHAR* Mission1BriefPath = TEXT("/Game/Audio/Missions/Mission1/VO_M1_Startup.VO_M1_Startup");
    const TCHAR* RooftopExtractionMissionBriefPath = TEXT("/Game/Audio/Missions/RooftopExtraction/VO_M21_RooftopExtraction_Startup.VO_M21_RooftopExtraction_Startup");
    const TCHAR* EnemyFootholdMissionBriefPath = TEXT("/Game/Audio/Missions/EnemyFoothold/VO_M22_Startup.VO_M22_Startup");
    const TCHAR* CabinSupplyConvoyMissionBriefPath = TEXT("/Game/Audio/Missions/CabinSupplyConvoy/VO_M23_Startup.VO_M23_Startup");
    const TCHAR* SurvivorExtractionMissionBriefPath = TEXT("/Game/Audio/Missions/SurvivorExtraction/VO_M24_Startup.VO_M24_Startup");
    const TCHAR* FinalEvacuationMissionBriefPath = TEXT("/Game/Audio/Missions/FinalEvacuation/VO_M25_Startup.VO_M25_Startup");

    const TCHAR* MD500BodyPath = TEXT("/Game/Vehicles/Playable/MH6Superior/SM_MH6_Superior_Fuselage.SM_MH6_Superior_Fuselage");
    const TCHAR* MD500AccessoryPath = TEXT("/Game/Vehicles/Playable/MH6Superior/SM_MH6_Superior_Accessories.SM_MH6_Superior_Accessories");
    const TCHAR* MD500WeaponPath = TEXT("/Game/Vehicles/Playable/MH6Superior/SM_MH6_Superior_Weapons.SM_MH6_Superior_Weapons");
    const TCHAR* MD500CockpitPath = TEXT("/Game/Vehicles/Playable/MH6Superior/SM_MH6_Superior_Interior.SM_MH6_Superior_Interior");
    const TCHAR* MD500GlassPath = TEXT("/Game/Vehicles/Playable/MH6Superior/SM_MH6_Superior_Glass.SM_MH6_Superior_Glass");
    const TCHAR* MD500MainRotorMountPath = TEXT("/Game/Vehicles/Playable/MH6Superior/SM_MH6_Superior_MainRotorMount.SM_MH6_Superior_MainRotorMount");
    const TCHAR* MD500MainRotorRotatingPath = TEXT("/Game/Vehicles/Playable/MH6Superior/SM_MH6_Superior_MainRotorRotating.SM_MH6_Superior_MainRotorRotating");
    const TCHAR* MD500TailRotorMountPath = TEXT("/Game/Vehicles/Playable/MH6Superior/SM_MH6_Superior_TailRotorMount.SM_MH6_Superior_TailRotorMount");
    const TCHAR* MD500TailRotorRotatingPath = TEXT("/Game/Vehicles/Playable/MH6Superior/SM_MH6_Superior_TailRotorRotating.SM_MH6_Superior_TailRotorRotating");
    const TCHAR* MD500EngineStartupPath = TEXT("/Game/Audio/Vehicles/MD500/SFX_MD500_EngineStart.SFX_MD500_EngineStart");
    const TCHAR* MD500EngineTakeoffPath = TEXT("/Game/Audio/Vehicles/MD500/SFX_MD500_Takeoff.SFX_MD500_Takeoff");
    const TCHAR* MD500EngineFlightLoopPath = TEXT("/Game/Audio/Vehicles/MD500/SC_MD500_EngineInFlight_Loop.SC_MD500_EngineInFlight_Loop");
    const TCHAR* MusicCombatPath = TEXT("/Game/Audio/Music/MUS_Combat.MUS_Combat");
    const TCHAR* MusicMission1Path = TEXT("/Game/Audio/Music/MUS_Mission1.MUS_Mission1");
    const TCHAR* MusicMission2Path = TEXT("/Game/Audio/Music/MUS_Mission2.MUS_Mission2");
    const TCHAR* MusicMission3Path = TEXT("/Game/Audio/Music/MUS_Mission3.MUS_Mission3");
    const TCHAR* MusicRescuePath = TEXT("/Game/Audio/Music/MUS_Rescue.MUS_Rescue");
    const TCHAR* RotorSplashPath = TEXT("/Game/Audio/Music/Rotorline/SC_RotorSplash_Loop.SC_RotorSplash_Loop");
    const TCHAR* RotorImpactPath = TEXT("/Game/Audio/Music/Rotorline/SC_RotorImpact_Loop.SC_RotorImpact_Loop");
    const TCHAR* RotorClashPath = TEXT("/Game/Audio/Music/Rotorline/SC_RotorClash_Loop.SC_RotorClash_Loop");
    const TCHAR* RotorAnthemPath = TEXT("/Game/Audio/Music/Rotorline/SC_RotorAnthem_Loop.SC_RotorAnthem_Loop");
    const TCHAR* KiowaReconMissionMusicPath = TEXT("/Game/Audio/Missions/KiowaReconStrike/MidnightStalker.MidnightStalker");
    const TCHAR* Bell222MissionMusicPath = TEXT("/Game/Audio/Music/Bell222/SC_Bell222_SkyborneAssault_Loop.SC_Bell222_SkyborneAssault_Loop");
    const TCHAR* Bell222FinalMissionBriefPath = TEXT("/Game/Audio/Vehicles/Bell222X/VO_Bell222_TheFinalMission.VO_Bell222_TheFinalMission");
    const TCHAR* Bell222BoostSoundPath = TEXT("/Game/Audio/Vehicles/Bell222X/SFX_Bell222X_AfterburnerScream.SFX_Bell222X_AfterburnerScream");
    const TCHAR* Bell222CloakSoundPath = TEXT("/Game/Audio/Vehicles/Bell222X/Stealth/SFX_Bell222X_Cloak.SFX_Bell222X_Cloak");
    const TCHAR* Bell222DecloakSoundPath = TEXT("/Game/Audio/Vehicles/Bell222X/Stealth/SFX_Bell222X_Decloak.SFX_Bell222X_Decloak");
    const TCHAR* RadioSquelchPath = TEXT("/Game/Audio/Radio/SFX_RadioSquelch.SFX_RadioSquelch");
    const TCHAR* ThreatWarningPath = TEXT("/Game/Audio/Combat/SFX_MissileLockWarning.SFX_MissileLockWarning");
    const TCHAR* ThreatLockedPath = TEXT("/Game/Audio/Combat/SFX_MissileLockedOn.SFX_MissileLockedOn");
    const TCHAR* RadarHomingPath = TEXT("/Game/Audio/Combat/SFX_RadarHomingIn.SFX_RadarHomingIn");
    const TCHAR* RadarLockedPath = TEXT("/Game/Audio/Combat/SFX_RadarLocked.SFX_RadarLocked");
    // Imported by Scripts/ImportApacheAudio.py. Keeping the object path here
    // makes the playable weapon independent of the temporary enemy-gun audio.
    const TCHAR* ApacheCannonPath = TEXT("/Game/Audio/Vehicles/AH64/SFX_AH64_30mm_Autocannon.SFX_AH64_30mm_Autocannon");
}

ARotorlineHelicopterPawn::ARotorlineHelicopterPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    SetRootComponent(CollisionBox);
    CollisionBox->SetBoxExtent(FVector(410.0f, 100.0f, 110.0f));
    CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
    CollisionBox->SetGenerateOverlapEvents(false);

    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    VisualRoot->SetupAttachment(CollisionBox);

    MeshAlignment = CreateDefaultSubobject<USceneComponent>(TEXT("MeshAlignment"));
    MeshAlignment->SetupAttachment(VisualRoot);
    MeshAlignment->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));
    MeshAlignment->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(MeshAlignment);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GlassMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlassMesh"));
    GlassMesh->SetupAttachment(MeshAlignment);
    GlassMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PlayerExplosionCore = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerExplosionCore"));
    PlayerExplosionSmoke = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerExplosionSmoke"));
    PlayerExplosionSparks = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerExplosionSparks"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlayerExplosionSphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlayerExplosionHotFinder(TEXT("/Game/Missions/Presentation/M_ExplosionHot.M_ExplosionHot"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlayerExplosionSmokeFinder(TEXT("/Game/Missions/Presentation/M_RocketSmoke.M_RocketSmoke"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlayerExplosionSparksFinder(TEXT("/Game/Missions/Assets/UserProvided/WeaponFX/SparksExplosion/sparksexplosion/StaticMeshes/sparksexplosion.sparksexplosion"));
    for (UStaticMeshComponent* Effect : { PlayerExplosionCore.Get(), PlayerExplosionSmoke.Get(), PlayerExplosionSparks.Get() })
    {
        Effect->SetupAttachment(CollisionBox);
        Effect->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Effect->SetVisibility(false, true);
        if (PlayerExplosionSphereFinder.Succeeded()) Effect->SetStaticMesh(PlayerExplosionSphereFinder.Object);
        if (PlayerExplosionHotFinder.Succeeded()) Effect->SetMaterial(0, PlayerExplosionHotFinder.Object);
    }
    if (PlayerExplosionSmokeFinder.Succeeded()) PlayerExplosionSmoke->SetMaterial(0, PlayerExplosionSmokeFinder.Object);
    if (PlayerExplosionSparksFinder.Succeeded()) PlayerExplosionSparks->SetStaticMesh(PlayerExplosionSparksFinder.Object);

    PlayerExplosionLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PlayerExplosionLight"));
    PlayerExplosionLight->SetupAttachment(CollisionBox);
    PlayerExplosionLight->SetLightColor(FLinearColor(1.0f, 0.12f, 0.01f));
    PlayerExplosionLight->SetIntensity(0.0f);
    PlayerExplosionLight->SetAttenuationRadius(4200.0f);
    static ConstructorHelpers::FObjectFinder<USoundBase> PlayerExplosionSoundFinder(TEXT("/Game/Audio/Combat/SFX_MissileImpact.SFX_MissileImpact"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PlayerAirExplosionFinder(
        TEXT("/Game/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Air_Explosion_01.Niagara_Air_Explosion_01"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PlayerGroundExplosionFinder(
        TEXT("/Game/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Explosion_05.Niagara_Explosion_05"));
    if (PlayerExplosionSoundFinder.Succeeded()) PlayerExplosionSound = PlayerExplosionSoundFinder.Object;
    if (PlayerAirExplosionFinder.Succeeded()) PlayerAirExplosionSystem = PlayerAirExplosionFinder.Object;
    if (PlayerGroundExplosionFinder.Succeeded()) PlayerGroundExplosionSystem = PlayerGroundExplosionFinder.Object;

    // Every selectable airframe shares collision-root light stations. Their
    // final positions are fitted to the selected airframe bounds after its
    // craft configuration is applied, so the bulbs sit on the body instead of
    // floating beside differently sized catalog meshes.
    LeftNavigationLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LeftNavigationLight"));
    RightNavigationLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RightNavigationLight"));
    TailStrobeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TailStrobeLight"));
    LeftNavigationLight->SetupAttachment(CollisionBox);
    RightNavigationLight->SetupAttachment(CollisionBox);
    TailStrobeLight->SetupAttachment(CollisionBox);
    LeftNavigationLight->SetRelativeLocation(FVector(35.0f, -210.0f, 45.0f));
    RightNavigationLight->SetRelativeLocation(FVector(35.0f, 210.0f, 45.0f));
    TailStrobeLight->SetRelativeLocation(FVector(-390.0f, 0.0f, 70.0f));
    LeftNavigationLight->SetLightColor(FLinearColor(1.0f, 0.015f, 0.01f));
    RightNavigationLight->SetLightColor(FLinearColor(0.015f, 1.0f, 0.08f));
    TailStrobeLight->SetLightColor(FLinearColor(0.78f, 0.90f, 1.0f));
    for (UPointLightComponent* NavigationLight : {
        LeftNavigationLight.Get(), RightNavigationLight.Get(), TailStrobeLight.Get() })
    {
        NavigationLight->SetAttenuationRadius(4600.0f);
        NavigationLight->SetSourceRadius(12.0f);
        NavigationLight->SetCastShadows(false);
        NavigationLight->SetIntensity(0.0f);
        NavigationLight->SetVisibility(false, true);
    }

    LandingLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("LandingLight"));
    LandingLight->SetupAttachment(CollisionBox);
    LandingLight->SetRelativeLocation(FVector(360.0f, 0.0f, -30.0f));
    LandingLight->SetRelativeRotation(FRotator(-14.0f, 0.0f, 0.0f));
    LandingLight->SetLightColor(FLinearColor(0.82f, 0.91f, 1.0f));
    LandingLight->SetIntensity(0.0f);
    LandingLight->SetAttenuationRadius(12000.0f);
    LandingLight->SetInnerConeAngle(18.0f);
    LandingLight->SetOuterConeAngle(34.0f);
    LandingLight->SetCastShadows(true);
    LandingLight->SetVisibility(false, true);

    LeftNavigationBulb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftNavigationBulb"));
    RightNavigationBulb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightNavigationBulb"));
    TailStrobeBulb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TailStrobeBulb"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> NavigationBulbSphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> NavigationRedFinder(TEXT("/Game/Missions/Presentation/M_TargetRedGlow.M_TargetRedGlow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> NavigationGreenFinder(TEXT("/Game/Missions/Presentation/M_SuccessGreenGlow.M_SuccessGreenGlow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> NavigationWhiteFinder(TEXT("/Game/Missions/Presentation/M_ObjectiveAmberGlow.M_ObjectiveAmberGlow"));
    for (UStaticMeshComponent* Bulb : {
        LeftNavigationBulb.Get(), RightNavigationBulb.Get(), TailStrobeBulb.Get() })
    {
        Bulb->SetupAttachment(CollisionBox);
        Bulb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Bulb->SetCastShadow(false);
        Bulb->SetRelativeScale3D(FVector(0.075f));
        Bulb->SetVisibility(false, true);
        if (NavigationBulbSphereFinder.Succeeded()) Bulb->SetStaticMesh(NavigationBulbSphereFinder.Object);
    }
    LeftNavigationBulb->SetRelativeLocation(LeftNavigationLight->GetRelativeLocation());
    RightNavigationBulb->SetRelativeLocation(RightNavigationLight->GetRelativeLocation());
    TailStrobeBulb->SetRelativeLocation(TailStrobeLight->GetRelativeLocation());
    if (NavigationRedFinder.Succeeded()) LeftNavigationBulb->SetMaterial(0, NavigationRedFinder.Object);
    if (NavigationGreenFinder.Succeeded()) RightNavigationBulb->SetMaterial(0, NavigationGreenFinder.Object);
    if (NavigationWhiteFinder.Succeeded()) TailStrobeBulb->SetMaterial(0, NavigationWhiteFinder.Object);

    // Twin boost exhaust stations unique to the Bell 222 X. Keeping these on
    // the collision root avoids inheriting the imported model's axis rotation.
    Bell222LeftBoostPlume = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bell222LeftBoostPlume"));
    Bell222RightBoostPlume = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bell222RightBoostPlume"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BoostConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BoostFlameFinder(TEXT("/Game/Missions/Presentation/M_RocketFlameGlow.M_RocketFlameGlow"));
    for (UStaticMeshComponent* Plume : { Bell222LeftBoostPlume.Get(), Bell222RightBoostPlume.Get() })
    {
        Plume->SetupAttachment(MeshAlignment);
        Plume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Plume->SetCastShadow(false);
        Plume->SetVisibility(false, true);
        Plume->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
        if (BoostConeFinder.Succeeded()) Plume->SetStaticMesh(BoostConeFinder.Object);
        if (BoostFlameFinder.Succeeded()) Plume->SetMaterial(0, BoostFlameFinder.Object);
    }
    // Source-mesh exhaust centers are the symmetric nacelle components near
    // X=2.2 m, Y=+/-0.48 m, Z=0.78 m. Mount in imported-mesh space so model
    // scale/alignment changes cannot separate the flames from the ports.
    Bell222LeftBoostPlume->SetRelativeLocation(FVector(255.0f, -48.0f, 78.0f));
    Bell222RightBoostPlume->SetRelativeLocation(FVector(255.0f, 48.0f, 78.0f));

    Bell222LeftBoostLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Bell222LeftBoostLight"));
    Bell222RightBoostLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Bell222RightBoostLight"));
    for (UPointLightComponent* BoostLight : { Bell222LeftBoostLight.Get(), Bell222RightBoostLight.Get() })
    {
        BoostLight->SetupAttachment(MeshAlignment);
        BoostLight->SetLightColor(FLinearColor(1.0f, 0.24f, 0.025f));
        BoostLight->SetIntensity(0.0f);
        BoostLight->SetAttenuationRadius(650.0f);
        BoostLight->SetSourceRadius(8.0f);
        BoostLight->SetCastShadows(false);
        BoostLight->SetVisibility(false, true);
    }
    Bell222LeftBoostLight->SetRelativeLocation(FVector(220.0f, -48.0f, 78.0f));
    Bell222RightBoostLight->SetRelativeLocation(FVector(220.0f, 48.0f, 78.0f));

    // Bell 222 fictional special-operations conversion. All mounts are modular
    // child components, leaving the licensed source mesh untouched.
    Bell222LeftGunRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Bell222LeftGunRoot"));
    Bell222RightGunRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Bell222RightGunRoot"));
    Bell222LeftGunRoot->SetupAttachment(VisualRoot);
    Bell222RightGunRoot->SetupAttachment(VisualRoot);
    Bell222LeftGunRoot->SetRelativeLocation(FVector(20.0f, -105.0f, -40.0f));
    Bell222RightGunRoot->SetRelativeLocation(FVector(20.0f, 105.0f, -40.0f));

    // Source-authored open sponson doors and gun assemblies. These retain the
    // actual Bell model geometry instead of displaying procedural stand-ins.
    Bell222LeftSourceGun = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bell222LeftSourceGun"));
    Bell222RightSourceGun = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bell222RightSourceGun"));
    for (UStaticMeshComponent* SourceGun : { Bell222LeftSourceGun.Get(), Bell222RightSourceGun.Get() })
    {
        SourceGun->SetupAttachment(MeshAlignment);
        SourceGun->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SourceGun->SetCastShadow(true);
        SourceGun->SetVisibility(false, true);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BellWeaponCubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BellWeaponCylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BellWeaponSphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BellWeaponDarkFinder(TEXT("/Game/Environment/Materials/Urban/M_Urban_Metal.M_Urban_Metal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BellWeaponGlowFinder(TEXT("/Game/Missions/Presentation/M_RocketFlameGlow.M_RocketFlameGlow"));

    // The source model's gear node includes long presentation outriggers that
    // sit well outside the Bell's body. Use a compact tricycle assembly for
    // gameplay instead, while retaining the automatic retract sequence.
    const FVector BellGearLocations[] = {
        FVector(-12.0f, -34.0f, -80.0f),
        FVector(-12.0f, 34.0f, -80.0f),
        FVector(136.0f, 0.0f, -75.0f),
    };
    for (int32 GearIndex = 0; GearIndex < UE_ARRAY_COUNT(BellGearLocations); ++GearIndex)
    {
        UStaticMeshComponent* GearPart = CreateDefaultSubobject<UStaticMeshComponent>(
            *FString::Printf(TEXT("Bell222CompactGear_%d"), GearIndex));
        GearPart->SetupAttachment(VisualRoot);
        GearPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GearPart->SetCastShadow(true);
        GearPart->SetVisibility(false, true);
        GearPart->SetRelativeLocation(BellGearLocations[GearIndex]);
        GearPart->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
        GearPart->SetRelativeScale3D(GearIndex == 2
            ? FVector(0.10f, 0.10f, 0.065f)
            : FVector(0.135f, 0.135f, 0.080f));
        if (BellWeaponCylinderFinder.Succeeded()) GearPart->SetStaticMesh(BellWeaponCylinderFinder.Object);
        if (BellWeaponDarkFinder.Succeeded()) GearPart->SetMaterial(0, BellWeaponDarkFinder.Object);
        Bell222CompactGearParts.Add(GearPart);
        Bell222CompactGearBaseLocations.Add(BellGearLocations[GearIndex]);
    }

    for (int32 Side = 0; Side < 2; ++Side)
    {
        USceneComponent* SideRoot = Side == 0 ? Bell222LeftGunRoot.Get() : Bell222RightGunRoot.Get();
        const FName HousingName(*FString::Printf(TEXT("Bell222GunHousing_%d"), Side));
        UStaticMeshComponent* Housing = CreateDefaultSubobject<UStaticMeshComponent>(HousingName);
        Housing->SetupAttachment(SideRoot);
        Housing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Housing->SetCastShadow(true);
        Housing->SetVisibility(false, true);
        Housing->SetRelativeLocation(FVector(18.0f, 0.0f, 0.0f));
        Housing->SetRelativeScale3D(FVector(0.82f, 0.76f, 0.22f));
        if (BellWeaponCubeFinder.Succeeded()) Housing->SetStaticMesh(BellWeaponCubeFinder.Object);
        if (BellWeaponDarkFinder.Succeeded()) Housing->SetMaterial(0, BellWeaponDarkFinder.Object);
        Bell222GunHousings.Add(Housing);

        for (int32 DoorIndex = 0; DoorIndex < 2; ++DoorIndex)
        {
            const FName DoorName(*FString::Printf(TEXT("Bell222GunDoor_%d_%d"), Side, DoorIndex));
            UStaticMeshComponent* Door = CreateDefaultSubobject<UStaticMeshComponent>(DoorName);
            Door->SetupAttachment(SideRoot);
            Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Door->SetCastShadow(true);
            Door->SetVisibility(false, true);
            Door->SetRelativeLocation(FVector(8.0f, 0.0f, DoorIndex == 0 ? 10.0f : -10.0f));
            Door->SetRelativeScale3D(FVector(0.70f, 0.55f, 0.10f));
            if (BellWeaponCubeFinder.Succeeded()) Door->SetStaticMesh(BellWeaponCubeFinder.Object);
            if (BellWeaponDarkFinder.Succeeded()) Door->SetMaterial(0, BellWeaponDarkFinder.Object);
            Bell222GunDoors.Add(Door);
        }

        for (int32 BarrelIndex = 0; BarrelIndex < 3; ++BarrelIndex)
        {
            const bool bHeavy = BarrelIndex == 2;
            const FName BarrelName(*FString::Printf(TEXT("Bell222GunBarrel_%d_%d"), Side, BarrelIndex));
            UStaticMeshComponent* Barrel = CreateDefaultSubobject<UStaticMeshComponent>(BarrelName);
            Barrel->SetupAttachment(SideRoot);
            Barrel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Barrel->SetCastShadow(true);
            Barrel->SetVisibility(false, true);
            Barrel->SetRelativeLocation(FVector(60.0f, (BarrelIndex == 0 ? -13.0f : (BarrelIndex == 1 ? 13.0f : 0.0f)), bHeavy ? -18.0f : 12.0f));
            Barrel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
            Barrel->SetRelativeScale3D(bHeavy ? FVector(0.105f, 0.105f, 1.05f) : FVector(0.060f, 0.060f, 0.88f));
            if (BellWeaponCylinderFinder.Succeeded()) Barrel->SetStaticMesh(BellWeaponCylinderFinder.Object);
            if (BellWeaponDarkFinder.Succeeded()) Barrel->SetMaterial(0, BellWeaponDarkFinder.Object);
            Bell222GunBarrels.Add(Barrel);

            const FName FlashName(*FString::Printf(TEXT("Bell222MuzzleFlash_%d_%d"), Side, BarrelIndex));
            UStaticMeshComponent* Flash = CreateDefaultSubobject<UStaticMeshComponent>(FlashName);
            Flash->SetupAttachment(SideRoot);
            Flash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Flash->SetCastShadow(false);
            Flash->SetVisibility(false, true);
            Flash->SetRelativeLocation(FVector(bHeavy ? 118.0f : 108.0f, Barrel->GetRelativeLocation().Y, Barrel->GetRelativeLocation().Z));
            Flash->SetRelativeScale3D(bHeavy ? FVector(0.17f) : FVector(0.10f));
            if (BellWeaponSphereFinder.Succeeded()) Flash->SetStaticMesh(BellWeaponSphereFinder.Object);
            if (BellWeaponGlowFinder.Succeeded()) Flash->SetMaterial(0, BellWeaponGlowFinder.Object);
            Bell222MuzzleFlashes.Add(Flash);
            Bell222MuzzleFlashUntil.Add(-1000.0);
        }
    }

    Bell222MissilePodRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Bell222MissilePodRoot"));
    Bell222MissilePodRoot->SetupAttachment(VisualRoot);
    Bell222MissilePodRoot->SetRelativeLocation(FVector(-5.0f, 0.0f, -52.0f));
    for (int32 DoorIndex = 0; DoorIndex < 2; ++DoorIndex)
    {
        const FName DoorName(*FString::Printf(TEXT("Bell222PodDoor_%d"), DoorIndex));
        UStaticMeshComponent* Door = CreateDefaultSubobject<UStaticMeshComponent>(DoorName);
        Door->SetupAttachment(Bell222MissilePodRoot);
        Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Door->SetCastShadow(true);
        Door->SetVisibility(false, true);
        Door->SetRelativeLocation(FVector(0.0f, DoorIndex == 0 ? -34.0f : 34.0f, 4.0f));
        Door->SetRelativeScale3D(FVector(0.62f, 0.28f, 0.035f));
        if (BellWeaponCubeFinder.Succeeded()) Door->SetStaticMesh(BellWeaponCubeFinder.Object);
        if (BellWeaponDarkFinder.Succeeded()) Door->SetMaterial(0, BellWeaponDarkFinder.Object);
        Bell222MissilePodDoors.Add(Door);
    }
    Bell222MissilePodBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bell222MissilePodBody"));
    Bell222MissilePodBody->SetupAttachment(Bell222MissilePodRoot);
    Bell222MissilePodBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bell222MissilePodBody->SetCastShadow(true);
    Bell222MissilePodBody->SetVisibility(false, true);
    Bell222MissilePodBody->SetRelativeLocation(FVector(0.0f, 0.0f, -24.0f));
    Bell222MissilePodBody->SetRelativeScale3D(FVector(0.38f, 0.46f, 0.10f));
    if (BellWeaponCubeFinder.Succeeded()) Bell222MissilePodBody->SetStaticMesh(BellWeaponCubeFinder.Object);
    if (BellWeaponDarkFinder.Succeeded()) Bell222MissilePodBody->SetMaterial(0, BellWeaponDarkFinder.Object);
    for (int32 MissileIndex = 0; MissileIndex < 4; ++MissileIndex)
    {
        const FName MissileName(*FString::Printf(TEXT("Bell222PodMissile_%d"), MissileIndex));
        UStaticMeshComponent* Missile = CreateDefaultSubobject<UStaticMeshComponent>(MissileName);
        Missile->SetupAttachment(Bell222MissilePodRoot);
        Missile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Missile->SetCastShadow(true);
        Missile->SetVisibility(false, true);
        Missile->SetRelativeLocation(FVector(30.0f, (MissileIndex - 1.5f) * 15.0f, -34.0f));
        Missile->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
        Missile->SetRelativeScale3D(FVector(0.055f, 0.055f, 0.62f));
        if (BellWeaponCylinderFinder.Succeeded()) Missile->SetStaticMesh(BellWeaponCylinderFinder.Object);
        if (BellWeaponDarkFinder.Succeeded()) Missile->SetMaterial(0, BellWeaponDarkFinder.Object);
        Bell222PodMissiles.Add(Missile);
    }

    DownwashGroundSheet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DownwashGroundSheet"));
    DownwashGroundSheetSecondary = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DownwashGroundSheetSecondary"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DownwashPlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DownwashGroundFinder(TEXT("/Game/Effects/RotorDownwash/M_RotorDownwashGround.M_RotorDownwashGround"));
    for (UStaticMeshComponent* Sheet : { DownwashGroundSheet.Get(), DownwashGroundSheetSecondary.Get() })
    {
        Sheet->SetupAttachment(CollisionBox);
        Sheet->SetAbsolute(true, true, true);
        Sheet->SetMobility(EComponentMobility::Movable);
        Sheet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Sheet->SetCastShadow(false);
        Sheet->SetTranslucentSortPriority(20);
        Sheet->SetBoundsScale(4.0f);
        Sheet->SetVisibility(false, true);
        if (DownwashPlaneFinder.Succeeded()) Sheet->SetStaticMesh(DownwashPlaneFinder.Object);
        if (DownwashGroundFinder.Succeeded()) Sheet->SetMaterial(0, DownwashGroundFinder.Object);
    }

    InteriorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InteriorMesh"));
    InteriorMesh->SetupAttachment(MeshAlignment);
    InteriorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // The standard Huey's imported rotor meshes use aircraft-space vertices.
    // Center dedicated pivots on the actual shafts so they can use the same
    // frame-continuous component rotation as the Marine Huey.
    HueyMainRotorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HueyMainRotorPivot"));
    HueyMainRotorPivot->SetupAttachment(MeshAlignment);
    HueyMainRotorPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 204.1f));

    MainRotorMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MainRotorMesh"));
    MainRotorMesh->SetupAttachment(HueyMainRotorPivot);
    MainRotorMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -204.1f));
    MainRotorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    HueyTailRotorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HueyTailRotorPivot"));
    HueyTailRotorPivot->SetupAttachment(MeshAlignment);
    HueyTailRotorPivot->SetRelativeLocation(FVector(16.9f, -555.8f, 193.7f));

    TailRotorMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TailRotorMesh"));
    TailRotorMesh->SetupAttachment(HueyTailRotorPivot);
    TailRotorMesh->SetRelativeLocation(FVector(-16.9f, 555.8f, -193.7f));
    TailRotorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    MD500SuperiorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MD500SuperiorRoot"));
    MD500SuperiorRoot->SetupAttachment(MeshAlignment);

    MD500BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MD500BodyMesh"));
    MD500BodyMesh->SetupAttachment(MD500SuperiorRoot);
    MD500BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MD500BodyMesh->SetVisibility(false, true);

    MD500AccessoryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MD500AccessoryMesh"));
    MD500AccessoryMesh->SetupAttachment(MD500SuperiorRoot);
    MD500AccessoryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MD500AccessoryMesh->SetVisibility(false, true);

    MD500WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MD500WeaponMesh"));
    MD500WeaponMesh->SetupAttachment(MD500SuperiorRoot);
    MD500WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MD500WeaponMesh->SetVisibility(false, true);

    // These stations come from the supplied model's actual weapon geometry,
    // not the pawn collision box.  The paired guns are the inboard assemblies
    // nearest the fuselage; the rocket/missile pods are the outboard stations.
    // Coordinates remain in the same model-native space as every superior
    // MH-6 mesh, so the shared root keeps them seated through all alignment.
    MD500LeftGunMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("MD500LeftGunMuzzle"));
    MD500RightGunMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("MD500RightGunMuzzle"));
    MD500LeftRocketMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("MD500LeftRocketMuzzle"));
    MD500RightRocketMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("MD500RightRocketMuzzle"));
    MD500LeftGunMuzzle->SetupAttachment(MD500SuperiorRoot);
    MD500RightGunMuzzle->SetupAttachment(MD500SuperiorRoot);
    MD500LeftRocketMuzzle->SetupAttachment(MD500SuperiorRoot);
    MD500RightRocketMuzzle->SetupAttachment(MD500SuperiorRoot);
    MD500LeftGunMuzzle->SetRelativeLocation(FVector(59.5f, -21.16f, -81.10f));
    MD500RightGunMuzzle->SetRelativeLocation(FVector(59.5f, 21.37f, -81.10f));
    MD500LeftRocketMuzzle->SetRelativeLocation(FVector(54.8f, -33.91f, -85.60f));
    MD500RightRocketMuzzle->SetRelativeLocation(FVector(54.8f, 34.12f, -85.60f));

    MD500CockpitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MD500CockpitMesh"));
    MD500CockpitMesh->SetupAttachment(MD500SuperiorRoot);
    MD500CockpitMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MD500CockpitMesh->SetVisibility(false, true);

    MD500GlassMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MD500GlassMesh"));
    MD500GlassMesh->SetupAttachment(MD500SuperiorRoot);
    MD500GlassMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MD500GlassMesh->SetVisibility(false, true);

    MD500MainRotorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MD500MainRotorPivot"));
    MD500MainRotorPivot->SetupAttachment(MD500SuperiorRoot);
    MD500MainRotorPivot->SetRelativeLocation(FVector(32.9767f, 0.0850f, -40.2828f));

    MD500MainRotorMountMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MD500MainRotorMountMesh"));
    MD500MainRotorMountMesh->SetupAttachment(MD500SuperiorRoot);
    MD500MainRotorMountMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MD500MainRotorMountMesh->SetVisibility(false, true);

    MD500MainRotorRotatingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MD500MainRotorRotatingMesh"));
    MD500MainRotorRotatingMesh->SetupAttachment(MD500MainRotorPivot);
    MD500MainRotorRotatingMesh->SetRelativeLocation(FVector(-32.9767f, -0.0850f, 40.2828f));
    MD500MainRotorRotatingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MD500MainRotorRotatingMesh->SetVisibility(false, true);

    MD500TailRotorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MD500TailRotorPivot"));
    MD500TailRotorPivot->SetupAttachment(MD500SuperiorRoot);
    MD500TailRotorPivot->SetRelativeLocation(FVector(-79.0857f, -5.4154f, -62.2113f));

    MD500TailRotorMountMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MD500TailRotorMountMesh"));
    MD500TailRotorMountMesh->SetupAttachment(MD500SuperiorRoot);
    MD500TailRotorMountMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MD500TailRotorMountMesh->SetVisibility(false, true);

    MD500TailRotorRotatingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MD500TailRotorRotatingMesh"));
    MD500TailRotorRotatingMesh->SetupAttachment(MD500TailRotorPivot);
    MD500TailRotorRotatingMesh->SetRelativeLocation(FVector(79.0857f, 5.4154f, 62.2113f));
    MD500TailRotorRotatingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MD500TailRotorRotatingMesh->SetVisibility(false, true);

    CatalogBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CatalogBodyMesh"));
    CatalogBodyMesh->SetupAttachment(MeshAlignment);
    CatalogBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CatalogBodyMesh->SetVisibility(false, true);

    CatalogSkeletalBodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CatalogSkeletalBodyMesh"));
    CatalogSkeletalBodyMesh->SetupAttachment(MeshAlignment);
    CatalogSkeletalBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CatalogSkeletalBodyMesh->SetVisibility(false, true);

    for (int32 Index = 0; Index < 3; ++Index)
    {
        UStaticMeshComponent* StaticRotor = CreateDefaultSubobject<UStaticMeshComponent>(
            *FString::Printf(TEXT("CatalogStaticRotor%d"), Index));
        StaticRotor->SetupAttachment(MeshAlignment);
        StaticRotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        StaticRotor->SetVisibility(false, true);
        CatalogStaticRotors.Add(StaticRotor);

        USkeletalMeshComponent* SkeletalRotor = CreateDefaultSubobject<USkeletalMeshComponent>(
            *FString::Printf(TEXT("CatalogSkeletalRotor%d"), Index));
        SkeletalRotor->SetupAttachment(MeshAlignment);
        SkeletalRotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SkeletalRotor->SetVisibility(false, true);
        CatalogSkeletalRotors.Add(SkeletalRotor);
    }

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(CollisionBox);
    SpringArm->TargetArmLength = 1150.0f;
    SpringArm->SocketOffset = FVector(0.0f, 0.0f, 230.0f);
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bEnableCameraLag = true;
    SpringArm->CameraLagSpeed = 5.0f;
    SpringArm->CameraLagMaxDistance = 300.0f;
    SpringArm->bEnableCameraRotationLag = true;
    SpringArm->CameraRotationLagSpeed = 7.0f;
    SpringArm->bDoCollisionTest = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->SetFieldOfView(88.0f);

    MissionFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("MissionFog"));
    MissionFog->SetupAttachment(CollisionBox);
    MissionFog->SetAbsolute(true, false, false);
    MissionFog->SetVisibility(false, true);
    MissionFog->SetFogDensity(0.0f);
    MissionFog->SetFogHeightFalloff(0.16f);
    MissionFog->SetFogMaxOpacity(0.88f);
    MissionFog->SetStartDistance(1200.0f);
    MissionFog->SetVolumetricFog(false);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> RainSystemFinder(
        TEXT("/Game/FX/Weather/NS_RotorlineRain.NS_RotorlineRain"));
    if (RainSystemFinder.Succeeded())
    {
        RainPrecipitationSystem = RainSystemFinder.Object;
    }

    KiowaLeftMinigun = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KiowaLeftMinigun"));
    KiowaRightRocketPod = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KiowaRightRocketPod"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> KiowaGunFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> KiowaPodFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> KiowaWeaponMaterialFinder(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    KiowaLeftMinigun->SetupAttachment(VisualRoot);
    KiowaLeftMinigun->SetRelativeLocation(FVector(60.0f, -105.0f, -50.0f));
    KiowaLeftMinigun->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    KiowaLeftMinigun->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.72f));
    KiowaRightRocketPod->SetupAttachment(VisualRoot);
    KiowaRightRocketPod->SetRelativeLocation(FVector(55.0f, 105.0f, -48.0f));
    KiowaRightRocketPod->SetRelativeScale3D(FVector(0.52f, 0.26f, 0.24f));
    for (UStaticMeshComponent* WeaponPart : { KiowaLeftMinigun.Get(), KiowaRightRocketPod.Get() })
    {
        WeaponPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WeaponPart->SetCastShadow(true);
        WeaponPart->SetVisibility(false, true);
        if (KiowaWeaponMaterialFinder.Succeeded())
        {
            WeaponPart->SetMaterial(0, KiowaWeaponMaterialFinder.Object);
        }
    }
    if (KiowaGunFinder.Succeeded()) KiowaLeftMinigun->SetStaticMesh(KiowaGunFinder.Object);
    if (KiowaPodFinder.Succeeded()) KiowaRightRocketPod->SetStaticMesh(KiowaPodFinder.Object);

    EngineStartupAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineStartupAudio"));
    EngineStartupAudio->SetupAttachment(CollisionBox);
    EngineStartupAudio->bAutoActivate = false;
    EngineStartupAudio->bAllowSpatialization = false;
    EngineStartupAudio->SetVolumeMultiplier(EngineStartupVolume);

    EngineTakeoffAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineTakeoffAudio"));
    EngineTakeoffAudio->SetupAttachment(CollisionBox);
    EngineTakeoffAudio->bAutoActivate = false;
    EngineTakeoffAudio->bAllowSpatialization = false;
    EngineTakeoffAudio->SetVolumeMultiplier(EngineFlightVolume);

    EngineFlightAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineFlightAudio"));
    EngineFlightAudio->SetupAttachment(CollisionBox);
    EngineFlightAudio->bAutoActivate = false;
    EngineFlightAudio->bAllowSpatialization = false;
    EngineFlightAudio->SetVolumeMultiplier(EngineFlightVolume);

    Bell222BoostAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("Bell222BoostAudio"));
    Bell222BoostAudio->SetupAttachment(CollisionBox);
    Bell222BoostAudio->bAutoActivate = false;
    Bell222BoostAudio->bAllowSpatialization = false;
    Bell222BoostAudio->SetVolumeMultiplier(0.72f);

    Bell222StealthAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("Bell222StealthAudio"));
    Bell222StealthAudio->SetupAttachment(CollisionBox);
    Bell222StealthAudio->bAutoActivate = false;
    Bell222StealthAudio->bAllowSpatialization = false;
    Bell222StealthAudio->SetVolumeMultiplier(0.72f);

    MissionBriefAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MissionBriefAudio"));
    MissionBriefAudio->SetupAttachment(CollisionBox);
    MissionBriefAudio->bAutoActivate = false;
    MissionBriefAudio->bAllowSpatialization = false;
    MissionBriefAudio->SetVolumeMultiplier(MissionBriefVolume);

    InstructorAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("InstructorAudio"));
    InstructorAudio->SetupAttachment(CollisionBox);
    InstructorAudio->bAutoActivate = false;
    InstructorAudio->bAllowSpatialization = false;
    InstructorAudio->bIsUISound = true;
    InstructorAudio->SetVolumeMultiplier(0.92f);

    MissionMusicAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MissionMusicAudio"));
    MissionMusicAudio->SetupAttachment(CollisionBox);
    MissionMusicAudio->bAutoActivate = false;
    MissionMusicAudio->bAllowSpatialization = false;
    MissionMusicAudio->bIsUISound = true;
    MissionMusicAudio->SetVolumeMultiplier(MissionMusicVolume);

    RadioAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RadioAudio"));
    RadioAudio->SetupAttachment(CollisionBox);
    RadioAudio->bAutoActivate = false;
    RadioAudio->bAllowSpatialization = false;
    RadioAudio->bIsUISound = true;
    RadioAudio->SetVolumeMultiplier(0.78f);

    RadioSquelchAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RadioSquelchAudio"));
    RadioSquelchAudio->SetupAttachment(CollisionBox);
    RadioSquelchAudio->bAutoActivate = false;
    RadioSquelchAudio->bAllowSpatialization = false;
    RadioSquelchAudio->bIsUISound = true;
    RadioSquelchAudio->SetVolumeMultiplier(0.78f);

    ThreatAlertAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ThreatAlertAudio"));
    ThreatAlertAudio->SetupAttachment(CollisionBox);
    ThreatAlertAudio->bAutoActivate = false;
    ThreatAlertAudio->bAllowSpatialization = false;
    ThreatAlertAudio->bIsUISound = true;
    ThreatAlertAudio->SetVolumeMultiplier(0.78f);

    ApacheCannonAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ApacheCannonAudio"));
    ApacheCannonAudio->SetupAttachment(CollisionBox);
    ApacheCannonAudio->bAutoActivate = false;
    ApacheCannonAudio->bAllowSpatialization = false;
    ApacheCannonAudio->SetVolumeMultiplier(0.72f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyFinder(RotorlineHelicopter::BodyPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GlassFinder(RotorlineHelicopter::GlassPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> InteriorFinder(RotorlineHelicopter::InteriorPath);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MainRotorFinder(RotorlineHelicopter::MainRotorPath);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> TailRotorFinder(RotorlineHelicopter::TailRotorPath);
    static ConstructorHelpers::FObjectFinder<UAnimSequence> AnimationFinder(RotorlineHelicopter::RotorAnimationPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> EngineStartupFinder(RotorlineHelicopter::EngineStartupPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> EngineFlightLoopFinder(RotorlineHelicopter::EngineFlightLoopPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> MissionBriefFinder(RotorlineHelicopter::MissionBriefPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> Mission1BriefFinder(RotorlineHelicopter::Mission1BriefPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MD500BodyFinder(RotorlineHelicopter::MD500BodyPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MD500AccessoryFinder(RotorlineHelicopter::MD500AccessoryPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MD500WeaponFinder(RotorlineHelicopter::MD500WeaponPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MD500CockpitFinder(RotorlineHelicopter::MD500CockpitPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MD500GlassFinder(RotorlineHelicopter::MD500GlassPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MD500MainRotorMountFinder(RotorlineHelicopter::MD500MainRotorMountPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MD500MainRotorRotatingFinder(RotorlineHelicopter::MD500MainRotorRotatingPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MD500TailRotorMountFinder(RotorlineHelicopter::MD500TailRotorMountPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MD500TailRotorRotatingFinder(RotorlineHelicopter::MD500TailRotorRotatingPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> MD500EngineStartupFinder(RotorlineHelicopter::MD500EngineStartupPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> MD500EngineTakeoffFinder(RotorlineHelicopter::MD500EngineTakeoffPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> MD500EngineFlightLoopFinder(RotorlineHelicopter::MD500EngineFlightLoopPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> MusicCombatFinder(RotorlineHelicopter::MusicCombatPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> MusicMission1Finder(RotorlineHelicopter::MusicMission1Path);
    static ConstructorHelpers::FObjectFinder<USoundBase> MusicMission2Finder(RotorlineHelicopter::MusicMission2Path);
    static ConstructorHelpers::FObjectFinder<USoundBase> MusicMission3Finder(RotorlineHelicopter::MusicMission3Path);
    static ConstructorHelpers::FObjectFinder<USoundBase> MusicRescueFinder(RotorlineHelicopter::MusicRescuePath);
    static ConstructorHelpers::FObjectFinder<USoundBase> RotorSplashFinder(RotorlineHelicopter::RotorSplashPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> RotorImpactFinder(RotorlineHelicopter::RotorImpactPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> RotorClashFinder(RotorlineHelicopter::RotorClashPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> RotorAnthemFinder(RotorlineHelicopter::RotorAnthemPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> KiowaReconMissionMusicFinder(RotorlineHelicopter::KiowaReconMissionMusicPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> Bell222MissionMusicFinder(RotorlineHelicopter::Bell222MissionMusicPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> Bell222FinalMissionBriefFinder(RotorlineHelicopter::Bell222FinalMissionBriefPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> Bell222BoostSoundFinder(RotorlineHelicopter::Bell222BoostSoundPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> Bell222CloakSoundFinder(RotorlineHelicopter::Bell222CloakSoundPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> Bell222DecloakSoundFinder(RotorlineHelicopter::Bell222DecloakSoundPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> RadioSquelchFinder(RotorlineHelicopter::RadioSquelchPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> ThreatWarningFinder(RotorlineHelicopter::ThreatWarningPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> ThreatLockedFinder(RotorlineHelicopter::ThreatLockedPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> RadarHomingFinder(RotorlineHelicopter::RadarHomingPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> RadarLockedFinder(RotorlineHelicopter::RadarLockedPath);
    static ConstructorHelpers::FObjectFinder<USoundBase> ApacheCannonFinder(RotorlineHelicopter::ApacheCannonPath);

    if (BodyFinder.Succeeded()) BodyMesh->SetStaticMesh(BodyFinder.Object);
    if (GlassFinder.Succeeded()) GlassMesh->SetStaticMesh(GlassFinder.Object);
    if (InteriorFinder.Succeeded()) InteriorMesh->SetStaticMesh(InteriorFinder.Object);
    if (MainRotorFinder.Succeeded()) MainRotorMesh->SetSkeletalMeshAsset(MainRotorFinder.Object);
    if (TailRotorFinder.Succeeded()) TailRotorMesh->SetSkeletalMeshAsset(TailRotorFinder.Object);
    if (AnimationFinder.Succeeded())
    {
        HueyRotorAnimation = AnimationFinder.Object;
        RotorAnimation = AnimationFinder.Object;
    }
    if (MD500BodyFinder.Succeeded()) MD500BodyMesh->SetStaticMesh(MD500BodyFinder.Object);
    if (MD500AccessoryFinder.Succeeded()) MD500AccessoryMesh->SetStaticMesh(MD500AccessoryFinder.Object);
    if (MD500WeaponFinder.Succeeded()) MD500WeaponMesh->SetStaticMesh(MD500WeaponFinder.Object);
    if (MD500CockpitFinder.Succeeded()) MD500CockpitMesh->SetStaticMesh(MD500CockpitFinder.Object);
    if (MD500GlassFinder.Succeeded()) MD500GlassMesh->SetStaticMesh(MD500GlassFinder.Object);
    if (MD500MainRotorMountFinder.Succeeded()) MD500MainRotorMountMesh->SetStaticMesh(MD500MainRotorMountFinder.Object);
    if (MD500MainRotorRotatingFinder.Succeeded()) MD500MainRotorRotatingMesh->SetStaticMesh(MD500MainRotorRotatingFinder.Object);
    if (MD500TailRotorMountFinder.Succeeded()) MD500TailRotorMountMesh->SetStaticMesh(MD500TailRotorMountFinder.Object);
    if (MD500TailRotorRotatingFinder.Succeeded()) MD500TailRotorRotatingMesh->SetStaticMesh(MD500TailRotorRotatingFinder.Object);
    if (EngineStartupFinder.Succeeded()) HueyEngineStartupSound = EngineStartupFinder.Object;
    if (EngineFlightLoopFinder.Succeeded()) HueyEngineFlightLoopSound = EngineFlightLoopFinder.Object;
    if (MissionBriefFinder.Succeeded()) HueyMissionBriefSound = MissionBriefFinder.Object;
    if (Mission1BriefFinder.Succeeded()) Mission1BriefSound = Mission1BriefFinder.Object;
    if (MD500EngineStartupFinder.Succeeded()) MD500EngineStartupSound = MD500EngineStartupFinder.Object;
    if (MD500EngineTakeoffFinder.Succeeded()) MD500EngineTakeoffSound = MD500EngineTakeoffFinder.Object;
    if (MD500EngineFlightLoopFinder.Succeeded()) MD500EngineFlightLoopSound = MD500EngineFlightLoopFinder.Object;
    if (MusicCombatFinder.Succeeded()) MusicCombatSound = MusicCombatFinder.Object;
    if (MusicMission1Finder.Succeeded()) MusicMission1Sound = MusicMission1Finder.Object;
    if (MusicMission2Finder.Succeeded()) MusicMission2Sound = MusicMission2Finder.Object;
    if (MusicMission3Finder.Succeeded()) MusicMission3Sound = MusicMission3Finder.Object;
    if (MusicRescueFinder.Succeeded()) MusicRescueSound = MusicRescueFinder.Object;
    if (RotorSplashFinder.Succeeded()) RotorlineGameplayMusic.Add(RotorSplashFinder.Object);
    if (RotorImpactFinder.Succeeded()) RotorlineGameplayMusic.Add(RotorImpactFinder.Object);
    if (RotorClashFinder.Succeeded()) RotorlineGameplayMusic.Add(RotorClashFinder.Object);
    if (RotorAnthemFinder.Succeeded()) RotorlineGameplayMusic.Add(RotorAnthemFinder.Object);
    if (KiowaReconMissionMusicFinder.Succeeded()) KiowaReconMissionMusicSound = KiowaReconMissionMusicFinder.Object;
    if (Bell222MissionMusicFinder.Succeeded()) Bell222MissionMusicSound = Bell222MissionMusicFinder.Object;
    if (Bell222FinalMissionBriefFinder.Succeeded()) Bell222FinalMissionBriefSound = Bell222FinalMissionBriefFinder.Object;
    if (Bell222BoostSoundFinder.Succeeded()) Bell222BoostSound = Bell222BoostSoundFinder.Object;
    if (Bell222CloakSoundFinder.Succeeded()) Bell222StealthSound = Bell222CloakSoundFinder.Object;
    if (Bell222DecloakSoundFinder.Succeeded()) Bell222DecloakSound = Bell222DecloakSoundFinder.Object;
    if (RadioSquelchFinder.Succeeded()) RadioSquelchSound = RadioSquelchFinder.Object;
    if (ThreatWarningFinder.Succeeded()) ThreatWarningSound = ThreatWarningFinder.Object;
    if (ThreatLockedFinder.Succeeded()) ThreatLockedSound = ThreatLockedFinder.Object;
    if (RadarHomingFinder.Succeeded()) RadarHomingSound = RadarHomingFinder.Object;
    if (RadarLockedFinder.Succeeded()) RadarLockedSound = RadarLockedFinder.Object;
    if (ApacheCannonFinder.Succeeded()) ApacheCannonSound = ApacheCannonFinder.Object;

    EngineStartupSound = HueyEngineStartupSound;
    EngineTakeoffSound = nullptr;
    EngineFlightLoopSound = HueyEngineFlightLoopSound;
    MissionBriefSound = nullptr;

    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
}

void ARotorlineHelicopterPawn::ConfigureDeployment(ERotorlineCraftType Craft, const FRotorlineMissionDefinition& Mission)
{
    bUseCatalogAircraft = false;
    bSelectedAircraftArmed = Craft == ERotorlineCraftType::AttackMD500;
    SelectedAircraftId = Craft == ERotorlineCraftType::AttackMD500 ? TEXT("md500_defender") : TEXT("uh1_huey");
    SelectedCraft = Craft;
    ActiveMission = Mission;
    CurrentObjectiveIndex = 0;
    TransitEncountersSpawned = 0;
    TransitThreatAttackPasses = 0;
    TransitThreatRetreatTime = -1000.0;
    LastTransitEncounterTime = -1000.0;
    bTransitThreatHarmless = false;
    bDeploymentConfigured = true;
    ApplyCraftConfiguration();
    ConfigureAircraftExhaust();
    UpdateAircraftLightStations();
    RouteMissionBriefAudio();
    bApacheCombatZoomEnabled = false;
    NormalCameraArmLength = SpringArm->TargetArmLength;
    NormalCameraSocketOffset = SpringArm->SocketOffset;
    Camera->SetFieldOfView(88.0f);
}

void ARotorlineHelicopterPawn::ConfigureDeployment(
    const FRotorlineAircraftDefinition& Aircraft,
    const FRotorlineMissionDefinition& Mission)
{
    SelectedAircraftDefinition = Aircraft;
    SelectedAircraftId = Aircraft.Id;
    bUseCatalogAircraft = !Aircraft.Id.Equals(TEXT("uh1_huey"), ESearchCase::IgnoreCase) &&
        !Aircraft.Id.Equals(TEXT("md500_defender"), ESearchCase::IgnoreCase);
    bSelectedAircraftArmed = Aircraft.WeaponLoadout.bEnabled || Aircraft.MissionSuitability.Attack >= 4 ||
        Aircraft.Role.Contains(TEXT("attack"), ESearchCase::IgnoreCase) ||
        Aircraft.Role.Contains(TEXT("gunship"), ESearchCase::IgnoreCase);
    const bool bKiowa = Aircraft.Id.Equals(TEXT("oh58_kiowa"), ESearchCase::IgnoreCase);
    const bool bKiowaReconMission =
        Mission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase) ||
        Mission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase);
    if (bKiowa && bKiowaReconMission)
    {
        bSelectedAircraftArmed = false;
    }
    SelectedCraft = bSelectedAircraftArmed ? ERotorlineCraftType::AttackMD500 : ERotorlineCraftType::SupportHuey;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_KIOWA_LOADOUT|aircraft=%s|mission=%s|mode=%s|armed=%d"),
        *Aircraft.Id, *Mission.Id,
        bKiowa && bKiowaReconMission ? TEXT("RECON_SENSOR") :
            (bKiowa ? TEXT("ATTACK_GUN_ROCKET") : TEXT("STANDARD")),
        bSelectedAircraftArmed ? 1 : 0);
    ActiveMission = Mission;
    CurrentObjectiveIndex = 0;
    TransitEncountersSpawned = 0;
    TransitThreatAttackPasses = 0;
    TransitThreatRetreatTime = -1000.0;
    LastTransitEncounterTime = -1000.0;
    bTransitThreatHarmless = false;
    bDeploymentConfigured = true;
    ApplyCraftConfiguration();
    ConfigureAircraftExhaust();
    UpdateAircraftLightStations();
    RouteMissionBriefAudio();
    bApacheCombatZoomEnabled = false;
    NormalCameraArmLength = SpringArm->TargetArmLength;
    NormalCameraSocketOffset = SpringArm->SocketOffset;
    Camera->SetFieldOfView(88.0f);
}

void ARotorlineHelicopterPawn::SetFleetQualificationMode(bool bSkipStartup)
{
    bFleetQualificationMode = true;
    bFleetQualificationSkipStartup = bSkipStartup;
    bFleetQualificationWeaponAttempted = false;
    bFleetQualificationWeaponPassed = !bSelectedAircraftArmed;
    bFleetQualificationCountermeasureAttempted = false;
    bFleetQualificationCountermeasurePassed = false;
    FleetQualificationElapsed = 0.0f;
    FleetQualificationMaxDisplacementMeters = 0.0f;
    FleetQualificationMaxAttitudeDegrees = 0.0f;
    FleetQualificationMilestone = 0;
}

void ARotorlineHelicopterPawn::SkipStartupForPlaytest()
{
    GetWorldTimerManager().ClearTimer(EngineTransitionTimer);
    GetWorldTimerManager().ClearTimer(EngineTakeoffTimer);
    GetWorldTimerManager().ClearTimer(MissionBriefTimer);
    MissionBriefAudio->Stop();
    EngineStartupAudio->Stop();
    EngineTakeoffAudio->Stop();
    bMissionBriefActive = false;
    BeginFlightEngineAudio();
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_QUICK_DEPLOY|STARTUP_SKIPPED|aircraft=%s|player_control=ENABLED"), *SelectedAircraftId);
}

void ARotorlineHelicopterPawn::BeginTransitionSpawnHold(float MinimumSeconds)
{
    TransitionSpawnHoldRemaining = FMath::Max(TransitionSpawnHoldRemaining, MinimumSeconds);
    bTransitionSpawnAwaitingNeutral = true;
    ForwardInput = 0.0f;
    StrafeInput = 0.0f;
    CollectiveInput = 0.0f;
    YawInput = 0.0f;
    Bell222SmoothedForwardInput = 0.0f;
    Bell222SmoothedCollectiveInput = 0.0f;
    Bell222CurrentSpeedScale = 1.0f;
    CurrentVelocity = FVector::ZeroVector;
    CurrentYawRate = 0.0f;
    LastWPress = LastSPress = LastAPress = LastDPress = -1000.0;
    LastSpacePress = LastZPress = LastCPress = LastQPress = LastEPress = -1000.0;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CAVE_SEQUENCE|BELL_SPAWN_HOLD|minimum=%.2f|release=INPUT_NEUTRAL"),
        MinimumSeconds);
}

void ARotorlineHelicopterPawn::ApplyCraftConfiguration()
{
    ResetBell222StealthMaterials();
    StopApacheCannonAudio(TEXT("AIRCRAFT_RECONFIGURED"), 0.0f);
    ResetBell222WeaponSystem();
    Bell222SmoothedForwardInput = 0.0f;
    Bell222SmoothedCollectiveInput = 0.0f;
    Bell222CurrentSpeedScale = 1.0f;
    // Every attack-class deployment begins in guided missile mode. D-pad Down
    // switches to the cannon sight without carrying state between aircraft.
    bApacheMissileLockMode = true;
    EnginePreIgnitionSound = nullptr;
    PedalSideforceAcceleration = 0.0f;
    CollectiveTorqueYawRate = 0.0f;
    RotorDiscBiasAcceleration = FVector::ZeroVector;
    const bool bUseMD500 = !bUseCatalogAircraft && SelectedCraft == ERotorlineCraftType::AttackMD500;
    const bool bUseHuey = !bUseCatalogAircraft && !bUseMD500;
    BodyMesh->SetVisibility(bUseHuey, true);
    GlassMesh->SetVisibility(bUseHuey, true);
    InteriorMesh->SetVisibility(bUseHuey, true);
    MainRotorMesh->SetVisibility(bUseHuey, true);
    TailRotorMesh->SetVisibility(bUseHuey, true);
    MD500BodyMesh->SetVisibility(bUseMD500, true);
    MD500AccessoryMesh->SetVisibility(bUseMD500, true);
    MD500WeaponMesh->SetVisibility(bUseMD500, true);
    MD500CockpitMesh->SetVisibility(bUseMD500, true);
    MD500GlassMesh->SetVisibility(bUseMD500, true);
    MD500MainRotorMountMesh->SetVisibility(bUseMD500, true);
    MD500MainRotorRotatingMesh->SetVisibility(bUseMD500, true);
    MD500TailRotorMountMesh->SetVisibility(bUseMD500, true);
    MD500TailRotorRotatingMesh->SetVisibility(bUseMD500, true);
    CatalogBodyMesh->SetVisibility(false, true);
    CatalogSkeletalBodyMesh->SetVisibility(false, true);
    for (UStaticMeshComponent* Rotor : CatalogStaticRotors) Rotor->SetVisibility(false, true);
    for (USkeletalMeshComponent* Rotor : CatalogSkeletalRotors) Rotor->SetVisibility(false, true);
    const bool bArmedKiowa =
        SelectedAircraftId.Equals(TEXT("oh58_kiowa"), ESearchCase::IgnoreCase) &&
        bSelectedAircraftArmed;
    KiowaLeftMinigun->SetVisibility(bArmedKiowa, true);
    KiowaRightRocketPod->SetVisibility(bArmedKiowa, true);

    MeshAlignment->SetRelativeScale3D(FVector(1.0f));
    MeshAlignment->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));
    MeshAlignment->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

    if (bUseCatalogAircraft)
    {
        ApplyCatalogAircraftConfiguration();
        ConfigureBell222WeaponComponents();
        InitializeBell222StealthMaterials();
        return;
    }

    if (bUseMD500)
    {
        MeshAlignment->SetRelativeScale3D(FVector(0.0133f));
        // The body, mounts, hubs and blades now share one model-native root.
        // Pivots are authored at the supplied shaft centers; the previous
        // borrowed tail rotor was 52 cm away from the actual tail gearbox.
        MD500SuperiorRoot->SetRelativeLocation(FVector(-1189.0f, -10454.0f, 34720.0f));
        MD500SuperiorRoot->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
        MD500SuperiorRoot->SetRelativeScale3D(FVector(330.0f));
        MD500MainRotorPivot->SetRelativeRotation(FRotator::ZeroRotator);
        MD500TailRotorPivot->SetRelativeRotation(FRotator::ZeroRotator);
        MD500MainRotorIntegratedDegrees = 0.0f;
        MD500TailRotorIntegratedDegrees = 0.0f;
        EngineStartupSound = MD500EngineStartupSound;
        EngineTakeoffSound = MD500EngineTakeoffSound;
        EngineFlightLoopSound = MD500EngineFlightLoopSound;
        MissionBriefSound = nullptr;
        RotorAnimation = nullptr;
        const FVector MainRotorPivotLocation(32.9767f, 0.0850f, -40.2828f);
        const FVector TailRotorPivotLocation(-79.0857f, -5.4154f, -62.2113f);
        const bool bRotorAttachmentPass =
            MD500MainRotorPivot->GetAttachParent() == MD500SuperiorRoot &&
            MD500TailRotorPivot->GetAttachParent() == MD500SuperiorRoot &&
            MD500MainRotorMountMesh->GetAttachParent() == MD500SuperiorRoot &&
            MD500TailRotorMountMesh->GetAttachParent() == MD500SuperiorRoot &&
            MD500MainRotorRotatingMesh->GetAttachParent() == MD500MainRotorPivot &&
            MD500TailRotorRotatingMesh->GetAttachParent() == MD500TailRotorPivot &&
            MD500MainRotorPivot->GetRelativeLocation().Equals(MainRotorPivotLocation, 0.01f) &&
            MD500TailRotorPivot->GetRelativeLocation().Equals(TailRotorPivotLocation, 0.01f) &&
            MD500MainRotorRotatingMesh->GetRelativeLocation().Equals(-MainRotorPivotLocation, 0.01f) &&
            MD500TailRotorRotatingMesh->GetRelativeLocation().Equals(-TailRotorPivotLocation, 0.01f);
        const bool bStationaryMountPass =
            MD500MainRotorMountMesh->GetRelativeLocation().IsNearlyZero(0.01f) &&
            MD500TailRotorMountMesh->GetRelativeLocation().IsNearlyZero(0.01f) &&
            MD500MainRotorMountMesh->GetRelativeRotation().IsNearlyZero(0.01f) &&
            MD500TailRotorMountMesh->GetRelativeRotation().IsNearlyZero(0.01f);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_MH6_ROTORS|CONFIG|assets=%d/4|main_mount=%d|main_rotating=%d|tail_mount=%d|tail_rotating=%d|legacy_rotors=0|attachment_pass=%d|stationary_pass=%d|main_pivot=32.977,0.085,-40.283|tail_pivot=-79.086,-5.415,-62.211"),
            (MD500MainRotorMountMesh->GetStaticMesh() ? 1 : 0) +
                (MD500MainRotorRotatingMesh->GetStaticMesh() ? 1 : 0) +
                (MD500TailRotorMountMesh->GetStaticMesh() ? 1 : 0) +
                (MD500TailRotorRotatingMesh->GetStaticMesh() ? 1 : 0),
            MD500MainRotorMountMesh->GetStaticMesh() ? 1 : 0,
            MD500MainRotorRotatingMesh->GetStaticMesh() ? 1 : 0,
            MD500TailRotorMountMesh->GetStaticMesh() ? 1 : 0,
            MD500TailRotorRotatingMesh->GetStaticMesh() ? 1 : 0,
            bRotorAttachmentPass ? 1 : 0,
            bStationaryMountPass ? 1 : 0);
        const bool bWeaponStationAttachmentPass =
            MD500LeftGunMuzzle && MD500RightGunMuzzle &&
            MD500LeftRocketMuzzle && MD500RightRocketMuzzle &&
            MD500LeftGunMuzzle->GetAttachParent() == MD500SuperiorRoot &&
            MD500RightGunMuzzle->GetAttachParent() == MD500SuperiorRoot &&
            MD500LeftRocketMuzzle->GetAttachParent() == MD500SuperiorRoot &&
            MD500RightRocketMuzzle->GetAttachParent() == MD500SuperiorRoot;
        const bool bWeaponStationLocationPass = bWeaponStationAttachmentPass &&
            MD500LeftGunMuzzle->GetRelativeLocation().Equals(FVector(59.5f, -21.16f, -81.10f), 0.01f) &&
            MD500RightGunMuzzle->GetRelativeLocation().Equals(FVector(59.5f, 21.37f, -81.10f), 0.01f) &&
            MD500LeftRocketMuzzle->GetRelativeLocation().Equals(FVector(54.8f, -33.91f, -85.60f), 0.01f) &&
            MD500RightRocketMuzzle->GetRelativeLocation().Equals(FVector(54.8f, 34.12f, -85.60f), 0.01f);
        const float GunStationSeparationCm = FVector::Distance(
            MD500LeftGunMuzzle->GetComponentLocation(), MD500RightGunMuzzle->GetComponentLocation());
        const float RocketStationSeparationCm = FVector::Distance(
            MD500LeftRocketMuzzle->GetComponentLocation(), MD500RightRocketMuzzle->GetComponentLocation());
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_MH6_WEAPONS|CONFIG|guns=2/2|rockets=2/2|attachment_pass=%d|location_pass=%d|gun_separation_cm=%.1f|rocket_separation_cm=%.1f"),
            bWeaponStationAttachmentPass ? 1 : 0,
            bWeaponStationLocationPass ? 1 : 0,
            GunStationSeparationCm,
            RocketStationSeparationCm);
        CollisionBox->SetBoxExtent(FVector(350.0f, 90.0f, 105.0f));
        MaxForwardSpeed = 8500.0f;
        MaxReverseSpeed = 3500.0f;
        // The legacy MD-500 path bypasses catalog tuning. Match the Kiowa's
        // speed-4/maneuverability-5 profile so the Defender is never the less
        // capable scout helicopter merely because it uses dedicated meshes.
        MaxStrafeSpeed = 4550.0f;
        MaxVerticalSpeed = 3700.0f;
        MaxYawRate = 78.0f;
        MaxPitchAngle = 36.5f;
        MaxRollAngle = 92.0f;
        CyclicAcceleration = 3150.0f;
        CollectiveAcceleration = 1750.0f;
        ForwardDrag = 0.10f;
        LateralDrag = 0.36f;
        VerticalDrag = 0.58f;
        CyclicResponse = 3.9f;
        YawResponse = 4.1f;
        TranslationalLiftAcceleration = 105.0f;
        CameraFollowResponse = 5.5f;
        VelocityResponse = 2.35f;
        AttitudeResponse = 4.1f;
        BoostMultiplier = 1.45f;
        SpringArm->TargetArmLength = 900.0f;
        SpringArm->SocketOffset = FVector(0.0f, 0.0f, 185.0f);
        MaxHealth = 82.0f;
    }
    else
    {
        EngineStartupSound = HueyEngineStartupSound;
        EngineTakeoffSound = nullptr;
        EngineFlightLoopSound = HueyEngineFlightLoopSound;
        MissionBriefSound = nullptr;
        RotorAnimation = HueyRotorAnimation;
        CollisionBox->SetBoxExtent(FVector(410.0f, 100.0f, 110.0f));
        MaxForwardSpeed = 7200.0f;
        MaxReverseSpeed = 3000.0f;
        MaxStrafeSpeed = 3600.0f;
        MaxVerticalSpeed = 1800.0f;
        MaxYawRate = 55.0f;
        MaxPitchAngle = 31.0f;
        MaxRollAngle = 36.0f;
        CyclicAcceleration = 2700.0f;
        CollectiveAcceleration = 1500.0f;
        ForwardDrag = 0.12f;
        LateralDrag = 0.42f;
        VerticalDrag = 0.65f;
        CyclicResponse = 2.8f;
        YawResponse = 3.2f;
        TranslationalLiftAcceleration = 85.0f;
        CameraFollowResponse = 4.5f;
        VelocityResponse = 1.9f;
        AttitudeResponse = 3.0f;
        BoostMultiplier = 1.45f;
        SpringArm->TargetArmLength = 1150.0f;
        SpringArm->SocketOffset = FVector(0.0f, 0.0f, 230.0f);
        MaxHealth = 110.0f;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_UH1_ROTOR|driver=CONTINUOUS_COMPONENT_ROTATION|main_dps=1080|tail_ratio=1.60|spool=MARINE_PARITY"));
    }
}

void ARotorlineHelicopterPawn::UpdateAircraftLightStations()
{
    const FVector Extent = CollisionBox->GetUnscaledBoxExtent();
    // Keep the emitters just inside the fitted collision envelope. Placing
    // them directly on or beyond the bounds made several catalog aircraft
    // look as though their lights were floating beside the airframe.
    const float SideX = FMath::Clamp(Extent.X * 0.08f, 18.0f, 58.0f);
    const float SideY = FMath::Clamp(Extent.Y * 0.82f, 62.0f, 126.0f);
    const float SideZ = FMath::Clamp(Extent.Z * 0.10f, 10.0f, 32.0f);
    const float TailX = -Extent.X * 0.86f;
    const float TailZ = FMath::Clamp(Extent.Z * 0.08f, 7.0f, 28.0f);

    const FVector PortStation(SideX, -SideY, SideZ);
    const FVector StarboardStation(SideX, SideY, SideZ);
    const FVector TailStation(TailX, 0.0f, TailZ);
    LeftNavigationLight->SetRelativeLocation(PortStation);
    RightNavigationLight->SetRelativeLocation(StarboardStation);
    TailStrobeLight->SetRelativeLocation(TailStation);
    LeftNavigationBulb->SetRelativeLocation(PortStation);
    RightNavigationBulb->SetRelativeLocation(StarboardStation);
    TailStrobeBulb->SetRelativeLocation(TailStation);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_NIGHT_OPS|LIGHT_STATIONS|aircraft=%s|port=%.1f,%.1f,%.1f|starboard=%.1f,%.1f,%.1f|tail=%.1f,%.1f,%.1f"),
        *SelectedAircraftId,
        PortStation.X,
        PortStation.Y,
        PortStation.Z,
        StarboardStation.X,
        StarboardStation.Y,
        StarboardStation.Z,
        TailStation.X,
        TailStation.Y,
        TailStation.Z);
}

void ARotorlineHelicopterPawn::RouteMissionBriefAudio()
{
    // The rescue brief belongs to Golden Hour, not to a support-aircraft class.
    // This keeps Cabin Lifeline silent and plays the rescue setup for Mission 3
    // even when the player selects an armed airframe.
    const bool bGoldenHourRescue = ActiveMission.Id.Equals(TEXT("medevac"), ESearchCase::IgnoreCase);
    const bool bMission1 = ActiveMission.Id.Equals(TEXT("tutorial"), ESearchCase::IgnoreCase);
    const bool bRooftopExtraction = ActiveMission.Id.Equals(TEXT("rooftop-extraction"), ESearchCase::IgnoreCase);
    const bool bEnemyFoothold = ActiveMission.Id.Equals(TEXT("enemy-foothold"), ESearchCase::IgnoreCase);
    const bool bCabinSupplyConvoy = ActiveMission.Id.Equals(TEXT("cabin-supply-convoy"), ESearchCase::IgnoreCase);
    const bool bBell222Opening = IsBell222SpecialOperations() &&
        ActiveMission.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase) &&
        Bell222FinalMissionBriefSound;
    USoundBase* RooftopExtractionBriefSound = bRooftopExtraction
        ? LoadObject<USoundBase>(nullptr, RotorlineHelicopter::RooftopExtractionMissionBriefPath)
        : nullptr;
    USoundBase* EnemyFootholdBriefSound = bEnemyFoothold
        ? LoadObject<USoundBase>(nullptr, RotorlineHelicopter::EnemyFootholdMissionBriefPath)
        : nullptr;
    USoundBase* CabinSupplyConvoyBriefSound = bCabinSupplyConvoy
        ? LoadObject<USoundBase>(nullptr, RotorlineHelicopter::CabinSupplyConvoyMissionBriefPath)
        : nullptr;
    if (bBell222Opening)
    {
        MissionBriefSound = Bell222FinalMissionBriefSound.Get();
    }
    else if (bEnemyFoothold)
    {
        MissionBriefSound = EnemyFootholdBriefSound;
    }
    else if (bCabinSupplyConvoy)
    {
        MissionBriefSound = CabinSupplyConvoyBriefSound;
    }
    else if (bRooftopExtraction)
    {
        MissionBriefSound = RooftopExtractionBriefSound;
    }
    else if (bMission1)
    {
        MissionBriefSound = Mission1BriefSound.Get();
    }
    else if (bGoldenHourRescue)
    {
        MissionBriefSound = HueyMissionBriefSound.Get();
    }
    else
    {
        MissionBriefSound = nullptr;
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_MISSION_AUDIO|ROUTE|mission=%s|title=%s|mission1_brief=%s|bell222_opening=%s|rescue_brief=%s|enemy_foothold_brief=%s"),
        *ActiveMission.Id,
        *ActiveMission.Title,
        bMission1 && MissionBriefSound ? TEXT("ENABLED") : TEXT("DISABLED"),
        bBell222Opening ? TEXT("ENABLED") : TEXT("DISABLED"),
        bGoldenHourRescue && MissionBriefSound ? TEXT("ENABLED") : TEXT("DISABLED"),
        bEnemyFoothold && MissionBriefSound ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void ARotorlineHelicopterPawn::ApplyCatalogAircraftConfiguration()
{
    ResetCatalogAircraftComponents();
    CatalogBodyMesh->SetStaticMesh(nullptr);
    CatalogSkeletalBodyMesh->SetSkeletalMesh(nullptr);
    for (UStaticMeshComponent* Rotor : CatalogStaticRotors) Rotor->SetStaticMesh(nullptr);
    for (USkeletalMeshComponent* Rotor : CatalogSkeletalRotors) Rotor->SetSkeletalMesh(nullptr);

    TMap<FString, int32> DeclaredRotorOrder;
    for (int32 Index = 0; Index < SelectedAircraftDefinition.RotorAssets.Num(); ++Index)
    {
        if (!SelectedAircraftDefinition.RotorAssets[Index].IsEmpty())
        {
            DeclaredRotorOrder.Add(SelectedAircraftDefinition.RotorAssets[Index], Index);
        }
    }

    TMap<FString, int32> ExplicitRotorGroupByPath;
    TArray<bool> ExplicitRotorGroupIsTail;
    TArray<FVector> ExplicitRotorGroupAxes;
    TArray<bool> ExplicitRotorGroupHasPivot;
    TArray<FVector> ExplicitRotorGroupPivots;
    TArray<bool> ExplicitRotorGroupHasMeshPivot;
    TArray<FVector> ExplicitRotorGroupMeshPivots;
    TArray<FRotator> ExplicitRotorGroupAlignmentRotations;
    for (int32 GroupIndex = 0; GroupIndex < SelectedAircraftDefinition.RotorGroups.Num(); ++GroupIndex)
    {
        const FRotorlineAircraftRotorGroup& Group = SelectedAircraftDefinition.RotorGroups[GroupIndex];
        const bool bTail = Group.Role.Equals(TEXT("tail"), ESearchCase::IgnoreCase);
        ExplicitRotorGroupIsTail.Add(bTail);
        ExplicitRotorGroupAxes.Add(RotorlineHelicopter::CatalogRotorAxis(Group.SpinAxis, bTail));
        ExplicitRotorGroupHasPivot.Add(Group.bHasExplicitPivot);
        ExplicitRotorGroupPivots.Add(Group.Pivot);
        ExplicitRotorGroupHasMeshPivot.Add(Group.bHasExplicitMeshPivot);
        ExplicitRotorGroupMeshPivots.Add(Group.MeshPivot);
        ExplicitRotorGroupAlignmentRotations.Add(Group.AlignmentRotation);
        for (const FString& AssetPath : Group.Assets)
        {
            if (!AssetPath.IsEmpty()) ExplicitRotorGroupByPath.Add(AssetPath, GroupIndex);
        }
    }

    TSet<FString> CandidatePaths;
    if (!SelectedAircraftDefinition.BodyAsset.IsEmpty())
    {
        CandidatePaths.Add(SelectedAircraftDefinition.BodyAsset);
    }
    for (const FString& RotorPath : SelectedAircraftDefinition.RotorAssets)
    {
        if (!RotorPath.IsEmpty())
        {
            CandidatePaths.Add(RotorPath);
        }
    }
    for (const FRotorlineAircraftRotorGroup& Group : SelectedAircraftDefinition.RotorGroups)
    {
        for (const FString& RotorPath : Group.Assets)
        {
            if (!RotorPath.IsEmpty()) CandidatePaths.Add(RotorPath);
        }
    }
    if (!SelectedAircraftDefinition.AssetRoot.IsEmpty())
    {
        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        TArray<FAssetData> Assets;
        AssetRegistry.GetAssetsByPath(FName(*SelectedAircraftDefinition.AssetRoot), Assets, true);
        for (const FAssetData& Asset : Assets)
        {
            CandidatePaths.Add(Asset.GetSoftObjectPath().ToString());
        }
    }

    TArray<FString> OrderedPaths = CandidatePaths.Array();
    OrderedPaths.Sort();
    FBox LocalBounds(ForceInit);
    int32 LoadedBodyParts = 0;
    int32 LoadedRotorParts = 0;
    const int32 ImplicitMainRotorGroup = SelectedAircraftDefinition.RotorGroups.Num();
    const int32 ImplicitTailRotorGroup = ImplicitMainRotorGroup + 1;
    TSet<FString> StationaryRotorAssets;
    for (const FString& StationaryPath : SelectedAircraftDefinition.StationaryRotorAssets)
    {
        if (!StationaryPath.IsEmpty()) StationaryRotorAssets.Add(StationaryPath);
    }
    TMap<int32, USceneComponent*> RotorGroupPivots;
    TMap<int32, FBox> RotorGroupBounds;
    TMap<int32, TArray<USceneComponent*>> RotorGroupParts;
    TMap<int32, bool> RotorGroupIsTail;
    TMap<int32, FVector> RotorGroupAxes;
    TMap<int32, bool> RotorGroupHasExplicitPivot;
    TMap<int32, FVector> RotorGroupExplicitPivots;
    TMap<int32, bool> RotorGroupHasExplicitMeshPivot;
    TMap<int32, FVector> RotorGroupExplicitMeshPivots;
    TMap<int32, FRotator> RotorGroupAlignmentRotations;
    for (const FString& Path : OrderedPaths)
    {
        if (CatalogDynamicStaticParts.Num() + CatalogDynamicSkeletalParts.Num() >= RotorlineHelicopter::MaximumCatalogMeshParts)
        {
            break;
        }

        const int32* ExplicitRotorGroup = ExplicitRotorGroupByPath.Find(Path);
        const int32* DeclaredRotorIndex = DeclaredRotorOrder.Find(Path);
        const bool bStationaryRotorAsset = StationaryRotorAssets.Contains(Path);
        const bool bImplicitRotor = !bStationaryRotorAsset
            && (DeclaredRotorIndex != nullptr || RotorlineHelicopter::IsCatalogRotorPath(Path));
        const bool bRotor = !bStationaryRotorAsset && (ExplicitRotorGroup != nullptr || bImplicitRotor);
        const bool bTailRotor = ExplicitRotorGroup
            ? ExplicitRotorGroupIsTail[*ExplicitRotorGroup]
            : RotorlineHelicopter::IsCatalogTailRotorPath(Path) || (DeclaredRotorIndex && *DeclaredRotorIndex > 0);
        const int32 RotorGroupIndex = ExplicitRotorGroup
            ? *ExplicitRotorGroup
            : bRotor ? (bTailRotor ? ImplicitTailRotorGroup : ImplicitMainRotorGroup) : INDEX_NONE;

        USceneComponent* RotorPivot = nullptr;
        if (bRotor)
        {
            if (USceneComponent** ExistingPivot = RotorGroupPivots.Find(RotorGroupIndex))
            {
                RotorPivot = *ExistingPivot;
            }
            else
            {
                RotorPivot = NewObject<USceneComponent>(this);
                AddInstanceComponent(RotorPivot);
                RotorPivot->SetupAttachment(MeshAlignment);
                RotorPivot->RegisterComponent();
                CatalogRotorPivots.Add(RotorPivot);
                RotorGroupPivots.Add(RotorGroupIndex, RotorPivot);
                RotorGroupBounds.Add(RotorGroupIndex, FBox(ForceInit));
                RotorGroupIsTail.Add(RotorGroupIndex, bTailRotor);
                RotorGroupAxes.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup
                        ? ExplicitRotorGroupAxes[*ExplicitRotorGroup]
                        : RotorlineHelicopter::CatalogRotorAxis(TEXT(""), bTailRotor));
                RotorGroupHasExplicitPivot.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup && ExplicitRotorGroupHasPivot[*ExplicitRotorGroup]);
                RotorGroupExplicitPivots.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup ? ExplicitRotorGroupPivots[*ExplicitRotorGroup] : FVector::ZeroVector);
                RotorGroupHasExplicitMeshPivot.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup && ExplicitRotorGroupHasMeshPivot[*ExplicitRotorGroup]);
                RotorGroupExplicitMeshPivots.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup ? ExplicitRotorGroupMeshPivots[*ExplicitRotorGroup] : FVector::ZeroVector);
                RotorGroupAlignmentRotations.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup ? ExplicitRotorGroupAlignmentRotations[*ExplicitRotorGroup] : FRotator::ZeroRotator);
            }
        }

        if (UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *Path))
        {
            UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
            AddInstanceComponent(Part);
            Part->SetupAttachment(RotorPivot ? RotorPivot : MeshAlignment.Get());
            Part->SetStaticMesh(StaticMesh);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Part->SetCastShadow(true);
            Part->RegisterComponent();
            CatalogDynamicStaticParts.Add(Part);
            if (SelectedAircraftDefinition.Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase) &&
                Path.Contains(TEXT("Bell222X_Gear"), ESearchCase::IgnoreCase))
            {
                Bell222LandingGearMesh = Part;
                Bell222LandingGearMesh->SetRelativeLocation(FVector::ZeroVector);
                Bell222LandingGearMesh->SetRelativeScale3D(FVector::OneVector);
                Bell222LandingGearMesh->SetVisibility(false, true);
            }
            if (bRotor)
            {
                RotorGroupBounds.FindChecked(RotorGroupIndex) += StaticMesh->GetBoundingBox();
                RotorGroupParts.FindOrAdd(RotorGroupIndex).Add(Part);
                ++LoadedRotorParts;
            }
            else
            {
                LocalBounds += StaticMesh->GetBoundingBox();
                ++LoadedBodyParts;
            }
            continue;
        }

        if (USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *Path))
        {
            USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(this);
            AddInstanceComponent(Part);
            Part->SetupAttachment(RotorPivot ? RotorPivot : MeshAlignment.Get());
            Part->SetSkeletalMesh(SkeletalMesh);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Part->SetCastShadow(true);
            Part->RegisterComponent();
            CatalogDynamicSkeletalParts.Add(Part);
            if (bRotor)
            {
                RotorGroupBounds.FindChecked(RotorGroupIndex) += SkeletalMesh->GetBounds().GetBox();
                RotorGroupParts.FindOrAdd(RotorGroupIndex).Add(Part);
                ++LoadedRotorParts;
            }
            else
            {
                LocalBounds += SkeletalMesh->GetBounds().GetBox();
                ++LoadedBodyParts;
            }
        }
    }

    for (const TPair<int32, USceneComponent*>& Group : RotorGroupPivots)
    {
        const TArray<USceneComponent*>* GroupParts = RotorGroupParts.Find(Group.Key);
        if (!GroupParts || GroupParts->IsEmpty())
        {
            // A catalog entry can legitimately name an optional rotor mesh that
            // is absent from this import. Do not promote an empty pivot into the
            // active rotor lists; the procedural disc fallback below will fill
            // only the missing rotor role.
            if (Group.Value)
            {
                Group.Value->DestroyComponent();
            }
            continue;
        }
        const FBox& Bounds = RotorGroupBounds.FindChecked(Group.Key);
        const bool bExplicitPivot = RotorGroupHasExplicitPivot.FindRef(Group.Key);
        const FVector PivotCenter = bExplicitPivot
            ? RotorGroupExplicitPivots.FindChecked(Group.Key)
            : Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;
        const bool bExplicitMeshPivot = RotorGroupHasExplicitMeshPivot.FindRef(Group.Key);
        const FVector MeshCenter = bExplicitMeshPivot
            ? RotorGroupExplicitMeshPivots.FindChecked(Group.Key)
            : PivotCenter;
        const FRotator AlignmentRotation = RotorGroupAlignmentRotations.FindRef(Group.Key);
        Group.Value->SetRelativeLocation(PivotCenter);
        for (USceneComponent* Part : *GroupParts)
        {
            Part->SetRelativeRotation(AlignmentRotation);
            Part->SetRelativeLocation(-AlignmentRotation.RotateVector(MeshCenter));
        }
        if (RotorGroupIsTail.FindChecked(Group.Key))
        {
            CatalogTailRotorParts.Add(Group.Value);
            CatalogTailRotorAxes.Add(RotorGroupAxes.FindChecked(Group.Key));
        }
        else
        {
            CatalogMainRotorParts.Add(Group.Value);
            CatalogMainRotorAxes.Add(RotorGroupAxes.FindChecked(Group.Key));
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_PLAYER_ROTOR_PIVOT|aircraft=%s|role=%s|parts=%d|source=%s|pivot=%.3f,%.3f,%.3f|mesh_pivot=%.3f,%.3f,%.3f|alignment=%.2f,%.2f,%.2f"),
            *SelectedAircraftDefinition.Id,
            RotorGroupIsTail.FindChecked(Group.Key) ? TEXT("tail") : TEXT("main"),
            GroupParts->Num(),
            bExplicitPivot ? TEXT("explicit") : TEXT("bounds"),
            PivotCenter.X,
            PivotCenter.Y,
            PivotCenter.Z,
            MeshCenter.X,
            MeshCenter.Y,
            MeshCenter.Z,
            AlignmentRotation.Pitch,
            AlignmentRotation.Yaw,
            AlignmentRotation.Roll);
    }

    if (!LocalBounds.IsValid)
    {
        LocalBounds = FBox(FVector(-350.0f, -180.0f, 0.0f), FVector(350.0f, 180.0f, 260.0f));
    }
    const bool bIntegratedAnimatedRotorGeometry =
        !SelectedAircraftDefinition.AnimationAsset.IsEmpty() && !CatalogDynamicSkeletalParts.IsEmpty();
    if (SelectedAircraftDefinition.bAllowProceduralRotorFallback && CatalogMainRotorParts.IsEmpty() && !bIntegratedAnimatedRotorGeometry)
    {
        AddCatalogFallbackMainRotor(LocalBounds);
    }
    if (SelectedAircraftDefinition.bAllowProceduralRotorFallback && CatalogTailRotorParts.IsEmpty() && !bIntegratedAnimatedRotorGeometry)
    {
        AddCatalogFallbackTailRotor(LocalBounds);
    }

    const float RequestedScale = FMath::Clamp(FMath::Abs(SelectedAircraftDefinition.PresentationScale), 0.001f, 250.0f);
    const float RawSpan = FMath::Max(LocalBounds.GetSize().GetMax(), 1.0f);
    const float RequestedSpan = RawSpan * RequestedScale;
    const bool bRequestedScaleFits = FMath::IsFinite(RequestedSpan)
        && RequestedSpan >= RotorlineHelicopter::MinimumCatalogAircraftSpanCm
        && RequestedSpan <= RotorlineHelicopter::MaximumCatalogAircraftSpanCm;
    const float ModelScale = bRequestedScaleFits
        ? RequestedScale
        : RotorlineHelicopter::TargetCatalogAircraftSpanCm / RawSpan;
    const FRotator ModelRotation(
        SelectedAircraftDefinition.PresentationPitch,
        SelectedAircraftDefinition.PresentationYaw - 90.0f,
        SelectedAircraftDefinition.PresentationRoll);
    MeshAlignment->SetRelativeScale3D(FVector(ModelScale));
    MeshAlignment->SetRelativeRotation(ModelRotation);

    FBox RotatedBounds(ForceInit);
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        const FVector Corner(
            (CornerIndex & 1) ? LocalBounds.Max.X : LocalBounds.Min.X,
            (CornerIndex & 2) ? LocalBounds.Max.Y : LocalBounds.Min.Y,
            (CornerIndex & 4) ? LocalBounds.Max.Z : LocalBounds.Min.Z);
        RotatedBounds += ModelRotation.RotateVector(Corner);
    }
    const FVector ScaledSize = RotatedBounds.GetSize() * ModelScale;
    const float ForwardHalf = FMath::Clamp(ScaledSize.X * 0.48f, 260.0f, 1150.0f);
    const float LateralHalf = FMath::Clamp(ScaledSize.Y * 0.48f, 75.0f, 360.0f);
    const float VerticalHalf = FMath::Clamp(ScaledSize.Z * 0.48f, 75.0f, 330.0f);
    CollisionBox->SetBoxExtent(FVector(ForwardHalf, LateralHalf, VerticalHalf));
    const FVector RotatedCenter = RotatedBounds.GetCenter();
    MeshAlignment->SetRelativeLocation(FVector(
        -RotatedCenter.X * ModelScale,
        -RotatedCenter.Y * ModelScale,
        -VerticalHalf - RotatedBounds.Min.Z * ModelScale) + SelectedAircraftDefinition.PresentationOffset);

    const int32 Speed = SelectedAircraftDefinition.Stats.Speed;
    const int32 Maneuverability = SelectedAircraftDefinition.Stats.Maneuverability;
    const int32 Armor = SelectedAircraftDefinition.Stats.Armor;
    MaxForwardSpeed = 5400.0f + Speed * 700.0f;
    if (SelectedAircraftDefinition.Id.Equals(TEXT("ch47_chinook"), ESearchCase::IgnoreCase))
    {
        // 87.5 m/s is exactly 315 km/h in Unreal's centimetres-per-second units.
        MaxForwardSpeed = 8750.0f;
    }
    MaxReverseSpeed = MaxForwardSpeed * 0.40f;
    MaxStrafeSpeed = 2200.0f + Maneuverability * 470.0f;
    MaxVerticalSpeed = 2200.0f + Maneuverability * 300.0f;
    MaxYawRate = 38.0f + Maneuverability * 8.0f;
    MaxPitchAngle = 24.0f + Maneuverability * 2.5f;
    // Preserve aircraft-specific maneuverability while allowing committed
    // aerobatic rolls. A poor full-control input can now produce a genuine
    // loss of control instead of stopping at a universal shallow bank.
    MaxRollAngle = 42.0f + Maneuverability * 10.0f;
    CyclicAcceleration = 2200.0f + Maneuverability * 190.0f;
    CollectiveAcceleration = 1250.0f + Maneuverability * 100.0f;
    ForwardDrag = FMath::Lerp(0.16f, 0.09f, (Speed - 1) / 4.0f);
    LateralDrag = FMath::Lerp(0.52f, 0.32f, (Maneuverability - 1) / 4.0f);
    VerticalDrag = FMath::Lerp(0.72f, 0.54f, (Maneuverability - 1) / 4.0f);
    CyclicResponse = 2.2f + Maneuverability * 0.34f;
    YawResponse = 2.5f + Maneuverability * 0.32f;
    TranslationalLiftAcceleration = 70.0f + Speed * 10.0f;
    CameraFollowResponse = 3.8f + Maneuverability * 0.35f;
    VelocityResponse = 1.55f + Maneuverability * 0.17f;
    AttitudeResponse = 2.4f + Maneuverability * 0.32f;
    BoostMultiplier = 1.35f + Speed * 0.025f;
    MaxHealth = 70.0f + Armor * 15.0f;

    // The Bell 222 uses the Apache's speed-4/maneuverability-4 handling model.
    // Its Bell222-specific distinction is the substantially stronger boost,
    // not floatier drag, response, or torque behavior.
    if (SelectedAircraftDefinition.Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase))
    {
        MaxForwardSpeed = 8200.0f;
        MaxReverseSpeed = 3280.0f;
        MaxStrafeSpeed = 4080.0f;
        MaxVerticalSpeed = 3400.0f;
        MaxYawRate = 70.0f;
        MaxPitchAngle = 34.0f;
        MaxRollAngle = 82.0f;
        CyclicAcceleration = 2960.0f;
        CollectiveAcceleration = 1650.0f;
        ForwardDrag = 0.1075f;
        LateralDrag = 0.37f;
        VerticalDrag = 0.585f;
        CyclicResponse = 3.56f;
        YawResponse = 3.78f;
        TranslationalLiftAcceleration = 110.0f;
        CameraFollowResponse = 5.2f;
        VelocityResponse = 2.23f;
        AttitudeResponse = 3.68f;
        BoostMultiplier = 3.60f;
        PedalSideforceAcceleration = 0.0f;
        CollectiveTorqueYawRate = 0.0f;
        RotorDiscBiasAcceleration = FVector::ZeroVector;
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_FLIGHT_DYNAMICS|craft=%s|forward_drag=%.3f|lateral_drag=%.3f|vertical_drag=%.3f|cyclic_response=%.2f|yaw_response=%.2f|pedal_sideforce=%.1f|collective_torque=%.1f|disc_bias=%.1f,%.1f"),
        *SelectedAircraftId,
        ForwardDrag,
        LateralDrag,
        VerticalDrag,
        CyclicResponse,
        YawResponse,
        PedalSideforceAcceleration,
        CollectiveTorqueYawRate,
        RotorDiscBiasAcceleration.X,
        RotorDiscBiasAcceleration.Y);
    SpringArm->TargetArmLength = FMath::Clamp(
        FMath::Max(
            CollisionBox->GetScaledBoxExtent().X * 2.65f,
            CollisionBox->GetScaledBoxExtent().Y * 3.20f),
        1100.0f,
        2400.0f);
    SpringArm->SocketOffset = FVector(0.0f, 0.0f, CollisionBox->GetScaledBoxExtent().Z * 1.65f);

    EnginePreIgnitionSound = !SelectedAircraftDefinition.PreIgnitionAudio.IsEmpty()
        ? LoadObject<USoundBase>(nullptr, *SelectedAircraftDefinition.PreIgnitionAudio)
        : nullptr;
    USoundBase* CatalogStartupSound = !SelectedAircraftDefinition.StartupAudio.IsEmpty()
        ? LoadObject<USoundBase>(nullptr, *SelectedAircraftDefinition.StartupAudio)
        : nullptr;
    const bool bSuppressLegacyAudioFallback =
        SelectedAircraftDefinition.Id.Equals(TEXT("ch47_chinook"), ESearchCase::IgnoreCase);
    EngineStartupSound = CatalogStartupSound
        ? CatalogStartupSound
        : (bSuppressLegacyAudioFallback
            ? nullptr
            : (bSelectedAircraftArmed ? MD500EngineStartupSound.Get() : HueyEngineStartupSound.Get()));
    EngineTakeoffSound = !SelectedAircraftDefinition.TakeoffAudio.IsEmpty()
        ? LoadObject<USoundBase>(nullptr, *SelectedAircraftDefinition.TakeoffAudio)
        : nullptr;
    USoundBase* CatalogFlightLoopSound = !SelectedAircraftDefinition.InflightAudio.IsEmpty()
        ? LoadObject<USoundBase>(nullptr, *SelectedAircraftDefinition.InflightAudio)
        : nullptr;
    EngineFlightLoopSound = CatalogFlightLoopSound
        ? CatalogFlightLoopSound
        : (bSuppressLegacyAudioFallback
            ? nullptr
            : (bSelectedAircraftArmed ? MD500EngineFlightLoopSound.Get() : HueyEngineFlightLoopSound.Get()));
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_HELICOPTER_AUDIO|ROUTE|craft=%s|startup=%s|startup_fallback=%d|flight_loop=%s|flight_fallback=%d"),
        *SelectedAircraftId,
        EngineStartupSound ? *EngineStartupSound->GetPathName() : TEXT("NONE"),
        CatalogStartupSound ? 0 : 1,
        EngineFlightLoopSound ? *EngineFlightLoopSound->GetPathName() : TEXT("NONE"),
        CatalogFlightLoopSound ? 0 : 1);
    if (!SelectedAircraftDefinition.AutocannonAudio.IsEmpty())
    {
        if (USoundBase* CatalogCannonSound = LoadObject<USoundBase>(nullptr, *SelectedAircraftDefinition.AutocannonAudio))
        {
            ApacheCannonSound = CatalogCannonSound;
        }
    }
    MissionBriefSound = nullptr;
    RotorAnimation = !SelectedAircraftDefinition.AnimationAsset.IsEmpty()
        ? LoadObject<UAnimSequence>(nullptr, *SelectedAircraftDefinition.AnimationAsset)
        : nullptr;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_PLAYER_AIRCRAFT|CONFIGURED|id=%s|armed=%d|speed=%d|maneuverability=%d|armor=%d|cargo=%d|body_parts=%d|rotor_parts=%d|static_parts=%d|skeletal_parts=%d|scale=%.5f"),
        *SelectedAircraftId,
        bSelectedAircraftArmed ? 1 : 0,
        Speed,
        Maneuverability,
        Armor,
        SelectedAircraftDefinition.Stats.Cargo,
        LoadedBodyParts,
        LoadedRotorParts,
        CatalogDynamicStaticParts.Num(),
        CatalogDynamicSkeletalParts.Num(),
        ModelScale);
}

void ARotorlineHelicopterPawn::ResetCatalogAircraftComponents()
{
    ResetBell222StealthMaterials();
    Bell222LandingGearMesh = nullptr;
    Bell222LandingGearAlpha = 0.0f;
    bBell222LandingGearRetracted = false;
    bBoostActive = false;
    bBell222BoostEffectActive = false;
    CatalogMainRotorParts.Reset();
    CatalogMainRotorAxes.Reset();
    CatalogTailRotorParts.Reset();
    CatalogTailRotorAxes.Reset();
    for (UStaticMeshComponent* Part : CatalogDynamicStaticParts)
    {
        if (Part)
        {
            Part->DestroyComponent();
        }
    }
    for (USkeletalMeshComponent* Part : CatalogDynamicSkeletalParts)
    {
        if (Part)
        {
            Part->DestroyComponent();
        }
    }
    CatalogDynamicStaticParts.Reset();
    CatalogDynamicSkeletalParts.Reset();
    for (USceneComponent* Pivot : CatalogRotorPivots)
    {
        if (Pivot)
        {
            Pivot->DestroyComponent();
        }
    }
    CatalogRotorPivots.Reset();
    if (CatalogFallbackMainRotorPivot)
    {
        CatalogFallbackMainRotorPivot->DestroyComponent();
        CatalogFallbackMainRotorPivot = nullptr;
    }
    if (CatalogFallbackTailRotorPivot)
    {
        CatalogFallbackTailRotorPivot->DestroyComponent();
        CatalogFallbackTailRotorPivot = nullptr;
    }
    bCatalogFallbackTailUsesXAxis = false;
}

void ARotorlineHelicopterPawn::ResetAircraftExhaust()
{
    for (UNiagaraComponent* Component : ExhaustComponents)
    {
        if (Component)
        {
            Component->DeactivateImmediate();
            Component->DestroyComponent();
        }
    }
    ExhaustComponents.Reset();
    for (USceneComponent* Root : ExhaustOutletRoots)
    {
        if (Root) Root->DestroyComponent();
    }
    ExhaustOutletRoots.Reset();
    bExhaustWasRunning = false;
    bExhaustStartupPulsePending = false;
    ExhaustStartupPulseRemaining = 0.0f;
}

void ARotorlineHelicopterPawn::ConfigureAircraftExhaust()
{
    ResetAircraftExhaust();
    const FRotorlineAircraftExhaustConfig& Config = SelectedAircraftDefinition.Exhaust;
    if (!Config.bEnabled || Config.Outlets.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_EXHAUST|DISABLED|aircraft=%s|reason=NO_CONFIGURATION"), *SelectedAircraftId);
        return;
    }

    if (!TurboshaftExhaustSystem)
    {
        TurboshaftExhaustSystem = LoadObject<UNiagaraSystem>(
            nullptr,
            TEXT("/Game/FX/HelicopterExhaust/NS_RotorlineTurboshaftExhaust.NS_RotorlineTurboshaftExhaust"));
    }
    if (!TurboshaftExhaustSystem)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_EXHAUST|DISABLED|aircraft=%s|reason=SYSTEM_MISSING"), *SelectedAircraftId);
        return;
    }

    for (int32 Index = 0; Index < Config.Outlets.Num(); ++Index)
    {
        const FRotorlineAircraftExhaustOutlet& Outlet = Config.Outlets[Index];
        USceneComponent* OutletRoot = NewObject<USceneComponent>(this);
        AddInstanceComponent(OutletRoot);
        OutletRoot->SetupAttachment(VisualRoot);
        OutletRoot->SetRelativeLocation(Outlet.Location);
        OutletRoot->SetRelativeRotation(Outlet.Rotation);
        OutletRoot->RegisterComponent();
        ExhaustOutletRoots.Add(OutletRoot);

        UNiagaraComponent* Exhaust = NewObject<UNiagaraComponent>(this);
        AddInstanceComponent(Exhaust);
        Exhaust->SetupAttachment(OutletRoot);
        Exhaust->SetAsset(TurboshaftExhaustSystem);
        Exhaust->SetAutoActivate(false);
        Exhaust->SetRelativeScale3D(FVector(
            Config.PlumeLength,
            Config.PlumeWidth * Outlet.Diameter / 26.0f,
            Config.PlumeWidth * Outlet.Diameter / 26.0f));
        Exhaust->RegisterComponent();
        Exhaust->SetVariableBool(TEXT("User.ExhaustEnabled"), false);
        Exhaust->SetVariableFloat(TEXT("User.OutletDiameter"), Outlet.Diameter);
        ExhaustComponents.Add(Exhaust);
    }

    bExhaustStartupPulsePending = Config.bStartupPuff;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_EXHAUST|CONFIGURED|aircraft=%s|outlets=%d|vapor=%.2f|distortion=%.2f|world_space=1"),
        *SelectedAircraftId,
        ExhaustComponents.Num(),
        Config.VaporAmount,
        Config.DistortionIntensity);
}

void ARotorlineHelicopterPawn::UpdateAircraftExhaust(float DeltaSeconds)
{
    if (ExhaustComponents.IsEmpty()) return;

    const FRotorlineAircraftExhaustConfig& Config = SelectedAircraftDefinition.Exhaust;
    const float RotorRPM = FMath::Clamp(
        CurrentRotorPlayRate / FMath::Max(RotorFlightPlayRate, 0.01f),
        0.0f,
        1.0f);
    const bool bEngineTurning = !bFuelStarved && !bPlayerAircraftDying && (bEngineReady || RotorRPM > 0.035f);
    if (bEngineTurning && !bExhaustWasRunning && bExhaustStartupPulsePending)
    {
        ExhaustStartupPulseRemaining = 0.72f;
        bExhaustStartupPulsePending = false;
    }
    ExhaustStartupPulseRemaining = FMath::Max(0.0f, ExhaustStartupPulseRemaining - DeltaSeconds);

    const float SpeedLoad = FMath::Clamp(CurrentVelocity.Size() / FMath::Max(MaxForwardSpeed, 1.0f), 0.0f, 1.0f);
    const float ControlLoad = FMath::Clamp(
        FMath::Max3(FMath::Abs(ForwardInput), FMath::Abs(StrafeInput), FMath::Abs(CollectiveInput)),
        0.0f,
        1.0f);
    const float EnginePower = bEngineTurning
        ? FMath::Clamp(FMath::Max3(RotorRPM * 0.42f, ControlLoad, SpeedLoad) + (bBoostActive ? 0.25f : 0.0f), 0.08f, 1.0f)
        : 0.0f;
    const float DamageLevel = 1.0f - FMath::Clamp(CurrentHealth / FMath::Max(MaxHealth, 1.0f), 0.0f, 1.0f);
    const FVector WindVelocity = MissionWindAcceleration * 0.35f;
    const float RotorWash = bEngineTurning && GetAboveGroundMeters() < 15.0f
        ? RotorRPM * (1.0f - GetAboveGroundMeters() / 15.0f)
        : 0.0f;

    float CameraDistance = 0.0f;
    if (const APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (PC->PlayerCameraManager)
        {
            CameraDistance = FVector::Distance(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation());
        }
    }
    const float DistanceVaporScale = 1.0f - FMath::Clamp((CameraDistance - 6000.0f) / 9000.0f, 0.0f, 1.0f);
    const float StartupBoost = ExhaustStartupPulseRemaining > 0.0f
        ? FMath::Sin((ExhaustStartupPulseRemaining / 0.72f) * PI)
        : 0.0f;

    for (int32 Index = 0; Index < ExhaustComponents.Num(); ++Index)
    {
        UNiagaraComponent* Exhaust = ExhaustComponents[Index];
        USceneComponent* OutletRoot = ExhaustOutletRoots.IsValidIndex(Index) ? ExhaustOutletRoots[Index].Get() : nullptr;
        if (!Exhaust || !OutletRoot) continue;

        if (!bEngineTurning)
        {
            Exhaust->SetVariableBool(TEXT("User.ExhaustEnabled"), false);
            Exhaust->Deactivate();
            continue;
        }
        if (!Exhaust->IsActive()) Exhaust->Activate(true);

        const float JetSpeed = (360.0f + EnginePower * 620.0f) * Config.VelocityMultiplier;
        const FVector JetVelocity = CurrentVelocity + OutletRoot->GetForwardVector() * JetSpeed + WindVelocity
            - FVector::UpVector * RotorWash * 65.0f;
        const float HeatRate = Config.DistortionIntensity * FMath::Lerp(18.0f, 34.0f, EnginePower);
        // Turboshaft exhaust is primarily heat shimmer. Keep visible vapor
        // sparse and continuous so it reads as a wisp, not a chain of bubbles.
        const float VaporRate = DistanceVaporScale * Config.VaporAmount
            * (FMath::Lerp(0.55f, 1.55f, EnginePower) + StartupBoost * 3.2f);

        Exhaust->SetVariableBool(TEXT("User.ExhaustEnabled"), true);
        Exhaust->SetVariableFloat(TEXT("User.EngineRPM"), RotorRPM);
        Exhaust->SetVariableFloat(TEXT("User.EnginePower"), EnginePower);
        Exhaust->SetVariableFloat(TEXT("User.ExhaustIntensity"), Config.DistortionIntensity);
        Exhaust->SetVariableFloat(TEXT("User.ExhaustTemperature"), FMath::Lerp(0.25f, 1.0f, EnginePower));
        Exhaust->SetVariableVec3(TEXT("User.AircraftVelocity"), CurrentVelocity);
        Exhaust->SetVariableVec3(TEXT("User.WindVelocity"), WindVelocity);
        Exhaust->SetVariableFloat(TEXT("User.RotorWashStrength"), RotorWash);
        Exhaust->SetVariableFloat(TEXT("User.AmbientHumidity"), 0.42f);
        Exhaust->SetVariableFloat(TEXT("User.DamageLevel"), DamageLevel);
        Exhaust->SetVariableFloat(TEXT("User.HeatSpawnRate"), HeatRate);
        Exhaust->SetVariableFloat(TEXT("User.VaporSpawnRate"), VaporRate);
        Exhaust->SetVariableVec3(TEXT("User.ExhaustJetVelocity"), JetVelocity);
    }
    bExhaustWasRunning = bEngineTurning;
}

void ARotorlineHelicopterPawn::UpdateBell222LandingGear(float DeltaSeconds)
{
    const bool bBell222 = SelectedAircraftId.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase);
    if (!bBell222)
    {
        if (Bell222LandingGearMesh)
        {
            Bell222LandingGearMesh->SetVisibility(false, true);
        }
        for (UStaticMeshComponent* Part : Bell222CompactGearParts)
        {
            if (Part) Part->SetVisibility(false, true);
        }
        return;
    }

    const float AboveGroundMeters = GetAboveGroundMeters();
    const bool bShouldRetract = bBell222LandingGearRetracted
        ? AboveGroundMeters > 7.0f && bEngineReady && !bPlayerAircraftDying
        : AboveGroundMeters > 12.0f && bEngineReady && !bPlayerAircraftDying;
    if (bShouldRetract != bBell222LandingGearRetracted)
    {
        bBell222LandingGearRetracted = bShouldRetract;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BELL222_GEAR|state=%s|agl_m=%.2f"),
            bShouldRetract ? TEXT("RETRACTING") : TEXT("DEPLOYING"),
            AboveGroundMeters);
    }

    const float TargetAlpha = bBell222LandingGearRetracted ? 1.0f : 0.0f;
    Bell222LandingGearAlpha = FMath::FInterpConstantTo(
        Bell222LandingGearAlpha, TargetAlpha, DeltaSeconds, 0.62f);

    // Use the source-authored wheel and strut assembly. It lowers into its
    // exact modeled position for landing and moves inside the fuselage while
    // retracting, then becomes hidden only once fully stowed.
    if (Bell222LandingGearMesh)
    {
        const float GearEased = FMath::SmoothStep(0.0f, 1.0f, Bell222LandingGearAlpha);
        Bell222LandingGearMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f * GearEased));
        Bell222LandingGearMesh->SetVisibility(Bell222LandingGearAlpha < 0.995f, true);
    }

    // Keep the old procedural stand-ins suppressed; the authored assembly is
    // now responsible for both deployed and retracting gear presentation.
    for (UStaticMeshComponent* Part : Bell222CompactGearParts)
    {
        if (Part) Part->SetVisibility(false, true);
    }
}

void ARotorlineHelicopterPawn::UpdateBell222BoostEffects(float DeltaSeconds)
{
    const bool bBell = SelectedAircraftId.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase);
    const bool bShowBoost = bBell && bEngineReady && bBoostActive && !bPlayerAircraftDying;
    if (bShowBoost != bBell222BoostEffectActive)
    {
        bBell222BoostEffectActive = bShowBoost;
        if (Bell222BoostAudio)
        {
            GetWorldTimerManager().ClearTimer(Bell222BoostFadeTimer);
            if (bShowBoost && Bell222BoostSound)
            {
                Bell222BoostAudio->Stop();
                Bell222BoostAudio->SetSound(Bell222BoostSound);
                Bell222BoostAudio->SetVolumeMultiplier(0.72f);
                Bell222BoostAudio->Play();
                const FTimerDelegate FadeDelegate = FTimerDelegate::CreateWeakLambda(this, [this]()
                {
                    if (Bell222BoostAudio && Bell222BoostAudio->IsPlaying())
                    {
                        Bell222BoostAudio->FadeOut(1.45f, 0.0f);
                    }
                });
                GetWorldTimerManager().SetTimer(
                    Bell222BoostFadeTimer,
                    FadeDelegate,
                    4.65f,
                    false);
            }
            else if (Bell222BoostAudio->IsPlaying())
            {
                Bell222BoostAudio->FadeOut(0.35f, 0.0f);
            }
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BELL222_TWIN_BOOST|state=%s|left=1|right=1"),
            bShowBoost ? TEXT("ACTIVE") : TEXT("IDLE"));
    }
    const float Pulse = GetWorld()
        ? 0.88f + 0.12f * FMath::Sin(GetWorld()->GetTimeSeconds() * 31.0f)
        : 1.0f;
    const FVector TargetScale = bShowBoost
        ? FVector(0.14f, 0.14f, 0.92f * Pulse)
        : FVector(0.02f, 0.02f, 0.02f);

    for (UStaticMeshComponent* Plume : { Bell222LeftBoostPlume.Get(), Bell222RightBoostPlume.Get() })
    {
        if (!Plume) continue;
        Plume->SetVisibility(bShowBoost, true);
        Plume->SetRelativeScale3D(FMath::VInterpTo(
            Plume->GetRelativeScale3D(), TargetScale, DeltaSeconds, 12.0f));
    }
    for (UPointLightComponent* BoostLight : { Bell222LeftBoostLight.Get(), Bell222RightBoostLight.Get() })
    {
        if (!BoostLight) continue;
        BoostLight->SetVisibility(bShowBoost, true);
        BoostLight->SetIntensity(bShowBoost ? 28000.0f * Pulse : 0.0f);
    }
}

bool ARotorlineHelicopterPawn::IsBell222SpecialOperations() const
{
    return SelectedAircraftId.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase) &&
        SelectedAircraftDefinition.WeaponLoadout.bEnabled;
}

bool ARotorlineHelicopterPawn::IsBellLairAuthorizedAircraft() const
{
    return IsBell222SpecialOperations() &&
        SelectedAircraftDefinition.Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase);
}

void ARotorlineHelicopterPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    InitializeBell222StealthInput();

    if (!bBell222StealthInputBound)
    {
        if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
        {
            EnhancedInput->BindAction(
                Bell222ToggleStealthAction,
                ETriggerEvent::Started,
                this,
                &ARotorlineHelicopterPawn::ToggleBell222Stealth);
            bBell222StealthInputBound = true;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_BELL222_STEALTH|INPUT_BOUND|action=IA_ToggleStealth|keyboard=K|gamepad=RightThumbstick|flight_controller=B6"));
        }
    }
}

void ARotorlineHelicopterPawn::ToggleNightVision()
{
    if (!Camera)
    {
        return;
    }

    bNightVisionEnabled = !bNightVisionEnabled;
    Camera->PostProcessSettings.bOverride_ColorSaturation = true;
    Camera->PostProcessSettings.bOverride_ColorGain = true;
    Camera->PostProcessSettings.bOverride_BloomIntensity = true;
    Camera->PostProcessSettings.ColorSaturation = bNightVisionEnabled
        ? FVector4(0.22f, 0.82f, 0.22f, 1.0f)
        : FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    Camera->PostProcessSettings.ColorGain = bNightVisionEnabled
        ? FVector4(0.30f, 1.35f, 0.34f, 1.0f)
        : FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    Camera->PostProcessSettings.BloomIntensity = bNightVisionEnabled ? 0.42f : 0.0f;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            7131,
            2.0f,
            bNightVisionEnabled ? FColor(80, 255, 110) : FColor::White,
            bNightVisionEnabled
                ? TEXT("NIGHT VISION  //  ON  [N]")
                : TEXT("NIGHT VISION  //  OFF  [N]"));
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_NIGHT_VISION|aircraft=%s|state=%s|input=N_BOUND"),
        *SelectedAircraftId,
        bNightVisionEnabled ? TEXT("ON") : TEXT("OFF"));
}

void ARotorlineHelicopterPawn::InitializeBell222StealthInput()
{
    if (!Bell222ToggleStealthAction)
    {
        Bell222ToggleStealthAction = NewObject<UInputAction>(this, TEXT("IA_ToggleStealth"));
        Bell222ToggleStealthAction->ValueType = EInputActionValueType::Boolean;
    }
    if (!Bell222StealthMappingContext)
    {
        Bell222StealthMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Bell222Stealth"));
        Bell222StealthMappingContext->MapKey(Bell222ToggleStealthAction, EKeys::K);
        Bell222StealthMappingContext->MapKey(Bell222ToggleStealthAction, EKeys::Gamepad_RightThumbstick);
    }

    if (!Bell222StealthCurve)
    {
        Bell222StealthCurve = NewObject<UCurveFloat>(this, TEXT("Curve_Bell222StealthTransition"));
        FRichCurve& Curve = Bell222StealthCurve->FloatCurve;
        const FKeyHandle StartKey = Curve.AddKey(0.0f, 0.0f);
        const FKeyHandle EndKey = Curve.AddKey(0.75f, 1.0f);
        Curve.SetKeyInterpMode(StartKey, RCIM_Cubic);
        Curve.SetKeyInterpMode(EndKey, RCIM_Cubic);
        Curve.AutoSetTangents();

        FOnTimelineFloat UpdateDelegate;
        UpdateDelegate.BindUFunction(this, FName(TEXT("UpdateBell222StealthTimeline")));
        FOnTimelineEvent FinishedDelegate;
        FinishedDelegate.BindUFunction(this, FName(TEXT("FinishBell222StealthTimeline")));
        Bell222StealthTimeline.AddInterpFloat(Bell222StealthCurve, UpdateDelegate);
        Bell222StealthTimeline.SetTimelineFinishedFunc(FinishedDelegate);
        Bell222StealthTimeline.SetTimelineLength(0.75f);
        Bell222StealthTimeline.SetLooping(false);
        Bell222StealthTimeline.SetPlaybackPosition(0.0f, false);
    }

    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
            {
                if (!InputSubsystem->HasMappingContext(Bell222StealthMappingContext))
                {
                    InputSubsystem->AddMappingContext(Bell222StealthMappingContext, 40);
                }
            }
        }
    }
}

void ARotorlineHelicopterPawn::InitializeBell222StealthMaterials()
{
    if (!IsBell222SpecialOperations())
    {
        return;
    }

    Bell222StealthParentMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Game/Vehicles/UserAdded/CombatReady/Bell222X/Stealth/"
             "M_Bell222StealthCamouflage.M_Bell222StealthCamouflage"));
    if (!Bell222StealthParentMaterial)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_BELL222_STEALTH|CACHE_FAILED|reason=MISSING_PARENT_MATERIAL"));
        return;
    }

    TArray<UMeshComponent*> CandidateMeshes;
    GetComponents(CandidateMeshes);
    int32 TextureBackedSlotCount = 0;
    int32 NeutralFallbackSlotCount = 0;
    for (UMeshComponent* Mesh : CandidateMeshes)
    {
        if (!Mesh || !Mesh->IsAttachedTo(VisualRoot))
        {
            continue;
        }
        const FString ComponentName = Mesh->GetName();
        if (ComponentName.Contains(TEXT("BoostPlume")) ||
            ComponentName.Contains(TEXT("MuzzleFlash")))
        {
            continue;
        }

        for (int32 SlotIndex = 0; SlotIndex < Mesh->GetNumMaterials(); ++SlotIndex)
        {
            UMaterialInterface* OriginalMaterial = Mesh->GetMaterial(SlotIndex);
            if (!OriginalMaterial)
            {
                continue;
            }

            UMaterialInstanceDynamic* StealthMaterial = UMaterialInstanceDynamic::Create(
                Bell222StealthParentMaterial,
                this,
                *FString::Printf(TEXT("MID_Bell222Stealth_%s_%d"), *ComponentName, SlotIndex));
            if (!StealthMaterial)
            {
                continue;
            }

            FLinearColor OriginalTint = FLinearColor::White;
            TArray<FMaterialParameterInfo> VectorParameters;
            TArray<FGuid> VectorParameterIds;
            OriginalMaterial->GetAllVectorParameterInfo(VectorParameters, VectorParameterIds);
            for (const FMaterialParameterInfo& Parameter : VectorParameters)
            {
                const FString ParameterName = Parameter.Name.ToString();
                if ((ParameterName.Contains(TEXT("Base"), ESearchCase::IgnoreCase) ||
                     ParameterName.Contains(TEXT("Color"), ESearchCase::IgnoreCase) ||
                     ParameterName.Contains(TEXT("Tint"), ESearchCase::IgnoreCase) ||
                     ParameterName.Contains(TEXT("Albedo"), ESearchCase::IgnoreCase) ||
                     ParameterName.Contains(TEXT("Diffuse"), ESearchCase::IgnoreCase)) &&
                    OriginalMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), OriginalTint))
                {
                    break;
                }
            }
            TArray<FMaterialParameterInfo> TextureParameters;
            TArray<FGuid> TextureParameterIds;
            OriginalMaterial->GetAllTextureParameterInfo(TextureParameters, TextureParameterIds);
            bool bOriginalTextureAssigned = false;
            for (const FMaterialParameterInfo& Parameter : TextureParameters)
            {
                const FString ParameterName = Parameter.Name.ToString();
                const bool bSurfaceColorTexture =
                    !ParameterName.Contains(TEXT("Normal"), ESearchCase::IgnoreCase) &&
                    !ParameterName.Contains(TEXT("Rough"), ESearchCase::IgnoreCase) &&
                    !ParameterName.Contains(TEXT("Metal"), ESearchCase::IgnoreCase) &&
                    !ParameterName.Contains(TEXT("Occlusion"), ESearchCase::IgnoreCase) &&
                    !ParameterName.Contains(TEXT("Specular"), ESearchCase::IgnoreCase) &&
                    !ParameterName.Contains(TEXT("Emissive"), ESearchCase::IgnoreCase);
                UTexture* OriginalTexture = nullptr;
                if (bSurfaceColorTexture &&
                    OriginalMaterial->GetTextureParameterValue(FHashedMaterialParameterInfo(Parameter), OriginalTexture) &&
                    OriginalTexture)
                {
                    StealthMaterial->SetTextureParameterValue(TEXT("OriginalTexture"), OriginalTexture);
                    bOriginalTextureAssigned = true;
                    break;
                }
            }
            if (!bOriginalTextureAssigned)
            {
                TArray<UTexture*> UsedTextures;
                OriginalMaterial->GetUsedTextures(UsedTextures);
                UTexture* BestColorTexture = nullptr;
                int32 BestTextureScore = -1;
                for (UTexture* UsedTexture : UsedTextures)
                {
                    if (!UsedTexture)
                    {
                        continue;
                    }
                    const FString TextureName = UsedTexture->GetName();
                    const bool bNonColorTexture =
                        TextureName.Contains(TEXT("Normal"), ESearchCase::IgnoreCase) ||
                        TextureName.Contains(TEXT("Rough"), ESearchCase::IgnoreCase) ||
                        TextureName.Contains(TEXT("Metal"), ESearchCase::IgnoreCase) ||
                        TextureName.Contains(TEXT("Specular"), ESearchCase::IgnoreCase) ||
                        TextureName.Contains(TEXT("Occlusion"), ESearchCase::IgnoreCase) ||
                        TextureName.Contains(TEXT("Emissive"), ESearchCase::IgnoreCase) ||
                        TextureName.EndsWith(TEXT("_N"), ESearchCase::IgnoreCase) ||
                        TextureName.EndsWith(TEXT("_R"), ESearchCase::IgnoreCase) ||
                        TextureName.EndsWith(TEXT("_M"), ESearchCase::IgnoreCase) ||
                        TextureName.EndsWith(TEXT("_AO"), ESearchCase::IgnoreCase);
                    if (bNonColorTexture)
                    {
                        continue;
                    }
                    int32 TextureScore = 0;
                    if (TextureName.Contains(TEXT("BaseColor"), ESearchCase::IgnoreCase)) TextureScore += 100;
                    if (TextureName.Contains(TEXT("Diffuse"), ESearchCase::IgnoreCase)) TextureScore += 90;
                    if (TextureName.Contains(TEXT("Albedo"), ESearchCase::IgnoreCase)) TextureScore += 80;
                    if (TextureName.Contains(TEXT("Color"), ESearchCase::IgnoreCase)) TextureScore += 70;
                    if (TextureName.EndsWith(TEXT("_D"), ESearchCase::IgnoreCase)) TextureScore += 60;
                    if (TextureScore > BestTextureScore)
                    {
                        BestTextureScore = TextureScore;
                        BestColorTexture = UsedTexture;
                    }
                }
                if (BestColorTexture)
                {
                    StealthMaterial->SetTextureParameterValue(TEXT("OriginalTexture"), BestColorTexture);
                    bOriginalTextureAssigned = true;
                }
            }
            if (bOriginalTextureAssigned)
            {
                ++TextureBackedSlotCount;
            }
            else
            {
                OriginalTint = FLinearColor(0.045f, 0.055f, 0.065f, 1.0f);
                ++NeutralFallbackSlotCount;
            }
            StealthMaterial->SetVectorParameterValue(TEXT("OriginalTint"), OriginalTint);
            StealthMaterial->SetScalarParameterValue(TEXT("StealthAmount"), 0.0f);

            Bell222StealthMeshComponents.Add(Mesh);
            Bell222StealthMaterialSlots.Add(SlotIndex);
            Bell222OriginalMaterials.Add(OriginalMaterial);
            Bell222StealthMaterialInstances.Add(StealthMaterial);
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL222_STEALTH|CACHE_READY|components=%d|slots=%d|texture_backed=%d|neutral_fallback=%d|transition=0.75|opacity_floor=0.22"),
        CandidateMeshes.Num(),
        Bell222StealthMaterialInstances.Num(),
        TextureBackedSlotCount,
        NeutralFallbackSlotCount);
}

void ARotorlineHelicopterPawn::ApplyBell222StealthMaterials()
{
    if (bBell222StealthMaterialsApplied)
    {
        return;
    }
    const int32 SlotCount = FMath::Min3(
        Bell222StealthMeshComponents.Num(),
        Bell222StealthMaterialSlots.Num(),
        Bell222StealthMaterialInstances.Num());
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        if (UMeshComponent* Mesh = Bell222StealthMeshComponents[Index].Get())
        {
            Mesh->SetMaterial(Bell222StealthMaterialSlots[Index], Bell222StealthMaterialInstances[Index]);
        }
    }
    bBell222StealthMaterialsApplied = SlotCount > 0;
}

void ARotorlineHelicopterPawn::RestoreBell222OriginalMaterials()
{
    const int32 SlotCount = FMath::Min3(
        Bell222StealthMeshComponents.Num(),
        Bell222StealthMaterialSlots.Num(),
        Bell222OriginalMaterials.Num());
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        if (UMeshComponent* Mesh = Bell222StealthMeshComponents[Index].Get())
        {
            Mesh->SetMaterial(Bell222StealthMaterialSlots[Index], Bell222OriginalMaterials[Index]);
        }
    }
    bBell222StealthMaterialsApplied = false;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL222_STEALTH|ORIGINALS_RESTORED|slots=%d"), SlotCount);
}

void ARotorlineHelicopterPawn::ResetBell222StealthMaterials()
{
    Bell222StealthTimeline.Stop();
    if (bBell222StealthMaterialsApplied)
    {
        RestoreBell222OriginalMaterials();
    }
    Bell222StealthMeshComponents.Reset();
    Bell222StealthMaterialSlots.Reset();
    Bell222OriginalMaterials.Reset();
    Bell222StealthMaterialInstances.Reset();
    Bell222StealthParentMaterial = nullptr;
    bStealthActive = false;
    bBell222DecloakAudioPrimed = false;
    Bell222StealthAmount = 0.0f;
    Bell222StealthExpiresAt = -1000.0;
    Bell222StealthCooldownUntil = -1000.0;
}

void ARotorlineHelicopterPawn::ToggleBell222Stealth()
{
    if (!IsBell222SpecialOperations() || Bell222StealthMaterialInstances.IsEmpty())
    {
        return;
    }

    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (!bStealthActive && Now < Bell222StealthCooldownUntil)
    {
        const float Remaining = static_cast<float>(Bell222StealthCooldownUntil - Now);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(7123, 1.2f, FColor(255, 190, 70),
                FString::Printf(TEXT("CLOAK COOLDOWN // %.0f S"), Remaining));
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BELL222_STEALTH|BLOCKED|reason=COOLDOWN|remaining_s=%.2f"), Remaining);
        return;
    }

    SetBell222StealthActive(!bStealthActive, bStealthActive ? TEXT("MANUAL") : TEXT("INPUT"));
}

void ARotorlineHelicopterPawn::SetBell222StealthActive(bool bActive, const TCHAR* Reason)
{
    if (!GetWorld() || bActive == bStealthActive)
    {
        return;
    }

    constexpr double ActiveDurationSeconds = 12.0;
    constexpr double CooldownDurationSeconds = 30.0;
    const double Now = GetWorld()->GetTimeSeconds();
    bStealthActive = bActive;
    if (bActive)
    {
        bBell222DecloakAudioPrimed = false;
        Bell222StealthExpiresAt = Now + ActiveDurationSeconds;
        ApplyBell222StealthMaterials();
        Bell222StealthTimeline.PlayFromStart();
        ResetCombatThreatState();
        PlayBell222StealthTransitionAudio(true);
    }
    else
    {
        Bell222StealthExpiresAt = -1000.0;
        Bell222StealthCooldownUntil = Now + CooldownDurationSeconds;
        Bell222StealthTimeline.Reverse();
        if (!bBell222DecloakAudioPrimed)
        {
            PlayBell222StealthTransitionAudio(false);
        }
        bBell222DecloakAudioPrimed = false;
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL222_STEALTH|TOGGLE|active=%d|amount=%.3f|reason=%s|active_s=%.1f|cooldown_s=%.1f|enemy_tracking=%s"),
        bActive ? 1 : 0,
        Bell222StealthAmount,
        Reason,
        bActive ? ActiveDurationSeconds : 0.0,
        bActive ? 0.0 : CooldownDurationSeconds,
        bActive ? TEXT("SUPPRESSED") : TEXT("ENABLED"));
}

void ARotorlineHelicopterPawn::UpdateBell222StealthTimeline(float Value)
{
    Bell222StealthAmount = FMath::Clamp(Value, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* Material : Bell222StealthMaterialInstances)
    {
        if (Material)
        {
            Material->SetScalarParameterValue(TEXT("StealthAmount"), Bell222StealthAmount);
        }
    }
}

void ARotorlineHelicopterPawn::FinishBell222StealthTimeline()
{
    if (bStealthActive)
    {
        Bell222StealthAmount = 1.0f;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BELL222_STEALTH|STATE|active=1|amount=1.000|visible_fraction=0.22-0.30"));
    }
    else
    {
        Bell222StealthAmount = 0.0f;
        RestoreBell222OriginalMaterials();
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BELL222_STEALTH|STATE|active=0|amount=0.000|original_materials=1"));
    }
}

void ARotorlineHelicopterPawn::PlayBell222StealthTransitionAudio(bool bActivating)
{
    USoundBase* TransitionSound = bActivating ? Bell222StealthSound : Bell222DecloakSound;
    if (!Bell222StealthAudio || !TransitionSound)
    {
        return;
    }
    Bell222StealthAudio->Stop();
    Bell222StealthAudio->SetSound(TransitionSound);
    Bell222StealthAudio->SetPitchMultiplier(1.0f);
    Bell222StealthAudio->SetVolumeMultiplier(0.72f);
    Bell222StealthAudio->Play();
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL222_STEALTH|AUDIO|transition=%s|asset=%s"),
        bActivating ? TEXT("CLOAK") : TEXT("DECLOAK"), *TransitionSound->GetPathName());
}

bool ARotorlineHelicopterPawn::IsBell222GunMode() const
{
    return Bell222WeaponMode == ERotorlineBellWeaponMode::Chain50 ||
        Bell222WeaponMode == ERotorlineBellWeaponMode::Cannon40 ||
        Bell222WeaponMode == ERotorlineBellWeaponMode::Linked;
}

bool ARotorlineHelicopterPawn::IsBell222MissileMode() const
{
    return Bell222WeaponMode == ERotorlineBellWeaponMode::Aim9 ||
        Bell222WeaponMode == ERotorlineBellWeaponMode::Hellfire ||
        Bell222WeaponMode == ERotorlineBellWeaponMode::Maverick;
}

const FRotorlineAircraftWeaponModeDefinition* ARotorlineHelicopterPawn::FindBell222WeaponDefinition(const TCHAR* Id) const
{
    return SelectedAircraftDefinition.WeaponLoadout.Modes.FindByPredicate(
        [Id](const FRotorlineAircraftWeaponModeDefinition& Mode)
        {
            return Mode.Id.Equals(Id, ESearchCase::IgnoreCase);
        });
}

const FRotorlineAircraftWeaponModeDefinition* ARotorlineHelicopterPawn::GetBell222WeaponDefinition() const
{
    switch (Bell222WeaponMode)
    {
    case ERotorlineBellWeaponMode::Chain50: return FindBell222WeaponDefinition(TEXT("chain50"));
    case ERotorlineBellWeaponMode::Cannon40: return FindBell222WeaponDefinition(TEXT("cannon40"));
    case ERotorlineBellWeaponMode::Linked: return FindBell222WeaponDefinition(TEXT("linked"));
    case ERotorlineBellWeaponMode::Aim9: return FindBell222WeaponDefinition(TEXT("aim9"));
    case ERotorlineBellWeaponMode::Hellfire: return FindBell222WeaponDefinition(TEXT("hellfire"));
    case ERotorlineBellWeaponMode::Maverick: return FindBell222WeaponDefinition(TEXT("maverick"));
    default: return nullptr;
    }
}

int32 ARotorlineHelicopterPawn::GetBell222WeaponAmmo() const
{
    if (Bell222WeaponMode == ERotorlineBellWeaponMode::Linked)
    {
        return FMath::Min(Bell222WeaponAmmo.FindRef(TEXT("chain50")), Bell222WeaponAmmo.FindRef(TEXT("cannon40")));
    }
    const FRotorlineAircraftWeaponModeDefinition* Mode = GetBell222WeaponDefinition();
    return Mode ? Bell222WeaponAmmo.FindRef(Mode->Id.ToLower()) : 0;
}

int32 ARotorlineHelicopterPawn::GetBell222WeaponCapacity() const
{
    if (Bell222WeaponMode == ERotorlineBellWeaponMode::Linked)
    {
        return FMath::Min(Bell222WeaponCapacity.FindRef(TEXT("chain50")), Bell222WeaponCapacity.FindRef(TEXT("cannon40")));
    }
    const FRotorlineAircraftWeaponModeDefinition* Mode = GetBell222WeaponDefinition();
    return Mode ? Bell222WeaponCapacity.FindRef(Mode->Id.ToLower()) : 0;
}

void ARotorlineHelicopterPawn::ConfigureBell222WeaponComponents()
{
    const bool bBell = IsBell222SpecialOperations();
    if (Bell222LeftSourceGun)
    {
        Bell222LeftSourceGun->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Vehicles/UserAdded/CombatReady/Bell222X/Bell222X_LeftGun/Bell222X_LeftGun/StaticMeshes/Bell222X_LeftGun.Bell222X_LeftGun")));
        Bell222LeftSourceGun->SetRelativeTransform(FTransform::Identity);
    }
    if (Bell222RightSourceGun)
    {
        Bell222RightSourceGun->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Vehicles/UserAdded/CombatReady/Bell222X/Bell222X_RightGun/Bell222X_RightGun/StaticMeshes/Bell222X_RightGun.Bell222X_RightGun")));
        Bell222RightSourceGun->SetRelativeTransform(FTransform::Identity);
    }
    if (Bell222LeftGunRoot) Bell222LeftGunRoot->SetRelativeLocation(SelectedAircraftDefinition.WeaponLoadout.LeftGunMount);
    if (Bell222RightGunRoot) Bell222RightGunRoot->SetRelativeLocation(SelectedAircraftDefinition.WeaponLoadout.RightGunMount);
    if (Bell222MissilePodRoot) Bell222MissilePodRoot->SetRelativeLocation(SelectedAircraftDefinition.WeaponLoadout.BellyPodMount);
    if (!bBell)
    {
        ResetBell222WeaponSystem();
        return;
    }
    Bell222WeaponMode = ERotorlineBellWeaponMode::Safe;
    Bell222ModeSelectedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    UpdateBell222WeaponVisuals(0.0f);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL222_WEAPON|state=CONFIGURED|gun_mounts=%.1f,%.1f,%.1f;%.1f,%.1f,%.1f|pod=%.1f,%.1f,%.1f|arc=+/-%.0f"),
        SelectedAircraftDefinition.WeaponLoadout.LeftGunMount.X,
        SelectedAircraftDefinition.WeaponLoadout.LeftGunMount.Y,
        SelectedAircraftDefinition.WeaponLoadout.LeftGunMount.Z,
        SelectedAircraftDefinition.WeaponLoadout.RightGunMount.X,
        SelectedAircraftDefinition.WeaponLoadout.RightGunMount.Y,
        SelectedAircraftDefinition.WeaponLoadout.RightGunMount.Z,
        SelectedAircraftDefinition.WeaponLoadout.BellyPodMount.X,
        SelectedAircraftDefinition.WeaponLoadout.BellyPodMount.Y,
        SelectedAircraftDefinition.WeaponLoadout.BellyPodMount.Z,
        SelectedAircraftDefinition.WeaponLoadout.PodArcDegrees);
}

void ARotorlineHelicopterPawn::ResetBell222WeaponSystem()
{
    Bell222WeaponMode = ERotorlineBellWeaponMode::Safe;
    Bell222GunDeploymentAlpha = 0.0f;
    Bell222PodDeploymentAlpha = 0.0f;
    Bell222WeaponLockProgress = 0.0f;
    Bell222LinkedHeat = 0.0f;
    Bell222PodYawDegrees = 0.0f;
    Bell222LockedTarget.Reset();
    if (Bell222LeftSourceGun) Bell222LeftSourceGun->SetVisibility(false, true);
    if (Bell222RightSourceGun) Bell222RightSourceGun->SetVisibility(false, true);
    for (UStaticMeshComponent* Part : Bell222GunHousings) if (Part) Part->SetVisibility(false, true);
    for (UStaticMeshComponent* Part : Bell222GunDoors) if (Part) Part->SetVisibility(false, true);
    for (UStaticMeshComponent* Part : Bell222GunBarrels) if (Part) Part->SetVisibility(false, true);
    for (UStaticMeshComponent* Part : Bell222MuzzleFlashes) if (Part) Part->SetVisibility(false, true);
    for (UStaticMeshComponent* Part : Bell222MissilePodDoors) if (Part) Part->SetVisibility(false, true);
    if (Bell222MissilePodBody) Bell222MissilePodBody->SetVisibility(false, true);
    for (UStaticMeshComponent* Part : Bell222PodMissiles) if (Part) Part->SetVisibility(false, true);
}

FString ARotorlineHelicopterPawn::GetBell222WeaponSystemState() const
{
    if (!IsBell222SpecialOperations() || bPlayerAircraftDying) return TEXT("UNAVAILABLE");
    if (Bell222WeaponMode == ERotorlineBellWeaponMode::Safe)
    {
        return Bell222GunDeploymentAlpha > 0.01f || Bell222PodDeploymentAlpha > 0.01f
            ? TEXT("RETRACTING") : TEXT("SAFE / CONCEALED");
    }
    const float Alpha = IsBell222GunMode() ? Bell222GunDeploymentAlpha : Bell222PodDeploymentAlpha;
    if (Alpha < 0.985f) return TEXT("DEPLOYING");
    return TEXT("READY");
}

void ARotorlineHelicopterPawn::CycleBell222Weapon(int32 Direction)
{
    if (!IsBell222SpecialOperations()) return;
    constexpr int32 ModeCount = 7;
    const int32 Current = static_cast<int32>(Bell222WeaponMode);
    Bell222WeaponMode = static_cast<ERotorlineBellWeaponMode>((Current + Direction + ModeCount) % ModeCount);
    Bell222ModeSelectedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    Bell222WeaponLockProgress = 0.0f;
    Bell222LockedTarget.Reset();
    const FRotorlineAircraftWeaponModeDefinition* Mode = GetBell222WeaponDefinition();
    const FString ModeName = Mode ? Mode->DisplayName : TEXT("WEAPONS SAFE");
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(7113, 2.0f,
            Bell222WeaponMode == ERotorlineBellWeaponMode::Safe ? FColor::Silver : FColor(90, 255, 160),
            FString::Printf(TEXT("BELL 222 WEAPON  //  %s  //  %d REMAINING"), *ModeName, GetBell222WeaponAmmo()));
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL222_WEAPON|state=SELECTED|mode=%s|ammo=%d|gun=%d|missile=%d"),
        *ModeName, GetBell222WeaponAmmo(), IsBell222GunMode() ? 1 : 0, IsBell222MissileMode() ? 1 : 0);
}

bool ARotorlineHelicopterPawn::IsBell222TargetValid(
    const ARotorlineMissionObjectiveActor* Candidate,
    const FRotorlineAircraftWeaponModeDefinition& Mode,
    float& OutDistanceMeters,
    bool& bOutInsideArc) const
{
    OutDistanceMeters = 0.0f;
    bOutInsideArc = false;
    if (!IsValid(Candidate) || !Candidate->IsDestroyObjective() || Candidate->IsDestroyedTarget() ||
        Candidate->GetHealthFraction() <= 0.0f || !Bell222MissilePodRoot || !GetWorld()) return false;

    const bool bAircraft = Candidate->IsAircraftThreat();
    const FString TargetClass = Mode.TargetClass.ToLower();
    if (TargetClass == TEXT("air") && !bAircraft) return false;
    if ((TargetClass == TEXT("ground") || TargetClass == TEXT("major_ground")) && bAircraft) return false;
    if (TargetClass == TEXT("major_ground"))
    {
        const ERotorlineThreatType Threat = Candidate->GetThreatType();
        const FString Label = Candidate->GetTargetLabel().ToLower();
        const bool bMajor = Threat == ERotorlineThreatType::Tank || Threat == ERotorlineThreatType::RocketArtillery ||
            Threat == ERotorlineThreatType::RadarMissile || Label.Contains(TEXT("bunker")) ||
            Label.Contains(TEXT("radar")) || Label.Contains(TEXT("structure")) || Label.Contains(TEXT("building"));
        if (!bMajor) return false;
    }

    const FVector Origin = Bell222MissilePodRoot->GetComponentLocation();
    const FVector ToTarget = Candidate->GetAimLocation() - Origin;
    OutDistanceMeters = ToTarget.Size() / 100.0f;
    if (OutDistanceMeters > Mode.MaxRangeMeters) return false;
    const FVector LocalDirection = VisualRoot->GetComponentTransform().InverseTransformVectorNoScale(ToTarget.GetSafeNormal());
    const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X));
    bOutInsideArc = FMath::Abs(Bearing) <= SelectedAircraftDefinition.WeaponLoadout.PodArcDegrees;
    if (!bOutInsideArc) return false;

    FHitResult CoverHit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RotorlineBell222TargetLineOfSight), true, this);
    Params.AddIgnoredActor(this);
    const FVector TraceStart = Origin + ToTarget.GetSafeNormal() * 80.0f;
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
        CoverHit, TraceStart, Candidate->GetAimLocation(), ECC_Visibility, Params);
    return !bBlocked || CoverHit.GetActor() == Candidate;
}

ARotorlineMissionObjectiveActor* ARotorlineHelicopterPawn::FindBestBell222Target(
    const FRotorlineAircraftWeaponModeDefinition& Mode,
    float& OutDistanceMeters,
    bool& bOutInsideArc) const
{
    OutDistanceMeters = 0.0f;
    bOutInsideArc = false;
    ARotorlineMissionObjectiveActor* Best = nullptr;
    float BestScore = -TNumericLimits<float>::Max();
    for (TActorIterator<ARotorlineMissionObjectiveActor> It(GetWorld()); It; ++It)
    {
        float DistanceMeters = 0.0f;
        bool bInsideArc = false;
        ARotorlineMissionObjectiveActor* Candidate = *It;
        if (!IsBell222TargetValid(Candidate, Mode, DistanceMeters, bInsideArc)) continue;
        const FVector ToTarget = (Candidate->GetAimLocation() - Bell222MissilePodRoot->GetComponentLocation()).GetSafeNormal();
        const float AimDot = FVector::DotProduct(VisualRoot->GetForwardVector(), ToTarget);
        // The articulated pod can track across its full mechanical arc, but
        // acquisition must still be deliberate. Fourteen degrees provides a
        // usable sight picture over small ground vehicles without returning
        // to the old whole-screen target sweep.
        constexpr float AcquisitionDot = 0.9702957f; // cos(14 degrees)
        if (AimDot < AcquisitionDot) continue;
        const float Score = AimDot * 10000.0f - DistanceMeters * 0.12f;
        if (Score > BestScore)
        {
            Best = Candidate;
            BestScore = Score;
            OutDistanceMeters = DistanceMeters;
            bOutInsideArc = bInsideArc;
        }
    }
    return Best;
}

void ARotorlineHelicopterPawn::UpdateBell222WeaponSystem(float DeltaSeconds)
{
    if (!IsBell222SpecialOperations()) return;
    const FRotorlineAircraftWeaponLoadout& Loadout = SelectedAircraftDefinition.WeaponLoadout;
    const float GunTarget = IsBell222GunMode() && !bPlayerAircraftDying ? 1.0f : 0.0f;
    const float PodTarget = IsBell222MissileMode() && !bPlayerAircraftDying ? 1.0f : 0.0f;
    Bell222GunDeploymentAlpha = FMath::FInterpConstantTo(
        Bell222GunDeploymentAlpha, GunTarget, DeltaSeconds, 1.0f / FMath::Max(0.05f, Loadout.GunDeploymentDuration));
    Bell222PodDeploymentAlpha = FMath::FInterpConstantTo(
        Bell222PodDeploymentAlpha, PodTarget, DeltaSeconds, 1.0f / FMath::Max(0.05f, Loadout.PodDeploymentDuration));
    Bell222LinkedHeat = FMath::Max(0.0f, Bell222LinkedHeat - DeltaSeconds * 24.0f);

    const FRotorlineAircraftWeaponModeDefinition* Mode = GetBell222WeaponDefinition();
    if (Mode && IsBell222MissileMode() && Bell222PodDeploymentAlpha >= 0.985f)
    {
        float DistanceMeters = 0.0f;
        bool bInsideArc = false;
        ARotorlineMissionObjectiveActor* Candidate = Bell222LockedTarget.Get();
        bool bKeepCurrentTarget = IsBell222TargetValid(
            Candidate, *Mode, DistanceMeters, bInsideArc);
        if (bKeepCurrentTarget)
        {
            const FVector ToCurrent = (Candidate->GetAimLocation() -
                Bell222MissilePodRoot->GetComponentLocation()).GetSafeNormal();
            // Retention gets modest hysteresis so normal airframe and sight
            // motion cannot repeatedly drop a valid lock on a small vehicle.
            constexpr float TrackingDot = 0.9396926f; // cos(20 degrees)
            bKeepCurrentTarget =
                FVector::DotProduct(VisualRoot->GetForwardVector(), ToCurrent) >= TrackingDot;
        }
        if (!bKeepCurrentTarget)
        {
            Candidate = FindBestBell222Target(*Mode, DistanceMeters, bInsideArc);
        }
        if (Candidate != Bell222LockedTarget.Get())
        {
            Bell222LockedTarget = Candidate;
            Bell222WeaponLockProgress = 0.0f;
        }
        else if (Candidate)
        {
            Bell222WeaponLockProgress = FMath::Min(1.0f,
                Bell222WeaponLockProgress + DeltaSeconds / FMath::Max(0.05f, Mode->LockSeconds));
            const FVector LocalDirection = VisualRoot->GetComponentTransform().InverseTransformVectorNoScale(
                (Candidate->GetAimLocation() - Bell222MissilePodRoot->GetComponentLocation()).GetSafeNormal());
            const float DesiredYaw = FMath::Clamp(
                FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X)),
                -Loadout.PodArcDegrees, Loadout.PodArcDegrees);
            Bell222PodYawDegrees = FMath::FInterpTo(Bell222PodYawDegrees, DesiredYaw, DeltaSeconds, 6.0f);
        }
        else
        {
            Bell222WeaponLockProgress = 0.0f;
            Bell222LockedTarget.Reset();
            Bell222PodYawDegrees = FMath::FInterpTo(Bell222PodYawDegrees, 0.0f, DeltaSeconds, 5.0f);
        }
    }
    else
    {
        Bell222WeaponLockProgress = 0.0f;
        Bell222LockedTarget.Reset();
        Bell222PodYawDegrees = FMath::FInterpTo(Bell222PodYawDegrees, 0.0f, DeltaSeconds, 5.0f);
    }
    UpdateBell222WeaponVisuals(DeltaSeconds);
}

void ARotorlineHelicopterPawn::UpdateBell222WeaponVisuals(float DeltaSeconds)
{
    const bool bBell = IsBell222SpecialOperations();
    const float GunEased = FMath::SmoothStep(0.0f, 1.0f, Bell222GunDeploymentAlpha);
    const float PodEased = FMath::SmoothStep(0.0f, 1.0f, Bell222PodDeploymentAlpha);
    // The imported left/right meshes are the model's own open doors, barrel
    // clusters, brackets and authored materials. They begin tucked inside the
    // sponsons and travel to the source model's exact firing position.
    const float SourceStowOffset = 62.0f * (1.0f - GunEased);
    if (Bell222LeftSourceGun)
    {
        Bell222LeftSourceGun->SetVisibility(bBell && GunEased > 0.015f, true);
        Bell222LeftSourceGun->SetRelativeLocation(FVector(0.0f, SourceStowOffset, 0.0f));
    }
    if (Bell222RightSourceGun)
    {
        Bell222RightSourceGun->SetVisibility(bBell && GunEased > 0.015f, true);
        Bell222RightSourceGun->SetRelativeLocation(FVector(0.0f, -SourceStowOffset, 0.0f));
    }

    // Keep the old procedural shapes as non-rendered muzzle transforms only.
    // The source-authored assemblies already contain their deployed lateral
    // position, so applying the former procedural GunDistance here pushed the
    // flashes and projectile origins out near the sponson tips. Anchor every
    // gun mode to the actual inboard barrel stations instead.
    if (Bell222LeftGunRoot)
    {
        Bell222LeftGunRoot->SetRelativeLocation(
            SelectedAircraftDefinition.WeaponLoadout.LeftGunMount);
    }
    if (Bell222RightGunRoot)
    {
        Bell222RightGunRoot->SetRelativeLocation(
            SelectedAircraftDefinition.WeaponLoadout.RightGunMount);
    }
    for (UStaticMeshComponent* Housing : Bell222GunHousings)
    {
        if (!Housing) continue;
        Housing->SetVisibility(false, true);
        Housing->SetRelativeScale3D(FVector(
            0.82f,
            0.76f * FMath::Max(0.08f, GunEased),
            0.22f));
    }
    for (int32 Index = 0; Index < Bell222GunDoors.Num(); ++Index)
    {
        UStaticMeshComponent* Door = Bell222GunDoors[Index];
        if (!Door) continue;
        // The former loose slab doors read as detached geometry. The solid
        // housing above is the visible deployable bay; keep these internal.
        Door->SetVisibility(false, true);
    }
    for (int32 Index = 0; Index < Bell222GunBarrels.Num(); ++Index)
    {
        UStaticMeshComponent* Barrel = Bell222GunBarrels[Index];
        if (!Barrel) continue;
        Barrel->SetVisibility(false, true);
        const bool bHeavy = Index % 3 == 2;
        const int32 LocalIndex = Index % 3;
        Barrel->SetRelativeLocation(FVector(
            60.0f,
            LocalIndex == 0 ? -13.0f : (LocalIndex == 1 ? 13.0f : 0.0f),
            bHeavy ? -18.0f : 12.0f));
    }
    for (int32 Index = 0; Index < Bell222MuzzleFlashes.Num(); ++Index)
    {
        UStaticMeshComponent* Flash = Bell222MuzzleFlashes[Index];
        if (!Flash) continue;
        const bool bHeavy = Index % 3 == 2;
        const int32 LocalIndex = Index % 3;
        Flash->SetRelativeLocation(FVector(
            bHeavy ? 118.0f : 108.0f,
            LocalIndex == 0 ? -13.0f : (LocalIndex == 1 ? 13.0f : 0.0f),
            bHeavy ? -18.0f : 12.0f));
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        Flash->SetVisibility(bBell && Bell222MuzzleFlashUntil.IsValidIndex(Index) && Now < Bell222MuzzleFlashUntil[Index], true);
    }

    for (int32 Index = 0; Index < Bell222MissilePodDoors.Num(); ++Index)
    {
        UStaticMeshComponent* Door = Bell222MissilePodDoors[Index];
        if (!Door) continue;
        Door->SetVisibility(bBell && PodEased > 0.01f, true);
        Door->SetRelativeRotation(FRotator(0.0f, 0.0f, (Index == 0 ? -72.0f : 72.0f) * PodEased));
    }
    if (Bell222MissilePodRoot) Bell222MissilePodRoot->SetRelativeRotation(FRotator(0.0f, Bell222PodYawDegrees, 0.0f));
    const float PodZ = -6.0f - SelectedAircraftDefinition.WeaponLoadout.PodDeploymentDistance * PodEased;
    if (Bell222MissilePodBody)
    {
        Bell222MissilePodBody->SetRelativeLocation(FVector(0.0f, 0.0f, PodZ));
        Bell222MissilePodBody->SetVisibility(bBell && PodEased > 0.03f, true);
    }
    for (int32 Index = 0; Index < Bell222PodMissiles.Num(); ++Index)
    {
        UStaticMeshComponent* Missile = Bell222PodMissiles[Index];
        if (!Missile) continue;
        Missile->SetRelativeLocation(FVector(30.0f, (Index - 1.5f) * 15.0f, PodZ - 10.0f));
        Missile->SetVisibility(bBell && PodEased > 0.03f && GetBell222WeaponAmmo() > Index, true);
    }
}

void ARotorlineHelicopterPawn::TriggerBell222MuzzleFlash(int32 MuzzleIndex, float Duration)
{
    if (Bell222MuzzleFlashUntil.IsValidIndex(MuzzleIndex) && GetWorld())
    {
        Bell222MuzzleFlashUntil[MuzzleIndex] = GetWorld()->GetTimeSeconds() + Duration;
    }
}

void ARotorlineHelicopterPawn::FireBell222ProjectilePair(
    const FRotorlineAircraftWeaponModeDefinition& Mode,
    bool bFortyMillimeter)
{
    if (!GetWorld()) return;
    FVector SightMuzzle;
    FVector SightDirection;
    FVector SightImpact;
    bool bBlocking = false;
    if (!GetApacheWeaponAimSolution(SightMuzzle, SightDirection, SightImpact, bBlocking)) return;
    // The Bell has two gun stations. Alternate the light-gun barrel used on
    // each side instead of spawning four fully simulated projectile actors
    // per firing interval. Each retained round carries the damage of the two
    // former same-side rounds, preserving DPS and ammunition consumption.
    const int32 ChainPhase = FMath::FloorToInt(
        GetWorld()->GetTimeSeconds() / FMath::Max(0.01f, Mode.FireInterval)) & 1;
    const int32 Indices[] = {
        bFortyMillimeter ? 2 : (ChainPhase == 0 ? 0 : 1),
        bFortyMillimeter ? 5 : (ChainPhase == 0 ? 3 : 4)
    };

    // Preserve the authored spacing between the Bell's left and right gun
    // stations, but place their shared midpoint on the sight axis. The source
    // mounts have a small common left/down bias, which makes both streams miss
    // the HUD pipper in the same direction even though their convergence math
    // is otherwise correct.
    FVector PairMidpoint = FVector::ZeroVector;
    int32 ValidMuzzleCount = 0;
    for (const int32 MuzzleIndex : Indices)
    {
        if (Bell222MuzzleFlashes.IsValidIndex(MuzzleIndex) && Bell222MuzzleFlashes[MuzzleIndex])
        {
            PairMidpoint += Bell222MuzzleFlashes[MuzzleIndex]->GetComponentLocation();
            ++ValidMuzzleCount;
        }
    }
    FVector PairCenterCorrection = FVector::ZeroVector;
    if (ValidMuzzleCount > 0)
    {
        PairMidpoint /= static_cast<float>(ValidMuzzleCount);
        const float PairForwardDistance = FVector::DotProduct(PairMidpoint - SightMuzzle, SightDirection);
        const FVector SightAxisAtMuzzles = SightMuzzle + SightDirection * PairForwardDistance;
        PairCenterCorrection = SightAxisAtMuzzles - PairMidpoint;
    }

    for (int32 PairIndex = 0; PairIndex < UE_ARRAY_COUNT(Indices); ++PairIndex)
    {
        const int32 MuzzleIndex = Indices[PairIndex];
        if (MuzzleIndex == INDEX_NONE || !Bell222MuzzleFlashes.IsValidIndex(MuzzleIndex)) continue;
        // The Bell has one authored cannon station on each side. Launch from
        // those physical mounts and converge both rounds on the exact shared
        // HUD impact point. A virtual centerline offset incorrectly placed
        // both visible streams on the same side of the aircraft.
        const FVector Muzzle = Bell222MuzzleFlashes[MuzzleIndex]->GetComponentLocation() + PairCenterCorrection;
        FVector Direction = (SightImpact - Muzzle).GetSafeNormal();
        if (Direction.IsNearlyZero()) Direction = SightDirection;
        // All three Bell gun modes share the same small lower-left residual in
        // packaged play. Apply one Bell-only angular boresight trim after the
        // paired mounts are centered so both physical streams remain intact.
        const FVector SightRight = VisualRoot ? VisualRoot->GetRightVector() : GetActorRightVector();
        const FVector SightUp = VisualRoot ? VisualRoot->GetUpVector() : GetActorUpVector();
        constexpr float BellBoresightRightTrim = 0.0045f;
        constexpr float BellBoresightUpTrim = 0.0045f;
        Direction = (Direction +
            SightRight * BellBoresightRightTrim +
            SightUp * BellBoresightUpTrim).GetSafeNormal();
        // The Bell's paired gun stations converge on the exact shared
        // boresight. Random cone dispersion made the rounds visibly miss the
        // HUD aim point, especially while correcting for aircraft drift.
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        if (ARotorlineCannonProjectile* Projectile = GetWorld()->SpawnActor<ARotorlineCannonProjectile>(
            ARotorlineCannonProjectile::StaticClass(), Muzzle, Direction.Rotation(), SpawnParams))
        {
            Projectile->LaunchAdvanced(
                Muzzle, Direction, bFortyMillimeter ? Mode.Damage : Mode.Damage * 2.0f, Mode.ProjectileSpeed,
                bFortyMillimeter ? Mode.BlastRadius : 0.0f,
                bFortyMillimeter ? TEXT("BELL_40MM_DIRECT") : TEXT("BELL_50CAL_DIRECT"),
                bFortyMillimeter ? 1.7f : 0.62f);
            TriggerBell222MuzzleFlash(MuzzleIndex, bFortyMillimeter ? 0.10f : 0.045f);
        }
    }
}

void ARotorlineHelicopterPawn::FireBell222GunMode()
{
    if (!IsBell222GunMode() || Bell222GunDeploymentAlpha < 0.985f || !bEngineReady ||
        bMissionFailed || bMissionComplete || bPlayerAircraftDying || !GetWorld()) return;
    const FRotorlineAircraftWeaponModeDefinition* SelectedMode = GetBell222WeaponDefinition();
    if (!SelectedMode) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - Bell222LastGunFireTime < SelectedMode->FireInterval) return;

    bool bFired = false;
    if (Bell222WeaponMode == ERotorlineBellWeaponMode::Chain50 || Bell222WeaponMode == ERotorlineBellWeaponMode::Linked)
    {
        int32& Ammo = Bell222WeaponAmmo.FindOrAdd(TEXT("chain50"));
        if (Ammo >= 4)
        {
            const FRotorlineAircraftWeaponModeDefinition* Chain = FindBell222WeaponDefinition(TEXT("chain50"));
            if (Chain) FireBell222ProjectilePair(*Chain, false);
            Ammo -= 4;
            bFired = true;
            StartApacheCannonAudio(TEXT("BELL_50CAL"));
        }
    }
    if (Bell222WeaponMode == ERotorlineBellWeaponMode::Cannon40 || Bell222WeaponMode == ERotorlineBellWeaponMode::Linked)
    {
        int32& Ammo = Bell222WeaponAmmo.FindOrAdd(TEXT("cannon40"));
        if (Ammo >= 2 && (Bell222WeaponMode != ERotorlineBellWeaponMode::Linked || Bell222LinkedHeat < 92.0f))
        {
            const FRotorlineAircraftWeaponModeDefinition* Cannon = FindBell222WeaponDefinition(TEXT("cannon40"));
            if (Cannon) FireBell222ProjectilePair(*Cannon, true);
            Ammo -= 2;
            bFired = true;
            if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Combat/SFX_TankCannon.SFX_TankCannon")))
            {
                UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(),
                    0.48f * GetAudioMix(ERotorlineAudioChannel::WeaponsExplosions) *
                    (IsSpokenDialogueActive() ? 0.18f : 1.0f));
            }
        }
    }
    if (!bFired) return;
    Bell222LastGunFireTime = Now;
    Bell222LinkedHeat = FMath::Min(100.0f, Bell222LinkedHeat +
        (Bell222WeaponMode == ERotorlineBellWeaponMode::Linked ? 18.0f : 4.0f));
    if (ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(Controller))
    {
        OperationsController->NotifyWeaponFired();
    }
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        PC->PlayDynamicForceFeedback(
            Bell222WeaponMode == ERotorlineBellWeaponMode::Chain50 ? 0.32f : 0.72f,
            0.08f, true, true, true, true);
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL222_WEAPON|state=FIRED|mode=%s|ammo=%d|linked_heat=%.1f"),
        *SelectedMode->DisplayName, GetBell222WeaponAmmo(), Bell222LinkedHeat);
}

void ARotorlineHelicopterPawn::FireBell222MissileMode()
{
    if (!IsBell222MissileMode() || Bell222PodDeploymentAlpha < 0.985f || !bEngineReady ||
        bMissionFailed || bMissionComplete || bPlayerAircraftDying || !GetWorld()) return;
    const FRotorlineAircraftWeaponModeDefinition* Mode = GetBell222WeaponDefinition();
    if (!Mode) return;
    int32& Ammo = Bell222WeaponAmmo.FindOrAdd(Mode->Id.ToLower());
    if (Ammo <= 0)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(7111, 1.5f, FColor::Red, TEXT("SELECTED MISSILE EMPTY"));
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - Bell222LastMissileFireTime < Mode->FireInterval) return;
    float DistanceMeters = 0.0f;
    bool bInsideArc = false;
    ARotorlineMissionObjectiveActor* Target = Bell222LockedTarget.Get();
    if (Bell222WeaponLockProgress < 0.999f || !IsBell222TargetValid(Target, *Mode, DistanceMeters, bInsideArc))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(7111, 1.4f, FColor::Red,
            !bInsideArc && Target ? TEXT("LAUNCH INHIBITED // TARGET OUTSIDE ADF ARC") : TEXT("LAUNCH INHIBITED // VALID LOCK REQUIRED"));
        return;
    }

    const FVector PodForward = Bell222MissilePodRoot->GetForwardVector().GetSafeNormal();
    const FVector PodRight = Bell222MissilePodRoot->GetRightVector().GetSafeNormal();
    const FVector LaunchLocation = Bell222MissilePodRoot->GetComponentLocation() +
        PodForward * 95.0f + PodRight * ((Ammo % 2 == 0) ? -24.0f : 24.0f) - VisualRoot->GetUpVector() * 90.0f;
    FVector LaunchDirection = (Target->GetAimLocation() - LaunchLocation).GetSafeNormal();
    if (LaunchDirection.IsNearlyZero()) LaunchDirection = PodForward;
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    ARotorlineRocketProjectile* Missile = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
        ARotorlineRocketProjectile::StaticClass(), LaunchLocation, LaunchDirection.Rotation(), SpawnParams);
    if (!Missile) return;
    Missile->LaunchPlayerWeapon(
        LaunchLocation, LaunchDirection, Target, Mode->Id, Mode->Damage,
        Mode->MinimumBlastDamage, Mode->BlastRadius, Mode->ProjectileSpeed, Mode->ProjectileAsset);
    --Ammo;
    Bell222LastMissileFireTime = Now;
    Bell222WeaponLockProgress = 0.0f;
    if (ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(Controller))
    {
        OperationsController->NotifyWeaponFired();
    }
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        PC->PlayDynamicForceFeedback(0.70f, 0.16f, true, true, true, true);
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL222_WEAPON|state=MISSILE_AWAY|mode=%s|target=%s|ammo=%d|distance=%.0f|inside_arc=%d"),
        *Mode->DisplayName, *Target->GetTargetLabel(), Ammo, DistanceMeters, bInsideArc ? 1 : 0);
}

bool ARotorlineHelicopterPawn::GetBellWeaponTargetData(FVector& OutWorldLocation, FString& OutLabel) const
{
    if (!IsBell222SpecialOperations() || Bell222WeaponLockProgress < 0.999f ||
        !Bell222LockedTarget.IsValid()) return false;
    const FRotorlineAircraftWeaponModeDefinition* Mode = GetBell222WeaponDefinition();
    float DistanceMeters = 0.0f;
    bool bInsideArc = false;
    if (!Mode || !IsBell222MissileMode() ||
        !IsBell222TargetValid(Bell222LockedTarget.Get(), *Mode, DistanceMeters, bInsideArc))
    {
        return false;
    }
    OutWorldLocation = Bell222LockedTarget->GetAimLocation();
    OutLabel = FString::Printf(
        TEXT("LOCKED // %s // %.0f M"),
        *Bell222LockedTarget->GetTargetLabel(),
        DistanceMeters);
    return true;
}

void ARotorlineHelicopterPawn::AddCatalogFallbackMainRotor(const FBox& Bounds)
{
    UStaticMesh* DiscMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!DiscMesh)
    {
        return;
    }
    const FVector Size = Bounds.GetSize();
    const FVector Center = Bounds.GetCenter();
    const float Radius = FMath::Max(Size.X, Size.Y) * 0.58f;
    const float Thickness = FMath::Max(Radius * 0.008f, 1.0f);
    CatalogFallbackMainRotorPivot = NewObject<USceneComponent>(this);
    AddInstanceComponent(CatalogFallbackMainRotorPivot);
    CatalogFallbackMainRotorPivot->SetupAttachment(MeshAlignment);
    CatalogFallbackMainRotorPivot->SetRelativeLocation(FVector(Center.X, Center.Y, Bounds.Max.Z + Size.Z * 0.04f));
    CatalogFallbackMainRotorPivot->RegisterComponent();

    UStaticMeshComponent* Disc = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Disc);
    Disc->SetupAttachment(CatalogFallbackMainRotorPivot);
    Disc->SetStaticMesh(DiscMesh);
    Disc->SetRelativeScale3D(FVector(Radius / 50.0f, Radius / 50.0f, Thickness / 100.0f));
    Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Disc->SetCastShadow(false);
    if (UMaterialInterface* RotorBlur = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/Blockout/M_Glass_Blockout.M_Glass_Blockout")))
    {
        Disc->SetMaterial(0, RotorBlur);
    }
    Disc->RegisterComponent();
    CatalogDynamicStaticParts.Add(Disc);
    CatalogMainRotorParts.Add(CatalogFallbackMainRotorPivot);
    CatalogMainRotorAxes.Add(FVector::UpVector);
}

void ARotorlineHelicopterPawn::AddCatalogFallbackTailRotor(const FBox& Bounds)
{
    UStaticMesh* DiscMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!DiscMesh)
    {
        return;
    }
    const FVector Size = Bounds.GetSize();
    const FVector Center = Bounds.GetCenter();
    const float Radius = FMath::Max(Size.Z * 0.34f, FMath::Min(Size.X, Size.Y) * 0.30f);
    const float Thickness = FMath::Max(Radius * 0.025f, 1.0f);
    bCatalogFallbackTailUsesXAxis = Size.X > Size.Y;
    const FVector PivotLocation = bCatalogFallbackTailUsesXAxis
        ? FVector(Bounds.Min.X, Center.Y, Center.Z + Size.Z * 0.10f)
        : FVector(Center.X, Bounds.Min.Y, Center.Z + Size.Z * 0.10f);
    CatalogFallbackTailRotorPivot = NewObject<USceneComponent>(this);
    AddInstanceComponent(CatalogFallbackTailRotorPivot);
    CatalogFallbackTailRotorPivot->SetupAttachment(MeshAlignment);
    CatalogFallbackTailRotorPivot->SetRelativeLocation(PivotLocation);
    CatalogFallbackTailRotorPivot->RegisterComponent();

    UStaticMeshComponent* Disc = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Disc);
    Disc->SetupAttachment(CatalogFallbackTailRotorPivot);
    Disc->SetStaticMesh(DiscMesh);
    Disc->SetRelativeScale3D(FVector(Radius / 50.0f, Radius / 50.0f, Thickness / 100.0f));
    Disc->SetRelativeRotation(bCatalogFallbackTailUsesXAxis
        ? FRotator(0.0f, 90.0f, 0.0f)
        : FRotator(-90.0f, 0.0f, 0.0f));
    Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Disc->SetCastShadow(false);
    if (UMaterialInterface* RotorBlur = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/Blockout/M_Glass_Blockout.M_Glass_Blockout")))
    {
        Disc->SetMaterial(0, RotorBlur);
    }
    Disc->RegisterComponent();
    CatalogDynamicStaticParts.Add(Disc);
    CatalogTailRotorParts.Add(CatalogFallbackTailRotorPivot);
    CatalogTailRotorAxes.Add(bCatalogFallbackTailUsesXAxis ? FVector::ForwardVector : FVector::RightVector);
}

FString ARotorlineHelicopterPawn::GetCraftDisplayName() const
{
    return !SelectedAircraftDefinition.DisplayName.IsEmpty()
        ? SelectedAircraftDefinition.DisplayName
        : FRotorlineMissionCatalog::CraftDisplayName(SelectedCraft);
}

TArray<USkeletalMeshComponent*> ARotorlineHelicopterPawn::GetActiveRotors() const
{
    if (bUseCatalogAircraft)
    {
        TArray<USkeletalMeshComponent*> ActiveComponents;
        for (USkeletalMeshComponent* Part : CatalogDynamicSkeletalParts)
        {
            if (Part && Part->IsVisible())
            {
                ActiveComponents.Add(Part);
            }
        }
        return ActiveComponents;
    }
    if (SelectedCraft == ERotorlineCraftType::AttackMD500)
    {
        // The superior MH-6 uses model-native static assemblies driven by
        // dedicated pivots, not imported skeletal clips.
        return {};
    }
    return { MainRotorMesh.Get(), TailRotorMesh.Get() };
}

void ARotorlineHelicopterPawn::ApplyActiveRotorAnimationRates()
{
    if (!bUseCatalogAircraft && SelectedCraft == ERotorlineCraftType::AttackMD500)
    {
        return;
    }

    // The standard Huey uses the same frame-continuous component driver as the
    // Marine Huey. Leave its imported animation paused; it contains an end hold
    // that causes a visible hitch during the low-RPM startup ramp.
    if (!bUseCatalogAircraft &&
        SelectedAircraftId.Equals(TEXT("uh1_huey"), ESearchCase::IgnoreCase))
    {
        MainRotorMesh->SetPlayRate(0.0f);
        TailRotorMesh->SetPlayRate(0.0f);
        return;
    }

    for (USkeletalMeshComponent* Rotor : GetActiveRotors())
    {
        Rotor->SetPlayRate(CurrentRotorPlayRate);
    }
}

void ARotorlineHelicopterPawn::UpdateHueyRotorAnimation(float DeltaSeconds)
{
    if (bUseCatalogAircraft ||
        !SelectedAircraftId.Equals(TEXT("uh1_huey"), ESearchCase::IgnoreCase) ||
        CurrentRotorPlayRate <= 0.0f ||
        !HueyMainRotorPivot ||
        !HueyTailRotorPivot)
    {
        return;
    }

    // Match the Marine UH-1's continuous rotor integration exactly. There is
    // no clip boundary, phase reset, or quantized pose sampling, so the startup
    // acceleration stays smooth from the first visible blade movement onward.
    const float MainSpinDegrees = DeltaSeconds * 180.0f * CurrentRotorPlayRate;
    const float TailSpinDegrees = MainSpinDegrees * 1.6f;
    HueyMainRotorPivot->AddLocalRotation(
        FQuat(FVector::UpVector, FMath::DegreesToRadians(MainSpinDegrees)));
    HueyTailRotorPivot->AddLocalRotation(
        FQuat(FVector::ForwardVector, FMath::DegreesToRadians(TailSpinDegrees)));
}

void ARotorlineHelicopterPawn::UpdateMD500RotorAnimation(float DeltaSeconds)
{
    if (bUseCatalogAircraft ||
        SelectedCraft != ERotorlineCraftType::AttackMD500 ||
        CurrentRotorPlayRate <= 0.0f ||
        !MD500MainRotorPivot ||
        !MD500TailRotorPivot)
    {
        return;
    }

    const float RotorPower = FMath::Clamp(
        CurrentRotorPlayRate / FMath::Max(0.01f, RotorFlightPlayRate),
        0.0f,
        1.0f);
    const float MainSpinDegrees = DeltaSeconds * 2160.0f * RotorPower;
    const float TailSpinDegrees = DeltaSeconds * 8640.0f * RotorPower;
    MD500MainRotorPivot->AddLocalRotation(
        FQuat(FVector::UpVector, FMath::DegreesToRadians(MainSpinDegrees)));
    MD500TailRotorPivot->AddLocalRotation(
        FQuat(FVector::RightVector, FMath::DegreesToRadians(TailSpinDegrees)));
    MD500MainRotorIntegratedDegrees += MainSpinDegrees;
    MD500TailRotorIntegratedDegrees += TailSpinDegrees;
}

void ARotorlineHelicopterPawn::UpdateCatalogRotorVisuals(float DeltaSeconds)
{
    if (!bUseCatalogAircraft || CurrentRotorPlayRate <= 0.0f)
    {
        return;
    }

    const bool bBell222 = SelectedAircraftId.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase);
    const float RotorPower = FMath::Clamp(
        CurrentRotorPlayRate / FMath::Max(0.01f, RotorFlightPlayRate), 0.0f, 1.0f);
    // The Bell uses independent static rotor assemblies rather than a authored
    // rotor animation. Drive them at believable governed RPM while preserving
    // the existing smooth startup power curve.
    const float MainSpinDegrees = bBell222
        ? DeltaSeconds * 1980.0f * RotorPower
        : DeltaSeconds * 180.0f * CurrentRotorPlayRate;
    const float TailSpinDegrees = bBell222
        ? DeltaSeconds * 6480.0f * RotorPower
        : MainSpinDegrees * 1.6f;
    for (int32 RotorIndex = 0; RotorIndex < CatalogMainRotorParts.Num(); ++RotorIndex)
    {
        USceneComponent* Part = CatalogMainRotorParts[RotorIndex];
        if (!Part || !Part->IsVisible()) continue;
        if (RotorAnimation && Part->IsA<USkeletalMeshComponent>()) continue;
        const FVector Axis = CatalogMainRotorAxes.IsValidIndex(RotorIndex)
            ? CatalogMainRotorAxes[RotorIndex].GetSafeNormal()
            : FVector::UpVector;
        Part->AddLocalRotation(FQuat(Axis, FMath::DegreesToRadians(MainSpinDegrees)));
    }
    for (int32 RotorIndex = 0; RotorIndex < CatalogTailRotorParts.Num(); ++RotorIndex)
    {
        USceneComponent* Part = CatalogTailRotorParts[RotorIndex];
        if (!Part || !Part->IsVisible()) continue;
        if (RotorAnimation && Part->IsA<USkeletalMeshComponent>()) continue;
        const FVector Axis = CatalogTailRotorAxes.IsValidIndex(RotorIndex)
            ? CatalogTailRotorAxes[RotorIndex].GetSafeNormal()
            : FVector::ForwardVector;
        Part->AddLocalRotation(FQuat(Axis, FMath::DegreesToRadians(TailSpinDegrees)));
    }
}

float ARotorlineHelicopterPawn::GetAudioMix(ERotorlineAudioChannel Channel) const
{
    if (const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(Controller))
    {
        return OperationsController->GetEffectiveAudioVolume(Channel);
    }
    return 1.0f;
}

float ARotorlineHelicopterPawn::GetMissionRadioVolume() const
{
    return GetAudioMix(ERotorlineAudioChannel::Radio);
}

float ARotorlineHelicopterPawn::GetMissionEngineVolume() const
{
    return GetAudioMix(ERotorlineAudioChannel::Engine);
}

void ARotorlineHelicopterPawn::RefreshAudioMix()
{
    const float EngineMix = GetAudioMix(ERotorlineAudioChannel::Engine);
    const float MusicMix = GetAudioMix(ERotorlineAudioChannel::Music);
    const float RadioMix = GetAudioMix(ERotorlineAudioChannel::Radio);
    const bool bDialogueActive = IsSpokenDialogueActive();
    const float EngineDialogueDuck = bDialogueActive ? 0.48f : 1.0f;
    const float MusicDialogueDuck = bDialogueActive ? 0.42f : 1.0f;
    const float WarningDialogueDuck = bDialogueActive ? 0.42f : 1.0f;
    const float WeaponDialogueDuck = bDialogueActive ? 0.18f : 1.0f;
    EngineStartupAudio->SetVolumeMultiplier((bMissionBriefActive ? EngineDuckedVolume : EngineStartupVolume) * EngineMix);
    EngineTakeoffAudio->SetVolumeMultiplier(EngineFlightVolume * EngineMix * EngineDialogueDuck);
    EngineFlightAudio->SetVolumeMultiplier(
        (bMissionBriefActive ? EngineDuckedVolume : EngineFlightVolume * EngineDialogueDuck) * EngineMix);
    MissionBriefAudio->SetVolumeMultiplier(MissionBriefVolume * RadioMix);
    InstructorAudio->SetVolumeMultiplier(0.92f * RadioMix);
    MissionMusicAudio->SetVolumeMultiplier(MissionMusicVolume * MusicMix * MusicDialogueDuck);
    RadioAudio->SetVolumeMultiplier(0.78f * RadioMix);
    RadioSquelchAudio->SetVolumeMultiplier(0.78f * RadioMix);
    // Warnings belong to the Radio / Warnings bus. Keeping them off the
    // weapons bus prevents a combat slider change from bypassing the user's
    // warning level and prevents refreshes from changing their loudness.
    ThreatAlertAudio->SetVolumeMultiplier(ThreatAlertBaseVolume * RadioMix * WarningDialogueDuck);
    ApacheCannonAudio->SetVolumeMultiplier(
        0.72f * GetAudioMix(ERotorlineAudioChannel::WeaponsExplosions) * WeaponDialogueDuck);
}

void ARotorlineHelicopterPawn::BeginPlay()
{
    Super::BeginPlay();

    InitializeBell222StealthInput();

    if (!bDeploymentConfigured)
    {
        ActiveMission.Id = TEXT("free-flight");
        ActiveMission.Title = TEXT("Free Flight");
        ActiveMission.Callsign = TEXT("ROTARY ONE");
        ApplyCraftConfiguration();
        UpdateAircraftLightStations();
        RouteMissionBriefAudio();
    }
    MissionStartTime = GetWorld()->GetTimeSeconds();
    ObjectiveStartTime = MissionStartTime;
    LastAmbientChatterTime = MissionStartTime;
    const bool bGoldenHourMission = ActiveMission.Id.Equals(TEXT("medevac"), ESearchCase::IgnoreCase);
    const bool bRooftopExtractionMission = ActiveMission.Id.Equals(TEXT("rooftop-extraction"), ESearchCase::IgnoreCase);
    const bool bEnemyFootholdMission = ActiveMission.Id.Equals(TEXT("enemy-foothold"), ESearchCase::IgnoreCase);
    const bool bCabinSupplyConvoyMission = ActiveMission.Id.Equals(TEXT("cabin-supply-convoy"), ESearchCase::IgnoreCase);
    const bool bSurvivorExtractionMission = ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase);
    const bool bFinalEvacuationMission = ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase);
    const bool bKiowaStrikeMission = ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase);
    const bool bKiowaAircraft =
        SelectedAircraftId.Equals(TEXT("oh58_kiowa"), ESearchCase::IgnoreCase);
    const bool bKiowaSensorMission = bKiowaAircraft &&
        (bKiowaStrikeMission || ActiveMission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase));
    const bool bBell222Opening = IsBell222SpecialOperations() && MissionBriefSound == Bell222FinalMissionBriefSound;
    if (bGoldenHourMission || bRooftopExtractionMission || bEnemyFootholdMission || bCabinSupplyConvoyMission ||
        bSurvivorExtractionMission || bFinalEvacuationMission ||
        bBell222Opening || bKiowaStrikeMission)
    {
        constexpr float GoldenHourMinimumRadioDelaySeconds = 90.0f;
        const float BriefDuration = MissionBriefSound ? MissionBriefSound->GetDuration() : 0.0f;
        const float MinimumRadioDelay = bGoldenHourMission ? GoldenHourMinimumRadioDelaySeconds : 0.0f;
        const float Bell222OpeningWindow = bBell222Opening ? BriefDuration + 1.0f : 0.0f;
        const float RadioDelay = bKiowaStrikeMission ? 180.0f : FMath::Max(
            MinimumRadioDelay,
            FMath::Max(Bell222OpeningWindow, BriefDuration + 3.0f));
        MissionRadioHoldUntil = MissionStartTime + RadioDelay;
        GetWorldTimerManager().SetTimer(
            MissionRadioHoldTimer,
            this,
            &ARotorlineHelicopterPawn::ReleaseMissionRadioHold,
            RadioDelay,
            false);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSION_RADIO|mission=%s|state=HELD|release_seconds=%.1f|brief_seconds=%.1f"),
            *ActiveMission.Id,
            RadioDelay,
            BriefDuration);
    }
    CurrentHealth = MaxHealth;
    FuelRemainingPercent = 100.0f;
    CurrentFuelBurnMultiplier = 1.0f;
    if (bKiowaStrikeMission)
    {
        SpawnKiowaReconGroundHarassment();
    }
    bFuelStarved = false;
    bFuelLowWarningIssued = false;
    bFuelCriticalWarningIssued = false;
    bFuelFumesWarningIssued = false;
    bMissionFailed = false;

    if (bKiowaSensorMission)
    {
        ActiveKiowaStrikeMission = GetWorld()->SpawnActor<ARotorlineKiowaStrikeMissionActor>(
            ARotorlineKiowaStrikeMissionActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
        if (ActiveKiowaStrikeMission)
        {
            ActiveKiowaStrikeMission->Configure(this, bKiowaStrikeMission);
        }
    }
    bMissionComplete = false;
    bPlayerAircraftDying = false;
    bPlayerCrashImpact = false;
    PlayerDeathElapsed = 0.0f;
    PlayerCrashImpactElapsed = 0.0f;
    PlayerCrashFallSpeed = 0.0f;
    MissionFailureReason.Reset();
    FString CombatLoopTestScenario;
    bCombatLoopTestMode = FParse::Value(
        FCommandLine::Get(),
        TEXT("RotorlineCombatLoopTest="),
        CombatLoopTestScenario);
    bCombatLoopPlayerFatalTriggered = false;
    bHawkRidgeQualificationMode =
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineHawkRidgeTest"));
    bHawkRidgeQualificationLockObserved = false;
    bHawkRidgeQualificationLaunchObserved = false;
    HawkRidgeQualificationElapsed = 0.0f;
    bRadarLockCalloutPlayed = false;
    LastRadioMessageTimes.Reset();
    NextRadioPlaybackAllowedTime = -1000.0;
    LastTacticalRadioCategoryTimes.Reset();
    PlayedRadioCalloutAssetsThisMission.Reset();
    ActiveEnemyHelicopterEncounters.Reset();
    EnemyHelicopterCooldownUntil = -1000.0;
    LastEnemyHelicopterSpawnAuthorizationTime = -1000.0;
    LastEnemyHelicopterBlockLogTime = -1000.0;
    LastEnemyHelicopterBlockReason.Reset();
    EnemyHelicopterEncounterGeneration = 0;
    bEnemyHelicopterCooldownActive = false;
    bEnemyHelicopterTerminalShutdownLogged = false;
    FString MissionLoopTestScenario;
    bMissionLoopTestMode = FParse::Value(
        FCommandLine::Get(),
        TEXT("RotorlineMissionLoopTest="),
        MissionLoopTestScenario);
    bEnemyHelicopterEncounterGateTestMode =
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineAirEncounterGateTest")) || bMissionLoopTestMode;
    bTutorialHelicopterKillTestMode =
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineTutorialHelicopterKillTest"));
    TutorialHelicopterKillTestStage = 0;
    TutorialHelicopterKillTestStartTime = GetWorld()->GetTimeSeconds();
    EnemyHelicopterEncounterGateTestStage = 0;
    EnemyHelicopterEncounterGateTestStartTime = GetWorld()->GetTimeSeconds();
    EnemyHelicopterEncounterGateTestCooldownStartTime = -1000.0;
    LoadRadioCallouts();
    SpawnFinalMissionSetPieces();
    int32 DestroyObjectives = 0;
    for (const FRotorlineObjectiveDefinition& Objective : ActiveMission.Objectives)
    {
        if (Objective.Kind == TEXT("destroy"))
        {
            ++DestroyObjectives;
        }
    }
    RocketAmmoCapacity = bSelectedAircraftArmed ? FMath::Max(12, DestroyObjectives * 4) : 0;
    ApacheCannonAmmoCapacity = HasAttackCombatPackage() ? 300 : 0;
    if (SelectedAircraftId.Equals(TEXT("md500_defender"), ESearchCase::IgnoreCase))
    {
        // The replacement MH-6 carries two seven-tube pods.
        RocketAmmoCapacity = bSelectedAircraftArmed ? 14 : 0;
    }
    RocketAmmo = RocketAmmoCapacity;
    ApacheCannonAmmo = ApacheCannonAmmoCapacity;
    Bell222WeaponAmmo.Reset();
    Bell222WeaponCapacity.Reset();
    if (IsBell222SpecialOperations())
    {
        for (const FRotorlineAircraftWeaponModeDefinition& Mode : SelectedAircraftDefinition.WeaponLoadout.Modes)
        {
            Bell222WeaponCapacity.Add(Mode.Id.ToLower(), Mode.Ammo);
            Bell222WeaponAmmo.Add(Mode.Id.ToLower(), Mode.Ammo);
        }
        // The Bell owns discrete data-driven stores; generic Apache racks stay
        // empty so rearm/HUD logic cannot double-count fictional ordnance.
        RocketAmmoCapacity = 0;
        RocketAmmo = 0;
        ApacheCannonAmmoCapacity = 0;
        ApacheCannonAmmo = 0;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BELL222_WEAPON|state=LOADOUT_READY|chain50=%d|cannon40=%d|aim9=%d|hellfire=%d|maverick=%d"),
            Bell222WeaponAmmo.FindRef(TEXT("chain50")), Bell222WeaponAmmo.FindRef(TEXT("cannon40")),
            Bell222WeaponAmmo.FindRef(TEXT("aim9")), Bell222WeaponAmmo.FindRef(TEXT("hellfire")),
            Bell222WeaponAmmo.FindRef(TEXT("maverick")));
    }
    // Every deployment-ready airframe carries a defensive flare/chaff suite.
    // Support crews should not be forced to outrun guided missiles with no defense.
    CountermeasureCharges = CountermeasureCapacity;
    LastCountermeasureFireTime = -1000.0;
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineBaseRearmTest")) ||
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineCityRearmTest")))
    {
        RocketAmmo = FMath::Max(0, RocketAmmoCapacity - 5);
        ApacheCannonAmmo = FMath::Max(0, ApacheCannonAmmoCapacity - 75);
        CountermeasureCharges = FMath::Max(0, CountermeasureCapacity - 2);
        CurrentHealth = MaxHealth * 0.4f;
        FuelRemainingPercent = 35.0f;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BASE_SERVICE_TEST|state=SERVICE_NEEDED|rockets=%d/%d|cannon=%d/%d|countermeasures=%d/%d|health=%.1f/%.1f|fuel=%.1f"),
            RocketAmmo, RocketAmmoCapacity, ApacheCannonAmmo, ApacheCannonAmmoCapacity,
            CountermeasureCharges, CountermeasureCapacity, CurrentHealth, MaxHealth, FuelRemainingPercent);
    }
    BaseRearmProgress = 0.0f;
    bBaseRearmLatched = false;
    bInsideBaseServiceZone = false;
    bInsideCityServiceZone = false;
    bBaseRearmActive = false;
    DiscoverMapHelipads();
    ApacheCannonHeat = 0.0f;
    bApacheCannonOverheated = false;
    ApplyMissionConditions();
    UpdateMissionMusic();

    SetActorLocationAndRotation(
        FVector(RotorlineHelicopter::SpawnX, RotorlineHelicopter::SpawnY, RotorlineHelicopter::SpawnZ),
        FRotator(0.0f, RotorlineHelicopter::SpawnYaw, 0.0f),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineDownwashPreview")))
    {
        FHitResult PreviewGroundHit;
        const FVector PreviewTraceStart = GetActorLocation() + FVector(0.0f, 0.0f, 5000.0f);
        const FVector PreviewTraceEnd = GetActorLocation() - FVector(0.0f, 0.0f, 10000.0f);
        FCollisionQueryParams PreviewTraceParams(SCENE_QUERY_STAT(RotorlineDownwashPreview), false, this);
        PreviewTraceParams.AddIgnoredActor(this);
        if (GetWorld()->LineTraceSingleByChannel(
            PreviewGroundHit,
            PreviewTraceStart,
            PreviewTraceEnd,
            ECC_Visibility,
            PreviewTraceParams))
        {
            const float PreviewClearanceCm = 700.0f;
            const float PreviewZ = PreviewGroundHit.ImpactPoint.Z +
                CollisionBox->GetScaledBoxExtent().Z + PreviewClearanceCm;
            SetActorLocation(
                FVector(GetActorLocation().X, GetActorLocation().Y, PreviewZ),
                false,
                nullptr,
                ETeleportType::TeleportPhysics);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_DOWNWASH|PREVIEW|agl_m=%.1f|surface_z=%.1f"),
                PreviewClearanceCm * 0.01f,
                PreviewGroundHit.ImpactPoint.Z);
        }
    }
    if (bCombatLoopTestMode)
    {
        AddActorWorldOffset(FVector(0.0f, 0.0f, 15000.0f), false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COMBAT_LOOP_TEST|PLAYER_SETUP|scenario=%s|altitude_offset_m=150|state=WAITING_FOR_ENEMY_DESTROYED"),
            *CombatLoopTestScenario);
    }
    if (bHawkRidgeQualificationMode)
    {
        // Low-level qualification point on the northern main-valley road. The
        // western ridge battery is roughly 415 m away and 180 m above it, so
        // this exercises the same masked/unmasked engagement geometry a player
        // encounters while using the road as a nap-of-earth route.
        const FVector CorridorXY(-27000.0f, 116000.0f, 0.0f);
        FHitResult GroundHit;
        FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(RotorlineHawkRidgeQualification), false, this);
        GroundParams.AddIgnoredActor(this);
        const bool bFoundGround = GetWorld()->LineTraceSingleByChannel(
            GroundHit,
            CorridorXY + FVector(0.0f, 0.0f, 100000.0f),
            CorridorXY - FVector(0.0f, 0.0f, 20000.0f),
            ECC_Visibility,
            GroundParams);
        const float GroundZ = bFoundGround ? GroundHit.ImpactPoint.Z : 13600.0f;
        const float QualificationZ = GroundZ + CollisionBox->GetScaledBoxExtent().Z + 2500.0f;
        SetActorLocationAndRotation(
            FVector(CorridorXY.X, CorridorXY.Y, QualificationZ),
            FRotator(0.0f, 17.0f, 0.0f),
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        CurrentVelocity = FVector::ZeroVector;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_HAWK_RIDGE_TEST|PLAYER_SETUP|aircraft=%s|location=%.1f,%.1f,%.1f|ground_z=%.1f|agl_m=25|state=WAITING_FOR_RADAR_LOCK"),
            *SelectedAircraftId,
            GetActorLocation().X,
            GetActorLocation().Y,
            GetActorLocation().Z,
            GroundZ);
    }
    InitializeRotorDownwash();

    if (RotorAnimation)
    {
        for (USkeletalMeshComponent* Rotor : GetActiveRotors())
        {
            Rotor->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Rotor->SetAnimation(RotorAnimation);
            Rotor->Play(true);
        }
        CurrentRotorPlayRate = RotorStartPlayRate;
        ApplyActiveRotorAnimationRates();
    }

    if (ActiveMission.Id.Equals(TEXT("cabin-supply-convoy"), ESearchCase::IgnoreCase))
    {
        MissionBriefSound = LoadObject<USoundBase>(
            nullptr, RotorlineHelicopter::CabinSupplyConvoyMissionBriefPath);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_M23_AUDIO|ROUTE|asset=%s|loaded=%d"),
            RotorlineHelicopter::CabinSupplyConvoyMissionBriefPath,
            MissionBriefSound ? 1 : 0);
    }
    if (bSurvivorExtractionMission)
    {
        MissionBriefSound = LoadObject<USoundBase>(nullptr, RotorlineHelicopter::SurvivorExtractionMissionBriefPath);
    }
    else if (bFinalEvacuationMission)
    {
        MissionBriefSound = LoadObject<USoundBase>(nullptr, RotorlineHelicopter::FinalEvacuationMissionBriefPath);
    }
    if (MissionBriefSound)
    {
        bMissionBriefActive = true;
        bMissionBriefPlaybackStarted = false;
        bMission1RememberCalloutPlayed = false;
        MissionBriefAudio->SetSound(MissionBriefSound);
        MissionBriefAudio->SetVolumeMultiplier(MissionBriefVolume * GetAudioMix(ERotorlineAudioChannel::Radio));
    }
    const bool bSynchronizeStartupWithMissionBrief =
        (bSurvivorExtractionMission || bFinalEvacuationMission) &&
        MissionBriefSound != nullptr;
    const float MissionBriefStartupHoldSeconds = bSynchronizeStartupWithMissionBrief
        ? FMath::Max(0.1f, MissionBriefSound->GetDuration() + 1.5f)
        : 0.0f;

    if (EnginePreIgnitionSound)
    {
        bRotorSpoolStageActive = false;
        EngineStartupElapsed = 0.0f;
        LastSpoolMilestone = -1;
        CurrentRotorPlayRate = 0.0f;
        ApplyActiveRotorAnimationRates();

        EngineStartupAudio->SetSound(EnginePreIgnitionSound);
        EngineStartupAudio->SetVolumeMultiplier((bMissionBriefActive ? EngineDuckedVolume : EngineStartupVolume) * GetAudioMix(ERotorlineAudioChannel::Engine));
        EngineStartupAudio->Play();
        const float PreIgnitionDuration = EnginePreIgnitionSound->GetDuration();
        const float PreIgnitionDelay = FMath::Max(0.1f, PreIgnitionDuration - EngineCrossfadeSeconds);
        const float SpoolDuration = EngineStartupSound ? EngineStartupSound->GetDuration() : 0.0f;
        const float SpoolDelay = EngineStartupSound
            ? FMath::Max(0.1f, SpoolDuration - EngineCrossfadeSeconds)
            : 0.0f;
        const float EngineSequenceSeconds = PreIgnitionDelay + SpoolDelay;
        const float SynchronizedSequenceSeconds = bSynchronizeStartupWithMissionBrief
            ? FMath::Max(EngineSequenceSeconds, MissionBriefStartupHoldSeconds)
            : EngineSequenceSeconds;
        const float SynchronizedSpoolSeconds = bSynchronizeStartupWithMissionBrief
            ? FMath::Max(SpoolDelay, SynchronizedSequenceSeconds - PreIgnitionDelay)
            : SpoolDelay;
        EngineSpoolDuration = FMath::Max(0.1f, SynchronizedSpoolSeconds);
        EngineReadyTime = GetWorld()->GetTimeSeconds() + SynchronizedSequenceSeconds;
        GetWorldTimerManager().SetTimer(
            EngineTransitionTimer,
            this,
            &ARotorlineHelicopterPawn::BeginRotorSpoolAudio,
            PreIgnitionDelay,
            false);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_HELICOPTER_AUDIO|PRE_IGNITION|craft=%s|duration=%.3f|transition=%.3f|next=ROTOR_SPOOL|spool_duration=%.3f|controls=LOCKED"),
            *GetCraftDisplayName(),
            PreIgnitionDuration,
            PreIgnitionDelay,
            SpoolDuration);
        if (bSynchronizeStartupWithMissionBrief)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_STARTUP_SYNC|mission=%s|phase=ARMED|brief_seconds=%.3f|engine_seconds=%.3f|sequence_seconds=%.3f|spool_seconds=%.3f|controls=LOCKED"),
                *ActiveMission.Id,
                MissionBriefSound->GetDuration(),
                EngineSequenceSeconds,
                SynchronizedSequenceSeconds,
                EngineSpoolDuration);
        }
    }
    else if (EngineStartupSound)
    {
        bRotorSpoolStageActive = true;
        EngineStartupAudio->SetSound(EngineStartupSound);
        EngineStartupAudio->SetVolumeMultiplier((bMissionBriefActive ? EngineDuckedVolume : EngineStartupVolume) * GetAudioMix(ERotorlineAudioChannel::Engine));
        EngineStartupAudio->Play();
        const float StartupDuration = EngineStartupSound->GetDuration();
        const float TransitionDelay = FMath::Max(0.1f, StartupDuration - EngineCrossfadeSeconds);
        const float SynchronizedSequenceSeconds = bSynchronizeStartupWithMissionBrief
            ? FMath::Max(TransitionDelay, MissionBriefStartupHoldSeconds)
            : TransitionDelay;
        EngineSpoolDuration = SynchronizedSequenceSeconds;
        EngineReadyTime = GetWorld()->GetTimeSeconds() + SynchronizedSequenceSeconds;
        const bool bUseTakeoffStage = EngineTakeoffSound != nullptr;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_HELICOPTER_AUDIO|STARTUP|craft=%s|duration=%.3f|transition=%.3f|next=%s"),
            *GetCraftDisplayName(),
            StartupDuration,
            TransitionDelay,
            bUseTakeoffStage ? TEXT("TAKEOFF") : TEXT("INFLIGHT"));
        if (bSynchronizeStartupWithMissionBrief)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_STARTUP_SYNC|mission=%s|phase=ARMED|brief_seconds=%.3f|engine_seconds=%.3f|sequence_seconds=%.3f|spool_seconds=%.3f|controls=LOCKED"),
                *ActiveMission.Id,
                MissionBriefSound->GetDuration(),
                TransitionDelay,
                SynchronizedSequenceSeconds,
                EngineSpoolDuration);
        }
        if (bUseTakeoffStage)
        {
            GetWorldTimerManager().SetTimer(
                EngineTransitionTimer,
                this,
                &ARotorlineHelicopterPawn::BeginTakeoffEngineAudio,
                TransitionDelay,
                false);
        }
        else
        {
            GetWorldTimerManager().SetTimer(
                EngineTransitionTimer,
                this,
                &ARotorlineHelicopterPawn::BeginFlightEngineAudio,
                TransitionDelay,
                false);
        }
    }
    else
    {
        BeginFlightEngineAudio();
    }

    const bool bDedicatedStartupSequence =
        ActiveMission.Id.Equals(TEXT("tutorial"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("medevac"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("rooftop-extraction"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("cabin-supply-convoy"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase) ||
        IsBell222SpecialOperations();
    if (!bDedicatedStartupSequence)
    {
        GetWorldTimerManager().ClearTimer(EngineTransitionTimer);
        GetWorldTimerManager().ClearTimer(EngineTakeoffTimer);
        BeginFlightEngineAudio();
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_FAST_STARTUP|mission=%s|aircraft=%s|mode=ACCELERATED|dedicated_audio=0"),
            *ActiveMission.Id,
            *SelectedAircraftId);
    }

    if (bMissionBriefActive)
    {
        // Mission voice belongs to deployment, never to an aircraft-specific
        // ignition or rotor-spool stage.
        StartMissionBriefPlayback(TEXT("DEPLOYMENT"));
    }

    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        PlayerController->SetControlRotation(FRotator(-11.0f, RotorlineHelicopter::SpawnYaw, 0.0f));
        SetMouseCaptured(false);
    }

    if (bFleetQualificationMode && bFleetQualificationSkipStartup)
    {
        GetWorldTimerManager().ClearTimer(EngineTransitionTimer);
        GetWorldTimerManager().ClearTimer(EngineTakeoffTimer);
        GetWorldTimerManager().ClearTimer(MissionBriefTimer);
        MissionBriefAudio->Stop();
        EngineStartupAudio->Stop();
        EngineTakeoffAudio->Stop();
        bMissionBriefActive = false;
        BeginFlightEngineAudio();
    }

    if (bFleetQualificationMode)
    {
        const TCHAR* RotorStrategy = RotorAnimation && !CatalogDynamicSkeletalParts.IsEmpty()
            ? TEXT("SKELETAL_ANIMATION")
            : (!CatalogMainRotorParts.IsEmpty() || !CatalogTailRotorParts.IsEmpty())
                ? TEXT("CENTERED_COMPONENTS")
                : TEXT("BESPOKE");
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_FLEET_TEST|READY|id=%s|catalog=%d|armed=%d|static_parts=%d|skeletal_parts=%d|main_rotors=%d|tail_rotors=%d|rotor_strategy=%s|box=%.1f,%.1f,%.1f|camera=%.1f|skip_startup=%d"),
            *SelectedAircraftId,
            bUseCatalogAircraft ? 1 : 0,
            bSelectedAircraftArmed ? 1 : 0,
            CatalogDynamicStaticParts.Num(),
            CatalogDynamicSkeletalParts.Num(),
            CatalogMainRotorParts.Num(),
            CatalogTailRotorParts.Num(),
            RotorStrategy,
            CollisionBox->GetUnscaledBoxExtent().X,
            CollisionBox->GetUnscaledBoxExtent().Y,
            CollisionBox->GetUnscaledBoxExtent().Z,
            SpringArm->TargetArmLength,
            bFleetQualificationSkipStartup ? 1 : 0);
    }

    if (ActiveMission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase) &&
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineKiowaReconTest")))
    {
        // Qualification starts at the first real sensor contact. Startup and
        // takeoff are already exercised by the fleet/runtime suites; this pass
        // measures all six sustained locks without requiring synthetic input.
        CurrentObjectiveIndex = 2;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_KIOWA_RECON_TEST|START|mission=recon|contacts=6|hold_seconds_each=7.5"));
    }

    if (ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase) &&
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineM25FinalePreview")))
    {
        CurrentObjectiveIndex = FMath::Max(0, ActiveMission.Objectives.Num() - 1);
        if (MissionBriefAudio) MissionBriefAudio->Stop();
        if (RadioAudio) RadioAudio->Stop();
        if (InstructorAudio) InstructorAudio->Stop();
        bMissionBriefActive = false;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_M25_FINALE_PREVIEW|state=ARMED|objective=%d|long_cargo_run=SKIPPED"),
            CurrentObjectiveIndex + 1);
    }

    RefreshMissionObjectiveActor();
}

void ARotorlineHelicopterPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    Bell222StealthTimeline.TickTimeline(DeltaSeconds);

    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    const double StealthNow = GetWorld()->GetTimeSeconds();
    const double StealthRemaining = Bell222StealthExpiresAt - StealthNow;
    if (bStealthActive && !bBell222DecloakAudioPrimed &&
        StealthRemaining > 0.0 && StealthRemaining <= 3.0)
    {
        PlayBell222StealthTransitionAudio(false);
        bBell222DecloakAudioPrimed = true;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BELL222_STEALTH|DECLOAK_AUDIO_PRIMED|remaining_s=%.2f"),
            StealthRemaining);
    }
    if (bStealthActive && StealthNow >= Bell222StealthExpiresAt)
    {
        SetBell222StealthActive(false, TEXT("DURATION_EXPIRED"));
    }

    // Rotorline's physical controller labels are one-based. Native button 5
    // is therefore B6. On Bell222 it is reserved for cloak; B1 remains the
    // selected-weapon trigger.
    bool bBell222StealthB6Pressed = false;
    if (IsBell222SpecialOperations() && GetGameInstance())
    {
        if (const URotorlineFlightControllerSubsystem* FlightInput =
            GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>())
        {
            const FString& DeviceId = FlightInput->GetActiveDeviceId();
            bBell222StealthB6Pressed = !DeviceId.IsEmpty() &&
                FlightInput->IsRawButtonPressed(DeviceId, 5);
        }
    }
    if (bBell222StealthB6Pressed && !bBell222StealthB6WasPressed)
    {
        ToggleBell222Stealth();
    }
    bBell222StealthB6WasPressed = bBell222StealthB6Pressed;

    UpdateEnemyHelicopterEncounterGate();
    if (bTutorialHelicopterKillTestMode)
    {
        UpdateTutorialHelicopterKillQualification();
        UpdateCamera(DeltaSeconds);
        UpdateMD500RotorAnimation(DeltaSeconds);
        UpdateCatalogRotorVisuals(DeltaSeconds);
        UpdateEngineAudio(DeltaSeconds);
        UpdateRotorDownwash(DeltaSeconds);
        return;
    }
    if (bEnemyHelicopterEncounterGateTestMode)
    {
        UpdateEnemyHelicopterEncounterGateQualification();
        UpdateCamera(DeltaSeconds);
        UpdateMD500RotorAnimation(DeltaSeconds);
        UpdateCatalogRotorVisuals(DeltaSeconds);
        UpdateEngineAudio(DeltaSeconds);
        UpdateRotorDownwash(DeltaSeconds);
        return;
    }

    if (bCombatLoopTestMode && !bCombatLoopPlayerFatalTriggered && !bPlayerAircraftDying)
    {
        bool bEnemyApacheDestroyed = false;
        for (TActorIterator<ARotorlineMissionObjectiveActor> It(GetWorld()); It; ++It)
        {
            const ARotorlineMissionObjectiveActor* Candidate = *It;
            if (IsValid(Candidate) && Candidate->IsDestroyedTarget() &&
                Candidate->GetTargetLabel().Contains(TEXT("apache"), ESearchCase::IgnoreCase))
            {
                bEnemyApacheDestroyed = true;
                break;
            }
        }
        if (bEnemyApacheDestroyed)
        {
            bCombatLoopPlayerFatalTriggered = true;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_COMBAT_LOOP_TEST|PLAYER_FATAL|damage=%.1f|health_before=%.1f|enemy_apache=DESTROYED"),
                MaxHealth + 50.0f, CurrentHealth);
            ApplyEnemyProjectileHit(MaxHealth + 50.0f);
        }
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::Tab))
    {
        SetMouseCaptured(!bMouseCaptured);
    }
    UpdateCamera(DeltaSeconds);
    UpdateEngineStartup(DeltaSeconds);
    UpdateFinalMissionCargoSling(DeltaSeconds);
    UpdateHueyRotorAnimation(DeltaSeconds);
    UpdateMD500RotorAnimation(DeltaSeconds);
    UpdateNightOpsLights();
    UpdateCatalogRotorVisuals(DeltaSeconds);
    if (!bHawkRidgeQualificationMode)
    {
        UpdateFlight(DeltaSeconds);
    }
    UpdateBell222LandingGear(DeltaSeconds);
    UpdateBell222BoostEffects(DeltaSeconds);
    UpdateAircraftExhaust(DeltaSeconds);
    UpdateFuel(DeltaSeconds);
    UpdateRotorDownwash(DeltaSeconds);
    UpdateFleetQualification(DeltaSeconds);
    UpdateEngineAudio(DeltaSeconds);
    UpdateControllerVibration();
    UpdateWeaponModeInput();
    UpdateBell222WeaponSystem(DeltaSeconds);
    UpdateApacheCannon(DeltaSeconds);
    UpdateQueuedRadio();
    RefreshAudioMix();
    UpdateMissionRuntime();
    UpdateBaseRearm(DeltaSeconds);
    UpdateFlightReadout();

    if (bHawkRidgeQualificationMode)
    {
        HawkRidgeQualificationElapsed += DeltaSeconds;
        if (HawkRidgeQualificationElapsed >= 20.0f)
        {
            const bool bPassed = bHawkRidgeQualificationLockObserved &&
                bHawkRidgeQualificationLaunchObserved;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_HAWK_RIDGE_TEST|RESULT|aircraft=%s|lock=%d|launch=%d|elapsed=%.1f|result=%s"),
                *SelectedAircraftId,
                bHawkRidgeQualificationLockObserved ? 1 : 0,
                bHawkRidgeQualificationLaunchObserved ? 1 : 0,
                HawkRidgeQualificationElapsed,
                bPassed ? TEXT("PASS") : TEXT("FAIL"));
            bHawkRidgeQualificationMode = false;
            FPlatformMisc::RequestExit(false);
        }
    }
}

void ARotorlineHelicopterPawn::UpdateNightOpsLights()
{
    if (!GetWorld()) return;

    const bool bAircraftLightsOn = bNightOperationLightsEnabled && !bPlayerAircraftDying;
    const float StrobePhase = FMath::Fmod(GetWorld()->GetTimeSeconds(), 1.25f);
    const bool bTailDoubleFlash = StrobePhase < 0.10f || (StrobePhase >= 0.22f && StrobePhase < 0.32f);

    LeftNavigationLight->SetVisibility(bAircraftLightsOn, true);
    RightNavigationLight->SetVisibility(bAircraftLightsOn, true);
    LeftNavigationLight->SetIntensity(bAircraftLightsOn ? 26000.0f : 0.0f);
    RightNavigationLight->SetIntensity(bAircraftLightsOn ? 26000.0f : 0.0f);
    LeftNavigationBulb->SetVisibility(bAircraftLightsOn, true);
    RightNavigationBulb->SetVisibility(bAircraftLightsOn, true);

    const bool bStrobeOn = bAircraftLightsOn && bTailDoubleFlash;
    TailStrobeLight->SetVisibility(bStrobeOn, true);
    TailStrobeLight->SetIntensity(bStrobeOn ? 72000.0f : 0.0f);
    TailStrobeBulb->SetVisibility(bStrobeOn, true);

    // Keep the close pad surface from blowing out the camera during startup;
    // the forward beam comes alive once the aircraft has actually lifted.
    const bool bLandingLightOn = bAircraftLightsOn && bSearchlightEnabledByPlayer && bEngineReady && GetAboveGroundMeters() > 1.5f;
    LandingLight->SetVisibility(bLandingLightOn, true);
    LandingLight->SetIntensity(bLandingLightOn ? 38000.0f : 0.0f);
}

void ARotorlineHelicopterPawn::UpdateWeaponModeInput()
{
    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(PlayerController);
    if (PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_LeftThumbstick) ||
        (OperationsController && OperationsController->WasFlightControllerActionJustPressed(
            RotorlineFlightControllerActions::Countermeasures)))
    {
        FireCountermeasures();
    }

    if (OperationsController && OperationsController->WasFlightControllerActionJustPressed(
        RotorlineFlightControllerActions::Searchlight))
    {
        bSearchlightEnabledByPlayer = !bSearchlightEnabledByPlayer;
        if (GEngine) GEngine->AddOnScreenDebugMessage(7118, 1.6f, FColor(90, 255, 220),
            bSearchlightEnabledByPlayer ? TEXT("SEARCHLIGHT // ON") : TEXT("SEARCHLIGHT // OFF"));
    }
    if (OperationsController && OperationsController->WasFlightControllerActionJustPressed(
        RotorlineFlightControllerActions::LandingGear))
    {
        bLandingGearExtended = !bLandingGearExtended;
        if (GEngine) GEngine->AddOnScreenDebugMessage(7119, 1.6f, FColor(255, 190, 70),
            bLandingGearExtended ? TEXT("LANDING GEAR // EXTENDED") : TEXT("LANDING GEAR // RETRACTED"));
    }

    const bool bKiowaSensorMissionActive = IsValid(ActiveKiowaStrikeMission) &&
        ActiveKiowaStrikeMission->IsSensorMissionActive();
    const bool bKiowaTargetLockPressed = bKiowaSensorMissionActive &&
        (PlayerController->WasInputKeyJustPressed(EKeys::T) ||
            PlayerController->WasInputKeyJustPressed(EKeys::LeftMouseButton) ||
            PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder) ||
            PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom) ||
            (OperationsController &&
                (OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::TargetLock) ||
                    OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::PrimaryFire) ||
                    OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::MissionInteract))));
    if (bKiowaTargetLockPressed)
    {
        ActiveKiowaStrikeMission->NotifyTargetLockPressed();
        PlayerController->PlayDynamicForceFeedback(0.16f, 0.07f, true, true, true, true);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(7127, 1.8f, FColor(255, 190, 70),
                TEXT("KIOWA SENSOR // TARGET ACQUISITION STARTED"));
        }
        return;
    }

    if (!HasAttackCombatPackage())
    {
        return;
    }

    const bool bTargetLockPressed = OperationsController &&
        OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::TargetLock);
    if (bTargetLockPressed)
    {
        if (IsBell222SpecialOperations())
        {
            if (!IsBell222MissileMode())
            {
                Bell222WeaponMode = ERotorlineBellWeaponMode::Aim9;
                Bell222ModeSelectedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
            }
            Bell222LockedTarget.Reset();
            Bell222WeaponLockProgress = 0.0f;
            PlayerController->PlayDynamicForceFeedback(0.24f, 0.09f, true, true, true, true);
            if (GEngine) GEngine->AddOnScreenDebugMessage(7113, 2.0f, FColor(255, 90, 70),
                TEXT("BELL 222 ADF SEEKER // ACQUIRING VALID TARGET"));
            return;
        }
        bApacheMissileLockMode = true;
        PlayerController->PlayDynamicForceFeedback(0.24f, 0.09f, true, true, true, true);
        if (GEngine) GEngine->AddOnScreenDebugMessage(7113, 2.0f, FColor(255, 90, 70),
            TEXT("TARGET LOCK // MISSILE SEEKER ACTIVE"));
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_TARGET_LOCK|aircraft=%s|mode=MISSILE_LOCK|input=FLIGHT_CONTROLLER"),
            *SelectedAircraftId);
        return;
    }

    // Some Windows joystick drivers mirror the physical POV hat into Unreal's
    // gamepad D-pad keys. Do not let hat-down trigger the gamepad weapon shortcut
    // when a native flight-controller profile is active; its semantic hat binding
    // remains exclusively View.External (zoom out).
    const bool bGamepadWeaponModePressed =
        (!OperationsController || !OperationsController->HasActiveFlightController()) &&
        PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down);
    const bool bControllerWeaponModePressed = OperationsController &&
        (OperationsController->WasFlightControllerActionJustPressed(
            RotorlineFlightControllerActions::WeaponNext) ||
            OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::WeaponPrevious));
    const bool bWeaponModePressed = bGamepadWeaponModePressed || bControllerWeaponModePressed;
    if (!bWeaponModePressed) return;

    if (IsBell222SpecialOperations())
    {
        const bool bPrevious = OperationsController && OperationsController->WasFlightControllerActionJustPressed(
            RotorlineFlightControllerActions::WeaponPrevious);
        CycleBell222Weapon(bPrevious ? -1 : 1);
        PlayerController->PlayDynamicForceFeedback(0.24f, 0.09f, true, true, true, true);
        return;
    }

    bApacheMissileLockMode = !bApacheMissileLockMode;
    PlayerController->PlayDynamicForceFeedback(0.24f, 0.09f, true, true, true, true);
    const TCHAR* ModeName = bApacheMissileLockMode ? TEXT("MISSILE LOCK") : TEXT("30MM CANNON");
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            7113,
            2.0f,
            bApacheMissileLockMode ? FColor(255, 90, 70) : FColor(90, 255, 160),
            FString::Printf(TEXT("WEAPON SELECTED  //  %s  //  B1 FIRE"), ModeName));
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_WEAPON_MODE|aircraft=%s|class=ATTACK|mode=%s|input=%s|trigger=B1"),
        *SelectedAircraftId, ModeName,
        bControllerWeaponModePressed ? TEXT("FLIGHT_CONTROLLER_CYCLE") : TEXT("GAMEPAD_DPAD_DOWN"));
}

float ARotorlineHelicopterPawn::GetCountermeasureCooldownRemaining() const
{
    if (!GetWorld()) return 0.0f;
    return FMath::Max(0.0f,
        RotorlineHelicopter::CountermeasureCooldownSeconds -
        static_cast<float>(GetWorld()->GetTimeSeconds() - LastCountermeasureFireTime));
}

void ARotorlineHelicopterPawn::FireCountermeasures()
{
    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController || bPlayerAircraftDying) return;

    const double Now = GetWorld()->GetTimeSeconds();
    const float CooldownRemaining = GetCountermeasureCooldownRemaining();
    if (CountermeasureCharges <= 0 || CooldownRemaining > 0.0f)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                7122,
                1.2f,
                FColor(255, 120, 70),
                CountermeasureCharges <= 0
                    ? TEXT("COUNTERMEASURES EMPTY // RETURN TO BASE")
                    : FString::Printf(TEXT("COUNTERMEASURES COOLING // %.1f SEC"), CooldownRemaining));
        }
        return;
    }

    --CountermeasureCharges;
    LastCountermeasureFireTime = Now;
    const FVector Back = -VisualRoot->GetForwardVector().GetSafeNormal();
    const FVector Right = VisualRoot->GetRightVector().GetSafeNormal();
    const FVector DecoyLocation = GetActorLocation() + Back * 1800.0f - FVector::UpVector * 420.0f;

    int32 DivertedMissiles = 0;
    float ClosestDivertedMissileMeters = 100000.0f;
    for (TActorIterator<ARotorlineRocketProjectile> It(GetWorld()); It; ++It)
    {
        ARotorlineRocketProjectile* Missile = *It;
        if (IsValid(Missile) &&
            FVector::DistSquared(Missile->GetActorLocation(), GetActorLocation()) <=
                FMath::Square(RotorlineHelicopter::CountermeasureMissileRangeCm) &&
            Missile->DivertEnemyGuidance(DecoyLocation))
        {
            ++DivertedMissiles;
            ClosestDivertedMissileMeters = FMath::Min(
                ClosestDivertedMissileMeters,
                FVector::Distance(Missile->GetActorLocation(), GetActorLocation()) / 100.0f);
        }
    }

    for (int32 Index = 0; Index < 14; ++Index)
    {
        const bool bChaff = Index >= 10;
        const float Side = (Index % 2 == 0 ? -1.0f : 1.0f) * FMath::FRandRange(0.35f, 1.0f);
        const FVector SpawnLocation = GetActorLocation() + Back * 280.0f + Right * Side * 165.0f - FVector::UpVector * 90.0f;
        const FVector EjectionVelocity =
            Back * FMath::FRandRange(850.0f, 1550.0f) +
            Right * Side * FMath::FRandRange(280.0f, 760.0f) +
            FVector::UpVector * FMath::FRandRange(-360.0f, 180.0f) +
            CurrentVelocity * 0.22f;
        if (ARotorlineRocketTrailSegment* Countermeasure = GetWorld()->SpawnActor<ARotorlineRocketTrailSegment>(
            ARotorlineRocketTrailSegment::StaticClass(), SpawnLocation, EjectionVelocity.Rotation()))
        {
            Countermeasure->InitializeCountermeasure(SpawnLocation, EjectionVelocity, bChaff);
        }
    }

    if (USoundBase* EjectSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Weapons/SFX_Enemy_Cannon.SFX_Enemy_Cannon")))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            EjectSound,
            GetActorLocation(),
            0.18f * GetAudioMix(ERotorlineAudioChannel::WeaponsExplosions) *
            (IsSpokenDialogueActive() ? 0.18f : 1.0f));
    }
    PlayerController->PlayDynamicForceFeedback(0.36f, 0.16f, true, true, true, true);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            7122,
            2.0f,
            FColor(255, 210, 90),
            FString::Printf(TEXT("FLARES / CHAFF  //  %d MISSILES DEFEATED  //  %d BURSTS LEFT"), DivertedMissiles, CountermeasureCharges));
    }
    if (DivertedMissiles > 0)
    {
        BroadcastRadio(TEXT("CREW: Flares out! Missile guidance broken!"), 2.8f, false);
        if (ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(Controller))
        {
            for (int32 MissileIndex = 0; MissileIndex < DivertedMissiles; ++MissileIndex)
            {
                OperationsController->NotifyMissileDodged(ClosestDivertedMissileMeters);
            }
        }
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_COUNTERMEASURE|BURST|input=L3|charges=%d/%d|missiles_diverted=%d|flares=10|chaff=4|cooldown=%.2f"),
        CountermeasureCharges,
        CountermeasureCapacity,
        DivertedMissiles,
        RotorlineHelicopter::CountermeasureCooldownSeconds);
}

void ARotorlineHelicopterPawn::UpdateFleetQualification(float DeltaSeconds)
{
    if (!bFleetQualificationMode)
    {
        return;
    }

    FleetQualificationElapsed += DeltaSeconds;
    const FVector Location = GetActorLocation();
    const float DisplacementMeters = FVector::Dist2D(
        Location,
        FVector(RotorlineHelicopter::SpawnX, RotorlineHelicopter::SpawnY, Location.Z)) * 0.01f;
    FleetQualificationMaxDisplacementMeters = FMath::Max(
        FleetQualificationMaxDisplacementMeters,
        DisplacementMeters);
    FleetQualificationMaxAttitudeDegrees = FMath::Max3(
        FleetQualificationMaxAttitudeDegrees,
        FMath::Abs(CurrentPitchAngle),
        FMath::Abs(CurrentRollAngle));

    const bool bBell222Qualification = IsBell222SpecialOperations();
    if (bBell222Qualification && bFleetQualificationSkipStartup &&
        Bell222WeaponMode == ERotorlineBellWeaponMode::Safe && FleetQualificationElapsed >= 0.05f)
    {
        CycleBell222Weapon(1);
    }

    if (!bFleetQualificationCountermeasureAttempted && bFleetQualificationSkipStartup &&
        FleetQualificationElapsed >= 0.75f)
    {
        bFleetQualificationCountermeasureAttempted = true;
        const int32 ChargesBefore = CountermeasureCharges;
        FireCountermeasures();
        bFleetQualificationCountermeasurePassed =
            ChargesBefore == CountermeasureCapacity && CountermeasureCharges == ChargesBefore - 1;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_FLEET_TEST|COUNTERMEASURE|id=%s|armed=%d|charges_before=%d|charges_after=%d|capacity=%d|support_ready=%d|passed=%d"),
            *SelectedAircraftId,
            bSelectedAircraftArmed ? 1 : 0,
            ChargesBefore,
            CountermeasureCharges,
            CountermeasureCapacity,
            !bSelectedAircraftArmed ? 1 : 0,
            bFleetQualificationCountermeasurePassed ? 1 : 0);
    }

    if (bSelectedAircraftArmed && !bFleetQualificationWeaponAttempted &&
        bFleetQualificationSkipStartup && FleetQualificationElapsed >= 1.0f)
    {
        bFleetQualificationWeaponAttempted = true;
        if (bBell222Qualification)
        {
            const int32 ChainAmmoBefore = Bell222WeaponAmmo.FindRef(TEXT("chain50"));
            FireBell222GunMode();
            const int32 ChainAmmoAfter = Bell222WeaponAmmo.FindRef(TEXT("chain50"));
            bFleetQualificationWeaponPassed =
                Bell222GunDeploymentAlpha >= 0.95f && ChainAmmoAfter == ChainAmmoBefore - 4;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("ROTORLINE_FLEET_TEST|WEAPON|id=%s|armed=1|system=bell222_concealed|mode=chain50|deployment=%.2f|ammo_before=%d|ammo_after=%d|projectiles_fired=%d|passed=%d"),
                *SelectedAircraftId,
                Bell222GunDeploymentAlpha,
                ChainAmmoBefore,
                ChainAmmoAfter,
                ChainAmmoBefore - ChainAmmoAfter,
                bFleetQualificationWeaponPassed ? 1 : 0);
        }
        else
        {
        const int32 RocketAmmoBefore = RocketAmmo;
        const int32 CannonAmmoBefore = ApacheCannonAmmo;
        const bool bMH6WeaponQualification =
            SelectedAircraftId.Equals(TEXT("md500_defender"), ESearchCase::IgnoreCase);
        FireMissionRocket();
        if (bMH6WeaponQualification)
        {
            // Exercise both physical pods in one bounded qualification. Normal
            // gameplay keeps the 0.75 s cadence; only this automated proof
            // advances the cooldown so the second shot must use the other pod.
            LastRocketFireTime = GetWorld()->GetTimeSeconds() - 1.0;
            FireMissionRocket();
        }
        const int32 ExpectedRocketShots = bMH6WeaponQualification ? 2 : 1;
        const bool bRocketPassed = RocketAmmo == RocketAmmoBefore - ExpectedRocketShots;
        const bool bApacheQualification = HasAttackCombatPackage();
        bool bCannonPassed = true;
        if (bApacheQualification)
        {
            FireApacheCannon();
            bCannonPassed = ApacheCannonAmmo == CannonAmmoBefore - 1;
        }
        bFleetQualificationWeaponPassed = bRocketPassed && bCannonPassed;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_FLEET_TEST|WEAPON|id=%s|armed=1|rocket_before=%d|rocket_after=%d|rocket_fired=%d|rocket_expected=%d|cannon_required=%d|cannon_before=%d|cannon_after=%d|cannon_fired=%d|passed=%d"),
            *SelectedAircraftId,
            RocketAmmoBefore,
            RocketAmmo,
            RocketAmmoBefore - RocketAmmo,
            ExpectedRocketShots,
            bApacheQualification ? 1 : 0,
            CannonAmmoBefore,
            ApacheCannonAmmo,
            bCannonPassed ? 1 : 0,
            bFleetQualificationWeaponPassed ? 1 : 0);
        }
    }

    constexpr float MilestoneSeconds[] = { 2.0f, 5.0f, 9.0f, 14.0f };
    if (FleetQualificationMilestone >= UE_ARRAY_COUNT(MilestoneSeconds) ||
        FleetQualificationElapsed < MilestoneSeconds[FleetQualificationMilestone])
    {
        return;
    }

    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    const bool bPossessed = PlayerController && PlayerController->GetPawn() == this;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_FLEET_TEST|TELEMETRY|id=%s|t=%.1f|possessed=%d|controller=%s|engine_ready=%d|speed_kph=%.1f|agl_m=%.1f|displacement_m=%.1f|yaw=%.1f|pitch=%.1f|roll=%.1f|input=%.2f,%.2f,%.2f,%.2f|tuning=%.0f,%.0f,%.0f,%.0f|health=%.0f|ammo=%d"),
        *SelectedAircraftId,
        FleetQualificationElapsed,
        bPossessed ? 1 : 0,
        PlayerController ? *PlayerController->GetClass()->GetName() : TEXT("NONE"),
        bEngineReady ? 1 : 0,
        CurrentVelocity.Size2D() * 0.036f,
        GetAboveGroundMeters(),
        DisplacementMeters,
        GetActorRotation().Yaw,
        CurrentPitchAngle,
        CurrentRollAngle,
        ForwardInput,
        StrafeInput,
        CollectiveInput,
        YawInput,
        MaxForwardSpeed,
        MaxStrafeSpeed,
        MaxVerticalSpeed,
        MaxYawRate,
        MaxHealth,
        RocketAmmo);
    ++FleetQualificationMilestone;

    if (FleetQualificationMilestone >= UE_ARRAY_COUNT(MilestoneSeconds))
    {
        int32 VisibleRotorComponents = 0;
        if (bUseCatalogAircraft)
        {
            for (USceneComponent* Part : CatalogMainRotorParts)
            {
                if (Part && Part->IsVisible()) ++VisibleRotorComponents;
            }
            for (USceneComponent* Part : CatalogTailRotorParts)
            {
                if (Part && Part->IsVisible()) ++VisibleRotorComponents;
            }
            if (RotorAnimation && !CatalogDynamicSkeletalParts.IsEmpty())
            {
                VisibleRotorComponents = FMath::Max(VisibleRotorComponents, 1);
            }
        }
        else
        {
            if (SelectedCraft == ERotorlineCraftType::AttackMD500)
            {
                if (MD500MainRotorRotatingMesh && MD500MainRotorRotatingMesh->IsVisible())
                {
                    ++VisibleRotorComponents;
                }
                if (MD500TailRotorRotatingMesh && MD500TailRotorRotatingMesh->IsVisible())
                {
                    ++VisibleRotorComponents;
                }
            }
            else
            {
                for (USkeletalMeshComponent* Part : GetActiveRotors())
                {
                    if (Part && Part->IsVisible()) ++VisibleRotorComponents;
                }
            }
        }
        const bool bMovementPassed = FleetQualificationMaxDisplacementMeters >= 5.0f &&
            FleetQualificationMaxAttitudeDegrees >= 2.0f;
        const bool bIntegratedRotorAnimation = bUseCatalogAircraft && RotorAnimation &&
            !CatalogDynamicSkeletalParts.IsEmpty();
        int32 DeclaredMainRotorGroups = 0;
        int32 DeclaredTailRotorGroups = 0;
        for (const FRotorlineAircraftRotorGroup& Group : SelectedAircraftDefinition.RotorGroups)
        {
            if (Group.Role.Equals(TEXT("tail"), ESearchCase::IgnoreCase))
            {
                ++DeclaredTailRotorGroups;
            }
            else
            {
                ++DeclaredMainRotorGroups;
            }
        }
        const bool bDeclaredCoaxialRotor = DeclaredMainRotorGroups >= 2 && DeclaredTailRotorGroups == 0;
        const bool bRotorRolesComplete = !bUseCatalogAircraft
            ? VisibleRotorComponents >= 2
            : bIntegratedRotorAnimation ||
                (!CatalogMainRotorParts.IsEmpty() && !CatalogTailRotorParts.IsEmpty() &&
                    VisibleRotorComponents >= 2) ||
                (bDeclaredCoaxialRotor && CatalogMainRotorParts.Num() >= 2 &&
                    VisibleRotorComponents >= 2);
        const bool bRotorPassed = bRotorRolesComplete && CurrentRotorPlayRate > 0.0f;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_FLEET_TEST|COMPLETE|id=%s|possessed=%d|body_asset=%s|catalog=%d|static_parts=%d|skeletal_parts=%d|visible_rotors=%d|rotor_rate=%.2f|rotor_pass=%d|mh6_main_turns=%.2f|mh6_tail_turns=%.2f|max_displacement_m=%.1f|max_attitude_deg=%.1f|movement_pass=%d|armed=%d|weapon_pass=%d|countermeasure_pass=%d|countermeasures=%d/%d|ammo=%d"),
            *SelectedAircraftId,
            bPossessed ? 1 : 0,
            *SelectedAircraftDefinition.BodyAsset,
            bUseCatalogAircraft ? 1 : 0,
            CatalogDynamicStaticParts.Num(),
            CatalogDynamicSkeletalParts.Num(),
            VisibleRotorComponents,
            CurrentRotorPlayRate,
            bRotorPassed ? 1 : 0,
            MD500MainRotorIntegratedDegrees / 360.0f,
            MD500TailRotorIntegratedDegrees / 360.0f,
            FleetQualificationMaxDisplacementMeters,
            FleetQualificationMaxAttitudeDegrees,
            bMovementPassed ? 1 : 0,
            bSelectedAircraftArmed ? 1 : 0,
            bFleetQualificationWeaponPassed ? 1 : 0,
            bFleetQualificationCountermeasurePassed ? 1 : 0,
            CountermeasureCharges,
            CountermeasureCapacity,
            RocketAmmo);
    }
}

void ARotorlineHelicopterPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(Bell222BoostFadeTimer);
    GetWorldTimerManager().ClearTimer(EngineTransitionTimer);
    GetWorldTimerManager().ClearTimer(EngineTakeoffTimer);
    GetWorldTimerManager().ClearTimer(MissionBriefTimer);
    ResetBell222StealthMaterials();
    if (Bell222StealthAudio)
    {
        Bell222StealthAudio->Stop();
    }
    GetWorldTimerManager().ClearTimer(MissionRadioHoldTimer);
    if (IsValid(ActiveObjectiveActor))
    {
        ActiveObjectiveActor->Destroy();
    }
    if (IsValid(ActiveCabinSupplyConvoy))
    {
        ActiveCabinSupplyConvoy->Destroy();
        ActiveCabinSupplyConvoy = nullptr;
    }
    if (IsValid(ActiveKiowaStrikeMission))
    {
        ActiveKiowaStrikeMission->Destroy();
        ActiveKiowaStrikeMission = nullptr;
    }
    if (IsValid(TransitThreatActor))
    {
        TransitThreatActor->Destroy();
    }
    ClearFinalMissionSetPieces();
    if (IsValid(ActiveFinalCinematic))
    {
        ActiveFinalCinematic->Destroy();
        ActiveFinalCinematic = nullptr;
    }
    MissionMusicAudio->Stop();
    RadioAudio->Stop();
    RadioSquelchAudio->Stop();
    StopApacheCannonAudio(TEXT("END_PLAY"), 0.0f);
    ResetCombatThreatState();
    if (bControllerVibrationActive)
    {
        if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
        {
            PlayerController->PlayDynamicForceFeedback(
                0.0f,
                0.0f,
                true,
                true,
                true,
                true,
                EDynamicForceFeedbackAction::Stop,
                ControllerVibrationHandle);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void ARotorlineHelicopterPawn::BeginRotorSpoolAudio()
{
    if (!EngineStartupSound)
    {
        BeginFlightEngineAudio();
        return;
    }

    bRotorSpoolStageActive = true;
    EngineStartupElapsed = 0.0f;
    LastSpoolMilestone = -1;
    const float SpoolDuration = EngineStartupSound->GetDuration();
    const float TransitionDelay = FMath::Max(0.1f, SpoolDuration - EngineCrossfadeSeconds);
    const bool bSynchronizedMissionStartup =
        ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase);
    const float BriefRemaining = bSynchronizedMissionStartup
        ? FMath::Max(0.0f, GetWorldTimerManager().GetTimerRemaining(MissionBriefTimer))
        : 0.0f;
    EngineSpoolDuration = bSynchronizedMissionStartup
        ? FMath::Max(TransitionDelay, BriefRemaining)
        : TransitionDelay;

    EngineTakeoffAudio->SetSound(EngineStartupSound);
    EngineTakeoffAudio->FadeIn(
        EngineCrossfadeSeconds,
        EngineStartupVolume * GetAudioMix(ERotorlineAudioChannel::Engine),
        0.0f);
    if (EngineStartupAudio->IsPlaying())
    {
        EngineStartupAudio->FadeOut(EngineCrossfadeSeconds, 0.0f);
    }
    GetWorldTimerManager().SetTimer(
        EngineTakeoffTimer,
        this,
        &ARotorlineHelicopterPawn::BeginFlightEngineAudio,
        TransitionDelay,
        false);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_HELICOPTER_AUDIO|ROTOR_SPOOL|craft=%s|duration=%.3f|transition=%.3f|controls=LOCKED"),
        *GetCraftDisplayName(),
        SpoolDuration,
        TransitionDelay);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            7101,
            4.0f,
            FColor(255, 215, 105),
            FString::Printf(TEXT("%s  |  ROTOR SPOOL-UP"), *GetCraftDisplayName()));
    }
}

void ARotorlineHelicopterPawn::StartMissionBriefPlayback(const TCHAR* Trigger)
{
    if (!bMissionBriefActive || bMissionBriefPlaybackStarted || !MissionBriefSound)
    {
        return;
    }

    bMissionBriefPlaybackStarted = true;
    MissionBriefAudio->Play();
    const float BriefDuration = MissionBriefSound->GetDuration();
    const bool bBriefNeedsTailPadding =
        ActiveMission.Id.Equals(TEXT("rooftop-extraction"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("enemy-foothold"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("cabin-supply-convoy"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase);
    const float BriefTailPadding = bBriefNeedsTailPadding ? 1.5f : 0.0f;
    GetWorldTimerManager().SetTimer(
        MissionBriefTimer,
        this,
        &ARotorlineHelicopterPawn::FinishMissionBrief,
        BriefDuration + BriefTailPadding,
        false);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_MISSION_AUDIO|START|mission=%s|duration=%.3f|trigger=%s|engine_duck=%.3f"),
        *ActiveMission.Id,
        BriefDuration,
        Trigger,
        EngineDuckedVolume);
}

void ARotorlineHelicopterPawn::BeginTakeoffEngineAudio()
{
    if (!EngineTakeoffSound)
    {
        BeginFlightEngineAudio();
        return;
    }

    const bool bHoldForMissionBrief =
        (ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
         ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase)) &&
        (bMissionBriefActive || (MissionBriefAudio && MissionBriefAudio->IsPlaying()));
    bEngineReady = !bHoldForMissionBrief;
    bRotorSpoolStageActive = bHoldForMissionBrief;
    if (bHoldForMissionBrief)
    {
        const float BriefRemaining = GetWorldTimerManager().GetTimerRemaining(MissionBriefTimer);
        EngineReadyTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.25f, BriefRemaining);
    }
    else
    {
        EngineReadyTime = -1.0;
    }
    CurrentRotorPlayRate = bHoldForMissionBrief
        ? FMath::Min(CurrentRotorPlayRate, RotorFlightPlayRate * 0.94f)
        : RotorFlightPlayRate;
    ApplyActiveRotorAnimationRates();

    const float TakeoffDuration = EngineTakeoffSound->GetDuration();
    const float TransitionDelay = FMath::Max(0.1f, TakeoffDuration - EngineCrossfadeSeconds);
    EngineTakeoffAudio->SetSound(EngineTakeoffSound);
    EngineTakeoffAudio->FadeIn(EngineCrossfadeSeconds, EngineFlightVolume * GetAudioMix(ERotorlineAudioChannel::Engine), 0.0f);
    if (EngineStartupAudio->IsPlaying())
    {
        EngineStartupAudio->FadeOut(EngineCrossfadeSeconds, 0.0f);
    }
    GetWorldTimerManager().SetTimer(
        EngineTakeoffTimer,
        this,
        &ARotorlineHelicopterPawn::BeginFlightEngineAudio,
        TransitionDelay,
        false);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_HELICOPTER_AUDIO|TAKEOFF|craft=%s|duration=%.3f|transition=%.3f|controls=%s|rotor_rate=%.3f"),
        *GetCraftDisplayName(),
        TakeoffDuration,
        TransitionDelay,
        bHoldForMissionBrief ? TEXT("LOCKED_FOR_BRIEF") : TEXT("UNLOCKED"),
        RotorFlightPlayRate);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            7101,
            4.0f,
            FColor(255, 215, 105),
            FString::Printf(
                TEXT("%s  |  %s"),
                *GetCraftDisplayName(),
                bHoldForMissionBrief
                    ? TEXT("TAKEOFF POWER - MISSION BRIEF HOLD")
                    : TEXT("TAKEOFF POWER - CONTROLS UNLOCKED")));
    }
}

void ARotorlineHelicopterPawn::BeginFlightEngineAudio()
{
    const bool bHoldForMissionBrief =
        (ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
         ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase)) &&
        (bMissionBriefActive || (MissionBriefAudio && MissionBriefAudio->IsPlaying()));
    if (bHoldForMissionBrief)
    {
        const float BriefRemaining = GetWorldTimerManager().GetTimerRemaining(MissionBriefTimer);
        const float RetryDelay = BriefRemaining > 0.25f ? BriefRemaining : 0.25f;
        EngineReadyTime = GetWorld()->GetTimeSeconds() + RetryDelay;
        bEngineReady = false;
        bRotorSpoolStageActive = true;
        GetWorldTimerManager().SetTimer(
            EngineTakeoffTimer,
            this,
            &ARotorlineHelicopterPawn::BeginFlightEngineAudio,
            RetryDelay,
            false);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_HELICOPTER_AUDIO|FLIGHT_READY_HOLD|craft=%s|mission=%s|brief_remaining=%.3f|controls=LOCKED"),
            *GetCraftDisplayName(),
            *ActiveMission.Id,
            RetryDelay);
        return;
    }

    if (bFlightEngineAudioStarted)
    {
        return;
    }

    bEngineReady = true;
    bRotorSpoolStageActive = false;
    EngineReadyTime = -1.0;
    CurrentRotorPlayRate = RotorFlightPlayRate;
    ApplyActiveRotorAnimationRates();
    if (ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_STARTUP_SYNC|mission=%s|phase=COMPLETE|brief_active=%d|brief_playing=%d|rotor_rate=%.3f|controls=UNLOCKED"),
            *ActiveMission.Id,
            bMissionBriefActive ? 1 : 0,
            MissionBriefAudio && MissionBriefAudio->IsPlaying() ? 1 : 0,
            CurrentRotorPlayRate);
    }

    if (EngineFlightLoopSound)
    {
        bFlightEngineAudioStarted = true;
        EngineFlightAudio->SetSound(EngineFlightLoopSound);
        const float FlightMixVolume = bMissionBriefActive ? EngineDuckedVolume : EngineFlightVolume;
        EngineFlightAudio->FadeIn(EngineCrossfadeSeconds, FlightMixVolume * GetAudioMix(ERotorlineAudioChannel::Engine), 0.0f);
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_HELICOPTER_AUDIO|FLIGHT_READY|craft=%s|crossfade=%.3f|volume=%.3f|controls=UNLOCKED|rotor_rate=%.3f"),
        *GetCraftDisplayName(),
        EngineCrossfadeSeconds,
        EngineFlightVolume,
        RotorFlightPlayRate);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_FLIGHT_MODEL|VETERAN|craft=%s|mapping=RS_CYCLIC_LSX_PEDALS_TRIGGERS_COLLECTIVE|pitch=%.1f|roll=%.1f|cyclic_accel=%.0f|collective_accel=%.0f"),
        *GetCraftDisplayName(),
        MaxPitchAngle,
        MaxRollAngle,
        CyclicAcceleration,
        CollectiveAcceleration);
    if (EngineStartupAudio->IsPlaying())
    {
        EngineStartupAudio->FadeOut(EngineCrossfadeSeconds, 0.0f);
    }
    if (EngineTakeoffAudio->IsPlaying())
    {
        EngineTakeoffAudio->FadeOut(EngineCrossfadeSeconds, 0.0f);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            7101,
            4.0f,
            FColor(255, 215, 105),
            FString::Printf(TEXT("%s  |  FLIGHT RPM - CONTROLS UNLOCKED"), *GetCraftDisplayName()));
    }
}

void ARotorlineHelicopterPawn::FinishMissionBrief()
{
    if (!bMissionBriefActive)
    {
        return;
    }
    if (MissionBriefAudio && MissionBriefAudio->IsPlaying())
    {
        GetWorldTimerManager().SetTimer(
            MissionBriefTimer,
            this,
            &ARotorlineHelicopterPawn::FinishMissionBrief,
            0.25f,
            false);
        return;
    }

    bMissionBriefActive = false;
    bMissionBriefPlaybackStarted = false;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_MISSION_AUDIO|END|engine_mix=RESTORING"));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            7102,
            3.0f,
            FColor(150, 230, 255),
            TEXT("MISSION BRIEF COMPLETE"));
    }
    if (!ActiveMission.Id.Equals(TEXT("medevac"), ESearchCase::IgnoreCase) &&
        !ActiveMission.Id.Equals(TEXT("rooftop-extraction"), ESearchCase::IgnoreCase) &&
        !ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase) &&
        !ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) &&
        !ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase))
    {
        BroadcastRadio(TEXT("COMMAND: Rotorline, cleared outbound. Execute the mission."), 6.5f);
    }
    else
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSION_RADIO|mission=%s|state=BRIEF_COMPLETE_RADIO_STILL_HELD|remaining_seconds=%.1f"),
            *ActiveMission.Id,
            FMath::Max(0.0, MissionRadioHoldUntil - GetWorld()->GetTimeSeconds()));
    }
}

bool ARotorlineHelicopterPawn::IsMissionRadioHoldActive() const
{
    const bool bKiowaSequenceOwnsRadio =
        ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase) &&
        IsValid(ActiveKiowaStrikeMission) &&
        !ActiveKiowaStrikeMission->IsComplete() &&
        !ActiveKiowaStrikeMission->IsFailed();
    const bool bTimedMissionHold = GetWorld() &&
        (ActiveMission.Id.Equals(TEXT("medevac"), ESearchCase::IgnoreCase) ||
            ActiveMission.Id.Equals(TEXT("rooftop-extraction"), ESearchCase::IgnoreCase) ||
            ActiveMission.Id.Equals(TEXT("cabin-supply-convoy"), ESearchCase::IgnoreCase) ||
            ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
            ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase) ||
            (IsBell222SpecialOperations() && MissionBriefSound == Bell222FinalMissionBriefSound)) &&
        GetWorld()->GetTimeSeconds() < MissionRadioHoldUntil;
    return bKiowaSequenceOwnsRadio || bTimedMissionHold || (InstructorAudio && InstructorAudio->IsPlaying());
}

void ARotorlineHelicopterPawn::ReleaseMissionRadioHold()
{
    if (!GetWorld() ||
        (!ActiveMission.Id.Equals(TEXT("medevac"), ESearchCase::IgnoreCase) &&
            !ActiveMission.Id.Equals(TEXT("rooftop-extraction"), ESearchCase::IgnoreCase) &&
            !ActiveMission.Id.Equals(TEXT("cabin-supply-convoy"), ESearchCase::IgnoreCase) &&
            !ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) &&
            !ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase) &&
            !(IsBell222SpecialOperations() && MissionBriefSound == Bell222FinalMissionBriefSound)))
    {
        return;
    }
    MissionRadioHoldUntil = -1000.0;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_MISSION_RADIO|mission=%s|state=RELEASED|elapsed_seconds=%.1f"),
        *ActiveMission.Id,
        GetWorld()->GetTimeSeconds() - MissionStartTime);
    const bool bFinalMissionStartup =
        ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase);
    if (!bMissionFailed && !bMissionComplete && !bFinalMissionStartup)
    {
        BroadcastRadio(TEXT("COMMAND: Rotorline, cleared outbound. Execute the mission."), 6.5f);
    }
}

void ARotorlineHelicopterPawn::UpdateEngineStartup(float DeltaSeconds)
{
    if (bPlayerAircraftDying)
    {
        return;
    }
    if (bEngineReady)
    {
        return;
    }

    // A catalog pre-ignition clip represents electrical/APU ignition. Keep
    // both rotors stopped until the dedicated spool recording begins.
    if (EnginePreIgnitionSound && !bRotorSpoolStageActive)
    {
        CurrentRotorPlayRate = 0.0f;
        return;
    }

    EngineStartupElapsed = FMath::Min(EngineStartupElapsed + DeltaSeconds, EngineSpoolDuration);
    float StartupProgress = FMath::Clamp(EngineStartupElapsed / EngineSpoolDuration, 0.0f, 1.0f);
    const bool bSynchronizedMissionStartup =
        ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
        ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase);
    const bool bMissionBriefStillPlaying =
        bMissionBriefActive || (MissionBriefAudio && MissionBriefAudio->IsPlaying());
    if (bSynchronizedMissionStartup && bMissionBriefStillPlaying)
    {
        StartupProgress = FMath::Min(StartupProgress, 0.94f);
    }
    const float SmoothProgress = StartupProgress * StartupProgress * (3.0f - 2.0f * StartupProgress);
    CurrentRotorPlayRate = FMath::Lerp(RotorStartPlayRate, RotorFlightPlayRate, SmoothProgress);
    ApplyActiveRotorAnimationRates();

    const int32 Milestone = FMath::Clamp(FMath::FloorToInt(StartupProgress * 4.0f), 0, 3);
    if (Milestone > LastSpoolMilestone)
    {
        LastSpoolMilestone = Milestone;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_HELICOPTER_AUDIO|SPOOL|craft=%s|progress=%d|rotor_rate=%.3f|controls=LOCKED"),
            *GetCraftDisplayName(),
            Milestone * 25,
            CurrentRotorPlayRate);
    }
}

void ARotorlineHelicopterPawn::SetMouseCaptured(bool bCaptured)
{
    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    bMouseCaptured = bCaptured;
    if (ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerController))
    {
        OperationsController->ApplyMouseMode(bCaptured);
    }
    else
    {
        PlayerController->bShowMouseCursor = !bCaptured;
        if (bCaptured)
        {
            PlayerController->SetInputMode(FInputModeGameOnly());
        }
        else
        {
            FInputModeGameAndUI Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Mode.SetHideCursorDuringCapture(false);
            PlayerController->SetInputMode(Mode);
        }
    }
}

void ARotorlineHelicopterPawn::UpdateCamera(float DeltaSeconds)
{
    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(PlayerController);
    const bool bBell222Selected = SelectedAircraftId.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase);
    if (!bBell222Selected)
    {
        bBell222CockpitViewEnabled = false;
    }
    else if (PlayerController->WasInputKeyJustPressed(EKeys::B))
    {
        bBell222CockpitViewEnabled = !bBell222CockpitViewEnabled;
        if (bBell222CockpitViewEnabled)
        {
            bCockpitViewEnabled = false;
            bApacheCombatZoomEnabled = false;
        }
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                7131,
                1.8f,
                FColor(90, 255, 220),
                bBell222CockpitViewEnabled
                    ? TEXT("BELL 222 COCKPIT  //  ON")
                    : TEXT("BELL 222 COCKPIT  //  OFF"));
        }
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_BELL222_COCKPIT|state=%s|input=B"),
            bBell222CockpitViewEnabled ? TEXT("ON") : TEXT("OFF"));
    }
    if (OperationsController)
    {
        auto RequestCockpitView = [&]()
        {
            // Flight-controller cockpit remains the fleet-wide forward view.
            // The Bell 222's modeled interior has its own keyboard-only mode.
            bCockpitViewEnabled = true;
        };

        if (OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::CockpitView))
            RequestCockpitView();
        if (OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::ExternalView))
        {
            bCockpitViewEnabled = false;
            bBell222CockpitViewEnabled = false;
        }
        if (OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::ChangeCamera))
        {
            if (bCockpitViewEnabled)
                bCockpitViewEnabled = false;
            else
                RequestCockpitView();
        }
        if (OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::MapView))
        {
            OperationsController->ToggleTacticalMap();
            if (GEngine) GEngine->AddOnScreenDebugMessage(7125, 2.0f, FColor(90, 255, 220),
                OperationsController->IsTacticalMapVisible() ? TEXT("TACTICAL MAP // ON") : TEXT("TACTICAL MAP // OFF"));
        }
        if (OperationsController->WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::RadioCommand))
        {
            BroadcastRadio(TEXT("ROOSTER: Command, radio check."), 3.5f, true);
        }
    }

    const bool bApacheSelected = HasAttackCombatPackage();
    const bool bZoomTogglePressed =
        PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_RightThumbstick) ||
        PlayerController->WasInputKeyJustPressed(EKeys::V);
    if (bApacheSelected && bZoomTogglePressed)
    {
        bApacheCombatZoomEnabled = !bApacheCombatZoomEnabled;
        PlayerController->PlayDynamicForceFeedback(0.18f, 0.07f, true, true, true, true);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                7130,
                1.5f,
                FColor(90, 255, 185),
                bApacheCombatZoomEnabled ? TEXT("COMBAT OPTIC  //  2.4X") : TEXT("COMBAT OPTIC  //  1.0X"));
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_ATTACK_ZOOM|aircraft=%s|state=%s|fov=%.0f|input=R3_OR_Z|precision_scale=%.2f|camera_response=%.1f"),
            *SelectedAircraftId,
            bApacheCombatZoomEnabled ? TEXT("ON") : TEXT("OFF"),
            bApacheCombatZoomEnabled ? ApacheCombatZoomFOV : 88.0f,
            bApacheCombatZoomEnabled ? ApacheCombatZoomInputScale : 1.0f,
            bApacheCombatZoomEnabled ? ApacheCombatZoomCameraResponse : CameraFollowResponse);
    }
    else if (!bApacheSelected)
    {
        bApacheCombatZoomEnabled = false;
    }

    if (bBell222CockpitViewEnabled)
    {
        // This is a genuine interior camera, not a zero-length external spring
        // arm.  The Bell source model faces local -X; these coordinates place
        // the lens at the pilot seat and aim it toward the modeled panel.
        if (Camera->GetAttachParent() != MeshAlignment)
        {
            Camera->AttachToComponent(MeshAlignment, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        }
        Camera->SetRelativeLocation(FVector(-310.0f, -48.0f, 8.0f));
        Camera->SetRelativeRotation(FRotator(-12.0f, 180.0f, 0.0f));
        Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, 78.0f, DeltaSeconds, 8.0f));
        return;
    }
    if (Camera->GetAttachParent() != SpringArm)
    {
        Camera->AttachToComponent(
            SpringArm,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            USpringArmComponent::SocketName);
        Camera->SetRelativeLocation(FVector::ZeroVector);
        Camera->SetRelativeRotation(FRotator::ZeroRotator);
    }

    const float TargetFOV = bApacheCombatZoomEnabled
        ? ApacheCombatZoomFOV
        : (bCockpitViewEnabled ? ForwardViewFOV : 88.0f);
    Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaSeconds, 8.0f));
    // The pawn body yaws directly, while cyclic pitch lives on VisualRoot.
    // Without absolute spring-arm rotation, parent yaw reaches the magnified
    // camera immediately and bypasses the precision interpolation below. The
    // optic therefore felt smooth vertically but twitchy horizontally.
    const bool bForwardCameraActive = bApacheCombatZoomEnabled || bCockpitViewEnabled;
    SpringArm->SetUsingAbsoluteRotation(bForwardCameraActive);
    const FVector AircraftExtent = CollisionBox->GetScaledBoxExtent();
    const FVector CombatOpticSocketOffset(
        AircraftExtent.X + ApacheCombatZoomForwardClearance,
        0.0f,
        FMath::Max(ApacheCombatZoomMinimumHeight, AircraftExtent.Z * 0.20f));
    const float TargetArmLength = NormalCameraArmLength;
    const FVector TargetSocketOffset = NormalCameraSocketOffset;
    if (bForwardCameraActive)
    {
        // Exterior/optic modes use a forward sight beyond the measured nose.
        // Snap beyond the measured nose so no transition frame can travel
        // through the fuselage or rotor mast. SocketOffset keeps the spring
        // arm's camera-channel obstruction test active against world geometry.
        SpringArm->bEnableCameraLag = false;
        SpringArm->bEnableCameraRotationLag = false;
        SpringArm->TargetArmLength = 0.0f;
        SpringArm->SocketOffset = CombatOpticSocketOffset;
    }
    else
    {
        SpringArm->bEnableCameraLag = true;
        SpringArm->bEnableCameraRotationLag = true;
        SpringArm->TargetArmLength = FMath::FInterpTo(
            SpringArm->TargetArmLength,
            TargetArmLength,
            DeltaSeconds,
            7.0f);
        SpringArm->SocketOffset = FMath::VInterpTo(
            SpringArm->SocketOffset,
            TargetSocketOffset,
            DeltaSeconds,
            7.0f);
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::R))
    {
        PlayerController->SetControlRotation(FRotator(-11.0f, GetActorRotation().Yaw, 0.0f));
        return;
    }

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    bool bMouseLookActive = false;
    if (bMouseCaptured)
    {
        PlayerController->GetInputMouseDelta(MouseX, MouseY);
        bMouseLookActive = FMath::Abs(MouseX) > 0.01f || FMath::Abs(MouseY) > 0.01f;
        if (bMouseLookActive)
        {
            bLastInputWasGamepad = false;
        }
    }
    FRotator CameraRotation = PlayerController->GetControlRotation();
    float FlightLookX = 0.0f;
    float FlightLookY = 0.0f;
    const bool bHasFlightLookX = OperationsController && OperationsController->GetFlightControllerAxis(
        RotorlineFlightControllerActions::LookX, FlightLookX);
    const bool bHasFlightLookY = OperationsController && OperationsController->GetFlightControllerAxis(
        RotorlineFlightControllerActions::LookY, FlightLookY);
    const bool bFlightLookActive = (bHasFlightLookX && FMath::Abs(FlightLookX) > 0.08f) ||
        (bHasFlightLookY && FMath::Abs(FlightLookY) > 0.08f);
    // Mouse-free mode must remain a proper chase camera. Previously this
    // function returned early, leaving the camera fixed in world space while
    // the helicopter flew away from it. Controller flight also uses chase
    // follow unless captured mouse movement explicitly requests free-look.
    if (bForwardCameraActive)
    {
        const FRotator NoseRotation = VisualRoot->GetComponentRotation();
        const FRotator SightRotation(NoseRotation.Pitch, NoseRotation.Yaw, 0.0f);
        // The combat reticle is nose-coupled, so its camera must use the same
        // rotation immediately. Lagging the zoom camera behind the nose makes
        // the reticle sweep vertically off-screen during pitch corrections.
        // Fine aiming remains controlled by the zoom-only input curve below.
        if (bApacheCombatZoomEnabled)
        {
            CameraRotation = SightRotation;
        }
        else
        {
            CameraRotation = FMath::RInterpTo(
                CameraRotation,
                SightRotation,
                DeltaSeconds,
                CameraFollowResponse);
        }
    }
    else if (bFlightLookActive)
    {
        CameraRotation.Yaw += FlightLookX * 90.0f * DeltaSeconds;
        CameraRotation.Pitch = FMath::ClampAngle(
            CameraRotation.Pitch - FlightLookY * 70.0f * DeltaSeconds, -65.0f, 35.0f);
    }
    else if (!bMouseLookActive)
    {
        CameraRotation.Yaw = FMath::FixedTurn(
            CameraRotation.Yaw,
            GetActorRotation().Yaw,
            CameraFollowResponse * 45.0f * DeltaSeconds);
        CameraRotation.Pitch = FMath::FInterpTo(CameraRotation.Pitch, -11.0f, DeltaSeconds, 2.5f);
    }
    else
    {
        CameraRotation.Yaw += MouseX * LookSensitivity;
        CameraRotation.Pitch = FMath::ClampAngle(CameraRotation.Pitch - MouseY * LookSensitivity, -65.0f, 35.0f);
    }
    CameraRotation.Roll = 0.0f;
    PlayerController->SetControlRotation(CameraRotation);
}

void ARotorlineHelicopterPawn::UpdateFlight(float DeltaSeconds)
{
    bBoostActive = false;
    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    if (bPlayerAircraftDying)
    {
        UpdatePlayerDestruction(DeltaSeconds);
        return;
    }

    if (bTransitionSpawnAwaitingNeutral)
    {
        TransitionSpawnHoldRemaining = FMath::Max(0.0f, TransitionSpawnHoldRemaining - DeltaSeconds);
        const bool bDriveInputHeld =
            PlayerController->IsInputKeyDown(EKeys::W) ||
            PlayerController->IsInputKeyDown(EKeys::S) ||
            PlayerController->IsInputKeyDown(EKeys::A) ||
            PlayerController->IsInputKeyDown(EKeys::D) ||
            FMath::Abs(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftY)) > 0.12f ||
            FMath::Abs(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftX)) > 0.12f ||
            PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) > 0.12f ||
            PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis) > 0.12f;

        if (TransitionSpawnHoldRemaining <= 0.0f && !bDriveInputHeld)
        {
            bTransitionSpawnAwaitingNeutral = false;
            LastWPress = LastSPress = LastAPress = LastDPress = -1000.0;
            LastSpacePress = LastZPress = LastCPress = LastQPress = LastEPress = -1000.0;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_CAVE_SEQUENCE|BELL_SPAWN_HOLD|status=RELEASED"));
        }
        else
        {
            ForwardInput = 0.0f;
            StrafeInput = 0.0f;
            CollectiveInput = 0.0f;
            YawInput = 0.0f;
            Bell222SmoothedForwardInput = 0.0f;
            Bell222SmoothedCollectiveInput = 0.0f;
            Bell222CurrentSpeedScale = 1.0f;
            CurrentVelocity = FVector::ZeroVector;
            CurrentYawRate = 0.0f;
            return;
        }
    }

    if (!bEngineReady)
    {
        ForwardInput = 0.0f;
        StrafeInput = 0.0f;
        CollectiveInput = 0.0f;
        YawInput = 0.0f;
        if (bFuelStarved)
        {
            CurrentVelocity.X *= FMath::Exp(-0.22f * DeltaSeconds);
            CurrentVelocity.Y *= FMath::Exp(-0.22f * DeltaSeconds);
            CurrentVelocity.Z = FMath::Max(CurrentVelocity.Z - 520.0f * DeltaSeconds, -1800.0f);
            const float ImpactSpeed = -CurrentVelocity.Z;
            FHitResult FuelOutHit;
            CollisionBox->MoveComponent(CurrentVelocity * DeltaSeconds, GetActorRotation(), true, &FuelOutHit);
            if (FuelOutHit.IsValidBlockingHit())
            {
                const AActor* FuelOutHitActor = FuelOutHit.GetActor();
                if (FuelOutHitActor && FuelOutHitActor->ActorHasTag(TEXT("RotorlineWaterSurface")))
                {
                    MissionFailureReason = TEXT("WATER IMPACT // AIRCRAFT LOST");
                    CurrentHealth = 0.0f;
                    UE_LOG(LogTemp, Display,
                        TEXT("ROTORLINE_WATER_IMPACT|mode=FUEL_OUT|speed_cm_s=%.1f|actor=%s"),
                        CurrentVelocity.Size(),
                        *FuelOutHitActor->GetActorNameOrLabel());
                    BeginPlayerDestruction();
                    return;
                }
                if (ImpactSpeed > 850.0f)
                {
                    MissionFailureReason = TEXT("FUEL EXHAUSTED // HARD LANDING");
                    CurrentHealth = 0.0f;
                    BeginPlayerDestruction();
                    return;
                }
                CurrentVelocity = FVector::VectorPlaneProject(CurrentVelocity, FuelOutHit.Normal) * 0.18f;
            }
        }
        else
        {
            CurrentVelocity = FMath::VInterpTo(CurrentVelocity, FVector::ZeroVector, DeltaSeconds, 5.0f);
        }
        CurrentYawRate = FMath::FInterpTo(CurrentYawRate, 0.0f, DeltaSeconds, 5.0f);
        CurrentPitchAngle = FMath::FInterpTo(CurrentPitchAngle, 0.0f, DeltaSeconds, 5.0f);
        CurrentRollAngle = FMath::FInterpTo(CurrentRollAngle, 0.0f, DeltaSeconds, 5.0f);
        VisualRoot->SetRelativeRotation(FMath::RInterpTo(
            VisualRoot->GetRelativeRotation(),
            FRotator::ZeroRotator,
            DeltaSeconds,
            AttitudeResponse));
        return;
    }

    constexpr double InputPulseSeconds = 0.14;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < LastWPress)
    {
        LastWPress = LastSPress = LastAPress = LastDPress = -1000.0;
        LastSpacePress = LastZPress = LastCPress = LastQPress = LastEPress = -1000.0;
    }

    const auto TrackPress = [PlayerController, Now](const FKey& Key, double& LastPress)
    {
        if (PlayerController->WasInputKeyJustPressed(Key))
        {
            LastPress = Now;
        }
    };
    TrackPress(EKeys::W, LastWPress);
    TrackPress(EKeys::S, LastSPress);
    TrackPress(EKeys::A, LastAPress);
    TrackPress(EKeys::D, LastDPress);
    TrackPress(EKeys::SpaceBar, LastSpacePress);
    TrackPress(EKeys::Z, LastZPress);
    TrackPress(EKeys::C, LastCPress);
    TrackPress(EKeys::Q, LastQPress);
    TrackPress(EKeys::E, LastEPress);

    const auto KeyActive = [PlayerController, Now](const FKey& Key, double LastPress)
    {
        return PlayerController->IsInputKeyDown(Key) || (Now - LastPress) <= InputPulseSeconds;
    };
    const auto AxisFromKeys = [&KeyActive](const FKey& Positive, double LastPositive, const FKey& Negative, double LastNegative)
    {
        return (KeyActive(Positive, LastPositive) ? 1.0f : 0.0f) -
               (KeyActive(Negative, LastNegative) ? 1.0f : 0.0f);
    };

    const auto ApplyDeadZone = [](float Value)
    {
        constexpr float DeadZone = 0.09f;
        if (FMath::Abs(Value) <= DeadZone)
        {
            return 0.0f;
        }
        const float Linear = FMath::Clamp((FMath::Abs(Value) - DeadZone) / (1.0f - DeadZone), 0.0f, 1.0f);
        const float Shaped = FMath::Lerp(Linear, Linear * Linear * Linear, 0.28f);
        return FMath::Sign(Value) * Shaped;
    };

    // DualSense flight layout: left stick Y is pitch, left stick X is yaw,
    // right stick X is lateral movement,
    // and R2/L2 provide ascend/descend collective. Unreal reports stick Y negative
    // when pushed forward; the flight model uses that negative value for nose-down.
    // A controller-only profile option can explicitly reverse this axis.
    const auto ApplyZoomPrecisionCurve = [this](float Value, bool bPitchAxis)
    {
        if (!bApacheCombatZoomEnabled || FMath::IsNearlyZero(Value))
        {
            return Value;
        }

        const float Magnitude = FMath::Abs(Value);

        // Pitch must remain continuous throughout stick travel while zoomed.
        // The previous pitch path held near zero through most of the stick range,
        // then blended rapidly to full authority, producing the observed
        // hold-then-zip behavior. Keep only a small hardware-noise dead zone and
        // use a continuous linear/cubic blend with no authority threshold.
        if (bPitchAxis)
        {
            constexpr float PitchNoiseDeadZone = 0.035f;
            if (Magnitude <= PitchNoiseDeadZone)
            {
                return 0.0f;
            }

            const float NormalizedPitch = FMath::Clamp(
                (Magnitude - PitchNoiseDeadZone) / (1.0f - PitchNoiseDeadZone),
                0.0f,
                1.0f);
            const float ContinuousPitch =
                (0.30f * NormalizedPitch) +
                (0.70f * NormalizedPitch * NormalizedPitch * NormalizedPitch);
            return FMath::Sign(Value) * ContinuousPitch;
        }

        // Preserve the accepted non-pitch zoom response for yaw and lateral input.
        const float FineAimDeadZone = FMath::Clamp(
            ApacheCombatZoomFineAimDeadZone,
            0.0f,
            0.35f);
        if (Magnitude <= FineAimDeadZone)
        {
            return 0.0f;
        }

        const float Normalized = FMath::Clamp(
            (Magnitude - FineAimDeadZone) / FMath::Max(KINDA_SMALL_NUMBER, 1.0f - FineAimDeadZone),
            0.0f,
            1.0f);
		const float InputExponent = ApacheCombatZoomInputExponent;
		const float InputScale = ApacheCombatZoomInputScale;
		const float FineResponse = FMath::Pow(Normalized, InputExponent) * InputScale;
        const float AuthorityStart = FMath::Clamp(
            ApacheCombatZoomFullAuthorityStart,
            FineAimDeadZone + 0.05f,
            0.95f);
        const float AuthorityBlend = FMath::SmoothStep(AuthorityStart, 1.0f, Normalized);
        const float Shaped = FMath::Lerp(FineResponse, Normalized, AuthorityBlend);
        return FMath::Sign(Value) * Shaped;
    };

    const ARotorlineOperationsPlayerController* GamepadSettingsController =
        Cast<ARotorlineOperationsPlayerController>(PlayerController);
    const float GamepadPitchDirection =
        GamepadSettingsController && GamepadSettingsController->IsGamepadPitchInverted() ? -1.0f : 1.0f;
    // Use the same single zoom curve as the sight/camera path so the reticle
    // remains synchronized with the aircraft nose while aiming.
    const float GamepadForward = ApplyZoomPrecisionCurve(
        ApplyDeadZone(GamepadPitchDirection *
            PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftY)), true);
    const float GamepadYaw = ApplyZoomPrecisionCurve(
        ApplyDeadZone(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftX)), false);
    const float GamepadStrafe = ApplyZoomPrecisionCurve(
        ApplyDeadZone(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightX)), false);
    const float GamepadCollective = FMath::Clamp(
        PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) -
        PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis),
        -1.0f,
        1.0f);

    const float KeyboardForward = AxisFromKeys(EKeys::W, LastWPress, EKeys::S, LastSPress);
    const float KeyboardStrafe = AxisFromKeys(EKeys::D, LastDPress, EKeys::A, LastAPress);
    const float KeyboardCollective = AxisFromKeys(EKeys::E, LastEPress, EKeys::Q, LastQPress);
    const bool bKeyboardCollectiveActive = FMath::Abs(KeyboardCollective) > 0.0f;
    const float KeyboardYaw = AxisFromKeys(EKeys::C, LastCPress, EKeys::Z, LastZPress);
    ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(PlayerController);
    float FlightPitch = 0.0f;
    float FlightRoll = 0.0f;
    float FlightYaw = 0.0f;
    float FlightCollectiveRaw = 0.5f;
    float FlightThrottleRaw = 0.5f;
    const bool bHasFlightPitch = OperationsController && OperationsController->GetFlightControllerAxis(
        RotorlineFlightControllerActions::Pitch, FlightPitch);
    const bool bHasFlightRoll = OperationsController && OperationsController->GetFlightControllerAxis(
        RotorlineFlightControllerActions::Roll, FlightRoll);
    const bool bHasFlightYaw = OperationsController && OperationsController->GetFlightControllerAxis(
        RotorlineFlightControllerActions::Yaw, FlightYaw);
    const bool bHasFlightCollective = OperationsController && OperationsController->GetFlightControllerAxis(
        RotorlineFlightControllerActions::Collective, FlightCollectiveRaw);
    const bool bHasFlightThrottle = OperationsController && OperationsController->GetFlightControllerAxis(
        RotorlineFlightControllerActions::Throttle, FlightThrottleRaw);
    const float FlightCollective = FMath::Clamp(
        (bHasFlightCollective ? FlightCollectiveRaw : FlightThrottleRaw) * 2.0f - 1.0f, -1.0f, 1.0f);

    // Older centered flight controls can rest well outside a conventional
    // gamepad dead zone. The connected Extreme 3D Pro, for example, reports
    // roughly 0.18-0.19 on centered roll/yaw and was moving aircraft across
    // the pad as soon as engine authority became available. Suppress that
    // hardware drift here for every aircraft, then remap the remaining range
    // so deliberate cyclic and pedal input still reaches full authority.
    const auto ApplyFlightControllerNeutralGuard = [](float Value)
    {
        constexpr float NeutralGuard = 0.22f;
        const float Magnitude = FMath::Abs(Value);
        if (Magnitude <= NeutralGuard)
        {
            return 0.0f;
        }
        return FMath::Sign(Value) *
            FMath::Clamp((Magnitude - NeutralGuard) / (1.0f - NeutralGuard), 0.0f, 1.0f);
    };
    if (bHasFlightPitch)
    {
        // Flight-controller profiles also carry DualSense pitch. Do not apply
        // the 22% HOTAS drift guard while aiming: it creates a large no-input
        // region followed by an abrupt pitch step. The zoom curve has its own
        // small noise guard and continuous response for precise sight control.
        FlightPitch = bApacheCombatZoomEnabled
            ? ApplyZoomPrecisionCurve(FlightPitch, true)
            : ApplyFlightControllerNeutralGuard(FlightPitch);
    }
    if (bHasFlightRoll) FlightRoll = ApplyFlightControllerNeutralGuard(FlightRoll);
    if (bHasFlightYaw) FlightYaw = ApplyFlightControllerNeutralGuard(FlightYaw);

    // Log the first meaningful excursion of each physical flight axis. This is
    // deliberately separate from keyboard/gamepad fusion so packaged flight
    // acceptance can prove that the configured HOTAS path drove the aircraft.
    const auto AuditControllerAxis = [this, Now](FName Action, bool bPresent, float Value)
    {
        constexpr float AcceptanceThreshold = 0.18f;
        if (!bPresent || FMath::Abs(Value) < AcceptanceThreshold)
        {
            return;
        }
        LastControllerAcceptanceAxisTime = Now;
        if (!ControllerAcceptanceAxesLogged.Contains(Action))
        {
            ControllerAcceptanceAxesLogged.Add(Action);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_CONTROLLER_ACCEPTANCE|AXIS|craft=%s|action=%s|value=%.3f|result=OBSERVED"),
                *SelectedAircraftId, *Action.ToString(), Value);
        }
    };
    AuditControllerAxis(RotorlineFlightControllerActions::Pitch, bHasFlightPitch, FlightPitch);
    AuditControllerAxis(RotorlineFlightControllerActions::Roll, bHasFlightRoll, FlightRoll);
    AuditControllerAxis(RotorlineFlightControllerActions::Yaw, bHasFlightYaw, FlightYaw);
    AuditControllerAxis(RotorlineFlightControllerActions::Collective,
        bHasFlightCollective || bHasFlightThrottle, FlightCollective);

    ForwardInput = bHasFlightPitch && FMath::Abs(FlightPitch) > 0.0f ? FlightPitch :
        (FMath::Abs(GamepadForward) > 0.0f ? GamepadForward : KeyboardForward);
    StrafeInput = bHasFlightRoll && FMath::Abs(FlightRoll) > 0.0f ? FlightRoll :
        (FMath::Abs(GamepadStrafe) > 0.0f ? GamepadStrafe : KeyboardStrafe);
    // Keyboard collective is an intentional hybrid override: Q/E must remain
    // usable even when a HOTAS profile also exposes a throttle/collective axis.
    CollectiveInput = bKeyboardCollectiveActive ? KeyboardCollective :
        ((bHasFlightCollective || bHasFlightThrottle) ? FlightCollective :
        (FMath::Abs(GamepadCollective) > 0.02f ? GamepadCollective : 0.0f));
    YawInput = bHasFlightYaw && FMath::Abs(FlightYaw) > 0.0f ? FlightYaw :
        (FMath::Abs(GamepadYaw) > 0.0f ? GamepadYaw : KeyboardYaw);

    if (IsBell222SpecialOperations() && !bFleetQualificationMode)
    {
        // Bell base tuning already mirrors the Apache. Keep the same direct
        // cyclic, collective, and pedal response rather than applying a second
        // precision curve and slow interpolation that makes the Bell float.
        Bell222SmoothedForwardInput = ForwardInput;
        Bell222SmoothedCollectiveInput = CollectiveInput;
    }
    else if (!IsBell222SpecialOperations())
    {
        Bell222SmoothedForwardInput = ForwardInput;
        Bell222SmoothedCollectiveInput = CollectiveInput;
        Bell222CurrentSpeedScale = 1.0f;
    }
    const float GamepadInputMagnitude = FMath::Max(
        FMath::Max(FMath::Abs(GamepadForward), FMath::Abs(GamepadYaw)),
        FMath::Max(FMath::Abs(GamepadStrafe), FMath::Abs(GamepadCollective)));
    const float KeyboardInputMagnitude = FMath::Max(
        FMath::Max(FMath::Abs(KeyboardForward), FMath::Abs(KeyboardYaw)),
        FMath::Max(FMath::Abs(KeyboardStrafe), FMath::Abs(KeyboardCollective)));
    if (GamepadInputMagnitude > 0.02f)
    {
        bLastInputWasGamepad = true;
    }
    else if (KeyboardInputMagnitude > 0.02f)
    {
        bLastInputWasGamepad = false;
    }

    // The headless fleet qualifier drives the same post-mapping control
    // values used by keyboard and DualSense input. This is isolated behind
    // the explicit command-line test mode and proves that each selected
    // airframe responds to collective, cyclic, strafe, and pedal control.
    if (bFleetQualificationMode && bFleetQualificationSkipStartup)
    {
        ForwardInput = 0.0f;
        StrafeInput = 0.0f;
        CollectiveInput = 0.0f;
        YawInput = 0.0f;
        if (FleetQualificationElapsed < 3.0f)
        {
            CollectiveInput = 0.70f;
        }
        else if (FleetQualificationElapsed < 7.0f)
        {
            ForwardInput = 0.58f;
            CollectiveInput = 0.12f;
        }
        else if (FleetQualificationElapsed < 11.0f)
        {
            if (SelectedAircraftId.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase) &&
                FleetQualificationElapsed < 9.0f)
            {
                ForwardInput = 1.0f;
                CollectiveInput = 0.05f;
            }
            else
            {
                StrafeInput = 0.46f;
                CollectiveInput = 0.08f;
                YawInput = 0.34f;
            }
        }
        else
        {
            ForwardInput = 0.18f;
            CollectiveInput = 0.06f;
        }
        bLastInputWasGamepad = true;
    }

    const bool bBell222 = IsBell222SpecialOperations();
    const bool bBellFullForwardBoost = bBell222 && ForwardInput >= 0.985f;
    const bool bBoosting = bBellFullForwardBoost ||
        PlayerController->IsInputKeyDown(EKeys::LeftShift) || PlayerController->IsInputKeyDown(EKeys::RightShift) ||
        PlayerController->IsInputKeyDown(EKeys::Gamepad_FaceButton_Left) ||
        (OperationsController && OperationsController->IsFlightControllerActionPressed(RotorlineFlightControllerActions::Boost)) ||
        (bFleetQualificationMode && bFleetQualificationSkipStartup &&
            SelectedAircraftId.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase) &&
            FleetQualificationElapsed >= 7.0f && FleetQualificationElapsed < 9.0f);
    const float TargetSpeedScale = bBoosting ? BoostMultiplier : 1.0f;
    if (bBell222)
    {
        // Spool into boost deliberately, then retain energy on release. The
        // cap now ramps down over several seconds instead of instantly
        // clamping an Bell222-speed aircraft back to helicopter speed.
        const float ScaleResponse = bBoosting ? 0.78f : 0.32f;
        Bell222CurrentSpeedScale = FMath::FInterpTo(
            Bell222CurrentSpeedScale, TargetSpeedScale, DeltaSeconds, ScaleResponse);
    }
    else
    {
        Bell222CurrentSpeedScale = TargetSpeedScale;
    }
    bBoostActive = bBoosting;
    const float ControlLoad = FMath::Max(
        FMath::Max(FMath::Abs(ForwardInput), FMath::Abs(StrafeInput)),
        FMath::Max(FMath::Abs(CollectiveInput), FMath::Abs(YawInput)));
    CurrentFuelBurnMultiplier = (bBoosting ? RotorlineHelicopter::BoostFuelBurnMultiplier : 1.0f) *
        FMath::Lerp(0.85f, 1.15f, ControlLoad);
    const float SpeedScale = bBell222 ? Bell222CurrentSpeedScale : TargetSpeedScale;

    float TargetPitch = -ForwardInput * MaxPitchAngle;
    if (bApacheCombatZoomEnabled)
    {
        if (!bZoomPitchCommandInitialized)
        {
            ZoomPitchCommandAngle = CurrentPitchAngle;
            bZoomPitchCommandInitialized = true;
        }

        // Zoom uses pitch-rate command rather than the normal self-centering
        // cyclic target. Releasing the stick therefore holds the current nose
        // angle instead of forcing the aircraft and sight back to level.
        // Combat zoom is an attitude-hold fine-aim mode. Keep full-stick
        // authority for steep attack angles, but accumulate that command
        // slowly enough that a controller can settle between corrections.
        constexpr float ZoomPitchCommandRate = 14.0f;
        const float ZoomPitchLimit = FMath::Max(MaxPitchAngle, 52.0f);
        ZoomPitchCommandAngle = FMath::Clamp(
            ZoomPitchCommandAngle - ForwardInput * ZoomPitchCommandRate * DeltaSeconds,
            -ZoomPitchLimit,
            ZoomPitchLimit);
        TargetPitch = ZoomPitchCommandAngle;
    }
    else
    {
        bZoomPitchCommandInitialized = false;
    }
    // Preserve fine control near center while giving full cyclic enough bank
    // authority for evasive turns. Apache remains deliberately heavier than
    // the lighter scout aircraft.
    const bool bApacheLikeHandling =
        SelectedAircraftId.Equals(TEXT("ah64_apache"), ESearchCase::IgnoreCase) ||
        SelectedAircraftId.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase);
    const float RollAuthority = bApacheLikeHandling
        ? 1.06f
        : 1.12f;
    const float TargetRoll = FMath::Clamp(
        StrafeInput * MaxRollAngle * RollAuthority,
        -MaxRollAngle * RollAuthority,
        MaxRollAngle * RollAuthority);
    const float HorizontalSpeedRatio = FMath::Clamp(
        CurrentVelocity.Size2D() / FMath::Max(1.0f, MaxForwardSpeed), 0.0f, 1.0f);
    const float HighSpeedCyclicResponse = FMath::Lerp(
        CyclicResponse,
        CyclicResponse * (bApacheLikeHandling ? 0.58f : 0.42f),
        HorizontalSpeedRatio);
    const float EffectivePitchResponse = bApacheCombatZoomEnabled
        ? FMath::Max(HighSpeedCyclicResponse, 7.0f)
        : HighSpeedCyclicResponse;
    CurrentPitchAngle = FMath::FInterpTo(
        CurrentPitchAngle, TargetPitch, DeltaSeconds, EffectivePitchResponse);

    // Diagnostic-only trace for the zoomed pitch instability. Keep the flight,
    // camera, and weapon behavior unchanged while recording each stage of the
    // control path so the source of any hold-then-jump can be identified from
    // a single packaged-game reproduction.
    if (bApacheCombatZoomEnabled && GetWorld())
    {
        static float LastZoomPitchTraceTime = -1000.0f;
        const float TraceTime = GetWorld()->GetTimeSeconds();
        if ((TraceTime - LastZoomPitchTraceTime) >= 0.10f)
        {
            LastZoomPitchTraceTime = TraceTime;
            const APlayerController* TraceController = Cast<APlayerController>(GetController());
            const float CameraPitch = TraceController
                ? TraceController->GetControlRotation().Pitch
                : 0.0f;
            const float VisualPitch = VisualRoot
                ? VisualRoot->GetComponentRotation().Pitch
                : 0.0f;

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("ROTORLINE_ZOOM_PITCH_TRACE craft=%s input=%.4f command=%.3f target=%.3f current=%.3f visual=%.3f camera=%.3f speed=%.1f response=%.3f"),
                *SelectedAircraftId,
                ForwardInput,
                ZoomPitchCommandAngle,
                TargetPitch,
                CurrentPitchAngle,
                VisualPitch,
                CameraPitch,
                CurrentVelocity.Size2D(),
                EffectivePitchResponse);
        }
    }
    CurrentRollAngle = FMath::FInterpTo(
        CurrentRollAngle, TargetRoll, DeltaSeconds, HighSpeedCyclicResponse);
    const float TargetYawRate =
        YawInput * MaxYawRate + CollectiveInput * CollectiveTorqueYawRate;
    CurrentYawRate = FMath::FInterpTo(CurrentYawRate, TargetYawRate, DeltaSeconds, YawResponse);

    AddActorWorldRotation(FRotator(0.0f, CurrentYawRate * DeltaSeconds, 0.0f));

    const FVector Forward = GetActorForwardVector();
    const FVector Right = GetActorRightVector();
    const float PitchRadians = FMath::DegreesToRadians(CurrentPitchAngle);
    const float RollRadians = FMath::DegreesToRadians(CurrentRollAngle);
    const float DiscVerticalFraction = FMath::Max(0.20f, FMath::Cos(PitchRadians) * FMath::Cos(RollRadians));
    const float HorizontalSpeed = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f).Size();
    const float BaseTranslationalLift = TranslationalLiftAcceleration *
        FMath::Clamp(HorizontalSpeed / 2500.0f, 0.0f, 1.0f);
    const bool bBell222Descending = IsBell222SpecialOperations() && CollectiveInput < 0.0f;
    const float BellDescentDemand = bBell222Descending
        ? FMath::Clamp(-CollectiveInput, 0.0f, 1.0f)
        : 0.0f;
    // Lowering collective on the Bell now unloads the rotor disc instead of
    // retaining full translational lift and floating through an approach.
    const float TranslationalLift = BaseTranslationalLift *
        (1.0f - 0.92f * BellDescentDemand);
    const float EffectiveCollectiveAcceleration = CollectiveAcceleration *
        (bBell222Descending ? 1.75f : 1.0f);
    const float BellWeightAcceleration = bBell222Descending
        ? (250.0f + 650.0f * BellDescentDemand) * BellDescentDemand
        : 0.0f;
    const float PositiveCollective = FMath::Max(0.0f, CollectiveInput);
    const FVector CoupledRotorAcceleration =
        Right * YawInput * PedalSideforceAcceleration +
        Forward * PositiveCollective * RotorDiscBiasAcceleration.X +
        Right * PositiveCollective * RotorDiscBiasAcceleration.Y;
    const float HighSpeedAccelerationRetention = bApacheLikeHandling ? 0.78f : 0.74f;
    const float InertialCyclicScale = FMath::Lerp(
        1.0f,
        HighSpeedAccelerationRetention,
        HorizontalSpeedRatio);
    const FVector FlightAcceleration =
        Forward * (-FMath::Sin(PitchRadians)) * CyclicAcceleration * SpeedScale * InertialCyclicScale +
        Right * FMath::Sin(RollRadians) * CyclicAcceleration * SpeedScale * InertialCyclicScale +
        CoupledRotorAcceleration +
        FVector::UpVector * (
            CollectiveInput * EffectiveCollectiveAcceleration +
            (DiscVerticalFraction - 1.0f) * BankLiftGravity +
            TranslationalLift - BellWeightAcceleration);

    CurrentVelocity += (FlightAcceleration + MissionWindAcceleration) * DeltaSeconds;
    float ForwardVelocity = FVector::DotProduct(CurrentVelocity, Forward);
    float LateralVelocity = FVector::DotProduct(CurrentVelocity, Right);
    float VerticalVelocity = CurrentVelocity.Z;
    ForwardVelocity *= FMath::Exp(-ForwardDrag * DeltaSeconds);
    LateralVelocity *= FMath::Exp(-LateralDrag * DeltaSeconds);
    const float EffectiveVerticalDrag = bBell222Descending
        ? FMath::Lerp(0.26f, 0.17f, BellDescentDemand)
        : VerticalDrag;
    VerticalVelocity *= FMath::Exp(-EffectiveVerticalDrag * DeltaSeconds);
    ForwardVelocity = FMath::Clamp(ForwardVelocity, -MaxReverseSpeed * SpeedScale, MaxForwardSpeed * SpeedScale);
    LateralVelocity = FMath::Clamp(LateralVelocity, -MaxStrafeSpeed * SpeedScale, MaxStrafeSpeed * SpeedScale);
    const float DescentLimit = IsBell222SpecialOperations() ? MaxVerticalSpeed * 0.80f : MaxVerticalSpeed;
    VerticalVelocity = FMath::Clamp(VerticalVelocity, -DescentLimit, MaxVerticalSpeed);
    CurrentVelocity = Forward * ForwardVelocity + Right * LateralVelocity + FVector::UpVector * VerticalVelocity;

    UpdateBellLairTransitCollision();
    const FVector PreImpactVelocity = CurrentVelocity;
    FHitResult Hit;
    CollisionBox->MoveComponent(CurrentVelocity * DeltaSeconds, GetActorRotation(), true, &Hit);
    if (Hit.IsValidBlockingHit())
    {
        HandleFlightCollision(Hit, PreImpactVelocity);
    }

    if (!bControllerAcceptanceTakeoffLogged && GetAboveGroundMeters() >= 2.0f &&
        (Now - LastControllerAcceptanceAxisTime) <= 1.0)
    {
        bControllerAcceptanceTakeoffLogged = true;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_CONTROLLER_ACCEPTANCE|TAKEOFF|craft=%s|agl_m=%.2f|result=PASS"),
            *SelectedAircraftId, GetAboveGroundMeters());
    }

    const FRotator DesiredAttitude(
        CurrentPitchAngle,
        0.0f,
        CurrentRollAngle - YawInput * 2.5f);
    VisualRoot->SetRelativeRotation(FMath::RInterpTo(
        VisualRoot->GetRelativeRotation(),
        DesiredAttitude,
        DeltaSeconds,
        AttitudeResponse * 1.5f));
}

void ARotorlineHelicopterPawn::UpdateBellLairTransitCollision()
{
    if (!CollisionBox)
    {
        return;
    }

    const FVector Location = GetActorLocation();
    const float LairFloorZ = RotorlineSupportLocations::BellLairPeak.Z -
        RotorlineSupportLocations::BellLairBurialDepthCm;
    const bool bInsideLaunchColumn =
        IsBellLairAuthorizedAircraft() &&
        FVector::Dist2D(Location, RotorlineSupportLocations::BellLairPeak) <= 4300.0f &&
        Location.Z >= LairFloorZ + 600.0f &&
        Location.Z <= RotorlineSupportLocations::BellLairPeak.Z + 26000.0f;

    if (bInsideLaunchColumn == bBellLairWorldStaticPassThrough)
    {
        return;
    }

    bBellLairWorldStaticPassThrough = bInsideLaunchColumn;
    CollisionBox->SetCollisionResponseToChannel(
        ECC_WorldStatic,
        bInsideLaunchColumn ? ECR_Ignore : ECR_Block);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL_LAIR|TRANSIT_COLLISION|world_static=%s|location=%.0f,%.0f,%.0f|floor_z=%.0f"),
        bInsideLaunchColumn ? TEXT("IGNORED") : TEXT("BLOCKED"),
        Location.X,
        Location.Y,
        Location.Z,
        LairFloorZ);
}

void ARotorlineHelicopterPawn::HandleFlightCollision(
    const FHitResult& Hit,
    const FVector& PreImpactVelocity)
{
    if (!Hit.IsValidBlockingHit() || bPlayerAircraftDying)
    {
        return;
    }

    const FVector ImpactNormal = Hit.ImpactNormal.GetSafeNormal();
    const float ClosingSpeed = FMath::Max(0.0f, -FVector::DotProduct(PreImpactVelocity, ImpactNormal));
    const FVector TangentialVelocity = FVector::VectorPlaneProject(PreImpactVelocity, ImpactNormal);
    const float TangentialSpeed = TangentialVelocity.Size();
    const float TotalSpeed = PreImpactVelocity.Size();
    const AActor* HitActor = Hit.GetActor();
    const FString HitActorName = HitActor ? HitActor->GetActorNameOrLabel() : FString();
    if (HitActor && HitActor->ActorHasTag(TEXT("RotorlineWaterSurface")))
    {
        MissionFailureReason = TEXT("WATER IMPACT // AIRCRAFT LOST");
        CurrentHealth = 0.0f;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_WATER_IMPACT|mode=POWERED|speed_cm_s=%.1f|closing_cm_s=%.1f|actor=%s"),
            TotalSpeed,
            ClosingSpeed,
            *HitActorName);
        BeginPlayerDestruction();
        return;
    }
    const bool bCarrierSurface = HitActor &&
        HitActor->ActorHasTag(TEXT("RotorlineCarrierStation"));
    const bool bPreparedLandingSurface = HitActor &&
        (bCarrierSurface ||
            HitActor->ActorHasTag(TEXT("RotorlineMissionPad")) ||
            HitActorName.Contains(TEXT("helipad"), ESearchCase::IgnoreCase) ||
            HitActorName.Contains(TEXT("rearm"), ESearchCase::IgnoreCase) ||
            HitActorName.Contains(TEXT("landing_pad"), ESearchCase::IgnoreCase));
    const bool bInsideServiceArea = bInsideBaseServiceZone || bInsideCityServiceZone;
    const float ContactAGL = GetAboveGroundMeters();
    const double ContactTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1000.0;
    const bool bGentleServicePadContact =
        bInsideServiceArea &&
        ContactAGL >= -1.0f &&
        ContactAGL <= 5.5f &&
        ClosingSpeed <= 650.0f &&
        TangentialSpeed <= 1200.0f;
    const bool bGentleCarrierDeckContact =
        bCarrierSurface &&
        ImpactNormal.Z >= 0.55f &&
        ClosingSpeed <= 650.0f &&
        TangentialSpeed <= 1200.0f;
    const bool bSettledPadContactLatch =
        bPreparedLandingSurface &&
        bInsideServiceArea &&
        ContactAGL >= -1.0f &&
        ContactAGL <= 5.5f &&
        ContactTime - LastSafeServicePadContactTime <= 8.0;
    const bool bControlledServicePadContact =
        bGentleServicePadContact || bGentleCarrierDeckContact ||
        bSettledPadContactLatch;

    // Service pads can return collision hits from an edge component whose
    // actor has no useful pad tag or name. A grounded, low-speed aircraft
    // inside a service zone is a controlled pad contact regardless of which
    // triangle generated the hit. Absorb the nudge without damage or failure.
    if (bControlledServicePadContact)
    {
        CurrentVelocity = TangentialVelocity * 0.22f;
        CurrentVelocity.Z = 0.0f;
        LastSafeServicePadContactTime = ContactTime;
        if (ContactTime - LastServicePadContactLogTime >= 0.75)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_IMPACT_PHYSICS|result=SERVICE_PAD_CONTACT|mode=%s|closing_mps=%.1f|tangent_mps=%.1f|agl_m=%.1f|actor=%s"),
                bSettledPadContactLatch
                    ? TEXT("SETTLED_LATCH")
                    : (bGentleCarrierDeckContact
                        ? TEXT("CARRIER_DECK")
                        : TEXT("GENTLE_TOUCHDOWN")),
                ClosingSpeed / 100.0f,
                TangentialSpeed / 100.0f,
                ContactAGL,
                HitActor ? *HitActorName : TEXT("WORLD"));
            LastServicePadContactLogTime = ContactTime;
        }
        return;
    }
    // Pad edge triangles can report a near-vertical normal for a single frame.
    // Classify the authored landing surface by identity so a gentle touchdown
    // on the Mission 10 reload pad cannot become an AIRFRAME IMPACT.
    const bool bGroundLikeContact = ImpactNormal.Z >= 0.55f || bPreparedLandingSurface || bInsideServiceArea;

    // Ground contacts retain a useful landing envelope. Walls, roofs, towers,
    // and other near-vertical structures have a much lower survivable closing
    // speed because the airframe cannot absorb that energy through the gear.
    const bool bFatalImpact = bGroundLikeContact
        ? (ClosingSpeed >= 1000.0f ||
            (ClosingSpeed >= 650.0f && TangentialSpeed >= 2800.0f) ||
            (TotalSpeed >= 3600.0f && ClosingSpeed >= 400.0f))
        : (ClosingSpeed >= 550.0f || (TotalSpeed >= 1200.0f && ClosingSpeed >= 250.0f));

    if (bFatalImpact)
    {
        const float HealthBefore = CurrentHealth;
        CurrentHealth = 0.0f;
        if (ARotorlineOperationsPlayerController* OperationsController =
            Cast<ARotorlineOperationsPlayerController>(Controller))
        {
            OperationsController->NotifyDamageTaken(HealthBefore);
        }

        const float ReboundSpeed = FMath::Clamp(ClosingSpeed * 0.22f, 180.0f, 900.0f);
        CurrentVelocity = TangentialVelocity * 0.32f + ImpactNormal * ReboundSpeed;
        CurrentVelocity.Z = FMath::Min(CurrentVelocity.Z, -350.0f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_IMPACT_PHYSICS|result=FATAL|surface=%s|closing_mps=%.1f|tangent_mps=%.1f|total_mps=%.1f|actor=%s"),
            bGroundLikeContact ? TEXT("GROUND") : TEXT("STRUCTURE"),
            ClosingSpeed / 100.0f,
            TangentialSpeed / 100.0f,
            TotalSpeed / 100.0f,
            Hit.GetActor() ? *Hit.GetActor()->GetActorNameOrLabel() : TEXT("WORLD"));
        FailMission(bGroundLikeContact ? TEXT("HARD LANDING") : TEXT("AIRFRAME IMPACT"));
        return;
    }

    // A helicopter with skids or wheels can absorb a controlled run-on
    // landing. Horizontal motion does not become damaging until it exceeds
    // the published 45 km/h safe envelope; vertical energy remains dominant.
    const float ContactSeverity = bGroundLikeContact
        ? ClosingSpeed + FMath::Max(0.0f, TangentialSpeed - 1200.0f) * 0.18f
        : ClosingSpeed + TangentialSpeed * 0.45f;
    const float DamageThreshold = bGroundLikeContact ? 500.0f : 350.0f;
    if (ContactSeverity >= DamageThreshold)
    {
        const float ImpactDamage = bGroundLikeContact
            ? FMath::Clamp((ContactSeverity - 450.0f) * 0.012f, 1.0f, 20.0f)
            : FMath::Clamp((ContactSeverity - 300.0f) * 0.018f, 2.0f, 28.0f);
        const float HealthBefore = CurrentHealth;
        CurrentHealth = FMath::Max(0.0f, CurrentHealth - ImpactDamage);
        if (ARotorlineOperationsPlayerController* OperationsController =
            Cast<ARotorlineOperationsPlayerController>(Controller))
        {
            OperationsController->NotifyDamageTaken(HealthBefore - CurrentHealth);
        }
        if (CurrentHealth <= 0.0f)
        {
            FailMission(TEXT("AIRFRAME IMPACT"));
            return;
        }
    }

    const bool bControlledSkid = bGroundLikeContact && ClosingSpeed <= 550.0f && TangentialSpeed <= 1400.0f;
    const float Restitution = bGroundLikeContact ? (bControlledSkid ? 0.015f : 0.04f) : 0.16f;
    const float TangentialRetention = bGroundLikeContact ? (bControlledSkid ? 0.82f : 0.62f) : 0.24f;
    CurrentVelocity = TangentialVelocity * TangentialRetention + ImpactNormal * ClosingSpeed * Restitution;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_IMPACT_PHYSICS|result=%s|surface=%s|closing_mps=%.1f|tangent_mps=%.1f|health=%.1f"),
        bControlledSkid ? TEXT("SKID") : TEXT("SURVIVED"),
        bGroundLikeContact ? TEXT("GROUND") : TEXT("STRUCTURE"),
        ClosingSpeed / 100.0f,
        TangentialSpeed / 100.0f,
        CurrentHealth);
}

void ARotorlineHelicopterPawn::UpdateFuel(float DeltaSeconds)
{
    if (!bEngineReady || bFuelStarved || bPlayerAircraftDying || bMissionFailed || bMissionComplete)
    {
        return;
    }

    // Once emergency servicing has started, preserve the remaining fumes long
    // enough for the three-second ground-service cycle to finish. Leaving the
    // pad or otherwise interrupting service immediately resumes normal burn.
    if (bBaseRearmActive &&
        FuelRemainingPercent <= RotorlineHelicopter::FuelWarningFumesPercent)
    {
        return;
    }

    const float FuelEnduranceSeconds = SelectedAircraftDefinition.FuelEnduranceSeconds > 0.0f
        ? SelectedAircraftDefinition.FuelEnduranceSeconds
        : RotorlineHelicopter::DefaultFuelEnduranceSeconds;
    FuelRemainingPercent = FMath::Max(
        0.0f,
        FuelRemainingPercent -
            (100.0f / FuelEnduranceSeconds) * CurrentFuelBurnMultiplier * DeltaSeconds);

    if (!bFuelLowWarningIssued && FuelRemainingPercent <= RotorlineHelicopter::FuelWarningLowPercent)
    {
        bFuelLowWarningIssued = true;
        BroadcastRadio(TEXT("CREW: Fuel state low. Plan the return or hit a service pad."), 5.5f, false);
    }
    if (!bFuelCriticalWarningIssued && FuelRemainingPercent <= RotorlineHelicopter::FuelWarningCriticalPercent)
    {
        bFuelCriticalWarningIssued = true;
        BroadcastRadio(TEXT("CREW: Fuel critical. Recover now."), 4.5f, false);
    }
    if (!bFuelFumesWarningIssued && FuelRemainingPercent <= RotorlineHelicopter::FuelWarningFumesPercent)
    {
        bFuelFumesWarningIssued = true;
        BroadcastRadio(TEXT("CREW: Running on fumes."), 3.8f, false);
    }

    if (FuelRemainingPercent <= 0.0f)
    {
        bFuelStarved = true;
        bEngineReady = false;
        CurrentFuelBurnMultiplier = 0.0f;
        StopApacheCannonAudio(TEXT("FUEL_STARVATION"), 0.05f);
        EngineStartupAudio->FadeOut(0.25f, 0.0f);
        EngineTakeoffAudio->FadeOut(0.25f, 0.0f);
        EngineFlightAudio->FadeOut(0.35f, 0.0f);
        BroadcastRadio(TEXT("CREW: Fuel exhausted! Engine power lost!"), 5.0f, false);
        UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_FUEL|state=EXHAUSTED|engine_power=0"));
    }
}

void ARotorlineHelicopterPawn::BeginPlayerDestruction()
{
    if (bPlayerAircraftDying)
    {
        return;
    }

    bPlayerAircraftDying = true;
    bPlayerCrashImpact = false;
    PlayerDeathElapsed = 0.0f;
    PlayerCrashImpactElapsed = 0.0f;
    PlayerCrashSpinDirection = FMath::FRand() < 0.5f ? -1.0f : 1.0f;
    PlayerCrashFallSpeed = FMath::Min(CurrentVelocity.Z, -350.0f);
    ForwardInput = 0.0f;
    StrafeInput = 0.0f;
    CollectiveInput = 0.0f;
    YawInput = 0.0f;
    CurrentYawRate = 0.0f;
    bEngineReady = false;
    StopApacheCannonAudio(TEXT("PLAYER_DESTROYED"), 0.02f);
    EngineStartupAudio->FadeOut(0.08f, 0.0f);
    EngineTakeoffAudio->FadeOut(0.08f, 0.0f);
    EngineFlightAudio->FadeOut(0.08f, 0.0f);

    const bool bUsingNiagaraAirExplosion = PlayerAirExplosionSystem != nullptr;
    if (bUsingNiagaraAirExplosion)
    {
        RotorlineExplosionFx::SpawnTransient(
            this,
            PlayerAirExplosionSystem,
            GetActorLocation(),
            GetActorRotation(),
            FVector(0.9f),
            1.2f,
            4.5f);
    }
    PlayerExplosionCore->SetVisibility(!bUsingNiagaraAirExplosion, true);
    PlayerExplosionSmoke->SetVisibility(!bUsingNiagaraAirExplosion, true);
    PlayerExplosionSparks->SetVisibility(!bUsingNiagaraAirExplosion, true);
    PlayerExplosionCore->SetRelativeLocation(FVector::ZeroVector);
    PlayerExplosionSmoke->SetRelativeLocation(FVector::ZeroVector);
    PlayerExplosionSparks->SetRelativeLocation(FVector(0.0f, 0.0f, -2175.0f));
    PlayerExplosionCore->SetRelativeScale3D(FVector(0.55f));
    PlayerExplosionSmoke->SetRelativeScale3D(FVector(0.75f));
    PlayerExplosionSparks->SetRelativeScale3D(FVector(9.0f));
    PlayerExplosionLight->SetIntensity(165000.0f);
    if (PlayerExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            PlayerExplosionSound,
            GetActorLocation(),
            0.9f * GetAudioMix(ERotorlineAudioChannel::WeaponsExplosions));
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_PLAYER_DESTRUCTION|state=EXPLOSION|health=%.1f|velocity_mps=%.1f|controls=DISABLED|fx=%s"),
        CurrentHealth,
        CurrentVelocity.Size() / 100.0f,
        bUsingNiagaraAirExplosion ? TEXT("NIAGARA") : TEXT("LEGACY_FALLBACK"));
}

void ARotorlineHelicopterPawn::UpdatePlayerDestruction(float DeltaSeconds)
{
    PlayerDeathElapsed += DeltaSeconds;
    CurrentRotorPlayRate = FMath::Max(0.0f, CurrentRotorPlayRate - DeltaSeconds * 0.85f);
    ApplyActiveRotorAnimationRates();
    UpdateMD500RotorAnimation(DeltaSeconds);

    if (bPlayerCrashImpact)
    {
        PlayerCrashImpactElapsed += DeltaSeconds;
        CurrentVelocity = FVector::ZeroVector;
        const float ImpactAlpha = FMath::Clamp(PlayerCrashImpactElapsed / 0.9f, 0.0f, 1.0f);
        if (!PlayerGroundExplosionSystem)
        {
            PlayerExplosionCore->SetRelativeScale3D(FVector(FMath::Lerp(1.5f, 10.0f, ImpactAlpha)));
            PlayerExplosionCore->SetVisibility(PlayerCrashImpactElapsed < 0.8f, true);
            PlayerExplosionSmoke->SetRelativeScale3D(FVector(FMath::Lerp(5.0f, 15.0f, ImpactAlpha)));
            PlayerExplosionSmoke->SetRelativeLocation(FVector(0.0f, 0.0f, ImpactAlpha * 450.0f));
            PlayerExplosionSparks->SetVisibility(PlayerCrashImpactElapsed < 0.55f, true);
            PlayerExplosionSparks->AddLocalRotation(FRotator(80.0f, 120.0f, 55.0f) * DeltaSeconds);
        }
        PlayerExplosionLight->SetIntensity(FMath::Lerp(175000.0f, 0.0f, ImpactAlpha));
        return;
    }

    const float BlastAlpha = FMath::Clamp(PlayerDeathElapsed / 0.8f, 0.0f, 1.0f);
    if (!PlayerAirExplosionSystem)
    {
        PlayerExplosionCore->SetRelativeScale3D(FVector(FMath::Lerp(0.55f, 8.0f, BlastAlpha)));
        PlayerExplosionCore->SetVisibility(PlayerDeathElapsed < 0.85f, true);
        PlayerExplosionSmoke->SetRelativeScale3D(FVector(FMath::Lerp(0.75f, 8.5f, FMath::Clamp(PlayerDeathElapsed / 2.2f, 0.0f, 1.0f))));
        PlayerExplosionSmoke->SetRelativeLocation(FVector(0.0f, 0.0f, FMath::Min(PlayerDeathElapsed * 90.0f, 350.0f)));
        PlayerExplosionSparks->SetVisibility(PlayerDeathElapsed < 0.65f, true);
        PlayerExplosionSparks->AddLocalRotation(FRotator(90.0f, 135.0f, 60.0f) * DeltaSeconds);
    }
    PlayerExplosionLight->SetIntensity(FMath::Lerp(165000.0f, 0.0f, BlastAlpha));

    PlayerCrashFallSpeed = FMath::Max(-7200.0f, PlayerCrashFallSpeed - 1550.0f * DeltaSeconds);
    const float HorizontalDamping = FMath::Exp(-0.16f * DeltaSeconds);
    CurrentVelocity.X *= HorizontalDamping;
    CurrentVelocity.Y *= HorizontalDamping;
    CurrentVelocity.Z = PlayerCrashFallSpeed;
    AddActorWorldRotation(FRotator(0.0f, PlayerCrashSpinDirection * 38.0f * DeltaSeconds, 0.0f));
    const FRotator CrashAttitude(
        FMath::Clamp(PlayerDeathElapsed * 13.0f, 0.0f, 52.0f),
        0.0f,
        PlayerCrashSpinDirection * FMath::Clamp(PlayerDeathElapsed * 31.0f, 0.0f, 82.0f));
    VisualRoot->SetRelativeRotation(FMath::RInterpTo(
        VisualRoot->GetRelativeRotation(), CrashAttitude, DeltaSeconds, 2.4f));

    FHitResult CrashHit;
    CollisionBox->MoveComponent(CurrentVelocity * DeltaSeconds, GetActorRotation(), true, &CrashHit);
    const bool bGroundImpact = CrashHit.IsValidBlockingHit() || GetAboveGroundMeters() <= 0.6f;
    if (!bGroundImpact)
    {
        return;
    }

    bPlayerCrashImpact = true;
    PlayerCrashImpactElapsed = 0.0f;
    CurrentVelocity = FVector::ZeroVector;
    const bool bUsingNiagaraGroundExplosion = PlayerGroundExplosionSystem != nullptr;
    if (bUsingNiagaraGroundExplosion)
    {
        const FVector GroundExplosionLocation = CrashHit.IsValidBlockingHit()
            ? CrashHit.ImpactPoint
            : GetActorLocation();
        RotorlineExplosionFx::SpawnTransient(
            this,
            PlayerGroundExplosionSystem,
            GroundExplosionLocation,
            FRotator::ZeroRotator,
            FVector(1.1f),
            1.35f,
            5.0f);
    }
    PlayerExplosionCore->SetVisibility(!bUsingNiagaraGroundExplosion, true);
    PlayerExplosionSmoke->SetVisibility(!bUsingNiagaraGroundExplosion, true);
    PlayerExplosionSparks->SetVisibility(!bUsingNiagaraGroundExplosion, true);
    PlayerExplosionCore->SetRelativeScale3D(FVector(1.5f));
    PlayerExplosionSmoke->SetRelativeScale3D(FVector(5.0f));
    PlayerExplosionSparks->SetRelativeScale3D(FVector(12.0f));
    PlayerExplosionLight->SetIntensity(175000.0f);
    if (PlayerExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            PlayerExplosionSound,
            GetActorLocation(),
            0.75f * GetAudioMix(ERotorlineAudioChannel::WeaponsExplosions));
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_PLAYER_DESTRUCTION|state=GROUND_IMPACT|elapsed=%.2f|menu=READY|controls=DISABLED|fx=%s"),
        PlayerDeathElapsed,
        bUsingNiagaraGroundExplosion ? TEXT("NIAGARA") : TEXT("LEGACY_FALLBACK"));
}

void ARotorlineHelicopterPawn::UpdateEngineAudio(float DeltaSeconds)
{
    if (bPlayerAircraftDying)
    {
        return;
    }
    if (!bFlightEngineAudioStarted || !EngineFlightAudio->IsPlaying())
    {
        return;
    }

    const float SpeedLoad = FMath::Clamp(CurrentVelocity.Size2D() / MaxForwardSpeed, 0.0f, 1.0f);
    const float ControlLoad = FMath::Max(SpeedLoad, FMath::Abs(CollectiveInput));
    const bool bChinookFlightProfile = SelectedAircraftId.Equals(TEXT("ch47_chinook"), ESearchCase::IgnoreCase);
    const float TargetPitch = bChinookFlightProfile ? 1.0f : FMath::Lerp(0.98f, 1.04f, ControlLoad);
    const float EngineBaseVolume = bMissionBriefActive ? EngineDuckedVolume : EngineFlightVolume;
    const float LoadVolumeBoost = bMissionBriefActive ? 0.02f : 0.07f;
    const float ChinookVolumeScale = bChinookFlightProfile ? 1.12f : 1.0f;
    const float TargetVolume = FMath::Lerp(EngineBaseVolume, EngineBaseVolume + LoadVolumeBoost, ControlLoad)
        * ChinookVolumeScale
        * GetAudioMix(ERotorlineAudioChannel::Engine);
    EngineFlightAudio->SetPitchMultiplier(FMath::FInterpTo(
        EngineFlightAudio->PitchMultiplier,
        TargetPitch,
        DeltaSeconds,
        2.0f));
    EngineFlightAudio->SetVolumeMultiplier(FMath::FInterpTo(
        EngineFlightAudio->VolumeMultiplier,
        TargetVolume,
        DeltaSeconds,
        2.0f));
}

void ARotorlineHelicopterPawn::InitializeRotorDownwash()
{
    if (bDownwashInitialized)
    {
        return;
    }

    bDownwashInitialized = true;
    DownwashGroundMaterial = DownwashGroundSheet->CreateDynamicMaterialInstance(0);
    DownwashGroundMaterialSecondary = DownwashGroundSheetSecondary->CreateDynamicMaterialInstance(0);
    if (DownwashGroundMaterialSecondary)
    {
        DownwashGroundMaterialSecondary->SetScalarParameterValue(TEXT("Phase"), 2.73f);
    }

    UStaticMesh* PuffMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* PuffMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Game/Effects/RotorDownwash/M_RotorDownwashVolume.M_RotorDownwashVolume"));
    const int32 PuffPoolSize = (PuffMesh && PuffMaterial) ? 44 : 0;
    DownwashPuffMeshes.Reserve(PuffPoolSize);
    DownwashPuffMaterials.Reserve(PuffPoolSize);
    DownwashPuffVelocities.Init(FVector::ZeroVector, PuffPoolSize);
    DownwashPuffAges.Init(-1.0f, PuffPoolSize);
    DownwashPuffLifetimes.Init(1.0f, PuffPoolSize);
    DownwashPuffStartScales.Init(1.0f, PuffPoolSize);
    DownwashPuffBaseOpacities.Init(0.0f, PuffPoolSize);

    for (int32 Index = 0; Index < PuffPoolSize; ++Index)
    {
        UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(
            this,
            *FString::Printf(TEXT("RotorDownwashPuff_%02d"), Index));
        Puff->SetupAttachment(CollisionBox);
        Puff->SetAbsolute(true, true, true);
        Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Puff->SetCastShadow(false);
        Puff->SetVisibility(false, true);
        Puff->SetStaticMesh(PuffMesh);
        Puff->SetMaterial(0, PuffMaterial);
        AddInstanceComponent(Puff);
        Puff->RegisterComponent();
        DownwashPuffMeshes.Add(Puff);
        DownwashPuffMaterials.Add(Puff->CreateDynamicMaterialInstance(0));
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_DOWNWASH|READY|presentation=POOLED_DUST|pool=%d|surfaces=GRASS,SAND,WATER,CONCRETE|altitude_driven=1|rotor_driven=1"),
        PuffPoolSize);
}

void ARotorlineHelicopterPawn::UpdateRotorDownwash(float DeltaSeconds)
{
    if (!bDownwashInitialized || !GetWorld())
    {
        return;
    }

    // Let already-emitted material remain in world space and dissipate after
    // the helicopter has moved away or climbed out of ground effect.
    for (int32 Index = 0; Index < DownwashPuffMeshes.Num(); ++Index)
    {
        UStaticMeshComponent* Puff = DownwashPuffMeshes[Index];
        if (!Puff || DownwashPuffAges[Index] < 0.0f)
        {
            continue;
        }

        DownwashPuffAges[Index] += DeltaSeconds;
        const float Alpha = FMath::Clamp(
            DownwashPuffAges[Index] / FMath::Max(0.05f, DownwashPuffLifetimes[Index]),
            0.0f,
            1.0f);
        if (Alpha >= 1.0f)
        {
            Puff->SetVisibility(false, true);
            DownwashPuffAges[Index] = -1.0f;
            continue;
        }

        FVector Velocity = DownwashPuffVelocities[Index];
        Puff->SetWorldLocation(Puff->GetComponentLocation() + Velocity * DeltaSeconds);
        Velocity *= FMath::Exp(-0.84f * DeltaSeconds);
        Velocity.Z += 18.0f * DeltaSeconds;
        DownwashPuffVelocities[Index] = Velocity;
        const float Expansion = FMath::Lerp(1.0f, 1.85f, Alpha);
        const float HorizontalScale = DownwashPuffStartScales[Index] * Expansion;
        Puff->SetWorldScale3D(FVector(HorizontalScale, HorizontalScale, HorizontalScale * 0.30f));
        if (UMaterialInstanceDynamic* Material = DownwashPuffMaterials[Index])
        {
            const float Fade = FMath::Square(1.0f - Alpha) * FMath::SmoothStep(0.0f, 0.12f, Alpha);
            Material->SetScalarParameterValue(TEXT("Opacity"), DownwashPuffBaseOpacities[Index] * Fade);
        }
    }

    FHitResult SurfaceHit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RotorlineDownwashSurface), false, this);
    TraceParams.AddIgnoredActor(this);
    const FVector ActorLocation = GetActorLocation();
    const FVector TraceEnd = ActorLocation - FVector(0.0f, 0.0f, 3200.0f);
    const bool bHitSurface = GetWorld()->LineTraceSingleByChannel(
        SurfaceHit,
        ActorLocation,
        TraceEnd,
        ECC_Visibility,
        TraceParams);

    enum class EDownwashSurface : uint8 { Grass, Sand, Water, Concrete };
    EDownwashSurface Surface = EDownwashSurface::Grass;
    FVector SurfacePoint = bHitSurface ? SurfaceHit.ImpactPoint : FVector(ActorLocation.X, ActorLocation.Y, -35.0f);
    FString HitName = bHitSurface
        ? (GetNameSafe(SurfaceHit.GetActor()) + TEXT(" ") + GetNameSafe(SurfaceHit.GetComponent())).ToLower()
        : FString();
    if (bHitSurface && SurfaceHit.GetComponent())
    {
        const int32 MaterialCount = SurfaceHit.GetComponent()->GetNumMaterials();
        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            HitName += TEXT(" ");
            HitName += GetNameSafe(SurfaceHit.GetComponent()->GetMaterial(MaterialIndex)).ToLower();
        }
    }
    const bool bNamedWater = HitName.Contains(TEXT("water")) || HitName.Contains(TEXT("ocean"));
    const bool bLikelyOcean = bHitSurface && SurfaceHit.ImpactPoint.Z < -200.0f;
    if (bNamedWater || bLikelyOcean)
    {
        Surface = EDownwashSurface::Water;
        SurfacePoint.Z = -35.0f;
    }
    else if (!bHitSurface)
    {
        DownwashGroundSheet->SetVisibility(false, true);
        DownwashGroundSheetSecondary->SetVisibility(false, true);
        return;
    }
    else if (HitName.Contains(TEXT("pad")) || HitName.Contains(TEXT("runway")) ||
        HitName.Contains(TEXT("road")) || HitName.Contains(TEXT("asphalt")) ||
        HitName.Contains(TEXT("concrete")))
    {
        Surface = EDownwashSurface::Concrete;
    }
    else if (SurfacePoint.Z < 3400.0f)
    {
        Surface = EDownwashSurface::Sand;
    }

    const float GroundClearanceCm = FMath::Max(
        0.0f,
        ActorLocation.Z - SurfacePoint.Z - CollisionBox->GetScaledBoxExtent().Z);
    const float RotorFactor = FMath::Clamp(
        CurrentRotorPlayRate / FMath::Max(0.01f, RotorFlightPlayRate),
        0.0f,
        1.0f);
    const float AltitudeFactor = 1.0f - FMath::SmoothStep(250.0f, 2400.0f, GroundClearanceCm);
    const float ControlLoad = FMath::Clamp(
        0.72f + FMath::Abs(CollectiveInput) * 0.28f + CurrentVelocity.Size2D() / FMath::Max(1.0f, MaxForwardSpeed) * 0.10f,
        0.72f,
        1.10f);
    const float Intensity = FMath::Clamp(FMath::Pow(RotorFactor, 1.35f) * AltitudeFactor * ControlLoad, 0.0f, 1.0f);
    if (Intensity < 0.025f || bPlayerAircraftDying)
    {
        DownwashGroundSheet->SetVisibility(false, true);
        DownwashGroundSheetSecondary->SetVisibility(false, true);
        return;
    }

    const FLinearColor SurfaceTint =
        Surface == EDownwashSurface::Water ? FLinearColor(0.58f, 0.76f, 0.82f, 1.0f) :
        Surface == EDownwashSurface::Sand ? FLinearColor(0.72f, 0.58f, 0.32f, 1.0f) :
        Surface == EDownwashSurface::Concrete ? FLinearColor(0.58f, 0.55f, 0.48f, 1.0f) :
        FLinearColor(0.48f, 0.42f, 0.24f, 1.0f);
    const float GroundOpacity =
        Surface == EDownwashSurface::Water ? 0.38f :
        Surface == EDownwashSurface::Sand ? 0.58f :
        Surface == EDownwashSurface::Concrete ? 0.30f : 0.42f;
    const float PuffOpacity =
        Surface == EDownwashSurface::Water ? 0.28f :
        Surface == EDownwashSurface::Sand ? 0.38f :
        Surface == EDownwashSurface::Concrete ? 0.12f : 0.24f;
    const float RotorRadiusCm = FMath::Clamp(CollisionBox->GetScaledBoxExtent().X * 1.65f, 470.0f, 860.0f);
    const float SpreadRadiusCm = RotorRadiusCm * FMath::Clamp(1.0f + GroundClearanceCm / 2100.0f, 1.0f, 1.78f);
    const float WorldTime = GetWorld()->GetTimeSeconds();

    for (int32 Layer = 0; Layer < 2; ++Layer)
    {
        UStaticMeshComponent* Sheet = Layer == 0 ? DownwashGroundSheet.Get() : DownwashGroundSheetSecondary.Get();
        UMaterialInstanceDynamic* Material = Layer == 0 ? DownwashGroundMaterial.Get() : DownwashGroundMaterialSecondary.Get();
        const float LayerScale = Layer == 0 ? 1.0f : 0.72f;
        Sheet->SetWorldLocation(SurfacePoint + FVector(0.0f, 0.0f, 12.0f + Layer * 7.0f));
        Sheet->SetWorldRotation(FRotator(0.0f, GetActorRotation().Yaw + WorldTime * (Layer == 0 ? 17.0f : -24.0f), 0.0f));
        Sheet->SetWorldScale3D(FVector(SpreadRadiusCm / 50.0f * LayerScale));
        Sheet->SetVisibility(true, true);
        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("Tint"), SurfaceTint);
            Material->SetScalarParameterValue(TEXT("Opacity"), GroundOpacity * Intensity * (Layer == 0 ? 1.0f : 0.46f));
        }
    }

    const float SpawnRate =
        Surface == EDownwashSurface::Water ? 18.0f :
        Surface == EDownwashSurface::Sand ? 16.0f :
        Surface == EDownwashSurface::Concrete ? 5.0f : 10.0f;
    DownwashSpawnAccumulator += SpawnRate * Intensity * DeltaSeconds;
    while (DownwashSpawnAccumulator >= 1.0f && !DownwashPuffMeshes.IsEmpty())
    {
        DownwashSpawnAccumulator -= 1.0f;
        const int32 Index = DownwashNextPuffIndex++ % DownwashPuffMeshes.Num();
        UStaticMeshComponent* Puff = DownwashPuffMeshes[Index];
        UMaterialInstanceDynamic* Material = DownwashPuffMaterials[Index];
        const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
        const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        const FVector Tangent(-Radial.Y, Radial.X, 0.0f);
        const float SpawnRadius = FMath::FRandRange(0.18f, 0.62f) * SpreadRadiusCm;
        const FVector SpawnLocation = SurfacePoint + Radial * SpawnRadius + FVector(0.0f, 0.0f, FMath::FRandRange(22.0f, 72.0f));
        const float RadialSpeed = FMath::FRandRange(390.0f, 760.0f) * Intensity;
        const float TangentSpeed = FMath::FRandRange(-250.0f, 250.0f) * Intensity;
        const float LiftSpeed = Surface == EDownwashSurface::Water
            ? FMath::FRandRange(45.0f, 105.0f)
            : FMath::FRandRange(12.0f, 58.0f);
        DownwashPuffVelocities[Index] = Radial * RadialSpeed + Tangent * TangentSpeed +
            CurrentVelocity * 0.08f + FVector(0.0f, 0.0f, LiftSpeed);
        DownwashPuffAges[Index] = 0.001f;
        DownwashPuffLifetimes[Index] = Surface == EDownwashSurface::Water
            ? FMath::FRandRange(0.62f, 0.98f)
            : FMath::FRandRange(0.78f, 1.35f);
        DownwashPuffStartScales[Index] =
            (Surface == EDownwashSurface::Water ? FMath::FRandRange(0.82f, 1.35f) : FMath::FRandRange(0.55f, 1.05f)) *
            FMath::Lerp(0.72f, 1.0f, Intensity);
        DownwashPuffBaseOpacities[Index] = PuffOpacity * Intensity * FMath::FRandRange(0.68f, 1.0f);
        Puff->SetWorldLocation(SpawnLocation);
        Puff->SetWorldRotation(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + FMath::FRandRange(-12.0f, 12.0f), 0.0f));
        Puff->SetWorldScale3D(FVector(
            DownwashPuffStartScales[Index],
            DownwashPuffStartScales[Index],
            DownwashPuffStartScales[Index] * 0.30f));
        Puff->SetVisibility(true, true);
        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("Tint"), SurfaceTint);
            Material->SetScalarParameterValue(TEXT("Phase"), FMath::FRandRange(0.0f, 6.28318f));
            Material->SetScalarParameterValue(TEXT("Opacity"), DownwashPuffBaseOpacities[Index]);
        }
    }

    DownwashAuditTime += DeltaSeconds;
    if (DownwashAuditTime >= 2.0f)
    {
        DownwashAuditTime = 0.0f;
        int32 ActivePuffs = 0;
        for (const float Age : DownwashPuffAges)
        {
            if (Age >= 0.0f) ++ActivePuffs;
        }
        const TCHAR* SurfaceName =
            Surface == EDownwashSurface::Water ? TEXT("WATER") :
            Surface == EDownwashSurface::Sand ? TEXT("SAND") :
            Surface == EDownwashSurface::Concrete ? TEXT("CONCRETE") : TEXT("GRASS");
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_DOWNWASH|ACTIVE|surface=%s|agl_m=%.1f|rotor=%.2f|intensity=%.2f|puffs=%d"),
            SurfaceName,
            GroundClearanceCm * 0.01f,
            RotorFactor,
            Intensity,
            ActivePuffs);
    }
}

void ARotorlineHelicopterPawn::UpdateControllerVibration()
{
    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    if (bPlayerAircraftDying)
    {
        if (bControllerVibrationActive)
        {
            PlayerController->PlayDynamicForceFeedback(
                0.0f,
                0.0f,
                true,
                false,
                false,
                true,
                EDynamicForceFeedbackAction::Stop,
                ControllerVibrationHandle);
            bControllerVibrationActive = false;
        }
        return;
    }

    const float StartupProgress = FMath::Clamp(CurrentRotorPlayRate / FMath::Max(0.01f, RotorFlightPlayRate), 0.0f, 1.0f);
    const float ControlLoad = FMath::Max(FMath::Abs(CollectiveInput), CurrentVelocity.Size2D() / FMath::Max(1.0f, MaxForwardSpeed));
    const float Intensity = bEngineReady
        ? FMath::Clamp(0.035f + ControlLoad * 0.075f, 0.035f, 0.12f)
        : FMath::Lerp(0.015f, 0.085f, StartupProgress);

    if (!bControllerVibrationActive)
    {
        ControllerVibrationHandle = PlayerController->PlayDynamicForceFeedback(
            Intensity,
            -1.0f,
            true,
            false,
            false,
            true,
            EDynamicForceFeedbackAction::Start);
        bControllerVibrationActive = true;
    }
    else
    {
        PlayerController->PlayDynamicForceFeedback(
            Intensity,
            -1.0f,
            true,
            false,
            false,
            true,
            EDynamicForceFeedbackAction::Update,
            ControllerVibrationHandle);
    }
}

float ARotorlineHelicopterPawn::GetAboveGroundMeters() const
{
    FHitResult GroundHit;
    const FVector Start = GetActorLocation();
    const FVector End = Start - FVector(0.0f, 0.0f, 100000.0f);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RotorlineAltitude), false, this);
    if (GetWorld()->LineTraceSingleByChannel(GroundHit, Start, End, ECC_Visibility, QueryParams))
    {
        return FMath::Max(0.0f, (Start.Z - GroundHit.ImpactPoint.Z - CollisionBox->GetScaledBoxExtent().Z) / 100.0f);
    }
    return -1.0f;
}

FVector ARotorlineHelicopterPawn::MissionLocationToWorld(const FVector& BrowserLocation) const
{
    // All browser-authored Ridge Cabin objectives share this coordinate. The
    // original world mapping put it on an unusable cliff; this surveyed plateau
    // is level across the full landing clearing.
    if (FMath::Abs(BrowserLocation.X - 480.0) < 35.0 && FMath::Abs(BrowserLocation.Z + 500.0) < 35.0)
    {
        return FVector(192243.6, 131234.1, 0.0);
    }

    constexpr double IslandHalfWorld = 403200.0;
    // The browser map authored home base at the runway shelf center. UE5 uses
    // the black-circle hero helipad 168 m east of that point as the real home.
    const FVector HomePadOffset(-2338.1, 16636.5, 0.0);
    if (BrowserLocation.X > 1500.0)
    {
        const double CanyonProgress = FMath::Clamp((BrowserLocation.X - 2400.0) / 500.0, 0.0, 1.0);
        const double CanyonCross = FMath::Clamp(BrowserLocation.Z / 1600.0, -1.0, 1.0);
        return FVector(
            FMath::Lerp(50000.0, 215000.0, CanyonProgress),
            FMath::Lerp(-90000.0, 185000.0, (CanyonCross + 1.0) * 0.5),
            0.0);
    }

    return FVector(
        FMath::Clamp((BrowserLocation.X / 1034.5) * IslandHalfWorld, -IslandHalfWorld * 0.9, IslandHalfWorld * 0.9),
        FMath::Clamp((-BrowserLocation.Z / 807.7) * IslandHalfWorld, -IslandHalfWorld * 0.9, IslandHalfWorld * 0.9),
        0.0) + HomePadOffset;
}

FVector ARotorlineHelicopterPawn::ResolveObjectiveWorld(const FRotorlineObjectiveDefinition& Objective) const
{
    const FVector Authored = Objective.bHasWorldLocation
        ? Objective.WorldLocation
        : MissionLocationToWorld(Objective.BrowserLocation);
    if (GetWorld() && Objective.Site.StartsWith(TEXT("ridge-cabin"), ESearchCase::IgnoreCase))
    {
        FVector JeepSpawn(-8130.0f, 213050.0f, 17770.0f);
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (It->ActorHasTag(TEXT("JeepSpawnPoint")))
            {
                JeepSpawn = It->GetActorLocation();
                break;
            }
        }

        AStaticMeshActor* NearestCabin = nullptr;
        float NearestCabinDistanceSq = TNumericLimits<float>::Max();
        for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
        {
            UStaticMeshComponent* MeshComponent = It->GetStaticMeshComponent();
            UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
            const FString SearchName = (It->GetName() + TEXT(" ") +
                (Mesh ? Mesh->GetPathName() : FString())).ToLower();
            if (!SearchName.Contains(TEXT("cabin"))) continue;
            const float DistanceSq = FVector::DistSquared2D(It->GetActorLocation(), JeepSpawn);
            if (DistanceSq < NearestCabinDistanceSq)
            {
                NearestCabinDistanceSq = DistanceSq;
                NearestCabin = *It;
            }
        }
        if (NearestCabin)
        {
            FVector CabinObjective = NearestCabin->GetActorLocation();
            if (Objective.Site.EndsWith(TEXT("-lz"), ESearchCase::IgnoreCase))
            {
                AStaticMeshActor* NearestPad = nullptr;
                float NearestPadDistanceSq = TNumericLimits<float>::Max();
                for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
                {
                    UStaticMeshComponent* MeshComponent = It->GetStaticMeshComponent();
                    UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
                    const FString SearchName = (It->GetName() + TEXT(" ") +
                        (Mesh ? Mesh->GetPathName() : FString())).ToLower();
                    if (!SearchName.Contains(TEXT("helipad")) &&
                        !SearchName.Contains(TEXT("heliport")) &&
                        !SearchName.Contains(TEXT("landing_pad"))) continue;
                    const float DistanceSq = FVector::DistSquared2D(
                        It->GetActorLocation(), NearestCabin->GetActorLocation());
                    if (DistanceSq < NearestPadDistanceSq)
                    {
                        NearestPadDistanceSq = DistanceSq;
                        NearestPad = *It;
                    }
                }
                if (NearestPad && NearestPadDistanceSq <= FMath::Square(15000.0f))
                {
                    FVector PadObjective = NearestPad->GetActorLocation();
                    PadObjective.Z += 80.0f;
                    UE_LOG(LogTemp, Display,
                        TEXT("ROTORLINE_CABIN_RETARGET|site=%s|actor=%s|type=EXISTING_PAD|location=%.0f,%.0f,%.0f"),
                        *Objective.Site, *NearestPad->GetName(),
                        PadObjective.X, PadObjective.Y, PadObjective.Z);
                    return PadObjective;
                }
            }
            CabinObjective.Z += Objective.Kind.Equals(TEXT("land"), ESearchCase::IgnoreCase) ? 80.0f : 160.0f;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_CABIN_RETARGET|site=%s|actor=%s|distance_from_jeep_m=%.1f|location=%.0f,%.0f,%.0f"),
                *Objective.Site, *NearestCabin->GetName(),
                FMath::Sqrt(NearestCabinDistanceSq) / 100.0f,
                CabinObjective.X, CabinObjective.Y, CabinObjective.Z);
            return CabinObjective;
        }
        UE_LOG(LogTemp, Warning,
            TEXT("ROTORLINE_CABIN_RETARGET|site=%s|state=FALLBACK_AUTHORED|reason=NO_LOADED_CABIN_NEAR_JEEP"),
            *Objective.Site);
    }
    const FString SearchText = (Objective.Text + TEXT(" ") + Objective.Target).ToLower();
    const bool bAircraftObjective = SearchText.Contains(TEXT("apache")) || SearchText.Contains(TEXT("hind")) ||
        SearchText.Contains(TEXT("gunship")) || SearchText.Contains(TEXT("md500"));
    const bool bLandingObjective = Objective.Kind == TEXT("land");
    const bool bReconGroundObjective = Objective.Kind.Equals(TEXT("designate-recon"), ESearchCase::IgnoreCase);
    const bool bGroundObjective = (Objective.Kind == TEXT("destroy") || bReconGroundObjective) && !bAircraftObjective;
    const bool bNavigationGroundObjective =
        Objective.Kind == TEXT("reach") ||
        Objective.Kind == TEXT("return") ||
        Objective.Kind == TEXT("interact");
    const bool bTerrainResolvedObjective =
        bLandingObjective || bGroundObjective || bNavigationGroundObjective;
    if (bLandingObjective &&
        (Objective.Site.Equals(TEXT("home"), ESearchCase::IgnoreCase) ||
         Objective.Site.Equals(TEXT("field-hospital"), ESearchCase::IgnoreCase)))
    {
        const FVector PreparedLocation(
            Authored.X,
            Authored.Y,
            Objective.Site.Equals(TEXT("home"), ESearchCase::IgnoreCase) ? 3442.0f : 3200.0f);
        SurveyedLandingSites.Add(Objective.Site, PreparedLocation);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_OBJECTIVE_SURVEY|site=%s|type=LANDING|status=PREPARED_PLATFORM|location=%.0f,%.0f,%.0f"),
            *Objective.Site, PreparedLocation.X, PreparedLocation.Y, PreparedLocation.Z);
        return PreparedLocation;
    }
    const bool bAuthoredPreparedSite = Objective.Site.EndsWith(TEXT("-lz"), ESearchCase::IgnoreCase) ||
        Objective.Site.Contains(TEXT("clearing"), ESearchCase::IgnoreCase) ||
        Objective.Site.Contains(TEXT("emplacement"), ESearchCase::IgnoreCase);
    if ((bLandingObjective || bGroundObjective) && bAuthoredPreparedSite && GetWorld())
    {
        FRotorlineGroundingProfile PreparedProfile = URotorlineGroundingLibrary::MakeProfile(
            ERotorlineGroundingMode::LinearPoint, TEXT("MissionPreparedSite"));
        PreparedProfile.bAllowPreparedGround = true;
        PreparedProfile.bRejectObstructionsAboveGround = true;
        PreparedProfile.bCheckCollisionPenetration = false;
        FRotorlineGroundingResult PreparedResult;
        if (URotorlineGroundingLibrary::SolveGroundContact(
            const_cast<ARotorlineHelicopterPawn*>(this), Authored,
            FVector2D::ZeroVector, const_cast<ARotorlineHelicopterPawn*>(this),
            PreparedProfile, PreparedResult))
        {
            if (PreparedResult.GroundActor && PreparedResult.GroundActor->ActorHasTag(TEXT("RotorlineMissionPad")))
            {
                const FVector PreparedLocation(
                    Authored.X, Authored.Y,
                    PreparedResult.ContactPoint.Z + (bGroundObjective ? 8.0f : 0.0f));
                if (bLandingObjective) SurveyedLandingSites.Add(Objective.Site, PreparedLocation);
                else SurveyedGroundSites.Add(Objective.Site, PreparedLocation);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_OBJECTIVE_SURVEY|site=%s|type=%s|status=PREPARED_CLEARING|location=%.0f,%.0f,%.0f"),
                    *Objective.Site, bLandingObjective ? TEXT("LANDING") : TEXT("GROUND_ENEMY"),
                    PreparedLocation.X, PreparedLocation.Y, PreparedLocation.Z);
                return PreparedLocation;
            }
        }
    }
    if (!Objective.Site.IsEmpty() && !bGroundObjective)
    {
        if (const FVector* Surveyed = SurveyedLandingSites.Find(Objective.Site))
        {
            return *Surveyed;
        }
    }
    if (!Objective.Site.IsEmpty() && bGroundObjective)
    {
        if (const FVector* Surveyed = SurveyedGroundSites.Find(Objective.Site))
        {
            return *Surveyed;
        }
    }

    // Every ground-route POI must resolve against the UE landscape. Browser
    // coordinates commonly arrive with Z=0, which buried Mission 19 markers
    // inside hills and cliffs even though their horizontal route was correct.
    if (!GetWorld() || !Objective.bHasLocation || !bTerrainResolvedObjective)
    {
        return Authored;
    }

    constexpr float MinimumDryObjectiveElevationCm = 800.0f;
    const float MaximumFootprintRoughnessCm = bLandingObjective
        ? 160.0f : (bReconGroundObjective ? 95.0f : (bNavigationGroundObjective ? 300.0f : 220.0f));
    const float MaximumClearingRoughnessCm = bLandingObjective
        ? 650.0f : (bReconGroundObjective ? 400.0f : (bNavigationGroundObjective ? 1000.0f : 900.0f));
    const float ClearanceRadius = bLandingObjective ? 3000.0f : (bNavigationGroundObjective ? 700.0f : 1200.0f);
    const TArray<float> FootprintRadii = bLandingObjective
        ? TArray<float>{ 450.0f, 900.0f, 1350.0f, 1800.0f }
        : (bNavigationGroundObjective
            ? TArray<float>{ 120.0f, 300.0f, 550.0f }
            : TArray<float>{ 180.0f, 350.0f, 500.0f, 700.0f });
    const TArray<float> SearchRadii = {
        0.0f, 5000.0f, 10000.0f, 15000.0f, 20000.0f, 25000.0f,
        30000.0f, 35000.0f, 40000.0f, 45000.0f, 50000.0f, 55000.0f,
        60000.0f, 65000.0f, 70000.0f, 75000.0f, 80000.0f, 85000.0f,
        90000.0f, 95000.0f, 100000.0f, 105000.0f, 110000.0f, 115000.0f,
        120000.0f, 140000.0f, 160000.0f
    };
    FVector BestLocation = Authored;
    float BestScore = TNumericLimits<float>::Max();
    FRotorlineGroundingProfile SurveyProfile = URotorlineGroundingLibrary::MakeProfile(
        ERotorlineGroundingMode::LinearPoint, TEXT("MissionTerrainSurvey"));
    SurveyProfile.bAllowPreparedGround = false;
    SurveyProfile.bRejectObstructionsAboveGround = true;
    SurveyProfile.bCheckCollisionPenetration = false;
    const auto TraceLandscape = [&](const FVector2D& XY, float& OutZ) -> bool
    {
        FRotorlineGroundingResult Result;
        if (!URotorlineGroundingLibrary::SolveGroundContact(
            const_cast<ARotorlineHelicopterPawn*>(this), FVector(XY.X, XY.Y, Authored.Z),
            FVector2D::ZeroVector, const_cast<ARotorlineHelicopterPawn*>(this),
            SurveyProfile, Result)) return false;
        OutZ = Result.ContactPoint.Z;
        return true;
    };

    float BestLocalRoughness = TNumericLimits<float>::Max();
    float BestMinimumHeight = -TNumericLimits<float>::Max();
    bool bFoundSafeCandidate = false;
    for (const float Radius : SearchRadii)
    {
        const int32 DirectionCount = Radius <= 1.0f ? 1 : 32;
        FVector BestAtRadius = Authored;
        float BestAtRadiusScore = TNumericLimits<float>::Max();
        float BestAtRadiusLocalRoughness = TNumericLimits<float>::Max();
        float BestAtRadiusMinimumHeight = -TNumericLimits<float>::Max();
        bool bFoundAtRadius = false;
        for (int32 Direction = 0; Direction < DirectionCount; ++Direction)
        {
            const float Angle = DirectionCount == 1 ? 0.0f : (2.0f * PI * Direction / DirectionCount);
            const FVector2D Candidate(Authored.X + FMath::Cos(Angle) * Radius, Authored.Y + FMath::Sin(Angle) * Radius);
            TArray<float> Heights;
            for (const FVector2D SampleOffset : { FVector2D::ZeroVector, FVector2D(ClearanceRadius, 0.0f), FVector2D(-ClearanceRadius, 0.0f), FVector2D(0.0f, ClearanceRadius), FVector2D(0.0f, -ClearanceRadius) })
            {
                float Height = 0.0f;
                if (TraceLandscape(Candidate + SampleOffset, Height)) Heights.Add(Height);
            }
            if (Heights.Num() != 5) continue;
            TArray<float> LocalHeights;
            LocalHeights.Add(Heights[0]);
            for (const float FootprintRadius : FootprintRadii)
            {
                for (int32 LocalDirection = 0; LocalDirection < 8; ++LocalDirection)
                {
                    const float LocalAngle = 2.0f * PI * LocalDirection / 8.0f;
                    const FVector2D LocalOffset(
                        FMath::Cos(LocalAngle) * FootprintRadius,
                        FMath::Sin(LocalAngle) * FootprintRadius);
                    float LocalHeight = 0.0f;
                    if (TraceLandscape(Candidate + LocalOffset, LocalHeight)) LocalHeights.Add(LocalHeight);
                }
            }
            if (LocalHeights.Num() != 1 + FootprintRadii.Num() * 8) continue;
            float MinimumHeight = Heights[0];
            float MaximumHeight = Heights[0];
            for (const float Height : Heights)
            {
                MinimumHeight = FMath::Min(MinimumHeight, Height);
                MaximumHeight = FMath::Max(MaximumHeight, Height);
            }
            const float Roughness = MaximumHeight - MinimumHeight;
            LocalHeights.Sort();
            const float LocalRoughness = LocalHeights.Last() - LocalHeights[0];
            const float LocalMaximum = LocalHeights.Last();
            const float LocalMedian = LocalHeights[LocalHeights.Num() / 2];
            const float MinimumCandidateHeight = FMath::Min(MinimumHeight, LocalHeights[0]);
            const bool bDry = MinimumCandidateHeight >= MinimumDryObjectiveElevationCm;
            const bool bLevel = LocalRoughness <= MaximumFootprintRoughnessCm &&
                Roughness <= MaximumClearingRoughnessCm;
            if (!bDry || !bLevel) continue;

            const float Score = Roughness * 12.0f + LocalRoughness * 42.0f +
                FMath::Abs(Heights[0] - LocalMedian) * 8.0f + Radius * 0.018f;
            if (Score < BestAtRadiusScore)
            {
                bFoundAtRadius = true;
                BestAtRadiusScore = Score;
                const float MarkerClearanceCm = bNavigationGroundObjective ? 150.0f : (bGroundObjective ? 8.0f : 0.0f);
                BestAtRadius = FVector(Candidate.X, Candidate.Y, LocalMaximum + MarkerClearanceCm);
                BestAtRadiusLocalRoughness = LocalRoughness;
                BestAtRadiusMinimumHeight = MinimumCandidateHeight;
            }
        }
        if (bFoundAtRadius)
        {
            bFoundSafeCandidate = true;
            BestLocation = BestAtRadius;
            BestScore = BestAtRadiusScore;
            BestLocalRoughness = BestAtRadiusLocalRoughness;
            BestMinimumHeight = BestAtRadiusMinimumHeight;
            break;
        }
    }
    if (!bFoundSafeCandidate)
    {
        float AuthoredGroundZ = 0.0f;
        if (TraceLandscape(FVector2D(Authored.X, Authored.Y), AuthoredGroundZ))
        {
            const FVector GroundVisibleFallback(
                Authored.X,
                Authored.Y,
                AuthoredGroundZ + (bNavigationGroundObjective ? 150.0f : (bGroundObjective ? 8.0f : 0.0f)));
            UE_LOG(LogTemp, Warning,
                TEXT("ROTORLINE_OBJECTIVE_SURVEY|site=%s|type=%s|status=GROUND_VISIBLE_FALLBACK|location=%.0f,%.0f,%.0f"),
                *Objective.Site,
                bNavigationGroundObjective ? TEXT("NAVIGATION_POI") :
                    (bLandingObjective ? TEXT("LANDING") : (bReconGroundObjective ? TEXT("RECON_CONTACT") : TEXT("GROUND_ENEMY"))),
                GroundVisibleFallback.X, GroundVisibleFallback.Y, GroundVisibleFallback.Z);
            return GroundVisibleFallback;
        }
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_OBJECTIVE_SURVEY|site=%s|type=%s|status=NO_SAFE_SITE|authored=%.0f,%.0f"),
            *Objective.Site, bNavigationGroundObjective ? TEXT("NAVIGATION_POI") :
                (bLandingObjective ? TEXT("LANDING") : (bReconGroundObjective ? TEXT("RECON_CONTACT") : TEXT("GROUND_ENEMY"))),
            Authored.X, Authored.Y);
        return Authored;
    }
    if (!Objective.Site.IsEmpty())
    {
        if (bLandingObjective) SurveyedLandingSites.Add(Objective.Site, BestLocation);
        else SurveyedGroundSites.Add(Objective.Site, BestLocation);
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_OBJECTIVE_SURVEY|site=%s|type=%s|status=SAFE|shift=%.0f|score=%.0f|min_elevation=%.0f|roughness=%.0f|location=%.0f,%.0f,%.0f"),
        *Objective.Site, bNavigationGroundObjective ? TEXT("NAVIGATION_POI") :
            (bLandingObjective ? TEXT("LANDING") : (bReconGroundObjective ? TEXT("RECON_CONTACT") : TEXT("GROUND_ENEMY"))),
        FVector::Dist2D(Authored, BestLocation), BestScore, BestMinimumHeight,
        BestLocalRoughness, BestLocation.X, BestLocation.Y, BestLocation.Z);
    return BestLocation;
}

bool ARotorlineHelicopterPawn::GetMissionNavigationData(
    FVector& OutWorldLocation,
    FString& OutLabel,
    int32& OutObjectiveIndex,
    int32& OutObjectiveCount) const
{
    OutObjectiveCount = ActiveMission.Objectives.Num();
    OutObjectiveIndex = CurrentObjectiveIndex;
    if (bMissionFailed || bMissionComplete || !ActiveMission.Objectives.IsValidIndex(CurrentObjectiveIndex))
    {
        return false;
    }

    const FRotorlineObjectiveDefinition& Objective = ActiveMission.Objectives[CurrentObjectiveIndex];
    const bool bEscortCabinConvoy =
        Objective.Kind.Equals(TEXT("escort-cabin-convoy"), ESearchCase::IgnoreCase) &&
        IsValid(ActiveCabinSupplyConvoy);
    OutLabel = bEscortCabinConvoy
        ? ActiveCabinSupplyConvoy->GetStatusText()
        : Objective.Text;
    if (bEscortCabinConvoy)
    {
        OutWorldLocation = ActiveCabinSupplyConvoy->GetLeadWorldLocation();
        return !OutWorldLocation.IsNearlyZero();
    }
    if (!Objective.bHasLocation)
    {
        return false;
    }
    if (Objective.Kind.Equals(TEXT("designate-strike"), ESearchCase::IgnoreCase) &&
        IsValid(ActiveKiowaStrikeMission) && !ActiveKiowaStrikeMission->IsTargetRevealed())
    {
        // Search using the mast sensor instead of receiving an exact HUD pin.
        return false;
    }
    OutWorldLocation = (Objective.Kind == TEXT("destroy") ||
        Objective.Kind.Equals(TEXT("designate-strike"), ESearchCase::IgnoreCase) ||
        Objective.Kind.Equals(TEXT("designate-recon"), ESearchCase::IgnoreCase)) && IsValid(ActiveObjectiveActor)
        ? ActiveObjectiveActor->GetAimLocation()
        : (CurrentObjectiveWorldLocation.IsNearlyZero() ? MissionLocationToWorld(Objective.BrowserLocation) : CurrentObjectiveWorldLocation);
    return true;
}

bool ARotorlineHelicopterPawn::GetThreatNavigationData(FVector& OutWorldLocation, FString& OutLabel) const
{
    // Bell missile modes own an authoritative, weapon-class-aware lock.
    // Suppress the shared Apache candidate so the HUD cannot display a second
    // unrelated target beside the actual Bell seeker lock.
    if (IsBell222SpecialOperations())
    {
        return false;
    }

    // Use the exact same sight-driven acquisition result as the rocket. The
    // previous HUD path unconditionally preferred a transit aircraft while
    // the weapon independently chose a nearer ground unit, so the red box and
    // the homing target could disagree (most noticeably on HIMARS batteries).
    float BestDistanceMeters = 0.0f;
    ARotorlineMissionObjectiveActor* BestTarget = FindBestMissileLockTarget(BestDistanceMeters);
    if (!BestTarget)
    {
        return false;
    }

    OutWorldLocation = BestTarget->GetAimLocation();
    OutLabel = BestTarget->GetTargetLabel();
    return true;
}

bool ARotorlineHelicopterPawn::GetKiowaSensorTargetData(
    FVector& OutWorldLocation,
    FString& OutLabel) const
{
    if (!ActiveMission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase) ||
        !IsValid(ActiveKiowaStrikeMission) ||
        !ActiveKiowaStrikeMission->IsSensorMissionActive() ||
        !IsValid(ActiveObjectiveActor))
    {
        return false;
    }

    OutWorldLocation = ActiveObjectiveActor->GetAimLocation();
    OutLabel = ActiveObjectiveActor->GetTargetLabel();
    return true;
}

ARotorlineMissionObjectiveActor* ARotorlineHelicopterPawn::FindBestMissileLockTarget(float& OutDistanceMeters) const
{
    OutDistanceMeters = 0.0f;
    if (!GetWorld())
    {
        MissileLockedTarget.Reset();
        return nullptr;
    }

    FVector SightOrigin = GetActorLocation();
    FVector SightDirection = GetActorForwardVector().GetSafeNormal();
    if (HasAttackCombatPackage())
    {
        FVector SightImpact;
        bool bSightBlocked = false;
        GetApacheWeaponAimSolution(SightOrigin, SightDirection, SightImpact, bSightBlocked);
    }

    constexpr float MaximumLockRangeMeters = 6000.0f;
    constexpr float AcquisitionDot = 0.9702957f; // cos(14 degrees)
    constexpr float TrackingDot = 0.9396926f; // cos(20 degrees)
    const auto EvaluateCandidate = [&](ARotorlineMissionObjectiveActor* Candidate, float RequiredAimDot,
        float& CandidateDistanceMeters, float& AimDot) -> bool
    {
        if (!IsValid(Candidate) || !Candidate->IsDestroyObjective() || Candidate->IsDestroyedTarget() ||
            Candidate->GetHealthFraction() <= 0.0f) return false;

        const FVector ToTarget = Candidate->GetAimLocation() - SightOrigin;
        CandidateDistanceMeters = ToTarget.Size() / 100.0f;
        AimDot = FVector::DotProduct(SightDirection, ToTarget.GetSafeNormal());
        if (CandidateDistanceMeters > MaximumLockRangeMeters || AimDot < RequiredAimDot) return false;

        FHitResult CoverHit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(RotorlineMissileLockLineOfSight), true, this);
        Params.AddIgnoredActor(this);
        const FVector TraceStart = SightOrigin + ToTarget.GetSafeNormal() * 80.0f;
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
            CoverHit, TraceStart, Candidate->GetAimLocation(), ECC_Visibility, Params);
        return !bBlocked || CoverHit.GetActor() == Candidate;
    };

    if (ARotorlineMissionObjectiveActor* CurrentTarget = MissileLockedTarget.Get())
    {
        float CurrentDistanceMeters = 0.0f;
        float CurrentAimDot = -1.0f;
        if (EvaluateCandidate(CurrentTarget, TrackingDot, CurrentDistanceMeters, CurrentAimDot))
        {
            OutDistanceMeters = CurrentDistanceMeters;
            return CurrentTarget;
        }
        MissileLockedTarget.Reset();
    }

    ARotorlineMissionObjectiveActor* BestTarget = nullptr;
    float BestScore = -TNumericLimits<float>::Max();
    for (TActorIterator<ARotorlineMissionObjectiveActor> It(GetWorld()); It; ++It)
    {
        ARotorlineMissionObjectiveActor* Candidate = *It;
        float CandidateDistanceMeters = 0.0f;
        float AimDot = -1.0f;
        if (!EvaluateCandidate(Candidate, AcquisitionDot, CandidateDistanceMeters, AimDot)) continue;

        // Angular alignment dominates acquisition, with a small distance bias
        // to keep two nearly overlapping contacts stable. A launcher under the
        // reticle therefore cannot be stolen by a closer off-axis emplacement.
        const float Score = AimDot * 12000.0f - CandidateDistanceMeters * 0.08f;
        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Candidate;
            OutDistanceMeters = CandidateDistanceMeters;
        }
    }
    MissileLockedTarget = BestTarget;
    return BestTarget;
}

bool ARotorlineHelicopterPawn::GetRadioChatter(FString& OutMessage) const
{
    if (!GetWorld() || ActiveRadioMessage.IsEmpty() || GetWorld()->GetTimeSeconds() > RadioMessageUntil)
    {
        return false;
    }
    OutMessage = ActiveRadioMessage;
    return true;
}

bool ARotorlineHelicopterPawn::IsSpokenDialogueActive() const
{
    const bool bKiowaSequenceDialogue =
        IsValid(ActiveKiowaStrikeMission) &&
        ActiveKiowaStrikeMission->IsDialogueAudioPlaying();
    return (RadioAudio && RadioAudio->IsPlaying()) ||
        (InstructorAudio && InstructorAudio->IsPlaying()) ||
        (MissionBriefAudio && MissionBriefAudio->IsPlaying()) ||
        bKiowaSequenceDialogue;
}

float ARotorlineHelicopterPawn::GetRadioChatterFadeAlpha() const
{
    if (!GetWorld() || ActiveRadioMessage.IsEmpty())
    {
        return 0.0f;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    const float Age = static_cast<float>(Now - LastRadioChatterTime);
    const float Remaining = static_cast<float>(RadioMessageUntil - Now);
    if (Remaining <= 0.0f)
    {
        return 0.0f;
    }

    const float FadeIn = FMath::Clamp(Age / 0.15f, 0.0f, 1.0f);
    const float FadeOut = FMath::Clamp(Remaining / 0.60f, 0.0f, 1.0f);
    return FMath::Min(FadeIn, FadeOut);
}

FRotorlineCockpitHUDState ARotorlineHelicopterPawn::GetCockpitHUDState() const
{
    FRotorlineCockpitHUDState State;
    State.AircraftName = GetCraftDisplayName().ToUpper();
    State.MissionCallsign = ActiveMission.Callsign.ToUpper();
    State.SpeedKph = CurrentVelocity.Size2D() * 0.036f;
    State.AltitudeAglMeters = GetAboveGroundMeters();
    State.DescentRateMps = FMath::Max(0.0f, -CurrentVelocity.Z / 100.0f);
    State.SafeSkidSpeedKph = 45.0f;
    State.SafeDescentRateMps = 4.0f;
    State.HeadingDegrees = FRotator::ClampAxis(GetActorRotation().Yaw);
    State.HullPercent = MaxHealth > KINDA_SMALL_NUMBER
        ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) * 100.0f
        : 0.0f;
    State.FuelPercent = FuelRemainingPercent;
    State.RotorPercent = FMath::Clamp(CurrentRotorPlayRate / FMath::Max(RotorFlightPlayRate, 0.01f), 0.0f, 1.0f) * 100.0f;
    State.EngineReadySeconds = EngineReadyTime >= 0.0 && GetWorld()
        ? FMath::Max(0.0, EngineReadyTime - GetWorld()->GetTimeSeconds())
        : FMath::Max(0.0f, EngineSpoolDuration - EngineStartupElapsed);
    State.RocketAmmo = RocketAmmo;
    State.RocketCapacity = RocketAmmoCapacity;
    State.CannonAmmo = ApacheCannonAmmo;
    State.CannonCapacity = ApacheCannonAmmoCapacity;
    State.CannonHeatPercent = FMath::Clamp(ApacheCannonHeat, 0.0f, 100.0f);
    State.Countermeasures = CountermeasureCharges;
    State.CountermeasureCapacity = CountermeasureCapacity;
    State.CountermeasureCooldown = GetCountermeasureCooldownRemaining();
    State.bEngineReady = bEngineReady;
    State.bMissionBriefActive = bMissionBriefActive;
    State.bArmed = bSelectedAircraftArmed;
    State.bMissileLockMode = bApacheMissileLockMode;
    State.bCombatZoom = bApacheCombatZoomEnabled;
    State.bCannonOverheated = bApacheCannonOverheated;
    State.bPlayerAircraftDying = bPlayerAircraftDying;
    State.bBellWeaponSystem = IsBell222SpecialOperations();
    State.bStealthActive = bStealthActive;
    if (GetWorld())
    {
        const double Now = GetWorld()->GetTimeSeconds();
        State.StealthSecondsRemaining = bStealthActive
            ? FMath::Max(0.0, Bell222StealthExpiresAt - Now) : 0.0;
        State.StealthCooldownSecondsRemaining = !bStealthActive
            ? FMath::Max(0.0, Bell222StealthCooldownUntil - Now) : 0.0;
    }
    if (IsValid(ActiveKiowaStrikeMission))
    {
        State.bReconStrikeSensor = true;
        State.ReconDesignationProgress = ActiveKiowaStrikeMission->GetDesignationProgress();
        State.ReconSensorStatus = ActiveKiowaStrikeMission->GetSensorStatus();
        State.AlliedStrikeStatus = ActiveKiowaStrikeMission->GetAlliedStrikeStatus();
    }
    if (State.bBellWeaponSystem)
    {
        const FRotorlineAircraftWeaponModeDefinition* Mode = GetBell222WeaponDefinition();
        State.SelectedWeapon = Mode ? Mode->DisplayName.ToUpper() : TEXT("WEAPONS SAFE");
        State.WeaponSystemState = GetBell222WeaponSystemState();
        State.SelectedWeaponAmmo = GetBell222WeaponAmmo();
        State.SelectedWeaponCapacity = GetBell222WeaponCapacity();
        State.WeaponLockProgress = Bell222WeaponLockProgress;
        if (Bell222LockedTarget.IsValid())
        {
            State.WeaponTarget = Bell222LockedTarget->GetTargetLabel().ToUpper();
            State.WeaponLockState = Bell222WeaponLockProgress >= 0.999f
                ? TEXT("LOCKED")
                : FString::Printf(TEXT("ACQUIRING %.0f%%"), Bell222WeaponLockProgress * 100.0f);
        }
        else
        {
            State.WeaponTarget = TEXT("NO VALID TARGET");
            State.WeaponLockState = IsBell222MissileMode() ? TEXT("NO LOCK") : TEXT("DIRECT FIRE");
        }
        State.CannonHeatPercent = Bell222WeaponMode == ERotorlineBellWeaponMode::Linked ? Bell222LinkedHeat : 0.0f;
        State.bMissileLockMode = IsBell222MissileMode();
    }
    State.StartupPhase = !EnginePreIgnitionSound
        ? TEXT("ENGINE START")
        : (!bRotorSpoolStageActive ? TEXT("PRE-IGNITION") : TEXT("ROTOR SPOOL-UP"));
    return State;
}

FRotorlineAwardsFlightState ARotorlineHelicopterPawn::GetAwardsFlightState() const
{
    FRotorlineAwardsFlightState State;
    State.Velocity = CurrentVelocity;
    State.AltitudeAglMeters = GetAboveGroundMeters();
    State.PitchDegrees = CurrentPitchAngle;
    State.RollDegrees = CurrentRollAngle;
    State.Health = CurrentHealth;
    State.MaxHealth = MaxHealth;
    State.FuelRemainingPercent = FuelRemainingPercent;
    State.bEnginePowerAvailable = bEngineReady && !bFuelStarved;
    State.bMissionFailed = bMissionFailed;
    State.bAircraftDying = bPlayerAircraftDying;
    return State;
}

FVector ARotorlineHelicopterPawn::GetMissionWorldLocation(const FVector& BrowserLocation) const
{
    return MissionLocationToWorld(BrowserLocation);
}

FVector ARotorlineHelicopterPawn::GetMissionWorldLocation(const FRotorlineObjectiveDefinition& Objective) const
{
    return Objective.bHasWorldLocation
        ? Objective.WorldLocation
        : MissionLocationToWorld(Objective.BrowserLocation);
}

bool ARotorlineHelicopterPawn::GetBaseRearmStatus(FString& OutStatus, bool& bOutServicing) const
{
    bOutServicing = bBaseRearmActive;
    bool bBellAmmoFull = true;
    for (const TPair<FString, int32>& Store : Bell222WeaponCapacity)
    {
        bBellAmmoFull &= Bell222WeaponAmmo.FindRef(Store.Key) >= Store.Value;
    }
    const bool bAmmoFull = bBellAmmoFull && RocketAmmo >= RocketAmmoCapacity &&
        ApacheCannonAmmo >= ApacheCannonAmmoCapacity &&
        CountermeasureCharges >= CountermeasureCapacity;
    const bool bHealthFull = CurrentHealth >= MaxHealth - KINDA_SMALL_NUMBER;
    const bool bFuelFull = FuelRemainingPercent >= RotorlineHelicopter::ServiceFuelThresholdPercent;
    if (bAmmoFull && bHealthFull && bFuelFull)
    {
        return false;
    }

    if (bBaseRearmActive)
    {
        OutStatus = FString::Printf(
            TEXT("%s SERVICING // HOLD POSITION // %.1f SEC"),
            bInsideCityServiceZone ? TEXT("CITY PAD") : TEXT("BASE"),
            FMath::Max(0.0f, RotorlineHelicopter::BaseRearmDurationSeconds - BaseRearmProgress));
    }
    else if (bInsideBaseServiceZone)
    {
        OutStatus = TEXT("BASE SERVICE // LAND AND STOP TO REARM / REPAIR / REFUEL");
    }
    else if (bInsideCityServiceZone)
    {
        OutStatus = TEXT("CITY SERVICE PAD // LAND AND STOP TO REARM / REPAIR / REFUEL");
    }
    else
    {
        OutStatus = TEXT("SERVICE REQUIRED // LAND AT BASE OR CITY REARM PAD");
    }
    return true;
}

void ARotorlineHelicopterPawn::BroadcastRadio(const FString& Message, float Duration, bool bPlaySquelch)
{
    if (!GetWorld() || Message.IsEmpty()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    TObjectPtr<USoundBase>* CalloutSound = RadioCalloutSounds.Find(Message);
    USoundBase* RoutedCalloutSound = CalloutSound ? CalloutSound->Get() : nullptr;
    const FString RoutedCalloutAsset = RoutedCalloutSound
        ? RoutedCalloutSound->GetName()
        : FString();
    if (RoutedCalloutAsset.Equals(TEXT("RADIO_023_COMMAND_CabinFlareOut"), ESearchCase::IgnoreCase))
    {
        const bool bCabinObjectiveActive =
            ActiveMission.Objectives.IsValidIndex(CurrentObjectiveIndex) &&
            ActiveMission.Objectives[CurrentObjectiveIndex].Site.Equals(TEXT("ridge-cabin-lz"), ESearchCase::IgnoreCase);
        if (!bCabinObjectiveActive)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_RADIO_DISCIPLINE|category=CABIN_ONLY|state=SUPPRESSED|mission=%s|objective=%d"),
                *ActiveMission.Id, CurrentObjectiveIndex);
            return;
        }
    }
    if (!RoutedCalloutAsset.IsEmpty() &&
        PlayedRadioCalloutAssetsThisMission.Contains(RoutedCalloutAsset))
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_RADIO_DISCIPLINE|category=RECORDED|state=SUPPRESSED|reason=CALLOUT_ALREADY_PLAYED|asset=%s|message=%s"),
            *RoutedCalloutAsset,
            *Message);
        return;
    }
    const FString TacticalCategory = RotorlineHelicopter::GetTacticalRadioCategory(Message);
    const bool bTacticalMessage = !TacticalCategory.IsEmpty();
    const bool bImmediateThreatWarning =
        TacticalCategory == TEXT("MISSILE_WARNING") ||
        TacticalCategory == TEXT("GUNFIRE_WARNING") ||
        TacticalCategory == TEXT("RADAR_LOCK");
    const bool bAircraftArrivalAnnouncement =
        Message.Contains(TEXT("Hind inbound"), ESearchCase::IgnoreCase) ||
        Message.Contains(TEXT("Apache inbound"), ESearchCase::IgnoreCase) ||
        Message.Contains(TEXT("armed scout closing"), ESearchCase::IgnoreCase) ||
        Message.Contains(TEXT("hostile light helicopter closing"), ESearchCase::IgnoreCase);
    const bool bEphemeralCombatCallout =
        bImmediateThreatWarning || bAircraftArrivalAnnouncement;
    const bool bAmbientCrewCallout =
        Message.StartsWith(TEXT("CREW:"), ESearchCase::IgnoreCase) &&
        !bTacticalMessage;

    // Command owns radar-status callouts. Gunner may announce weapon fire, but
    // never duplicates the radar warning with a second voice.
    if (TacticalCategory == TEXT("RADAR_LOCK") &&
        Message.StartsWith(TEXT("GUNNER:"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_RADIO_DISCIPLINE|category=RADAR_LOCK|state=SUPPRESSED|reason=COMMAND_ONLY|message=%s"),
            *Message);
        return;
    }
    if (IsMissionRadioHoldActive())
    {
        // Treat a held callout as handled for chatter pacing. Without this,
        // the ambient chatter loop retries the same line every frame while
        // the authored mission sequence owns the radio channel.
        LastRadioChatterTime = Now;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSION_RADIO|mission=%s|state=SUPPRESSED|remaining_seconds=%.1f|message=%s"),
            *ActiveMission.Id,
            FMath::Max(0.0, MissionRadioHoldUntil - GetWorld()->GetTimeSeconds()),
            *Message);
        return;
    }
    if (bAmbientCrewCallout && CountActiveEnemyHelicopters() > 0)
    {
        LastRadioChatterTime = Now;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_RADIO_DISCIPLINE|category=CREW|state=SUPPRESSED|reason=HOSTILE_AIRCRAFT_ACTIVE|message=%s"),
            *Message);
        return;
    }
    if (const double* LastMessageTime = LastRadioMessageTimes.Find(Message))
    {
        if (Now - *LastMessageTime < RotorlineHelicopter::RepeatedRadioMessageCooldownSeconds)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_RADIO_DISCIPLINE|category=%s|state=SUPPRESSED|reason=MESSAGE_COOLDOWN|remaining_seconds=%.1f|message=%s"),
                bTacticalMessage ? *TacticalCategory : TEXT("GENERAL"),
                RotorlineHelicopter::RepeatedRadioMessageCooldownSeconds - (Now - *LastMessageTime),
                *Message);
            return;
        }
    }
    if (bTacticalMessage)
    {
        if (const double* LastCategoryTime = LastTacticalRadioCategoryTimes.Find(TacticalCategory))
        {
            const double Cooldown = RotorlineHelicopter::GetTacticalRadioCooldown(TacticalCategory);
            if (Now - *LastCategoryTime < Cooldown)
            {
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_RADIO_DISCIPLINE|category=%s|state=SUPPRESSED|reason=CATEGORY_COOLDOWN|message=%s"),
                    *TacticalCategory,
                    *Message);
                return;
            }
        }

    }
    if (bImmediateThreatWarning && RadioAudio && RadioAudio->IsPlaying())
    {
        const FString ActiveCategory =
            RotorlineHelicopter::GetTacticalRadioCategory(ActiveRadioMessage);
        const bool bActiveThreatWarning =
            ActiveCategory == TEXT("MISSILE_WARNING") ||
            ActiveCategory == TEXT("GUNFIRE_WARNING") ||
            ActiveCategory == TEXT("RADAR_LOCK");
        if (bActiveThreatWarning)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_RADIO_DISCIPLINE|category=%s|state=SUPPRESSED|reason=THREAT_WARNING_ALREADY_ACTIVE|active=%s|message=%s"),
                *TacticalCategory,
                *ActiveRadioMessage,
                *Message);
            return;
        }

        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_RADIO_DISCIPLINE|category=%s|state=PREEMPTING|reason=IMMEDIATE_THREAT|interrupted=%s|message=%s"),
            *TacticalCategory,
            *ActiveRadioMessage,
            *Message);
        RadioAudio->Stop();
        ActiveRadioMessage.Empty();
        RadioMessageUntil = Now;
        NextRadioPlaybackAllowedTime = Now;
    }
    const bool bSpeechBusy = IsSpokenDialogueActive() ||
        (RadioAudio && RadioAudio->IsPlaying()) ||
        (!bImmediateThreatWarning &&
            (Now < RadioMessageUntil || Now < NextRadioPlaybackAllowedTime));
    if (bSpeechBusy)
    {
        if (bEphemeralCombatCallout)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_RADIO_DISCIPLINE|category=%s|state=SUPPRESSED|reason=EPHEMERAL_EVENT_CHANNEL_BUSY|active=%s|message=%s"),
                bTacticalMessage ? *TacticalCategory : TEXT("AIRCRAFT_ARRIVAL"),
                *ActiveRadioMessage,
                *Message);
            return;
        }
        if (!QueuedRadioMessages.Contains(Message))
        {
            constexpr int32 MaximumQueuedRadioMessages = 12;
            if (QueuedRadioMessages.Num() >= MaximumQueuedRadioMessages)
            {
                const int32 RemovalIndex = bTacticalMessage
                    ? QueuedRadioMessages.Num() - 1
                    : 0;
                QueuedRadioMessages.RemoveAt(RemovalIndex);
                QueuedRadioDurations.RemoveAt(RemovalIndex);
            }
            if (bTacticalMessage)
            {
                QueuedRadioMessages.Insert(Message, 0);
                QueuedRadioDurations.Insert(Duration, 0);
            }
            else
            {
                QueuedRadioMessages.Add(Message);
                QueuedRadioDurations.Add(Duration);
            }
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_RADIO_DISCIPLINE|category=%s|state=QUEUED|priority=%s|active=%s|queued=%d|message=%s"),
            bTacticalMessage ? *TacticalCategory : TEXT("GENERAL"),
            bTacticalMessage ? TEXT("TACTICAL_FRONT") : TEXT("GENERAL_BACK"),
            *ActiveRadioMessage,
            QueuedRadioMessages.Num(),
            *Message);
        return;
    }
    // The synthetic squelch reads as a piercing whistle when repeated before
    // every callout. Keep the parameter for existing call sites, but suppress
    // the cue globally and leave the routed spoken radio audio untouched.
    (void)bPlaySquelch;
    const float PlaybackDuration = RoutedCalloutSound
        ? FMath::Max(Duration, RoutedCalloutSound->GetDuration() + 0.35f)
        : Duration;
    ActiveRadioMessage = Message;
    RadioMessageUntil = Now + PlaybackDuration;
    NextRadioPlaybackAllowedTime =
        RadioMessageUntil + RotorlineHelicopter::MinimumRadioCalloutSeparationSeconds;
    LastRadioChatterTime = Now;
    LastRadioMessageTimes.Add(Message, Now);
    if (bTacticalMessage)
    {
        LastTacticalRadioCategoryTimes.Add(TacticalCategory, Now);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_RADIO_DISCIPLINE|category=%s|state=PLAYING|speaker=%s"),
            *TacticalCategory,
            Message.StartsWith(TEXT("COMMAND:"), ESearchCase::IgnoreCase) ? TEXT("COMMAND") :
            (Message.StartsWith(TEXT("GUNNER:"), ESearchCase::IgnoreCase) ? TEXT("GUNNER") : TEXT("OTHER")));
    }
    if (RadioSquelchAudio && RadioSquelchAudio->IsPlaying())
    {
        RadioSquelchAudio->Stop();
    }
    UE_LOG(LogTemp, Verbose, TEXT("ROTORLINE_RADIO_SQUELCH|state=DISABLED"));
    if (RoutedCalloutSound)
    {
        PlayedRadioCalloutAssetsThisMission.Add(RoutedCalloutAsset);
        RadioAudio->SetVolumeMultiplier(0.78f * GetAudioMix(ERotorlineAudioChannel::Radio));
        RadioAudio->SetSound(RoutedCalloutSound);
        RadioAudio->Play();
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_RADIO_AUDIO|message=%s|asset=%s|state=PLAYING|duration=%.2f"),
            *Message, *RoutedCalloutSound->GetName(), PlaybackDuration);
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_RADIO|%s"), *Message);
}

void ARotorlineHelicopterPawn::UpdateQueuedRadio()
{
    if (!GetWorld() || IsSpokenDialogueActive() || (RadioAudio && RadioAudio->IsPlaying()) ||
        GetWorld()->GetTimeSeconds() < RadioMessageUntil ||
        GetWorld()->GetTimeSeconds() < NextRadioPlaybackAllowedTime ||
        QueuedRadioMessages.IsEmpty() || QueuedRadioDurations.IsEmpty())
    {
        return;
    }

    ActiveRadioMessage.Empty();
    const FString NextMessage = QueuedRadioMessages[0];
    const float NextDuration = QueuedRadioDurations[0];
    QueuedRadioMessages.RemoveAt(0);
    QueuedRadioDurations.RemoveAt(0);
    BroadcastRadio(NextMessage, NextDuration, false);
}

void ARotorlineHelicopterPawn::LoadRadioCallouts()
{
    RadioCalloutSounds.Reset();
    for (const RotorlineHelicopter::FRadioCalloutDefinition& Definition : RotorlineHelicopter::RadioCalloutDefinitions)
    {
        const FString AssetPath = FString::Printf(
            TEXT("/Game/Audio/Radio/Callouts/%s.%s"),
            Definition.AssetName,
            Definition.AssetName);
        if (USoundBase* CalloutSound = LoadObject<USoundBase>(nullptr, *AssetPath))
        {
            RadioCalloutSounds.Add(Definition.Message, CalloutSound);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ROTORLINE_RADIO_AUDIO|message=%s|asset=%s|state=MISSING"), Definition.Message, *AssetPath);
        }
    }
    if (RadioCalloutSounds.Num() == UE_ARRAY_COUNT(RotorlineHelicopter::RadioCalloutDefinitions))
    {
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_RADIO_AUDIO|loaded=%d|expected=%d"), RadioCalloutSounds.Num(), UE_ARRAY_COUNT(RotorlineHelicopter::RadioCalloutDefinitions));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_RADIO_AUDIO|loaded=%d|expected=%d"), RadioCalloutSounds.Num(), UE_ARRAY_COUNT(RotorlineHelicopter::RadioCalloutDefinitions));
    }
}

void ARotorlineHelicopterPawn::PlayThreatAlert(USoundBase* AlertSound, float BaseVolume)
{
    if (!ThreatAlertAudio || !AlertSound)
    {
        return;
    }

    const float EngineStartupBefore = EngineStartupAudio->VolumeMultiplier;
    const float EngineTakeoffBefore = EngineTakeoffAudio->VolumeMultiplier;
    const float EngineFlightBefore = EngineFlightAudio->VolumeMultiplier;
    const float MissionBriefBefore = MissionBriefAudio->VolumeMultiplier;
    const float MusicBefore = MissionMusicAudio->VolumeMultiplier;
    const float MasterMixBefore = GetAudioMix(ERotorlineAudioChannel::Master);
    const float EnvironmentMixBefore = GetAudioMix(ERotorlineAudioChannel::Environment);
    const float EngineMixBefore = GetAudioMix(ERotorlineAudioChannel::Engine);
    const float MusicMixBefore = GetAudioMix(ERotorlineAudioChannel::Music);
    const float WeaponsMixBefore = GetAudioMix(ERotorlineAudioChannel::WeaponsExplosions);

    // Warnings own their dedicated component and may duck beneath speech, but
    // they must never stop the shared spoken-radio component mid-sentence.
    ThreatAlertBaseVolume = FMath::Clamp(BaseVolume, 0.0f, 1.0f);
    const float WarningDialogueDuck = IsSpokenDialogueActive() ? 0.42f : 1.0f;
    ThreatAlertAudio->SetVolumeMultiplier(
        ThreatAlertBaseVolume * GetAudioMix(ERotorlineAudioChannel::Radio) * WarningDialogueDuck);
    const bool bSameAlertAlreadyPlaying =
        ThreatAlertAudio->IsPlaying() && ThreatAlertAudio->Sound == AlertSound;
    if (!bSameAlertAlreadyPlaying)
    {
        ThreatAlertAudio->Stop();
        ThreatAlertAudio->SetSound(AlertSound);
        ThreatAlertAudio->Play();
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_WARNING_PRIORITY|speech=%s|warning=%s|action=%s"),
        IsSpokenDialogueActive() ? TEXT("PROTECTED") : TEXT("IDLE"),
        *AlertSound->GetName(),
        bSameAlertAlreadyPlaying ? TEXT("COALESCED") : TEXT("PLAYED"));

    const bool bComponentsStable =
        FMath::IsNearlyEqual(EngineStartupBefore, EngineStartupAudio->VolumeMultiplier) &&
        FMath::IsNearlyEqual(EngineTakeoffBefore, EngineTakeoffAudio->VolumeMultiplier) &&
        FMath::IsNearlyEqual(EngineFlightBefore, EngineFlightAudio->VolumeMultiplier) &&
        FMath::IsNearlyEqual(MissionBriefBefore, MissionBriefAudio->VolumeMultiplier) &&
        FMath::IsNearlyEqual(MusicBefore, MissionMusicAudio->VolumeMultiplier);
    const bool bSettingsStable =
        FMath::IsNearlyEqual(MasterMixBefore, GetAudioMix(ERotorlineAudioChannel::Master)) &&
        FMath::IsNearlyEqual(EnvironmentMixBefore, GetAudioMix(ERotorlineAudioChannel::Environment)) &&
        FMath::IsNearlyEqual(EngineMixBefore, GetAudioMix(ERotorlineAudioChannel::Engine)) &&
        FMath::IsNearlyEqual(MusicMixBefore, GetAudioMix(ERotorlineAudioChannel::Music)) &&
        FMath::IsNearlyEqual(WeaponsMixBefore, GetAudioMix(ERotorlineAudioChannel::WeaponsExplosions));
    bLastAlertIsolationPassed = bComponentsStable && bSettingsStable;
    UE_CLOG(!bLastAlertIsolationPassed, LogTemp, Error,
        TEXT("ROTORLINE_ALERT_ISOLATION|status=FAIL|components=%d|settings=%d"),
        bComponentsStable ? 1 : 0, bSettingsStable ? 1 : 0);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_ALERT_ISOLATION|status=%s|components=%s|settings=%s|master=%.3f|environment=%.3f|engine=%.3f|music=%.3f|weapons=%.3f"),
        bLastAlertIsolationPassed ? TEXT("PASS") : TEXT("FAIL"),
        bComponentsStable ? TEXT("STABLE") : TEXT("CHANGED"),
        bSettingsStable ? TEXT("STABLE") : TEXT("CHANGED"),
        MasterMixBefore, EnvironmentMixBefore, EngineMixBefore, MusicMixBefore, WeaponsMixBefore);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_ALERT_AUDIO|base=%.2f|radio_mix=%.2f|stacking=BLOCKED"),
        ThreatAlertBaseVolume,
        GetAudioMix(ERotorlineAudioChannel::Radio));
}

bool ARotorlineHelicopterPawn::RunAlertIsolationAudit()
{
    if (!ThreatWarningSound)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_ALERT_ISOLATION|status=FAIL|reason=MISSING_WARNING_SOUND"));
        return false;
    }
    PlayThreatAlert(ThreatWarningSound, 0.38f);
    ThreatAlertAudio->Stop();
    return bLastAlertIsolationPassed;
}

void ARotorlineHelicopterPawn::UpdateMissionMusic()
{
    // Skyborne Assault is the Bell 222's identity track. It begins during the
    // startup sequence and remains authoritative for the full sortie instead
    // of being replaced when mission/objective types change.
    const bool bKiowaReconStrike = ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase);
    const bool bBellSpecialMusic = IsBell222SpecialOperations() && Bell222MissionMusicSound;
    USoundBase* DesiredMusic = bKiowaReconStrike && KiowaReconMissionMusicSound
        ? KiowaReconMissionMusicSound.Get()
        : (bBellSpecialMusic
            ? Bell222MissionMusicSound.Get()
            : (RotorlineGameplayMusic.IsEmpty()
                ? MusicMission1Sound.Get()
                : RotorlineGameplayMusic[GetTypeHash(ActiveMission.Id) % RotorlineGameplayMusic.Num()].Get()));

    if (!DesiredMusic || DesiredMusic == CurrentMissionMusic) return;
    MissionMusicAudio->Stop();
    CurrentMissionMusic = DesiredMusic;
    MissionMusicAudio->SetSound(CurrentMissionMusic);
    MissionMusicAudio->FadeIn(2.8f, MissionMusicVolume * GetAudioMix(ERotorlineAudioChannel::Music), 0.0f);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_MUSIC|track=%s|mission=%s|objective=%d|aircraft=%s|policy=%s"),
        *CurrentMissionMusic->GetName(), *ActiveMission.Id, CurrentObjectiveIndex + 1,
        *SelectedAircraftId,
        bKiowaReconStrike ? TEXT("KIOWA_RECON_STRIKE_SPECIAL_PRESERVED") :
            (bBellSpecialMusic ? TEXT("BELL_FULL_SORTIE_SPECIAL_PRESERVED") : TEXT("GENERAL_ROTATION")));
}

int32 ARotorlineHelicopterPawn::CountActiveEnemyHelicopters() const
{
    int32 ActiveCount = 0;
    for (const FEnemyHelicopterEncounter& Encounter : ActiveEnemyHelicopterEncounters)
    {
        const ARotorlineMissionObjectiveActor* Actor = Encounter.Actor.Get();
        if (IsValid(Actor) && Actor->IsAircraftCombatActive())
        {
            ++ActiveCount;
        }
    }
    return ActiveCount;
}

bool ARotorlineHelicopterPawn::IsMissionAirObjective(const FRotorlineObjectiveDefinition& Objective) const
{
    if (!Objective.Kind.Equals(TEXT("destroy"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    const FString Identity = (Objective.Target + TEXT(" ") + Objective.Text).ToLower();
    return Identity.Contains(TEXT("apache")) ||
        Identity.Contains(TEXT("hind")) ||
        Identity.Contains(TEXT("gunship")) ||
        Identity.Contains(TEXT("helicopter")) ||
        Identity.Contains(TEXT("md500")) ||
        Identity.Contains(TEXT("md-500"));
}

void ARotorlineHelicopterPawn::LogEnemyHelicopterSpawnBlocked(
    const TCHAR* Source,
    const TCHAR* Reason,
    double CooldownRemaining)
{
    if (!GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const FString ReasonText(Reason);
    // A deferred mission objective retries from Tick. Keep the proof useful
    // without writing the same blocked marker every frame.
    if (LastEnemyHelicopterBlockReason == ReasonText && Now - LastEnemyHelicopterBlockLogTime < 5.0)
    {
        return;
    }
    LastEnemyHelicopterBlockReason = ReasonText;
    LastEnemyHelicopterBlockLogTime = Now;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_AIR_ENCOUNTER|BLOCKED|source=%s|reason=%s|generation=%llu|world_time=%.1f|active=%d|limit=%d|timer_count=0|cooldown_until=%.1f|cooldown_remaining=%.1f"),
        Source,
        Reason,
        EnemyHelicopterEncounterGeneration,
        Now,
        CountActiveEnemyHelicopters(),
        FMath::Max(0, ActiveMission.MaxConcurrentEnemyHelicopters),
        EnemyHelicopterCooldownUntil,
        FMath::Max(0.0, CooldownRemaining));
}

void ARotorlineHelicopterPawn::StartEnemyHelicopterCooldown(const TCHAR* Reason)
{
    if (!GetWorld() || bMissionFailed || bMissionComplete || bEnemyHelicopterCooldownActive)
    {
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    const double CooldownSeconds =
        ActiveMission.Id.Equals(TEXT("search"), ESearchCase::IgnoreCase)
            ? 22.0
            : RotorlineHelicopter::MinimumEnemyHelicopterCooldownSeconds;
    bEnemyHelicopterCooldownActive = true;
    EnemyHelicopterCooldownUntil = Now + CooldownSeconds;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_AIR_ENCOUNTER|COOLDOWN_STARTED|reason=%s|generation=%llu|world_time=%.1f|active=0|limit=%d|timer_count=0|cooldown_until=%.1f|cooldown_remaining=%.1f"),
        Reason,
        EnemyHelicopterEncounterGeneration,
        Now,
        FMath::Max(0, ActiveMission.MaxConcurrentEnemyHelicopters),
        EnemyHelicopterCooldownUntil,
        CooldownSeconds);
}

bool ARotorlineHelicopterPawn::CanSpawnEnemyHelicopterEncounter(const TCHAR* Source)
{
    if (!GetWorld()) return false;
    // Dedicated vehicle/combat qualification owns its explicitly spawned
    // targets and must remain deterministic. The gate's own qualification is
    // intentionally not bypassed.
    if (!bEnemyHelicopterEncounterGateTestMode &&
        (bEnemyFlightTestMode || bCombatLoopTestMode || bCombatPreviewMode))
    {
        return true;
    }
    if (ActiveMission.Id.Equals(TEXT("tutorial"), ESearchCase::IgnoreCase) &&
        !bEnemyHelicopterEncounterGateTestMode &&
        !bTutorialHelicopterKillTestMode)
    {
        LogEnemyHelicopterSpawnBlocked(Source, TEXT("TUTORIAL_NO_COMBAT"), 0.0);
        return false;
    }

    const int32 ActiveLimit = FMath::Max(0, ActiveMission.MaxConcurrentEnemyHelicopters);
    if (ActiveLimit == 0)
    {
        LogEnemyHelicopterSpawnBlocked(Source, TEXT("MISSION_AIR_DISABLED"), 0.0);
        return false;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    if (!bEnemyFlightTestMode &&
        Now - MissionStartTime < RotorlineHelicopter::OpeningCombatGraceSeconds)
    {
        LogEnemyHelicopterSpawnBlocked(
            Source,
            TEXT("OPENING_SAFETY"),
            RotorlineHelicopter::OpeningCombatGraceSeconds - (Now - MissionStartTime));
        return false;
    }
    if (bMissionComplete || bMissionFailed)
    {
        LogEnemyHelicopterSpawnBlocked(
            Source,
            bMissionComplete ? TEXT("MISSION_COMPLETE") : TEXT("MISSION_FAILED"),
            0.0);
        return false;
    }

    const int32 ActiveCount = CountActiveEnemyHelicopters();
    if (ActiveCount >= ActiveLimit)
    {
        LogEnemyHelicopterSpawnBlocked(Source, TEXT("ACTIVE"), 0.0);
        return false;
    }

    if (bEnemyHelicopterCooldownActive)
    {
        const double CooldownRemaining = EnemyHelicopterCooldownUntil - Now;
        if (CooldownRemaining > 0.0)
        {
            LogEnemyHelicopterSpawnBlocked(Source, TEXT("COOLDOWN"), CooldownRemaining);
            return false;
        }
        bEnemyHelicopterCooldownActive = false;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AIR_ENCOUNTER|COOLDOWN_COMPLETE|generation=%llu|world_time=%.1f|active=%d|limit=%d|timer_count=0|cooldown_until=%.1f|cooldown_remaining=0.0"),
            EnemyHelicopterEncounterGeneration,
            Now,
            ActiveCount,
            ActiveLimit,
            EnemyHelicopterCooldownUntil);
    }

    if (Now - LastEnemyHelicopterSpawnAuthorizationTime <
        RotorlineHelicopter::DuplicateEnemyHelicopterTriggerWindowSeconds)
    {
        LogEnemyHelicopterSpawnBlocked(Source, TEXT("ACTIVE"), 0.0);
        return false;
    }

    LastEnemyHelicopterSpawnAuthorizationTime = Now;
    LastEnemyHelicopterBlockReason.Reset();
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_AIR_ENCOUNTER|SPAWN_ALLOWED|source=%s|generation=%llu|world_time=%.1f|active=%d|limit=%d|timer_count=0|cooldown_until=%.1f|cooldown_remaining=0.0"),
        Source,
        EnemyHelicopterEncounterGeneration + 1,
        Now,
        ActiveCount,
        ActiveLimit,
        EnemyHelicopterCooldownUntil);
    return true;
}

void ARotorlineHelicopterPawn::RegisterEnemyHelicopterEncounter(
    ARotorlineMissionObjectiveActor* Actor,
    const TCHAR* Source)
{
    if (!IsValid(Actor) || !Actor->IsAircraftCombatActive()) return;
    for (const FEnemyHelicopterEncounter& Encounter : ActiveEnemyHelicopterEncounters)
    {
        if (Encounter.Actor.Get() == Actor) return;
    }

    FEnemyHelicopterEncounter& Encounter = ActiveEnemyHelicopterEncounters.AddDefaulted_GetRef();
    Encounter.Actor = Actor;
    Encounter.Source = Source;
    Encounter.Generation = ++EnemyHelicopterEncounterGeneration;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_AIR_ENCOUNTER|REGISTERED|actor=%s|source=%s|generation=%llu|world_time=%.1f|active=%d|limit=%d|timer_count=0|cooldown_until=%.1f|cooldown_remaining=0.0"),
        *Actor->GetName(),
        Source,
        Encounter.Generation,
        Now,
        CountActiveEnemyHelicopters(),
        FMath::Max(0, ActiveMission.MaxConcurrentEnemyHelicopters),
        EnemyHelicopterCooldownUntil);
}

void ARotorlineHelicopterPawn::RetireEnemyHelicopterEncounter(
    ARotorlineMissionObjectiveActor* Actor,
    const TCHAR* Reason,
    bool bDestroyActor)
{
    if (!Actor) return;
    FString Source = TEXT("UNKNOWN");
    uint64 Generation = EnemyHelicopterEncounterGeneration;
    bool bRemoved = false;
    for (int32 Index = ActiveEnemyHelicopterEncounters.Num() - 1; Index >= 0; --Index)
    {
        if (ActiveEnemyHelicopterEncounters[Index].Actor.Get() == Actor)
        {
            Source = ActiveEnemyHelicopterEncounters[Index].Source;
            Generation = ActiveEnemyHelicopterEncounters[Index].Generation;
            ActiveEnemyHelicopterEncounters.RemoveAtSwap(Index);
            bRemoved = true;
        }
    }
    if (!bRemoved) return;

    const FString ActorName = IsValid(Actor) ? Actor->GetName() : TEXT("EXPIRED");
    if (bDestroyActor && IsValid(Actor)) Actor->Destroy();
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_AIR_ENCOUNTER|RETIRED|actor=%s|source=%s|reason=%s|generation=%llu|world_time=%.1f|active=%d|limit=%d|timer_count=0|cooldown_until=%.1f|cooldown_remaining=0.0"),
        *ActorName,
        *Source,
        Reason,
        Generation,
        Now,
        CountActiveEnemyHelicopters(),
        FMath::Max(0, ActiveMission.MaxConcurrentEnemyHelicopters),
        EnemyHelicopterCooldownUntil);
    if (CountActiveEnemyHelicopters() == 0)
    {
        StartEnemyHelicopterCooldown(Reason);
    }
}

void ARotorlineHelicopterPawn::UpdateEnemyHelicopterEncounterGate()
{
    if (!GetWorld()) return;
    if (!bEnemyHelicopterEncounterGateTestMode &&
        (bEnemyFlightTestMode || bCombatLoopTestMode || bCombatPreviewMode))
    {
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    if (bMissionComplete || bMissionFailed)
    {
        if (!bEnemyHelicopterTerminalShutdownLogged)
        {
            if (IsValid(TransitThreatActor)) TransitThreatActor->Destroy();
            TransitThreatActor = nullptr;
            ActiveEnemyHelicopterEncounters.Reset();
            bEnemyHelicopterCooldownActive = false;
            EnemyHelicopterCooldownUntil = -1000.0;
            ResetCombatThreatState();
            bEnemyHelicopterTerminalShutdownLogged = true;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_AIR_ENCOUNTER|TERMINAL_SHUTDOWN|reason=%s|generation=%llu|world_time=%.1f|active=0|limit=%d|timer_count=0|cooldown_active=0|cooldown_until=-1000.0|cooldown_remaining=0.0"),
                bMissionComplete ? TEXT("MISSION_COMPLETE") : TEXT("MISSION_FAILED"),
                EnemyHelicopterEncounterGeneration,
                Now,
                FMath::Max(0, ActiveMission.MaxConcurrentEnemyHelicopters));
        }
        return;
    }

    bool bEncounterRetired = false;
    FString RetirementReason = TEXT("DESPAWNED");
    for (int32 Index = ActiveEnemyHelicopterEncounters.Num() - 1; Index >= 0; --Index)
    {
        const FEnemyHelicopterEncounter Encounter = ActiveEnemyHelicopterEncounters[Index];
        ARotorlineMissionObjectiveActor* Actor = Encounter.Actor.Get();
        if (IsValid(Actor) && Actor->IsAircraftCombatActive()) continue;

        if (IsValid(Actor))
        {
            RetirementReason = Actor->IsDestroyedTarget()
                ? TEXT("DESTROYED_OR_CRASH_COMPLETE")
                : TEXT("DISABLED_OR_CRASHING");
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AIR_ENCOUNTER|RETIRED|actor=%s|source=%s|reason=%s|generation=%llu|world_time=%.1f|active=%d|limit=%d|timer_count=0|cooldown_until=%.1f|cooldown_remaining=0.0"),
            IsValid(Actor) ? *Actor->GetName() : TEXT("EXPIRED"),
            *Encounter.Source,
            *RetirementReason,
            Encounter.Generation,
            Now,
            FMath::Max(0, CountActiveEnemyHelicopters() - 1),
            FMath::Max(0, ActiveMission.MaxConcurrentEnemyHelicopters),
            EnemyHelicopterCooldownUntil);
        ActiveEnemyHelicopterEncounters.RemoveAtSwap(Index);
        bEncounterRetired = true;
    }
    if (bEncounterRetired && CountActiveEnemyHelicopters() == 0)
    {
        StartEnemyHelicopterCooldown(*RetirementReason);
    }

    // A crash actor remains for its visual descent, then becomes a hidden
    // objective marker. Release the transit pointer only after that sequence.
    if (IsValid(TransitThreatActor) && TransitThreatActor->IsDestroyedTarget())
    {
        TransitThreatActor->Destroy();
        TransitThreatActor = nullptr;
        TransitThreatRetreatTime = -1000.0;
        bTransitThreatHarmless = false;
        ResetCombatThreatState();
    }

    if (bEnemyHelicopterCooldownActive && Now >= EnemyHelicopterCooldownUntil)
    {
        bEnemyHelicopterCooldownActive = false;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AIR_ENCOUNTER|COOLDOWN_COMPLETE|generation=%llu|world_time=%.1f|active=%d|limit=%d|timer_count=0|cooldown_until=%.1f|cooldown_remaining=0.0"),
            EnemyHelicopterEncounterGeneration,
            Now,
            CountActiveEnemyHelicopters(),
            FMath::Max(0, ActiveMission.MaxConcurrentEnemyHelicopters),
            EnemyHelicopterCooldownUntil);
    }
}

void ARotorlineHelicopterPawn::UpdateEnemyHelicopterEncounterGateQualification()
{
    if (!GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const double Elapsed = Now - EnemyHelicopterEncounterGateTestStartTime;
    const FVector TestObjective = GetActorLocation() + GetActorForwardVector().GetSafeNormal2D() * 120000.0f;

    if (EnemyHelicopterEncounterGateTestStage == 0 && Elapsed >= 1.0)
    {
        // Prove the live opening grace first, then advance only the qualification
        // clock so this long-form gate test does not spend another 40 seconds
        // waiting before exercising concurrency, cooldown, and redeployment.
        SpawnTransitThreat(TestObjective);
        if (IsValid(TransitThreatActor))
        {
            UE_LOG(LogTemp, Error, TEXT("ROTORLINE_AIR_ENCOUNTER_TEST|result=FAIL|reason=OPENING_SAFETY_BYPASSED"));
            FPlatformMisc::RequestExit(false);
            return;
        }
        MissionStartTime = Now - RotorlineHelicopter::OpeningCombatGraceSeconds - 0.1f;
        LastEnemyHelicopterBlockLogTime = -1000.0;
        SpawnTransitThreat(TestObjective);
        if (!IsValid(TransitThreatActor))
        {
            UE_LOG(LogTemp, Error, TEXT("ROTORLINE_AIR_ENCOUNTER_TEST|result=FAIL|reason=INITIAL_SPAWN"));
            FPlatformMisc::RequestExit(false);
            return;
        }
        EnemyHelicopterEncounterGateTestStage = 1;
    }
    else if (EnemyHelicopterEncounterGateTestStage == 1 && Elapsed >= 2.0)
    {
        SpawnTransitThreat(TestObjective); // Must be rejected as ACTIVE.
        if (ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(Controller))
        {
            OperationsController->NotifyWeaponFired();
            TransitThreatActor->ApplyRocketDamage(10000.0f);
            OperationsController->NotifyWeaponHit(true, true);
        }
        else
        {
            TransitThreatActor->ApplyRocketDamage(10000.0f);
        }
        EnemyHelicopterEncounterGateTestCooldownStartTime = Now;
        EnemyHelicopterEncounterGateTestStage = 2;
    }
    else if (EnemyHelicopterEncounterGateTestStage == 2 &&
        Now - EnemyHelicopterEncounterGateTestCooldownStartTime >= 1.0)
    {
        SpawnTransitThreat(TestObjective); // Must be rejected during cooldown.
        EnemyHelicopterEncounterGateTestStage = 3;
    }
    else if (EnemyHelicopterEncounterGateTestStage == 3 &&
        Now - EnemyHelicopterEncounterGateTestCooldownStartTime >= 59.0)
    {
        LastEnemyHelicopterBlockLogTime = -1000.0;
        SpawnTransitThreat(TestObjective); // Still rejected immediately before 60 s.
        EnemyHelicopterEncounterGateTestStage = 4;
    }
    else if (EnemyHelicopterEncounterGateTestStage == 4 &&
        Now - EnemyHelicopterEncounterGateTestCooldownStartTime >= 60.1)
    {
        SpawnTransitThreat(TestObjective); // First legal replacement.
        if (!IsValid(TransitThreatActor))
        {
            UE_LOG(LogTemp, Error, TEXT("ROTORLINE_AIR_ENCOUNTER_TEST|result=FAIL|reason=POST_COOLDOWN_SPAWN"));
            FPlatformMisc::RequestExit(false);
            return;
        }
        EnemyHelicopterEncounterGateTestStage = 5;
    }
    else if (EnemyHelicopterEncounterGateTestStage == 5 &&
        Now - EnemyHelicopterEncounterGateTestCooldownStartTime >= 60.6)
    {
        if (bMissionLoopTestMode)
        {
            if (ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(Controller))
            {
                OperationsController->NotifyWeaponFired();
                TransitThreatActor->ApplyRocketDamage(10000.0f);
                OperationsController->NotifyWeaponHit(true, true);
            }
            else if (IsValid(TransitThreatActor))
            {
                TransitThreatActor->ApplyRocketDamage(10000.0f);
            }
            UpdateEnemyHelicopterEncounterGate();
            while (!bMissionComplete && ActiveMission.Objectives.IsValidIndex(CurrentObjectiveIndex))
            {
                CompleteCurrentObjective();
            }
            EnemyHelicopterEncounterGateTestStage = 6;
        }
        else
        {
            bMissionComplete = true;
            UpdateEnemyHelicopterEncounterGate();
            SpawnTransitThreat(TestObjective); // Must be rejected by terminal state.
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_AIR_ENCOUNTER_TEST|result=PASS|cooldown_observed=%.1f|generation=%llu|active=0|timer_count=0|terminal=MISSION_COMPLETE"),
                Now - EnemyHelicopterEncounterGateTestCooldownStartTime,
                EnemyHelicopterEncounterGeneration);
            FPlatformMisc::RequestExit(false);
            EnemyHelicopterEncounterGateTestStage = 6;
        }
    }
}

void ARotorlineHelicopterPawn::UpdateTutorialHelicopterKillQualification()
{
    if (!GetWorld()) return;
    const double Elapsed = GetWorld()->GetTimeSeconds() - TutorialHelicopterKillTestStartTime;
    const FVector TestObjective = GetActorLocation() + GetActorForwardVector().GetSafeNormal2D() * 120000.0f;
    ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(Controller);

    if (TutorialHelicopterKillTestStage == 0 && Elapsed >= 1.0)
    {
        SpawnTransitThreat(TestObjective);
        const bool bDamageableAircraft = IsValid(TransitThreatActor) &&
            TransitThreatActor->IsAircraftThreat() && TransitThreatActor->IsDestroyObjective();
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_TUTORIAL_HELICOPTER_TEST|CONFIG|damageable=%d|harmless=%d|target=%s|health_pct=%.0f"),
            bDamageableAircraft ? 1 : 0,
            bTransitThreatHarmless ? 1 : 0,
            IsValid(TransitThreatActor) ? *TransitThreatActor->GetTargetLabel() : TEXT("NONE"),
            IsValid(TransitThreatActor) ? TransitThreatActor->GetHealthFraction() * 100.0f : -1.0f);
        if (!bDamageableAircraft || !bTransitThreatHarmless ||
            !SelectedAircraftId.Equals(TEXT("ah64_apache"), ESearchCase::IgnoreCase))
        {
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_TUTORIAL_HELICOPTER_TEST|result=FAIL|reason=CONFIGURATION"));
            FPlatformMisc::RequestExit(false);
            TutorialHelicopterKillTestStage = 3;
            return;
        }
        TutorialHelicopterKillTestStage = 1;
    }
    else if (TutorialHelicopterKillTestStage == 1 && Elapsed >= 2.0)
    {
        const int32 AmmoBefore = RocketAmmo;
        FireMissionRocket();
        const bool bRocketFired = RocketAmmo == AmmoBefore - 1;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_TUTORIAL_HELICOPTER_TEST|FIRE|rocket_fired=%d|ammo_before=%d|ammo_after=%d"),
            bRocketFired ? 1 : 0, AmmoBefore, RocketAmmo);
        if (!bRocketFired)
        {
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_TUTORIAL_HELICOPTER_TEST|result=FAIL|reason=ROCKET_NOT_FIRED"));
            FPlatformMisc::RequestExit(false);
            TutorialHelicopterKillTestStage = 3;
            return;
        }
        TutorialHelicopterKillTestStage = 2;
    }
    else if (TutorialHelicopterKillTestStage == 2)
    {
        const FRotorlineMissionResults* Results = OperationsController
            ? &OperationsController->GetMissionResults()
            : nullptr;
        if (Results && Results->EnemyHelicoptersDestroyed == 1)
        {
            const bool bPassed = Results->WeaponShotsFired == 1 && Results->WeaponHits == 1 &&
                Results->GroundEnemiesDestroyed == 0;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_TUTORIAL_HELICOPTER_TEST|result=%s|enemy_helicopters_destroyed=%d|ground_destroyed=%d|shots=%d|hits=%d|health_pct=%.0f"),
                bPassed ? TEXT("PASS") : TEXT("FAIL"),
                Results->EnemyHelicoptersDestroyed,
                Results->GroundEnemiesDestroyed,
                Results->WeaponShotsFired,
                Results->WeaponHits,
                IsValid(TransitThreatActor) ? TransitThreatActor->GetHealthFraction() * 100.0f : 0.0f);
            FPlatformMisc::RequestExit(false);
            TutorialHelicopterKillTestStage = 3;
        }
        else if (Elapsed >= 12.0)
        {
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_TUTORIAL_HELICOPTER_TEST|result=FAIL|reason=KILL_NOT_TRACKED|shots=%d|hits=%d|enemy_helicopters_destroyed=%d"),
                Results ? Results->WeaponShotsFired : -1,
                Results ? Results->WeaponHits : -1,
                Results ? Results->EnemyHelicoptersDestroyed : -1);
            FPlatformMisc::RequestExit(false);
            TutorialHelicopterKillTestStage = 3;
        }
    }
}

void ARotorlineHelicopterPawn::LogMissionLoopResetSnapshot(int32 Generation) const
{
    if (!bMissionLoopTestMode)
    {
        return;
    }
    const bool bHealthFull = FMath::IsNearlyEqual(CurrentHealth, MaxHealth, 0.01f);
    const bool bWeaponsRefilled =
        RocketAmmo == RocketAmmoCapacity && ApacheCannonAmmo == ApacheCannonAmmoCapacity;
    const bool bClean = bHealthFull && bWeaponsRefilled &&
        !bMissionFailed && !bMissionComplete && CountActiveEnemyHelicopters() == 0 &&
        !bEnemyHelicopterCooldownActive;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_MISSION_LOOP_TEST|RESET_SNAPSHOT|generation=%d|mission=%s|enemy_active=%d|enemy_timers=0|cooldown=%s|alerts=%s|ai=%s|health=%s|damage=ZERO|weapons=%s|audio=%s|checkpoints=RESET|mission_flags=%s|status=%s"),
        Generation,
        *ActiveMission.Id,
        CountActiveEnemyHelicopters(),
        bEnemyHelicopterCooldownActive ? TEXT("DIRTY") : TEXT("RESET"),
        ThreatAlertAudio && ThreatAlertAudio->IsPlaying() ? TEXT("DIRTY") : TEXT("CLEARED"),
        ActiveEnemyHelicopterEncounters.IsEmpty() ? TEXT("RESET") : TEXT("DIRTY"),
        bHealthFull ? TEXT("FULL") : TEXT("DIRTY"),
        bWeaponsRefilled ? TEXT("REFILLED") : TEXT("DIRTY"),
        (RadioAudio && RadioAudio->IsPlaying()) || (RadioSquelchAudio && RadioSquelchAudio->IsPlaying())
            ? TEXT("DIRTY") : TEXT("RESET"),
        (!bMissionFailed && !bMissionComplete && CurrentObjectiveIndex == 0) ? TEXT("RESET") : TEXT("DIRTY"),
        bClean ? TEXT("PASS") : TEXT("FAIL"));
}

void ARotorlineHelicopterPawn::LogMissionLoopExpectedStats(float ElapsedSeconds) const
{
    const ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(Controller);
    if (!bMissionLoopTestMode || !OperationsController)
    {
        return;
    }

    constexpr int32 AirDestroyed = 2;
    constexpr int32 GroundDestroyed = 0;
    constexpr int32 ShotsFired = 2;
    constexpr int32 ShotsHit = 2;
    constexpr float Accuracy = 100.0f;
    const int32 PrimaryTotal = ActiveMission.Objectives.Num();
    const int32 TimeBonus = ActiveMission.TimeTarget > 0
        ? FMath::Max(0, FMath::RoundToInt(static_cast<float>(ActiveMission.TimeTarget) - ElapsedSeconds)) * 5
        : 0;
    const int32 Score = FMath::Max(0,
        ActiveMission.Reward * 100 + PrimaryTotal * 500 + AirDestroyed * 1000 +
        ShotsHit * 40 + TimeBonus);
    int32 Stars = 4; // Aircraft condition is 100 percent in this deterministic sortie.
    if (ActiveMission.TimeTarget > 0 && ElapsedSeconds <= static_cast<float>(ActiveMission.TimeTarget))
    {
        ++Stars;
    }
    Stars = FMath::Clamp(Stars, 1, 5);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_MISSION_LOOP_TEST|EXPECTED_STATS|generation=%d|mission=%s|elapsed=%.1f|primary_completed=%d|primary_total=%d|optional_completed=0|optional_total=0|air_destroyed=%d|ground_destroyed=%d|civilians_rescued=0|cargo_delivered=0|damage_taken=0.0|condition=100.0|shots_fired=%d|shots_hit=%d|accuracy=%.1f|score=%d|grade=%d_STAR"),
        OperationsController->GetMissionResetGeneration(),
        *ActiveMission.Id,
        ElapsedSeconds,
        PrimaryTotal,
        PrimaryTotal,
        AirDestroyed,
        GroundDestroyed,
        ShotsFired,
        ShotsHit,
        Accuracy,
        Score,
        Stars);
}

void ARotorlineHelicopterPawn::SpawnTransitThreat(const FVector& ObjectiveWorld)
{
    if (!GetWorld()) return;
    if (IsValid(TransitThreatActor) && TransitThreatActor->IsAircraftCombatActive())
    {
        LogEnemyHelicopterSpawnBlocked(TEXT("TRANSIT_PACING"), TEXT("ACTIVE"), 0.0);
        return;
    }
    if (!CanSpawnEnemyHelicopterEncounter(TEXT("TRANSIT_PACING"))) return;
    const FVector TowardObjective = (ObjectiveWorld - GetActorLocation()).GetSafeNormal2D();
    if (TowardObjective.IsNearlyZero()) return;
    const FVector CrossRoute(-TowardObjective.Y, TowardObjective.X, 0.0f);
    const float Side = (CurrentObjectiveIndex % 2 == 0) ? 1.0f : -1.0f;
    // Spawn aircraft close enough to read their maneuver and rotor movement,
    // not as a distant icon that takes a minute to reach the player.
    FVector ThreatLocation = GetActorLocation() + TowardObjective * 56000.0f + CrossRoute * 14000.0f * Side;
    ThreatLocation.Z = GetActorLocation().Z + 1800.0f;
    FRotorlineObjectiveDefinition ThreatDefinition;
    // Tutorial pacing keeps this aircraft harmless, but every helicopter that
    // looks targetable must still accept damage and award a kill. The old
    // flyby kind silently rejected all weapon damage and created an apparent
    // invincible enemy.
    const bool bTrainingFlyby = ActiveMission.Difficulty <= 1 &&
        !bEnemyHelicopterEncounterGateTestMode;
    const bool bLostSignalHarassment =
        ActiveMission.Id.Equals(TEXT("search"), ESearchCase::IgnoreCase);
    const bool bKiowaReconStrikeHarassment =
        ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase);
    const bool bLimitedNonWeaponHarassment =
        !ActiveMission.bRequiresWeapons &&
        !ActiveMission.Id.Equals(TEXT("tutorial"), ESearchCase::IgnoreCase) &&
        !ActiveMission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase) &&
        !ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase) &&
        !ActiveMission.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase);
    const bool bLimitedSupportHarassment =
        bLostSignalHarassment ||
        bLimitedNonWeaponHarassment ||
        (ActiveMission.Difficulty == 2 && SelectedCraft == ERotorlineCraftType::SupportHuey);
    ThreatDefinition.Kind = TEXT("destroy");
    FString ContactCallout;
    if (bTrainingFlyby)
    {
        ThreatDefinition.Text = TEXT("ROTORLINE TRAINER FLYBY");
        ThreatDefinition.Target = TEXT("enemy-gunship-md500-transit");
        ContactCallout = TEXT("INSTRUCTOR: Trainer crossing the route. Hold altitude and maintain separation.");
    }
    else if (bLimitedSupportHarassment)
    {
        ThreatDefinition.Text = TEXT("HOSTILE SCOUT HELICOPTER");
        ThreatDefinition.Target = TEXT("enemy-gunship-md500-transit");
        if (bLostSignalHarassment && TransitEncountersSpawned == 1)
        {
            ContactCallout = TEXT("GUNNER: Hostile light helicopter closing fast. Machine guns, eleven o'clock!");
        }
        else if (bLostSignalHarassment && TransitEncountersSpawned >= 2)
        {
            ContactCallout = TEXT("GUNNER: Tracers ahead. Keep moving and do not hover.");
        }
        else
        {
            ContactCallout = TEXT("CREW: Armed scout closing from the right. Stay low, keep moving, and make it miss.");
        }
    }
    else if (bKiowaReconStrikeHarassment && TransitEncountersSpawned == 0)
    {
        ThreatDefinition.Text = TEXT("HOSTILE MD-500 RECON INTERCEPTOR");
        ThreatDefinition.Target = TEXT("enemy-gunship-md500-transit");
        ContactCallout = TEXT("SCOUT ONE: Hostile scout crossing the sweep route. Stay mobile and keep the mast picture clean.");
    }
    else if (bKiowaReconStrikeHarassment && TransitEncountersSpawned == 1)
    {
        ThreatDefinition.Text = TEXT("AH-64 APACHE HUNTER-KILLER");
        ThreatDefinition.Target = TEXT("enemy-apache-rocket-transit");
        ContactCallout = TEXT("COMMAND: Apache hunter-killer entering the sector. Break its lock and continue the sweep.");
    }
    else if (bKiowaReconStrikeHarassment && TransitEncountersSpawned == 2)
    {
        ThreatDefinition.Text = TEXT("MI-24 HIND REACTION FORCE");
        ThreatDefinition.Target = TEXT("enemy-hind-rocket-transit");
        ContactCallout = TEXT("COMMAND: Hind reaction force inbound. Use terrain and protect the designation aircraft.");
    }
    else if (bKiowaReconStrikeHarassment)
    {
        ThreatDefinition.Text = TEXT("AH-64 APACHE SCREEN");
        ThreatDefinition.Target = TEXT("enemy-apache-rocket-transit");
        ContactCallout = TEXT("SCOUT ONE: Final hostile screen ahead. Clear the intercept and find the priority vehicle.");
    }
    else if (ActiveMission.Difficulty >= 5)
    {
        ThreatDefinition.Text = TEXT("MI-24 HIND ROCKET GUNSHIP");
        ThreatDefinition.Target = TEXT("enemy-hind-rocket-transit");
        ContactCallout = TEXT("COMMAND: Hind inbound. Rockets hot. Break now or destroy it.");
    }
    else if (ActiveMission.Difficulty >= 4)
    {
        ThreatDefinition.Text = TEXT("AH-64 APACHE ATTACK HELICOPTER");
        ThreatDefinition.Target = TEXT("enemy-apache-rocket-transit");
        ContactCallout = TEXT("COMMAND: Apache inbound. Break its rocket lock or destroy it.");
    }
    else
    {
        ThreatDefinition.Text = TEXT("MD-500 ATTACK GUNSHIP");
        ThreatDefinition.Target = TEXT("enemy-gunship-md500-transit");
        ContactCallout = TEXT("GUNNER: Hostile light helicopter closing fast. Machine guns, eleven o'clock!");
    }
    ThreatDefinition.bHasLocation = true;
    ThreatDefinition.Radius = 80.0f;
    const FRotator InterceptHeading = (GetActorLocation() - ThreatLocation).Rotation();
    TransitThreatActor = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
        ARotorlineMissionObjectiveActor::StaticClass(), ThreatLocation, InterceptHeading);
    if (!TransitThreatActor) return;
    TransitThreatActor->Configure(ThreatDefinition, ThreatLocation);
    RegisterEnemyHelicopterEncounter(TransitThreatActor, TEXT("TRANSIT_PACING"));
    TransitThreatObjectiveIndex = CurrentObjectiveIndex;
    ++TransitEncountersSpawned;
    LastTransitEncounterTime = GetWorld()->GetTimeSeconds();
    TransitThreatAttackPasses = 0;
    bTransitThreatHarmless = bTrainingFlyby;
    // Every route interceptor is a bounded hit-and-run encounter. Previously,
    // armed contacts outside support missions had no retreat deadline and
    // could chase the player for the rest of the sortie.
    TransitThreatRetreatTime = GetWorld()->GetTimeSeconds() +
        (bTrainingFlyby ? 32.0 : (bLostSignalHarassment ? 45.0 :
            (bLimitedSupportHarassment ? 58.0 : 52.0)));
    bTransitThreatAnnounced = true;
    BroadcastRadio(ContactCallout, 7.0f);
    UpdateMissionMusic();
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_COMBAT|TRANSIT_THREAT|objective=%d|type=%s|location=%.0f,%.0f"), CurrentObjectiveIndex + 1, *ThreatDefinition.Target, ThreatLocation.X, ThreatLocation.Y);
}

void ARotorlineHelicopterPawn::SpawnKiowaReconGroundHarassment()
{
    if (!GetWorld() ||
        !ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase))
    {
        return;
    }

    struct FGroundHarassmentStation
    {
        int32 ObjectiveIndex;
        FVector2D Offset;
        const TCHAR* Label;
    };
    const FGroundHarassmentStation Stations[] = {
        { 2, FVector2D(15000.0f, 15000.0f), TEXT("ALPHA ARMOR 1") },
        { 2, FVector2D(18500.0f, 17000.0f), TEXT("ALPHA ARMOR 2") },
        { 2, FVector2D(22000.0f, 14500.0f), TEXT("ALPHA ARMOR 3") },
        { 3, FVector2D(-17000.0f, 13000.0f), TEXT("BRAVO ARMOR 1") },
        { 3, FVector2D(-20500.0f, 16500.0f), TEXT("BRAVO ARMOR 2") },
        { 3, FVector2D(-23500.0f, 12500.0f), TEXT("BRAVO ARMOR 3") },
        { 5, FVector2D(-14500.0f, -16500.0f), TEXT("CHARLIE ARMOR 1") },
        { 5, FVector2D(-18500.0f, -19000.0f), TEXT("CHARLIE ARMOR 2") },
        { 5, FVector2D(-22000.0f, -15500.0f), TEXT("CHARLIE ARMOR 3") },
        { 7, FVector2D(15000.0f, -15000.0f), TEXT("ECHO ARMOR 1") },
        { 7, FVector2D(19000.0f, -18000.0f), TEXT("ECHO ARMOR 2") },
        { 7, FVector2D(22500.0f, -14500.0f), TEXT("ECHO ARMOR 3") },
    };

    int32 Spawned = 0;
    for (const FGroundHarassmentStation& Station : Stations)
    {
        if (!ActiveMission.Objectives.IsValidIndex(Station.ObjectiveIndex)) continue;
        const FRotorlineObjectiveDefinition& RouteObjective =
            ActiveMission.Objectives[Station.ObjectiveIndex];
        if (!RouteObjective.bHasLocation) continue;

        const FVector RoutePoint = ResolveObjectiveWorld(RouteObjective);
        FVector Candidate(
            RoutePoint.X + Station.Offset.X,
            RoutePoint.Y + Station.Offset.Y,
            RoutePoint.Z + 100000.0f);
        FHitResult GroundHit;
        FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(RotorlineM18TankGrounding), false, this);
        if (!GetWorld()->LineTraceSingleByChannel(
                GroundHit,
                Candidate,
                Candidate - FVector::UpVector * 200000.0f,
                ECC_Visibility,
                GroundParams))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("ROTORLINE_M18_GROUND_SCREEN|state=SKIPPED|station=%s|reason=NO_GROUND"),
                Station.Label);
            continue;
        }

        const FVector SpawnLocation = GroundHit.ImpactPoint + FVector::UpVector * 45.0f;
        const float SpawnYaw = (RoutePoint - SpawnLocation).Rotation().Yaw;
        FRotorlineObjectiveDefinition TankDefinition;
        TankDefinition.Kind = TEXT("destroy");
        TankDefinition.Text = Station.Label;
        TankDefinition.Target = TEXT("tank");
        TankDefinition.bHasLocation = true;
        TankDefinition.Radius = 80.0f;
        ARotorlineMissionObjectiveActor* Tank = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
            ARotorlineMissionObjectiveActor::StaticClass(),
            SpawnLocation,
            FRotator(0.0f, SpawnYaw, 0.0f));
        if (!Tank) continue;
        Tank->Configure(TankDefinition, SpawnLocation);
        Tank->SetMissionMarkerVisibility(false);
        ++Spawned;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_M18_GROUND_SCREEN|state=SPAWNED|station=%s|route_distance_m=%.0f|location=%.0f,%.0f,%.0f"),
            Station.Label,
            FVector::Dist2D(RoutePoint, SpawnLocation) / 100.0f,
            SpawnLocation.X,
            SpawnLocation.Y,
            SpawnLocation.Z);
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_M18_GROUND_SCREEN|state=READY|spawned=%d|expected=12|formations=4|objective_markers=HIDDEN"),
        Spawned);
}

void ARotorlineHelicopterPawn::UpdateMissionPacing(float DistanceMeters)
{
    if (!GetWorld() || !bEngineReady || bMissionFailed || bMissionComplete || !ActiveMission.Objectives.IsValidIndex(CurrentObjectiveIndex)) return;
    if (bInsideBaseServiceZone)
    {
        if (IsValid(TransitThreatActor))
        {
            RetireEnemyHelicopterEncounter(TransitThreatActor, TEXT("BASE_AIRSPACE"), true);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_AIR_ENCOUNTER|RETIRE|reason=BASE_AIRSPACE|mission=%s"),
                *ActiveMission.Id);
        }
    }
    // The dedicated enemy-flight qualification owns exactly one selected
    // gunship. Normal transit encounters would contaminate its runtime proof.
    if (bEnemyFlightTestMode) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const FRotorlineObjectiveDefinition& Objective = ActiveMission.Objectives[CurrentObjectiveIndex];

    if (IsValid(TransitThreatActor) && TransitThreatRetreatTime > 0.0 && Now >= TransitThreatRetreatTime)
    {
        const FString ExitCall = bTransitThreatHarmless
            ? TEXT("INSTRUCTOR: Trainer clear. Continue the route.")
            : TEXT("CREW: Scout is breaking away. Route ahead is open.");
        RetireEnemyHelicopterEncounter(TransitThreatActor, TEXT("LEFT_COMBAT"), true);
        TransitThreatActor = nullptr;
        TransitThreatRetreatTime = -1000.0;
        bTransitThreatHarmless = false;
        ResetCombatThreatState();
        BroadcastRadio(ExitCall, 6.0f);
        UpdateMissionMusic();
    }

    const bool bEarlyTrainingBeat =
        !ActiveMission.Id.Equals(TEXT("tutorial"), ESearchCase::IgnoreCase) &&
        ActiveMission.Difficulty <= 1 && TransitEncountersSpawned == 0;
    const bool bEarlySupportDanger =
        ActiveMission.Difficulty == 2 && SelectedCraft == ERotorlineCraftType::SupportHuey &&
        TransitEncountersSpawned < 3 && (TransitEncountersSpawned == 0 || Now - LastTransitEncounterTime >= 55.0);
    const bool bLostSignalDanger =
        ActiveMission.Id.Equals(TEXT("search"), ESearchCase::IgnoreCase) &&
        CurrentObjectiveIndex <= 5 &&
        TransitEncountersSpawned < 5 &&
        (TransitEncountersSpawned == 0 || Now - LastTransitEncounterTime >= 30.0);
    const bool bBrokenBirdDanger =
        ActiveMission.Id.Equals(TEXT("recovery"), ESearchCase::IgnoreCase) &&
        CurrentObjectiveIndex < ActiveMission.Objectives.Num() - 1 &&
        TransitEncountersSpawned < 4 &&
        (TransitEncountersSpawned == 0 || Now - LastTransitEncounterTime >= 38.0);
    const bool bGroundOnlyRecon = ActiveMission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase);
    const bool bKiowaReconStrikeDanger =
        ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase) &&
        CurrentObjectiveIndex >= 2 &&
        CurrentObjectiveIndex < ActiveMission.Objectives.Num() - 1 &&
        TransitEncountersSpawned < 4 &&
        (TransitEncountersSpawned == 0 || Now - LastTransitEncounterTime >= 45.0);
    const bool bNonWeaponRouteDanger =
        !ActiveMission.bRequiresWeapons &&
        ActiveMission.Difficulty >= 3 &&
        !bGroundOnlyRecon &&
        !ActiveMission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase) &&
        !ActiveMission.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase) &&
        TransitEncountersSpawned < 2 &&
        (TransitEncountersSpawned == 0 || Now - LastTransitEncounterTime >= 90.0);
    const bool bStandardCombatBeat =
        ActiveMission.bRequiresWeapons && !bGroundOnlyRecon && (ActiveMission.Difficulty >= 3 ||
            (SelectedCraft == ERotorlineCraftType::AttackMD500 && ActiveMission.Difficulty >= 2));
    const float BeatDelay = (bLostSignalDanger || bBrokenBirdDanger) ? 4.0f :
        (bEarlyTrainingBeat ? 8.0f : 6.0f);
    const float BeatDistance = bBrokenBirdDanger ? 320.0f : 500.0f;

    if (!bInsideBaseServiceZone &&
        Objective.bHasLocation && Objective.Kind != TEXT("destroy") && Objective.Kind != TEXT("start-engine") && Objective.Kind != TEXT("takeoff") &&
        TransitThreatObjectiveIndex != CurrentObjectiveIndex && !IsValid(TransitThreatActor) &&
        Now - ObjectiveStartTime > BeatDelay && DistanceMeters > BeatDistance && GetAboveGroundMeters() > 12.0f &&
        (bEarlyTrainingBeat || bEarlySupportDanger || bLostSignalDanger || bBrokenBirdDanger ||
            bKiowaReconStrikeDanger || bNonWeaponRouteDanger || bStandardCombatBeat))
    {
        SpawnTransitThreat(CurrentObjectiveWorldLocation);
    }

    if (IsValid(TransitThreatActor) && TransitThreatActor->IsDestroyedTarget() && bTransitThreatAnnounced)
    {
        bTransitThreatAnnounced = false;
        BroadcastRadio(TEXT("COMMAND: Threat is down. Continue the mission."), 6.0f);
        UpdateMissionMusic();
    }

    // Ambient dialogue has its own clock. Scripted objective updates and
    // tactical warnings should get radio priority, but must not postpone crew
    // conversation until the final return leg.
    const double ChatterInterval = ActiveMission.Difficulty <= 2 ? 45.0 : 55.0;
    if (bMissionBriefActive || IsMissionRadioHoldActive() || Now < RadioMessageUntil ||
        Now - LastAmbientChatterTime < ChatterInterval)
    {
        return;
    }
    TArray<FString> Chatter;
    if (Objective.Kind == TEXT("destroy") || IsValid(TransitThreatActor))
    {
        Chatter = {
            TEXT("GUNNER: Tracers ahead. Keep moving and do not hover."),
            TEXT("COMMAND: Confirm the target before you loose a rocket."),
            TEXT("GUNNER: I have the site. Bring the nose around.")
        };
    }
    else
    {
        const FString MissionType = ActiveMission.Type.ToLower();
        if (MissionType == TEXT("search"))
        {
            if (CurrentObjectiveIndex <= 3)
            {
                Chatter = {
                    TEXT("CREW: Beacon strength is changing. Check the next relay and watch for smoke."),
                    TEXT("COMMAND: Survivor clock is active. Keep the shoreline on your right."),
                    TEXT("CREW: I have a weak return ahead. Could be the missing hiker.")
                };
            }
            else
            {
                Chatter = {
                    TEXT("COMMAND: Hospital team is standing by. Bring the survivor straight in.")
                };
            }
        }
        else if (MissionType == TEXT("combat-evacuation"))
        {
            Chatter = {
                TEXT("CREW: Coast-guard station is beyond the southern headland. Watching the shoreline."),
                TEXT("COMMAND: Harbor road is cut. The engineers have no ground route out."),
                TEXT("COMMAND: Relief pad is ready on the north shore. Bring all six home.")
            };
        }
        else if (MissionType == TEXT("medevac") || MissionType == TEXT("combat-rescue"))
        {
            Chatter = {
                TEXT("MEDIC: Patient is unstable. Give us the smoothest approach you can."),
                TEXT("COMMAND: Trauma team has the pad. Do not lose time in the circuit."),
                TEXT("CREW: Landing zone ahead. Checking wires and slope."),
                TEXT("MEDIC: We are ready to transfer as soon as the skids settle.")
            };
        }
        else if (MissionType == TEXT("supply") || MissionType == TEXT("cargo-run"))
        {
            Chatter = {
                TEXT("CREW: Cargo is secure. Avoid abrupt pedal turns."),
                TEXT("COMMAND: Cabin team has a flare out for your approach."),
                TEXT("CREW: Watching the load and the rising ground."),
                TEXT("COMMAND: Deliver, confirm the handoff, and clear the site.")
            };
        }
        else
        {
            Chatter = {
                TEXT("INSTRUCTOR: Hold the route and keep scanning outside."),
                TEXT("CREW: Ridgeline is clear. Watching the low ground."),
                TEXT("COMMAND: Keep the landing zone in sight and check your approach."),
                TEXT("CREW: Wind is moving across the valley. Correcting right.")
            };
        }
    }
    const bool bPastOpeningPhase =
        CurrentObjectiveIndex >= 2 ||
        Now - MissionStartTime >= 75.0;
    if (bPastOpeningPhase &&
        ActiveMission.Id.Equals(TEXT("tutorial"), ESearchCase::IgnoreCase) &&
        !bMission1RememberCalloutPlayed)
    {
        bMission1RememberCalloutPlayed = true;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_M1_CALLOUT|event=REMEMBER_THIS_TIME|state=PLAYING|once=1"));
        BroadcastRadio(TEXT("CREW: I remember this time."), 18.48f);
        return;
    }
    if (Chatter.IsEmpty())
    {
        return;
    }
    if (bPastOpeningPhase)
    {
        Chatter.Append({
            TEXT("CREW: People are counting on us."),
            TEXT("CREW: Keep it steady."),
            TEXT("CREW: Check your gauges.")
        });
    }
    const int32 RawChatterIndex = CurrentObjectiveIndex + FMath::FloorToInt((Now - MissionStartTime) / ChatterInterval);
    const int32 ChatterIndex = ((RawChatterIndex % Chatter.Num()) + Chatter.Num()) % Chatter.Num();
    LastAmbientChatterTime = Now;
    BroadcastRadio(Chatter[ChatterIndex], 6.0f);
}

void ARotorlineHelicopterPawn::UpdateMissionRuntime()
{
    if (bMissionFailed || bMissionComplete || !ActiveMission.Objectives.IsValidIndex(CurrentObjectiveIndex))
    {
        return;
    }

    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    const FRotorlineObjectiveDefinition& Objective = ActiveMission.Objectives[CurrentObjectiveIndex];
    if (Objective.Kind.Equals(TEXT("final-cinematic"), ESearchCase::IgnoreCase))
    {
        // Once the cargo reaches Pacific Dawn the combat portion is over. Do
        // not let surviving gauntlet actors attack an input-locked player or
        // consume full combat tick cost during the ending movie.
        ClearFinalMissionPressureActors();
        if (!IsValid(ActiveFinalCinematic))
        {
            ActiveFinalCinematic = GetWorld()->SpawnActor<ARotorlineFinalCinematicActor>(ARotorlineFinalCinematicActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
            if (ActiveFinalCinematic) ActiveFinalCinematic->StartFinale();
        }
        if (IsValid(ActiveFinalCinematic) && ActiveFinalCinematic->IsFinished())
        {
            ActiveFinalCinematic->Destroy();
            ActiveFinalCinematic = nullptr;
            CompleteCurrentObjective();
        }
        return;
    }
    // Weapon input is mission-global. It must be processed before specialized
    // objective branches return from this update.
    const bool bFirePressed = PlayerController->WasInputKeyJustPressed(EKeys::LeftMouseButton) ||
        PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder) ||
        (Cast<ARotorlineOperationsPlayerController>(PlayerController) &&
            CastChecked<ARotorlineOperationsPlayerController>(PlayerController)->WasFlightControllerActionJustPressed(
                RotorlineFlightControllerActions::PrimaryFire));
    if (bFirePressed && IsBell222SpecialOperations() && IsBell222MissileMode())
    {
        FireBell222MissileMode();
    }
    else if (bFirePressed && SelectedCraft == ERotorlineCraftType::AttackMD500 &&
        (!HasAttackCombatPackage() || bApacheMissileLockMode) && !IsBell222SpecialOperations())
    {
        FireMissionRocket();
    }
    if (Objective.Kind.Equals(TEXT("escort-cabin-convoy"), ESearchCase::IgnoreCase))
    {
        if (!IsValid(ActiveCabinSupplyConvoy))
        {
            RefreshMissionObjectiveActor();
        }
        if (!IsValid(ActiveCabinSupplyConvoy))
        {
            FailMission(TEXT("WAREHOUSE SUPPLY COLUMN COULD NOT DEPLOY"), false);
            return;
        }
        UpdateMissionCombat(0.0f);
        if (ActiveCabinSupplyConvoy->IsMissionFailed())
        {
            FailMission(TEXT("WAREHOUSE SUPPLY COLUMN LOST"), false);
            return;
        }
        if (ActiveCabinSupplyConvoy->IsMissionSucceeded())
        {
            BroadcastRadio(
                TEXT("COMMAND: Supply column is secure at the warehouse. Temporary storage confirmed."),
                7.0f);
            CompleteCurrentObjective();
        }
        return;
    }
    const bool bKiowaDesignationObjective =
        Objective.Kind.Equals(TEXT("designate-strike"), ESearchCase::IgnoreCase) ||
        Objective.Kind.Equals(TEXT("designate-recon"), ESearchCase::IgnoreCase);
    if (bKiowaDesignationObjective)
    {
        // Sensor objectives used to return before the combat director ticked,
        // which made every Mission 6 site inert precisely while the Kiowa was
        // forced to hover and hold a lock. Keep ground pressure active through
        // the full reconnaissance dwell.
        UpdateMissionCombat(0.0f);
        if (!IsValid(ActiveKiowaStrikeMission))
        {
            FailMission(TEXT("KIOWA RECONNAISSANCE SENSOR UNAVAILABLE"), false);
            return;
        }
        if (ActiveKiowaStrikeMission->IsFailed())
        {
            FailMission(Objective.Kind.Equals(TEXT("designate-strike"), ESearchCase::IgnoreCase)
                ? TEXT("ALLIED STRIKE ABORTED")
                : TEXT("RECONNAISSANCE SENSOR ABORTED"), false);
            return;
        }
        // Destroying the Jaguar legitimately removes the objective actor. Once
        // designation has completed, the allied strike coordinator owns the
        // remaining confirmation and egress sequence, so the missing target is
        // expected and must never be interpreted as package failure.
        if (!IsValid(ActiveObjectiveActor) && ActiveKiowaStrikeMission->IsSensorMissionActive())
        {
            FailMission(TEXT("RECONNAISSANCE TARGET UNAVAILABLE"), false);
            return;
        }
        if (ActiveKiowaStrikeMission->IsComplete())
        {
            CompleteCurrentObjective();
        }
        return;
    }
    if (IsMissionAirObjective(Objective) && !IsValid(ActiveObjectiveActor))
    {
        // Mission-authored aircraft use the same serialization/cooldown gate as
        // ambient contacts. A deferred objective is retried here without
        // creating a timer or a second spawn authority.
        RefreshMissionObjectiveActor();
        if (!IsValid(ActiveObjectiveActor)) return;
    }
    const float AGL = GetAboveGroundMeters();
    const bool bInteractPressed = PlayerController->WasInputKeyJustPressed(EKeys::F) ||
        PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom) ||
        (Cast<ARotorlineOperationsPlayerController>(PlayerController) &&
            CastChecked<ARotorlineOperationsPlayerController>(PlayerController)->WasFlightControllerActionJustPressed(
                RotorlineFlightControllerActions::MissionInteract));
    bool bInsideObjective = !Objective.bHasLocation;
    float DistanceMeters = 0.0f;
    if (Objective.bHasLocation)
    {
        const FVector ObjectiveWorld = CurrentObjectiveWorldLocation;
        DistanceMeters = FVector::Dist2D(GetActorLocation(), ObjectiveWorld) / 100.0f;
        bInsideObjective = DistanceMeters <= FMath::Max(15.0f, Objective.Radius * 4.0f);
    }
    UpdateMissionPacing(DistanceMeters);
    UpdateMissionCombat(DistanceMeters);

    bool bCompleted = false;
    if (Objective.Kind == TEXT("start-engine"))
    {
        bCompleted = bEngineReady;
    }
    else if (Objective.Kind == TEXT("takeoff"))
    {
        bCompleted = AGL >= 12.0f;
    }
    else if (Objective.Kind == TEXT("reach") || Objective.Kind == TEXT("return"))
    {
        // A navigation objective without a parsed location must never count as
        // complete. This protects campaign missions from silently ending when
        // mission data contains a misspelled or missing coordinate field.
        bCompleted = Objective.bHasLocation && bInsideObjective &&
            (!Objective.bHasMaxAltitude || AGL <= Objective.MaxAltitude);
    }
    else if (Objective.Kind == TEXT("land"))
    {
        bCompleted = bInsideObjective && AGL >= 0.0f && AGL <= 4.0f && CurrentVelocity.Size() < 650.0f;
    }
    else if (Objective.Kind == TEXT("interact"))
    {
        bCompleted = bInsideObjective && bInteractPressed;
    }
    else if (Objective.Kind == TEXT("destroy"))
    {
        bCompleted = IsValid(ActiveObjectiveActor) && ActiveObjectiveActor->IsDestroyedTarget();
    }

    if (bCompleted)
    {
        CompleteCurrentObjective();
        return;
    }

}

void ARotorlineHelicopterPawn::DiscoverMapHelipads()
{
    AdditionalServicePadLocations.Reset();
    AdditionalServicePadLocations.Add(RotorlineSupportLocations::FieldHospitalHelipad);

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Remove only marker rings created by this discovery system so repeated
    // deployments cannot stack lights around the same authored pad.
    for (TActorIterator<ARotorlineHelipadBeaconActor> It(World); It; ++It)
    {
        ARotorlineHelipadBeaconActor* ExistingBeacon = *It;
        if (IsValid(ExistingBeacon) && ExistingBeacon->ActorHasTag(TEXT("RotorlineMapHelipadBeacon")))
        {
            ExistingBeacon->Destroy();
        }
    }

    int32 StaticPadCount = 0;
    int32 AddedBeaconCount = 0;
    const FVector HomePadLocation(
        RotorlineHelicopter::SpawnX,
        RotorlineHelicopter::SpawnY,
        0.0);

    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        AStaticMeshActor* StaticActor = *It;
        if (!IsValid(StaticActor) || StaticActor->IsHidden())
        {
            continue;
        }

        const UStaticMeshComponent* MeshComponent = StaticActor->GetStaticMeshComponent();
        const UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
        FString SearchText = StaticActor->GetName() + TEXT(" ") + StaticActor->GetActorNameOrLabel();
        if (StaticMesh)
        {
            SearchText += TEXT(" ") + StaticMesh->GetPathName();
        }

        const bool bTaggedMissionPad = StaticActor->ActorHasTag(TEXT("RotorlineMissionPad"));
        const bool bNamedHelipad =
            SearchText.Contains(TEXT("helipad"), ESearchCase::IgnoreCase) ||
            SearchText.Contains(TEXT("heliport"), ESearchCase::IgnoreCase) ||
            SearchText.Contains(TEXT("landing_pad"), ESearchCase::IgnoreCase);
        const bool bConcealedLairPad =
            SearchText.Contains(TEXT("bell_lair"), ESearchCase::IgnoreCase) ||
            SearchText.Contains(TEXT("belllair"), ESearchCase::IgnoreCase);
        if ((!bTaggedMissionPad && !bNamedHelipad) || bConcealedLairPad)
        {
            continue;
        }

        FVector BoundsOrigin = StaticActor->GetActorLocation();
        FVector BoundsExtent = FVector::ZeroVector;
        StaticActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
        const FVector ServiceLocation(
            BoundsOrigin.X,
            BoundsOrigin.Y,
            BoundsOrigin.Z + BoundsExtent.Z);

        const bool bAlreadyRegistered = AdditionalServicePadLocations.ContainsByPredicate(
            [&ServiceLocation](const FVector& ExistingLocation)
            {
                return FVector::Dist2D(ExistingLocation, ServiceLocation) <= 1000.0;
            });
        if (!bAlreadyRegistered)
        {
            AdditionalServicePadLocations.Add(ServiceLocation);
        }
        ++StaticPadCount;

        // Suppress an authored-pad ring only when a real operational beacon is
        // aligned to that physical deck. The old coordinate-only assumption
        // left manually placed replacement pads dark whenever the planned
        // beacon failed grounding or appeared at another elevation.
        bool bCoveredByOperationalBeacon = false;
        for (TActorIterator<ARotorlineHelipadBeaconActor> BeaconIt(World); BeaconIt; ++BeaconIt)
        {
            const ARotorlineHelipadBeaconActor* OperationalBeacon = *BeaconIt;
            if (!IsValid(OperationalBeacon) ||
                OperationalBeacon->ActorHasTag(TEXT("RotorlineMapHelipadBeacon")))
            {
                continue;
            }
            const FVector BeaconLocation = OperationalBeacon->GetActorLocation();
            const bool bDeckAligned =
                FVector::Dist2D(ServiceLocation, BeaconLocation) <= 1200.0 &&
                FMath::Abs(ServiceLocation.Z - BeaconLocation.Z) <= 600.0;
            if (bDeckAligned)
            {
                bCoveredByOperationalBeacon = true;
                break;
            }
        }
        if (bCoveredByOperationalBeacon)
        {
            continue;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ARotorlineHelipadBeaconActor* Beacon = World->SpawnActor<ARotorlineHelipadBeaconActor>(
            ARotorlineHelipadBeaconActor::StaticClass(),
            ServiceLocation,
            StaticActor->GetActorRotation(),
            SpawnParameters);
        if (Beacon)
        {
            Beacon->Tags.AddUnique(TEXT("RotorlineMapHelipadBeacon"));
            Beacon->Configure(
                true,
                true,
                ServiceLocation,
                FString::Printf(TEXT("MAP_HELIPAD_%s"), *StaticActor->GetName()));
            ++AddedBeaconCount;
        }
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Rotorline map helipads: static_pads=%d service_zones=%d added_beacons=%d always_blinking=1"),
        StaticPadCount,
        AdditionalServicePadLocations.Num(),
        AddedBeaconCount);
}

void ARotorlineHelicopterPawn::UpdateBaseRearm(float DeltaSeconds)
{
    const bool bBellLairMission = IsBell222SpecialOperations() &&
        ActiveMission.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase);
    const FVector Home = bBellLairMission
        ? FVector(RotorlineSupportLocations::BellLairPeak.X, RotorlineSupportLocations::BellLairPeak.Y, GetActorLocation().Z)
        : FVector(RotorlineHelicopter::SpawnX, RotorlineHelicopter::SpawnY, GetActorLocation().Z);
    const float DistanceFromBaseCm = FVector::Dist2D(GetActorLocation(), Home);
    float DistanceFromCityPadCm = FVector::Dist2D(
        GetActorLocation(), RotorlineSupportLocations::CentralTownRearmPad);
    const FVector CarrierCenter(530000.0f, 405000.0f, GetActorLocation().Z);
    const float DistanceFromCarrierCm = FVector::Dist2D(
        GetActorLocation(), CarrierCenter);
    constexpr float CarrierServiceRadiusCm = 19000.0f;
    constexpr float CarrierServiceResetRadiusCm = 23000.0f;
    for (const FVector& ServicePadLocation : AdditionalServicePadLocations)
    {
        DistanceFromCityPadCm = FMath::Min(
            DistanceFromCityPadCm,
            FVector::Dist2D(GetActorLocation(), ServicePadLocation));
    }
    bInsideBaseServiceZone = DistanceFromBaseCm <= RotorlineHelicopter::BaseServiceRadiusCm;
    const bool bInsideCarrierServiceZone =
        DistanceFromCarrierCm <= CarrierServiceRadiusCm;
    bInsideCityServiceZone =
        DistanceFromCityPadCm <= RotorlineSupportLocations::ServiceRadiusCm ||
        bInsideCarrierServiceZone;
    const float DistanceFromServiceCm = FMath::Min(
        FMath::Min(DistanceFromBaseCm, DistanceFromCityPadCm),
        DistanceFromCarrierCm);
    const TCHAR* ServiceSite = bInsideCarrierServiceZone
        ? TEXT("AIRCRAFT_CARRIER")
        : (bInsideCityServiceZone ? TEXT("CENTRAL_TOWN") : TEXT("HOME"));

    if (DistanceFromBaseCm > RotorlineHelicopter::BaseServiceResetRadiusCm &&
        DistanceFromCityPadCm > RotorlineSupportLocations::ServiceResetRadiusCm &&
        DistanceFromCarrierCm > CarrierServiceResetRadiusCm)
    {
        bBaseRearmLatched = false;
    }

    bool bBellAmmoDepleted = false;
    for (const TPair<FString, int32>& Store : Bell222WeaponCapacity)
    {
        bBellAmmoDepleted |= Bell222WeaponAmmo.FindRef(Store.Key) < Store.Value;
    }
    const bool bAmmoDepleted = bBellAmmoDepleted || RocketAmmo < RocketAmmoCapacity ||
        ApacheCannonAmmo < ApacheCannonAmmoCapacity || CountermeasureCharges < CountermeasureCapacity;
    const bool bRepairNeeded = CurrentHealth < MaxHealth - KINDA_SMALL_NUMBER;
    const bool bFuelNeeded = FuelRemainingPercent < RotorlineHelicopter::ServiceFuelThresholdPercent;
    if ((!bAmmoDepleted && !bRepairNeeded && !bFuelNeeded) || bBaseRearmLatched || bMissionFailed)
    {
        BaseRearmProgress = 0.0f;
        bBaseRearmActive = false;
        return;
    }

    const float AGL = GetAboveGroundMeters();
    // Terrain traces and pad collision resolution can jitter slightly below
    // zero while the skids are visibly planted. Use separate horizontal and
    // vertical limits so harmless settling does not continuously reset the
    // three-second service timer.
    const float HorizontalServiceSpeed = CurrentVelocity.Size2D();
    const float VerticalServiceSpeed = FMath::Abs(CurrentVelocity.Z);
    const bool bNormallyLandedAndStopped =
        AGL >= -1.0f &&
        AGL <= 5.5f &&
        HorizontalServiceSpeed <= 350.0f &&
        VerticalServiceSpeed <= 250.0f;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const bool bLandedOnCarrierDeck =
        bInsideCarrierServiceZone &&
        Now - LastSafeServicePadContactTime <= 8.0 &&
        HorizontalServiceSpeed <= 350.0f &&
        VerticalServiceSpeed <= 250.0f;
    const bool bEmergencyPadRecovery =
        FuelRemainingPercent <= RotorlineHelicopter::FuelWarningFumesPercent &&
        (bBaseRearmActive || Now - LastSafeServicePadContactTime <= 2.5) &&
        (bInsideCarrierServiceZone || (AGL >= -1.0f && AGL <= 5.5f)) &&
        HorizontalServiceSpeed <= 500.0f &&
        VerticalServiceSpeed <= 300.0f;
    const bool bLandedAndStopped =
        bNormallyLandedAndStopped || bLandedOnCarrierDeck ||
        bEmergencyPadRecovery;
    if ((!bInsideBaseServiceZone && !bInsideCityServiceZone) || !bLandedAndStopped || (!bEngineReady && !bFuelStarved))
    {
        if (bBaseRearmActive)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_BASE_SERVICE|state=INTERRUPTED|site=%s|distance_m=%.1f|agl_m=%.1f|speed_mps=%.1f"),
                ServiceSite, DistanceFromServiceCm / 100.0f, AGL, CurrentVelocity.Size() / 100.0f);
        }
        BaseRearmProgress = 0.0f;
        bBaseRearmActive = false;
        return;
    }

    if (!bBaseRearmActive)
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BASE_SERVICE|state=REARM_STARTED|site=%s|rockets=%d/%d|cannon=%d/%d|countermeasures=%d/%d|health=%.1f/%.1f|fuel=%.1f"),
            ServiceSite, RocketAmmo, RocketAmmoCapacity, ApacheCannonAmmo, ApacheCannonAmmoCapacity,
            CountermeasureCharges, CountermeasureCapacity, CurrentHealth, MaxHealth, FuelRemainingPercent);
    }
    bBaseRearmActive = true;
    BaseRearmProgress += DeltaSeconds;
    if (BaseRearmProgress < RotorlineHelicopter::BaseRearmDurationSeconds)
    {
        return;
    }

    const float HealthBeforeService = CurrentHealth;
    const float FuelBeforeService = FuelRemainingPercent;
    const float ExpectedHealthAfterService = FMath::Clamp(
        HealthBeforeService + (MaxHealth - HealthBeforeService) * RotorlineHelicopter::ServiceRepairMissingHealthFraction,
        0.0f,
        MaxHealth);
    RocketAmmo = RocketAmmoCapacity;
    ApacheCannonAmmo = ApacheCannonAmmoCapacity;
    for (const TPair<FString, int32>& Store : Bell222WeaponCapacity)
    {
        Bell222WeaponAmmo.Add(Store.Key, Store.Value);
    }
    CountermeasureCharges = CountermeasureCapacity;
    CurrentHealth = ExpectedHealthAfterService;
    FuelRemainingPercent = 100.0f;
    CurrentFuelBurnMultiplier = 1.0f;
    bFuelStarved = false;
    bFuelLowWarningIssued = false;
    bFuelCriticalWarningIssued = false;
    bFuelFumesWarningIssued = false;
    ApacheCannonHeat = 0.0f;
    bApacheCannonOverheated = false;
    BaseRearmProgress = 0.0f;
    bBaseRearmActive = false;
    bBaseRearmLatched = true;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            7120,
            4.0f,
            FColor(90, 255, 160),
            FString::Printf(TEXT("%s SERVICE COMPLETE  //  FUEL 100%%  //  HULL +%.0f  //  ROCKETS %d  //  30MM %d  //  CM %d"),
                bInsideCityServiceZone ? TEXT("CITY PAD") : TEXT("BASE"),
                CurrentHealth - HealthBeforeService, RocketAmmo, ApacheCannonAmmo, CountermeasureCharges));
    }
    if (ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(Controller))
    {
        OperationsController->NotifyAircraftCondition(CurrentHealth, MaxHealth);
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BASE_SERVICE|state=REARM_COMPLETE|site=%s|rockets=%d|cannon=%d|countermeasures=%d|health_before=%.1f|health_after=%.1f|fuel_before=%.1f|fuel_after=%.1f|repair_fraction_missing=%.2f|service_seconds=%.1f"),
        ServiceSite, RocketAmmo, ApacheCannonAmmo, CountermeasureCharges,
        HealthBeforeService, CurrentHealth, FuelBeforeService, FuelRemainingPercent,
        RotorlineHelicopter::ServiceRepairMissingHealthFraction,
        RotorlineHelicopter::BaseRearmDurationSeconds);
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineBaseRearmTest")) ||
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineCityRearmTest")))
    {
        const bool bRepairPassed = FMath::IsNearlyEqual(CurrentHealth, ExpectedHealthAfterService, 0.01f) &&
            FMath::IsNearlyEqual(CurrentHealth, MaxHealth * 0.7f, 0.01f) &&
            CurrentHealth > HealthBeforeService && CurrentHealth <= MaxHealth &&
            FMath::IsNearlyEqual(FuelRemainingPercent, 100.0f, 0.01f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BASE_SERVICE_TEST|state=%s|health_before=%.1f|health_after=%.1f|health_expected=%.1f|fuel_before=%.1f|fuel_after=%.1f|countermeasures=%d/%d|auto_exit=1"),
            bRepairPassed ? TEXT("PASS") : TEXT("FAIL"), HealthBeforeService, CurrentHealth,
            ExpectedHealthAfterService, FuelBeforeService, FuelRemainingPercent,
            CountermeasureCharges, CountermeasureCapacity);
        FPlatformMisc::RequestExit(false);
    }
}

void ARotorlineHelicopterPawn::CompleteCurrentObjective()
{
    const bool bFinalEvacuationStartupLocked =
        ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase) &&
        (!bEngineReady ||
         bMissionBriefActive ||
         (MissionBriefAudio && MissionBriefAudio->IsPlaying()));
    if (bFinalEvacuationStartupLocked)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                7125,
                2.0f,
                FColor(255, 215, 105),
                TEXT("CHINOOK STARTUP IN PROGRESS  //  HOLD FOR MISSION BRIEF"));
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_M25_CARGO|state=PICKUP_HELD|reason=STARTUP_SEQUENCE_ACTIVE|engine_ready=%d|brief_active=%d"),
            bEngineReady ? 1 : 0,
            bMissionBriefActive ? 1 : 0);
        return;
    }

    const int32 CompletedIndex = CurrentObjectiveIndex;
    const FRotorlineObjectiveDefinition CompletedObjective =
        ActiveMission.Objectives.IsValidIndex(CompletedIndex)
            ? ActiveMission.Objectives[CompletedIndex]
            : FRotorlineObjectiveDefinition();
    UpdateFinalMissionCargo(CompletedObjective);
    ResetCombatThreatState();
    if (IsValid(ActiveObjectiveActor))
    {
        RetireEnemyHelicopterEncounter(ActiveObjectiveActor, TEXT("OBJECTIVE_COMPLETE"), false);
        ActiveObjectiveActor->Destroy();
        ActiveObjectiveActor = nullptr;
    }
    if (IsValid(ActiveCabinSupplyConvoy))
    {
        ActiveCabinSupplyConvoy->Destroy();
        ActiveCabinSupplyConvoy = nullptr;
    }
    if (IsValid(TransitThreatActor))
    {
        RetireEnemyHelicopterEncounter(TransitThreatActor, TEXT("OBJECTIVE_ADVANCED"), true);
        TransitThreatActor = nullptr;
    }
    TransitThreatObjectiveIndex = INDEX_NONE;
    bTransitThreatAnnounced = false;
    TransitThreatAttackPasses = 0;
    TransitThreatRetreatTime = -1000.0;
    bTransitThreatHarmless = false;
    ++CurrentObjectiveIndex;
    if (ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase))
    {
        if (CompletedObjective.Kind.Equals(TEXT("takeoff"), ESearchCase::IgnoreCase)) PlayFinalMissionCallout(TEXT("/Game/Audio/Missions/SurvivorExtraction/VO_M24_Obj1.VO_M24_Obj1"), TEXT("OBJ_1_ASSIGNED"));
        else if (CompletedObjective.Kind.Equals(TEXT("land"), ESearchCase::IgnoreCase) && CompletedObjective.Site.Equals(TEXT("western-church-lz"), ESearchCase::IgnoreCase)) PlayFinalMissionCallout(TEXT("/Game/Audio/Missions/SurvivorExtraction/VO_M24_Obj2.VO_M24_Obj2"), TEXT("OBJ_1_REACHED"));
        else if (CompletedObjective.Kind.Equals(TEXT("interact"), ESearchCase::IgnoreCase) && CompletedObjective.Site.Equals(TEXT("ridge-cabin-lz"), ESearchCase::IgnoreCase)) PlayFinalMissionCallout(TEXT("/Game/Audio/Missions/SurvivorExtraction/VO_M24_Obj3.VO_M24_Obj3"), TEXT("FINAL_SURVIVORS_ABOARD"));
    }
    if (ActiveMission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase) &&
        CompletedObjective.Kind.Equals(TEXT("designate-recon"), ESearchCase::IgnoreCase) &&
        CurrentObjectiveIndex == 8 &&
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineKiowaReconTest")))
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_KIOWA_RECON_TEST|RESULT|contacts=6|allied_strike=DISABLED|result=PASS"));
        FPlatformMisc::RequestExit(false);
        return;
    }
    ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(Controller);
    if (OperationsController)
    {
        OperationsController->NotifyObjectiveCompleted(false);
        if (CompletedObjective.RescueCount > 0)
        {
            OperationsController->NotifyCivilianRescued(CompletedObjective.RescueCount);
        }
        if (CompletedObjective.CargoDeliveryCount > 0)
        {
            OperationsController->NotifyCargoDelivered(
                CompletedObjective.CargoDeliveryCount,
                CompletedObjective.bSlingLoadDelivery);
        }
    }
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        PlayerController->PlayDynamicForceFeedback(0.42f, 0.13f, true, true, true, true);
    }

    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_MISSION|mission=%s|objective=%d|state=COMPLETE"), *ActiveMission.Id, CompletedIndex + 1);
    if (CurrentObjectiveIndex >= ActiveMission.Objectives.Num())
    {
        const float Elapsed = GetWorld()->GetTimeSeconds() - MissionStartTime;
        bMissionComplete = true;
        ClearFinalMissionSetPieces();
        // Completion is terminal before the results screen pauses the world.
        // This clears the active encounter, cancels its cooldown, and proves a
        // late checkpoint/script request cannot create another helicopter.
        UpdateEnemyHelicopterEncounterGate();
        if (bMissionLoopTestMode)
        {
            CanSpawnEnemyHelicopterEncounter(TEXT("QUALIFICATION_POST_COMPLETE"));
        }
        ResetCombatThreatState();
        StopApacheCannonAudio(TEXT("MISSION_COMPLETE"));
        if (ThreatAlertAudio) ThreatAlertAudio->Stop();
        if (RadioAudio) RadioAudio->Stop();
        if (InstructorAudio) InstructorAudio->Stop();
        if (RadioSquelchAudio) RadioSquelchAudio->Stop();
        if (MissionMusicAudio) MissionMusicAudio->FadeOut(0.8f, 0.0f);
        if (OperationsController)
        {
            OperationsController->NotifyAircraftCondition(CurrentHealth, MaxHealth);
            if (bMissionLoopTestMode)
            {
                LogMissionLoopExpectedStats(Elapsed);
            }
            OperationsController->RecordMissionCompletion(ActiveMission, Elapsed);
        }
        if (GEngine && !ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase))
        {
            GEngine->AddOnScreenDebugMessage(
                7110,
                12.0f,
                FColor(120, 255, 150),
                FString::Printf(TEXT("MISSION COMPLETE  //  %s  //  +%d XP  //  %.0f:%02.0f"), *ActiveMission.Title.ToUpper(), ActiveMission.Reward, FMath::FloorToFloat(Elapsed / 60.0f), FMath::Fmod(Elapsed, 60.0f)));
        }
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_MISSION|mission=%s|state=COMPLETE|elapsed=%.1f|reward=%d"), *ActiveMission.Id, Elapsed, ActiveMission.Reward);
    }
    else
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(7112, 2.0f, FColor(120, 255, 150), TEXT("OBJECTIVE COMPLETE"));
        }
        RefreshMissionObjectiveActor();
    }
}

void ARotorlineHelicopterPawn::PlayFinalMissionCallout(const TCHAR* AssetPath, const TCHAR* EventName)
{
    USoundBase* Sound = LoadObject<USoundBase>(nullptr, AssetPath);
    if (!Sound || !InstructorAudio) return;
    if (RadioAudio) RadioAudio->Stop();
    if (MissionBriefAudio) MissionBriefAudio->Stop();
    InstructorAudio->Stop(); InstructorAudio->SetSound(Sound);
    InstructorAudio->SetVolumeMultiplier(GetMissionRadioVolume()); InstructorAudio->Play();
    const float Duration = FMath::Max(1.0f, Sound->GetDuration());
    MissionRadioHoldUntil = GetWorld()->GetTimeSeconds() + Duration + 1.0f;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_FINAL_MISSION_AUDIO|mission=%s|event=%s|duration=%.2f"), *ActiveMission.Id, EventName, Duration);
}

void ARotorlineHelicopterPawn::UpdateFinalMissionCargo(const FRotorlineObjectiveDefinition& CompletedObjective)
{
    if (!ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase) || !CompletedObjective.Kind.Equals(TEXT("interact"), ESearchCase::IgnoreCase)) return;
    const FString LowerText = CompletedObjective.Text.ToLower();
    if (LowerText.Contains(TEXT("secure")) && LowerText.Contains(TEXT("aircraft shipping container")))
    {
        if (IsValid(ActiveFinalMissionCargo))
        {
            ActiveFinalMissionCargo->Destroy();
            ActiveFinalMissionCargo = nullptr;
        }
        if (FinalMissionCrateActors.IsValidIndex(0) && IsValid(FinalMissionCrateActors[0]))
        {
            ActiveFinalMissionCargo = FinalMissionCrateActors[0];
            FinalMissionCrateActors[0] = nullptr;
        }
        if (!IsValid(ActiveFinalMissionCargo))
        {
            ActiveFinalMissionCargo = GetWorld()->SpawnActor<AStaticMeshActor>(
                AStaticMeshActor::StaticClass(),
                GetActorTransform());
        }
        if (ActiveFinalMissionCargo)
        {
            UStaticMesh* Mesh = ActiveFinalMissionCargo->GetStaticMeshComponent()->GetStaticMesh();
            if (!Mesh)
            {
                Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Environment/Imported/Hangar/ClassifiedAirframeCrate/rotorline_airframe_shipping_crate/StaticMeshes/SM_Rotorline_AirframeShippingCrate.SM_Rotorline_AirframeShippingCrate"));
            }
            if (Mesh)
            {
                ActiveFinalMissionCargo->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
                ActiveFinalMissionCargo->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                ActiveFinalMissionCargo->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                ActiveFinalMissionCargo->GetStaticMeshComponent()->SetVisibility(true, true);
                ActiveFinalMissionCargo->GetStaticMeshComponent()->SetHiddenInGame(false, true);
                ActiveFinalMissionCargo->SetActorEnableCollision(false);
                ActiveFinalMissionCargo->SetActorHiddenInGame(false);
                ActiveFinalMissionCargo->SetActorScale3D(FVector::OneVector);
                ActiveFinalMissionCargo->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                UpdateFinalMissionCargoSling(1.0f);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_M25_CARGO|state=ATTACHED|objective=%s|mesh=VISIBLE|ground_clamped=1|full_sling_z=-600"),
                    *CompletedObjective.Text);
            }
            else
            {
                ActiveFinalMissionCargo->Destroy();
                ActiveFinalMissionCargo = nullptr;
                UE_LOG(LogTemp, Error,
                    TEXT("ROTORLINE_M25_CARGO|state=ATTACH_FAILED|reason=CRATE_MESH_MISSING|objective=%s"),
                    *CompletedObjective.Text);
            }
        }
    }
    else if (LowerText.Contains(TEXT("deliver")) && LowerText.Contains(TEXT("aircraft shipping container")))
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Environment/Imported/Hangar/ClassifiedAirframeCrate/rotorline_airframe_shipping_crate/StaticMeshes/SM_Rotorline_AirframeShippingCrate.SM_Rotorline_AirframeShippingCrate"));
        FVector DeliveryLocation =
            FVector(530000.0f, 405000.0f, 0.0f) + FVector(-5200.0f, -1700.0f, 1680.0f);
        if (Mesh)
        {
            FHitResult DeckHit;
            FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(M25DeliveredCrateGrounding), false, this);
            const FVector TraceStart(DeliveryLocation.X, DeliveryLocation.Y, DeliveryLocation.Z + 10000.0f);
            const FVector TraceEnd(DeliveryLocation.X, DeliveryLocation.Y, DeliveryLocation.Z - 10000.0f);
            if (GetWorld()->LineTraceSingleByChannel(
                    DeckHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
            {
                const FBoxSphereBounds Bounds = Mesh->GetBounds();
                const float BottomFromPivot = Bounds.Origin.Z - Bounds.BoxExtent.Z;
                DeliveryLocation.Z = DeckHit.ImpactPoint.Z - BottomFromPivot + 8.0f;
            }
        }
        AStaticMeshActor* DeliveredCrate = GetWorld()->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(),
            DeliveryLocation,
            FRotator(0.0f, 90.0f, 0.0f));
        if (DeliveredCrate && Mesh)
        {
            DeliveredCrate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            DeliveredCrate->GetStaticMeshComponent()->SetStaticMesh(Mesh);
            DeliveredCrate->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            DeliveredCrate->GetStaticMeshComponent()->SetVisibility(true, true);
            DeliveredCrate->GetStaticMeshComponent()->SetHiddenInGame(false, true);
            DeliveredCrate->SetActorEnableCollision(false);
            DeliveredCrate->SetActorHiddenInGame(false);
            DeliveredCrate->SetActorScale3D(FVector::OneVector);
            FinalMissionDeliveredCrates.Add(DeliveredCrate);
        }
        else if (DeliveredCrate)
        {
            DeliveredCrate->Destroy();
        }
        if (IsValid(ActiveFinalMissionCargo)) { ActiveFinalMissionCargo->Destroy(); ActiveFinalMissionCargo = nullptr; }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_M25_CARGO|state=%s|objective=%s|location=%s"),
            Mesh ? TEXT("DELIVERED_VISIBLE") : TEXT("DELIVERY_FAILED_MESH_MISSING"),
            *CompletedObjective.Text,
            *DeliveryLocation.ToCompactString());
    }
}

void ARotorlineHelicopterPawn::UpdateFinalMissionCargoSling(float DeltaSeconds)
{
    if (!ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase) ||
        !IsValid(ActiveFinalMissionCargo))
    {
        return;
    }

    UStaticMeshComponent* CargoComponent = ActiveFinalMissionCargo->GetStaticMeshComponent();
    UStaticMesh* CargoMesh = CargoComponent ? CargoComponent->GetStaticMesh() : nullptr;
    if (!CargoComponent || !CargoMesh)
    {
        return;
    }

    CargoComponent->SetVisibility(true, true);
    CargoComponent->SetHiddenInGame(false, true);
    ActiveFinalMissionCargo->SetActorHiddenInGame(false);

    const FVector FullSlingLocation =
        GetActorTransform().TransformPosition(FVector(-100.0f, 0.0f, -600.0f));
    FVector DesiredLocation = FullSlingLocation;

    FHitResult GroundHit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(M25SlingGroundClamp), false, this);
    TraceParams.AddIgnoredActor(ActiveFinalMissionCargo);
    const FVector TraceStart(
        FullSlingLocation.X,
        FullSlingLocation.Y,
        GetActorLocation().Z + 5000.0f);
    const FVector TraceEnd(
        FullSlingLocation.X,
        FullSlingLocation.Y,
        GetActorLocation().Z - 20000.0f);
    if (GetWorld()->LineTraceSingleByChannel(
            GroundHit,
            TraceStart,
            TraceEnd,
            ECC_Visibility,
            TraceParams))
    {
        const FBoxSphereBounds Bounds = CargoMesh->GetBounds();
        const float BottomFromPivot = Bounds.Origin.Z - Bounds.BoxExtent.Z;
        const float GroundedPivotZ = GroundHit.ImpactPoint.Z - BottomFromPivot + 8.0f;
        DesiredLocation.Z = FMath::Max(DesiredLocation.Z, GroundedPivotZ);
    }

    const FVector SmoothedLocation = FMath::VInterpTo(
        ActiveFinalMissionCargo->GetActorLocation(),
        DesiredLocation,
        FMath::Max(0.0f, DeltaSeconds),
        8.0f);
    const FRotator SlingRotation(0.0f, GetActorRotation().Yaw, 0.0f);
    ActiveFinalMissionCargo->SetActorLocationAndRotation(
        SmoothedLocation,
        SlingRotation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
}

void ARotorlineHelicopterPawn::ClearFinalMissionPressureActors()
{
    for (ARotorlineMissionObjectiveActor* Actor : FinalMissionPressureActors)
    {
        if (!IsValid(Actor)) continue;
        if (Actor->IsAircraftThreat())
        {
            RetireEnemyHelicopterEncounter(Actor, TEXT("FINAL_PRESSURE_CLEANUP"), true);
        }
        if (IsValid(Actor)) Actor->Destroy();
    }
    FinalMissionPressureActors.Reset();
}

void ARotorlineHelicopterPawn::ClearFinalMissionSetPieces()
{
    ClearFinalMissionPressureActors();
    for (AStaticMeshActor* Actor : FinalMissionCrateActors)
    {
        if (IsValid(Actor)) Actor->Destroy();
    }
    for (AStaticMeshActor* Actor : FinalMissionDeliveredCrates)
    {
        if (IsValid(Actor)) Actor->Destroy();
    }
    FinalMissionCrateActors.Reset();
    FinalMissionDeliveredCrates.Reset();
    if (IsValid(ActiveFinalMissionCargo))
    {
        ActiveFinalMissionCargo->Destroy();
        ActiveFinalMissionCargo = nullptr;
    }
}

void ARotorlineHelicopterPawn::SpawnFinalMissionSetPieces()
{
    ClearFinalMissionSetPieces();
    if (!GetWorld()) return;

    struct FPressureSpawn
    {
        FVector Location;
        const TCHAR* Target;
        const TCHAR* Label;
        float Yaw;
    };

    TArray<FPressureSpawn> Spawns;
    if (ActiveMission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase))
    {
        Spawns =
        {
            { FVector(-218800.0f, 158600.0f, 0.0f), TEXT("tank"), TEXT("M24 western armor one"), 35.0f },
            { FVector(-229400.0f, 170900.0f, 0.0f), TEXT("tank"), TEXT("M24 western armor two"), 210.0f },
            { FVector(-216500.0f, 172400.0f, 0.0f), TEXT("flak"), TEXT("M24 western flak cannon"), 120.0f },
            { FVector(214500.0f, -158500.0f, 0.0f), TEXT("tank"), TEXT("M24 hospital armor one"), 300.0f },
            { FVector(231000.0f, -170500.0f, 0.0f), TEXT("tank"), TEXT("M24 hospital armor two"), 120.0f },
            { FVector(228500.0f, -153500.0f, 0.0f), TEXT("flak"), TEXT("M24 hospital flak cannon"), 200.0f },
            { FVector(179500.0f, 121500.0f, 0.0f), TEXT("tank"), TEXT("M24 cabin armor one"), 25.0f },
            { FVector(197000.0f, 137500.0f, 0.0f), TEXT("tank"), TEXT("M24 cabin armor two"), 205.0f },
            { FVector(176000.0f, 139000.0f, 0.0f), TEXT("flak"), TEXT("M24 cabin flak cannon"), 80.0f }
        };
    }
    else if (ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase))
    {
        Spawns =
        {
            { FVector(-170000.0f, -142000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 01"), 35.0f },
            { FVector(-158000.0f, -126000.0f, 0.0f), TEXT("flak"), TEXT("M25 gauntlet flak 01"), 210.0f },
            { FVector(-142000.0f, -112000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 02"), 55.0f },
            { FVector(-124000.0f, -93000.0f, 0.0f), TEXT("himars"), TEXT("M25 gauntlet rocket battery 01"), 20.0f },
            { FVector(-108000.0f, -76000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 03"), 80.0f },
            { FVector(-90000.0f, -57000.0f, 0.0f), TEXT("flak"), TEXT("M25 gauntlet flak 02"), 245.0f },
            { FVector(-71000.0f, -38000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 04"), 105.0f },
            { FVector(-52000.0f, -18000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 05"), 285.0f },
            { FVector(-33000.0f, 2000.0f, 0.0f), TEXT("flak"), TEXT("M25 gauntlet flak 03"), 120.0f },
            { FVector(-14000.0f, 23000.0f, 0.0f), TEXT("himars"), TEXT("M25 gauntlet rocket battery 02"), 195.0f },
            { FVector(6000.0f, 44000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 06"), 145.0f },
            { FVector(27000.0f, 65000.0f, 0.0f), TEXT("flak"), TEXT("M25 gauntlet flak 04"), 320.0f },
            { FVector(48000.0f, 86000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 07"), 165.0f },
            { FVector(70000.0f, 106000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 08"), 340.0f },
            { FVector(92000.0f, 124000.0f, 0.0f), TEXT("flak"), TEXT("M25 gauntlet flak 05"), 185.0f },
            { FVector(114000.0f, 140000.0f, 0.0f), TEXT("himars"), TEXT("M25 gauntlet rocket battery 03"), 260.0f },
            { FVector(136000.0f, 154000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 09"), 205.0f },
            { FVector(158000.0f, 166000.0f, 0.0f), TEXT("flak"), TEXT("M25 gauntlet flak 06"), 25.0f },
            { FVector(180000.0f, 176000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 10"), 225.0f },
            { FVector(200000.0f, 184000.0f, 0.0f), TEXT("tank"), TEXT("M25 gauntlet armor 11"), 45.0f },
            { FVector(218000.0f, 190000.0f, 0.0f), TEXT("flak"), TEXT("M25 gauntlet flak 07"), 250.0f },
            { FVector(-145000.0f, -90000.0f, 6500.0f), TEXT("md500 gunship"), TEXT("M25 gauntlet gunship 01"), 35.0f },
            { FVector(-76000.0f, -26000.0f, 8000.0f), TEXT("apache gunship"), TEXT("M25 gauntlet gunship 02"), 215.0f },
            { FVector(-5000.0f, 42000.0f, 7000.0f), TEXT("hind gunship"), TEXT("M25 gauntlet gunship 03"), 65.0f },
            { FVector(68000.0f, 106000.0f, 9000.0f), TEXT("md500 gunship"), TEXT("M25 gauntlet gunship 04"), 245.0f },
            { FVector(138000.0f, 158000.0f, 7500.0f), TEXT("apache gunship"), TEXT("M25 gauntlet gunship 05"), 95.0f },
            { FVector(208000.0f, 192000.0f, 8500.0f), TEXT("hind gunship"), TEXT("M25 gauntlet gunship 06"), 275.0f }
        };
    }

    for (const FPressureSpawn& Spawn : Spawns)
    {
        FRotorlineObjectiveDefinition Definition;
        Definition.Kind = TEXT("destroy");
        Definition.Text = Spawn.Label;
        Definition.Target = Spawn.Target;
        Definition.bHasLocation = true;
        Definition.Radius = 80.0f;
        ARotorlineMissionObjectiveActor* Threat = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
            ARotorlineMissionObjectiveActor::StaticClass(), Spawn.Location, FRotator(0.0f, Spawn.Yaw, 0.0f));
        if (Threat)
        {
            Threat->Configure(Definition, Spawn.Location);
            Threat->SetMissionMarkerVisibility(false);
            FinalMissionPressureActors.Add(Threat);
            if (Threat->IsAircraftThreat())
            {
                RegisterEnemyHelicopterEncounter(Threat, TEXT("FINAL_GAUNTLET"));
            }
        }
    }

    if (ActiveMission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase))
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Environment/Imported/Hangar/ClassifiedAirframeCrate/rotorline_airframe_shipping_crate/StaticMeshes/SM_Rotorline_AirframeShippingCrate.SM_Rotorline_AirframeShippingCrate"));
        const FVector BaseOffsets[] =
        {
            FVector(1200.0f, 0.0f, 0.0f)
        };
        for (const FVector& Offset : BaseOffsets)
        {
            FVector CrateLocation = FVector(-236194.0f, -193028.0f, 0.0f) + Offset;
            FHitResult GroundHit;
            FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(M25CrateGrounding), false, this);
            const FVector TraceStart(CrateLocation.X, CrateLocation.Y, 100000.0f);
            const FVector TraceEnd(CrateLocation.X, CrateLocation.Y, -100000.0f);
            if (GetWorld()->LineTraceSingleByChannel(
                    GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
            {
                CrateLocation.Z = GroundHit.ImpactPoint.Z + 5.0f;
            }
            else
            {
                CrateLocation.Z = GetActorLocation().Z;
            }
            if (Mesh)
            {
                const FBoxSphereBounds Bounds = Mesh->GetBounds();
                const float BottomFromPivot = Bounds.Origin.Z - Bounds.BoxExtent.Z;
                CrateLocation.Z -= BottomFromPivot;
            }
            AStaticMeshActor* Crate = GetWorld()->SpawnActor<AStaticMeshActor>(
                AStaticMeshActor::StaticClass(),
                CrateLocation,
                FRotator(0.0f, 90.0f, 0.0f));
            if (Crate && Mesh)
            {
                Crate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
                Crate->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                Crate->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Crate->GetStaticMeshComponent()->SetVisibility(true, true);
                Crate->GetStaticMeshComponent()->SetHiddenInGame(false, true);
                Crate->SetActorEnableCollision(false);
                Crate->SetActorHiddenInGame(false);
                Crate->SetActorScale3D(FVector::OneVector);
                FinalMissionCrateActors.Add(Crate);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_M25_CARGO|state=PICKUP_VISIBLE|location=%s|bounds_z=%.1f"),
                    *CrateLocation.ToCompactString(),
                    Mesh->GetBounds().BoxExtent.Z);
            }
            else if (Crate)
            {
                Crate->Destroy();
            }
        }
        if (!Mesh)
        {
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_M25_CARGO|state=PICKUP_FAILED|reason=CRATE_MESH_MISSING"));
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_FINAL_MISSION_SETPIECES|mission=%s|threats=%d|pickup_crates=%d"),
        *ActiveMission.Id, FinalMissionPressureActors.Num(), FinalMissionCrateActors.Num());
}

void ARotorlineHelicopterPawn::RefreshMissionObjectiveActor()
{
    if (IsValid(ActiveObjectiveActor))
    {
        ActiveObjectiveActor->Destroy();
        ActiveObjectiveActor = nullptr;
    }
    if (!ActiveMission.Objectives.IsValidIndex(CurrentObjectiveIndex))
    {
        return;
    }

    const FRotorlineObjectiveDefinition& Objective = ActiveMission.Objectives[CurrentObjectiveIndex];
    ObjectiveStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    TransitThreatObjectiveIndex = INDEX_NONE;
    bTransitThreatAnnounced = false;
    UpdateMissionMusic();
    if (Objective.Kind.Equals(TEXT("escort-cabin-convoy"), ESearchCase::IgnoreCase))
    {
        if (!IsValid(ActiveCabinSupplyConvoy))
        {
            ActiveCabinSupplyConvoy = GetWorld()->SpawnActor<ARotorlineCabinSupplyConvoyActor>(
                ARotorlineCabinSupplyConvoyActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
            if (!ActiveCabinSupplyConvoy ||
                !ActiveCabinSupplyConvoy->ConfigureAndStart(this))
            {
                if (IsValid(ActiveCabinSupplyConvoy)) ActiveCabinSupplyConvoy->Destroy();
                ActiveCabinSupplyConvoy = nullptr;
                return;
            }
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_MISSION|mission=%s|objective=%d|cabin_supply_convoy=DEPLOYED"),
                *ActiveMission.Id, CurrentObjectiveIndex + 1);
        }
        CurrentObjectiveWorldLocation = ActiveCabinSupplyConvoy->GetLeadWorldLocation();
        if (CurrentObjectiveIndex > 0 && !bMissionBriefActive)
        {
            BroadcastRadio(
                TEXT("COMMAND: Supply column departing the airfield. Follow it to the warehouse and keep the road clear."),
                8.0f);
        }
        return;
    }
    if (!Objective.bHasLocation)
    {
        return;
    }

    if (IsMissionAirObjective(Objective))
    {
        if (!CanSpawnEnemyHelicopterEncounter(TEXT("MISSION_OBJECTIVE")))
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_AIR_ENCOUNTER|OBJECTIVE_DEFERRED|source=MISSION_OBJECTIVE|reason=OPENING_OR_GATE|world_time=%.1f|mission_elapsed=%.1f|grace_seconds=%.1f"),
                GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0,
                GetWorld() ? GetWorld()->GetTimeSeconds() - MissionStartTime : 0.0,
                RotorlineHelicopter::OpeningCombatGraceSeconds);
            return;
        }
        const int32 ActiveEnemyHelicopters = CountActiveEnemyHelicopters();
        const int32 MissionAirThreatLimit = FMath::Max(0, ActiveMission.MaxConcurrentEnemyHelicopters);
        if (ActiveEnemyHelicopters >= MissionAirThreatLimit)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_AIR_ENCOUNTER|OBJECTIVE_DEFERRED|source=MISSION_OBJECTIVE|reason=CAPACITY|generation=%llu|world_time=%.1f|active=%d|limit=%d"),
                EnemyHelicopterEncounterGeneration,
                GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0,
                ActiveEnemyHelicopters,
                MissionAirThreatLimit);
            return;
        }

        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AIR_ENCOUNTER|MISSION_OBJECTIVE_PRIORITY|generation=%llu|world_time=%.1f|active=%d|limit=%d"),
            EnemyHelicopterEncounterGeneration,
            GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0,
            ActiveEnemyHelicopters,
            MissionAirThreatLimit);
    }

    FVector ObjectiveWorld = ResolveObjectiveWorld(Objective);
    const bool bRooftopExtractionTeam =
        ActiveMission.Id.Equals(TEXT("rooftop-extraction"), ESearchCase::IgnoreCase) &&
        Objective.Site.Equals(TEXT("rooftop-extraction-team"), ESearchCase::IgnoreCase);
    if (bRooftopExtractionTeam && GetWorld())
    {
        // Runtime population owns the city buildings, so a JSON Z value cannot
        // reliably identify a roof. Resolve the tallest flat-roof shop in the
        // harbor district from the instances that actually spawned, then use
        // that same point for the HUD, interaction test, and visible team.
        const FVector2D PreferredDistrict(158000.0, -119000.0);
        double BestHeightMeters = -1.0;
        double BestDistrictDistanceSquared = TNumericLimits<double>::Max();
        FVector BestRoofLocation = FVector::ZeroVector;
        bool bFoundRooftop = false;
        for (TActorIterator<ARotorlineBuildingClusterActor> It(GetWorld()); It; ++It)
        {
            ARotorlineBuildingClusterActor* Cluster = *It;
            UHierarchicalInstancedStaticMeshComponent* ShellInstances =
                Cluster ? Cluster->GetShellInstancesComponent() : nullptr;
            if (!Cluster || !Cluster->Tags.Contains(TEXT("RotorlineRuntimePopulation")) || !ShellInstances)
            {
                continue;
            }
            const int32 InstanceCount = ShellInstances->GetInstanceCount();
            for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
            {
                FTransform ShellTransform;
                if (!ShellInstances->GetInstanceTransform(InstanceIndex, ShellTransform, true))
                {
                    continue;
                }
                const FVector DimensionsMeters = ShellTransform.GetScale3D().GetAbs();
                const bool bUsableFlatRoof =
                    DimensionsMeters.X >= 20.0 && DimensionsMeters.X <= 32.0 &&
                    DimensionsMeters.Y >= 14.0 && DimensionsMeters.Y <= 20.0 &&
                    DimensionsMeters.Z >= 12.0;
                if (!bUsableFlatRoof)
                {
                    continue;
                }
                const FVector ShellCenter = ShellTransform.GetLocation();
                const double DistrictDistanceSquared = FVector2D::DistSquared(
                    FVector2D(ShellCenter.X, ShellCenter.Y), PreferredDistrict);
                const bool bTaller = DimensionsMeters.Z > BestHeightMeters + 0.05;
                const bool bCloserAtSameHeight =
                    FMath::IsNearlyEqual(DimensionsMeters.Z, BestHeightMeters, 0.05) &&
                    DistrictDistanceSquared < BestDistrictDistanceSquared;
                if (!bTaller && !bCloserAtSameHeight)
                {
                    continue;
                }
                BestHeightMeters = DimensionsMeters.Z;
                BestDistrictDistanceSquared = DistrictDistanceSquared;
                // Procedural flat roofs add a 76 cm cap above the shell. Eight
                // extra centimetres keeps character feet visibly grounded.
                BestRoofLocation = FVector(
                    ShellCenter.X,
                    ShellCenter.Y,
                    ShellCenter.Z + DimensionsMeters.Z * 50.0 + 84.0);
                bFoundRooftop = true;
            }
        }
        if (bFoundRooftop)
        {
            ObjectiveWorld = BestRoofLocation;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_M21_ROOFTOP|RESOLVED|location=(%.1f,%.1f,%.1f)|building_height_m=%.1f|district=HARBOR_SUBURB"),
                ObjectiveWorld.X, ObjectiveWorld.Y, ObjectiveWorld.Z, BestHeightMeters);
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_M21_ROOFTOP|FAILED|reason=NO_RUNTIME_FLAT_ROOF|fallback=(%.1f,%.1f,%.1f)"),
                ObjectiveWorld.X, ObjectiveWorld.Y, ObjectiveWorld.Z);
        }
    }
    CurrentObjectiveWorldLocation = ObjectiveWorld;
    FVector ObjectiveActorWorld = ObjectiveWorld;
    if (Objective.Kind == TEXT("interact") && !bRooftopExtractionTeam &&
        FVector::Dist2D(GetActorLocation(), ObjectiveWorld) < 3500.0f)
    {
        // Keep base pickup props visible beside the aircraft while preserving the
        // original interaction radius centered on the mission's authored point.
        ObjectiveActorWorld += GetActorRightVector() * 1800.0f;
    }
    ActiveObjectiveActor = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
        ARotorlineMissionObjectiveActor::StaticClass(),
        ObjectiveActorWorld,
        FRotator::ZeroRotator);
    if (ActiveObjectiveActor)
    {
        ActiveObjectiveActor->Configure(Objective, ObjectiveActorWorld);
        if ((Objective.Kind.Equals(TEXT("designate-strike"), ESearchCase::IgnoreCase) ||
                Objective.Kind.Equals(TEXT("designate-recon"), ESearchCase::IgnoreCase)) &&
            IsValid(ActiveKiowaStrikeMission))
        {
            ActiveKiowaStrikeMission->BeginReconnaissance(ActiveObjectiveActor);
        }
        if (ActiveObjectiveActor->IsAircraftThreat())
        {
            RegisterEnemyHelicopterEncounter(ActiveObjectiveActor, TEXT("MISSION_OBJECTIVE"));
        }
    }
    if (CurrentObjectiveIndex > 0 && !bMissionBriefActive)
    {
        BroadcastRadio(FString::Printf(TEXT("COMMAND: New objective. %s"), *Objective.Text), 6.5f);
    }
}

void ARotorlineHelicopterPawn::FireMissionRocket()
{
    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController || !bSelectedAircraftArmed)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(7111, 2.0f, FColor::Red,
                TEXT("THIS AIRCRAFT IS CONFIGURED FOR SUPPORT / RECON - NO ROCKETS FITTED"));
        }
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastRocketFireTime < 0.75)
    {
        return;
    }
    if (RocketAmmo <= 0)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(7111, 2.0f, FColor::Red, TEXT("ROCKET RACK EMPTY"));
        return;
    }

    const bool bApacheSelected = HasAttackCombatPackage();
    ARotorlineMissionObjectiveActor* LockTarget = nullptr;
    float DistanceMeters = 0.0f;
    // Attack helicopters share the two-mode Apache combat package. Homing is
    // available only while the red MISSILE LOCK display is selected.
    if (bApacheMissileLockMode)
    {
        LockTarget = FindBestMissileLockTarget(DistanceMeters);
    }

    const FVector CollisionExtent = CollisionBox->GetScaledBoxExtent();
    const bool bMH6WeaponStations =
        SelectedAircraftId.Equals(TEXT("md500_defender"), ESearchCase::IgnoreCase) &&
        MD500LeftRocketMuzzle && MD500RightRocketMuzzle;
    const bool bLaunchLeftStation = (RocketAmmo % 2) == 0;
    FString LaunchStation = TEXT("GENERIC_RACK");
    FVector LaunchLocation;
    if (bMH6WeaponStations)
    {
        USceneComponent* Station = bLaunchLeftStation
            ? MD500LeftRocketMuzzle.Get()
            : MD500RightRocketMuzzle.Get();
        LaunchLocation = Station->GetComponentLocation();
        LaunchStation = bLaunchLeftStation ? TEXT("MH6_LEFT_POD") : TEXT("MH6_RIGHT_POD");
    }
    else
    {
        const float LaunchForward = FMath::Max(430.0f, CollisionExtent.X * 0.90f);
        const float LaunchLateral = FMath::Max(150.0f, CollisionExtent.Y * 0.62f);
        const float LaunchSide = bLaunchLeftStation ? -LaunchLateral : LaunchLateral;
        LaunchLocation = GetActorLocation() + GetActorForwardVector() * LaunchForward +
            GetActorRightVector() * LaunchSide - FVector::UpVector * FMath::Max(35.0f, CollisionExtent.Z * 0.15f);
    }
    FVector LaunchDirection = bMH6WeaponStations
        ? VisualRoot->GetForwardVector().GetSafeNormal()
        : GetActorForwardVector().GetSafeNormal();
    bool bReticleAimedUnguidedRocket = false;
    if (!LockTarget && bApacheSelected)
    {
        FVector SightMuzzleLocation;
        FVector SightDirection;
        FVector SightImpactLocation;
        bool bSightBlockingHit = false;
        if (GetApacheWeaponAimSolution(SightMuzzleLocation, SightDirection, SightImpactLocation, bSightBlockingHit))
        {
            // Rockets leave alternating wing racks, so aim each pod at the same
            // point represented by the Apache sight instead of merely flying a
            // parallel actor-forward path beside the reticle.
            LaunchDirection = (SightImpactLocation - LaunchLocation).GetSafeNormal();
            if (LaunchDirection.IsNearlyZero())
            {
                LaunchDirection = SightDirection.GetSafeNormal();
            }
            bReticleAimedUnguidedRocket = true;
        }
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    ARotorlineRocketProjectile* Rocket = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
        ARotorlineRocketProjectile::StaticClass(), LaunchLocation, LaunchDirection.Rotation(), SpawnParams);
    if (!Rocket)
    {
        return;
    }

    Rocket->Launch(LaunchLocation, LaunchDirection, LockTarget);
    --RocketAmmo;
    if (ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(Controller))
    {
        OperationsController->NotifyWeaponFired();
    }
    LastRocketFireTime = Now;
    PlayerController->PlayDynamicForceFeedback(0.72f, 0.18f, true, true, true, true);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(7111, 1.5f, FColor(255, 200, 75), FString::Printf(TEXT("ROCKET AWAY  //  %s  //  %d REMAINING"), LockTarget ? TEXT("TARGET TRACKING") : TEXT("UNGUIDED"), RocketAmmo));
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_WEAPON|ROCKET|state=FIRED|ammo=%d|distance=%.0f|homing=%d|reticle_aim=%d|mode=%s|launch_dot_nose=%.4f|station=%s"),
        RocketAmmo,
        DistanceMeters,
        LockTarget ? 1 : 0,
        bReticleAimedUnguidedRocket ? 1 : 0,
        bApacheMissileLockMode ? TEXT("MISSILE_LOCK") : TEXT("ATTACK_CANNON"),
        FVector::DotProduct(LaunchDirection, GetActorForwardVector().GetSafeNormal()),
        *LaunchStation);
}

bool ARotorlineHelicopterPawn::GetApacheWeaponAimSolution(
    FVector& OutMuzzleLocation,
    FVector& OutAimDirection,
    FVector& OutImpactLocation,
    bool& bOutBlockingHit) const
{
    if (!HasAttackCombatPackage() || !GetWorld())
    {
        return false;
    }

    // The pawn actor itself only yaws; cyclic pitch and roll live on VisualRoot.
    // Use that component's world-space nose axis so the sight and every weapon
    // using this solution follow the visible attack-helicopter attitude, not a level actor
    // axis or the offset third-person camera.
    OutAimDirection = VisualRoot->GetForwardVector().GetSafeNormal();
    const FVector CollisionExtent = CollisionBox->GetScaledBoxExtent();
    OutMuzzleLocation = VisualRoot->GetComponentLocation() +
        OutAimDirection * FMath::Max(430.0f, CollisionExtent.X * 0.92f);
    // Keep every attack-aircraft sight on the visible nose axis. The previous
    // non-Bell downward offset made the HUD and projectile path disagree.

    constexpr float MaximumWeaponRangeCm = 500000.0f;
    const FVector TraceEnd = OutMuzzleLocation + OutAimDirection * MaximumWeaponRangeCm;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RotorlineApacheWeaponSight), true, this);
    TraceParams.AddIgnoredActor(this);

    FHitResult Impact;
    bOutBlockingHit = GetWorld()->LineTraceSingleByChannel(
        Impact,
        OutMuzzleLocation,
        TraceEnd,
        ECC_Visibility,
        TraceParams);
    OutImpactLocation = bOutBlockingHit ? Impact.ImpactPoint : TraceEnd;
    return true;
}

void ARotorlineHelicopterPawn::UpdateApacheCannon(float DeltaSeconds)
{
    if (!HasAttackCombatPackage())
    {
        StopApacheCannonAudio(TEXT("NOT_ATTACK_CLASS"));
        return;
    }

    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController || !GetWorld())
    {
        StopApacheCannonAudio(TEXT("CONTROL_UNAVAILABLE"));
        return;
    }

    if (IsBell222SpecialOperations())
    {
        ARotorlineOperationsPlayerController* OperationsController =
            Cast<ARotorlineOperationsPlayerController>(PlayerController);
        const bool bPrimaryFireHeld = PlayerController->IsInputKeyDown(EKeys::Gamepad_RightShoulder) ||
            PlayerController->IsInputKeyDown(EKeys::LeftMouseButton) ||
            (OperationsController && OperationsController->IsFlightControllerActionPressed(
                RotorlineFlightControllerActions::PrimaryFire));
        const bool bSecondaryFireHeld = PlayerController->IsInputKeyDown(EKeys::Gamepad_LeftShoulder) ||
            PlayerController->IsInputKeyDown(EKeys::RightMouseButton);
        if (IsBell222GunMode() && (bPrimaryFireHeld || bSecondaryFireHeld))
        {
            FireBell222GunMode();
        }
        else
        {
            StopApacheCannonAudio(TEXT("BELL_TRIGGER_RELEASED"));
        }
        return;
    }

    constexpr float HeatDissipationPerSecond = 26.0f;
    constexpr float HeatRecoveryThreshold = 45.0f;
    constexpr double CoolingDelay = 0.65;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastApacheCannonFireTime >= CoolingDelay)
    {
        ApacheCannonHeat = FMath::Max(0.0f, ApacheCannonHeat - HeatDissipationPerSecond * DeltaSeconds);
    }
    if (bApacheCannonOverheated && ApacheCannonHeat <= HeatRecoveryThreshold)
    {
        bApacheCannonOverheated = false;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_WEAPON|APACHE_30MM|state=COOLED|heat=%.1f"), ApacheCannonHeat);
    }

    ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(PlayerController);
    const bool bPrimaryFireHeld = PlayerController->IsInputKeyDown(EKeys::Gamepad_RightShoulder) ||
        PlayerController->IsInputKeyDown(EKeys::LeftMouseButton) ||
        (OperationsController && OperationsController->IsFlightControllerActionPressed(
            RotorlineFlightControllerActions::PrimaryFire));
    // Secondary fire remains a direct cannon trigger. In 30MM mode, primary
    // fire also drives the cannon so one joystick trigger fires the weapon
    // currently selected by Hat-Right/Weapon Cycle.
    const bool bCannonHeld = (!bApacheMissileLockMode && bPrimaryFireHeld) ||
        PlayerController->IsInputKeyDown(EKeys::Gamepad_LeftShoulder) ||
        PlayerController->IsInputKeyDown(EKeys::RightMouseButton) ||
        (OperationsController && OperationsController->IsFlightControllerActionPressed(
            RotorlineFlightControllerActions::SecondaryFire));
    if (!bCannonHeld)
    {
        StopApacheCannonAudio(TEXT("TRIGGER_RELEASED"));
        return;
    }
    if (!bEngineReady)
    {
        StopApacheCannonAudio(TEXT("ENGINE_UNAVAILABLE"));
        return;
    }
    if (bMissionFailed || bMissionComplete)
    {
        StopApacheCannonAudio(TEXT("MISSION_INACTIVE"));
        return;
    }

    if (ApacheCannonAmmo <= 0)
    {
        StopApacheCannonAudio(TEXT("AMMO_EMPTY"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(7112, 0.18f, FColor::Red, TEXT("30MM AMMUNITION DEPLETED"));
        }
        return;
    }
    if (bApacheCannonOverheated)
    {
        StopApacheCannonAudio(TEXT("OVERHEATED"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(7112, 0.18f, FColor(255, 120, 45),
                FString::Printf(TEXT("30MM OVERHEATED  //  COOLING %.0f%%"), ApacheCannonHeat));
        }
        return;
    }

    constexpr double FireInterval = 0.095; // ~632 rounds/minute, M230 high-rate alpha tune.
    if (Now - LastApacheCannonFireTime >= FireInterval)
    {
        FireApacheCannon();
    }
}

void ARotorlineHelicopterPawn::FireApacheCannon()
{
    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController || !GetWorld() || ApacheCannonAmmo <= 0)
    {
        StopApacheCannonAudio(
            ApacheCannonAmmo <= 0 ? TEXT("AMMO_EMPTY") : TEXT("CONTROL_UNAVAILABLE"));
        return;
    }

    FVector MuzzleLocation;
    FVector AimDirection;
    FVector ImpactLocation;
    bool bBlockingHit = false;
    if (!GetApacheWeaponAimSolution(MuzzleLocation, AimDirection, ImpactLocation, bBlockingHit))
    {
        StopApacheCannonAudio(TEXT("AIM_UNAVAILABLE"));
        return;
    }

    // The HUD pipper and cannon use one authoritative boresight. Random cone
    // dispersion caused stable-hover rounds to miss the displayed aim point.
    const FVector ShotDirection = AimDirection.GetSafeNormal();

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    const bool bTwinLittleBirdMiniguns =
        SelectedAircraftId.Equals(TEXT("md500_defender"), ESearchCase::IgnoreCase);
    const bool bUsingModelWeaponStations = bTwinLittleBirdMiniguns &&
        MD500LeftGunMuzzle && MD500RightGunMuzzle;
    if (bTwinLittleBirdMiniguns && !bUsingModelWeaponStations)
    {
        StopApacheCannonAudio(TEXT("MH6_MUZZLES_UNAVAILABLE"));
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_MH6_WEAPONS|GUN|state=BLOCKED|reason=MUZZLES_UNAVAILABLE"));
        return;
    }
    const int32 MuzzleCount = bTwinLittleBirdMiniguns ? 2 : 1;
    bool bSpawnedProjectile = false;
    for (int32 MuzzleIndex = 0; MuzzleIndex < MuzzleCount; ++MuzzleIndex)
    {
        const FVector ShotMuzzle = bUsingModelWeaponStations
            ? (MuzzleIndex == 0
                ? MD500LeftGunMuzzle->GetComponentLocation()
                : MD500RightGunMuzzle->GetComponentLocation())
            : MuzzleLocation;
        // Each physical muzzle must converge on the world-space impact point
        // represented by the HUD reticle. Parallel streams only agree with
        // the sight at extreme range and fall visibly below it during steep,
        // close attack runs because the chase camera sits above the muzzles.
        const FVector ConvergedDirection = (ImpactLocation - ShotMuzzle).GetSafeNormal();
        const FVector PairedDirection = ConvergedDirection.IsNearlyZero()
            ? ShotDirection
            : ConvergedDirection;
        ARotorlineCannonProjectile* Projectile = GetWorld()->SpawnActor<ARotorlineCannonProjectile>(
            ARotorlineCannonProjectile::StaticClass(),
            ShotMuzzle,
            PairedDirection.Rotation(),
            SpawnParams);
        if (Projectile)
        {
            Projectile->Launch(ShotMuzzle, PairedDirection, 28.0f);
            bSpawnedProjectile = true;
        }
    }
    if (!bSpawnedProjectile)
    {
        StopApacheCannonAudio(TEXT("PROJECTILE_SPAWN_FAILED"));
        return;
    }

    --ApacheCannonAmmo;
    if (ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(Controller))
    {
        OperationsController->NotifyWeaponFired();
    }
    LastApacheCannonFireTime = GetWorld()->GetTimeSeconds();
    ApacheCannonHeat = FMath::Min(100.0f, ApacheCannonHeat + 2.6f);
    if (ApacheCannonHeat >= 99.9f)
    {
        bApacheCannonOverheated = true;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_WEAPON|APACHE_30MM|state=OVERHEATED|ammo=%d"), ApacheCannonAmmo);
    }

    StartApacheCannonAudio(TEXT("TRIGGER_SHOT"));
    if (ApacheCannonAmmo <= 0)
    {
        StopApacheCannonAudio(TEXT("AMMO_EMPTY"));
    }
    else if (bApacheCannonOverheated)
    {
        StopApacheCannonAudio(TEXT("OVERHEATED"));
    }
    PlayerController->PlayDynamicForceFeedback(0.34f, 0.055f, true, true, true, true);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_WEAPON|APACHE_30MM|state=FIRED|ammo=%d|heat=%.1f|blocked=%d|muzzles=%d|model_stations=%d"),
        ApacheCannonAmmo,
        ApacheCannonHeat,
        bBlockingHit ? 1 : 0,
        MuzzleCount,
        bUsingModelWeaponStations ? 1 : 0);
}

void ARotorlineHelicopterPawn::StartApacheCannonAudio(const TCHAR* Reason)
{
    if (!ApacheCannonSound || !ApacheCannonAudio)
    {
        return;
    }

    // The source recording is a sustained burst rather than a one-shot sample.
    // Cancel any pending release fade before restarting so rapid L1 taps always
    // begin cleanly and never overlap the tail of the previous 7.6 s clip.
    const bool bClipRestart = bApacheCannonAudioTriggerActive && !ApacheCannonAudio->IsPlaying();
    if (bApacheCannonAudioTriggerActive && ApacheCannonAudio->IsPlaying())
    {
        return;
    }
    ApacheCannonAudio->Stop();
    ApacheCannonAudio->SetSound(ApacheCannonSound);
    ApacheCannonAudio->SetVolumeMultiplier(
        0.72f * GetAudioMix(ERotorlineAudioChannel::WeaponsExplosions) *
        (IsSpokenDialogueActive() ? 0.18f : 1.0f));
    // The supplied sustained burst contains roughly 100 ms of digital silence
    // at its head. Start just past it so quick L1 taps still produce an audible
    // first report before trigger-release fading begins.
    constexpr float CannonAudibleStartSeconds = 0.12f;
    ApacheCannonAudio->Play(CannonAudibleStartSeconds);
    bApacheCannonAudioTriggerActive = true;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_WEAPON_AUDIO|APACHE_30MM|state=START|reason=%s|clip_restart=%d|weapons_mix=%.3f"),
        Reason,
        bClipRestart ? 1 : 0,
        GetAudioMix(ERotorlineAudioChannel::WeaponsExplosions));
}

void ARotorlineHelicopterPawn::StopApacheCannonAudio(const TCHAR* Reason, float FadeOutSeconds)
{
    if (!ApacheCannonAudio)
    {
        bApacheCannonAudioTriggerActive = false;
        return;
    }

    const bool bWasTriggerActive = bApacheCannonAudioTriggerActive;
    if (!bWasTriggerActive)
    {
        // EndPlay and aircraft reconfiguration must also terminate a release
        // fade that may still be alive after the trigger state was cleared.
        if (FadeOutSeconds <= 0.0f && ApacheCannonAudio->IsPlaying())
        {
            ApacheCannonAudio->Stop();
        }
        return;
    }

    bApacheCannonAudioTriggerActive = false;
    if (ApacheCannonAudio->IsPlaying())
    {
        if (FadeOutSeconds > 0.0f)
        {
            ApacheCannonAudio->FadeOut(FadeOutSeconds, 0.0f);
        }
        else
        {
            ApacheCannonAudio->Stop();
        }
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_WEAPON_AUDIO|APACHE_30MM|state=STOP|reason=%s|fade_ms=%.0f"),
        Reason,
        FMath::Max(0.0f, FadeOutSeconds) * 1000.0f);
}

void ARotorlineHelicopterPawn::FireEnemyShot(
    ARotorlineMissionObjectiveActor* Site,
    ERotorlineEnemyWeaponType WeaponType,
    float Damage)
{
    if (!GetWorld() || !IsValid(Site) || Site->IsDestroyedTarget()) return;
    if (bStealthActive)
    {
        return;
    }
    if (ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(Controller))
    {
        OperationsController->NotifyEnemyFire(3.0f);
        OperationsController->NotifyDetection(3.0f);
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Site;
    const FVector MuzzleLocation = Site->GetMuzzleLocation();
    if (WeaponType == ERotorlineEnemyWeaponType::ArtilleryRocket &&
        Site->GetThreatType() == ERotorlineThreatType::RocketArtillery)
    {
        const FVector LaunchDirection = Site->GetWeaponAimDirection();
        ARotorlineRocketProjectile* EnemyRocket = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
            ARotorlineRocketProjectile::StaticClass(), MuzzleLocation, LaunchDirection.Rotation(), SpawnParams);
        if (EnemyRocket)
        {
            Site->NotifyWeaponFired(WeaponType);
            EnemyRocket->LaunchEnemyArtillery(MuzzleLocation, LaunchDirection, this, Damage);
        }
        return;
    }
    if (WeaponType == ERotorlineEnemyWeaponType::GuidedMissile &&
        Site->GetThreatType() == ERotorlineThreatType::RadarMissile)
    {
        // The HAWK launcher assembly owns the launch vector. Spawn and boost
        // down the tracked rail direction instead of the trailer chassis yaw.
        const FVector LaunchDirection = Site->GetWeaponAimDirection();
        ARotorlineRocketProjectile* EnemyMissile = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
            ARotorlineRocketProjectile::StaticClass(), MuzzleLocation, LaunchDirection.Rotation(), SpawnParams);
        if (EnemyMissile)
        {
            Site->NotifyWeaponFired(WeaponType);
            EnemyMissile->LaunchEnemyAirDefense(MuzzleLocation, LaunchDirection, this, Damage);
        }
        return;
    }
    if (WeaponType == ERotorlineEnemyWeaponType::GuidedMissile &&
        Site->GetThreatType() == ERotorlineThreatType::RocketGunship)
    {
        ARotorlineRocketProjectile* EnemyRocket = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
            ARotorlineRocketProjectile::StaticClass(), MuzzleLocation, Site->GetActorForwardVector().Rotation(), SpawnParams);
        if (EnemyRocket)
        {
            Site->NotifyWeaponFired(WeaponType);
            const FVector InitialDirection = (GetActorLocation() - MuzzleLocation).GetSafeNormal();
            EnemyRocket->LaunchEnemy(MuzzleLocation, InitialDirection, this, Damage);
        }
        return;
    }
    ARotorlineEnemyProjectile* Projectile = GetWorld()->SpawnActor<ARotorlineEnemyProjectile>(
        ARotorlineEnemyProjectile::StaticClass(), MuzzleLocation, FRotator::ZeroRotator, SpawnParams);
    if (Projectile)
    {
        Site->NotifyWeaponFired(WeaponType);
        const bool bGroundDirectFire = WeaponType == ERotorlineEnemyWeaponType::TankShell ||
            WeaponType == ERotorlineEnemyWeaponType::Flak;
        const FVector InitialDirection = bGroundDirectFire
            ? Site->GetWeaponAimDirection()
            : FVector::ZeroVector;
        Projectile->Launch(
            MuzzleLocation,
            this,
            Damage,
            WeaponType,
            InitialDirection);
    }
}

void ARotorlineHelicopterPawn::ResetCombatThreatState()
{
    if (ThreatAlertAudio) ThreatAlertAudio->Stop();
    BurstFireSite = nullptr;
    PendingMissileSite = nullptr;
    BurstShotsRemaining = 0;
    BurstDamage = 0.0f;
    bPendingHimarsLockedAlert = false;
    bPendingAircraftLockedAlert = false;
    NextBurstShotTime = -1000.0;
    PendingMissileLaunchTime = -1000.0;
}

void ARotorlineHelicopterPawn::UpdateMissionCombat(float DistanceMeters)
{
    (void)DistanceMeters;
    if (!GetWorld()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bEnemyFlightTestMode &&
        Now - MissionStartTime < RotorlineHelicopter::OpeningCombatGraceSeconds)
    {
        ResetCombatThreatState();
        return;
    }

    const float TerrainMaskHeight = ActiveMission.Id == TEXT("evacuation") ? 22.0f : 8.0f;
    // This is a hard ceasefire zone, independent of where a defense actor was
    // spawned. It protects startup and departure from mission and network
    // threats alike.
    const bool bBellLairMission = IsBell222SpecialOperations() &&
        ActiveMission.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase);
    const FVector HomeSanctuaryLocation = bBellLairMission
        ? FVector(RotorlineSupportLocations::BellLairPeak.X, RotorlineSupportLocations::BellLairPeak.Y, 0.0f)
        : FVector(RotorlineHelicopter::SpawnX, RotorlineHelicopter::SpawnY, 0.0f);
    const bool bAtHomeSanctuary = FVector::Dist2D(
        GetActorLocation(), HomeSanctuaryLocation) <
        RotorlineCombatTuning::HomeSanctuaryRadiusCm;
    if (bAtHomeSanctuary != bHomeSanctuaryActive)
    {
        bHomeSanctuaryActive = bAtHomeSanctuary;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COMBAT|HOME_SANCTUARY|state=%s|radius_m=1100|distance_m=%.0f"),
            bAtHomeSanctuary ? TEXT("ENTER") : TEXT("EXIT"),
            FVector::Dist2D(GetActorLocation(), HomeSanctuaryLocation) / 100.0f);
    }
    const float CityServiceDistanceCm = FVector::Dist2D(
        GetActorLocation(), RotorlineSupportLocations::CentralTownRearmPad);
    const bool bAtCityServiceSanctuary =
        CityServiceDistanceCm < RotorlineSupportLocations::CityServiceSanctuaryRadiusCm;
    if (bAtCityServiceSanctuary != bCityServiceSanctuaryActive)
    {
        bCityServiceSanctuaryActive = bAtCityServiceSanctuary;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COMBAT|CITY_SERVICE_SANCTUARY|state=%s|radius_m=120|distance_m=%.0f"),
            bAtCityServiceSanctuary ? TEXT("ENTER") : TEXT("EXIT"),
            CityServiceDistanceCm / 100.0f);
    }
    const bool bAtObjectiveSanctuary = ActiveMission.Objectives.IsValidIndex(CurrentObjectiveIndex) &&
        (ActiveMission.Objectives[CurrentObjectiveIndex].Kind == TEXT("land") || ActiveMission.Objectives[CurrentObjectiveIndex].Kind == TEXT("interact")) &&
        FVector::Dist2D(GetActorLocation(), CurrentObjectiveWorldLocation) < 12500.0f;
    const bool bPlayerProtectedBySanctuary = !bEnemyFlightTestMode &&
        (GetAboveGroundMeters() < TerrainMaskHeight || bAtHomeSanctuary ||
            bAtCityServiceSanctuary || bAtObjectiveSanctuary);
    if (bPlayerProtectedBySanctuary)
    {
        if (PendingMissileSite || BurstFireSite)
        {
            ResetCombatThreatState();
        }
        return;
    }

    if (PendingMissileSite)
    {
        if (!IsValid(PendingMissileSite) || PendingMissileSite->IsDestroyedTarget() ||
            !PendingMissileSite->HasCurrentWeaponSolution())
        {
            ResetCombatThreatState();
        }
        else if (Now >= PendingMissileLaunchTime)
        {
            const bool bRadarMissileLaunch = PendingMissileSite->GetThreatType() ==
                ERotorlineThreatType::RadarMissile;
            const bool bHawkRidgeLaunch = PendingMissileSite->GetTargetLabel().Contains(
                TEXT("MIM-23 HAWK"), ESearchCase::IgnoreCase);
            // Threat tones and the physical missile launch are combat audio,
            // not radio chatter. Keep them active for every launch.
            PlayThreatAlert(RadarLockedSound ? RadarLockedSound.Get() : ThreatLockedSound.Get(), 0.44f);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_MISSILE_ALERT_AUDIO|phase=LOCKED|asset=SFX_RadarLocked|threat=%s|launch=IMMINENT"),
                bRadarMissileLaunch ? TEXT("RADAR_GUIDED") : TEXT("AIRCRAFT_MISSILE"));
            const float MissileDamage = PendingMissileSite->GetThreatType() == ERotorlineThreatType::RadarMissile ? 7.0f : 6.0f;
            FireEnemyShot(PendingMissileSite, ERotorlineEnemyWeaponType::GuidedMissile, MissileDamage);
            if (bHawkRidgeLaunch)
            {
                bHawkRidgeQualificationLaunchObserved = true;
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_HAWK_RIDGE|MISSILE_LAUNCH|site=%s|range_m=%.1f|target_aircraft=%s"),
                    *PendingMissileSite->GetTargetLabel(),
                    FVector::Dist(GetActorLocation(), PendingMissileSite->GetAimLocation()) / 100.0f,
                    *SelectedAircraftId);
            }
            if (!bRadarMissileLaunch)
            {
                BroadcastRadio(TEXT("GUNNER: Missile away! Break hard and get behind terrain!"), 4.5f, false);
            }
            else
            {
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_RADAR_AUDIO|event=MISSILE_LAUNCH|radio=SILENT|threat_tone=ACTIVE|physical_missile_audio=ACTIVE"));
            }
            PendingMissileSite = nullptr;
            PendingMissileLaunchTime = -1000.0;
        }
        return;
    }

    if (BurstShotsRemaining > 0)
    {
        if (!IsValid(BurstFireSite) || BurstFireSite->IsDestroyedTarget() ||
            !BurstFireSite->HasCurrentWeaponSolution())
        {
            BurstFireSite = nullptr;
            BurstShotsRemaining = 0;
            bPendingHimarsLockedAlert = false;
            bPendingAircraftLockedAlert = false;
            return;
        }
        if (Now >= NextBurstShotTime)
        {
            const ERotorlineEnemyWeaponType WeaponType = static_cast<ERotorlineEnemyWeaponType>(BurstWeaponType);
            if (WeaponType == ERotorlineEnemyWeaponType::ArtilleryRocket && bPendingHimarsLockedAlert)
            {
                bPendingHimarsLockedAlert = false;
                PlayThreatAlert(RadarLockedSound ? RadarLockedSound.Get() : ThreatLockedSound.Get(), 0.44f);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_MISSILE_ALERT_AUDIO|phase=LOCKED|asset=SFX_RadarLocked|threat=HIMARS|launch=IMMINENT"));
            }
            if ((WeaponType == ERotorlineEnemyWeaponType::AutoCannon ||
                    WeaponType == ERotorlineEnemyWeaponType::MachineGun) &&
                bPendingAircraftLockedAlert)
            {
                bPendingAircraftLockedAlert = false;
                PlayThreatAlert(RadarLockedSound ? RadarLockedSound.Get() : ThreatLockedSound.Get(), 0.40f);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_MISSILE_ALERT_AUDIO|phase=LOCKED|asset=SFX_RadarLocked|threat=ENEMY_AIRCRAFT|attack=GUN_RUN"));
            }
            FireEnemyShot(BurstFireSite, WeaponType, BurstDamage);
            --BurstShotsRemaining;
            NextBurstShotTime = Now + (WeaponType == ERotorlineEnemyWeaponType::AutoCannon ? 0.085 :
                (WeaponType == ERotorlineEnemyWeaponType::MachineGun ? 0.11 :
                (WeaponType == ERotorlineEnemyWeaponType::MortarShell ? 0.90 :
                (WeaponType == ERotorlineEnemyWeaponType::ArtilleryRocket ? 0.55 : 0.16))));
            if (BurstShotsRemaining <= 0) BurstFireSite = nullptr;
        }
        return;
    }

    ARotorlineMissionObjectiveActor* FiringSite = nullptr;
    float NearestMeters = TNumericLimits<float>::Max();
    const auto AttackIntervalFor = [&](ERotorlineThreatType Type)
    {
        float Interval = 1.25f;
        switch (Type)
        {
        case ERotorlineThreatType::Tank: Interval = 4.4f; break;
        case ERotorlineThreatType::RocketArtillery: Interval = 10.5f; break;
        case ERotorlineThreatType::RadarMissile: Interval = 11.0f; break;
        case ERotorlineThreatType::MachineGunship: Interval = 1.35f; break;
        case ERotorlineThreatType::RocketGunship: Interval = 1.75f; break;
        default: break;
        }
        return FMath::Max(0.85f, Interval - ActiveMission.Difficulty * 0.08f);
    };
    const auto ConsiderSite = [&](ARotorlineMissionObjectiveActor* Site)
    {
        if (Site == TransitThreatActor && bTransitThreatHarmless) return;
        if (!IsValid(Site) || !Site->IsDestroyObjective() || Site->IsDestroyedTarget() ||
            Site->GetThreatType() == ERotorlineThreatType::None) return;
        if (bStealthActive) return;
        const bool bMortarSite = Site->GetThreatType() == ERotorlineThreatType::RocketArtillery &&
            Site->GetTargetLabel().Contains(TEXT("mortar"), ESearchCase::IgnoreCase);
        const bool bReconFlakSite = Site->GetThreatType() == ERotorlineThreatType::Flak &&
            Site->GetTargetLabel().Contains(TEXT("recon-flak"), ESearchCase::IgnoreCase);
        const FVector SiteTargetLocation = Site->GetCurrentWeaponTargetLocation();
        if (SiteTargetLocation.IsNearlyZero()) return;
        const float SiteDistanceMeters = FVector::Dist(
            Site->GetMuzzleLocation(), SiteTargetLocation) / 100.0f;
        float MaximumRange = RotorlineCombatTuning::FlakRangeMeters;
        switch (Site->GetThreatType())
        {
        case ERotorlineThreatType::Flak: MaximumRange = bReconFlakSite
            ? RotorlineCombatTuning::ReconFlakRangeMeters
            : RotorlineCombatTuning::FlakRangeMeters; break;
        case ERotorlineThreatType::Tank: MaximumRange = RotorlineCombatTuning::TankRangeMeters; break;
        case ERotorlineThreatType::RocketArtillery: MaximumRange = bMortarSite
            ? 900.0f : RotorlineCombatTuning::RocketArtilleryRangeMeters; break;
        case ERotorlineThreatType::RadarMissile: MaximumRange = RotorlineCombatTuning::RadarMissileRangeMeters; break;
        case ERotorlineThreatType::MachineGunship: MaximumRange = RotorlineCombatTuning::MachineGunshipRangeMeters; break;
        case ERotorlineThreatType::RocketGunship: MaximumRange = RotorlineCombatTuning::RocketGunshipRangeMeters; break;
        default: break;
        }
        // Distance and cadence are cheap. Reject those first so far-away M25
        // gauntlet actors cannot issue synchronous line-of-sight traces every
        // frame while the player is still several route sectors away.
        if (SiteDistanceMeters > MaximumRange || SiteDistanceMeters >= NearestMeters) return;
        const double LastSiteAttack = LastThreatAttackTimes.FindRef(Site);
        const float SiteAttackInterval = bMortarSite
            ? FMath::Max(6.5f, 8.5f - ActiveMission.Difficulty * 0.15f)
            : AttackIntervalFor(Site->GetThreatType());
        if (Now - LastSiteAttack < SiteAttackInterval || !Site->HasCurrentWeaponSolution()) return;
        FiringSite = Site;
        NearestMeters = SiteDistanceMeters;
    };
    ConsiderSite(ActiveObjectiveActor);
    ConsiderSite(TransitThreatActor);
    for (TActorIterator<ARotorlineMissionObjectiveActor> It(GetWorld()); It; ++It)
    {
        if (*It == ActiveObjectiveActor || *It == TransitThreatActor) continue;
        ConsiderSite(*It);
    }
    if (!FiringSite) return;

    const bool bLimitedSupportContact =
        FiringSite == TransitThreatActor &&
        (!ActiveMission.bRequiresWeapons ||
            (SelectedCraft == ERotorlineCraftType::SupportHuey && ActiveMission.Difficulty <= 2));
    if (bLimitedSupportContact && TransitThreatAttackPasses >= 4)
    {
        return;
    }

    if (Now - LastEnemyAttackTime < 0.45) return;
    LastEnemyAttackTime = Now;
    LastThreatAttackTimes.Add(FiringSite, Now);
    if (bLimitedSupportContact)
    {
        ++TransitThreatAttackPasses;
    }

    if (FiringSite->GetThreatType() == ERotorlineThreatType::RadarMissile)
    {
        PendingMissileSite = FiringSite;
        PendingMissileLaunchTime = Now + 3.45;
        if (FiringSite->GetTargetLabel().Contains(TEXT("MIM-23 HAWK"), ESearchCase::IgnoreCase))
        {
            bHawkRidgeQualificationLockObserved = true;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_HAWK_RIDGE|RADAR_LOCK|site=%s|range_m=%.1f|launch_delay_s=3.45|target_aircraft=%s"),
                *FiringSite->GetTargetLabel(),
                NearestMeters,
                *SelectedAircraftId);
        }
        // Preserve the non-verbal radar warning on every lock. Only spoken
        // radio chatter is de-duplicated below.
        PlayThreatAlert(RadarHomingSound ? RadarHomingSound.Get() : ThreatWarningSound.Get(), 0.38f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSILE_ALERT_AUDIO|phase=HOMING_IN|asset=SFX_RadarHomingIn|threat=RADAR_GUIDED|lock_delay_s=3.45"));
        if (!bRadarLockCalloutPlayed && !IsMissionRadioHoldActive())
        {
            bRadarLockCalloutPlayed = true;
            BroadcastRadio(TEXT("COMMAND: Radar lock. Use the terrain before it launches."), 4.5f, false);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_RADAR_AUDIO|event=FIRST_LOCK|state=PLAYED|scope=SORTIE"));
        }
        else
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_RADAR_AUDIO|event=RADAR_LOCK|state=SILENT|reason=%s"),
                IsMissionRadioHoldActive() ? TEXT("MISSION_RADIO_HOLD") : TEXT("FIRST_LOCK_ALREADY_PLAYED"));
        }
        return;
    }

    if (FiringSite->GetThreatType() == ERotorlineThreatType::RocketGunship)
    {
        int32& AttackSequence = ThreatAttackSequences.FindOrAdd(FiringSite);
        const bool bRocketAttack = (AttackSequence++ % 3) == 0;
        if (bRocketAttack)
        {
            PendingMissileSite = FiringSite;
            PendingMissileLaunchTime = Now + 1.65;
            PlayThreatAlert(RadarHomingSound ? RadarHomingSound.Get() : ThreatWarningSound.Get(), 0.32f);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_MISSILE_ALERT_AUDIO|phase=HOMING_IN|asset=SFX_RadarHomingIn|threat=AIRCRAFT_MISSILE|lock_delay_s=1.65"));
            const bool bHindRocketLaunch = FiringSite->GetTargetLabel().Contains(
                TEXT("HIND"), ESearchCase::IgnoreCase);
            const FString RocketLaunchCallout = bHindRocketLaunch
                ? TEXT("GUNNER: HIND rocket launch! Break and use terrain!")
                : TEXT("GUNNER: Apache rocket launch! Break and use terrain!");
            BroadcastRadio(RocketLaunchCallout, 4.0f, false);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("ROTORLINE_ENEMY_ROCKET_CALLOUT|aircraft=%s|route=%s"),
                *FiringSite->GetTargetLabel(),
                bHindRocketLaunch ? TEXT("HIND") : TEXT("APACHE"));
            return;
        }
        BurstFireSite = FiringSite;
        NextBurstShotTime = Now + 1.0;
        BurstWeaponType = static_cast<uint8>(ERotorlineEnemyWeaponType::AutoCannon);
        BurstShotsRemaining = 7;
        BurstDamage = 1.6f;
        bPendingAircraftLockedAlert = true;
        PlayThreatAlert(RadarHomingSound ? RadarHomingSound.Get() : ThreatWarningSound.Get(), 0.30f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSILE_ALERT_AUDIO|phase=HOMING_IN|asset=SFX_RadarHomingIn|threat=ENEMY_AIRCRAFT|attack=GUN_RUN|lock_delay_s=1.0"));
        const bool bHindGunRun = FiringSite->GetTargetLabel().Contains(
            TEXT("HIND"), ESearchCase::IgnoreCase);
        const FString GunRunCallout = bHindGunRun
            ? TEXT("GUNNER: Gunship firing! Keep moving!")
            : TEXT("GUNNER: Apache cannon! Jink now!");
        BroadcastRadio(GunRunCallout, 3.2f, false);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_ENEMY_CANNON_CALLOUT|aircraft=%s|route=%s"),
            *FiringSite->GetTargetLabel(),
            bHindGunRun ? TEXT("GENERIC_GUNSHIP") : TEXT("APACHE"));
        return;
    }

    if (FiringSite->GetThreatType() == ERotorlineThreatType::RocketArtillery)
    {
        if (FiringSite->GetTargetLabel().Contains(TEXT("mortar"), ESearchCase::IgnoreCase))
        {
            BurstFireSite = FiringSite;
            NextBurstShotTime = Now;
            BurstWeaponType = static_cast<uint8>(ERotorlineEnemyWeaponType::MortarShell);
            BurstShotsRemaining = 2;
            BurstDamage = 4.0f;
            PlayThreatAlert(ThreatWarningSound, 0.22f);
            BroadcastRadio(TEXT("CREW: Mortars ranging us. Keep moving through the lock!"), 4.0f, false);
            return;
        }
        BurstFireSite = FiringSite;
        NextBurstShotTime = Now + 1.65;
        BurstWeaponType = static_cast<uint8>(ERotorlineEnemyWeaponType::ArtilleryRocket);
        BurstShotsRemaining = 3;
        BurstDamage = 4.5f;
        bPendingHimarsLockedAlert = true;
        PlayThreatAlert(RadarHomingSound ? RadarHomingSound.Get() : ThreatWarningSound.Get(), 0.34f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSILE_ALERT_AUDIO|phase=HOMING_IN|asset=SFX_RadarHomingIn|threat=HIMARS|lock_delay_s=1.65"));
        BroadcastRadio(TEXT("COMMAND: HIMARS salvo inbound! Break low and put terrain between you and the launcher!"), 5.0f, false);
        return;
    }

    BurstFireSite = FiringSite;
    NextBurstShotTime = Now;
    if (FiringSite->GetThreatType() == ERotorlineThreatType::MachineGunship)
    {
        NextBurstShotTime = Now + 1.0;
        BurstWeaponType = static_cast<uint8>(ERotorlineEnemyWeaponType::MachineGun);
        BurstShotsRemaining = 9;
        BurstDamage = bLimitedSupportContact ? 0.7f : 1.0f;
        bPendingAircraftLockedAlert = true;
        PlayThreatAlert(RadarHomingSound ? RadarHomingSound.Get() : ThreatWarningSound.Get(), 0.30f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSILE_ALERT_AUDIO|phase=HOMING_IN|asset=SFX_RadarHomingIn|threat=ENEMY_AIRCRAFT|attack=GUN_RUN|lock_delay_s=1.0"));
    }
    else if (FiringSite->GetThreatType() == ERotorlineThreatType::Tank)
    {
        BurstWeaponType = static_cast<uint8>(ERotorlineEnemyWeaponType::TankShell);
        BurstShotsRemaining = 1;
        BurstDamage = 8.0f;
    }
    else
    {
        BurstWeaponType = static_cast<uint8>(ERotorlineEnemyWeaponType::Flak);
        BurstShotsRemaining = 2;
        BurstDamage = 4.0f;
    }
    if (Now - LastRadioChatterTime > 8.0)
    {
        BroadcastRadio(FiringSite->GetThreatType() == ERotorlineThreatType::MachineGunship
            ? TEXT("GUNNER: Gunship firing! Keep moving!")
            : TEXT("GUNNER: Incoming fire. Break now!"), 4.5f);
    }
}

void ARotorlineHelicopterPawn::ApplyEnemyProjectileHit(float Damage, const FVector& ImpactImpulse)
{
    if (bMissionFailed || bMissionComplete) return;
    if (bCombatPreviewMode)
    {
        Damage *= 0.18f;
    }
    if (!ImpactImpulse.IsNearlyZero())
    {
        const FVector ClampedImpulse = ImpactImpulse.GetClampedToMaxSize(1400.0f);
        CurrentVelocity += ClampedImpulse;
        const FVector ImpulseDirection = ClampedImpulse.GetSafeNormal();
        CurrentRollAngle = FMath::Clamp(
            CurrentRollAngle + FVector::DotProduct(ImpulseDirection, GetActorRightVector()) * 8.0f,
            -MaxRollAngle,
            MaxRollAngle);
        CurrentPitchAngle = FMath::Clamp(
            CurrentPitchAngle - FVector::DotProduct(ImpulseDirection, GetActorForwardVector()) * 5.0f,
            -MaxPitchAngle,
            MaxPitchAngle);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_IMPACT_PHYSICS|result=MISSILE_IMPULSE|mps=%.1f"),
            ClampedImpulse.Size() / 100.0f);
    }
    const float HealthBefore = CurrentHealth;
    CurrentHealth = FMath::Max(0.0f, CurrentHealth - Damage);
    if (ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(Controller))
    {
        OperationsController->NotifyDamageTaken(HealthBefore - CurrentHealth);
    }
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        PlayerController->PlayDynamicForceFeedback(0.58f, 0.14f, true, true, true, true);
    }
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(7113, 1.4f, FColor::Red, FString::Printf(TEXT("ENEMY FIRE  //  -%.0f HULL  //  %.0f REMAINING"), Damage, CurrentHealth));
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_COMBAT|HIT|damage=%.1f|health=%.1f"), Damage, CurrentHealth);
    if (CurrentHealth <= 0.0f)
    {
        FailMission(TEXT("AIRCRAFT LOST"));
    }
}

void ARotorlineHelicopterPawn::HandleEnemyAircraftCollision(
    const FString& EnemyAirframe,
    float RelativeImpactSpeed)
{
    if (bMissionFailed || bMissionComplete || bPlayerAircraftDying)
    {
        return;
    }

    const float HealthBefore = CurrentHealth;
    CurrentHealth = 0.0f;
    if (ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(Controller))
    {
        OperationsController->NotifyDamageTaken(HealthBefore);
    }
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        PlayerController->PlayDynamicForceFeedback(1.0f, 0.35f, true, true, true, true);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("ROTORLINE_PLAYER_COLLISION|enemy=%s|relative_mps=%.1f|result=FATAL_MIDAIR"),
        *EnemyAirframe,
        RelativeImpactSpeed / 100.0f);
    FailMission(
        FString::Printf(TEXT("MID-AIR COLLISION // %s"), *EnemyAirframe),
        true);
}

void ARotorlineHelicopterPawn::FailMission(const FString& Reason, bool bDestroyPlayerAircraft)
{
    if (bMissionFailed || bMissionComplete)
    {
        return;
    }
    bMissionFailed = true;
    MissionFailureReason = Reason;
    if (InstructorAudio) InstructorAudio->Stop();
    ResetCombatThreatState();
    ClearFinalMissionSetPieces();
    if (bDestroyPlayerAircraft && IsValid(ActiveCabinSupplyConvoy))
    {
        ActiveCabinSupplyConvoy->Destroy();
        ActiveCabinSupplyConvoy = nullptr;
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_MISSION|mission=%s|state=FAILED|reason=%s"), *ActiveMission.Id, *Reason);
    if (bDestroyPlayerAircraft)
    {
        BeginPlayerDestruction();
    }
    if (GEngine)
    {
        const FString FailureMessage = bDestroyPlayerAircraft
            ? FString::Printf(TEXT("MISSION FAILED  //  %s  //  AIRCRAFT DESTROYED - BRACE FOR IMPACT"), *Reason)
            : FString::Printf(TEXT("MISSION FAILED  //  %s"), *Reason);
        GEngine->AddOnScreenDebugMessage(7110, 12.0f, FColor::Red, FailureMessage);
    }
}

void ARotorlineHelicopterPawn::ApplyMissionConditions()
{
    MissionWindAcceleration = FVector::ZeroVector;
    const FString Weather = ActiveMission.Weather.ToLower();
    const FString TimeOfDay = ActiveMission.TimeOfDay.ToLower();
    bNightOperationLightsEnabled = TimeOfDay == TEXT("night") ||
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineCityRearmPreview"));

    Camera->PostProcessBlendWeight = 1.0f;
    FPostProcessSettings& Settings = Camera->PostProcessSettings;
    Settings.bOverride_ColorSaturation = true;
    Settings.ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    Settings.bOverride_SceneColorTint = true;
    Settings.SceneColorTint = FLinearColor::White;
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = 0.0f;
    MissionFog->SetVisibility(false, true);
    // Capture a stable deployment-level datum once. Sea-level anchoring placed
    // exponential density beneath much of the elevated island, while attaching
    // it to the aircraft made density change during climbs.
    const float FogAnchorZ = GetActorLocation().Z - 150.0f;
    MissionFog->SetWorldLocation(FVector(0.0f, 0.0f, FogAnchorZ));
    MissionFog->SetFogDensity(0.0f);
    MissionFog->SetVolumetricFog(false);
    ConfigureWeatherPrecipitation(false);

    // Day sorties use a readable overcast atmosphere instead of the previous
    // hard, cloudless presentation. This is genuine volumetric height fog,
    // not a HUD overlay, and leaves enough range for navigation and combat.
    if (TimeOfDay != TEXT("night"))
    {
        Settings.ColorSaturation *= FVector4(0.88f, 0.90f, 0.92f, 1.0f);
        Settings.SceneColorTint *= FLinearColor(0.82f, 0.87f, 0.90f);
        Settings.AutoExposureBias -= 0.32f;
        MissionFog->SetVisibility(true, true);
        MissionFog->SetFogDensity(0.045f);
        MissionFog->SetFogHeightFalloff(0.085f);
        MissionFog->SetFogInscatteringColor(FLinearColor(0.58f, 0.64f, 0.67f));
        MissionFog->SetFogMaxOpacity(0.82f);
        MissionFog->SetStartDistance(700.0f);
        MissionFog->SetVolumetricFog(true);
        MissionFog->SetVolumetricFogScatteringDistribution(0.22f);
        MissionFog->SetVolumetricFogAlbedo(FColor(190, 200, 202));
        MissionFog->SetVolumetricFogExtinctionScale(0.78f);
        MissionFog->SetVolumetricFogDistance(18000.0f);
        MissionFog->SetVolumetricFogStartDistance(350.0f);
        MissionFog->SetVolumetricFogNearFadeInDistance(300.0f);
    }

    if (TimeOfDay == TEXT("night"))
    {
        Settings.ColorSaturation = FVector4(0.62f, 0.72f, 1.0f, 1.0f);
        Settings.SceneColorTint = FLinearColor(0.30f, 0.42f, 0.72f);
        Settings.AutoExposureBias = -1.4f;
        MissionFog->SetVisibility(true, true);
        MissionFog->SetFogDensity(0.012f);
        MissionFog->SetFogHeightFalloff(0.16f);
        MissionFog->SetFogInscatteringColor(FLinearColor(0.12f, 0.18f, 0.28f));
        MissionFog->SetFogMaxOpacity(0.68f);
        MissionFog->SetStartDistance(1800.0f);
        MissionFog->SetVolumetricFog(true);
        MissionFog->SetVolumetricFogScatteringDistribution(0.32f);
        MissionFog->SetVolumetricFogAlbedo(FColor(105, 125, 155));
        MissionFog->SetVolumetricFogExtinctionScale(0.42f);
        MissionFog->SetVolumetricFogDistance(15000.0f);
        MissionFog->SetVolumetricFogStartDistance(900.0f);
        MissionFog->SetVolumetricFogNearFadeInDistance(500.0f);
    }
    else if (TimeOfDay == TEXT("sunset"))
    {
        Settings.ColorSaturation = FVector4(1.05f, 0.92f, 0.80f, 1.0f);
        Settings.SceneColorTint = FLinearColor(1.0f, 0.82f, 0.66f);
        Settings.AutoExposureBias = 0.05f;
    }

    if (Weather == TEXT("wind"))
    {
        MissionWindAcceleration = FVector(42.0f, 66.0f, 0.0f);
    }
    else if (Weather == TEXT("rain"))
    {
        MissionWindAcceleration = FVector(18.0f, 32.0f, -8.0f);
        Settings.ColorSaturation *= FVector4(0.72f, 0.78f, 0.82f, 1.0f);
        Settings.SceneColorTint *= FLinearColor(0.55f, 0.65f, 0.74f);
        Settings.AutoExposureBias -= 0.45f;
        MissionFog->SetVisibility(true, true);
        MissionFog->SetFogDensity(0.018f);
        MissionFog->SetFogHeightFalloff(0.20f);
        MissionFog->SetFogInscatteringColor(FLinearColor(0.20f, 0.27f, 0.34f));
        MissionFog->SetStartDistance(2200.0f);
        MissionFog->SetVolumetricFog(true);
        MissionFog->SetVolumetricFogScatteringDistribution(0.35f);
        MissionFog->SetVolumetricFogAlbedo(FColor(150, 170, 185));
        MissionFog->SetVolumetricFogExtinctionScale(0.65f);
        MissionFog->SetVolumetricFogDistance(14000.0f);
        MissionFog->SetVolumetricFogStartDistance(900.0f);
        MissionFog->SetVolumetricFogNearFadeInDistance(500.0f);
        // Keep the scene response active, but do not expose an unqualified
        // precipitation renderer in Shipping. Rain must use a production
        // Niagara asset with a proper streak texture and collision/splashes.
    }
    else if (Weather == TEXT("fog"))
    {
        MissionWindAcceleration = FVector(8.0f, 12.0f, 0.0f);
        Settings.ColorSaturation *= FVector4(0.78f, 0.82f, 0.86f, 1.0f);
        Settings.SceneColorTint *= FLinearColor(0.72f, 0.78f, 0.80f);
        Settings.AutoExposureBias -= 0.25f;
        MissionFog->SetVisibility(true, true);
        MissionFog->SetFogDensity(0.082f);
        MissionFog->SetFogHeightFalloff(0.055f);
        MissionFog->SetFogInscatteringColor(FLinearColor(0.48f, 0.55f, 0.58f));
        MissionFog->SetFogMaxOpacity(0.92f);
        MissionFog->SetStartDistance(0.0f);
        MissionFog->SetVolumetricFog(true);
        MissionFog->SetVolumetricFogScatteringDistribution(0.15f);
        MissionFog->SetVolumetricFogAlbedo(FColor(185, 195, 195));
        MissionFog->SetVolumetricFogExtinctionScale(1.10f);
        MissionFog->SetVolumetricFogDistance(14000.0f);
        MissionFog->SetVolumetricFogStartDistance(0.0f);
        MissionFog->SetVolumetricFogNearFadeInDistance(250.0f);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_MISSION_CONDITIONS|weather=%s|time=%s|wind=%.1f,%.1f,%.1f"),
        *ActiveMission.Weather,
        *ActiveMission.TimeOfDay,
        MissionWindAcceleration.X,
        MissionWindAcceleration.Y,
        MissionWindAcceleration.Z);
    const IConsoleVariable* VolumetricFogCVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricFog"));
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_FOG|weather=%s|time=%s|visible=%d|density=%.4f|height_falloff=%.3f|start_cm=%.0f|volumetric_component=%d|volumetric_cvar=%d|world_z=%.1f"),
        *ActiveMission.Weather, *ActiveMission.TimeOfDay,
        MissionFog->IsVisible() ? 1 : 0,
        MissionFog->FogDensity, MissionFog->FogHeightFalloff, MissionFog->StartDistance,
        MissionFog->bEnableVolumetricFog ? 1 : 0,
        VolumetricFogCVar ? VolumetricFogCVar->GetInt() : -1,
        MissionFog->GetComponentLocation().Z);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_NIGHT_OPS|AIRCRAFT_LIGHTS|aircraft=%s|enabled=%d|nav=PORT_RED,STARBOARD_GREEN|strobe=DOUBLE_FLASH|landing=ENGINE_READY"),
        *SelectedAircraftId,
        bNightOperationLightsEnabled ? 1 : 0);
}

void ARotorlineHelicopterPawn::ConfigureWeatherPrecipitation(bool bEnabled)
{
    if (!bEnabled)
    {
        for (UNiagaraComponent* Component : WeatherPrecipitationComponents)
        {
            if (IsValid(Component))
            {
                Component->DeactivateImmediate();
                Component->SetVisibility(false, true);
            }
        }
        return;
    }

    if (!RainPrecipitationSystem)
    {
        RainPrecipitationSystem = LoadObject<UNiagaraSystem>(
            nullptr, TEXT("/Game/FX/Weather/NS_RotorlineRain.NS_RotorlineRain"));
    }
    if (!RainPrecipitationSystem || !Camera)
    {
        UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_WEATHER|RAIN_NIAGARA_MISSING"));
        return;
    }

    if (WeatherPrecipitationComponents.IsEmpty())
    {
        constexpr int32 GridRadius = 2;
        constexpr float CellSpacing = 850.0f;
        for (int32 X = -GridRadius; X <= GridRadius; ++X)
        {
            for (int32 Y = -GridRadius; Y <= GridRadius; ++Y)
            {
                UNiagaraComponent* Rain = NewObject<UNiagaraComponent>(this);
                Rain->SetupAttachment(Camera);
                Rain->SetAsset(RainPrecipitationSystem);
                Rain->SetAutoActivate(false);
                Rain->SetRelativeLocation(FVector(
                    450.0f + static_cast<float>(X) * CellSpacing,
                    static_cast<float>(Y) * CellSpacing,
                    1500.0f + static_cast<float>((X * 17 + Y * 31) & 3) * 180.0f));
                Rain->SetVectorParameter(TEXT("RainVelocity"), FVector(180.0f, 320.0f, -5200.0f));
                Rain->SetFloatParameter(TEXT("RainSpawnRate"), 12.0f);
                Rain->RegisterComponent();
                WeatherPrecipitationComponents.Add(Rain);
            }
        }
    }

    for (UNiagaraComponent* Component : WeatherPrecipitationComponents)
    {
        if (IsValid(Component))
        {
            Component->SetVisibility(true, true);
            Component->Activate(true);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_WEATHER|RAIN_NIAGARA_ACTIVE|emitters=%d|world_space=1"),
        WeatherPrecipitationComponents.Num());
}

void ARotorlineHelicopterPawn::UpdateFlightReadout()
{
    // Persistent flight, mission, weapon, and startup information is rendered
    // by ARotorlineOperationsHUD. Keep this tick hook for compatibility with
    // existing runtime sequencing, but never repaint debug text over the HUD.
}
