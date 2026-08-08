#include "RotorlineFlightControllerSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <mmsystem.h>
#include <GameInput.h>
#include "Windows/HideWindowsPlatformTypes.h"
#ifdef PlaySound
#undef PlaySound
#endif
#endif

DEFINE_LOG_CATEGORY_STATIC(LogRotorlineFlightController, Log, All);

namespace RotorlineFlightControllerActions
{
    const FName Roll(TEXT("Flight.Roll"));
    const FName Pitch(TEXT("Flight.Pitch"));
    const FName Yaw(TEXT("Flight.Yaw"));
    const FName Collective(TEXT("Flight.Collective"));
    const FName Throttle(TEXT("Flight.Throttle"));
    const FName LookX(TEXT("View.LookX"));
    const FName LookY(TEXT("View.LookY"));
    const FName PrimaryFire(TEXT("Combat.PrimaryFire"));
    const FName SecondaryFire(TEXT("Combat.SecondaryFire"));
    const FName WeaponNext(TEXT("Combat.WeaponNext"));
    const FName WeaponPrevious(TEXT("Combat.WeaponPrevious"));
    const FName TargetLock(TEXT("Combat.TargetLock"));
    const FName MissionInteract(TEXT("Mission.Interaction"));
    const FName LandingGear(TEXT("Flight.LandingGear"));
    const FName Searchlight(TEXT("Flight.Searchlight"));
    const FName ChangeCamera(TEXT("View.ChangeCamera"));
    const FName CockpitView(TEXT("View.Cockpit"));
    const FName ExternalView(TEXT("View.External"));
    const FName MapView(TEXT("View.Map"));
    const FName RadioCommand(TEXT("Radio.Command"));
    const FName FireCannon(TEXT("Combat.FireCannon"));
    const FName FireRockets(TEXT("Combat.FireRockets"));
    const FName FireMissile(TEXT("Combat.FireMissile"));
    const FName Countermeasures(TEXT("Combat.Countermeasures"));
    const FName Boost(TEXT("Flight.Boost"));
    const FName CycleWeapon(TEXT("Combat.CycleWeapon"));
    const FName TargetNext(TEXT("Combat.TargetNext"));
    const FName TargetPrevious(TEXT("Combat.TargetPrevious"));
    const FName GearToggle(TEXT("Flight.GearToggle"));
    const FName Pause(TEXT("System.Pause"));
    const FName MenuAccept(TEXT("Menu.Accept"));
    const FName MenuBack(TEXT("Menu.Back"));

    const TArray<FName>& All()
    {
        static const TArray<FName> Actions = {
            Roll, Pitch, Yaw, Collective, Throttle, LookX, LookY,
            PrimaryFire, SecondaryFire, WeaponNext, WeaponPrevious, TargetLock,
            MissionInteract, LandingGear, Searchlight, ChangeCamera,
            CockpitView, ExternalView, MapView, RadioCommand,
            FireCannon, FireRockets, FireMissile, Countermeasures, Boost,
            CycleWeapon, TargetNext, TargetPrevious, GearToggle,
            Pause, MenuAccept, MenuBack
        };
        return Actions;
    }

    bool IsKnown(const FName Action)
    {
        return Action.IsNone() || All().Contains(Action);
    }
}

namespace
{
    constexpr int32 CurrentProfileSchemaVersion = 2;

    bool IsVirpilDeviceName(const FString& DisplayName)
    {
        const FString UpperName = DisplayName.ToUpper();
        return UpperName.Contains(TEXT("VIRPIL")) ||
            UpperName.StartsWith(TEXT("VPC ")) ||
            UpperName.Contains(TEXT("MONGOOST")) ||
            UpperName.Contains(TEXT("WARBRD")) ||
            UpperName.Contains(TEXT("CONSTELLATION"));
    }

    bool IsVirpilFlightStickName(const FString& DisplayName)
    {
        if (!IsVirpilDeviceName(DisplayName))
        {
            return false;
        }

        // VPC devices are independently configurable USB controllers. Only
        // apply flight-stick defaults when the product name identifies a
        // stick/base; throttles, collectives, pedals and panels remain fully
        // discoverable and rebindable without unsafe guessed axis mappings.
        const FString UpperName = DisplayName.ToUpper();
        if (UpperName.Contains(TEXT("THROTTLE")) ||
            UpperName.Contains(TEXT("COLLECTIVE")) ||
            UpperName.Contains(TEXT("PEDAL")) ||
            UpperName.Contains(TEXT("PANEL")))
        {
            return false;
        }
        return UpperName.Contains(TEXT("STICK")) ||
            UpperName.Contains(TEXT("BASE")) ||
            UpperName.Contains(TEXT("WARBRD")) ||
            UpperName.Contains(TEXT("CONSTELLATION"));
    }

    bool MigrateProfileToCurrentSchema(FRotorlineFlightControllerProfile& Profile)
    {
        if (Profile.SchemaVersion == CurrentProfileSchemaVersion)
        {
            return false;
        }
        if (Profile.SchemaVersion != 1)
        {
            return false;
        }

        // Schema 2 separates view/zoom from weapon selection on Rotorline's
        // verified Logitech Extreme 3D Pro layout. B3/B4 own weapon selection;
        // the hat remains available for the forward/external camera views.
        const bool bVerifiedExtreme3DPro = Profile.VendorId == 0x046D &&
            Profile.ProductId == 0xC215 && Profile.ExpectedButtonCount >= 4 &&
            Profile.ExpectedHatCount >= 1;
        if (bVerifiedExtreme3DPro)
        {
            Profile.ButtonBindings.RemoveAll([](const FRotorlineButtonBinding& Binding)
            {
                return Binding.NativeButtonIndex == 2 || Binding.NativeButtonIndex == 3 ||
                    Binding.Action == RotorlineFlightControllerActions::WeaponNext ||
                    Binding.Action == RotorlineFlightControllerActions::WeaponPrevious;
            });

            FRotorlineButtonBinding NextWeapon;
            NextWeapon.Action = RotorlineFlightControllerActions::WeaponNext;
            NextWeapon.NativeButtonIndex = 2; // Physical B3.
            Profile.ButtonBindings.Add(NextWeapon);

            FRotorlineButtonBinding PreviousWeapon;
            PreviousWeapon.Action = RotorlineFlightControllerActions::WeaponPrevious;
            PreviousWeapon.NativeButtonIndex = 3; // Physical B4.
            Profile.ButtonBindings.Add(PreviousWeapon);

            for (FRotorlineHatBinding& Hat : Profile.HatBindings)
            {
                if (Hat.UpAction == RotorlineFlightControllerActions::WeaponNext ||
                    Hat.UpAction == RotorlineFlightControllerActions::WeaponPrevious) Hat.UpAction = NAME_None;
                if (Hat.RightAction == RotorlineFlightControllerActions::WeaponNext ||
                    Hat.RightAction == RotorlineFlightControllerActions::WeaponPrevious) Hat.RightAction = NAME_None;
                if (Hat.DownAction == RotorlineFlightControllerActions::WeaponNext ||
                    Hat.DownAction == RotorlineFlightControllerActions::WeaponPrevious) Hat.DownAction = NAME_None;
                if (Hat.LeftAction == RotorlineFlightControllerActions::WeaponNext ||
                    Hat.LeftAction == RotorlineFlightControllerActions::WeaponPrevious) Hat.LeftAction = NAME_None;
            }
        }

        Profile.SchemaVersion = CurrentProfileSchemaVersion;
        return true;
    }

    FString MakeSafeId(FString Value)
    {
        Value.TrimStartAndEndInline();
        for (TCHAR& Character : Value)
        {
            if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_'))
            {
                Character = TEXT('_');
            }
        }
        return Value.Left(96);
    }

    TSharedPtr<FJsonObject> AxisBindingToJson(const FRotorlineAxisBinding& Binding)
    {
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("action"), Binding.Action.ToString());
        Object->SetNumberField(TEXT("axis"), Binding.NativeAxisIndex);
        Object->SetBoolField(TEXT("centered"), Binding.bCentered);
        Object->SetBoolField(TEXT("invert"), Binding.bInvert);
        Object->SetNumberField(TEXT("deadzone"), Binding.Deadzone);
        Object->SetNumberField(TEXT("sensitivity"), Binding.Sensitivity);
        Object->SetNumberField(TEXT("curve"), Binding.CurveExponent);
        Object->SetNumberField(TEXT("scale"), Binding.Scale);
        Object->SetNumberField(TEXT("minimum"), Binding.Calibration.RawMinimum);
        Object->SetNumberField(TEXT("center"), Binding.Calibration.RawCenter);
        Object->SetNumberField(TEXT("maximum"), Binding.Calibration.RawMaximum);
        Object->SetNumberField(TEXT("noiseFloor"), Binding.Calibration.NoiseFloor);
        Object->SetStringField(TEXT("userLabel"), Binding.UserLabel);
        Object->SetBoolField(TEXT("ignore"), Binding.bIgnore);
        Object->SetNumberField(TEXT("centerOffset"), Binding.CenterOffset);
        return Object;
    }

    bool TryNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double& OutValue)
    {
        return Object.IsValid() && Object->TryGetNumberField(Field, OutValue) && FMath::IsFinite(OutValue);
    }

    bool ReadAction(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FName& OutAction)
    {
        FString Value;
        if (!Object.IsValid() || !Object->TryGetStringField(Field, Value))
        {
            return false;
        }
        OutAction = FName(*Value);
        return RotorlineFlightControllerActions::IsKnown(OutAction);
    }

    float AngularDistance(float A, float B)
    {
        return FMath::Abs(FMath::FindDeltaAngleDegrees(A, B));
    }

#if PLATFORM_WINDOWS
    using namespace GameInput::v3;

    struct FGameInputEnumerationContext
    {
        TArray<IGameInputDevice*> Devices;
    };

    void CALLBACK CaptureGameInputDevice(
        GameInputCallbackToken,
        void* Context,
        IGameInputDevice* Device,
        uint64,
        GameInputDeviceStatus CurrentStatus,
        GameInputDeviceStatus)
    {
        if (!Context || !Device || (CurrentStatus & GameInputDeviceConnected) == 0)
        {
            return;
        }
        Device->AddRef();
        static_cast<FGameInputEnumerationContext*>(Context)->Devices.Add(Device);
    }

    FString GuidToStableString(const GUID& Guid)
    {
        return FString::Printf(
            TEXT("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"),
            Guid.Data1, Guid.Data2, Guid.Data3,
            Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
            Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
    }
#endif
}

void URotorlineFlightControllerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ReloadProfiles();
    RefreshDevices();
}

