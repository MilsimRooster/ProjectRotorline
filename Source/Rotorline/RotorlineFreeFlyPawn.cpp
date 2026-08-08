#include "RotorlineFreeFlyPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace RotorlineFreeFly
{
    constexpr double SpawnX = -233856.0;
    constexpr double SpawnY = -209664.0;
    constexpr double SpawnZ = 30000.0;
    constexpr float SpawnYaw = 8.0f;
    constexpr float SpawnPitch = -12.0f;

    // Central Valley visual-approval benchmark. This alternate spawn is only
    // used by the explicit -EnvironmentBenchmark preview launch.
    // Start at the edge of the protected town meadow, about 60 m AGL, looking
    // into West Ridge.  This makes the benchmark readable immediately instead
    // of presenting a high-altitude view of the deliberately empty clearing.
    constexpr double BenchmarkSpawnX = -10000.0;
    constexpr double BenchmarkSpawnY = 25000.0;
    constexpr double BenchmarkSpawnZ = 10500.0;
    constexpr float BenchmarkSpawnYaw = 155.0f;
    constexpr float BenchmarkSpawnPitch = -5.0f;

    // Strategic overview of the opposing west/east HAWK positions and the
    // road corridor caught between their interlocking engagement lanes.
    constexpr double HawkRidgeSpawnX = -20650.0;
    constexpr double HawkRidgeSpawnY = 25000.0;
    constexpr double HawkRidgeSpawnZ = 47000.0;
    constexpr float HawkRidgeSpawnYaw = 90.0f;
    constexpr float HawkRidgeSpawnPitch = -11.0f;

    // Close terrain-contact audit for the opposing east-ridge launcher.
    constexpr double HawkEastSpawnX = 16225.0;
    constexpr double HawkEastSpawnY = 119826.0;
    constexpr double HawkEastSpawnZ = 31500.0;
    constexpr float HawkEastSpawnYaw = 5.1f;
    constexpr float HawkEastSpawnPitch = -22.5f;
}

