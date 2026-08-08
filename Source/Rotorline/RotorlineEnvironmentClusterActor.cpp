#include "RotorlineEnvironmentClusterActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineEnvironment
{
    void ConfigureHISM(
        UHierarchicalInstancedStaticMeshComponent* Component,
        UStaticMesh* Mesh,
        UMaterialInterface* Material,
        const int32 StartCullDistance,
        const int32 EndCullDistance,
        const bool bCollision,
        const bool bCastShadow)
    {
        Component->SetStaticMesh(Mesh);
        Component->SetMobility(EComponentMobility::Static);
        Component->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Component->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
        Component->SetCastShadow(bCastShadow);
        Component->bCastDynamicShadow = bCastShadow;
        Component->bCastContactShadow = bCastShadow;
        Component->SetCullDistances(StartCullDistance, EndCullDistance);
        Component->bEnableDensityScaling = true;
        Component->bAffectDistanceFieldLighting = bCastShadow;
        if (Material)
        {
            Component->SetMaterial(0, Material);
        }
    }
}

ARotorlineEnvironmentClusterActor::ARotorlineEnvironmentClusterActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetMobility(EComponentMobility::Static);
    RootComponent = SceneRoot;

    TallTrees = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTrees"));
    MixedTrees = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MixedTrees"));
    Shrubs = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Shrubs"));
    Grass = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Grass"));
    Rocks = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Rocks"));
    Foundations = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Foundations"));
    Parking = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Parking"));
    Curbs = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Curbs"));
    FencePosts = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("FencePosts"));
    RoadsideReflectors = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RoadsideReflectors"));

    for (UHierarchicalInstancedStaticMeshComponent* Component : {
        TallTrees.Get(), MixedTrees.Get(), Shrubs.Get(), Grass.Get(), Rocks.Get(), Foundations.Get(), Parking.Get(),
        Curbs.Get(), FencePosts.Get(), RoadsideReflectors.Get()})
    {
        Component->SetupAttachment(SceneRoot);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> TallTreeFinder(
        TEXT("/Game/Environment/Imported/Vegetation/LowPolyForest/low_poly_forest_tree_pack/StaticMeshes/Background_Tree_Atlas_005_Background_Tree_Atlas_0.Background_Tree_Atlas_005_Background_Tree_Atlas_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MixedTreeFinder(
        TEXT("/Game/Environment/Imported/Vegetation/LowPolyForest/low_poly_forest_tree_pack/StaticMeshes/Background_Tree_Atlas_003_Background_Tree_Atlas_0.Background_Tree_Atlas_003_Background_Tree_Atlas_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShrubFinder(
        TEXT("/Game/Environment/Imported/Vegetation/LowPolyForest/low_poly_forest_tree_pack/StaticMeshes/Background_Tree_Atlas_010_Background_Tree_Atlas_0.Background_Tree_Atlas_010_Background_Tree_Atlas_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassFinder(
        TEXT("/Game/Environment/Nature/Grass/GeneratedMesh/SM_GrassClump/StaticMeshes/SM_GrassClump.SM_GrassClump"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RockFinder(
        TEXT("/Game/Environment/Imported/StandaloneRocks/low-poly-sculpted-boulder/StaticMeshes/low-poly-sculpted-boulder.low-poly-sculpted-boulder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ConcreteFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Concrete.M_Concrete"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AsphaltFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Asphalt.M_Asphalt"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShoulderFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Road_Shoulder.M_Road_Shoulder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Marking_White.M_Marking_White"));

    RotorlineEnvironment::ConfigureHISM(TallTrees, TallTreeFinder.Object, nullptr, 7000, 320000, true, true);
    RotorlineEnvironment::ConfigureHISM(MixedTrees, MixedTreeFinder.Object, nullptr, 5000, 280000, true, true);
    RotorlineEnvironment::ConfigureHISM(Shrubs, ShrubFinder.Object, nullptr, 2500, 120000, false, false);
    RotorlineEnvironment::ConfigureHISM(Grass, GrassFinder.Object, nullptr, 1500, 110000, false, false);
    RotorlineEnvironment::ConfigureHISM(Rocks, RockFinder.Object, nullptr, 2500, 220000, true, true);
    RotorlineEnvironment::ConfigureHISM(Foundations, CubeFinder.Object, ConcreteFinder.Object, 0, 220000, true, true);
    RotorlineEnvironment::ConfigureHISM(Parking, CubeFinder.Object, AsphaltFinder.Object, 0, 200000, true, true);
    RotorlineEnvironment::ConfigureHISM(Curbs, CubeFinder.Object, ConcreteFinder.Object, 0, 140000, true, true);
    RotorlineEnvironment::ConfigureHISM(FencePosts, CubeFinder.Object, ShoulderFinder.Object, 2500, 120000, true, true);
    RotorlineEnvironment::ConfigureHISM(RoadsideReflectors, CubeFinder.Object, WhiteFinder.Object, 1500, 120000, false, false);
}

void ARotorlineEnvironmentClusterActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildInstances();
}

void ARotorlineEnvironmentClusterActor::Populate(
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

void ARotorlineEnvironmentClusterActor::RebuildInstances()
{
    Populate(TallTrees, TallTreeInstances);
    Populate(MixedTrees, MixedTreeInstances);
    Populate(Shrubs, ShrubInstances);
    Populate(Grass, GrassInstances);
    Populate(Rocks, RockInstances);
    Populate(Foundations, FoundationInstances);
    Populate(Parking, ParkingInstances);
    Populate(Curbs, CurbInstances);
    Populate(FencePosts, FencePostInstances);
    Populate(RoadsideReflectors, RoadsideReflectorInstances);
}
