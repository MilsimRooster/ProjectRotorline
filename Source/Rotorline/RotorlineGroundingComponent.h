#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RotorlineGroundingLibrary.h"
#include "RotorlineGroundingComponent.generated.h"

UCLASS(ClassGroup=(Rotorline), meta=(BlueprintSpawnableComponent))
class ROTORLINE_API URotorlineGroundingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URotorlineGroundingComponent();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category="Rotorline|Grounding")
    bool ApplyGrounding(bool bForce, FRotorlineGroundingResult& OutResult);

    UFUNCTION(BlueprintCallable, Category="Rotorline|Grounding")
    void SetExplicitExclusion(ERotorlineGroundingExclusion Reason, const FString& Detail);

    UFUNCTION(BlueprintPure, Category="Rotorline|Grounding")
    bool IsExplicitlyExcluded() const { return Exclusion != ERotorlineGroundingExclusion::None; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    FRotorlineGroundingProfile Profile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    FName PlacementOwner = TEXT("Unassigned");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    ERotorlineGroundingExclusion Exclusion = ERotorlineGroundingExclusion::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    FString ExclusionDetail;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    bool bApplyOnBeginPlay = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rotorline|Grounding")
    bool bGroundingApplied = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rotorline|Grounding")
    FTransform LastAppliedTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rotorline|Grounding")
    ERotorlineGroundingFailure LastFailure = ERotorlineGroundingFailure::None;
};

