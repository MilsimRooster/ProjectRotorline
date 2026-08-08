#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RotorlineFreeFlyGameMode.generated.h"

UCLASS()
class ROTORLINE_API ARotorlineFreeFlyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARotorlineFreeFlyGameMode();

protected:
    virtual void BeginPlay() override;
};