void URotorlineFlightControllerSubsystem::Deinitialize()
{
    ReleaseGameInput();
    Devices.Reset();
    Profiles.Reset();
    NativeStates.Reset();
    ActiveDeviceId.Reset();
    ActiveProfileId.Reset();
    Super::Deinitialize();
}

void URotorlineFlightControllerSubsystem::Tick(float DeltaTime)
{
    // RefreshDevices performs a blocking Windows HID/GameInput enumeration.
    // Keep that work at startup and behind the explicit RESCAN action in the
    // controls screen. Running it from Tick caused a 60-100 ms game-thread
    // stall every second whenever no dedicated flight controller was active.
    PollInput();
}

TStatId URotorlineFlightControllerSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URotorlineFlightControllerSubsystem, STATGROUP_Tickables);
}

bool URotorlineFlightControllerSubsystem::IsTickable() const
{
    return !HasAnyFlags(RF_ClassDefaultObject) && GetGameInstance() != nullptr;
}

UWorld* URotorlineFlightControllerSubsystem::GetTickableGameObjectWorld() const
{
    return GetWorld();
}

bool URotorlineFlightControllerSubsystem::EnsureGameInputInitialized()
{
#if PLATFORM_WINDOWS
    if (GameInputInterface)
    {
        return true;
    }
    GameInput::v3::IGameInput* Interface = nullptr;
    if (FAILED(GameInput::v3::GameInputCreate(&Interface)) || !Interface)
    {
        UE_LOG(LogRotorlineFlightController, Warning,
            TEXT("ROTORLINE_CONTROLLER|BACKEND_UNAVAILABLE|backend=GameInput|fallback=WinMM"));
        return false;
    }
    GameInputInterface = Interface;
    return true;
#else
    return false;
#endif
}

void URotorlineFlightControllerSubsystem::ReleaseGameInput()
{
#if PLATFORM_WINDOWS
    for (const TPair<FString, void*>& Pair : GameInputDevices)
    {
        if (GameInput::v3::IGameInputDevice* Device =
            static_cast<GameInput::v3::IGameInputDevice*>(Pair.Value))
        {
            Device->Release();
        }
    }
    GameInputDevices.Reset();
    if (GameInput::v3::IGameInput* Interface =
        static_cast<GameInput::v3::IGameInput*>(GameInputInterface))
    {
        Interface->Release();
    }
#endif
    GameInputInterface = nullptr;
}

void URotorlineFlightControllerSubsystem::EnumerateGameInputDevices(
    TArray<FRotorlineControllerDeviceInfo>& OutDevices)
{
#if PLATFORM_WINDOWS
    if (!EnsureGameInputInitialized())
    {
        return;
    }

    for (const TPair<FString, void*>& Pair : GameInputDevices)
    {
        if (GameInput::v3::IGameInputDevice* Device =
            static_cast<GameInput::v3::IGameInputDevice*>(Pair.Value))
        {
            Device->Release();
        }
    }
    GameInputDevices.Reset();

    GameInput::v3::IGameInput* Interface =
        static_cast<GameInput::v3::IGameInput*>(GameInputInterface);
    FGameInputEnumerationContext Context;
    GameInput::v3::GameInputCallbackToken Token = 0;
    const HRESULT Result = Interface->RegisterDeviceCallback(
        nullptr,
        GameInput::v3::GameInputKindController,
        GameInput::v3::GameInputDeviceConnected,
        GameInput::v3::GameInputBlockingEnumeration,
        &Context,
        CaptureGameInputDevice,
        &Token);
    if (FAILED(Result))
    {
        return;
    }
    Interface->UnregisterCallback(Token);

    TSet<UINT> ClaimedWinMMIndices;
    for (GameInput::v3::IGameInputDevice* NativeDevice : Context.Devices)
    {
        const GameInput::v3::GameInputDeviceInfo* Info = nullptr;
        if (!NativeDevice || FAILED(NativeDevice->GetDeviceInfo(&Info)) || !Info ||
            (Info->supportedInput & GameInput::v3::GameInputKindController) == 0)
        {
            if (NativeDevice) NativeDevice->Release();
            continue;
        }

        GameInput::v3::IGameInputReading* Reading = nullptr;
        if (FAILED(Interface->GetCurrentReading(
                GameInput::v3::GameInputKindController, NativeDevice, &Reading)) || !Reading)
        {
            NativeDevice->Release();
            continue;
        }

        FRotorlineControllerDeviceInfo Device;
        Device.VendorId = Info->vendorId;
        Device.ProductId = Info->productId;
        Device.DisplayName = Info->displayName ? UTF8_TO_TCHAR(Info->displayName) : TEXT("");
        if (Device.VendorId == 0x046D && Device.ProductId == 0xC215)
        {
            Device.DisplayName = TEXT("Logitech Extreme 3D Pro");
        }
        if (Device.DisplayName.IsEmpty())
        {
            Device.DisplayName = FString::Printf(
                TEXT("Flight Controller %04X:%04X"), Device.VendorId, Device.ProductId);
        }
        if (IsVirpilDeviceName(Device.DisplayName))
        {
            Device.BackendName = TEXT("Microsoft GameInput (VIRPIL VPC native HID)");
        }
        const FString PnpPath = Info->pnpPath ? UTF8_TO_TCHAR(Info->pnpPath) : TEXT("");
        Device.DeviceId = FString::Printf(
            TEXT("GAMEINPUT-%04X-%04X-%s-%08X"),
            Device.VendorId,
            Device.ProductId,
            *GuidToStableString(Info->containerId),
            FCrc::StrCrc32(*PnpPath));
        Device.NativeDeviceIndex = INDEX_NONE;
        if (Device.BackendName.IsEmpty())
        {
            Device.BackendName = TEXT("Microsoft GameInput");
        }
        // GameInput does not guarantee a meaningful centered sample until the
        // controller has produced its first report.  Flight sticks exposed by
        // WinMM already have a current absolute state, so pair the two views
        // when Windows exposes the same VID/PID through both APIs.  GameInput
        // remains the identity/capability backend; WinMM supplies the initial
        // and subsequent absolute samples without the startup zero-frame.
        const UINT WinMMDeviceCount = joyGetNumDevs();
        for (UINT WinMMIndex = 0; WinMMIndex < WinMMDeviceCount; ++WinMMIndex)
        {
            if (ClaimedWinMMIndices.Contains(WinMMIndex))
            {
                continue;
            }
            JOYCAPSW WinMMCaps{};
            JOYINFOEX WinMMProbe{};
            WinMMProbe.dwSize = sizeof(WinMMProbe);
            WinMMProbe.dwFlags = JOY_RETURNALL;
            if (joyGetDevCapsW(WinMMIndex, &WinMMCaps, sizeof(WinMMCaps)) == JOYERR_NOERROR &&
                joyGetPosEx(WinMMIndex, &WinMMProbe) == JOYERR_NOERROR &&
                static_cast<int32>(WinMMCaps.wMid) == Device.VendorId &&
                static_cast<int32>(WinMMCaps.wPid) == Device.ProductId)
            {
                Device.NativeDeviceIndex = static_cast<int32>(WinMMIndex);
                Device.BackendName = IsVirpilDeviceName(Device.DisplayName)
                    ? TEXT("Microsoft GameInput + WinMM absolute state (VIRPIL VPC native HID)")
                    : TEXT("Microsoft GameInput + WinMM absolute state");
                ClaimedWinMMIndices.Add(WinMMIndex);
                break;
            }
        }
        Device.bConnected = true;
        Device.bGamepadCompatible = (Info->supportedInput & GameInput::v3::GameInputKindGamepad) != 0;
        Device.Capabilities.AxisCount = FMath::Min<int32>(Reading->GetControllerAxisCount(), 256);
        Device.Capabilities.ButtonCount = FMath::Min<int32>(Reading->GetControllerButtonCount(), 256);
        Device.Capabilities.HatCount = FMath::Min<int32>(Reading->GetControllerSwitchCount(), 32);
        for (int32 AxisIndex = 0; AxisIndex < Device.Capabilities.AxisCount; ++AxisIndex)
        {
            FRotorlineControllerAxisCapability Axis;
            Axis.NativeName = FName(*FString::Printf(TEXT("Axis %d"), AxisIndex + 1));
            Axis.NativeIndex = AxisIndex;
            Axis.RawMinimum = 0.0f;
            Axis.RawMaximum = 1.0f;
            Device.Capabilities.Axes.Add(Axis);
        }
        Reading->Release();

        // Callback gave us an owned reference; keep it until the next refresh.
        GameInputDevices.Add(Device.DeviceId, NativeDevice);
        OutDevices.Add(MoveTemp(Device));
    }
#endif
}

