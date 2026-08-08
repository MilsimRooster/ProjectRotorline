#pragma once

#include "CoreMinimal.h"

class UNiagaraComponent;
class UNiagaraSystem;

namespace RotorlineExplosionFx
{
    ROTORLINE_API UNiagaraComponent* SpawnTransient(
        UObject* WorldContext,
        UNiagaraSystem* System,
        const FVector& Location,
        const FRotator& Rotation,
        const FVector& Scale,
        float DeactivateAfterSeconds = 1.0f,
        float HardLifetimeSeconds = 4.5f);
}
