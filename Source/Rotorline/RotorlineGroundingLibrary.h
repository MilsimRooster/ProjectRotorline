#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RotorlineGroundingLibrary.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class ERotorlineGroundingMode : uint8
{
    Upright,
    SurfaceAligned,
    MultiPointFootprint,
    Vehicle,
    LinearPoint
};

UENUM(BlueprintType)
enum class ERotorlineGroundingFailure : uint8
{
    None,
    InvalidWorld,
    Excluded,
    MissingContactBounds,
    MissingGroundHit,
    InvalidSurface,
    Obstructed,
    ExcessiveSlope,
    UnevenFootprint,
    CollisionPenetration
};

UENUM(BlueprintType)
enum class ERotorlineGroundingExclusion : uint8
{
    None,
    Airborne,
    Suspended,
    Mounted,
    SocketAttached,
    Underground,
    Interior,
    InvisibleGameplayVolume,
    AuthoredAssemblyPart,
    RooftopPCG,
    RoadSurface,
    Waterborne
};

USTRUCT(BlueprintType)
struct ROTORLINE_API FRotorlineGroundingProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    FName ProfileName = TEXT("DefaultUpright");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    ERotorlineGroundingMode Mode = ERotorlineGroundingMode::Upright;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    bool bAllowLandscape = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    bool bAllowPreparedGround = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    bool bRejectObstructionsAboveGround = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    bool bRequireAllSamples = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    bool bCheckCollisionPenetration = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    bool bDrawDebug = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="100.0"))
    float TraceHeightCm = 150000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="100.0"))
    float TraceDepthCm = 50000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    float PreservedOffsetCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    float ContactSinkCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="0.0", ClampMax="89.0"))
    float MaximumSlopeDegrees = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="0.0"))
    float MaximumFootprintRoughnessCm = 125.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="0.0", ClampMax="0.45"))
    float FootprintInsetRatio = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="0", ClampMax="8"))
    int32 EdgeSamplesPerSide = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="0.0"))
    float ObstructionClearanceCm = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="0.0"))
    float PenetrationToleranceCm = 8.0f;

    // Optional radial search used by runtime spawners when the authored XY is
    // obstructed or the footprint is unsafe. Zero keeps authored XY exact.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="0.0"))
    float MaximumRelocationRadiusCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="100.0"))
    float RelocationStepCm = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding", meta=(ClampMin="4", ClampMax="32"))
    int32 RelocationDirections = 12;

    // When populated, only these primitive components contribute the physical
    // contact bounds. This keeps roofs, markers, rotors and attached props out
    // of the actor's ground-contact calculation.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotorline|Grounding")
    TArray<FName> ContactComponentNames;
};

USTRUCT(BlueprintType)
struct ROTORLINE_API FRotorlineGroundingResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    ERotorlineGroundingFailure Failure = ERotorlineGroundingFailure::None;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    FName ProfileName = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    FTransform DesiredTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    FVector TranslationDelta = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    FVector ContactPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    FVector SurfaceNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    float MinimumGroundZ = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    float MaximumGroundZ = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    float FootprintRoughnessCm = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    float SurfaceSlopeDegrees = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    int32 RequestedSamples = 0;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    int32 ValidSamples = 0;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    TObjectPtr<AActor> GroundActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Rotorline|Grounding")
    FString Detail;
};

UCLASS()
class ROTORLINE_API URotorlineGroundingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Rotorline|Grounding")
    static FRotorlineGroundingProfile MakeProfile(ERotorlineGroundingMode Mode, FName ProfileName);

    UFUNCTION(BlueprintCallable, Category="Rotorline|Grounding")
    static bool SolveActorGrounding(AActor* Actor, const FRotorlineGroundingProfile& Profile, FRotorlineGroundingResult& OutResult);

    UFUNCTION(BlueprintCallable, Category="Rotorline|Grounding")
    static bool ApplyActorGrounding(AActor* Actor, const FRotorlineGroundingProfile& Profile, FRotorlineGroundingResult& OutResult, bool bSweep = true);

    UFUNCTION(BlueprintPure, Category="Rotorline|Grounding")
    static FRotorlineGroundingResult EvaluateActorGrounding(AActor* Actor, const FRotorlineGroundingProfile& Profile);

    UFUNCTION(BlueprintCallable, Category="Rotorline|Grounding", meta=(WorldContext="WorldContextObject"))
    static bool SolveGroundContact(
        UObject* WorldContextObject,
        const FVector& CurrentContactPoint,
        const FVector2D& FootprintHalfExtent,
        AActor* IgnoredActor,
        const FRotorlineGroundingProfile& Profile,
        FRotorlineGroundingResult& OutResult);

    static bool CalculateContactBounds(AActor* Actor, const FRotorlineGroundingProfile& Profile, FBox& OutBounds);

    UFUNCTION(BlueprintCallable, Category="Rotorline|Grounding")
    static bool SolveInstancedMeshGrounding(
        UInstancedStaticMeshComponent* Component,
        int32 InstanceIndex,
        const FRotorlineGroundingProfile& Profile,
        FRotorlineGroundingResult& OutResult);

    UFUNCTION(BlueprintCallable, Category="Rotorline|Grounding")
    static bool ApplyInstancedMeshGrounding(
        UInstancedStaticMeshComponent* Component,
        int32 InstanceIndex,
        const FRotorlineGroundingProfile& Profile,
        FRotorlineGroundingResult& OutResult);

    UFUNCTION(BlueprintPure, Category="Rotorline|Grounding")
    static FRotorlineGroundingResult EvaluateInstancedMeshGrounding(
        UInstancedStaticMeshComponent* Component,
        int32 InstanceIndex,
        const FRotorlineGroundingProfile& Profile);
};
