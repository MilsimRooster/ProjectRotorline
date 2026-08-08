#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineCabinSupplyConvoyActor.generated.h"

class ARotorlineHelicopterPawn;
class ARotorlineMissionObjectiveActor;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Mission 23's independent airfield-to-cabin supply column.
 *
 * This actor derives its route from the serialized production pavement and
 * owns only the live Warehouse Supply Run state machine.
 */
UCLASS()
class ROTORLINE_API ARotorlineCabinSupplyConvoyActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineCabinSupplyConvoyActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    bool ConfigureAndStart(ARotorlineHelicopterPawn* InPlayer);
    bool IsMissionSucceeded() const { return bMissionSucceeded; }
    bool IsMissionFailed() const { return bMissionFailed; }
    float GetProgressFraction() const;
    FVector GetLeadWorldLocation() const;
    FString GetStatusText() const;

private:
    bool BuildProductionRoadRoute();
    void SpawnSupplyColumn();
    void PlaceVehicle(int32 UnitIndex, float RouteDistanceCm);
    FVector SampleRoute(float DistanceCm, FVector* OutDirection = nullptr) const;
    USceneComponent* CreateVehicleRoot(const FString& Name);
    void AddVehicleMesh(USceneComponent* Parent, const FString& Name, const TCHAR* AssetPath);
    void UpdateThreatStages();
    ARotorlineMissionObjectiveActor* SpawnThreat(
        const FString& TargetId,
        const FString& Label,
        float RouteFraction,
        float LateralOffsetCm,
        float HeightOffsetCm);

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TObjectPtr<ARotorlineHelicopterPawn> Player;

    UPROPERTY()
    TArray<TObjectPtr<USceneComponent>> VehicleRoots;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> VehicleMeshes;

    TArray<FVector> RoutePoints;
    TArray<float> RouteDistances;
    UPROPERTY()
    TArray<TObjectPtr<ARotorlineMissionObjectiveActor>> SpawnedThreats;
    float RouteLengthCm = 0.0f;
    float LeadDistanceCm = 0.0f;
    float ConvoySpeedCmPerSecond = 2700.0f;
    float VehicleSpacingCm = 1800.0f;
    bool bStarted = false;
    bool bMissionSucceeded = false;
    bool bMissionFailed = false;
    uint8 SpawnedThreatStageMask = 0;
};
