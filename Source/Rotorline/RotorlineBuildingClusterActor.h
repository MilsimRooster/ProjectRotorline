#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineBuildingClusterActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/**
 * World Partition-friendly district architecture assembled from modular HISM
 * layers. Authoring tools bake deterministic building shells and facade detail
 * into these arrays while PCG handles lower-risk decorative variation.
 */
UCLASS(Blueprintable)
class ROTORLINE_API ARotorlineBuildingClusterActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineBuildingClusterActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UHierarchicalInstancedStaticMeshComponent* GetShellInstancesComponent() const
    {
        return Shells.Get();
    }

    UFUNCTION(CallInEditor, BlueprintCallable, Category="Rotorline|Environment|Architecture")
    void RebuildBuildingInstances();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> ShellInstances;

    /** Terrain-reaching plinths generated from each shell's complete footprint. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> FoundationInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> AccentInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> RoofInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> WindowInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> LitWindowInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> DoorInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> TrimInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> RooftopInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> IndustrialLargeInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> IndustrialCompactInstances;

    /** Asphalt streets, parking courts, and service aprons built from engine primitives. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> PavementInstances;

    /** Cultivated plots and compacted work yards built from engine primitives. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> FieldInstances;

    /** Water, fuel, and agricultural tanks built from the engine cylinder primitive. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Architecture")
    TArray<FTransform> UtilityTankInstances;

private:
    static void Populate(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const TArray<FTransform>& Instances);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Shells;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Foundations;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Accents;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Roofs;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Windows;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LitWindows;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Doors;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Trim;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RooftopEquipment;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> IndustrialLarge;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> IndustrialCompact;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Pavement;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Fields;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> UtilityTanks;
};
