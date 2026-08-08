#include "RotorlineFreeFlyGameMode.h"

#include "RotorlineEnemyIslandActor.h"
#include "EngineUtils.h"
#include "RotorlineOperationsHUD.h"
#include "RotorlineOperationsPlayerController.h"
#include "RotorlineRuntimePopulation.h"

ARotorlineFreeFlyGameMode::ARotorlineFreeFlyGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ARotorlineOperationsPlayerController::StaticClass();
    HUDClass = ARotorlineOperationsHUD::StaticClass();
}

void ARotorlineFreeFlyGameMode::BeginPlay()
{
    Super::BeginPlay();
    RotorlineRuntimePopulation::Spawn(GetWorld());

    if (UWorld* World = GetWorld())
    {
        bool bAuthoredEnemyIslandFound = false;
        for (TActorIterator<ARotorlineEnemyIslandActor> It(World); It; ++It)
        {
            bAuthoredEnemyIslandFound = true;
            break;
        }
        if (!bAuthoredEnemyIslandFound)
        {
            FActorSpawnParameters SpawnParameters;
            SpawnParameters.Name = TEXT("RotorlineEnemyStagingIsland");
            SpawnParameters.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            World->SpawnActor<ARotorlineEnemyIslandActor>(
                ARotorlineEnemyIslandActor::StaticClass(),
                FVector(650000.0f, 500000.0f, 0.0f),
                FRotator::ZeroRotator,
                SpawnParameters);
        }
    }
}
