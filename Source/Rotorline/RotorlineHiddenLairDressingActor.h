#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RotorlineHiddenLairDressingActor.generated.h"

class UPointLightComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class ROTORLINE_API ARotorlineHiddenLairDressingActor : public AActor
{
    GENERATED_BODY()

public:
    ARotorlineHiddenLairDressingActor();

    UFUNCTION(BlueprintCallable, Category="Hidden Lair Dressing")
    void SetDressingVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category="Hidden Lair Dressing")
    void SetClearanceDebugVisible(bool bVisible);

    bool ValidateClearance(FString& OutDetail) const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorMainDoor;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorCommandBay;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorMaintenanceBay;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorUtilityBay;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorLogisticsBay;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorUpperGantryA;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorUpperGantryB;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorStructuralRibs;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorPipeRacks;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> AnchorDecalsAndCables;

    UPROPERTY(EditAnywhere, Category="Hidden Lair Dressing")
    bool bDressingVisible = true;

    UPROPERTY(EditAnywhere, Category="Hidden Lair Dressing")
    bool bClearanceDebugVisible = false;

    UPROPERTY()
    TArray<TObjectPtr<UPrimitiveComponent>> DressingGeometry;

    UPROPERTY()
    TArray<TObjectPtr<UPointLightComponent>> DressingLights;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> RotorClearanceDebug;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> DoorCorridorDebug;
};
