#include "RotorlineEnemyProjectile.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "RotorlineHelicopterPawn.h"
#include "UObject/ConstructorHelpers.h"

ARotorlineEnemyProjectile::ARotorlineEnemyProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Tracer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tracer"));
    Tracer->SetupAttachment(Root);
    Tracer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedFinder(TEXT("/Game/Missions/Presentation/M_TargetRedGlow.M_TargetRedGlow"));
    if (SphereFinder.Succeeded()) Tracer->SetStaticMesh(SphereFinder.Object);
    if (RedFinder.Succeeded()) Tracer->SetMaterial(0, RedFinder.Object);
    // The projectile actor's local X axis follows Velocity. The sphere's long
    // scale axis is local Z, so rotate it onto X; otherwise incoming rounds
    // present as tall, camera-facing glowing sticks instead of compact tracers.
    Tracer->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    Tracer->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.70f));
    Tracer->SetCastShadow(false);

    TracerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TracerLight"));
    TracerLight->SetupAttachment(Root);
    TracerLight->SetLightColor(FLinearColor(1.0f, 0.01f, 0.0f));
    TracerLight->SetIntensity(12000.0f);
    TracerLight->SetAttenuationRadius(650.0f);

}

void ARotorlineEnemyProjectile::Launch(
    const FVector& Start,
    ARotorlineHelicopterPawn* Target,
    float Damage,
    ERotorlineEnemyWeaponType WeaponType,
    const FVector& InitialAimDirection)
{
    SetActorLocation(Start);
    TargetActor = Target;
    HitDamage = Damage;
    ActiveWeapon = WeaponType;
    BallisticGravityCmPerSecondSquared = 0.0f;
    switch (ActiveWeapon)
    {
    case ERotorlineEnemyWeaponType::MachineGun:
        ProjectileSpeed = 24500.0f;
        MaximumLife = 6.0f;
        HitRadius = 240.0f;
        HomingAcceleration = 0.0f;
        Tracer->SetRelativeScale3D(FVector(0.035f, 0.035f, 0.42f));
        TracerLight->SetIntensity(5000.0f);
        break;
    case ERotorlineEnemyWeaponType::AutoCannon:
        ProjectileSpeed = 28500.0f;
        MaximumLife = 6.0f;
        HitRadius = 275.0f;
        HomingAcceleration = 0.0f;
        Tracer->SetRelativeScale3D(FVector(0.045f, 0.045f, 0.58f));
        TracerLight->SetLightColor(FLinearColor(1.0f, 0.22f, 0.01f));
        TracerLight->SetIntensity(9000.0f);
        if (UMaterialInterface* HotTracer = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Missions/Presentation/M_RocketFlameGlow.M_RocketFlameGlow")))
        {
            Tracer->SetMaterial(0, HotTracer);
        }
        break;
    case ERotorlineEnemyWeaponType::TankShell:
        ProjectileSpeed = 11800.0f;
        MaximumLife = 8.0f;
        HitRadius = 360.0f;
        HomingAcceleration = 0.0f;
        Tracer->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.72f));
        TracerLight->SetIntensity(12000.0f);
        break;
    case ERotorlineEnemyWeaponType::MortarShell:
        ProjectileSpeed = 10500.0f;
        MaximumLife = 7.0f;
        HitRadius = 520.0f;
        HomingAcceleration = 0.0f;
        BallisticGravityCmPerSecondSquared = 980.0f;
        Tracer->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.36f));
        TracerLight->SetLightColor(FLinearColor(1.0f, 0.32f, 0.02f));
        TracerLight->SetIntensity(10000.0f);
        break;
    case ERotorlineEnemyWeaponType::ArtilleryRocket:
        ProjectileSpeed = 19000.0f;
        MaximumLife = 13.5f;
        HitRadius = 500.0f;
        HomingAcceleration = 11500.0f;
        Tracer->SetRelativeScale3D(FVector(0.12f, 0.12f, 1.25f));
        TracerLight->SetIntensity(22000.0f);
        break;
    case ERotorlineEnemyWeaponType::GuidedMissile:
        ProjectileSpeed = 7600.0f;
        MaximumLife = 13.5f;
        HitRadius = 420.0f;
        HomingAcceleration = 4200.0f;
        Tracer->SetRelativeScale3D(FVector(0.12f, 0.12f, 1.25f));
        TracerLight->SetIntensity(22000.0f);
        break;
    default:
        ProjectileSpeed = 12500.0f;
        MaximumLife = 7.0f;
        HitRadius = 300.0f;
        HomingAcceleration = 0.0f;
        Tracer->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.52f));
        TracerLight->SetIntensity(8000.0f);
        break;
    }
    const FVector AimPoint = IsValid(TargetActor) ? TargetActor->GetActorLocation() : Start + FVector::ForwardVector * 10000.0f;
    if (ActiveWeapon == ERotorlineEnemyWeaponType::MortarShell && IsValid(TargetActor))
    {
        // Mortars are genuinely indirect: calculate a high ballistic arc to
        // the aircraft's current track rather than disguising a homing rocket
        // with a mortar label. A vector change after the launch remains the
        // player's defense.
        const float HorizontalDistance = FVector::Dist2D(Start, AimPoint);
        const float FlightTime = FMath::Clamp(HorizontalDistance / 10500.0f, 2.25f, 5.25f);
        const FVector PredictedAim = AimPoint + TargetActor->GetVelocity() * FMath::Min(1.25f, FlightTime * 0.30f);
        const FVector Displacement = PredictedAim - Start;
        Velocity.X = Displacement.X / FlightTime;
        Velocity.Y = Displacement.Y / FlightTime;
        Velocity.Z = (Displacement.Z + 0.5f * BallisticGravityCmPerSecondSquared * FMath::Square(FlightTime)) / FlightTime;
    }
    else if (!InitialAimDirection.IsNearlyZero())
    {
        Velocity = InitialAimDirection.GetSafeNormal() * ProjectileSpeed;
    }
    else
    {
        Velocity = (AimPoint - Start).GetSafeNormal() * ProjectileSpeed;
    }
    SetActorRotation(Velocity.Rotation());
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_COMBAT|SHOT|weapon=%d|damage=%.1f"), static_cast<int32>(ActiveWeapon), HitDamage);
}

void ARotorlineEnemyProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    LifeSeconds += DeltaSeconds;
    if (IsValid(TargetActor) && TargetActor->IsBell222StealthActive())
    {
        Destroy();
        return;
    }
    if (!IsValid(TargetActor) || LifeSeconds > MaximumLife)
    {
        Destroy();
        return;
    }

    const FVector OldLocation = GetActorLocation();
    const FVector TargetLocation = TargetActor->GetActorLocation();
    const FVector ToTarget = TargetLocation - OldLocation;

    if (HomingAcceleration > 0.0f)
    {
        const FVector DesiredVelocity = ToTarget.GetSafeNormal() * ProjectileSpeed;
        Velocity = FMath::VInterpConstantTo(Velocity, DesiredVelocity, DeltaSeconds, HomingAcceleration);
    }
    if (BallisticGravityCmPerSecondSquared > 0.0f)
    {
        Velocity.Z -= BallisticGravityCmPerSecondSquared * DeltaSeconds;
    }
    const FVector NewLocation = OldLocation + Velocity * DeltaSeconds;

    FHitResult WorldHit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RotorlineEnemyProjectileCover), false, this);
    TraceParams.AddIgnoredActor(this);
    if (GetOwner()) TraceParams.AddIgnoredActor(GetOwner());
    const bool bWorldBlocked = GetWorld()->LineTraceSingleByChannel(
        WorldHit,
        OldLocation,
        NewLocation,
        ECC_Visibility,
        TraceParams);

    // Sweep the complete travelled segment and resolve the earliest event.
    // Previously proximity damage happened before any world trace, allowing a
    // round to hurt the player after crossing a building during the same tick.
    const FVector ClosestTargetPoint = FMath::ClosestPointOnSegment(TargetLocation, OldLocation, NewLocation);
    const float TargetTravelDistanceSquared = FVector::DistSquared(OldLocation, ClosestTargetPoint);
    const bool bInsideTarget = FVector::DistSquared(TargetLocation, ClosestTargetPoint) <= FMath::Square(HitRadius);
    const bool bTargetBeforeCover = !bWorldBlocked ||
        TargetTravelDistanceSquared <= FVector::DistSquared(OldLocation, WorldHit.ImpactPoint) + FMath::Square(25.0f);
    if (bInsideTarget && bTargetBeforeCover)
    {
        SetActorLocation(ClosestTargetPoint);
        TargetActor->ApplyEnemyProjectileHit(HitDamage);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COMBAT|PROJECTILE_RESOLVED|result=PLAYER_HIT|cover_before_target=0|weapon=%d"),
            static_cast<int32>(ActiveWeapon));
        Destroy();
        return;
    }
    if (bWorldBlocked)
    {
        SetActorLocation(WorldHit.ImpactPoint);
        if (ActiveWeapon == ERotorlineEnemyWeaponType::MortarShell && IsValid(TargetActor))
        {
            const float ImpactDistance = FVector::Dist(WorldHit.ImpactPoint, TargetActor->GetActorLocation());
            if (ImpactDistance <= 1200.0f)
            {
                const float SplashScale = 1.0f - FMath::Clamp(ImpactDistance / 1200.0f, 0.0f, 0.8f);
                TargetActor->ApplyEnemyProjectileHit(HitDamage * SplashScale);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_MORTAR|IMPACT|result=SPLASH_HIT|distance_m=%.1f|damage=%.1f"),
                    ImpactDistance / 100.0f, HitDamage * SplashScale);
            }
            else
            {
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_MORTAR|IMPACT|result=NEAR_MISS|distance_m=%.1f"),
                    ImpactDistance / 100.0f);
            }
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_COMBAT|PROJECTILE_RESOLVED|result=BLOCKED_BY_WORLD|cover_before_target=1|weapon=%d|blocker=%s"),
            static_cast<int32>(ActiveWeapon),
            *GetNameSafe(WorldHit.GetActor()));
        Destroy();
        return;
    }
    SetActorRotation(Velocity.Rotation());
    SetActorLocation(NewLocation, false);
}
