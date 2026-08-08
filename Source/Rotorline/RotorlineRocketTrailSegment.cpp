#include "RotorlineRocketTrailSegment.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARotorlineRocketTrailSegment::ARotorlineRocketTrailSegment()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Smoke = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Smoke"));
    Smoke->SetupAttachment(Root);
    Smoke->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Smoke->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SmokeFinder(TEXT("/Game/Missions/Presentation/M_RocketSmoke.M_RocketSmoke"));
    if (SphereFinder.Succeeded()) Smoke->SetStaticMesh(SphereFinder.Object);
    if (SmokeFinder.Succeeded()) Smoke->SetMaterial(0, SmokeFinder.Object);
    Smoke->SetRelativeScale3D(FVector(0.28f, 0.42f, 0.42f));
}

void ARotorlineRocketTrailSegment::InitializeTrail(const FVector& Location, const FVector& Direction)
{
    SetActorLocation(Location);
    SetActorRotation(Direction.Rotation());
    DriftVelocity = -Direction.GetSafeNormal() * 55.0f + FVector(0.0f, 0.0f, 115.0f);
    SmokeMaterial = Smoke->CreateDynamicMaterialInstance(0);
    if (SmokeMaterial)
    {
        SmokeMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.52f);
        SmokeMaterial->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.34f, 0.31f, 0.27f, 1.0f));
    }
}

void ARotorlineRocketTrailSegment::InitializeCountermeasure(
    const FVector& Location,
    const FVector& Velocity,
    bool bChaff)
{
    bCountermeasure = true;
    bChaffCountermeasure = bChaff;
    LifetimeSeconds = bChaff ? 1.65f : 2.15f;
    DriftVelocity = Velocity;
    SetActorLocation(Location);
    SetActorRotation(Velocity.Rotation());
    if (!bChaff)
    {
        if (UMaterialInterface* HotMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Game/Missions/Presentation/M_RocketFlameGlow.M_RocketFlameGlow")))
        {
            Smoke->SetMaterial(0, HotMaterial);
        }
        Smoke->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.32f));
    }
    else
    {
        Smoke->SetRelativeScale3D(FVector(0.11f, 0.32f, 0.32f));
        SmokeMaterial = Smoke->CreateDynamicMaterialInstance(0);
        if (SmokeMaterial)
        {
            SmokeMaterial->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.62f, 0.66f, 0.64f, 1.0f));
            SmokeMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.68f);
        }
    }
}

void ARotorlineRocketTrailSegment::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AgeSeconds += DeltaSeconds;
    const float Alpha = FMath::Clamp(AgeSeconds / LifetimeSeconds, 0.0f, 1.0f);
    if (bCountermeasure)
    {
        DriftVelocity.Z -= (bChaffCountermeasure ? 170.0f : 520.0f) * DeltaSeconds;
        DriftVelocity *= FMath::Pow(bChaffCountermeasure ? 0.84f : 0.92f, DeltaSeconds);
        SetActorLocation(GetActorLocation() + DriftVelocity * DeltaSeconds);
        AddActorLocalRotation(FRotator(130.0f, 210.0f, 95.0f) * DeltaSeconds);
        const float Flicker = bChaffCountermeasure ? 1.0f : (0.78f + FMath::Abs(FMath::Sin(AgeSeconds * 31.0f)) * 0.42f);
        const FVector StartScale = bChaffCountermeasure ? FVector(0.11f, 0.32f, 0.32f) : FVector(0.15f, 0.15f, 0.32f);
        Smoke->SetRelativeScale3D(StartScale * FMath::Lerp(Flicker, 0.08f, Alpha));
        if (SmokeMaterial)
        {
            SmokeMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.68f * FMath::Square(1.0f - Alpha));
        }
        if (AgeSeconds >= LifetimeSeconds) Destroy();
        return;
    }
    SetActorLocation(GetActorLocation() + DriftVelocity * DeltaSeconds);
    AddActorLocalRotation(FRotator(13.0f, 24.0f, 9.0f) * DeltaSeconds);
    const FVector Scale(
        FMath::Lerp(0.28f, 2.8f, Alpha),
        FMath::Lerp(0.42f, 2.1f, Alpha),
        FMath::Lerp(0.42f, 2.1f, Alpha));
    Smoke->SetRelativeScale3D(Scale);
    if (SmokeMaterial)
    {
        const FLinearColor TrailTint = FMath::Lerp(
            FLinearColor(0.34f, 0.31f, 0.27f, 1.0f),
            FLinearColor(0.14f, 0.15f, 0.16f, 1.0f),
            Alpha);
        SmokeMaterial->SetVectorParameterValue(TEXT("Tint"), TrailTint);
        SmokeMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.52f * FMath::Square(1.0f - Alpha));
    }
    if (AgeSeconds >= LifetimeSeconds) Destroy();
}
