#include "RotorlineJeepPawn.h"
#include "RotorlineOperationsPlayerController.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Sound/SoundWave.h"

ARotorlineJeepPawn::ARotorlineJeepPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
    Collision->SetBoxExtent(FVector(150.0f, 70.0f, 42.0f));
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    RootComponent = Collision;

    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    VisualRoot->SetupAttachment(Collision);

    DriverRoot = CreateDefaultSubobject<USceneComponent>(TEXT("JeepDriverRoot"));
    DriverRoot->SetupAttachment(Collision);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(Collision);
    CameraBoom->TargetArmLength = 650.0f;
    CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 125.0f));
    CameraBoom->SetRelativeRotation(FRotator(-13.0f, 0.0f, 0.0f));
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 8.0f;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("DriverCamera"));
    Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    Camera->FieldOfView = 78.0f;

    EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("JeepEngineAudio"));
    EngineAudio->SetupAttachment(Collision);
    EngineAudio->bAutoActivate = false;
    EngineAudio->SetVolumeMultiplier(0.32f);

    DrivingAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("JeepDrivingAudio"));
    DrivingAudio->SetupAttachment(Collision);
    DrivingAudio->bAutoActivate = false;
    DrivingAudio->SetVolumeMultiplier(0.26f);

    ThemeMusicAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("JeepThemeMusicAudio"));
    ThemeMusicAudio->SetupAttachment(Collision);
    ThemeMusicAudio->bAutoActivate = false;
    ThemeMusicAudio->bAllowSpatialization = false;
    ThemeMusicAudio->bIsUISound = true;

    DiscoveryInstructionAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("JeepDiscoveryInstructionAudio"));
    DiscoveryInstructionAudio->SetupAttachment(Collision);
    DiscoveryInstructionAudio->bAutoActivate = false;
    DiscoveryInstructionAudio->bAllowSpatialization = false;
    DiscoveryInstructionAudio->bIsUISound = true;

    LeftHeadlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("JeepHeadlightLeft"));
    RightHeadlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("JeepHeadlightRight"));
    for (USpotLightComponent* Headlight : { LeftHeadlight, RightHeadlight })
    {
        Headlight->SetupAttachment(Collision);
        Headlight->SetIntensity(52000.0f);
        Headlight->SetAttenuationRadius(5200.0f);
        Headlight->SetInnerConeAngle(18.0f);
        Headlight->SetOuterConeAngle(31.0f);
        Headlight->SetLightColor(FColor(255, 238, 205));
        Headlight->SetRelativeRotation(FRotator(-7.0f, 0.0f, 0.0f));
        Headlight->SetCastShadows(true);
    }
    LeftHeadlight->SetRelativeLocation(FVector(142.0f, -48.0f, 28.0f));
    RightHeadlight->SetRelativeLocation(FVector(142.0f, 48.0f, 28.0f));
}

