#include "RotorlineBuildingClusterActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineBuildings
{
    void Configure(
        UHierarchicalInstancedStaticMeshComponent* Component,
        UStaticMesh* Mesh,
        UMaterialInterface* Material,
        const bool bCollision,
        const bool bCastShadow,
        const int32 CullDistance)
    {
        Component->SetStaticMesh(Mesh);
        Component->SetMobility(EComponentMobility::Static);
        Component->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Component->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
        Component->SetCastShadow(bCastShadow);
        Component->bCastDynamicShadow = bCastShadow;
        Component->bCastContactShadow = bCastShadow;
        Component->bAffectDistanceFieldLighting = bCastShadow;
        Component->SetCullDistances(0, CullDistance);
        Component->bEnableDensityScaling = false;
        if (Material)
        {
            Component->SetMaterial(0, Material);
        }
    }
}

ARotorlineBuildingClusterActor::ARotorlineBuildingClusterActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetMobility(EComponentMobility::Static);
    RootComponent = SceneRoot;

    Shells = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Shells"));
    Foundations = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Foundations"));
    Accents = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Accents"));
    Roofs = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Roofs"));
    Windows = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Windows"));
    LitWindows = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("LitWindows"));
    Doors = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Doors"));
    Trim = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Trim"));
    RooftopEquipment = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RooftopEquipment"));
    IndustrialLarge = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("IndustrialLarge"));
    IndustrialCompact = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("IndustrialCompact"));
    Pavement = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Pavement"));
    Fields = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Fields"));
    UtilityTanks = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("UtilityTanks"));

    for (UHierarchicalInstancedStaticMeshComponent* Component : {
        Shells.Get(), Foundations.Get(), Accents.Get(), Roofs.Get(), Windows.Get(), LitWindows.Get(),
        Doors.Get(), Trim.Get(), RooftopEquipment.Get(), IndustrialLarge.Get(), IndustrialCompact.Get(),
        Pavement.Get(), Fields.Get(), UtilityTanks.Get()})
    {
        Component->SetupAttachment(SceneRoot);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Urban_Wall_Production.M_Urban_Wall_Production"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AccentFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Urban_Accent_Production.M_Urban_Accent_Production"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoofFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Urban_Roof_Production.M_Urban_Roof_Production"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WindowFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Urban_Window_Dark.M_Urban_Window_Dark"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> LitWindowFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Urban_Window_Lit.M_Urban_Window_Lit"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DoorFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Urban_Metal.M_Urban_Metal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrimFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Urban_Trim.M_Urban_Trim"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AsphaltFinder(
        TEXT("/Game/Environment/Materials/Roads/M_RoadAsphalt_Visibility.M_RoadAsphalt_Visibility"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> EarthFinder(
        TEXT("/Game/Environment/Materials/Roads/M_RoadShoulder_Visibility.M_RoadShoulder_Visibility"));

    RotorlineBuildings::Configure(Shells, CubeFinder.Object, WallFinder.Object, true, true, 500000);
    RotorlineBuildings::Configure(Foundations, CubeFinder.Object, TrimFinder.Object, true, true, 500000);
    Foundations->ComponentTags.AddUnique(TEXT("RotorlineApprovedGround"));
    RotorlineBuildings::Configure(Accents, CubeFinder.Object, AccentFinder.Object, true, true, 500000);
    RotorlineBuildings::Configure(Roofs, CubeFinder.Object, RoofFinder.Object, true, true, 500000);
    RotorlineBuildings::Configure(Windows, CubeFinder.Object, WindowFinder.Object, false, false, 240000);
    RotorlineBuildings::Configure(LitWindows, CubeFinder.Object, LitWindowFinder.Object, false, false, 260000);
    RotorlineBuildings::Configure(Doors, CubeFinder.Object, DoorFinder.Object, true, true, 260000);
    RotorlineBuildings::Configure(Trim, CubeFinder.Object, TrimFinder.Object, false, true, 300000);
    RotorlineBuildings::Configure(RooftopEquipment, CubeFinder.Object, DoorFinder.Object, true, true, 300000);
    // Retain the serialized legacy layers for map compatibility, but never
    // render imported GLB buildings. Population authoring converts their
    // transforms into the same procedural cube language used by every site.
    RotorlineBuildings::Configure(IndustrialLarge, CubeFinder.Object, WallFinder.Object, true, true, 650000);
    RotorlineBuildings::Configure(IndustrialCompact, CubeFinder.Object, WallFinder.Object, true, true, 500000);
    RotorlineBuildings::Configure(Pavement, CubeFinder.Object, AsphaltFinder.Object, true, false, 500000);
    RotorlineBuildings::Configure(Fields, CubeFinder.Object, EarthFinder.Object, true, false, 500000);
    RotorlineBuildings::Configure(UtilityTanks, CylinderFinder.Object, DoorFinder.Object, true, true, 500000);
}

void ARotorlineBuildingClusterActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildBuildingInstances();
}

void ARotorlineBuildingClusterActor::Populate(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const TArray<FTransform>& Instances)
{
    if (!Component)
    {
        return;
    }
    Component->ClearInstances();
    Component->PreAllocateInstancesMemory(Instances.Num());
    for (const FTransform& Instance : Instances)
    {
        Component->AddInstance(Instance, false);
    }
    Component->BuildTreeIfOutdated(true, true);
}

void ARotorlineBuildingClusterActor::RebuildBuildingInstances()
{
    // The original harbor GLB shells had separate procedural cap roofs. If the
    // imported shell layer is present, suppress both halves together; rendering
    // only the cap is what produced the floating rooftop remnants.
    const bool bLegacyImportedIndustrial =
        IndustrialLargeInstances.Num() > 0 || IndustrialCompactInstances.Num() > 0;
    static const TArray<FTransform> EmptyInstances;

    Populate(Shells, ShellInstances);
    Populate(Foundations, FoundationInstances);
    Populate(Accents, AccentInstances);
    Populate(Roofs, bLegacyImportedIndustrial ? EmptyInstances : RoofInstances);
    Populate(Windows, WindowInstances);
    Populate(LitWindows, LitWindowInstances);
    Populate(Doors, DoorInstances);
    Populate(Trim, TrimInstances);
    Populate(RooftopEquipment, RooftopInstances);
    Populate(IndustrialLarge, EmptyInstances);
    Populate(IndustrialCompact, EmptyInstances);
    Populate(Pavement, PavementInstances);
    Populate(Fields, FieldInstances);
    Populate(UtilityTanks, UtilityTankInstances);
}
