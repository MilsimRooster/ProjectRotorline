#include "RotorlineVisualBenchmarkActor.h"

#include "RotorlineGroundingLibrary.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineVisualBenchmark
{
    void Configure(
        UHierarchicalInstancedStaticMeshComponent* Component,
        UStaticMesh* Mesh,
        const bool bCollision,
        const bool bCastShadow,
        const int32 StartCullDistance,
        const int32 EndCullDistance)
    {
        Component->SetStaticMesh(Mesh);
        Component->SetMobility(EComponentMobility::Static);
        Component->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Component->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
        Component->SetCastShadow(bCastShadow);
        Component->bCastDynamicShadow = bCastShadow;
        Component->bCastContactShadow = bCastShadow;
        Component->bAffectDistanceFieldLighting = bCastShadow;
        Component->bEnableDensityScaling = false;
        Component->SetCullDistances(StartCullDistance, EndCullDistance);
    }
}

ARotorlineVisualBenchmarkActor::ARotorlineVisualBenchmarkActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetMobility(EComponentMobility::Static);
    RootComponent = SceneRoot;

    TallTreeBranches = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTree_Branches"));
    TallTreeTwigs = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTree_Twigs"));
    TallTreeBranchSecondary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTree_BranchSecondary"));
    TallTreeBranchTertiary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTree_BranchTertiary"));
    TallTreeBranchTertiaryAlt = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTree_BranchTertiaryAlt"));
    TallTreeLeavesPrimary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTree_LeavesPrimary"));
    TallTreeLeavesSecondary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTree_LeavesSecondary"));
    TallTreeLeavesTertiary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTree_LeavesTertiary"));
    TallTreeRoots = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TallTree_Roots"));
    MixedTreeTrunk = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MixedTree_Trunk"));
    MixedTreeBranchPrimary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MixedTree_BranchPrimary"));
    MixedTreeBranchSecondary = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MixedTree_BranchSecondary"));
    MixedTreeLeaves = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MixedTree_Leaves"));
    ShrubStems = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Shrub_Stems"));
    ShrubLeaves = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Shrub_Leaves"));
    GrassBlades = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Grass_Blades"));
    RockBoulders = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Rock_Boulders"));

    for (UHierarchicalInstancedStaticMeshComponent* Component : {
        TallTreeBranches.Get(), TallTreeTwigs.Get(), TallTreeBranchSecondary.Get(), TallTreeBranchTertiary.Get(),
        TallTreeBranchTertiaryAlt.Get(), TallTreeLeavesPrimary.Get(), TallTreeLeavesSecondary.Get(),
        TallTreeLeavesTertiary.Get(), TallTreeRoots.Get(), MixedTreeTrunk.Get(), MixedTreeBranchPrimary.Get(),
        MixedTreeBranchSecondary.Get(), MixedTreeLeaves.Get(), ShrubStems.Get(), ShrubLeaves.Get(),
        GrassBlades.Get(), RockBoulders.Get()})
    {
        Component->SetupAttachment(SceneRoot);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> TallBranchesFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "WGJ_Maple01_01__Shape_Trunk_0.WGJ_Maple01_01__Shape_Trunk_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TallTwigsFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "WGJ_Maple01_01__Shape_Branch_1_0.WGJ_Maple01_01__Shape_Branch_1_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TallBranchSecondaryFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "WGJ_Maple01_01__Shape_Branch_2_0.WGJ_Maple01_01__Shape_Branch_2_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TallBranchTertiaryFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "WGJ_Maple01_01__Shape_Branch_3_0.WGJ_Maple01_01__Shape_Branch_3_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TallLeavesPrimaryFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "WGJ_Maple01_01__Shape_Leaf_0.WGJ_Maple01_01__Shape_Leaf_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MixedTrunkFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "WGJ_Maple04_01_Shape_Trunk_0.WGJ_Maple04_01_Shape_Trunk_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MixedBranchPrimaryFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "WGJ_Maple04_01_Shape_Branch_2_0.WGJ_Maple04_01_Shape_Branch_2_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MixedBranchSecondaryFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "WGJ_Maple04_01_Shape_Branch_3_0.WGJ_Maple04_01_Shape_Branch_3_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MixedLeavesFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "WGJ_Maple04_01_Shape_Leaf_0.WGJ_Maple04_01_Shape_Leaf_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShrubStemsFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "MH_Fern01_01_Shape_Stem_0.MH_Fern01_01_Shape_Stem_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShrubLeavesFinder(TEXT(
        "/Game/Environment/Benchmark/SourceTreesVegetationPack/trees_vegetation_pack/StaticMeshes/"
        "MH_Fern01_01_Shape_Leaf_0.MH_Fern01_01_Shape_Leaf_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassFinder(TEXT(
        "/Game/Environment/Nature/Grass/GeneratedMesh/SM_GrassClump/StaticMeshes/"
        "SM_GrassClump.SM_GrassClump"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RockFinder(TEXT(
        "/Game/Environment/Imported/StandaloneRocks/low-poly-grey-boulder/StaticMeshes/"
        "low-poly-grey-boulder.low-poly-grey-boulder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> TallCanopyMaterialFinder(TEXT(
        "/Game/Environment/Benchmark/Materials/Trees/M_Benchmark_TallCanopy."
        "M_Benchmark_TallCanopy"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MixedCanopyMaterialFinder(TEXT(
        "/Game/Environment/Benchmark/Materials/Trees/M_Benchmark_MixedCanopy."
        "M_Benchmark_MixedCanopy"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShrubCanopyMaterialFinder(TEXT(
        "/Game/Environment/Benchmark/Materials/Trees/M_Benchmark_ShrubCanopy."
        "M_Benchmark_ShrubCanopy"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrunkMaterialFinder(TEXT(
        "/Game/Environment/Benchmark/Materials/Trees/M_Benchmark_TreeTrunk."
        "M_Benchmark_TreeTrunk"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MapleLeavesMaterialFinder(TEXT(
        "/Game/Environment/Benchmark/Materials/Foliage/M_Benchmark_MapleLeaves."
        "M_Benchmark_MapleLeaves"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> FernLeavesMaterialFinder(TEXT(
        "/Game/Environment/Benchmark/Materials/Foliage/M_Benchmark_FernLeaves."
        "M_Benchmark_FernLeaves"));
    RotorlineVisualBenchmark::Configure(TallTreeBranches, TallBranchesFinder.Object, false, true, 8000, 260000);
    RotorlineVisualBenchmark::Configure(TallTreeTwigs, TallTwigsFinder.Object, true, true, 8000, 260000);
    RotorlineVisualBenchmark::Configure(TallTreeBranchSecondary, TallBranchSecondaryFinder.Object, false, true, 8000, 260000);
    RotorlineVisualBenchmark::Configure(TallTreeBranchTertiary, TallBranchTertiaryFinder.Object, false, true, 8000, 260000);
    RotorlineVisualBenchmark::Configure(TallTreeBranchTertiaryAlt, nullptr, false, false, 8000, 260000);
    RotorlineVisualBenchmark::Configure(TallTreeLeavesPrimary, TallLeavesPrimaryFinder.Object, false, true, 8000, 260000);
    RotorlineVisualBenchmark::Configure(TallTreeLeavesSecondary, nullptr, false, false, 8000, 260000);
    RotorlineVisualBenchmark::Configure(TallTreeLeavesTertiary, nullptr, false, false, 8000, 260000);
    RotorlineVisualBenchmark::Configure(TallTreeRoots, nullptr, false, false, 8000, 260000);
    RotorlineVisualBenchmark::Configure(MixedTreeTrunk, MixedTrunkFinder.Object, false, true, 7000, 220000);
    RotorlineVisualBenchmark::Configure(MixedTreeBranchPrimary, MixedBranchPrimaryFinder.Object, true, true, 7000, 220000);
    RotorlineVisualBenchmark::Configure(MixedTreeBranchSecondary, MixedBranchSecondaryFinder.Object, false, true, 7000, 220000);
    RotorlineVisualBenchmark::Configure(MixedTreeLeaves, MixedLeavesFinder.Object, false, true, 7000, 220000);
    RotorlineVisualBenchmark::Configure(ShrubStems, ShrubStemsFinder.Object, false, true, 2500, 110000);
    RotorlineVisualBenchmark::Configure(ShrubLeaves, ShrubLeavesFinder.Object, false, true, 2500, 110000);
    RotorlineVisualBenchmark::Configure(GrassBlades, GrassFinder.Object, false, false, 1200, 85000);
    RotorlineVisualBenchmark::Configure(RockBoulders, RockFinder.Object, true, true, 2500, 180000);

    for (UHierarchicalInstancedStaticMeshComponent* Component : {
        TallTreeBranches.Get(), TallTreeTwigs.Get(), TallTreeBranchSecondary.Get(), TallTreeBranchTertiary.Get()})
    {
        Component->SetMaterial(0, TrunkMaterialFinder.Object);
    }
    TallTreeLeavesPrimary->SetMaterial(0, MapleLeavesMaterialFinder.Object);
    MixedTreeTrunk->SetMaterial(0, TrunkMaterialFinder.Object);
    MixedTreeBranchPrimary->SetMaterial(0, TrunkMaterialFinder.Object);
    MixedTreeBranchSecondary->SetMaterial(0, TrunkMaterialFinder.Object);
    MixedTreeLeaves->SetMaterial(0, MapleLeavesMaterialFinder.Object);
    ShrubStems->SetMaterial(0, TrunkMaterialFinder.Object);
    ShrubLeaves->SetMaterial(0, FernLeavesMaterialFinder.Object);
}

void ARotorlineVisualBenchmarkActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildInstances();
    if (GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
    {
        GroundGrassInstancesToLandscape();
    }
}

void ARotorlineVisualBenchmarkActor::BeginPlay()
{
    Super::BeginPlay();
    GroundGrassInstancesToLandscape();
}

void ARotorlineVisualBenchmarkActor::Populate(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const TArray<FTransform>& Instances)
{
    if (!Component)
    {
        return;
    }

    Component->ClearInstances();
    if (!Component->GetStaticMesh())
    {
        return;
    }
    Component->PreAllocateInstancesMemory(Instances.Num());
    for (const FTransform& Instance : Instances)
    {
        Component->AddInstance(Instance, false);
    }
    Component->BuildTreeIfOutdated(true, true);
}

void ARotorlineVisualBenchmarkActor::RebuildInstances()
{
    Populate(TallTreeBranches, TallTreeInstances);
    Populate(TallTreeTwigs, TallTreeInstances);
    Populate(TallTreeBranchSecondary, TallTreeInstances);
    Populate(TallTreeBranchTertiary, TallTreeInstances);
    Populate(TallTreeBranchTertiaryAlt, TallTreeInstances);
    Populate(TallTreeLeavesPrimary, TallTreeInstances);
    Populate(TallTreeLeavesSecondary, TallTreeInstances);
    Populate(TallTreeLeavesTertiary, TallTreeInstances);
    Populate(TallTreeRoots, TallTreeInstances);
    Populate(MixedTreeTrunk, MixedTreeInstances);
    Populate(MixedTreeBranchPrimary, MixedTreeInstances);
    Populate(MixedTreeBranchSecondary, MixedTreeInstances);
    Populate(MixedTreeLeaves, MixedTreeInstances);
    Populate(ShrubStems, ShrubInstances);
    Populate(ShrubLeaves, ShrubInstances);
    Populate(GrassBlades, GrassInstances);
    Populate(RockBoulders, RockInstances);
}

void ARotorlineVisualBenchmarkActor::GroundGrassInstancesToLandscape()
{
    if (!GrassBlades || !GetWorld() || GrassBlades->GetInstanceCount() == 0)
    {
        return;
    }

    FRotorlineGroundingProfile Profile = URotorlineGroundingLibrary::MakeProfile(
        ERotorlineGroundingMode::Upright, TEXT("VisualGrassLandscape"));
    Profile.bAllowLandscape = true;
    Profile.bAllowPreparedGround = false;
    Profile.bRejectObstructionsAboveGround = false;
    Profile.bRequireAllSamples = false;
    Profile.bCheckCollisionPenetration = false;
    Profile.MaximumSlopeDegrees = 89.0f;
    Profile.ContactSinkCm = 2.0f;

    bool bUpdatedAnyInstance = false;
    for (int32 InstanceIndex = 0; InstanceIndex < GrassBlades->GetInstanceCount(); ++InstanceIndex)
    {
        FRotorlineGroundingResult Result;
        if (!URotorlineGroundingLibrary::SolveInstancedMeshGrounding(
                GrassBlades, InstanceIndex, Profile, Result))
        {
            continue;
        }

        bUpdatedAnyInstance |= GrassBlades->UpdateInstanceTransform(
            InstanceIndex, Result.DesiredTransform, true, false, true);
    }

    if (bUpdatedAnyInstance)
    {
        GrassBlades->BuildTreeIfOutdated(true, true);
        GrassBlades->MarkRenderStateDirty();
    }
}
