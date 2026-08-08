#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineVisualBenchmarkActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/**
 * Benchmark-only biome actor that reconstructs complete vegetation assemblies
 * from the trunk/branch/leaf meshes produced by Unreal's GLB importer.
 *
 * It is intentionally separate from ARotorlineEnvironmentClusterActor so the
 * approved island-wide clusters keep their current meshes until the visual
 * benchmark is explicitly accepted.
 */
UCLASS(Blueprintable)
class ROTORLINE_API ARotorlineVisualBenchmarkActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineVisualBenchmarkActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category="Rotorline|Environment|Benchmark")
    void RebuildInstances();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Benchmark")
    TArray<FTransform> TallTreeInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Benchmark")
    TArray<FTransform> MixedTreeInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Benchmark")
    TArray<FTransform> ShrubInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Benchmark")
    TArray<FTransform> GrassInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Benchmark")
    TArray<FTransform> RockInstances;

private:
    static void Populate(UHierarchicalInstancedStaticMeshComponent* Component, const TArray<FTransform>& Instances);
    void GroundGrassInstancesToLandscape();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTreeBranches;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTreeTwigs;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTreeBranchSecondary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTreeBranchTertiary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTreeBranchTertiaryAlt;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTreeLeavesPrimary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTreeLeavesSecondary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTreeLeavesTertiary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTreeRoots;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MixedTreeTrunk;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MixedTreeBranchPrimary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MixedTreeBranchSecondary;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MixedTreeLeaves;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubStems;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubLeaves;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassBlades;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RockBoulders;
};
