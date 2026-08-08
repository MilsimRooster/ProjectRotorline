#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "RotorlineFlightControllerSubsystem.generated.h"

// These names are the public contract between physical controller profiles and
// gameplay.  Profile files store these strings rather than controller-specific
// button labels, so rebinding a HOTAS never changes the gameplay-facing action.
namespace RotorlineFlightControllerActions
{
    ROTORLINE_API extern const FName Roll;
    ROTORLINE_API extern const FName Pitch;
    ROTORLINE_API extern const FName Yaw;
    ROTORLINE_API extern const FName Collective;
    ROTORLINE_API extern const FName Throttle;
    ROTORLINE_API extern const FName LookX;
    ROTORLINE_API extern const FName LookY;
    ROTORLINE_API extern const FName PrimaryFire;
    ROTORLINE_API extern const FName SecondaryFire;
    ROTORLINE_API extern const FName WeaponNext;
    ROTORLINE_API extern const FName WeaponPrevious;
    ROTORLINE_API extern const FName TargetLock;
    ROTORLINE_API extern const FName MissionInteract;
    ROTORLINE_API extern const FName LandingGear;
    ROTORLINE_API extern const FName Searchlight;
    ROTORLINE_API extern const FName ChangeCamera;
    ROTORLINE_API extern const FName CockpitView;
    ROTORLINE_API extern const FName ExternalView;
    ROTORLINE_API extern const FName MapView;
    ROTORLINE_API extern const FName RadioCommand;
    // Legacy profile aliases retained so older local profiles remain readable.
    ROTORLINE_API extern const FName FireCannon;
    ROTORLINE_API extern const FName FireRockets;
    ROTORLINE_API extern const FName FireMissile;
    ROTORLINE_API extern const FName Countermeasures;
    ROTORLINE_API extern const FName Boost;
    ROTORLINE_API extern const FName CycleWeapon;
    ROTORLINE_API extern const FName TargetNext;
    ROTORLINE_API extern const FName TargetPrevious;
    ROTORLINE_API extern const FName GearToggle;
    ROTORLINE_API extern const FName Pause;
    ROTORLINE_API extern const FName MenuAccept;
    ROTORLINE_API extern const FName MenuBack;

    ROTORLINE_API const TArray<FName>& All();
    ROTORLINE_API bool IsKnown(const FName Action);
}

USTRUCT(BlueprintType)
struct FRotorlineControllerAxisCapability
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName NativeName = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    int32 NativeIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    float RawMinimum = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float RawMaximum = 65535.0f;
};

USTRUCT(BlueprintType)
struct FRotorlineControllerCapabilities
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 AxisCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 ButtonCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 HatCount = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FRotorlineControllerAxisCapability> Axes;
};

USTRUCT(BlueprintType)
struct FRotorlineControllerDeviceInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString DeviceId;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly)
    int32 VendorId = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 ProductId = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 NativeDeviceIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    FString BackendName;

    UPROPERTY(BlueprintReadOnly)
    bool bConnected = false;

    // True for devices already handled by Rotorline's normal gamepad path.
    // They remain visible for diagnostics but are not offered as flight sticks.
    UPROPERTY(BlueprintReadOnly)
    bool bGamepadCompatible = false;

    UPROPERTY(BlueprintReadOnly)
    FRotorlineControllerCapabilities Capabilities;
};

USTRUCT(BlueprintType)
struct FRotorlineAxisCalibration
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RawMinimum = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RawCenter = 32767.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RawMaximum = 65535.0f;

    // Largest observed center jitter in raw device units. The configuration UI
    // can use this to recommend a deadzone without guessing at hardware noise.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoiseFloor = 0.0f;
};

USTRUCT(BlueprintType)
struct FRotorlineAxisBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Action = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NativeAxisIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString UserLabel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIgnore = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCentered = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bInvert = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="0.95"))
    float Deadzone = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="4.0"))
    float Sensitivity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="4.0"))
    float CurveExponent = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="-4.0", ClampMax="4.0"))
    float Scale = 1.0f;

    // Fine trim expressed as a fraction of half the calibrated range.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="-0.25", ClampMax="0.25"))
    float CenterOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotorlineAxisCalibration Calibration;
};

USTRUCT(BlueprintType)
struct FRotorlineButtonBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Action = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NativeButtonIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FRotorlineHatBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NativeHatIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName UpAction = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName RightAction = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName DownAction = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName LeftAction = NAME_None;
};

USTRUCT(BlueprintType)
struct FRotorlineFlightControllerProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SchemaVersion = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ProfileId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ProfileName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DeviceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DeviceName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 VendorId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ProductId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ExpectedAxisCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ExpectedButtonCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ExpectedHatCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotorlineControllerCapabilities DetectedCapabilities;

    // Set only after the configuration UI has shown an explicit confirmation.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowDuplicateAxisBindings = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowDuplicateButtonBindings = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAllowDuplicateHatBindings = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FRotorlineAxisBinding> AxisBindings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FRotorlineButtonBinding> ButtonBindings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FRotorlineHatBinding> HatBindings;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRotorlineControllerConnectionEvent, const FString&, DeviceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRotorlineControllerProfileEvent, const FString&, ProfileId);

