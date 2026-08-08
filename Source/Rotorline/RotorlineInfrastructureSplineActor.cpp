#include "RotorlineInfrastructureSplineActor.h"
#include "RotorlineGroundingLibrary.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARotorlineInfrastructureSplineActor::ARotorlineInfrastructureSplineActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetMobility(EComponentMobility::Static);
    RootComponent = SceneRoot;

    RouteSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RouteSpline"));
    RouteSpline->SetupAttachment(SceneRoot);
    RouteSpline->SetMobility(EComponentMobility::Static);
    RouteSpline->SetClosedLoop(false);

    Posts = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Posts"));
    Crossarms = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Crossarms"));
    Wires = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Wires"));
    Posts->SetupAttachment(SceneRoot);
    Crossarms->SetupAttachment(SceneRoot);
    Wires->SetupAttachment(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PostMaterialFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Road_Shoulder.M_Road_Shoulder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WireMaterialFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Asphalt.M_Asphalt"));

    for (UHierarchicalInstancedStaticMeshComponent* Component : {Posts.Get(), Crossarms.Get(), Wires.Get()})
    {
        Component->SetStaticMesh(CubeFinder.Object);
        Component->SetMobility(EComponentMobility::Static);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(Component != Wires);
        Component->bCastDynamicShadow = Component != Wires;
        Component->bCastContactShadow = false;
        Component->SetCullDistances(3500, 240000);
    }
    if (PostMaterialFinder.Succeeded())
    {
        Posts->SetMaterial(0, PostMaterialFinder.Object);
        Crossarms->SetMaterial(0, PostMaterialFinder.Object);
    }
    if (WireMaterialFinder.Succeeded())
    {
        Wires->SetMaterial(0, WireMaterialFinder.Object);
    }
}

void ARotorlineInfrastructureSplineActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildInfrastructure();
}

void ARotorlineInfrastructureSplineActor::AddWire(const FVector& Start, const FVector& End)
{
    const FVector Delta = End - Start;
    const float Length = Delta.Size();
    if (Length <= 1.0f)
    {
        return;
    }

    const FRotator Rotation = Delta.Rotation();
    Wires->AddInstance(FTransform(Rotation, (Start + End) * 0.5f, FVector(Length / 100.0f, 0.018f, 0.018f)), false);
}

FVector ARotorlineInfrastructureSplineActor::ResolveLandscapeBase(const FVector& SplineBase)
{
    if (!bSnapPostsToLandscape || !GetWorld())
    {
        return SplineBase;
    }

    FVector WorldBase = GetActorTransform().TransformPosition(SplineBase);
    FRotorlineGroundingProfile Profile = URotorlineGroundingLibrary::MakeProfile(
        ERotorlineGroundingMode::LinearPoint, TEXT("InfrastructurePost"));
    Profile.ContactSinkCm = PostGroundEmbedCm;
    Profile.bAllowPreparedGround = false;
    Profile.bCheckCollisionPenetration = false;
    FRotorlineGroundingResult Result;
    if (URotorlineGroundingLibrary::SolveGroundContact(
        this, WorldBase, FVector2D::ZeroVector, this,
        Profile, Result))
    {
        WorldBase.Z = Result.ContactPoint.Z;
        return GetActorTransform().InverseTransformPosition(WorldBase);
    }

    return SplineBase;
}

void ARotorlineInfrastructureSplineActor::RebuildInfrastructure()
{
    RouteSpline->ClearSplinePoints(false);
    for (int32 Index = 0; Index < ControlPoints.Num(); ++Index)
    {
        RouteSpline->AddSplinePoint(ControlPoints[Index], ESplineCoordinateSpace::Local, false);
        RouteSpline->SetSplinePointType(Index, ESplinePointType::CurveClamped, false);
    }
    RouteSpline->UpdateSpline();

    Posts->ClearInstances();
    Crossarms->ClearInstances();
    Wires->ClearInstances();

    if (ControlPoints.Num() < 2)
    {
        return;
    }

    const float Length = RouteSpline->GetSplineLength();
    const int32 SegmentCount = FMath::Max(1, FMath::CeilToInt(Length / FMath::Max(PostSpacingCm, 1500.0f)));
    TArray<FVector> WireCenters;
    TArray<FVector> WireLefts;
    TArray<FVector> WireRights;
    WireCenters.Reserve(SegmentCount + 1);
    WireLefts.Reserve(SegmentCount + 1);
    WireRights.Reserve(SegmentCount + 1);

    for (int32 Index = 0; Index <= SegmentCount; ++Index)
    {
        const float Distance = Length * static_cast<float>(Index) / static_cast<float>(SegmentCount);
        const FVector SplineBase = RouteSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
        const FVector Base = ResolveLandscapeBase(SplineBase);
        const FRotator SplineRotation = RouteSpline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
        // Utility poles stay vertical on hills. Only their heading follows the route.
        const FRotator Rotation(0.0f, SplineRotation.Yaw, 0.0f);
        const FVector Right = Rotation.RotateVector(FVector::RightVector);
        const FVector Top = Base + FVector(0.0f, 0.0f, PostHeightCm);

        Posts->AddInstance(FTransform(Rotation, Base + FVector(0.0f, 0.0f, PostHeightCm * 0.5f), FVector(0.16f, 0.16f, PostHeightCm / 100.0f)), false);
        Crossarms->AddInstance(FTransform(Rotation, Top - FVector(0.0f, 0.0f, 55.0f), FVector(0.14f, 3.4f, 0.14f)), false);

        WireCenters.Add(Top + FVector(0.0f, 0.0f, 20.0f));
        WireLefts.Add(Top - Right * WireSeparationCm);
        WireRights.Add(Top + Right * WireSeparationCm);
    }

    for (int32 Index = 1; Index < WireCenters.Num(); ++Index)
    {
        AddWire(WireCenters[Index - 1], WireCenters[Index]);
        AddWire(WireLefts[Index - 1], WireLefts[Index]);
        AddWire(WireRights[Index - 1], WireRights[Index]);
    }

    Posts->BuildTreeIfOutdated(true, true);
    Crossarms->BuildTreeIfOutdated(true, true);
    Wires->BuildTreeIfOutdated(true, true);
}
