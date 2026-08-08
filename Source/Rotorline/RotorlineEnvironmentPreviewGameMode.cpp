#include "RotorlineEnvironmentPreviewGameMode.h"

#include "RotorlineFreeFlyPawn.h"
#include "RotorlineInfrastructureSplineActor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ARotorlineEnvironmentPreviewGameMode::ARotorlineEnvironmentPreviewGameMode()
{
    DefaultPawnClass = ARotorlineFreeFlyPawn::StaticClass();
    PlayerControllerClass = APlayerController::StaticClass();
    HUDClass = nullptr;
}

void ARotorlineEnvironmentPreviewGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (!FParse::Param(FCommandLine::Get(), TEXT("EnvironmentBenchmark")))
    {
        return;
    }

    // The visual-approval launch is intentionally presentation-only. Remove
    // legacy blockout props and utility splines from the one-kilometre frame
    // without modifying or saving the island map.
    constexpr double BenchmarkCenterX = -8064.0;
    constexpr double BenchmarkCenterY = -8064.0;
    constexpr double BenchmarkHalfExtent = 50000.0;
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        FVector BoundsOrigin;
        FVector BoundsExtent;
        It->GetActorBounds(false, BoundsOrigin, BoundsExtent);
        const bool bInsideBenchmark =
            FMath::Abs(BoundsOrigin.X - BenchmarkCenterX) <= BenchmarkHalfExtent &&
            FMath::Abs(BoundsOrigin.Y - BenchmarkCenterY) <= BenchmarkHalfExtent;
        const bool bSmallBlockoutOrProp = BoundsExtent.GetMax() < 10000.0;
        if (bInsideBenchmark && bSmallBlockoutOrProp)
        {
            It->SetActorHiddenInGame(true);
            It->SetActorEnableCollision(false);
        }
    }
    for (TActorIterator<ARotorlineInfrastructureSplineActor> It(GetWorld()); It; ++It)
    {
        It->SetActorHiddenInGame(true);
        It->SetActorEnableCollision(false);
    }

    // Lock the benchmark one stop below the old gameplay exposure so foliage,
    // cliff texture, and forest-floor values remain readable in direct sun.
    APostProcessVolume* Grade = GetWorld()->SpawnActor<APostProcessVolume>();
    if (Grade)
    {
        Grade->bUnbound = true;
        Grade->Priority = 1000.0f;
        Grade->Settings.bOverride_AutoExposureBias = true;
        Grade->Settings.AutoExposureBias = -1.0f;
        Grade->Settings.bOverride_ColorSaturation = true;
        Grade->Settings.ColorSaturation = FVector4(0.92f, 0.94f, 0.90f, 1.0f);
        Grade->Settings.bOverride_ColorContrast = true;
        Grade->Settings.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.0f);
    }
}