void URotorlineFlightControllerSubsystem::RefreshDevices()
{
    TArray<FRotorlineControllerDeviceInfo> FoundDevices;

    // GameInput is the primary backend because controller axes, buttons, and
    // switches are truly variable-length and devices carry stable HID identity.
    EnumerateGameInputDevices(FoundDevices);

#if PLATFORM_WINDOWS
    static const FName AxisNames[] = {
        TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("R"), TEXT("U"), TEXT("V")
    };
    const UINT DeviceCount = joyGetNumDevs();
    for (UINT DeviceIndex = 0; DeviceIndex < DeviceCount; ++DeviceIndex)
    {
        JOYCAPSW Caps{};
        if (joyGetDevCapsW(DeviceIndex, &Caps, sizeof(Caps)) != JOYERR_NOERROR)
        {
            continue;
        }

        JOYINFOEX Probe{};
        Probe.dwSize = sizeof(Probe);
        Probe.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx(DeviceIndex, &Probe) != JOYERR_NOERROR)
        {
            continue;
        }

        // Do not show the same HID twice when GameInput already owns it.
        if (FoundDevices.ContainsByPredicate([&Caps](const FRotorlineControllerDeviceInfo& Existing)
            {
                return Existing.VendorId == static_cast<int32>(Caps.wMid) &&
                    Existing.ProductId == static_cast<int32>(Caps.wPid);
            }))
        {
            continue;
        }

        FRotorlineControllerDeviceInfo Device;
        Device.DisplayName = FString(Caps.szPname).TrimStartAndEnd();
        // Some WinMM HID bridges expose only the generic Microsoft driver
        // label.  Keep capability discovery fully dynamic, but provide a
        // useful product label for hardware we can identify unambiguously.
        if (Caps.wMid == 0x046D && Caps.wPid == 0xC215)
        {
            Device.DisplayName = TEXT("Logitech Extreme 3D Pro");
        }
        if (Device.DisplayName.IsEmpty())
        {
            Device.DisplayName = FString::Printf(TEXT("Windows Flight Controller %u"), DeviceIndex + 1);
        }
        Device.VendorId = static_cast<int32>(Caps.wMid);
        Device.ProductId = static_cast<int32>(Caps.wPid);
        // Keep the DualSense in the existing gamepad path when Windows only
        // exposes it through WinMM, rather than offering it as a joystick.
        Device.bGamepadCompatible = Device.VendorId == 0x054C && Device.ProductId == 0x0CE6;
        Device.NativeDeviceIndex = static_cast<int32>(DeviceIndex);
        Device.BackendName = TEXT("WinMM fallback (6 axes / 32 buttons / 1 POV maximum)");
        if (IsVirpilDeviceName(Device.DisplayName))
        {
            Device.BackendName += TEXT(" - VIRPIL VPC; GameInput recommended for full button range");
        }
        Device.DeviceId = FString::Printf(
            TEXT("WINMM-%04X-%04X-%08X-J%u"),
            Device.VendorId,
            Device.ProductId,
            FCrc::StrCrc32(*Device.DisplayName.ToLower()),
            DeviceIndex);
        Device.bConnected = true;
        Device.Capabilities.ButtonCount = FMath::Clamp<int32>(Caps.wNumButtons, 0, 32);
        Device.Capabilities.HatCount = (Caps.wCaps & JOYCAPS_HASPOV) != 0 ? 1 : 0;

        auto AddAxis = [&Device](int32 NativeIndex, DWORD Minimum, DWORD Maximum)
        {
            FRotorlineControllerAxisCapability Axis;
            Axis.NativeName = AxisNames[NativeIndex];
            Axis.NativeIndex = NativeIndex;
            Axis.RawMinimum = static_cast<int32>(Minimum);
            Axis.RawMaximum = static_cast<int32>(Maximum);
            Device.Capabilities.Axes.Add(Axis);
        };

        AddAxis(0, Caps.wXmin, Caps.wXmax);
        AddAxis(1, Caps.wYmin, Caps.wYmax);
        if ((Caps.wCaps & JOYCAPS_HASZ) != 0) AddAxis(2, Caps.wZmin, Caps.wZmax);
        if ((Caps.wCaps & JOYCAPS_HASR) != 0) AddAxis(3, Caps.wRmin, Caps.wRmax);
        if ((Caps.wCaps & JOYCAPS_HASU) != 0) AddAxis(4, Caps.wUmin, Caps.wUmax);
        if ((Caps.wCaps & JOYCAPS_HASV) != 0) AddAxis(5, Caps.wVmin, Caps.wVmax);
        Device.Capabilities.AxisCount = Device.Capabilities.Axes.Num();
        FoundDevices.Add(MoveTemp(Device));
    }
#endif

    TSet<FString> PreviousIds;
    for (const FRotorlineControllerDeviceInfo& Device : Devices)
    {
        if (Device.bConnected)
        {
            PreviousIds.Add(Device.DeviceId);
        }
    }

    TSet<FString> CurrentIds;
    for (const FRotorlineControllerDeviceInfo& Device : FoundDevices)
    {
        CurrentIds.Add(Device.DeviceId);
        if (!PreviousIds.Contains(Device.DeviceId))
        {
            UE_LOG(LogRotorlineFlightController, Log,
                TEXT("ROTORLINE_CONTROLLER|CONNECTED|id=%s|name=%s|axes=%d|buttons=%d|hats=%d"),
                *Device.DeviceId, *Device.DisplayName, Device.Capabilities.AxisCount,
                Device.Capabilities.ButtonCount, Device.Capabilities.HatCount);
            OnControllerConnected.Broadcast(Device.DeviceId);
            if (IsVirpilDeviceName(Device.DisplayName))
            {
                UE_LOG(LogRotorlineFlightController, Display,
                    TEXT("ROTORLINE_CONTROLLER|VIRPIL_READY|id=%s|name=%s|backend=%s|axes=%d|buttons=%d|hats=%d|profile=dynamic"),
                    *Device.DeviceId, *Device.DisplayName, *Device.BackendName,
                    Device.Capabilities.AxisCount, Device.Capabilities.ButtonCount,
                    Device.Capabilities.HatCount);
            }
        }
    }

    for (const FString& PreviousId : PreviousIds)
    {
        if (!CurrentIds.Contains(PreviousId))
        {
            UE_LOG(LogRotorlineFlightController, Warning,
                TEXT("ROTORLINE_CONTROLLER|DISCONNECTED|id=%s|fallback=keyboard_gamepad"), *PreviousId);
            OnControllerDisconnected.Broadcast(PreviousId);
            ClearActiveStateAfterDisconnect(PreviousId);
        }
    }

    Devices = MoveTemp(FoundDevices);

    if (ActiveDeviceId.IsEmpty() && Devices.Num() > 0)
    {
        const FRotorlineControllerDeviceInfo* Preferred = Devices.FindByPredicate([](const FRotorlineControllerDeviceInfo& Device)
        {
            return !Device.bGamepadCompatible;
        });
        if (Preferred) SetActiveDevice(Preferred->DeviceId);
    }
}

void URotorlineFlightControllerSubsystem::PollInput()
{
    for (const FRotorlineControllerDeviceInfo& Device : Devices)
    {
        FNativeState State;
        if (PollNativeDevice(Device, State))
        {
            NativeStates.Add(Device.DeviceId, MoveTemp(State));
        }
        else
        {
            NativeStates.Remove(Device.DeviceId);
            if (Device.DeviceId == ActiveDeviceId)
            {
                ClearActiveStateAfterDisconnect(Device.DeviceId);
            }
        }
    }

    if (bCalibrationActive && CalibrationDeviceId == ActiveDeviceId)
    {
        if (const FNativeState* State = NativeStates.Find(CalibrationDeviceId))
        {
            UpdateCalibrationCapture(*State);
        }
    }
}

bool URotorlineFlightControllerSubsystem::PollNativeDevice(
    const FRotorlineControllerDeviceInfo& Device,
    FNativeState& OutState) const
{
#if PLATFORM_WINDOWS
    if (Device.BackendName.StartsWith(TEXT("Microsoft GameInput")))
    {
        if (Device.NativeDeviceIndex != INDEX_NONE)
        {
            JOYCAPSW Caps{};
            JOYINFOEX Info{};
            Info.dwSize = sizeof(Info);
            Info.dwFlags = JOY_RETURNALL;
            const UINT NativeIndex = static_cast<UINT>(Device.NativeDeviceIndex);
            if (joyGetDevCapsW(NativeIndex, &Caps, sizeof(Caps)) == JOYERR_NOERROR &&
                joyGetPosEx(NativeIndex, &Info) == JOYERR_NOERROR)
            {
                const DWORD NativeAxes[] = {
                    Info.dwXpos, Info.dwYpos, Info.dwZpos,
                    Info.dwRpos, Info.dwUpos, Info.dwVpos
                };
                const DWORD NativeMinimums[] = {
                    Caps.wXmin, Caps.wYmin, Caps.wZmin,
                    Caps.wRmin, Caps.wUmin, Caps.wVmin
                };
                const DWORD NativeMaximums[] = {
                    Caps.wXmax, Caps.wYmax, Caps.wZmax,
                    Caps.wRmax, Caps.wUmax, Caps.wVmax
                };
                OutState.RawAxes.Init(0.5f, Device.Capabilities.AxisCount);
                for (int32 AxisIndex = 0; AxisIndex < Device.Capabilities.Axes.Num(); ++AxisIndex)
                {
                    const int32 SourceIndex = Device.Capabilities.Axes[AxisIndex].NativeIndex;
                    if (SourceIndex >= 0 && SourceIndex < UE_ARRAY_COUNT(NativeAxes))
                    {
                        const float Range = static_cast<float>(NativeMaximums[SourceIndex] -
                            NativeMinimums[SourceIndex]);
                        OutState.RawAxes[AxisIndex] = Range > 0.0f
                            ? FMath::Clamp(
                                (static_cast<float>(NativeAxes[SourceIndex]) -
                                    static_cast<float>(NativeMinimums[SourceIndex])) / Range,
                                0.0f,
                                1.0f)
                            : 0.5f;
                    }
                }

                OutState.Buttons.Init(false, Device.Capabilities.ButtonCount);
                for (int32 ButtonIndex = 0; ButtonIndex < OutState.Buttons.Num(); ++ButtonIndex)
                {
                    OutState.Buttons[ButtonIndex] =
                        ButtonIndex < 32 && (Info.dwButtons & (1u << ButtonIndex)) != 0;
                }
                OutState.HatAngles.Init(-1.0f, Device.Capabilities.HatCount);
                if (Device.Capabilities.HatCount > 0 && Info.dwPOV != JOY_POVCENTERED)
                {
                    OutState.HatAngles[0] = static_cast<float>(Info.dwPOV) / 100.0f;
                }
                OutState.bConnected = true;
                return true;
            }
        }

        GameInput::v3::IGameInput* Interface =
            static_cast<GameInput::v3::IGameInput*>(GameInputInterface);
        void* const* OpaqueDevice = GameInputDevices.Find(Device.DeviceId);
        GameInput::v3::IGameInputDevice* NativeDevice = OpaqueDevice
            ? static_cast<GameInput::v3::IGameInputDevice*>(*OpaqueDevice)
            : nullptr;
        if (!Interface || !NativeDevice)
        {
            return false;
        }

        GameInput::v3::IGameInputReading* Reading = nullptr;
        if (FAILED(Interface->GetCurrentReading(
                GameInput::v3::GameInputKindController, NativeDevice, &Reading)) || !Reading)
        {
            return false;
        }

        const int32 AxisCount = FMath::Min<int32>(Reading->GetControllerAxisCount(), 256);
        const int32 ButtonCount = FMath::Min<int32>(Reading->GetControllerButtonCount(), 256);
        const int32 SwitchCount = FMath::Min<int32>(Reading->GetControllerSwitchCount(), 32);
        OutState.RawAxes.SetNumZeroed(AxisCount);
        OutState.Buttons.Init(false, ButtonCount);
        TArray<GameInput::v3::GameInputSwitchPosition> Switches;
        Switches.Init(GameInput::v3::GameInputSwitchCenter, SwitchCount);
        Reading->GetControllerAxisState(AxisCount, OutState.RawAxes.GetData());
        Reading->GetControllerButtonState(ButtonCount, OutState.Buttons.GetData());
        Reading->GetControllerSwitchState(SwitchCount, Switches.GetData());
        Reading->Release();

        OutState.HatAngles.Init(-1.0f, SwitchCount);
        for (int32 SwitchIndex = 0; SwitchIndex < SwitchCount; ++SwitchIndex)
        {
            if (Switches[SwitchIndex] != GameInput::v3::GameInputSwitchCenter)
            {
                OutState.HatAngles[SwitchIndex] =
                    (static_cast<int32>(Switches[SwitchIndex]) - 1) * 45.0f;
            }
        }
        OutState.bConnected = true;
        return true;
    }

    JOYINFOEX Info{};
    Info.dwSize = sizeof(Info);
    Info.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(static_cast<UINT>(Device.NativeDeviceIndex), &Info) != JOYERR_NOERROR)
    {
        return false;
    }

    const DWORD NativeAxes[] = {
        Info.dwXpos, Info.dwYpos, Info.dwZpos, Info.dwRpos, Info.dwUpos, Info.dwVpos
    };
    OutState.RawAxes.Init(0.0f, Device.Capabilities.AxisCount);
    for (int32 AxisIndex = 0; AxisIndex < Device.Capabilities.Axes.Num(); ++AxisIndex)
    {
        const int32 NativeIndex = Device.Capabilities.Axes[AxisIndex].NativeIndex;
        if (NativeIndex >= 0 && NativeIndex < UE_ARRAY_COUNT(NativeAxes))
        {
            OutState.RawAxes[AxisIndex] = static_cast<float>(NativeAxes[NativeIndex]);
        }
    }

    OutState.Buttons.Init(false, Device.Capabilities.ButtonCount);
    for (int32 ButtonIndex = 0; ButtonIndex < OutState.Buttons.Num(); ++ButtonIndex)
    {
        OutState.Buttons[ButtonIndex] = (Info.dwButtons & (1u << ButtonIndex)) != 0;
    }

    OutState.HatAngles.Init(-1.0f, Device.Capabilities.HatCount);
    if (Device.Capabilities.HatCount > 0 && Info.dwPOV != JOY_POVCENTERED)
    {
        OutState.HatAngles[0] = static_cast<float>(Info.dwPOV) / 100.0f;
    }
    OutState.bConnected = true;
    return true;
