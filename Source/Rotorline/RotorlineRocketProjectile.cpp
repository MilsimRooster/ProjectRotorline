#include "RotorlineRocketProjectile.h"

#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RotorlineExplosionFx.h"
#include "RotorlineMissionObjectiveActor.h"
#include "RotorlineHelicopterPawn.h"
#include "RotorlineOperationsPlayerController.h"
#include "RotorlineRocketTrailSegment.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineRocketAudio
{
    constexpr int32 MaxConcurrentSounds = 3;
    TArray<TWeakObjectPtr<UAudioComponent>> ActiveSounds;

    float GetDialogueDuck(const UObject* WorldContext)
    {
        const APlayerController* PlayerController =
            UGameplayStatics::GetPlayerController(WorldContext, 0);
        const ARotorlineHelicopterPawn* Helicopter = PlayerController
            ? Cast<ARotorlineHelicopterPawn>(PlayerController->GetPawn())
            : nullptr;
        return Helicopter && Helicopter->IsSpokenDialogueActive() ? 0.18f : 1.0f;
    }

    void ConfigureSpatialization(UAudioComponent* Component, float InnerRadius, float FalloffDistance)
    {
        if (!Component) return;
        Component->bAllowSpatialization = true;
        Component->bOverrideAttenuation = true;
        Component->AttenuationOverrides.bAttenuate = true;
        Component->AttenuationOverrides.bSpatialize = true;
        Component->AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
        Component->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
        Component->AttenuationOverrides.AttenuationShapeExtents = FVector(InnerRadius, 0.0f, 0.0f);
        Component->AttenuationOverrides.FalloffDistance = FalloffDistance;
        Component->AttenuationOverrides.dBAttenuationAtMax = -48.0f;
    }

    UAudioComponent* PlayBoundedAttached(const UObject* WorldContext, USoundBase* Sound,
        USceneComponent* AttachTo, float Volume)
    {
        if (!WorldContext || !Sound || !AttachTo || Volume <= 0.0f)
        {
            return nullptr;
        }

        ActiveSounds.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& Entry)
        {
            return !Entry.IsValid() || !Entry->IsPlaying();
        });

        while (ActiveSounds.Num() >= MaxConcurrentSounds)
        {
            if (UAudioComponent* Oldest = ActiveSounds[0].Get())
            {
                Oldest->Stop();
            }
            ActiveSounds.RemoveAt(0);
        }

        if (UAudioComponent* Component = UGameplayStatics::SpawnSoundAttached(
            Sound,
            AttachTo,
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            false,
            Volume))
        {
            ConfigureSpatialization(Component, 600.0f, 60000.0f);
            ActiveSounds.Add(Component);
            UE_LOG(LogTemp, Verbose, TEXT("ROTORLINE_ROCKET_AUDIO|active=%d|max=%d|volume=%.3f"),
                ActiveSounds.Num(), MaxConcurrentSounds, Volume);
            return Component;
        }
        return nullptr;
    }

    void PlayBoundedAtLocation(const UObject* WorldContext, USoundBase* Sound, const FVector& Location, float Volume)
    {
        if (!WorldContext || !Sound || Volume <= 0.0f) return;
        ActiveSounds.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& Entry)
        {
            return !Entry.IsValid() || !Entry->IsPlaying();
        });
        while (ActiveSounds.Num() >= MaxConcurrentSounds)
        {
            if (UAudioComponent* Oldest = ActiveSounds[0].Get()) Oldest->Stop();
            ActiveSounds.RemoveAt(0);
        }
        if (UAudioComponent* Component = UGameplayStatics::SpawnSoundAtLocation(
            WorldContext, Sound, Location, FRotator::ZeroRotator, Volume))
        {
            ConfigureSpatialization(Component, 900.0f, 80000.0f);
            ActiveSounds.Add(Component);
        }
    }
}

namespace RotorlineRocketDamage
{
    constexpr float DirectDamage = 125.0f;
    constexpr float MinimumBlastDamage = 28.0f;
    constexpr float BlastRadius = 900.0f;
    constexpr float CoverTraceInset = 30.0f;
}

namespace RotorlineHawkMissile
{
    // Gameplay-readable HAWK speed: 100 m/s gives the pilot several seconds
    // to see the smoke trail, deploy countermeasures, or mask behind terrain.
    constexpr float SpeedCmPerSecond = 10000.0f;
    constexpr float StraightBoostSeconds = 1.50f;
    constexpr float SteeringRateCmPerSecond = 20000.0f;
}

