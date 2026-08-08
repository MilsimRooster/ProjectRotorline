#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace RotorlineRuntimePopulation
{
    /** Spawn the island population without modifying or resaving the canonical map. */
    ROTORLINE_API void Spawn(UWorld* World);
}