#else
    return false;
#endif
}

bool URotorlineFlightControllerSubsystem::SetActiveDevice(const FString& DeviceId)
{
    const FRotorlineControllerDeviceInfo* Device = FindDevice(DeviceId);
    if (!Device || !Device->bConnected)
    {
        return false;
    }

    ActiveDeviceId = DeviceId;
    if (const FRotorlineFlightControllerProfile* Exact = Profiles.FindByPredicate([Device](const FRotorlineFlightControllerProfile& Profile)
        {
            return Profile.DeviceId == Device->DeviceId &&
                Profile.ExpectedAxisCount <= Device->Capabilities.AxisCount &&
                Profile.ExpectedButtonCount <= Device->Capabilities.ButtonCount &&
                Profile.ExpectedHatCount <= Device->Capabilities.HatCount;
        }))
    {
        ActiveProfileId = Exact->ProfileId;
        OnActiveProfileChanged.Broadcast(ActiveProfileId);
    }
    else
    {
        // Rotorline's Logitech layout is verified in hardware. A clean package
        // must never turn its axes or B1-B4 mappings into SETUP_REQUIRED just
        // because the executable directory was refreshed.
        const bool bVerifiedExtreme3DPro = Device->VendorId == 0x046D && Device->ProductId == 0xC215 &&
            Device->Capabilities.AxisCount == 4 && Device->Capabilities.ButtonCount == 12 &&
            Device->Capabilities.HatCount >= 1;
        if (bVerifiedExtreme3DPro)
        {
            const FRotorlineFlightControllerProfile DefaultProfile = MakeDefaultProfile(Device->DeviceId);
            if (SaveProfile(DefaultProfile))
            {
                ActiveProfileId = DefaultProfile.ProfileId;
                UE_LOG(LogRotorlineFlightController, Display,
                    TEXT("ROTORLINE_CONTROLLER_PROFILE|VERIFIED_DEFAULT_APPLIED|device=%s|profile=%s|B1=FIRE|B2=CM|B3=NEXT|B4=PREVIOUS"),
                    *Device->DeviceId, *ActiveProfileId);
            }
            else
            {
                ActiveProfileId.Reset();
            }
        }
        else
        {
            // Unknown hardware still uses the visible setup workflow; never
            // guess at arbitrary axis and button layouts.
            ActiveProfileId.Reset();
        }
        OnActiveProfileChanged.Broadcast(ActiveProfileId);
    }
    return true;
}

bool URotorlineFlightControllerSubsystem::ApplyProfile(const FString& ProfileId)
{
    const FRotorlineControllerDeviceInfo* Device = FindDevice(ActiveDeviceId);
    const FRotorlineFlightControllerProfile* Profile = FindProfile(ProfileId);
    if (!Device || !Profile)
    {
        return false;
    }

    const bool bExact = Profile->DeviceId == Device->DeviceId;
    const bool bIdentityCompatible = bExact ||
        ((Profile->VendorId == 0 || Profile->VendorId == Device->VendorId) &&
         (Profile->ProductId == 0 || Profile->ProductId == Device->ProductId));
    const bool bCompatible = bIdentityCompatible &&
        Profile->ExpectedAxisCount <= Device->Capabilities.AxisCount &&
        Profile->ExpectedButtonCount <= Device->Capabilities.ButtonCount &&
        Profile->ExpectedHatCount <= Device->Capabilities.HatCount;
    if (!bCompatible)
    {
        return false;
    }

    ActiveProfileId = ProfileId;
    OnActiveProfileChanged.Broadcast(ActiveProfileId);
    return true;
}

bool URotorlineFlightControllerSubsystem::ApplyTransientProfile(
    const FRotorlineFlightControllerProfile& Profile)
{
    const FRotorlineControllerDeviceInfo* Device = FindDevice(ActiveDeviceId);
    if (!Device)
    {
        return false;
    }

    FRotorlineFlightControllerProfile Validated = Profile;
    FString Reason;
    if (!ValidateProfile(Validated, Reason))
    {
        UE_LOG(LogRotorlineFlightController, Warning,
            TEXT("ROTORLINE_CONTROLLER_PROFILE|TRANSIENT_REJECTED|reason=%s"), *Reason);
        return false;
    }

    const bool bExact = Validated.DeviceId == Device->DeviceId;
    const bool bIdentityCompatible = bExact ||
        ((Validated.VendorId == 0 || Validated.VendorId == Device->VendorId) &&
         (Validated.ProductId == 0 || Validated.ProductId == Device->ProductId));
    const bool bCompatible = bIdentityCompatible &&
        Validated.ExpectedAxisCount <= Device->Capabilities.AxisCount &&
        Validated.ExpectedButtonCount <= Device->Capabilities.ButtonCount &&
        Validated.ExpectedHatCount <= Device->Capabilities.HatCount;
    if (!bCompatible)
    {
        return false;
    }

    const int32 ExistingIndex = Profiles.IndexOfByPredicate([&Validated](const FRotorlineFlightControllerProfile& Entry)
    {
        return Entry.ProfileId == Validated.ProfileId;
    });
    if (ExistingIndex == INDEX_NONE)
    {
        Profiles.Add(Validated);
    }
    else
    {
        Profiles[ExistingIndex] = Validated;
    }
    ActiveProfileId = Validated.ProfileId;
    OnActiveProfileChanged.Broadcast(ActiveProfileId);
    UE_LOG(LogRotorlineFlightController, Display,
        TEXT("ROTORLINE_CONTROLLER_PROFILE|TRANSIENT_APPLIED|profile=%s|device=%s"),
        *ActiveProfileId, *ActiveDeviceId);
    return true;
}

