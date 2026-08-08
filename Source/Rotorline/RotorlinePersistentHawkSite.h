#pragma once

#include "CoreMinimal.h"
#include "RotorlineMissionObjectiveActor.h"
#include "RotorlinePersistentHawkSite.generated.h"

class UStaticMeshComponent;

/**
 * A terrain-grounded, always-authored HAWK site that participates in the same
 * radar-lock, guided-missile, countermeasure, damage, and destruction systems
 * as mission-spawned air-defence objectives.
 */
UCLASS(Blueprintable)
class ROTORLINE_API ARotorlinePersistentHawkSite : public ARotorlineMissionObjectiveActor
{
    GENERATED_BODY()

public:
    ARotorlinePersistentHawkSite();

    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Combat|Air Defence")
    FString SiteDesignation = TEXT("RIDGE HAWK");

protected:
    virtual void BeginPlay() override;

private:
    void ConfigurePersistentSite();

    UPROPERTY(VisibleAnywhere, Category="Rotorline|Combat|Air Defence")
    TObjectPtr<UStaticMeshComponent> PreparedFiringPad;
};
