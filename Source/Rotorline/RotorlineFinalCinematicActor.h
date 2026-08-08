#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineFinalCinematicActor.generated.h"

class UPointLightComponent;
class UCameraComponent;
class UNiagaraComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ROTORLINE_API ARotorlineFinalCinematicActor : public AActor
{
    GENERATED_BODY()
public:
    ARotorlineFinalCinematicActor();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void StartFinale();
    bool IsFinished() const { return bFinished; }
private:
    void CompleteFinale(const TCHAR* CompletionMode);
    void CleanupFinaleState();

    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY() TObjectPtr<UCameraComponent> CinematicCamera;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Missile;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Fireball;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Shockwave;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> SmokeColumn;
    UPROPERTY() TObjectPtr<UPointLightComponent> BlastLight;
    UPROPERTY() TObjectPtr<UNiagaraComponent> HeroGasBombFX;
    UPROPERTY() TObjectPtr<UNiagaraComponent> CoreExplosionFX;
    UPROPERTY() TObjectPtr<UNiagaraComponent> DustShockwaveFX;
    UPROPERTY() TObjectPtr<UNiagaraComponent> SparksFX;
    UPROPERTY() TObjectPtr<UNiagaraComponent> PersistentSmokeFX;
    UPROPERTY() TObjectPtr<UNiagaraComponent> VolumetricExplosionFX;
    FVector MissileStart = FVector::ZeroVector;
    FVector ImpactLocation = FVector::ZeroVector;
    FVector CameraBaseLocation = FVector::ZeroVector;
    FRotator CameraBaseRotation = FRotator::ZeroRotator;
    int32 PreviousNiagaraQualityLevel = INDEX_NONE;
    bool bNiagaraQualityOverridden = false;
    TWeakObjectPtr<AActor> PreviousViewTarget;
    float Elapsed = 0.0f;
    bool bStarted = false;
    bool bImpacted = false;
    bool bFinished = false;
    bool bEndingMovieStarted = false;
    bool bCleanupComplete = false;
};