FRotorlineFlightControllerProfile URotorlineFlightControllerSubsystem::MakeDefaultProfile(const FString& DeviceId) const
{
    FRotorlineFlightControllerProfile Profile;
    const FRotorlineControllerDeviceInfo* Device = FindDevice(DeviceId);
    if (!Device)
    {
        return Profile;
    }

    Profile.SchemaVersion = CurrentProfileSchemaVersion;
    Profile.DeviceId = Device->DeviceId;
    Profile.DeviceName = Device->DisplayName;
    Profile.VendorId = Device->VendorId;
    Profile.ProductId = Device->ProductId;
    Profile.ExpectedAxisCount = Device->Capabilities.AxisCount;
    Profile.ExpectedButtonCount = Device->Capabilities.ButtonCount;
    Profile.ExpectedHatCount = Device->Capabilities.HatCount;
    Profile.DetectedCapabilities = Device->Capabilities;
    Profile.ProfileId = MakeSafeId(FString::Printf(TEXT("%s-default"), *Device->DeviceId));
    Profile.ProfileName = FString::Printf(TEXT("%s Default"), *Device->DisplayName);
    const bool bVerifiedExtreme3DPro = Device->VendorId == 0x046D && Device->ProductId == 0xC215 &&
        Device->Capabilities.AxisCount == 4 && Device->Capabilities.ButtonCount == 12 &&
        Device->Capabilities.HatCount >= 1;
    const bool bVirpilFlightStick = IsVirpilFlightStickName(Device->DisplayName) &&
        Device->Capabilities.AxisCount >= 2;

    const FName DefaultAxisActions[] = {
        RotorlineFlightControllerActions::Roll,
        RotorlineFlightControllerActions::Pitch,
        RotorlineFlightControllerActions::Collective,
        RotorlineFlightControllerActions::Yaw,
        RotorlineFlightControllerActions::LookX,
        RotorlineFlightControllerActions::LookY
    };
    for (int32 AxisIndex = 0; AxisIndex < Device->Capabilities.Axes.Num(); ++AxisIndex)
    {
        const FRotorlineControllerAxisCapability& Capability = Device->Capabilities.Axes[AxisIndex];
        FRotorlineAxisBinding Binding;
        Binding.Action = (bVerifiedExtreme3DPro || bVirpilFlightStick) &&
            AxisIndex < static_cast<int32>(UE_ARRAY_COUNT(DefaultAxisActions))
            ? DefaultAxisActions[AxisIndex]
            : NAME_None;
        // The small base throttle on the Logitech Extreme 3D Pro is awkward
        // to operate while holding the stick. Rotorline's verified hybrid
        // default leaves it unassigned and uses Q/E for descend/ascend.
        if (bVerifiedExtreme3DPro && AxisIndex == 2)
        {
            Binding.Action = NAME_None;
        }
        // VIRPIL sticks can expose additional mini-stick, brake-lever and
        // virtual axes in any VPC-configured order. X/Y are the only portable
        // flight defaults; every other axis is deliberately user-assigned.
        if (bVirpilFlightStick && AxisIndex >= 2)
        {
            Binding.Action = NAME_None;
        }
        Binding.NativeAxisIndex = AxisIndex;
        Binding.UserLabel = Capability.NativeName.ToString();
        Binding.bIgnore = Binding.Action.IsNone();
        Binding.bCentered = !bVerifiedExtreme3DPro || AxisIndex != 2;
        Binding.bInvert = (bVerifiedExtreme3DPro || bVirpilFlightStick) && AxisIndex == 1;
        Binding.Calibration.RawMinimum = Capability.RawMinimum;
        Binding.Calibration.RawMaximum = Capability.RawMaximum;
        Binding.Calibration.RawCenter = (Capability.RawMinimum + Capability.RawMaximum) * 0.5f;
        Profile.AxisBindings.Add(Binding);
    }

    const FName DefaultButtonActions[] = {
        RotorlineFlightControllerActions::PrimaryFire,
        RotorlineFlightControllerActions::Countermeasures,
        RotorlineFlightControllerActions::WeaponNext,
        RotorlineFlightControllerActions::WeaponPrevious,
        RotorlineFlightControllerActions::TargetLock,
        RotorlineFlightControllerActions::SecondaryFire,
        RotorlineFlightControllerActions::MissionInteract,
        RotorlineFlightControllerActions::LandingGear,
        RotorlineFlightControllerActions::Searchlight,
        RotorlineFlightControllerActions::ChangeCamera,
        RotorlineFlightControllerActions::RadioCommand,
        RotorlineFlightControllerActions::Pause
    };
    const int32 DefaultButtonCount = (bVerifiedExtreme3DPro || bVirpilFlightStick) ? FMath::Min(
        Device->Capabilities.ButtonCount,
        bVirpilFlightStick ? 4 : static_cast<int32>(UE_ARRAY_COUNT(DefaultButtonActions))) : 0;
    for (int32 ButtonIndex = 0; ButtonIndex < DefaultButtonCount; ++ButtonIndex)
    {
        FRotorlineButtonBinding Binding;
        Binding.Action = DefaultButtonActions[ButtonIndex];
        Binding.NativeButtonIndex = ButtonIndex;
        Profile.ButtonBindings.Add(Binding);
    }

    if ((bVerifiedExtreme3DPro || bVirpilFlightStick) && Device->Capabilities.HatCount > 0)
    {
        FRotorlineHatBinding Hat;
        Hat.NativeHatIndex = 0;
        Hat.UpAction = RotorlineFlightControllerActions::CockpitView;
        Hat.DownAction = RotorlineFlightControllerActions::ExternalView;
        Hat.RightAction = NAME_None;
        Hat.LeftAction = NAME_None;
        Profile.HatBindings.Add(Hat);
    }
    return Profile;
}

bool URotorlineFlightControllerSubsystem::SaveProfile(const FRotorlineFlightControllerProfile& Profile)
{
    FRotorlineFlightControllerProfile CleanProfile = Profile;
    FString Reason;
    if (!ValidateProfile(CleanProfile, Reason))
    {
        UE_LOG(LogRotorlineFlightController, Warning,
            TEXT("ROTORLINE_CONTROLLER_PROFILE|REJECTED|reason=%s"), *Reason);
        return false;
    }

    if (!WriteProfileFile(CleanProfile, GetProfileFilename(CleanProfile.ProfileId)))
    {
        return false;
    }

    const int32 ExistingIndex = Profiles.IndexOfByPredicate(
        [&CleanProfile](const FRotorlineFlightControllerProfile& Candidate)
        {
            return Candidate.ProfileId == CleanProfile.ProfileId;
        });
    if (ExistingIndex == INDEX_NONE)
    {
        Profiles.Add(MoveTemp(CleanProfile));
    }
    else
    {
        Profiles[ExistingIndex] = MoveTemp(CleanProfile);
    }
    return true;
}

bool URotorlineFlightControllerSubsystem::DeleteProfile(const FString& ProfileId)
{
    if (ProfileId.IsEmpty())
    {
        return false;
    }
    const bool bDeleted = IFileManager::Get().Delete(*GetProfileFilename(ProfileId), false, true);
    Profiles.RemoveAll([&ProfileId](const FRotorlineFlightControllerProfile& Profile)
    {
        return Profile.ProfileId == ProfileId;
    });
    if (ActiveProfileId == ProfileId)
    {
        ActiveProfileId.Reset();
    }
    return bDeleted;
}

bool URotorlineFlightControllerSubsystem::ImportProfile(const FString& SourceFilename, FString& OutProfileId)
{
    OutProfileId.Reset();
    FRotorlineFlightControllerProfile Profile;
    if (!LoadProfileFile(SourceFilename, Profile))
    {
        return false;
    }

    FString BaseId = MakeSafeId(Profile.ProfileId);
    if (BaseId.IsEmpty())
    {
        BaseId = TEXT("imported-controller-profile");
    }
    FString UniqueId = BaseId;
    for (int32 Suffix = 2; FindProfile(UniqueId) != nullptr; ++Suffix)
    {
        UniqueId = FString::Printf(TEXT("%s-%d"), *BaseId, Suffix);
    }
    Profile.ProfileId = UniqueId;
    if (!SaveProfile(Profile))
    {
        return false;
    }
    OutProfileId = UniqueId;
    return true;
}

bool URotorlineFlightControllerSubsystem::ExportProfile(
    const FString& ProfileId,
    const FString& DestinationFilename) const
{
    const FRotorlineFlightControllerProfile* Profile = FindProfile(ProfileId);
    return Profile && WriteProfileFile(*Profile, DestinationFilename);
}

void URotorlineFlightControllerSubsystem::ReloadProfiles()
{
    TArray<FRotorlineFlightControllerProfile> LoadedProfiles;
    const FString PersistentDirectory = GetProfileDirectory();
    const FString LegacyDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FlightControllerProfiles"));
    IFileManager::Get().MakeDirectory(*PersistentDirectory, true);

    auto LoadDirectory = [this, &LoadedProfiles, &PersistentDirectory](
        const FString& Directory, bool bMigrateToPersistent)
    {
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("*.json")), true, false);
        Files.Sort();
        for (const FString& File : Files)
        {
            FRotorlineFlightControllerProfile Profile;
            const FString FullPath = FPaths::Combine(Directory, File);
            bool bSchemaMigrated = false;
            if (!LoadProfileFile(FullPath, Profile, &bSchemaMigrated) ||
                LoadedProfiles.ContainsByPredicate([&Profile](const FRotorlineFlightControllerProfile& Existing)
                {
                    return Existing.ProfileId == Profile.ProfileId;
                }))
            {
                continue;
            }

            const FString PersistentPath = FPaths::Combine(PersistentDirectory, File);
            if (bMigrateToPersistent)
            {
                if (WriteProfileFile(Profile, PersistentPath))
                {
                    UE_LOG(LogRotorlineFlightController, Display,
                        TEXT("ROTORLINE_CONTROLLER_PROFILE|LEGACY_MIGRATED|source=%s|destination=%s"),
                        *FullPath, *PersistentPath);
                }
                else
                {
                    UE_LOG(LogRotorlineFlightController, Warning,
                        TEXT("ROTORLINE_CONTROLLER_PROFILE|LEGACY_MIGRATION_FAILED|source=%s"), *FullPath);
                }
            }
            else if (bSchemaMigrated && !WriteProfileFile(Profile, FullPath))
            {
                UE_LOG(LogRotorlineFlightController, Warning,
                    TEXT("ROTORLINE_CONTROLLER_PROFILE|MIGRATION_NOT_PERSISTED|file=%s"), *FullPath);
            }
            LoadedProfiles.Add(MoveTemp(Profile));
        }
    };

    LoadDirectory(PersistentDirectory, false);
    if (!FPaths::IsSamePath(PersistentDirectory, LegacyDirectory))
    {
        LoadDirectory(LegacyDirectory, true);
    }
    Profiles = MoveTemp(LoadedProfiles);
}

bool URotorlineFlightControllerSubsystem::GetAxisValue(FName Action, float& OutValue) const
{
    OutValue = 0.0f;
    const FRotorlineFlightControllerProfile* Profile = FindProfile(ActiveProfileId);
    const FNativeState* State = NativeStates.Find(ActiveDeviceId);
    if (!Profile || !State || !State->bConnected)
    {
        return false;
    }

    for (const FRotorlineAxisBinding& Binding : Profile->AxisBindings)
    {
        if (!Binding.bIgnore && Binding.Action == Action && State->RawAxes.IsValidIndex(Binding.NativeAxisIndex))
        {
            OutValue = FilterAxisValue(State->RawAxes[Binding.NativeAxisIndex], Binding);
            return true;
        }
    }
    return false;
}

bool URotorlineFlightControllerSubsystem::IsActionPressed(FName Action) const
{
    const FRotorlineFlightControllerProfile* Profile = FindProfile(ActiveProfileId);
    const FNativeState* State = NativeStates.Find(ActiveDeviceId);
    if (!Profile || !State || !State->bConnected)
    {
        return false;
    }

    // Analog triggers may be assigned to digital semantic actions by the
    // binding wizard. Flight axes themselves are queried through GetAxisValue;
    // any other mapped axis becomes pressed only after crossing its tuned
    // threshold, so trigger noise cannot produce phantom fire events.
    for (const FRotorlineAxisBinding& Binding : Profile->AxisBindings)
    {
        if (Binding.bIgnore || Binding.Action != Action ||
            !State->RawAxes.IsValidIndex(Binding.NativeAxisIndex))
        {
            continue;
        }
        const bool bFlightAxis = Action == RotorlineFlightControllerActions::Roll ||
            Action == RotorlineFlightControllerActions::Pitch ||
            Action == RotorlineFlightControllerActions::Yaw ||
            Action == RotorlineFlightControllerActions::Collective ||
            Action == RotorlineFlightControllerActions::Throttle ||
            Action == RotorlineFlightControllerActions::LookX ||
            Action == RotorlineFlightControllerActions::LookY;
        const bool bDigitalTrigger = Binding.UserLabel.Contains(TEXT("DIGITAL TRIGGER"));
        const float TriggerRange = FMath::Max(
            Binding.Calibration.RawMaximum - Binding.Calibration.RawMinimum,
            KINDA_SMALL_NUMBER);
        const float TriggerTravel = FMath::Abs(
            State->RawAxes[Binding.NativeAxisIndex] - Binding.Calibration.RawCenter) / TriggerRange;
        if (!bFlightAxis && (bDigitalTrigger
            ? TriggerTravel >= 0.60f
            : FMath::Abs(FilterAxisValue(State->RawAxes[Binding.NativeAxisIndex], Binding)) >= 0.60f))
        {
            return true;
        }
    }

    for (const FRotorlineButtonBinding& Binding : Profile->ButtonBindings)
    {
        if (Binding.Action == Action && State->Buttons.IsValidIndex(Binding.NativeButtonIndex) &&
            State->Buttons[Binding.NativeButtonIndex])
        {
            return true;
        }
    }

    for (const FRotorlineHatBinding& Binding : Profile->HatBindings)
    {
        if (!State->HatAngles.IsValidIndex(Binding.NativeHatIndex))
        {
            continue;
        }
        const float Angle = State->HatAngles[Binding.NativeHatIndex];
        if (Angle < 0.0f)
        {
            continue;
        }
        if (Binding.UpAction == Action && AngularDistance(Angle, 0.0f) <= 45.0f) return true;
        if (Binding.RightAction == Action && AngularDistance(Angle, 90.0f) <= 45.0f) return true;
        if (Binding.DownAction == Action && AngularDistance(Angle, 180.0f) <= 45.0f) return true;
        if (Binding.LeftAction == Action && AngularDistance(Angle, 270.0f) <= 45.0f) return true;
    }
    return false;
}

