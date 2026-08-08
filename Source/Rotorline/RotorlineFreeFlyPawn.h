#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RotorlineFreeFlyPawn.generated.h"

class UCameraComponent;
class USceneComponent;

UCLASS()
class ROTORLINE_API ARotorlineFreeFlyPawn : public APawn
{
    GENERATED_BODY()

public:
    ARotorlineFreeFlyPawn();

    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Preview")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Rotorline|Preview")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Preview")
    float CruiseSpeed = 20000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Preview")
    float BoostMultiplier = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Rotorline|Preview")
    float LookSensitivity = 0.12f;
};
