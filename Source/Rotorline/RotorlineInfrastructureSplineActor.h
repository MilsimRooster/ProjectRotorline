#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineInfrastructureSplineActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class USplineComponent;

/** Reusable spline-driven powerline/roadside infrastructure authoring actor. */
UCLASS(Blueprintable)
class ROTORLINE_API ARotorlineInfrastructureSplineActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineInfrastructureSplineActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(CallInEditor, BlueprintCallable, Category="Rotorline|Environment")
    void RebuildInfrastructure();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment")
    TArray<FVector> ControlPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment", meta=(ClampMin="1500.0"))
    float PostSpacingCm = 6500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment", meta=(ClampMin="300.0"))
    float PostHeightCm = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment")
    float WireSeparationCm = 135.0f;

    /** Snap every generated pole base to the landscape below it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment")
    bool bSnapPostsToLandscape = true;

    /** Small embed prevents a bright gap where the pole meets rolling terrain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Environment", meta=(ClampMin="0.0", ClampMax="100.0"))
    float PostGroundEmbedCm = 8.0f;

private:
    void AddWire(const FVector& Start, const FVector& End);
    FVector ResolveLandscapeBase(const FVector& SplineBase);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USplineComponent> RouteSpline;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Posts;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Crossarms;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Wires;
};