ARotorlineRocketProjectile::ARotorlineRocketProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    RocketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RocketMesh"));
    RocketMesh->SetupAttachment(Root);
    RocketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MissileFinder(TEXT("/Game/Missions/Assets/UserProvided/Weapons/BasicMissile/basic_missle/StaticMeshes/basic_missle.basic_missle"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlameFinder(TEXT("/Game/Missions/Presentation/M_RocketFlameGlow.M_RocketFlameGlow"));
    if (MissileFinder.Succeeded())
    {
        RocketMesh->SetStaticMesh(MissileFinder.Object);
        RocketMesh->SetRelativeScale3D(FVector(0.25f));
        RocketMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    }
    else
    {
        if (CylinderFinder.Succeeded()) RocketMesh->SetStaticMesh(CylinderFinder.Object);
        if (FlameFinder.Succeeded()) RocketMesh->SetMaterial(0, FlameFinder.Object);
        RocketMesh->SetRelativeScale3D(FVector(0.18f, 0.18f, 1.0f));
        RocketMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    }

    TrailNear = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrailNear"));
    TrailMid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrailMid"));
    TrailFar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrailFar"));
    ImpactFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactFlash"));
    ExplosionCore = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExplosionCore"));
    ExplosionFlameA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExplosionFlameA"));
    ExplosionFlameB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExplosionFlameB"));
    ExplosionSmoke = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExplosionSmoke"));
    ExplosionSparks = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExplosionSparks"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> HotFinder(TEXT("/Game/Missions/Presentation/M_ExplosionHot.M_ExplosionHot"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SmokeFinder(TEXT("/Game/Missions/Presentation/M_RocketSmoke.M_RocketSmoke"));
    for (UStaticMeshComponent* Trail : { TrailNear.Get(), TrailMid.Get(), TrailFar.Get(), ImpactFlash.Get(), ExplosionCore.Get(), ExplosionFlameA.Get(), ExplosionFlameB.Get(), ExplosionSmoke.Get(), ExplosionSparks.Get() })
    {
        Trail->SetupAttachment(Root);
        Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (SphereFinder.Succeeded()) Trail->SetStaticMesh(SphereFinder.Object);
        if (HotFinder.Succeeded()) Trail->SetMaterial(0, HotFinder.Object);
    }
    TrailNear->SetRelativeLocation(FVector(-70.0f, 0.0f, 0.0f));
    TrailMid->SetRelativeLocation(FVector(-145.0f, 0.0f, 0.0f));
    TrailFar->SetRelativeLocation(FVector(-225.0f, 0.0f, 0.0f));
    TrailNear->SetRelativeScale3D(FVector(0.85f, 0.28f, 0.28f));
    TrailMid->SetRelativeScale3D(FVector(0.65f, 0.21f, 0.21f));
    TrailFar->SetRelativeScale3D(FVector(0.45f, 0.15f, 0.15f));
    ImpactFlash->SetVisibility(false, true);
    ExplosionCore->SetVisibility(false, true);
    ExplosionFlameA->SetVisibility(false, true);
    ExplosionFlameB->SetVisibility(false, true);
    ExplosionSmoke->SetVisibility(false, true);
    ExplosionSparks->SetVisibility(false, true);
    if (SmokeFinder.Succeeded()) ExplosionSmoke->SetMaterial(0, SmokeFinder.Object);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SparksFinder(TEXT("/Game/Missions/Assets/UserProvided/WeaponFX/SparksExplosion/sparksexplosion/StaticMeshes/sparksexplosion.sparksexplosion"));
    if (SparksFinder.Succeeded()) ExplosionSparks->SetStaticMesh(SparksFinder.Object);

    RocketLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RocketLight"));
    RocketLight->SetupAttachment(Root);
    RocketLight->SetLightColor(FLinearColor(1.0f, 0.18f, 0.01f));
    RocketLight->SetIntensity(35000.0f);
    RocketLight->SetAttenuationRadius(1200.0f);

    static ConstructorHelpers::FObjectFinder<USoundBase> PlayerLaunchSoundFinder(TEXT("/Game/Audio/Weapons/Missiles/SFX_PlayerMissileLaunch.SFX_PlayerMissileLaunch"));
    static ConstructorHelpers::FObjectFinder<USoundBase> EnemyLaunchSoundFinder(TEXT("/Game/Audio/Weapons/Missiles/SFX_EnemyMissileLaunch.SFX_EnemyMissileLaunch"));
    static ConstructorHelpers::FObjectFinder<USoundBase> ExplosionSoundFinder(TEXT("/Game/Audio/Weapons/Missiles/SFX_MissileImpactHeavy.SFX_MissileImpactHeavy"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ImpactExplosionFinder(
        TEXT("/Game/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Explosion_03.Niagara_Explosion_03"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PlayerHitExplosionFinder(
        TEXT("/Game/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Air_Explosion_03.Niagara_Air_Explosion_03"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PlayerHitSparksFinder(
        TEXT("/Game/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Sparks_Explosion_01.Niagara_Sparks_Explosion_01"));
    if (PlayerLaunchSoundFinder.Succeeded()) PlayerLaunchSound = PlayerLaunchSoundFinder.Object;
    if (EnemyLaunchSoundFinder.Succeeded()) EnemyLaunchSound = EnemyLaunchSoundFinder.Object;
    if (ExplosionSoundFinder.Succeeded()) ExplosionSound = ExplosionSoundFinder.Object;
    if (ImpactExplosionFinder.Succeeded()) ImpactExplosionSystem = ImpactExplosionFinder.Object;
    if (PlayerHitExplosionFinder.Succeeded()) PlayerHitExplosionSystem = PlayerHitExplosionFinder.Object;
    if (PlayerHitSparksFinder.Succeeded()) PlayerHitSparksSystem = PlayerHitSparksFinder.Object;
}

void ARotorlineRocketProjectile::Launch(const FVector& Start, const FVector& InitialDirection, ARotorlineMissionObjectiveActor* Target)
{
    LaunchPlayerWeapon(
        Start,
        InitialDirection,
        Target,
        TEXT("ROCKET"),
        RotorlineRocketDamage::DirectDamage,
        RotorlineRocketDamage::MinimumBlastDamage,
        RotorlineRocketDamage::BlastRadius,
        36000.0f,
        FString());
}

void ARotorlineRocketProjectile::LaunchPlayerWeapon(
    const FVector& Start,
    const FVector& InitialDirection,
    ARotorlineMissionObjectiveActor* Target,
    const FString& WeaponId,
    float DirectDamage,
    float MinimumBlastDamage,
    float BlastRadius,
    float ProjectileSpeed,
    const FString& ProjectileAsset)
{
    SetActorLocation(Start);
    PlayerLaunchOrigin = Start;
    TargetActor = Target;
    bPlayerGuidedWeapon = IsValid(Target);
    DirectImpactTarget = nullptr;
    bPlayerDamageEnabled = true;
    PlayerWeaponId = WeaponId.IsEmpty() ? TEXT("ROCKET") : WeaponId.ToUpper();
    PlayerDirectDamage = FMath::Max(0.0f, DirectDamage);
    PlayerMinimumBlastDamage = FMath::Clamp(MinimumBlastDamage, 0.0f, PlayerDirectDamage);
    PlayerBlastRadius = FMath::Max(0.0f, BlastRadius);
    PlayerProjectileSpeed = FMath::Max(1000.0f, ProjectileSpeed);
    if (!ProjectileAsset.IsEmpty())
    {
        if (UStaticMesh* WeaponMesh = LoadObject<UStaticMesh>(nullptr, *ProjectileAsset))
        {
            RocketMesh->SetStaticMesh(WeaponMesh);
            RocketMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
            RocketMesh->SetRelativeScale3D(FVector(0.18f));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_BELL222_WEAPON|asset_missing=%s|weapon=%s"), *ProjectileAsset, *PlayerWeaponId);
        }
    }
    // Fast enough to feel forceful from a helicopter while retaining enough
    // travel time to read the smoke trail and steer toward distant targets.
    Velocity = InitialDirection.GetSafeNormal() * PlayerProjectileSpeed;
    SetActorRotation(Velocity.Rotation());
    // Weapon assets arrive mastered much hotter than the engine/environment.
    // Keep impact force without blasting the full mix at default settings.
    float WeaponMix = 0.425f;
    if (const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        WeaponMix = OperationsController->GetEffectiveAudioVolume(ERotorlineAudioChannel::WeaponsExplosions);
    }
    LaunchAudioUnduckedVolume = 0.36f * WeaponMix;
    LaunchAudioComponent = RotorlineRocketAudio::PlayBoundedAttached(
        this, PlayerLaunchSound, Root,
        LaunchAudioUnduckedVolume * RotorlineRocketAudio::GetDialogueDuck(this));
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_WEAPON|%s|state=LAUNCHED|homing=%d|damage=%.1f|blast_cm=%.0f|speed=%.0f"),
        *PlayerWeaponId, IsValid(TargetActor) ? 1 : 0, PlayerDirectDamage, PlayerBlastRadius, PlayerProjectileSpeed);
}

void ARotorlineRocketProjectile::LaunchEnemy(
    const FVector& Start,
    const FVector& InitialDirection,
    ARotorlineHelicopterPawn* Target,
    float Damage)
{
    SetActorLocation(Start);
    TargetActor = nullptr;
    DirectImpactTarget = nullptr;
    bPlayerDamageEnabled = false;
    EnemyTargetActor = Target;
    EnemyDamage = Damage;
    bEnemyRocket = true;
    Velocity = InitialDirection.GetSafeNormal() * 26000.0f;
    SetActorRotation(Velocity.Rotation());
    float WeaponMix = 0.425f;
    if (const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        WeaponMix = OperationsController->GetEffectiveAudioVolume(ERotorlineAudioChannel::WeaponsExplosions);
    }
    LaunchAudioUnduckedVolume = 0.30f * WeaponMix;
    LaunchAudioComponent = RotorlineRocketAudio::PlayBoundedAttached(
        this, EnemyLaunchSound, Root,
        LaunchAudioUnduckedVolume * RotorlineRocketAudio::GetDialogueDuck(this));
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_ENEMY_WEAPON|APACHE_ROCKET|state=LAUNCHED|damage=%.1f"), EnemyDamage);
}

void ARotorlineRocketProjectile::LaunchEnemyArtillery(
    const FVector& Start,
    const FVector& InitialDirection,
    ARotorlineHelicopterPawn* Target,
    float Damage)
{
    SetActorLocation(Start);
    TargetActor = nullptr;
    DirectImpactTarget = nullptr;
    bPlayerDamageEnabled = false;
    EnemyTargetActor = Target;
    EnemyDamage = Damage;
    bEnemyRocket = true;
    bEnemyArtilleryRocket = true;

    // Leave the pod in a readable loft before guidance bends the rocket onto
    // the aircraft. This produces a visible artillery launch instead of a
    // shell or an instant straight-line hit.
    const FVector LoftedDirection = (InitialDirection.GetSafeNormal() * 0.82f + FVector::UpVector * 0.57f).GetSafeNormal();
    Velocity = LoftedDirection * 19000.0f;
    SetActorRotation(Velocity.Rotation());
    float WeaponMix = 0.425f;
    if (const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        WeaponMix = OperationsController->GetEffectiveAudioVolume(ERotorlineAudioChannel::WeaponsExplosions);
    }
    LaunchAudioUnduckedVolume = 0.32f * WeaponMix;
    LaunchAudioComponent = RotorlineRocketAudio::PlayBoundedAttached(
        this, EnemyLaunchSound, Root,
        LaunchAudioUnduckedVolume * RotorlineRocketAudio::GetDialogueDuck(this));
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_ENEMY_WEAPON|HIMARS_ROCKET|state=LAUNCHED|damage=%.1f|lofted=1"), EnemyDamage);
}

void ARotorlineRocketProjectile::LaunchEnemyAirDefense(
    const FVector& Start,
    const FVector& InitialDirection,
    ARotorlineHelicopterPawn* Target,
    float Damage)
{
    SetActorLocation(Start);
    TargetActor = nullptr;
    DirectImpactTarget = nullptr;
    bPlayerDamageEnabled = false;
    EnemyTargetActor = Target;
    EnemyDamage = Damage;
    bEnemyRocket = true;
    bEnemyArtilleryRocket = false;
    bEnemyAirDefenseMissile = true;

    // The rack already supplies the launch elevation. Leave the rail on that
    // exact vector; guidance is deliberately held until the boost phase clears
    // the physical launcher instead of injecting a fake vertical loft here.
    Velocity = InitialDirection.GetSafeNormal() * RotorlineHawkMissile::SpeedCmPerSecond;
    SetActorRotation(Velocity.Rotation());
    float WeaponMix = 0.425f;
    if (const ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        WeaponMix = OperationsController->GetEffectiveAudioVolume(ERotorlineAudioChannel::WeaponsExplosions);
    }
    LaunchAudioUnduckedVolume = 0.90f * WeaponMix;
    LaunchAudioComponent = RotorlineRocketAudio::PlayBoundedAttached(
        this, EnemyLaunchSound, Root,
        LaunchAudioUnduckedVolume * RotorlineRocketAudio::GetDialogueDuck(this));
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_ENEMY_WEAPON|HAWK_MISSILE|state=LAUNCHED|damage=%.1f|rack_aligned=1|rail_pitch=%.1f|speed_mps=100|visual=MISSILE_SMOKE|physical_audio=ACTIVE|volume_scale=0.90"),
        EnemyDamage,
        Velocity.Rotation().Pitch);
}

void ARotorlineRocketProjectile::DetonateVisualOnly(const FVector& Location)
{
    SetActorLocation(Location);
    TargetActor = nullptr;
    DirectImpactTarget = nullptr;
    EnemyTargetActor = nullptr;
    bEnemyRocket = false;
    bPlayerDamageEnabled = false;
    Explode(false);
}

bool ARotorlineRocketProjectile::DivertEnemyGuidance(const FVector& DecoyLocation)
{
    if (!bEnemyRocket || bExploding || bCountermeasureDiverted || !IsValid(EnemyTargetActor))
    {
        return false;
    }

    bCountermeasureDiverted = true;
    CountermeasureTarget = DecoyLocation;
    CountermeasureElapsed = 0.0f;
    EnemyTargetActor = nullptr;
    const float CurrentSpeed = FMath::Max(7600.0f, Velocity.Size());
    const FVector DecoyDirection = (CountermeasureTarget - GetActorLocation()).GetSafeNormal();
    Velocity = (Velocity.GetSafeNormal() * 0.30f + DecoyDirection * 0.70f).GetSafeNormal() * CurrentSpeed;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_COUNTERMEASURE|MISSILE_DIVERTED|artillery=%d|distance_to_decoy_m=%.1f"),
        bEnemyArtilleryRocket ? 1 : 0,
        FVector::Dist(GetActorLocation(), CountermeasureTarget) / 100.0f);
    return true;
}

void ARotorlineRocketProjectile::ApplyPlayerBlastDamage()
{
    if (!bPlayerDamageEnabled || bEnemyRocket || !GetWorld())
    {
        return;
    }

    ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    int32 DamagedTargetCount = 0;
    int32 DestroyedTargetCount = 0;
    const FVector ImpactLocation = GetActorLocation();

    for (TActorIterator<ARotorlineMissionObjectiveActor> It(GetWorld()); It; ++It)
    {
        ARotorlineMissionObjectiveActor* Candidate = *It;
        if (!IsValid(Candidate) || !Candidate->IsDestroyObjective() || Candidate->IsDestroyedTarget() ||
            Candidate->GetHealthFraction() <= 0.0f)
        {
            continue;
        }

        const bool bDirectImpact = Candidate == DirectImpactTarget.Get();
        const FVector AimLocation = Candidate->GetAimLocation();
        const float CenterDistance = FVector::Dist(ImpactLocation, AimLocation);
        const float SurfaceDistance = FMath::Max(0.0f, CenterDistance - Candidate->GetProjectileHitRadius());
        if (!bDirectImpact && SurfaceDistance > PlayerBlastRadius)
        {
            continue;
        }

        // Splash cannot pass through terrain or buildings. A swept direct hit
        // has already proven an unobstructed projectile path and bypasses this
        // secondary cover trace.
        if (!bDirectImpact)
        {
            const FVector ToTarget = AimLocation - ImpactLocation;
            const FVector TraceDirection = ToTarget.GetSafeNormal();
            const FVector TraceStart = ImpactLocation +
            TraceDirection * RotorlineRocketDamage::CoverTraceInset +
                FVector::UpVector * RotorlineRocketDamage::CoverTraceInset;
            FHitResult CoverHit;
            FCollisionQueryParams CoverParams(SCENE_QUERY_STAT(RotorlinePlayerRocketBlastCover), false, this);
            CoverParams.AddIgnoredActor(this);
            if (GetOwner()) CoverParams.AddIgnoredActor(GetOwner());
            const bool bCovered = GetWorld()->LineTraceSingleByChannel(
                CoverHit, TraceStart, AimLocation, ECC_Visibility, CoverParams);
            if (bCovered)
            {
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_DAMAGE_INTEGRITY|BLAST_BLOCKED|target=%s|distance_cm=%.0f|blocker=%s"),
                    *Candidate->GetTargetLabel(), SurfaceDistance,
                    CoverHit.GetActor() ? *CoverHit.GetActor()->GetName() : TEXT("WORLD"));
                continue;
            }
        }

        const float BlastAlpha = bDirectImpact
            ? 1.0f
            : 1.0f - FMath::Clamp(SurfaceDistance / FMath::Max(1.0f, PlayerBlastRadius), 0.0f, 1.0f);
        const float BaseRequestedDamage = bDirectImpact
            ? PlayerDirectDamage
            : FMath::Lerp(PlayerMinimumBlastDamage, PlayerDirectDamage, BlastAlpha);
        const float TravelDistanceCm = FVector::Distance(PlayerLaunchOrigin, ImpactLocation);
        const float RangeAlpha = FMath::Clamp(
            (TravelDistanceCm - 90000.0f) / (350000.0f - 90000.0f), 0.0f, 1.0f);
        const float RequestedDamage =
            BaseRequestedDamage * FMath::Lerp(1.0f, 0.82f, RangeAlpha);
        const bool bAircraft = Candidate->IsAircraftThreat();
        float AppliedDamage = 0.0f;
        const bool bDestroyed = Candidate->ApplyCombatDamage(
            RequestedDamage,
            bDirectImpact ? *FString::Printf(TEXT("%s_DIRECT"), *PlayerWeaponId) : *FString::Printf(TEXT("%s_BLAST"), *PlayerWeaponId),
            AppliedDamage);
        if (AppliedDamage <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        ++DamagedTargetCount;
        if (bDestroyed) ++DestroyedTargetCount;
        if (OperationsController)
        {
            OperationsController->NotifyWeaponHit(bDestroyed, bAircraft);
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_DAMAGE_INTEGRITY|ROCKET_HIT|target=%s|mode=%s|surface_distance_cm=%.0f|requested=%.1f|applied=%.1f|health=%.1f|max_health=%.1f|destroyed=%d"),
            *Candidate->GetTargetLabel(), bDirectImpact ? TEXT("DIRECT") : TEXT("BLAST"),
            SurfaceDistance, RequestedDamage, AppliedDamage,
            Candidate->GetCurrentHealth(), Candidate->GetMaximumHealth(), bDestroyed ? 1 : 0);
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_DAMAGE_INTEGRITY|ROCKET_RESOLUTION|damaged_targets=%d|destroyed_targets=%d|direct=%d|blast_radius_cm=%.0f"),
        DamagedTargetCount, DestroyedTargetCount, IsValid(DirectImpactTarget) ? 1 : 0,
        PlayerBlastRadius);
}

void ARotorlineRocketProjectile::Explode(bool bDamageTarget)
{
    if (bExploding) return;
    bExploding = true;
    const FVector ImpactVelocity = Velocity;
    Velocity = FVector::ZeroVector;
    if (LaunchAudioComponent)
    {
        LaunchAudioComponent->Stop();
        LaunchAudioComponent = nullptr;
    }
    LaunchAudioUnduckedVolume = 0.0f;
    if (bPlayerDamageEnabled && !bEnemyRocket)
    {
        if (bDamageTarget && !IsValid(DirectImpactTarget) && IsValid(TargetActor))
        {
            DirectImpactTarget = TargetActor;
        }
        ApplyPlayerBlastDamage();
    }
    if (bDamageTarget && bEnemyRocket && IsValid(EnemyTargetActor))
    {
        const FVector ImpactImpulse = ImpactVelocity.GetSafeNormal() *
            FMath::Clamp(EnemyDamage * 60.0f, 320.0f, 1100.0f);
        EnemyTargetActor->ApplyEnemyProjectileHit(EnemyDamage, ImpactImpulse);
    }

    const bool bPlayerAircraftImpact = bDamageTarget && bEnemyRocket && IsValid(EnemyTargetActor);
    UNiagaraSystem* SelectedExplosionSystem = bPlayerAircraftImpact && PlayerHitExplosionSystem
        ? PlayerHitExplosionSystem.Get()
        : ImpactExplosionSystem.Get();
    bUsingNiagaraExplosion = SelectedExplosionSystem != nullptr;
    if (bUsingNiagaraExplosion)
    {
        RotorlineExplosionFx::SpawnTransient(
            this,
            SelectedExplosionSystem,
            GetActorLocation(),
            ImpactVelocity.Rotation(),
            FVector(bPlayerAircraftImpact ? 0.58f : 0.68f),
            bPlayerAircraftImpact ? 0.72f : 0.95f,
            bPlayerAircraftImpact ? 3.2f : 4.0f);
        if (bPlayerAircraftImpact && PlayerHitSparksSystem)
        {
            RotorlineExplosionFx::SpawnTransient(
                this,
                PlayerHitSparksSystem,
                GetActorLocation(),
                ImpactVelocity.Rotation(),
                FVector(0.75f),
                0.35f,
                1.8f);
        }
    }
    RocketMesh->SetVisibility(false, true);
    TrailNear->SetVisibility(false, true);
    TrailMid->SetVisibility(false, true);
    TrailFar->SetVisibility(false, true);
    ImpactFlash->SetVisibility(!bUsingNiagaraExplosion, true);
    ExplosionCore->SetVisibility(!bUsingNiagaraExplosion, true);
    ExplosionFlameA->SetVisibility(!bUsingNiagaraExplosion, true);
    ExplosionFlameB->SetVisibility(!bUsingNiagaraExplosion, true);
    ExplosionSmoke->SetVisibility(!bUsingNiagaraExplosion, true);
    ExplosionSparks->SetVisibility(!bUsingNiagaraExplosion, true);
    ImpactFlash->SetRelativeScale3D(FVector(0.6f));
    ExplosionCore->SetRelativeScale3D(FVector(0.35f));
    ExplosionFlameA->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.5f));
    ExplosionFlameB->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.4f));
    ExplosionFlameA->SetRelativeLocation(FVector(80.0f, -65.0f, 45.0f));
    ExplosionFlameB->SetRelativeLocation(FVector(-95.0f, 75.0f, 30.0f));
    ExplosionSmoke->SetRelativeScale3D(FVector(0.5f));
    ExplosionSmokeMaterial = ExplosionSmoke->CreateDynamicMaterialInstance(0);
    if (ExplosionSmokeMaterial)
    {
        ExplosionSmokeMaterial->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.16f, 0.13f, 0.10f, 1.0f));
        ExplosionSmokeMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.48f);
    }
    ExplosionSparks->SetRelativeScale3D(FVector(8.0f));
    ExplosionSparks->SetRelativeLocation(FVector(0.0f, 0.0f, -2175.0f));
    RocketLight->SetIntensity(90000.0f);
    RocketLight->SetAttenuationRadius(3000.0f);
    float WeaponMix = 0.425f;
    if (const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        WeaponMix = OperationsController->GetEffectiveAudioVolume(ERotorlineAudioChannel::WeaponsExplosions);
    }
    RotorlineRocketAudio::PlayBoundedAtLocation(
        this, ExplosionSound, GetActorLocation(),
        0.40f * WeaponMix * RotorlineRocketAudio::GetDialogueDuck(this));
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_WEAPON|ROCKET|state=IMPACT|target_hit=%d|player_hit=%d|fx=%s|cleanup=HARD_LIMIT"),
        bDamageTarget ? 1 : 0,
        bPlayerAircraftImpact ? 1 : 0,
        bUsingNiagaraExplosion ? TEXT("NIAGARA") : TEXT("LEGACY_FALLBACK"));
}

void ARotorlineRocketProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (LaunchAudioComponent && LaunchAudioComponent->IsPlaying())
    {
        LaunchAudioComponent->SetVolumeMultiplier(
            LaunchAudioUnduckedVolume * RotorlineRocketAudio::GetDialogueDuck(this));
    }
    LifeSeconds += DeltaSeconds;
    if (bExploding)
    {
        ExplosionSeconds += DeltaSeconds;
        if (bUsingNiagaraExplosion)
        {
            // Niagara owns the complete explosion presentation. Never let the
            // old procedural sphere fallback reappear on the following Tick;
            // those meshes produced the overexposed white fireballs that hid
            // the imported flare, debris, and smoke simulation.
            ImpactFlash->SetVisibility(false, true);
            ExplosionCore->SetVisibility(false, true);
            ExplosionFlameA->SetVisibility(false, true);
            ExplosionFlameB->SetVisibility(false, true);
            ExplosionSmoke->SetVisibility(false, true);
            ExplosionSparks->SetVisibility(false, true);
            RocketLight->SetIntensity(FMath::Lerp(
                90000.0f,
                0.0f,
                FMath::Clamp(ExplosionSeconds / 0.72f, 0.0f, 1.0f)));
            if (ExplosionSeconds >= 1.35f) Destroy();
            return;
        }
        const float FlashAlpha = FMath::Clamp(ExplosionSeconds / 0.32f, 0.0f, 1.0f);
        const float SmokeAlpha = FMath::Clamp(ExplosionSeconds / 1.20f, 0.0f, 1.0f);
        ImpactFlash->SetRelativeScale3D(FVector(FMath::Lerp(0.6f, 14.0f, FlashAlpha)));
        ExplosionCore->SetRelativeScale3D(FVector(FMath::Lerp(0.35f, 7.5f, FMath::Clamp(ExplosionSeconds / 0.55f, 0.0f, 1.0f))));
        const float FlameAlpha = FMath::Clamp(ExplosionSeconds / 0.82f, 0.0f, 1.0f);
        const float FlameFlickerA = 0.82f + FMath::Abs(FMath::Sin(ExplosionSeconds * 31.0f)) * 0.42f;
        const float FlameFlickerB = 0.78f + FMath::Abs(FMath::Sin(ExplosionSeconds * 37.0f + 1.2f)) * 0.48f;
        ExplosionFlameA->SetRelativeScale3D(FVector(2.8f * FlameFlickerA, 2.4f / FlameFlickerA, FMath::Lerp(1.0f, 8.5f, FlameAlpha)));
        ExplosionFlameB->SetRelativeScale3D(FVector(2.3f / FlameFlickerB, 2.7f * FlameFlickerB, FMath::Lerp(0.8f, 7.0f, FlameAlpha)));
        ExplosionFlameA->SetRelativeLocation(FVector(80.0f, -65.0f, FMath::Lerp(45.0f, 520.0f, FlameAlpha)));
        ExplosionFlameB->SetRelativeLocation(FVector(-95.0f, 75.0f, FMath::Lerp(30.0f, 410.0f, FlameAlpha)));
        ExplosionFlameA->AddLocalRotation(FRotator(35.0f, 65.0f, 18.0f) * DeltaSeconds);
        ExplosionFlameB->AddLocalRotation(FRotator(-42.0f, 48.0f, -25.0f) * DeltaSeconds);
        ExplosionSmoke->SetRelativeScale3D(FVector(FMath::Lerp(0.5f, 12.0f, SmokeAlpha)));
        ExplosionSmoke->SetRelativeLocation(FVector(0.0f, 0.0f, SmokeAlpha * 350.0f));
        ExplosionSparks->AddLocalRotation(FRotator(70.0f * DeltaSeconds, 110.0f * DeltaSeconds, 45.0f * DeltaSeconds));
        ExplosionSparks->SetRelativeScale3D(FVector(FMath::Lerp(8.0f, 15.0f, FlashAlpha)));
        ExplosionSparks->SetVisibility(ExplosionSeconds < 0.48f, true);
        ImpactFlash->SetVisibility(ExplosionSeconds < 0.38f, true);
        ExplosionCore->SetVisibility(ExplosionSeconds < 0.72f, true);
        ExplosionFlameA->SetVisibility(ExplosionSeconds < 0.92f, true);
        ExplosionFlameB->SetVisibility(ExplosionSeconds < 0.86f, true);
        if (ExplosionSmokeMaterial)
        {
            ExplosionSmokeMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.48f * FMath::Square(1.0f - SmokeAlpha));
        }
        RocketLight->SetIntensity(FMath::Lerp(150000.0f, 0.0f, FMath::Clamp(ExplosionSeconds / 0.72f, 0.0f, 1.0f)));
        if (ExplosionSeconds >= 1.35f) Destroy();
        return;
    }
    if (LifeSeconds > 13.5f)
    {
        Destroy();
        return;
    }

    if (bEnemyRocket && IsValid(EnemyTargetActor) && EnemyTargetActor->IsBell222StealthActive())
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_BELL222_STEALTH|MISSILE_LOCK_BROKEN|air_defense=%d|artillery=%d"),
            bEnemyAirDefenseMissile ? 1 : 0,
            bEnemyArtilleryRocket ? 1 : 0);
        EnemyTargetActor = nullptr;
    }

    if (bCountermeasureDiverted)
    {
        CountermeasureElapsed += DeltaSeconds;
        const FVector ToDecoy = CountermeasureTarget - GetActorLocation();
        const float DesiredSpeed = bEnemyArtilleryRocket ? 19000.0f :
            (bEnemyAirDefenseMissile ? RotorlineHawkMissile::SpeedCmPerSecond : 26000.0f);
        Velocity = FMath::VInterpConstantTo(
            Velocity,
            ToDecoy.GetSafeNormal() * DesiredSpeed,
            DeltaSeconds,
            52000.0f);
        if (ToDecoy.SizeSquared() <= FMath::Square(420.0f) || CountermeasureElapsed >= 1.8f)
        {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COUNTERMEASURE|MISSILE_DEFEATED|artillery=%d|air_defense=%d|elapsed=%.2f"),
            bEnemyArtilleryRocket ? 1 : 0,
            bEnemyAirDefenseMissile ? 1 : 0,
            CountermeasureElapsed);
            Explode(false);
            return;
        }
    }
    else if (IsValid(TargetActor) || (bEnemyRocket && IsValid(EnemyTargetActor)))
    {
        const FVector TargetLocation = IsValid(TargetActor)
            ? TargetActor->GetAimLocation()
            : EnemyTargetActor->GetActorLocation() + FVector::UpVector * 80.0f;
        const FVector ToTarget = TargetLocation - GetActorLocation();
            const float DesiredSpeed = bEnemyArtilleryRocket ? 19000.0f :
            (bEnemyAirDefenseMissile ? RotorlineHawkMissile::SpeedCmPerSecond : (bEnemyRocket ? 26000.0f : PlayerProjectileSpeed));
        const float SteeringRate = bEnemyArtilleryRocket ? 11500.0f :
            // Once the straight rail boost completes, the HAWK seeker needs
            // enough authority to pull onto a low target inside the valley
            // without continuing its initial climb into the far ridgeline.
            (bEnemyAirDefenseMissile ? RotorlineHawkMissile::SteeringRateCmPerSecond : (bEnemyRocket ? 32000.0f : 76000.0f));
        // HAWK missiles hold the actual rack vector through the initial motor
        // boost, then begin guidance. This is a straight rail launch, not a
        // canned parabolic or vertical trajectory.
        if (!bEnemyAirDefenseMissile || LifeSeconds >= RotorlineHawkMissile::StraightBoostSeconds)
        {
            const FVector DesiredVelocity = ToTarget.GetSafeNormal() * DesiredSpeed;
            Velocity = FMath::VInterpConstantTo(Velocity, DesiredVelocity, DeltaSeconds, SteeringRate);
        }
    }

    const FVector OldLocation = GetActorLocation();
    TrailSpawnAccumulator += DeltaSeconds;
    if (TrailSpawnAccumulator >= 0.065f)
    {
        TrailSpawnAccumulator = 0.0f;
        if (ARotorlineRocketTrailSegment* Segment = GetWorld()->SpawnActor<ARotorlineRocketTrailSegment>(
            ARotorlineRocketTrailSegment::StaticClass(), OldLocation - Velocity.GetSafeNormal() * 120.0f, Velocity.Rotation()))
        {
            Segment->InitializeTrail(OldLocation - Velocity.GetSafeNormal() * 120.0f, Velocity.GetSafeNormal());
        }
    }
    const FVector NewLocation = OldLocation + Velocity * DeltaSeconds;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RotorlineRocketImpact), false, this);
    Params.AddIgnoredActor(this);
    if (GetOwner()) Params.AddIgnoredActor(GetOwner());
    const bool bWorldBlocked = GetWorld()->LineTraceSingleByChannel(Hit, OldLocation, NewLocation, ECC_Visibility, Params);

    // Mission presentation meshes intentionally have collision disabled. Sweep
    // every damageable target against the complete travelled segment so locked
    // and unguided rockets share the same hit registration and cannot tunnel at
    // low frame rates.
    if (!bCountermeasureDiverted && bPlayerDamageEnabled && !bEnemyRocket)
    {
        ARotorlineMissionObjectiveActor* SweptTarget = nullptr;
        FVector SweptImpactPoint = FVector::ZeroVector;
        float ClosestTravelDistanceSquared = TNumericLimits<float>::Max();
        for (TActorIterator<ARotorlineMissionObjectiveActor> It(GetWorld()); It; ++It)
        {
            ARotorlineMissionObjectiveActor* Candidate = *It;
            if (!IsValid(Candidate) || !Candidate->IsDestroyObjective() || Candidate->IsDestroyedTarget() ||
                Candidate->GetHealthFraction() <= 0.0f)
            {
                continue;
            }
            // A guided player missile is committed to the actor assigned at
            // launch. Abstract hit volumes from unrelated ground or air
            // objectives must not intercept it and appear to steal guidance.
            if (bPlayerGuidedWeapon && Candidate != TargetActor)
            {
                continue;
            }

            const FVector AimLocation = Candidate->GetAimLocation();
            const FVector ClosestPoint = FMath::ClosestPointOnSegment(AimLocation, OldLocation, NewLocation);
            const float TravelDistanceSquared = FVector::DistSquared(OldLocation, ClosestPoint);
            const float HitRadius = Candidate->GetProjectileHitRadius();
            const bool bInsideTarget = FVector::DistSquared(AimLocation, ClosestPoint) <= FMath::Square(HitRadius);
            const bool bTargetBeforeTerrain = !bWorldBlocked ||
                TravelDistanceSquared <= FVector::DistSquared(OldLocation, Hit.ImpactPoint) + FMath::Square(25.0f);
            if (bInsideTarget && bTargetBeforeTerrain && TravelDistanceSquared < ClosestTravelDistanceSquared)
            {
                SweptTarget = Candidate;
                SweptImpactPoint = ClosestPoint;
                ClosestTravelDistanceSquared = TravelDistanceSquared;
            }
        }

        if (SweptTarget)
        {
            DirectImpactTarget = SweptTarget;
            SetActorLocation(SweptImpactPoint);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_DAMAGE_INTEGRITY|SWEPT_ROCKET_HIT|target=%s|radius_cm=%.0f|travel_cm=%.0f|locked_target=%d"),
                *SweptTarget->GetTargetLabel(), SweptTarget->GetProjectileHitRadius(),
                FVector::Dist(OldLocation, SweptImpactPoint), SweptTarget == TargetActor ? 1 : 0);
            Explode(true);
            return;
        }
    }

    if (!bCountermeasureDiverted && bEnemyRocket && IsValid(EnemyTargetActor))
    {
        const FVector TargetLocation = EnemyTargetActor->GetActorLocation() + FVector::UpVector * 80.0f;
        const FVector ClosestPoint = FMath::ClosestPointOnSegment(TargetLocation, OldLocation, NewLocation);
        const float HitRadius = 500.0f;
        const bool bInsideTarget = FVector::DistSquared(TargetLocation, ClosestPoint) <= FMath::Square(HitRadius);
        const bool bTargetBeforeTerrain = !bWorldBlocked ||
            FVector::DistSquared(OldLocation, ClosestPoint) <= FVector::DistSquared(OldLocation, Hit.ImpactPoint) + FMath::Square(25.0f);
        if (bInsideTarget && bTargetBeforeTerrain)
        {
            SetActorLocation(ClosestPoint);
            if (bEnemyAirDefenseMissile)
            {
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_ENEMY_WEAPON|HAWK_MISSILE|state=TARGET_HIT|flight_seconds=%.2f"),
                    LifeSeconds);
            }
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_WEAPON|ROCKET|state=SWEPT_TARGET_HIT|radius_cm=%.0f|travel_cm=%.0f|target=%s"),
                HitRadius,
                FVector::Dist(OldLocation, ClosestPoint),
                TEXT("PLAYER"));
            Explode(true);
            return;
        }
    }

    if (bWorldBlocked)
    {
        SetActorLocation(Hit.ImpactPoint);
        if (bEnemyAirDefenseMissile)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_ENEMY_WEAPON|HAWK_MISSILE|state=IMPACT_WORLD|flight_seconds=%.2f|blocker=%s"),
                LifeSeconds,
                Hit.GetActor() ? *Hit.GetActor()->GetActorNameOrLabel() : TEXT("WORLD"));
        }
        Explode(false);
        return;
    }
    SetActorRotation(Velocity.Rotation());
    SetActorLocation(NewLocation, false);
}
