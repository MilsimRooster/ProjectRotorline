#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineHelipadBeaconActor.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class URotorlineGroundingComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ROTORLINE_API ARotorlineHelipadBeaconActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineHelipadBeaconActor();
    virtual void Tick(float DeltaSeconds) override;

    void Configure(
        bool bInHomeBeaconMode,
        bool bInNightOperations,
        const FVector& DesiredLocation,
        const FString& InSiteId);

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> PadMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<URotorlineGroundingComponent> Grounding;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> BeaconBulbs;

    UPROPERTY()
    TArray<TObjectPtr<UPointLightComponent>> BeaconLights;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> AmberGlowMaterial;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> GreenGlowMaterial;

    bool bHomeBeaconMode = false;
    bool bNightOperations = false;
    FString SiteId = TEXT("HELIPAD");
};