UCLASS()
class ROTORLINE_API URotorlineFlightControllerSubsystem final : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
    virtual UWorld* GetTickableGameObjectWorld() const override;

    UPROPERTY(BlueprintAssignable)
    FRotorlineControllerConnectionEvent OnControllerConnected;

    UPROPERTY(BlueprintAssignable)
    FRotorlineControllerConnectionEvent OnControllerDisconnected;

    UPROPERTY(BlueprintAssignable)
    FRotorlineControllerProfileEvent OnActiveProfileChanged;

    UFUNCTION(BlueprintCallable)
    void RefreshDevices();

    UFUNCTION(BlueprintPure)
    const TArray<FRotorlineControllerDeviceInfo>& GetDevices() const { return Devices; }

    UFUNCTION(BlueprintPure)
    const TArray<FRotorlineFlightControllerProfile>& GetProfiles() const { return Profiles; }

    UFUNCTION(BlueprintCallable)
    bool SetActiveDevice(const FString& DeviceId);

    UFUNCTION(BlueprintPure)
    FString GetActiveDeviceId() const { return ActiveDeviceId; }

    UFUNCTION(BlueprintPure)
    FString GetActiveProfileId() const { return ActiveProfileId; }

    UFUNCTION(BlueprintCallable)
    bool ApplyProfile(const FString& ProfileId);

    // Applies validated working values for the current session without
    // writing a profile file. The settings screen uses this for live Apply /
    // Cancel semantics; SaveProfile remains the only persistent write path.
    UFUNCTION(BlueprintCallable)
    bool ApplyTransientProfile(const FRotorlineFlightControllerProfile& Profile);

    UFUNCTION(BlueprintCallable)
    FRotorlineFlightControllerProfile MakeDefaultProfile(const FString& DeviceId) const;

    UFUNCTION(BlueprintCallable)
    bool SaveProfile(const FRotorlineFlightControllerProfile& Profile);

    UFUNCTION(BlueprintCallable)
    bool DeleteProfile(const FString& ProfileId);

    UFUNCTION(BlueprintCallable)
    bool ImportProfile(const FString& SourceFilename, FString& OutProfileId);

    UFUNCTION(BlueprintCallable)
    bool ExportProfile(const FString& ProfileId, const FString& DestinationFilename) const;

    UFUNCTION(BlueprintCallable)
    void ReloadProfiles();

    UFUNCTION(BlueprintPure)
    bool GetAxisValue(FName Action, float& OutValue) const;

    UFUNCTION(BlueprintPure)
    bool IsActionPressed(FName Action) const;

    UFUNCTION(BlueprintPure)
    bool GetHatAngle(int32 HatIndex, float& OutDegrees) const;

    UFUNCTION(BlueprintPure)
    bool GetRawAxisValue(const FString& DeviceId, int32 AxisIndex, float& OutRawValue) const;

    UFUNCTION(BlueprintPure)
    bool IsRawButtonPressed(const FString& DeviceId, int32 ButtonIndex) const;

    UFUNCTION(BlueprintPure)
    bool GetRawHatAngle(const FString& DeviceId, int32 HatIndex, float& OutDegrees) const;

    UFUNCTION(BlueprintPure)
    bool GetActiveProfile(FRotorlineFlightControllerProfile& OutProfile) const;

    UFUNCTION(BlueprintCallable)
    bool BeginCalibration(const FString& DeviceId);

    UFUNCTION(BlueprintCallable)
    bool CaptureCalibrationCenter();

    UFUNCTION(BlueprintCallable)
    bool FinishCalibration(bool bApplyToActiveProfile);

    UFUNCTION(BlueprintCallable)
    void CancelCalibration();

    UFUNCTION(BlueprintPure)
    bool IsCalibrating() const { return bCalibrationActive; }

    static float FilterAxisValue(float RawValue, const FRotorlineAxisBinding& Binding);

private:
    struct FNativeState
    {
        TArray<float> RawAxes;
        TArray<bool> Buttons;
        TArray<float> HatAngles;
        bool bConnected = false;
    };

    void PollInput();
    bool PollNativeDevice(const FRotorlineControllerDeviceInfo& Device, FNativeState& OutState) const;
    bool EnsureGameInputInitialized();
    void ReleaseGameInput();
    void EnumerateGameInputDevices(TArray<FRotorlineControllerDeviceInfo>& OutDevices);
    const FRotorlineControllerDeviceInfo* FindDevice(const FString& DeviceId) const;
    const FRotorlineFlightControllerProfile* FindProfile(const FString& ProfileId) const;
    const FRotorlineFlightControllerProfile* FindCompatibleProfile(const FRotorlineControllerDeviceInfo& Device) const;
    bool LoadProfileFile(
        const FString& Filename,
        FRotorlineFlightControllerProfile& OutProfile,
        bool* bOutMigrated = nullptr) const;
    bool WriteProfileFile(const FRotorlineFlightControllerProfile& Profile, const FString& Filename) const;
    bool ValidateProfile(FRotorlineFlightControllerProfile& Profile, FString& OutReason) const;
    FString GetProfileDirectory() const;
    FString GetProfileFilename(const FString& ProfileId) const;
    void ClearActiveStateAfterDisconnect(const FString& DeviceId);
    void UpdateCalibrationCapture(const FNativeState& State);

    UPROPERTY()
    TArray<FRotorlineControllerDeviceInfo> Devices;

    UPROPERTY()
    TArray<FRotorlineFlightControllerProfile> Profiles;

    FString ActiveDeviceId;
    FString ActiveProfileId;
    TMap<FString, FNativeState> NativeStates;
    void* GameInputInterface = nullptr;
    TMap<FString, void*> GameInputDevices;

    bool bCalibrationActive = false;
    FString CalibrationDeviceId;
    TArray<FRotorlineAxisCalibration> PendingCalibration;
};