bool URotorlineFlightControllerSubsystem::GetHatAngle(int32 HatIndex, float& OutDegrees) const
{
    OutDegrees = -1.0f;
    const FNativeState* State = NativeStates.Find(ActiveDeviceId);
    if (!State || !State->bConnected || !State->HatAngles.IsValidIndex(HatIndex))
    {
        return false;
    }
    OutDegrees = State->HatAngles[HatIndex];
    return true;
}

bool URotorlineFlightControllerSubsystem::GetRawAxisValue(
    const FString& DeviceId,
    int32 AxisIndex,
    float& OutRawValue) const
{
    OutRawValue = 0.0f;
    const FNativeState* State = NativeStates.Find(DeviceId);
    if (!State || !State->bConnected || !State->RawAxes.IsValidIndex(AxisIndex))
    {
        return false;
    }
    OutRawValue = State->RawAxes[AxisIndex];
    return true;
}

bool URotorlineFlightControllerSubsystem::IsRawButtonPressed(
    const FString& DeviceId,
    int32 ButtonIndex) const
{
    const FNativeState* State = NativeStates.Find(DeviceId);
    return State && State->bConnected && State->Buttons.IsValidIndex(ButtonIndex) &&
        State->Buttons[ButtonIndex];
}

bool URotorlineFlightControllerSubsystem::GetRawHatAngle(
    const FString& DeviceId,
    int32 HatIndex,
    float& OutDegrees) const
{
    OutDegrees = -1.0f;
    const FNativeState* State = NativeStates.Find(DeviceId);
    if (!State || !State->bConnected || !State->HatAngles.IsValidIndex(HatIndex))
    {
        return false;
    }
    OutDegrees = State->HatAngles[HatIndex];
    return true;
}

bool URotorlineFlightControllerSubsystem::GetActiveProfile(
    FRotorlineFlightControllerProfile& OutProfile) const
{
    if (const FRotorlineFlightControllerProfile* Profile = FindProfile(ActiveProfileId))
    {
        OutProfile = *Profile;
        return true;
    }
    OutProfile = FRotorlineFlightControllerProfile();
    return false;
}

bool URotorlineFlightControllerSubsystem::BeginCalibration(const FString& DeviceId)
{
    const FRotorlineControllerDeviceInfo* Device = FindDevice(DeviceId);
    const FNativeState* State = NativeStates.Find(DeviceId);
    if (!Device || !State || !State->bConnected || State->RawAxes.Num() != Device->Capabilities.AxisCount)
    {
        return false;
    }

    bCalibrationActive = true;
    CalibrationDeviceId = DeviceId;
    PendingCalibration.SetNum(State->RawAxes.Num());
    for (int32 AxisIndex = 0; AxisIndex < State->RawAxes.Num(); ++AxisIndex)
    {
        PendingCalibration[AxisIndex].RawMinimum = State->RawAxes[AxisIndex];
        PendingCalibration[AxisIndex].RawCenter = State->RawAxes[AxisIndex];
        PendingCalibration[AxisIndex].RawMaximum = State->RawAxes[AxisIndex];
    }
    return true;
}

bool URotorlineFlightControllerSubsystem::CaptureCalibrationCenter()
{
    const FNativeState* State = NativeStates.Find(CalibrationDeviceId);
    if (!bCalibrationActive || !State || !State->bConnected ||
        State->RawAxes.Num() != PendingCalibration.Num())
    {
        return false;
    }
    for (int32 AxisIndex = 0; AxisIndex < State->RawAxes.Num(); ++AxisIndex)
    {
        PendingCalibration[AxisIndex].RawCenter = State->RawAxes[AxisIndex];
    }
    return true;
}

bool URotorlineFlightControllerSubsystem::FinishCalibration(bool bApplyToActiveProfile)
{
    if (!bCalibrationActive)
    {
        return false;
    }

    bool bResult = true;
    if (bApplyToActiveProfile)
    {
        const int32 ProfileIndex = Profiles.IndexOfByPredicate([this](const FRotorlineFlightControllerProfile& Profile)
        {
            return Profile.ProfileId == ActiveProfileId;
        });
        if (ProfileIndex == INDEX_NONE || CalibrationDeviceId != ActiveDeviceId)
        {
            bResult = false;
        }
        else
        {
            FRotorlineFlightControllerProfile UpdatedProfile = Profiles[ProfileIndex];
            for (FRotorlineAxisBinding& Binding : UpdatedProfile.AxisBindings)
            {
                if (PendingCalibration.IsValidIndex(Binding.NativeAxisIndex))
                {
                    Binding.Calibration = PendingCalibration[Binding.NativeAxisIndex];
                }
            }
            bResult = SaveProfile(UpdatedProfile);
        }
    }
    CancelCalibration();
    return bResult;
}

void URotorlineFlightControllerSubsystem::CancelCalibration()
{
    bCalibrationActive = false;
    CalibrationDeviceId.Reset();
    PendingCalibration.Reset();
}

float URotorlineFlightControllerSubsystem::FilterAxisValue(
    float RawValue,
    const FRotorlineAxisBinding& Binding)
{
    const float Minimum = Binding.Calibration.RawMinimum;
    const float HalfRange = FMath::Max(
        (Binding.Calibration.RawMaximum - Binding.Calibration.RawMinimum) * 0.5f,
        KINDA_SMALL_NUMBER);
    const float Center = Binding.Calibration.RawCenter +
        FMath::Clamp(Binding.CenterOffset, -0.25f, 0.25f) * HalfRange;
    const float Maximum = Binding.Calibration.RawMaximum;
    const float Deadzone = FMath::Clamp(Binding.Deadzone, 0.0f, 0.95f);
    float Normalized = 0.0f;

    if (Binding.bCentered)
    {
        if (RawValue >= Center)
        {
            Normalized = (RawValue - Center) / FMath::Max(Maximum - Center, KINDA_SMALL_NUMBER);
        }
        else
        {
            Normalized = (RawValue - Center) / FMath::Max(Center - Minimum, KINDA_SMALL_NUMBER);
        }
        Normalized = FMath::Clamp(Normalized, -1.0f, 1.0f);
        const float Magnitude = FMath::Abs(Normalized);
        if (Magnitude <= Deadzone)
        {
            Normalized = 0.0f;
        }
        else
        {
            Normalized = FMath::Sign(Normalized) * ((Magnitude - Deadzone) / (1.0f - Deadzone));
        }
    }
    else
    {
        Normalized = FMath::Clamp(
            (RawValue - Minimum) / FMath::Max(Maximum - Minimum, KINDA_SMALL_NUMBER),
            0.0f,
            1.0f);
        if (Binding.bInvert)
        {
            Normalized = 1.0f - Normalized;
        }
        Normalized = Normalized <= Deadzone ? 0.0f : (Normalized - Deadzone) / (1.0f - Deadzone);
    }

    const float Curved = FMath::Sign(Normalized) *
        FMath::Pow(FMath::Abs(Normalized), FMath::Clamp(Binding.CurveExponent, 0.1f, 4.0f));
    const float Direction = Binding.bCentered && Binding.bInvert ? -1.0f : 1.0f;
    return FMath::Clamp(
        Curved * FMath::Clamp(Binding.Sensitivity, 0.1f, 4.0f) * Binding.Scale * Direction,
        Binding.bCentered ? -1.0f : 0.0f,
        1.0f);
}

const FRotorlineControllerDeviceInfo* URotorlineFlightControllerSubsystem::FindDevice(const FString& DeviceId) const
{
    return Devices.FindByPredicate([&DeviceId](const FRotorlineControllerDeviceInfo& Device)
    {
        return Device.DeviceId == DeviceId;
    });
}

const FRotorlineFlightControllerProfile* URotorlineFlightControllerSubsystem::FindProfile(const FString& ProfileId) const
{
    return Profiles.FindByPredicate([&ProfileId](const FRotorlineFlightControllerProfile& Profile)
    {
        return Profile.ProfileId == ProfileId;
    });
}

const FRotorlineFlightControllerProfile* URotorlineFlightControllerSubsystem::FindCompatibleProfile(
    const FRotorlineControllerDeviceInfo& Device) const
{
    if (const FRotorlineFlightControllerProfile* Exact = Profiles.FindByPredicate(
        [&Device](const FRotorlineFlightControllerProfile& Profile)
        {
            return Profile.DeviceId == Device.DeviceId &&
                Profile.ExpectedAxisCount <= Device.Capabilities.AxisCount &&
                Profile.ExpectedButtonCount <= Device.Capabilities.ButtonCount &&
                Profile.ExpectedHatCount <= Device.Capabilities.HatCount;
        }))
    {
        return Exact;
    }

    return Profiles.FindByPredicate([&Device](const FRotorlineFlightControllerProfile& Profile)
    {
        const bool bIdentityMatches =
            ((Profile.VendorId != 0 && Profile.VendorId == Device.VendorId &&
              Profile.ProductId != 0 && Profile.ProductId == Device.ProductId) ||
             (!Profile.DeviceName.IsEmpty() && Profile.DeviceName.Equals(Device.DisplayName, ESearchCase::IgnoreCase)));
        return bIdentityMatches &&
            Profile.ExpectedAxisCount <= Device.Capabilities.AxisCount &&
            Profile.ExpectedButtonCount <= Device.Capabilities.ButtonCount &&
            Profile.ExpectedHatCount <= Device.Capabilities.HatCount;
    });
}

