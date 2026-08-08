#include "RotorlineExplosionFx.h"

#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

UNiagaraComponent* RotorlineExplosionFx::SpawnTransient(
    UObject* WorldContext,
    UNiagaraSystem* System,
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Scale,
    float DeactivateAfterSeconds,
    float HardLifetimeSeconds)
{
    if (!WorldContext || !System)
    {
        return nullptr;
    }

    UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        WorldContext, System, Location, Rotation, Scale,
        false, true, ENCPoolMethod::None, true);
    UWorld* World = WorldContext->GetWorld();
    if (!Component || !World)
    {
        return Component;
    }

    Component->SetAutoDestroy(false);
    const TWeakObjectPtr<UNiagaraComponent> WeakComponent(Component);

    FTimerHandle DeactivateTimer;
    World->GetTimerManager().SetTimer(
        DeactivateTimer,
        FTimerDelegate::CreateLambda([WeakComponent]()
        {
            if (UNiagaraComponent* ActiveComponent = WeakComponent.Get())
            {
                ActiveComponent->Deactivate();
            }
        }),
        FMath::Max(0.05f, DeactivateAfterSeconds), false);

    FTimerHandle DestroyTimer;
    World->GetTimerManager().SetTimer(
        DestroyTimer,
        FTimerDelegate::CreateLambda([WeakComponent]()
        {
            if (UNiagaraComponent* ActiveComponent = WeakComponent.Get())
            {
                ActiveComponent->DeactivateImmediate();
                ActiveComponent->DestroyComponent();
            }
        }),
        FMath::Max(DeactivateAfterSeconds + 0.1f, HardLifetimeSeconds), false);

    return Component;
}
