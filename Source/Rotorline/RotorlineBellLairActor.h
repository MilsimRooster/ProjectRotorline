#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineBellLairActor.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UChildActorComponent;
class APawn;

UCLASS()
class ROTORLINE_API ARotorlineBellLairActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineBellLairActor();

    virtual void Tick(float DeltaSeconds) override;

    void Configure(bool bInNightOperations);

#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostLoad() override;
    virtual void PostRegisterAllComponents() override;
    virtual void PostEditUndo() override;
#endif

private:
#if WITH_EDITOR
    void EnsureEditorPreviewVisible();
#endif

    void SetHatchOpen(bool bOpen);
    void UpdateHatch(float DeltaSeconds);
    void SetLandscapePassThrough(APawn* PlayerPawn, bool bEnable);

    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    UPROPERTY()
    TObjectPtr<UChildActorComponent> DressingActorComponent;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> PadMesh;

    UPROPERTY()
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RockRim;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> HatchPanels;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> WarningBulbs;

    UPROPERTY()
    TArray<TObjectPtr<UPointLightComponent>> InteriorLights;

    UPROPERTY()
    TArray<TObjectPtr<UPointLightComponent>> WarningLights;

    TArray<FVector> HatchClosedLocations;
    TArray<FVector> HatchOpenLocations;
    float HatchOpenAmount = 0.0f;
    float SettledCloseTime = 0.0f;
    float PreviousPlayerZ = 0.0f;
    float DressingCaptureElapsedSeconds = 0.0f;
    float DressingCaptureRequestTime = 0.0f;
    int32 DressingCaptureIndex = 0;
    bool bHatchCommandOpen = false;
    bool bNightOperations = false;
    bool bLandscapePassThroughEnabled = false;
    bool bUnauthorizedAircraftLogged = false;
    bool bDressingCaptureViewPending = false;
    bool bDressingCaptureCompleteLogged = false;
    bool bDressingReadinessLogged = false;
    TWeakObjectPtr<APawn> LandscapePassThroughPawn;
};
