#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineCaveTransitionActor.generated.h"

class UAudioComponent;
class UBoxComponent;
class UPointLightComponent;
class USceneComponent;

UCLASS()
class ROTORLINE_API ARotorlineCaveTransitionActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineCaveTransitionActor();

private:
    UFUNCTION()
    void HandleEntryOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UPROPERTY()
    USceneComponent* Root = nullptr;

    UPROPERTY()
    UBoxComponent* EntryTrigger = nullptr;

    UPROPERTY()
    UAudioComponent* CaveAmbience = nullptr;

    UPROPERTY()
    TArray<UPointLightComponent*> EntranceLights;

    bool bTransitionStarted = false;
};
