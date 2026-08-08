#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RotorlineOperationsHUD.generated.h"

UCLASS()
class ROTORLINE_API ARotorlineOperationsHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    void DrawStartupFlow();
    void DrawCreditsRoll();
    void DrawCastGallery();
    void DrawOperationsBoard();
    void DrawAircraftHangar();
    void DrawFlightPauseOverlay();
    void DrawMissionFailureOverlay();
    void DrawMissionCompleteOverlay();
    void DrawAwardPresentationOverlay();
    void DrawPatchWall();
    void DrawAudioSettingsOverlay();
    void DrawGraphicsSettingsOverlay();
    void DrawControlsSettingsOverlay();
    void DrawFlightNavigation();
    void DrawTextBlock(const FString& Text, float X, float Y, float MaxWidth, float Scale, const FLinearColor& Color);
    void DrawWrappedText(const FString& Text, float X, float Y, float MaxWidth, float TextScale, const FLinearColor& Color, int32 MaxLines);
    void DrawDigitalGaugeCell(const FString& Label, const FString& Value, const FString& Unit,
        float X, float Y, float Width, float Height, float LayoutScale, float TextScale,
        const FLinearColor& Accent, bool bWarning = false);

    bool bApacheReticleAuditLogged = false;
    bool bHasSmoothedObjectiveTargetScreen = false;
    bool bHasSmoothedThreatTargetScreen = false;
    bool bHasSmoothedBellTargetScreen = false;
    FVector2D SmoothedObjectiveTargetScreen = FVector2D::ZeroVector;
    FVector2D SmoothedThreatTargetScreen = FVector2D::ZeroVector;
    FVector2D SmoothedBellTargetScreen = FVector2D::ZeroVector;
};
