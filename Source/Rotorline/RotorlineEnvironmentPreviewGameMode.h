#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RotorlineEnvironmentPreviewGameMode.generated.h"

/**
 * Lightweight, menu-free inspection mode for approval-gated environment work.
 *
 * It is selected explicitly from the launch URL and never replaces the normal
 * Rotorline operations game mode.
 */
UCLASS()
class ROTORLINE_API ARotorlineEnvironmentPreviewGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARotorlineEnvironmentPreviewGameMode();

protected:
    virtual void BeginPlay() override;
};
