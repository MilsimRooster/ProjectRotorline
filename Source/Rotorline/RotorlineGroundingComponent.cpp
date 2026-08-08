#include "RotorlineGroundingComponent.h"

#include "GameFramework/Actor.h"

URotorlineGroundingComponent::URotorlineGroundingComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void URotorlineGroundingComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!bApplyOnBeginPlay) return;
    FRotorlineGroundingResult Result;
    ApplyGrounding(false, Result);
}

bool URotorlineGroundingComponent::ApplyGrounding(bool bForce, FRotorlineGroundingResult& OutResult)
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        OutResult = FRotorlineGroundingResult();
        OutResult.Failure = ERotorlineGroundingFailure::InvalidWorld;
        LastFailure = OutResult.Failure;
        return false;
    }
    if (IsExplicitlyExcluded())
    {
        OutResult = FRotorlineGroundingResult();
        OutResult.ProfileName = Profile.ProfileName;
        OutResult.Failure = ERotorlineGroundingFailure::Excluded;
        OutResult.Detail = ExclusionDetail;
        LastFailure = OutResult.Failure;
        return false;
    }
    if (bGroundingApplied && !bForce)
    {
        OutResult = FRotorlineGroundingResult();
        OutResult.bSuccess = true;
        OutResult.ProfileName = Profile.ProfileName;
        OutResult.DesiredTransform = LastAppliedTransform;
        OutResult.Detail = TEXT("Stable one-time grounding already applied");
        return true;
    }

    const bool bSuccess = URotorlineGroundingLibrary::ApplyActorGrounding(Owner, Profile, OutResult, true);
    LastFailure = OutResult.Failure;
    if (bSuccess)
    {
        bGroundingApplied = true;
        LastAppliedTransform = Owner->GetActorTransform();
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_GROUNDING_V2|actor=%s|profile=%s|owner=%s|state=APPLIED|samples=%d/%d|roughness_cm=%.1f|slope_deg=%.1f"),
            *Owner->GetName(), *Profile.ProfileName.ToString(), *PlacementOwner.ToString(),
            OutResult.ValidSamples, OutResult.RequestedSamples,
            OutResult.FootprintRoughnessCm, OutResult.SurfaceSlopeDegrees);
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_GROUNDING_V2|actor=%s|profile=%s|owner=%s|state=FAILED|failure=%d|detail=%s"),
            *Owner->GetName(), *Profile.ProfileName.ToString(), *PlacementOwner.ToString(),
            static_cast<int32>(OutResult.Failure), *OutResult.Detail);
    }
    return bSuccess;
}

void URotorlineGroundingComponent::SetExplicitExclusion(
    ERotorlineGroundingExclusion Reason,
    const FString& Detail)
{
    Exclusion = Reason;
    ExclusionDetail = Detail;
    bGroundingApplied = false;
}