bool URotorlineFlightControllerSubsystem::LoadProfileFile(
    const FString& Filename,
    FRotorlineFlightControllerProfile& OutProfile,
    bool* bOutMigrated) const
{
    if (bOutMigrated)
    {
        *bOutMigrated = false;
    }
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Filename) || Text.Len() > 1024 * 1024)
    {
        UE_LOG(LogRotorlineFlightController, Warning,
            TEXT("ROTORLINE_CONTROLLER_PROFILE|IGNORED|file=%s|reason=unreadable_or_oversize"), *Filename);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogRotorlineFlightController, Warning,
            TEXT("ROTORLINE_CONTROLLER_PROFILE|IGNORED|file=%s|reason=invalid_json"), *Filename);
        return false;
    }

    FRotorlineFlightControllerProfile Candidate;
    double Number = 0.0;
    if (!TryNumber(Root, TEXT("schemaVersion"), Number) ||
        !Root->TryGetStringField(TEXT("profileId"), Candidate.ProfileId) ||
        !Root->TryGetStringField(TEXT("profileName"), Candidate.ProfileName) ||
        !Root->TryGetStringField(TEXT("deviceId"), Candidate.DeviceId) ||
        !Root->TryGetStringField(TEXT("deviceName"), Candidate.DeviceName))
    {
        return false;
    }
    Candidate.SchemaVersion = FMath::RoundToInt(Number);
    if (TryNumber(Root, TEXT("vendorId"), Number)) Candidate.VendorId = FMath::RoundToInt(Number);
    if (TryNumber(Root, TEXT("productId"), Number)) Candidate.ProductId = FMath::RoundToInt(Number);
    if (TryNumber(Root, TEXT("expectedAxisCount"), Number)) Candidate.ExpectedAxisCount = FMath::RoundToInt(Number);
    if (TryNumber(Root, TEXT("expectedButtonCount"), Number)) Candidate.ExpectedButtonCount = FMath::RoundToInt(Number);
    if (TryNumber(Root, TEXT("expectedHatCount"), Number)) Candidate.ExpectedHatCount = FMath::RoundToInt(Number);
    Root->TryGetBoolField(TEXT("allowDuplicateAxes"), Candidate.bAllowDuplicateAxisBindings);
    Root->TryGetBoolField(TEXT("allowDuplicateButtons"), Candidate.bAllowDuplicateButtonBindings);
    Root->TryGetBoolField(TEXT("allowDuplicateHats"), Candidate.bAllowDuplicateHatBindings);
    const TSharedPtr<FJsonObject>* CapabilitiesObject = nullptr;
    if (Root->TryGetObjectField(TEXT("capabilities"), CapabilitiesObject) && CapabilitiesObject && CapabilitiesObject->IsValid())
    {
        if (TryNumber(*CapabilitiesObject, TEXT("axisCount"), Number)) Candidate.DetectedCapabilities.AxisCount = FMath::RoundToInt(Number);
        if (TryNumber(*CapabilitiesObject, TEXT("buttonCount"), Number)) Candidate.DetectedCapabilities.ButtonCount = FMath::RoundToInt(Number);
        if (TryNumber(*CapabilitiesObject, TEXT("hatCount"), Number)) Candidate.DetectedCapabilities.HatCount = FMath::RoundToInt(Number);
        const TArray<TSharedPtr<FJsonValue>>* CapabilityAxes = nullptr;
        if ((*CapabilitiesObject)->TryGetArrayField(TEXT("axes"), CapabilityAxes))
        {
            for (const TSharedPtr<FJsonValue>& AxisValue : *CapabilityAxes)
            {
                const TSharedPtr<FJsonObject> AxisObject = AxisValue.IsValid() ? AxisValue->AsObject() : nullptr;
                FRotorlineControllerAxisCapability Axis;
                FString AxisName;
                if (!AxisObject.IsValid() || !AxisObject->TryGetStringField(TEXT("name"), AxisName) ||
                    !TryNumber(AxisObject, TEXT("index"), Number))
                {
                    return false;
                }
                Axis.NativeName = FName(*AxisName);
                Axis.NativeIndex = FMath::RoundToInt(Number);
                if (TryNumber(AxisObject, TEXT("minimum"), Number)) Axis.RawMinimum = Number;
                if (TryNumber(AxisObject, TEXT("maximum"), Number)) Axis.RawMaximum = Number;
                Candidate.DetectedCapabilities.Axes.Add(Axis);
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* AxisArray = nullptr;
    if (Root->TryGetArrayField(TEXT("axes"), AxisArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *AxisArray)
        {
            const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
            FRotorlineAxisBinding Binding;
            if (!ReadAction(Object, TEXT("action"), Binding.Action) ||
                !TryNumber(Object, TEXT("axis"), Number))
            {
                return false;
            }
            Binding.NativeAxisIndex = FMath::RoundToInt(Number);
            Object->TryGetBoolField(TEXT("centered"), Binding.bCentered);
            Object->TryGetBoolField(TEXT("invert"), Binding.bInvert);
            if (TryNumber(Object, TEXT("deadzone"), Number)) Binding.Deadzone = Number;
            if (TryNumber(Object, TEXT("sensitivity"), Number)) Binding.Sensitivity = Number;
            if (TryNumber(Object, TEXT("curve"), Number)) Binding.CurveExponent = Number;
            if (TryNumber(Object, TEXT("scale"), Number)) Binding.Scale = Number;
            if (TryNumber(Object, TEXT("minimum"), Number)) Binding.Calibration.RawMinimum = Number;
            if (TryNumber(Object, TEXT("center"), Number)) Binding.Calibration.RawCenter = Number;
            if (TryNumber(Object, TEXT("maximum"), Number)) Binding.Calibration.RawMaximum = Number;
            if (TryNumber(Object, TEXT("noiseFloor"), Number)) Binding.Calibration.NoiseFloor = Number;
            Object->TryGetStringField(TEXT("userLabel"), Binding.UserLabel);
            Object->TryGetBoolField(TEXT("ignore"), Binding.bIgnore);
            if (TryNumber(Object, TEXT("centerOffset"), Number)) Binding.CenterOffset = Number;
            Candidate.AxisBindings.Add(Binding);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* ButtonArray = nullptr;
    if (Root->TryGetArrayField(TEXT("buttons"), ButtonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *ButtonArray)
        {
            const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
            FRotorlineButtonBinding Binding;
            if (!ReadAction(Object, TEXT("action"), Binding.Action) ||
                !TryNumber(Object, TEXT("button"), Number))
            {
                return false;
            }
            Binding.NativeButtonIndex = FMath::RoundToInt(Number);
            Candidate.ButtonBindings.Add(Binding);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* HatArray = nullptr;
    if (Root->TryGetArrayField(TEXT("hats"), HatArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *HatArray)
        {
            const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
            FRotorlineHatBinding Binding;
            if (!TryNumber(Object, TEXT("hat"), Number) ||
                !ReadAction(Object, TEXT("up"), Binding.UpAction) ||
                !ReadAction(Object, TEXT("right"), Binding.RightAction) ||
                !ReadAction(Object, TEXT("down"), Binding.DownAction) ||
                !ReadAction(Object, TEXT("left"), Binding.LeftAction))
            {
                return false;
            }
            Binding.NativeHatIndex = FMath::RoundToInt(Number);
            Candidate.HatBindings.Add(Binding);
        }
    }

    const bool bMigrated = MigrateProfileToCurrentSchema(Candidate);
    if (bOutMigrated)
    {
        *bOutMigrated = bMigrated;
    }

    FString Reason;
    if (!ValidateProfile(Candidate, Reason))
    {
        UE_LOG(LogRotorlineFlightController, Warning,
            TEXT("ROTORLINE_CONTROLLER_PROFILE|IGNORED|file=%s|reason=%s"), *Filename, *Reason);
        return false;
    }
    OutProfile = MoveTemp(Candidate);
    return true;
}

bool URotorlineFlightControllerSubsystem::WriteProfileFile(
    const FRotorlineFlightControllerProfile& Profile,
    const FString& Filename) const
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("schemaVersion"), Profile.SchemaVersion);
    Root->SetStringField(TEXT("profileId"), Profile.ProfileId);
    Root->SetStringField(TEXT("profileName"), Profile.ProfileName);
    Root->SetStringField(TEXT("deviceId"), Profile.DeviceId);
    Root->SetStringField(TEXT("deviceName"), Profile.DeviceName);
    Root->SetNumberField(TEXT("vendorId"), Profile.VendorId);
    Root->SetNumberField(TEXT("productId"), Profile.ProductId);
    Root->SetNumberField(TEXT("expectedAxisCount"), Profile.ExpectedAxisCount);
    Root->SetNumberField(TEXT("expectedButtonCount"), Profile.ExpectedButtonCount);
    Root->SetNumberField(TEXT("expectedHatCount"), Profile.ExpectedHatCount);
    Root->SetBoolField(TEXT("allowDuplicateAxes"), Profile.bAllowDuplicateAxisBindings);
    Root->SetBoolField(TEXT("allowDuplicateButtons"), Profile.bAllowDuplicateButtonBindings);
    Root->SetBoolField(TEXT("allowDuplicateHats"), Profile.bAllowDuplicateHatBindings);
    TSharedPtr<FJsonObject> Capabilities = MakeShared<FJsonObject>();
    Capabilities->SetNumberField(TEXT("axisCount"), Profile.DetectedCapabilities.AxisCount);
    Capabilities->SetNumberField(TEXT("buttonCount"), Profile.DetectedCapabilities.ButtonCount);
    Capabilities->SetNumberField(TEXT("hatCount"), Profile.DetectedCapabilities.HatCount);
    TArray<TSharedPtr<FJsonValue>> CapabilityAxes;
    for (const FRotorlineControllerAxisCapability& Axis : Profile.DetectedCapabilities.Axes)
    {
        TSharedPtr<FJsonObject> AxisObject = MakeShared<FJsonObject>();
        AxisObject->SetStringField(TEXT("name"), Axis.NativeName.ToString());
        AxisObject->SetNumberField(TEXT("index"), Axis.NativeIndex);
        AxisObject->SetNumberField(TEXT("minimum"), Axis.RawMinimum);
        AxisObject->SetNumberField(TEXT("maximum"), Axis.RawMaximum);
        CapabilityAxes.Add(MakeShared<FJsonValueObject>(AxisObject));
    }
    Capabilities->SetArrayField(TEXT("axes"), CapabilityAxes);
    Root->SetObjectField(TEXT("capabilities"), Capabilities);

    TArray<TSharedPtr<FJsonValue>> Axes;
    for (const FRotorlineAxisBinding& Binding : Profile.AxisBindings)
    {
        Axes.Add(MakeShared<FJsonValueObject>(AxisBindingToJson(Binding)));
    }
    Root->SetArrayField(TEXT("axes"), Axes);

    TArray<TSharedPtr<FJsonValue>> Buttons;
    for (const FRotorlineButtonBinding& Binding : Profile.ButtonBindings)
    {
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("action"), Binding.Action.ToString());
        Object->SetNumberField(TEXT("button"), Binding.NativeButtonIndex);
        Buttons.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("buttons"), Buttons);

    TArray<TSharedPtr<FJsonValue>> Hats;
    for (const FRotorlineHatBinding& Binding : Profile.HatBindings)
    {
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("hat"), Binding.NativeHatIndex);
        Object->SetStringField(TEXT("up"), Binding.UpAction.ToString());
        Object->SetStringField(TEXT("right"), Binding.RightAction.ToString());
        Object->SetStringField(TEXT("down"), Binding.DownAction.ToString());
        Object->SetStringField(TEXT("left"), Binding.LeftAction.ToString());
        Hats.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("hats"), Hats);

    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        return false;
    }

    const FString Directory = FPaths::GetPath(Filename);
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString TemporaryFilename = Filename + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(Text, *TemporaryFilename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        return false;
    }
    if (!IFileManager::Get().Move(*Filename, *TemporaryFilename, true, true))
    {
        IFileManager::Get().Delete(*TemporaryFilename, false, true);
        return false;
    }
    return true;
}

bool URotorlineFlightControllerSubsystem::ValidateProfile(
    FRotorlineFlightControllerProfile& Profile,
    FString& OutReason) const
{
    Profile.ProfileId = MakeSafeId(Profile.ProfileId);
    Profile.ProfileName.TrimStartAndEndInline();
    if (Profile.SchemaVersion != CurrentProfileSchemaVersion)
    {
        OutReason = TEXT("unsupported_schema");
        return false;
    }
    if (Profile.ProfileId.IsEmpty() || Profile.ProfileName.IsEmpty())
    {
        OutReason = TEXT("missing_identity");
        return false;
    }
    if (Profile.AxisBindings.Num() > 256 || Profile.ButtonBindings.Num() > 256 || Profile.HatBindings.Num() > 32)
    {
        OutReason = TEXT("binding_limit_exceeded");
        return false;
    }

    Profile.ExpectedAxisCount = FMath::Clamp(Profile.ExpectedAxisCount, 0, 256);
    Profile.ExpectedButtonCount = FMath::Clamp(Profile.ExpectedButtonCount, 0, 256);
    Profile.ExpectedHatCount = FMath::Clamp(Profile.ExpectedHatCount, 0, 32);
    Profile.DetectedCapabilities.AxisCount = FMath::Clamp(
        Profile.DetectedCapabilities.AxisCount > 0
            ? Profile.DetectedCapabilities.AxisCount
            : Profile.ExpectedAxisCount,
        0,
        256);
    Profile.DetectedCapabilities.ButtonCount = FMath::Clamp(
        Profile.DetectedCapabilities.ButtonCount > 0
            ? Profile.DetectedCapabilities.ButtonCount
            : Profile.ExpectedButtonCount,
        0,
        256);
    Profile.DetectedCapabilities.HatCount = FMath::Clamp(
        Profile.DetectedCapabilities.HatCount > 0
            ? Profile.DetectedCapabilities.HatCount
            : Profile.ExpectedHatCount,
        0,
        32);
    // Expected counts are the compatibility contract checked against the live
    // device by ApplyProfile. The descriptive capability object is imported
    // JSON too, so it must never be allowed to widen these trusted bounds.
    const int32 AxisCapabilityLimit = Profile.ExpectedAxisCount;
    const int32 ButtonCapabilityLimit = Profile.ExpectedButtonCount;
    const int32 HatCapabilityLimit = Profile.ExpectedHatCount;
    TSet<int32> UsedAxes;
    for (FRotorlineAxisBinding& Binding : Profile.AxisBindings)
    {
        if (!RotorlineFlightControllerActions::IsKnown(Binding.Action) ||
            (!Binding.bIgnore && Binding.Action.IsNone()) ||
            Binding.NativeAxisIndex < 0 || Binding.NativeAxisIndex >= AxisCapabilityLimit ||
            !FMath::IsFinite(Binding.Calibration.RawMinimum) ||
            !FMath::IsFinite(Binding.Calibration.RawCenter) ||
            !FMath::IsFinite(Binding.Calibration.RawMaximum) ||
            Binding.Calibration.RawMinimum >= Binding.Calibration.RawMaximum ||
            (Binding.bCentered &&
                (Binding.Calibration.RawMinimum >= Binding.Calibration.RawCenter ||
                 Binding.Calibration.RawCenter >= Binding.Calibration.RawMaximum)))
        {
            OutReason = TEXT("invalid_axis_binding");
            return false;
        }
        if (!Binding.bIgnore && UsedAxes.Contains(Binding.NativeAxisIndex) &&
            !Profile.bAllowDuplicateAxisBindings)
        {
            OutReason = TEXT("duplicate_axis_requires_confirmation");
            return false;
        }
        if (!Binding.bIgnore)
        {
            UsedAxes.Add(Binding.NativeAxisIndex);
        }
        Binding.Deadzone = FMath::Clamp(Binding.Deadzone, 0.0f, 0.95f);
        Binding.Sensitivity = FMath::Clamp(Binding.Sensitivity, 0.1f, 4.0f);
        Binding.CurveExponent = FMath::Clamp(Binding.CurveExponent, 0.1f, 4.0f);
        Binding.Scale = FMath::Clamp(Binding.Scale, -4.0f, 4.0f);
        Binding.CenterOffset = FMath::Clamp(Binding.CenterOffset, -0.25f, 0.25f);
        Binding.Calibration.NoiseFloor = FMath::Max(0.0f, Binding.Calibration.NoiseFloor);
    }
    TSet<int32> UsedButtons;
    for (const FRotorlineButtonBinding& Binding : Profile.ButtonBindings)
    {
        if (!RotorlineFlightControllerActions::IsKnown(Binding.Action) || Binding.Action.IsNone() ||
            Binding.NativeButtonIndex < 0 || Binding.NativeButtonIndex >= ButtonCapabilityLimit)
        {
            OutReason = TEXT("invalid_button_binding");
            return false;
        }
        if (UsedButtons.Contains(Binding.NativeButtonIndex) &&
            !Profile.bAllowDuplicateButtonBindings)
        {
            OutReason = TEXT("duplicate_button_requires_confirmation");
            return false;
        }
        UsedButtons.Add(Binding.NativeButtonIndex);
    }
    TSet<uint64> UsedHatDirections;
    for (const FRotorlineHatBinding& Binding : Profile.HatBindings)
    {
        if (Binding.NativeHatIndex < 0 || Binding.NativeHatIndex >= HatCapabilityLimit ||
            !RotorlineFlightControllerActions::IsKnown(Binding.UpAction) ||
            !RotorlineFlightControllerActions::IsKnown(Binding.RightAction) ||
            !RotorlineFlightControllerActions::IsKnown(Binding.DownAction) ||
            !RotorlineFlightControllerActions::IsKnown(Binding.LeftAction))
        {
            OutReason = TEXT("invalid_hat_binding");
            return false;
        }
        const FName HatActions[] = { Binding.UpAction, Binding.RightAction, Binding.DownAction, Binding.LeftAction };
        for (int32 Direction = 0; Direction < 4; ++Direction)
        {
            if (HatActions[Direction].IsNone()) continue;
            const uint64 Key = (static_cast<uint64>(static_cast<uint32>(Binding.NativeHatIndex)) << 32) |
                static_cast<uint32>(Direction);
            if (UsedHatDirections.Contains(Key) && !Profile.bAllowDuplicateHatBindings)
            {
                OutReason = TEXT("duplicate_hat_requires_confirmation");
                return false;
            }
            UsedHatDirections.Add(Key);
        }
    }
    return true;
}

FString URotorlineFlightControllerSubsystem::GetProfileDirectory() const
{
    // Keep hardware mappings outside the packaged executable tree so replacing
    // a build cannot erase a player's calibrated flight controls.
    return FPaths::Combine(
        FPlatformProcess::UserSettingsDir(),
        TEXT("Rotorline"),
        TEXT("FlightControllerProfiles"));
}

FString URotorlineFlightControllerSubsystem::GetProfileFilename(const FString& ProfileId) const
{
    return FPaths::Combine(GetProfileDirectory(), MakeSafeId(ProfileId) + TEXT(".json"));
}

void URotorlineFlightControllerSubsystem::ClearActiveStateAfterDisconnect(const FString& DeviceId)
{
    NativeStates.Remove(DeviceId);
    if (CalibrationDeviceId == DeviceId)
    {
        CancelCalibration();
    }
    if (ActiveDeviceId == DeviceId)
    {
        ActiveDeviceId.Reset();
        ActiveProfileId.Reset();
    }
}

void URotorlineFlightControllerSubsystem::UpdateCalibrationCapture(const FNativeState& State)
{
    if (State.RawAxes.Num() != PendingCalibration.Num())
    {
        CancelCalibration();
        return;
    }
    for (int32 AxisIndex = 0; AxisIndex < State.RawAxes.Num(); ++AxisIndex)
    {
        PendingCalibration[AxisIndex].RawMinimum =
            FMath::Min(PendingCalibration[AxisIndex].RawMinimum, State.RawAxes[AxisIndex]);
        PendingCalibration[AxisIndex].RawMaximum =
            FMath::Max(PendingCalibration[AxisIndex].RawMaximum, State.RawAxes[AxisIndex]);
        const float CapturedRange = PendingCalibration[AxisIndex].RawMaximum -
            PendingCalibration[AxisIndex].RawMinimum;
        const float CenterDelta = FMath::Abs(
            State.RawAxes[AxisIndex] - PendingCalibration[AxisIndex].RawCenter);
        if (CapturedRange > KINDA_SMALL_NUMBER && CenterDelta <= CapturedRange * 0.08f)
        {
            PendingCalibration[AxisIndex].NoiseFloor =
                FMath::Max(PendingCalibration[AxisIndex].NoiseFloor, CenterDelta);
        }
    }
}
