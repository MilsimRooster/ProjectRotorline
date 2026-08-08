#include "RotorlineCannonProjectile.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "RotorlineMissionObjectiveActor.h"
#include "RotorlineOperationsPlayerController.h"
#include "RotorlineRocketProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineCannon
{
    constexpr float ProjectileSpeed = 50000.0f;
    constexpr float MaximumLife = 3.0f;
    constexpr float TargetSweepRadius = 230.0f;
}

ARotorlineCannonProjectile::ARotorlineCannonProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Tracer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tracer"));
    Tracer->SetupAttachment(Root);
    Tracer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> TracerMaterialFinder(
        TEXT("/Game/Missions/Presentation/M_RocketFlameGlow.M_RocketFlameGlow"));
    if (SphereFinder.Succeeded()) Tracer->SetStaticMesh(SphereFinder.Object);
    if (TracerMaterialFinder.Succeeded()) Tracer->SetMaterial(0, TracerMaterialFinder.Object);
    // The basic sphere's long axis is local Z. Rotate it into actor-forward X.
    Tracer->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    // Chase camera readability: retain a narrow 30 mm character while giving
    // the fast 500 m/s projectile enough luminous length to survive a single
    // rendered frame at distance. This is a tracer streak, not hit geometry.
    Tracer->SetRelativeScale3D(FVector(0.11f, 0.11f, 3.20f));
    Tracer->SetCastShadow(false);

    TracerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TracerLight"));
    TracerLight->SetupAttachment(Root);
    TracerLight->SetLightColor(FLinearColor(1.0f, 0.32f, 0.015f));
    TracerLight->SetIntensity(95000.0f);
    TracerLight->SetAttenuationRadius(1500.0f);
    TracerLight->SetSourceRadius(18.0f);
}

void ARotorlineCannonProjectile::Launch(const FVector& Start, const FVector& Direction, float Damage)
{
    LaunchAdvanced(Start, Direction, Damage, RotorlineCannon::ProjectileSpeed, 0.0f, TEXT("30MM_DIRECT"));
}

void ARotorlineCannonProjectile::LaunchAdvanced(
    const FVector& Start,
    const FVector& Direction,
    float Damage,
    float Speed,
    float InBlastRadius,
    const FString& InDamageSource,
    float TracerScale)
{
    SetActorLocation(Start);
    LaunchOrigin = Start;
    Velocity = Direction.GetSafeNormal() * FMath::Max(1000.0f, Speed);
    HitDamage = FMath::Max(0.0f, Damage);
    BlastRadius = FMath::Max(0.0f, InBlastRadius);
    DamageSource = InDamageSource.IsEmpty() ? TEXT("CANNON_DIRECT") : InDamageSource;
    Tracer->SetRelativeScale3D(Tracer->GetRelativeScale3D() * FMath::Max(0.35f, TracerScale));
    // Bell .50-cal rounds are emitted rapidly from both stations. Their mesh
    // tracers remain visible, but omitting a dynamic point light per round
    // avoids dozens of overlapping movable lights during sustained fire.
    TracerLight->SetVisibility(TracerScale >= 1.0f, true);
    SetActorRotation(Velocity.Rotation());
}

void ARotorlineCannonProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    LifeSeconds += DeltaSeconds;
    // A hot muzzle-bright first frame settles into the sustained orange tracer
    // so the pilot gets immediate visual confirmation on every trigger pull.
    const float TracerSettleAlpha = FMath::Clamp(LifeSeconds / 0.09f, 0.0f, 1.0f);
    TracerLight->SetIntensity(FMath::Lerp(180000.0f, 95000.0f, TracerSettleAlpha));
    if (!GetWorld() || LifeSeconds >= RotorlineCannon::MaximumLife)
    {
        Destroy();
        return;
    }

    const FVector OldLocation = GetActorLocation();
    const FVector NewLocation = OldLocation + Velocity * DeltaSeconds;

    FHitResult WorldHit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RotorlinePlayerCannonImpact), false, this);
    Params.AddIgnoredActor(this);
    if (GetOwner()) Params.AddIgnoredActor(GetOwner());
    const bool bWorldBlocked = GetWorld()->LineTraceSingleByChannel(
        WorldHit, OldLocation, NewLocation, ECC_Visibility, Params);

    ARotorlineMissionObjectiveActor* HitTarget = nullptr;
    FVector TargetImpactPoint = FVector::ZeroVector;
    float ClosestTravelDistanceSquared = TNumericLimits<float>::Max();
    for (TActorIterator<ARotorlineMissionObjectiveActor> It(GetWorld()); It; ++It)
    {
        ARotorlineMissionObjectiveActor* Candidate = *It;
        if (!IsValid(Candidate) || !Candidate->IsDestroyObjective() || Candidate->IsDestroyedTarget() ||
            Candidate->GetHealthFraction() <= 0.0f) continue;

        const FVector AimLocation = Candidate->GetAimLocation();
        const FVector ClosestPoint = FMath::ClosestPointOnSegment(AimLocation, OldLocation, NewLocation);
        const float DistanceSquared = FVector::DistSquared(AimLocation, ClosestPoint);
        const float HitRadius = FMath::Max(RotorlineCannon::TargetSweepRadius, Candidate->GetProjectileHitRadius());
        const float TravelDistanceSquared = FVector::DistSquared(OldLocation, ClosestPoint);
        const bool bTargetBeforeTerrain = !bWorldBlocked ||
            TravelDistanceSquared <= FVector::DistSquared(OldLocation, WorldHit.ImpactPoint) + FMath::Square(25.0f);
        if (DistanceSquared <= FMath::Square(HitRadius) && bTargetBeforeTerrain &&
            TravelDistanceSquared < ClosestTravelDistanceSquared)
        {
            HitTarget = Candidate;
            TargetImpactPoint = ClosestPoint;
            ClosestTravelDistanceSquared = TravelDistanceSquared;
        }
    }
    if (HitTarget)
    {
        SetActorLocation(TargetImpactPoint);
        const bool bAircraft = HitTarget->IsAircraftThreat();
        const float TravelDistanceCm = FVector::Distance(LaunchOrigin, TargetImpactPoint);
        const float RangeAlpha = FMath::Clamp(
            (TravelDistanceCm - 60000.0f) / (250000.0f - 60000.0f), 0.0f, 1.0f);
        const float RangeAdjustedDamage = HitDamage * FMath::Lerp(1.0f, 0.72f, RangeAlpha);
        float AppliedDamage = 0.0f;
        const bool bDestroyed = HitTarget->ApplyCombatDamage(
            RangeAdjustedDamage, *DamageSource, AppliedDamage);
        if (AppliedDamage > KINDA_SMALL_NUMBER)
        {
            if (ARotorlineOperationsPlayerController* OperationsController =
                Cast<ARotorlineOperationsPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
            {
                OperationsController->NotifyWeaponHit(bDestroyed, bAircraft);
            }
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_WEAPON|APACHE_30MM|state=SWEPT_TARGET_HIT|requested=%.1f|damage=%.1f|health=%.1f|max_health=%.1f|health_pct=%.0f|hit_index=%d|radius_cm=%.0f|target=%s"),
            RangeAdjustedDamage, AppliedDamage, HitTarget->GetCurrentHealth(), HitTarget->GetMaximumHealth(),
            HitTarget->GetHealthFraction() * 100.0f, HitTarget->GetPlayerDamageEventCount(),
            HitTarget->GetProjectileHitRadius(), *HitTarget->GetTargetLabel());
        if (BlastRadius > 0.0f)
        {
            for (TActorIterator<ARotorlineMissionObjectiveActor> BlastIt(GetWorld()); BlastIt; ++BlastIt)
            {
                ARotorlineMissionObjectiveActor* Candidate = *BlastIt;
                if (!IsValid(Candidate) || Candidate == HitTarget || !Candidate->IsDestroyObjective() || Candidate->IsDestroyedTarget()) continue;
                const float SurfaceDistance = FMath::Max(0.0f,
                    FVector::Distance(TargetImpactPoint, Candidate->GetAimLocation()) - Candidate->GetProjectileHitRadius());
                if (SurfaceDistance > BlastRadius) continue;
                float SplashApplied = 0.0f;
                Candidate->ApplyCombatDamage(
                    FMath::Lerp(HitDamage * 0.22f, HitDamage * 0.58f, 1.0f - SurfaceDistance / BlastRadius),
                    TEXT("BELL_40MM_BLAST"),
                    SplashApplied);
            }
            if (ARotorlineRocketProjectile* Impact = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
                ARotorlineRocketProjectile::StaticClass(), TargetImpactPoint, FRotator::ZeroRotator))
            {
                Impact->DetonateVisualOnly(TargetImpactPoint);
            }
        }
        Destroy();
        return;
    }

    if (bWorldBlocked)
    {
        SetActorLocation(WorldHit.ImpactPoint);
        if (BlastRadius > 0.0f)
        {
            if (ARotorlineRocketProjectile* Impact = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
                ARotorlineRocketProjectile::StaticClass(), WorldHit.ImpactPoint, FRotator::ZeroRotator))
            {
                Impact->DetonateVisualOnly(WorldHit.ImpactPoint);
            }
        }
        Destroy();
        return;
    }

    SetActorLocation(NewLocation, false);
    SetActorRotation(Velocity.Rotation());
}