void ARotorlineJeepPawn::ConfigureVehicle(const FRotorlineAircraftDefinition& Definition)
{
    VehicleName = Definition.DisplayName;
    const TArray<FString>& Paths = Definition.BodyAssets.IsEmpty()
        ? TArray<FString>{Definition.BodyAsset}
        : Definition.BodyAssets;

    auto AttachWheelMesh = [this, &Definition](
        UStaticMesh* StaticMesh,
        const FVector& SourceWheelOrigin,
        const FVector& DestinationWheelOrigin,
        const bool bFrontWheel)
    {
        if (!StaticMesh) return;
        USceneComponent* SteeringPivot = NewObject<USceneComponent>(this);
        AddInstanceComponent(SteeringPivot);
        SteeringPivot->SetupAttachment(VisualRoot);
        SteeringPivot->SetRelativeLocation(FVector(
            DestinationWheelOrigin.X - Definition.SourceCenter.X,
            DestinationWheelOrigin.Y - Definition.SourceCenter.Y,
            DestinationWheelOrigin.Z - Definition.SourceMinimumZ));
        SteeringPivot->RegisterComponent();
        if (bFrontWheel)
        {
            FrontWheelSteeringPivots.Add(SteeringPivot);
        }

        USceneComponent* SpinPivot = NewObject<USceneComponent>(this);
        AddInstanceComponent(SpinPivot);
        SpinPivot->SetupAttachment(SteeringPivot);
        SpinPivot->RegisterComponent();
        WheelPivots.Add(SpinPivot);

        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
        AddInstanceComponent(Part);
        Part->SetupAttachment(SpinPivot);
        Part->SetRelativeLocation(-SourceWheelOrigin);
        Part->SetStaticMesh(StaticMesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->RegisterComponent();
        MeshParts.Add(Part);
    };

    for (const FString& Path : Paths)
    {
        if (Path.IsEmpty()) continue;
        const FVector StableNearLeftOrigin(-8084.43f, -9405.12f, 5551.77f);
        const FVector StableNearRightOrigin(9320.51f, -9405.13f, 5551.77f);
        const TCHAR* StableNearLeftMesh =
            TEXT("/Game/Vehicles/Playable/Jeeps/WranglerWheels/wrangler_front_left/StaticMeshes/SM_Wrangler_front_left.SM_Wrangler_front_left");
        const TCHAR* StableNearRightMesh =
            TEXT("/Game/Vehicles/Playable/Jeeps/WranglerWheels/wrangler_front_right/StaticMeshes/SM_Wrangler_front_right.SM_Wrangler_front_right");
        if (Path.Contains(TEXT("6_low_001_Plane_006")))
        {
            AttachWheelMesh(
                LoadObject<UStaticMesh>(nullptr, StableNearLeftMesh),
                StableNearLeftOrigin,
                StableNearLeftOrigin,
                false);
            AttachWheelMesh(
                LoadObject<UStaticMesh>(nullptr, StableNearRightMesh),
                StableNearRightOrigin,
                StableNearRightOrigin,
                false);
            if (UStaticMesh* SuspensionMesh = LoadObject<UStaticMesh>(
                nullptr,
                TEXT("/Game/Vehicles/Playable/Jeeps/WranglerWheels/wrangler_suspension/StaticMeshes/SM_Wrangler_suspension.SM_Wrangler_suspension")))
            {
                UStaticMeshComponent* SuspensionPart = NewObject<UStaticMeshComponent>(this);
                AddInstanceComponent(SuspensionPart);
                SuspensionPart->SetupAttachment(VisualRoot);
                SuspensionPart->SetRelativeLocation(FVector(
                    -Definition.SourceCenter.X,
                    -Definition.SourceCenter.Y,
                    -Definition.SourceMinimumZ));
                SuspensionPart->SetStaticMesh(SuspensionMesh);
                SuspensionPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                SuspensionPart->RegisterComponent();
                MeshParts.Add(SuspensionPart);
            }
            continue;
        }
        UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *Path);
        if (!StaticMesh) continue;

        FVector WheelOrigin = FVector::ZeroVector;
        const bool bLeftTire = Path.Contains(TEXT("6_low_004_Plane_015"));
        const bool bRightTire = Path.Contains(TEXT("6_low_003_Plane_022"));
        if (bLeftTire)
        {
            WheelOrigin = FVector(-8037.47f, 18999.13f, 5553.51f);
        }
        else if (bRightTire)
        {
            WheelOrigin = FVector(9185.52f, 18999.13f, 5553.51f);
        }

        if (bLeftTire || bRightTire)
        {
            AttachWheelMesh(
                LoadObject<UStaticMesh>(
                    nullptr,
                    bLeftTire ? StableNearLeftMesh : StableNearRightMesh),
                bLeftTire ? StableNearLeftOrigin : StableNearRightOrigin,
                WheelOrigin,
                true);
            continue;
        }

        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
        AddInstanceComponent(Part);
        Part->SetupAttachment(VisualRoot);
        Part->SetRelativeLocation(FVector(
            -Definition.SourceCenter.X,
            -Definition.SourceCenter.Y,
            -Definition.SourceMinimumZ));
        Part->SetStaticMesh(StaticMesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->RegisterComponent();
        MeshParts.Add(Part);
    }

    VisualRoot->SetRelativeScale3D(FVector(Definition.PresentationScale));
    VisualRoot->SetRelativeRotation(FRotator(
        Definition.PresentationPitch,
        Definition.PresentationYaw,
        Definition.PresentationRoll));
    VisualRoot->AddRelativeLocation(Definition.PresentationOffset + FVector(0.0f, 0.0f, -58.0f));

    USkeletalMesh* MaleDriverMesh = LoadObject<USkeletalMesh>(
        nullptr,
        TEXT("/Game/Vehicles/Playable/Jeeps/MaleDriver/male_driver/SkeletalMeshes/SM_MaleJeepDriver.SM_MaleJeepDriver"));
    if (MaleDriverMesh)
    {
        DriverMesh = NewObject<USkeletalMeshComponent>(this);
        AddInstanceComponent(DriverMesh);
        DriverMesh->SetupAttachment(DriverRoot);
        DriverMesh->SetSkeletalMesh(MaleDriverMesh);
        DriverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        DriverMesh->SetGenerateOverlapEvents(false);
        DriverMesh->SetCastShadow(true);
        DriverMesh->RegisterComponent();

        if (UAnimSequence* DrivingPose = LoadObject<UAnimSequence>(
            nullptr,
            TEXT("/Game/Vehicles/Playable/Jeeps/MaleDriver/male_driver/SkeletalMeshes/SM_MaleJeepDriver_Anim.SM_MaleJeepDriver_Anim")))
        {
            DriverMesh->PlayAnimation(DrivingPose, true);
        }

        const FBox DriverBounds = MaleDriverMesh->GetImportedBounds().GetBox();
        const float DriverScale = DriverBounds.GetSize().Z > 0.01f
            ? 150.0f / DriverBounds.GetSize().Z
            : 100.0f;
        const FVector SeatCenter(-56.0f, -33.0f, 91.0f);
        DriverRoot->SetRelativeScale3D(FVector(DriverScale));
        DriverRoot->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        DriverRoot->SetRelativeLocation(SeatCenter - DriverBounds.GetCenter() * DriverScale);
    }

    if (USoundBase* StartupSound = LoadObject<USoundBase>(
        nullptr,
        TEXT("/Game/Audio/Vehicles/Jeep/S_Jeep_Startup.S_Jeep_Startup")))
    {
        EngineAudio->SetSound(StartupSound);
        EngineAudio->Play(0.0f);
    }
    if (USoundWave* DriveSound = LoadObject<USoundWave>(
        nullptr,
        TEXT("/Game/Audio/Vehicles/Jeep/S_Jeep_Driving.S_Jeep_Driving")))
    {
        DriveSound->bLooping = true;
        DrivingAudio->SetSound(DriveSound);
        DrivingAudio->SetVolumeMultiplier(0.0f);
        DrivingAudio->Play(0.0f);
    }
    if (USoundWave* ThemeSound = LoadObject<USoundWave>(
        nullptr,
        TEXT("/Game/Audio/Vehicles/Jeep/MUS_Jeep_TideOfGold.MUS_Jeep_TideOfGold")))
    {
        ThemeSound->bLooping = true;
        ThemeMusicAudio->SetSound(ThemeSound);
        ThemeMusicAudio->SetVolumeMultiplier(0.40f);
        ThemeMusicAudio->Play(0.0f);
    }
    if (Definition.Id.Equals(TEXT("jeep_wrangler"), ESearchCase::IgnoreCase))
    {
        if (USoundBase* DiscoverySound = LoadObject<USoundBase>(
            nullptr,
            TEXT("/Game/Audio/Vehicles/Jeep/VO_Jeep_FindTheHelicopter.VO_Jeep_FindTheHelicopter")))
        {
            DiscoveryInstructionAudio->SetSound(DiscoverySound);
            DiscoveryInstructionAudio->SetVolumeMultiplier(0.78f);
            DiscoveryInstructionAudio->Play(0.0f);
        }
    }
}

void ARotorlineJeepPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APlayerController* Player = Cast<APlayerController>(Controller);
    if (!Player || DeltaSeconds <= 0.0f) return;

    float MouseLookX = 0.0f;
    float MouseLookY = 0.0f;
    Player->GetInputMouseDelta(MouseLookX, MouseLookY);
    const float StickLookX = Player->GetInputAnalogKeyState(EKeys::Gamepad_RightX);
    const float StickLookY = Player->GetInputAnalogKeyState(EKeys::Gamepad_RightY);
    const bool bCameraLookInput =
        FMath::Abs(MouseLookX) > 0.01f || FMath::Abs(MouseLookY) > 0.01f ||
        FMath::Abs(StickLookX) > 0.08f || FMath::Abs(StickLookY) > 0.08f;
    if (Player->IsInputKeyDown(EKeys::R) ||
        Player->IsInputKeyDown(EKeys::Gamepad_RightThumbstick))
    {
        CameraOrbitYaw = 0.0f;
        CameraOrbitPitch = -13.0f;
        CameraLookIdleSeconds = 0.0f;
    }
    else if (bCameraLookInput)
    {
        CameraOrbitYaw = FMath::UnwindDegrees(
            CameraOrbitYaw + MouseLookX * 0.18f + StickLookX * 95.0f * DeltaSeconds);
        CameraOrbitPitch = FMath::Clamp(
            CameraOrbitPitch - MouseLookY * 0.14f - StickLookY * 72.0f * DeltaSeconds,
            -55.0f,
            18.0f);
        CameraLookIdleSeconds = 0.0f;
    }
    else
    {
        CameraLookIdleSeconds += DeltaSeconds;
        if (CameraLookIdleSeconds > 2.5f && FMath::Abs(CurrentSpeedCmPerSecond) > 100.0f)
        {
            CameraOrbitYaw = FMath::FInterpTo(CameraOrbitYaw, 0.0f, DeltaSeconds, 1.8f);
            CameraOrbitPitch = FMath::FInterpTo(CameraOrbitPitch, -13.0f, DeltaSeconds, 1.8f);
        }
    }
    CameraBoom->SetRelativeRotation(FRotator(CameraOrbitPitch, CameraOrbitYaw, 0.0f));

    const float KeyboardThrottle =
        (Player->IsInputKeyDown(EKeys::W) || Player->IsInputKeyDown(EKeys::Up) ? 1.0f : 0.0f) -
        (Player->IsInputKeyDown(EKeys::S) || Player->IsInputKeyDown(EKeys::Down) ? 1.0f : 0.0f);
    const float GamepadThrottle =
        Player->GetInputAnalogKeyState(EKeys::Gamepad_RightTrigger) -
        Player->GetInputAnalogKeyState(EKeys::Gamepad_LeftTrigger);
    const float Throttle = FMath::Clamp(
        FMath::Abs(KeyboardThrottle) > 0.01f ? KeyboardThrottle : GamepadThrottle,
        -1.0f, 1.0f);

    const float KeyboardSteer =
        (Player->IsInputKeyDown(EKeys::D) || Player->IsInputKeyDown(EKeys::Right) ? 1.0f : 0.0f) -
        (Player->IsInputKeyDown(EKeys::A) || Player->IsInputKeyDown(EKeys::Left) ? 1.0f : 0.0f);
    const float StickSteer = Player->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
    const float Steer = FMath::Clamp(
        FMath::Abs(KeyboardSteer) > 0.01f ? KeyboardSteer : StickSteer,
        -1.0f, 1.0f);
    CurrentSteeringDegrees = FMath::FInterpTo(
        CurrentSteeringDegrees,
        Steer * 28.0f,
        DeltaSeconds,
        9.0f);
    for (USceneComponent* SteeringPivot : FrontWheelSteeringPivots)
    {
        if (SteeringPivot)
        {
            SteeringPivot->SetRelativeRotation(FRotator(0.0f, CurrentSteeringDegrees, 0.0f));
        }
    }

    const bool bBrake = Player->IsInputKeyDown(EKeys::SpaceBar) ||
        Player->IsInputKeyDown(EKeys::Gamepad_FaceButton_Left);
    const float TargetSpeed = Throttle >= 0.0f ? Throttle * 2500.0f : Throttle * 950.0f;
    CurrentSpeedCmPerSecond = FMath::FInterpTo(
        CurrentSpeedCmPerSecond,
        bBrake ? 0.0f : TargetSpeed,
        DeltaSeconds,
        bBrake ? 8.0f : (FMath::Abs(Throttle) > 0.02f ? 1.8f : 0.8f));

    const bool bDriving = FMath::Abs(Throttle) > 0.08f || FMath::Abs(CurrentSpeedCmPerSecond) > 90.0f;
    float EngineMix = 1.0f;
    if (const ARotorlineOperationsPlayerController* Operations =
        Cast<ARotorlineOperationsPlayerController>(Player))
    {
        EngineMix = Operations->GetEffectiveAudioVolume(ERotorlineAudioChannel::Engine);
        if (EngineAudio)
        {
            EngineAudio->SetVolumeMultiplier(0.32f * EngineMix);
        }
        if (ThemeMusicAudio)
        {
            ThemeMusicAudio->SetVolumeMultiplier(
                0.35f * Operations->GetEffectiveAudioVolume(ERotorlineAudioChannel::Music));
        }
        if (DiscoveryInstructionAudio)
        {
            DiscoveryInstructionAudio->SetVolumeMultiplier(
                0.88f * Operations->GetEffectiveAudioVolume(ERotorlineAudioChannel::Radio));
        }
    }
    if (DrivingAudio && DrivingAudio->Sound)
    {
        if (!DrivingAudio->IsPlaying())
        {
            DrivingAudio->Play(0.0f);
        }
        DrivingAudioVolume = FMath::FInterpTo(
            DrivingAudioVolume,
            bDriving ? 0.26f : 0.0f,
            DeltaSeconds,
            5.0f);
        DrivingAudio->SetVolumeMultiplier(DrivingAudioVolume * EngineMix);
    }

    const float SpeedRatio = FMath::Clamp(FMath::Abs(CurrentSpeedCmPerSecond) / 900.0f, 0.0f, 1.0f);
    const float DirectionSign = CurrentSpeedCmPerSecond < 0.0f ? -1.0f : 1.0f;
    AddActorWorldRotation(FRotator(0.0f, Steer * 62.0f * SpeedRatio * DirectionSign * DeltaSeconds, 0.0f));

    const FVector DriveDelta = GetActorForwardVector() * CurrentSpeedCmPerSecond * DeltaSeconds;
    bool bObstacleAhead = false;
    if (!DriveDelta.IsNearlyZero())
    {
        FCollisionQueryParams ObstacleParams(SCENE_QUERY_STAT(RotorlineJeepObstacle), false, this);
        const FVector TravelDirection = DriveDelta.GetSafeNormal();
        const FVector Right = GetActorRightVector();
        for (const float SideOffset : { -52.0f, 0.0f, 52.0f })
        {
            const FVector ProbeStart = GetActorLocation() + Right * SideOffset + FVector(0.0f, 0.0f, 68.0f);
            const FVector ProbeEnd = ProbeStart + TravelDirection * 175.0f;
            FHitResult ObstacleHit;
            if (GetWorld()->LineTraceSingleByChannel(
                ObstacleHit, ProbeStart, ProbeEnd, ECC_Visibility, ObstacleParams))
            {
                const UPrimitiveComponent* HitComponent = ObstacleHit.GetComponent();
                if (HitComponent && HitComponent->IsVisible())
                {
                    bObstacleAhead = true;
                    break;
                }
            }
        }
    }
    if (!bObstacleAhead)
    {
        AddActorWorldOffset(DriveDelta, false);
    }
    else
    {
        CurrentSpeedCmPerSecond *= -0.08f;
    }

    const float WheelDegrees = (CurrentSpeedCmPerSecond * DeltaSeconds / 38.0f) * (180.0f / PI);
    for (USceneComponent* WheelPivot : WheelPivots)
    {
        if (WheelPivot) WheelPivot->AddLocalRotation(FRotator(0.0f, 0.0f, WheelDegrees));
    }

    FHitResult GroundHit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RotorlineJeepGround), false, this);
    const FVector TraceStart = GetActorLocation() + FVector(0.0f, 0.0f, 220.0f);
    const FVector TraceEnd = GetActorLocation() - FVector(0.0f, 0.0f, 2500.0f);
    if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
    {
        const FVector ForwardOnGround = FVector::VectorPlaneProject(
            GetActorForwardVector(), GroundHit.ImpactNormal).GetSafeNormal();
        const FRotator GroundRotation = FRotationMatrix::MakeFromXZ(
            ForwardOnGround, GroundHit.ImpactNormal).Rotator();
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), GroundRotation, DeltaSeconds, 9.0f));
        FVector GroundedLocation = GetActorLocation();
        GroundedLocation.Z = FMath::FInterpTo(
            GroundedLocation.Z, GroundHit.ImpactPoint.Z + 58.0f, DeltaSeconds, 12.0f);
        SetActorLocation(GroundedLocation, false);
        VerticalSpeedCmPerSecond = 0.0f;
    }
    else
    {
        VerticalSpeedCmPerSecond = FMath::Max(VerticalSpeedCmPerSecond - 980.0f * DeltaSeconds, -3200.0f);
        AddActorWorldOffset(FVector(0.0f, 0.0f, VerticalSpeedCmPerSecond * DeltaSeconds), true);
    }
}
