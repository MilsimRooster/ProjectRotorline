#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RotorlineAircraftCatalog.h"
#include "RotorlineJeepPawn.generated.h"

class UBoxComponent;
class UAudioComponent;
class UCameraComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USpotLightComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS()
class ROTORLINE_API ARotorlineJeepPawn : public APawn
{
    GENERATED_BODY()

public:
    ARotorlineJeepPawn();
    virtual void Tick(float DeltaSeconds) override;
    void ConfigureVehicle(const FRotorlineAircraftDefinition& Definition);

    float GetSpeedKmh() const { return FMath::Abs(CurrentSpeedCmPerSecond) * 0.036f; }
    const FString& GetVehicleName() const { return VehicleName; }
    int32 GetMeshPartCount() const { return MeshParts.Num(); }
    int32 GetWheelPivotCount() const { return WheelPivots.Num(); }

private:
    UPROPERTY()
    UBoxComponent* Collision = nullptr;

    UPROPERTY()
    USceneComponent* VisualRoot = nullptr;

    UPROPERTY()
    USpringArmComponent* CameraBoom = nullptr;

    UPROPERTY()
    UCameraComponent* Camera = nullptr;

    UPROPERTY()
    TArray<UStaticMeshComponent*> MeshParts;

    UPROPERTY()
    USceneComponent* DriverRoot = nullptr;

    UPROPERTY()
    USkeletalMeshComponent* DriverMesh = nullptr;

    UPROPERTY()
    TArray<USceneComponent*> WheelPivots;

    UPROPERTY()
    TArray<USceneComponent*> FrontWheelSteeringPivots;

    UPROPERTY()
    UAudioComponent* EngineAudio = nullptr;

    UPROPERTY()
    UAudioComponent* DrivingAudio = nullptr;

    UPROPERTY()
    UAudioComponent* ThemeMusicAudio = nullptr;

    UPROPERTY()
    UAudioComponent* DiscoveryInstructionAudio = nullptr;

    UPROPERTY()
    USpotLightComponent* LeftHeadlight = nullptr;

    UPROPERTY()
    USpotLightComponent* RightHeadlight = nullptr;

    FString VehicleName = TEXT("JEEP");
    float CurrentSpeedCmPerSecond = 0.0f;
    float VerticalSpeedCmPerSecond = 0.0f;
    float DrivingAudioVolume = 0.0f;
    float CameraOrbitYaw = 0.0f;
    float CameraOrbitPitch = -13.0f;
    float CameraLookIdleSeconds = 0.0f;
    float CurrentSteeringDegrees = 0.0f;
};
