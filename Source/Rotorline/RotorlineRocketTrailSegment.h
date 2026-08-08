#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineRocketTrailSegment.generated.h"

class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ROTORLINE_API ARotorlineRocketTrailSegment : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineRocketTrailSegment();
    virtual void Tick(float DeltaSeconds) override;
    void InitializeTrail(const FVector& Location, const FVector& Direction);
    void InitializeCountermeasure(const FVector& Location, const FVector& Velocity, bool bChaff);

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Smoke;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> SmokeMaterial;

    FVector DriftVelocity = FVector::ZeroVector;
    float AgeSeconds = 0.0f;
    float LifetimeSeconds = 1.35f;
    bool bCountermeasure = false;
    bool bChaffCountermeasure = false;
};