ARotorlineFreeFlyPawn::ARotorlineFreeFlyPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SceneRoot);
    Camera->bUsePawnControlRotation = true;
    Camera->SetFieldOfView(90.0f);

    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = true;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void ARotorlineFreeFlyPawn::BeginPlay()
{
    Super::BeginPlay();

    const bool bEnvironmentBenchmark = FParse::Param(FCommandLine::Get(), TEXT("EnvironmentBenchmark"));
    const bool bHawkRidgePreview = FParse::Param(FCommandLine::Get(), TEXT("HawkRidgePreview"));
    const bool bHawkEastPreview = FParse::Param(FCommandLine::Get(), TEXT("HawkRidgeEastPreview"));
    const FVector PreviewLocation(
        bHawkEastPreview ? RotorlineFreeFly::HawkEastSpawnX : (bHawkRidgePreview ? RotorlineFreeFly::HawkRidgeSpawnX : (bEnvironmentBenchmark ? RotorlineFreeFly::BenchmarkSpawnX : RotorlineFreeFly::SpawnX)),
        bHawkEastPreview ? RotorlineFreeFly::HawkEastSpawnY : (bHawkRidgePreview ? RotorlineFreeFly::HawkRidgeSpawnY : (bEnvironmentBenchmark ? RotorlineFreeFly::BenchmarkSpawnY : RotorlineFreeFly::SpawnY)),
        bHawkEastPreview ? RotorlineFreeFly::HawkEastSpawnZ : (bHawkRidgePreview ? RotorlineFreeFly::HawkRidgeSpawnZ : (bEnvironmentBenchmark ? RotorlineFreeFly::BenchmarkSpawnZ : RotorlineFreeFly::SpawnZ)));
    const FRotator PreviewRotation(
        bHawkEastPreview ? RotorlineFreeFly::HawkEastSpawnPitch : (bHawkRidgePreview ? RotorlineFreeFly::HawkRidgeSpawnPitch : (bEnvironmentBenchmark ? RotorlineFreeFly::BenchmarkSpawnPitch : RotorlineFreeFly::SpawnPitch)),
        bHawkEastPreview ? RotorlineFreeFly::HawkEastSpawnYaw : (bHawkRidgePreview ? RotorlineFreeFly::HawkRidgeSpawnYaw : (bEnvironmentBenchmark ? RotorlineFreeFly::BenchmarkSpawnYaw : RotorlineFreeFly::SpawnYaw)),
        0.0f);

    Camera->SetFieldOfView(bHawkEastPreview ? 64.0f : (bHawkRidgePreview ? 75.0f : 90.0f));

    SetActorLocationAndRotation(PreviewLocation, PreviewRotation, false, nullptr, ETeleportType::TeleportPhysics);

    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        PlayerController->SetIgnoreMoveInput(false);
        PlayerController->SetIgnoreLookInput(false);
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        PlayerController->SetInputMode(Mode);
        PlayerController->bShowMouseCursor = true;
        if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
        {
            ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
            ViewportClient->SetMouseLockMode(EMouseLockMode::DoNotLock);
        }
        PlayerController->SetControlRotation(PreviewRotation);
        PlayerController->PlayDynamicForceFeedback(
            0.16f, 0.22f, true, true, true, true, EDynamicForceFeedbackAction::Start);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            12.0f,
            FColor(230, 240, 255),
            bHawkEastPreview
                ? TEXT("MAIN VALLEY HAWK BRAVO  |  east-ridge terrain-contact audit")
                : bHawkRidgePreview
                ? TEXT("MAIN VALLEY HAWK CROSS-FIRE  |  west/east interlocking radar-missile sites")
                : bEnvironmentBenchmark
                ? TEXT("CENTRAL VALLEY VISUAL BENCHMARK  |  DUALSENSE: LS move, RS look, R2/L2 up/down, R1 boost  |  WASD + mouse also active")
                : TEXT("FREE FLY  |  WASD move  |  E/Q up/down  |  Shift boost  |  Mouse look  |  Esc releases mouse"));
    }

    const bool bHawkCapture = bHawkRidgePreview && FParse::Param(FCommandLine::Get(), TEXT("HawkRidgeCapture"));
    const bool bHawkEastCapture = bHawkEastPreview && FParse::Param(FCommandLine::Get(), TEXT("HawkRidgeCapture"));
    if (bHawkCapture || bHawkEastCapture)
    {
        FTimerHandle CaptureTimer;
        GetWorldTimerManager().SetTimer(CaptureTimer, []()
        {
            const bool bCapturingHawkEast = FParse::Param(FCommandLine::Get(), TEXT("HawkRidgeEastPreview"));
            const FString ScreenshotPath = FPaths::Combine(
                FPaths::ProjectSavedDir(),
                bCapturingHawkEast ? TEXT("Screenshots/HawkRidgeEastRuntime.png") : TEXT("Screenshots/HawkRidgeRuntime.png"));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_HAWK_PREVIEW|CAPTURE_REQUESTED|path=%s"), *ScreenshotPath);
        }, 10.0f, false);

        FTimerHandle ExitTimer;
        GetWorldTimerManager().SetTimer(ExitTimer, []()
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_HAWK_PREVIEW|CAPTURE_COMPLETE|status=EXITING"));
            FPlatformMisc::RequestExit(false);
        }, 12.0f, false);
    }
}

void ARotorlineFreeFlyPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::Escape))
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        PlayerController->SetInputMode(Mode);
        PlayerController->bShowMouseCursor = true;
        if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
        {
            ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
            ViewportClient->SetMouseLockMode(EMouseLockMode::DoNotLock);
        }
        return;
    }

    FRotator ViewRotation = PlayerController->GetControlRotation();

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    PlayerController->GetInputMouseDelta(MouseX, MouseY);
    const auto ApplyDeadZone = [](const float Value)
    {
        constexpr float DeadZone = 0.14f;
        const float Magnitude = FMath::Abs(Value);
        return Magnitude <= DeadZone
            ? 0.0f
            : FMath::Sign(Value) * ((Magnitude - DeadZone) / (1.0f - DeadZone));
    };
    const float GamepadLookX = ApplyDeadZone(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightX));
    const float GamepadLookY = ApplyDeadZone(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightY));
    ViewRotation.Yaw += MouseX * LookSensitivity + GamepadLookX * 105.0f * DeltaSeconds;
    ViewRotation.Pitch = FMath::ClampAngle(
        ViewRotation.Pitch - MouseY * LookSensitivity - GamepadLookY * 82.0f * DeltaSeconds,
        -89.0f,
        89.0f);
    ViewRotation.Roll = 0.0f;
    PlayerController->SetControlRotation(ViewRotation);

    // Keep very short key presses alive for a few frames. This makes preview
    // navigation reliable for remote-desktop tools without changing normal
    // held-key behavior.
    static double LastWPress = -1000.0;
    static double LastSPress = -1000.0;
    static double LastAPress = -1000.0;
    static double LastDPress = -1000.0;
    static double LastEPress = -1000.0;
    static double LastQPress = -1000.0;
    constexpr double InputPulseSeconds = 0.12;

    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < LastWPress)
    {
        LastWPress = LastSPress = LastAPress = LastDPress = LastEPress = LastQPress = -1000.0;
    }

    if (PlayerController->WasInputKeyJustPressed(EKeys::W)) LastWPress = Now;
    if (PlayerController->WasInputKeyJustPressed(EKeys::S)) LastSPress = Now;
    if (PlayerController->WasInputKeyJustPressed(EKeys::A)) LastAPress = Now;
    if (PlayerController->WasInputKeyJustPressed(EKeys::D)) LastDPress = Now;
    if (PlayerController->WasInputKeyJustPressed(EKeys::E)) LastEPress = Now;
    if (PlayerController->WasInputKeyJustPressed(EKeys::Q)) LastQPress = Now;

    const auto IsActive = [Now](bool bHeld, double LastPress)
    {
        return bHeld || (Now - LastPress) <= InputPulseSeconds;
    };

    const bool bForward = IsActive(PlayerController->IsInputKeyDown(EKeys::W), LastWPress);
    const bool bBackward = IsActive(PlayerController->IsInputKeyDown(EKeys::S), LastSPress);
    const bool bLeft = IsActive(PlayerController->IsInputKeyDown(EKeys::A), LastAPress);
    const bool bRight = IsActive(PlayerController->IsInputKeyDown(EKeys::D), LastDPress);
    const bool bUp = IsActive(PlayerController->IsInputKeyDown(EKeys::E), LastEPress);
    const bool bDown = IsActive(PlayerController->IsInputKeyDown(EKeys::Q), LastQPress);

    const float GamepadForward = ApplyDeadZone(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftY));
    const float GamepadRight = ApplyDeadZone(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftX));
    const float GamepadUp =
        PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) -
        PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
    const float ForwardInput = FMath::Clamp(
        (bForward ? 1.0f : 0.0f) - (bBackward ? 1.0f : 0.0f) + GamepadForward,
        -1.0f, 1.0f);
    const float RightInput = FMath::Clamp(
        (bRight ? 1.0f : 0.0f) - (bLeft ? 1.0f : 0.0f) + GamepadRight,
        -1.0f, 1.0f);
    const float UpInput = FMath::Clamp(
        (bUp ? 1.0f : 0.0f) - (bDown ? 1.0f : 0.0f) + GamepadUp,
        -1.0f, 1.0f);

    if (PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder))
    {
        PlayerController->PlayDynamicForceFeedback(
            0.32f, 0.16f, true, true, true, true, EDynamicForceFeedbackAction::Start);
    }

    FVector Movement =
        ViewRotation.Vector() * ForwardInput +
        FRotationMatrix(FRotator(0.0f, ViewRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::Y) * RightInput +
        FVector::UpVector * UpInput;

    if (!Movement.IsNearlyZero())
    {
        Movement = Movement.GetClampedToMaxSize(1.0f);
        const bool bBoosting =
            PlayerController->IsInputKeyDown(EKeys::LeftShift) ||
            PlayerController->IsInputKeyDown(EKeys::RightShift) ||
            PlayerController->IsInputKeyDown(EKeys::Gamepad_RightShoulder) ||
            PlayerController->IsInputKeyDown(EKeys::Gamepad_LeftThumbstick);
        const float Speed = CruiseSpeed * (bBoosting ? BoostMultiplier : 1.0f);
        SetActorLocation(GetActorLocation() + Movement * Speed * DeltaSeconds, false);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                9001,
                0.18f,
                FColor::Green,
                FString::Printf(TEXT("MOVING  |  X %.0f  Y %.0f  Z %.0f"),
                    GetActorLocation().X,
                    GetActorLocation().Y,
                    GetActorLocation().Z));
        }
    }
}
