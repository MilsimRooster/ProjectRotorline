#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineEnemyIslandAssaultActor.generated.h"

class ARotorlineMissionObjectiveActor;
class UWorld;

UCLASS()
class ROTORLINE_API ARotorlineEnemyIslandAssaultActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineEnemyIslandAssaultActor();

    static void Deploy(UWorld* World);
    static void Clear(UWorld* World);

    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    virtual void BeginPlay() override;

private:
    void SpawnGroundGarrison();
    void StartAerialAssault();
    void SpawnAerialWave(int32 WaveIndex);
    ARotorlineMissionObjectiveActor* SpawnThreat(
        const FString& Label,
        const FString& Target,
        const FVector& Location);

    TArray<TWeakObjectPtr<ARotorlineMissionObjectiveActor>> SpawnedThreats;
    FTimerHandle WaveTwoTimer;
    FTimerHandle WaveThreeTimer;
    bool bAerialAssaultStarted = false;
};
