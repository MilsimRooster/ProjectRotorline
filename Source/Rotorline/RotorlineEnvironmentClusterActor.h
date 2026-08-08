#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineEnvironmentClusterActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/**
 * Baked, deterministic environment dressing for a World Partition cell.
 *
 * Authoring scripts populate the transform arrays once. At runtime the actor
 * renders those transforms through HISM components, avoiding thousands of
 * individual StaticMeshActors while preserving normal World Partition culling.
 */
UCLASS(Blueprintable)
class ROTORLINE_API ARotorlineEnvironmentClusterActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineEnvironmentClusterActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category="Rotorline|Environment")
    void RebuildInstances();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> TallTreeInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> MixedTreeInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> ShrubInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> GrassInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> RockInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> FoundationInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> ParkingInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> CurbInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> FencePostInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Placement")
    TArray<FTransform> RoadsideReflectorInstances;

private:
    static void Populate(UHierarchicalInstancedStaticMeshComponent* Component, const TArray<FTransform>& Instances);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TallTrees;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MixedTrees;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Shrubs;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Grass;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Rocks;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Foundations;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Parking;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Curbs;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FencePosts;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RoadsideReflectors;
};
