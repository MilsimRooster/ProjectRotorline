#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineRoadNetworkActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/**
 * Regional road chunk rendered through HISM components.
 *
 * The build script harvests the exact transforms of the proven road geometry,
 * so the conversion reduces actor/World Partition overhead without changing
 * collision, route alignment, markings, or terrain conformity.
 */
UCLASS(Blueprintable)
class ROTORLINE_API ARotorlineRoadNetworkActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineRoadNetworkActor();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category="Rotorline|Environment|Roads")
    void RebuildRoadInstances();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Roads")
    TArray<FTransform> ShoulderInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Roads")
    TArray<FTransform> PavementInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Roads")
    TArray<FTransform> CenterlineInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Roads")
    TArray<FTransform> EdgeLineInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Roads")
    TArray<FTransform> DitchInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Roads")
    TArray<FTransform> GuardrailInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Roads")
    TArray<FTransform> CulvertInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment|Roads")
    TArray<FTransform> ReflectorInstances;

private:
    void ApplyProductionMaterials();
    void BuildRuntimeMarkingsFromPavement();

    static void Populate(UHierarchicalInstancedStaticMeshComponent* Component, const TArray<FTransform>& Instances);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Shoulders;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Pavement;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Centerlines;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> EdgeLines;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Ditches;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Guardrails;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Culverts;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Reflectors;
};
