#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineAircraftCatalog.h"
#include "RotorlineHangarPreviewActor.generated.h"

class UCameraComponent;
class UPointLightComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class ROTORLINE_API ARotorlineHangarPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineHangarPreviewActor();

    virtual void Tick(float DeltaSeconds) override;

    void ConfigureAircraft(
        const TArray<FString>& StaticMeshPaths,
        const TArray<FString>& RotorMeshPaths,
        const TArray<FRotorlineAircraftRotorGroup>& RotorGroups,
        const TArray<FString>& StationaryRotorAssets,
        bool bEnableFallbackRotors,
        const FVector& AircraftScale,
        const FRotator& AircraftRotation,
        const FVector& AircraftOffset);
    void ConfigureClassifiedPlaceholder();

    UCameraComponent* GetPreviewCamera() const { return PreviewCamera; }
    bool IsAircraftFramed() const { return bAircraftFramed; }

private:
    void ResetAircraftComponents();
    void FramePreviewCamera(
        const FBox& RotatedBodyBounds,
        float EffectiveScale,
        const FVector& CenteredOffset,
        const FVector& PresentationCenter);
    void AddFallbackMainRotor(const FBox& Bounds);
    void AddFallbackTailRotor(const FBox& Bounds);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> ShowcaseRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> PreviewCamera;

    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UStaticMeshComponent>> AircraftStaticParts;

    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<USkeletalMeshComponent>> AircraftRotorParts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USceneComponent>> SpinningMainRotorParts;

    UPROPERTY(Transient)
    TArray<FVector> SpinningMainRotorAxes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USceneComponent>> SpinningTailRotorParts;

    UPROPERTY(Transient)
    TArray<FVector> SpinningTailRotorAxes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USceneComponent>> AircraftRotorPivots;

    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> FallbackMainRotorPivot;

    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> FallbackTailRotorPivot;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> FallbackBladeMesh;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> FallbackRotorMaterial;

    // Hard asset reference so the classified crate and its embedded-material
    // dependencies are included by the cooker in packaged builds.
    UPROPERTY()
    TObjectPtr<UStaticMesh> ClassifiedAirframeCrateMesh;

    float ShowcaseYaw = 0.0f;
    FRotator AircraftBaseRotation = FRotator::ZeroRotator;
    bool bFallbackTailUsesXAxis = true;
    bool bAircraftFramed = false;
};
