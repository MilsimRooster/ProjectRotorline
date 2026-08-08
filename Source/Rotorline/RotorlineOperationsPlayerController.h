#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RotorlineAircraftCatalog.h"
#include "RotorlineAwards.h"
#include "RotorlineCastCatalog.h"
#include "RotorlineMissionCatalog.h"
#include "RotorlineFlightControllerSubsystem.h"
#include "RotorlineOperationsPlayerController.generated.h"

class URotorlineProfileSave;
class UAudioComponent;
class USoundBase;
class UTexture2D;
class UMediaPlayer;
class UMediaTexture;
class UMediaSoundComponent;
class UFileMediaSource;
class ARotorlineHangarPreviewActor;
class ARotorlineHelipadBeaconActor;
class ARotorlineBellLairActor;
class ARotorlineCaveTransitionActor;
class ARotorlineMissionObjectiveActor;

enum class ERotorlineAudioChannel : uint8
{
    Master,
    Environment,
    Engine,
    Music,
    Radio,
    WeaponsExplosions
};

enum class ERotorlineStartupState : uint8
{
    Inactive,
    Intro,
    StartScreen,
    CastGallery,
    PatchWall,
    Credits,
    EnteringOperations
};

enum class ERotorlineControlsMode : uint8
{
    Home,
    FirstTimePrompt,
    AxisCalibration,
    ButtonBinding,
    AxisTuning,
    LiveTest,
    DeviceSelect
};

UCLASS()
class ROTORLINE_API ARotorlineOperationsPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ARotorlineOperationsPlayerController();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;

    bool IsOperationsMenuOpen() const { return bOperationsMenuOpen; }
    bool IsHangarOpen() const { return bHangarOpen; }
    int32 GetSelectedMissionIndex() const { return SelectedMissionIndex; }
    ERotorlineCraftType GetSelectedCraft() const { return SelectedCraft; }
    const TArray<FRotorlineMissionDefinition>& GetMissions() const { return Missions; }
    const TArray<FRotorlineAircraftDefinition>& GetAircraft() const { return Aircraft; }
    int32 GetSelectedAircraftIndex() const { return SelectedAircraftIndex; }
    const FRotorlineAircraftDefinition* GetSelectedAircraft() const;
    int32 GetSelectedAircraftMissionFit() const;
    UTexture2D* GetAircraftBlueprintTexture(const FString& AircraftId) const;
    const FString& GetCatalogError() const { return CatalogError; }
    bool HasReceivedGamepadInput() const { return bGamepadInputSeen; }
    int32 GetReputation() const;
    bool IsMissionUnlocked(const FRotorlineMissionDefinition& Mission) const;
    bool IsMissionCompleted(const FString& MissionId) const;
    bool IsAircraftUnlocked(const FRotorlineAircraftDefinition& AircraftDefinition) const;
    bool IsBell222Unlocked() const;
    int32 GetCompletedCampaignMissionCount() const;
    int32 GetCampaignMissionCount() const { return Missions.Num(); }
    bool IsAudioSettingsOpen() const { return bAudioSettingsOpen; }
    bool IsGraphicsSettingsOpen() const { return bGraphicsSettingsOpen; }
    bool IsControlsSettingsOpen() const { return bControlsSettingsOpen; }
    bool IsFlightPauseMenuOpen() const { return bFlightPauseMenuOpen; }
    bool IsMissionFailureScreenOpen() const { return bMissionFailureScreenOpen; }
    int32 GetSelectedMissionFailureAction() const { return SelectedMissionFailureAction; }
    FString GetMissionFailureReason() const;
    bool IsAbortMissionPending() const { return bAbortMissionPending; }
    int32 GetSelectedPauseRow() const { return SelectedPauseRow; }
    int32 GetSelectedAudioRow() const { return SelectedAudioRow; }
    int32 GetSelectedGraphicsRow() const { return SelectedGraphicsRow; }
    int32 GetSelectedControlsTab() const { return SelectedControlsTab; }
    int32 GetSelectedControlsRow() const { return SelectedControlsRow; }
    int32 GetControlsLiveTestPage() const { return ControlsLiveTestPage; }
    ERotorlineControlsMode GetControlsMode() const { return ControlsMode; }
    int32 GetControlsWizardStep() const { return ControlsWizardStep; }
    int32 GetControlsDetectedAxis() const { return ControlsDetectedAxis; }
    bool IsGamepadPitchInverted() const;
    const FString& GetControlsStatus() const { return ControlsStatus; }
    const FString& GetControlsCaptureFeedback() const { return ControlsCaptureFeedback; }
    const FString& GetControlsDeviceId() const { return ControlsDeviceId; }
    const FRotorlineFlightControllerProfile& GetWorkingControllerProfile() const { return WorkingControllerProfile; }
    FString GetSimpleGraphicsModeLabel() const
    {
        return bTurboGraphicsMode
            ? TEXT("TURBO MODE // FAST PC // QUALITY + RAY TRACING")
            : TEXT("SNAIL MODE // OLDER PC // PERFORMANCE + NO RAY TRACING");
    }
    FName GetControlsWizardAction() const;
    FString GetControlsWizardCurrentBinding() const;
    bool GetFlightControllerAxis(FName Action, float& OutValue) const;
    bool IsFlightControllerActionPressed(FName Action) const;
    bool WasFlightControllerActionJustPressed(FName Action) const;
    bool HasActiveFlightController() const;
    bool IsFlightControllerInputSuppressed() const { return bControlsSettingsOpen || bGraphicsSettingsOpen; }
    bool IsTacticalMapVisible() const { return bTacticalMapVisible; }
    void ToggleTacticalMap() { bTacticalMapVisible = !bTacticalMapVisible; }
    FString GetFlightControllerNotification() const { return FlightControllerNotification; }
    bool ShouldShowFlightControllerNotification() const { return FlightControllerNotificationSeconds > 0.0f; }
    float GetAudioSetting(ERotorlineAudioChannel Channel) const;
    float GetEffectiveAudioVolume(ERotorlineAudioChannel Channel) const;
    bool IsMissionCompleteScreenOpen() const { return bMissionCompleteScreenOpen; }
    int32 GetSelectedMissionCompleteAction() const { return SelectedMissionCompleteAction; }
    const FRotorlineMissionResults& GetMissionResults() const { return MissionResults; }
    int32 GetMissionResetGeneration() const { return MissionResetGeneration; }
    bool IsAwardPresentationOpen() const { return bAwardPresentationOpen; }
    bool IsPatchWallOpen() const { return bPatchWallOpen; }
    bool IsStartupFlowVisible() const { return StartupState != ERotorlineStartupState::Inactive; }
    bool IsStartupIntroOpen() const { return StartupState == ERotorlineStartupState::Intro; }
    bool IsLoreIntroPlaying() const { return StartupState == ERotorlineStartupState::Intro && bPlayingLoreIntro; }
    bool IsStartupMenuOpen() const { return StartupState == ERotorlineStartupState::StartScreen; }
    bool IsCastGalleryOpen() const { return StartupState == ERotorlineStartupState::CastGallery; }
    bool IsStartupPatchWallOpen() const { return StartupState == ERotorlineStartupState::PatchWall; }
    bool IsCreditsOpen() const { return StartupState == ERotorlineStartupState::Credits; }
    bool IsM25FinalCreditsSequenceActive() const { return bM25FinalCreditsSequenceActive; }
    bool HasM25FinalCreditsSequenceCompleted() const { return bM25FinalCreditsSequenceCompleted; }
    bool HasM25FinalCreditsSequenceFailed() const { return bM25FinalCreditsSequenceFailed; }
    bool BeginM25FinalCreditsSequence();
    bool ShouldShowCreditsRoll() const;
    bool IsEnteringOperations() const { return StartupState == ERotorlineStartupState::EnteringOperations; }
    int32 GetSelectedStartupMenuIndex() const { return SelectedStartupMenuIndex; }
    int32 GetHoveredStartupMenuIndex() const { return HoveredStartupMenuIndex; }
    int32 GetPressedStartupMenuIndex() const { return PressedStartupMenuIndex; }
    float GetStartupFadeAlpha() const { return StartupFadeAlpha; }
    float GetStartupStateElapsed() const { return StartupStateElapsed; }
    bool IsStartupMediaReady() const { return bStartupMediaReady; }
    UMediaTexture* GetStartupMediaTexture() const { return StartupMediaTexture; }
    UTexture2D* GetStartupBackgroundTexture() const { return StartupBackgroundTexture; }
    UTexture2D* GetOperationsBoardBackgroundTexture() const { return OperationsBoardBackgroundTexture; }
    FString GetStartupMediaTimeLabel() const;
    float GetCreditsScrollProgress() const;
    const TArray<FRotorlineCastMember>& GetCastMembers() const { return CastMembers; }
    int32 GetSelectedCastIndex() const { return SelectedCastIndex; }
    UTexture2D* GetCastCardTexture(int32 Index) const;
    bool IsCastVoicePlaying() const;
    float GetCastVoiceProgress() const;
    float GetCastTransitionAlpha() const { return CastTransitionAlpha; }
    int32 GetCastTransitionDirection() const { return CastTransitionDirection; }
    const FString& GetCastCatalogError() const { return CastCatalogError; }
    int32 GetAwardPresentationIndex() const { return AwardPresentationIndex; }
    int32 GetPatchWallSelection() const { return PatchWallSelection; }
    const TArray<FRotorlineAwardEvaluation>& GetNewlyEarnedAwards() const { return NewlyEarnedAwards; }
    const TArray<FRotorlineAwardDefinition>& GetAwardDefinitions() const { return AwardSystem.GetDefinitions(); }
    const FRotorlineAwardDefinition* GetAwardDefinition(const FString& AwardId) const { return AwardSystem.FindDefinition(AwardId); }
    const FRotorlinePlayerAwardRecord* GetAwardRecord(const FString& AwardId) const;
    UTexture2D* GetAwardPatchTexture(const FString& AwardId) const;
    float GetAwardCompletionPercent() const;
    const FRotorlineCareerStatistics* GetCareerStatistics() const;
    const FString& GetAwardsCatalogError() const { return AwardsCatalogError; }

    UFUNCTION(exec) void RotorlineAwardsList();
    UFUNCTION(exec) void RotorlineAwardsProgress();
    UFUNCTION(exec) void RotorlineAwardsUnlock(const FString& AwardId);
    UFUNCTION(exec) void RotorlineAwardsReset();
    UFUNCTION(exec) void RotorlineAwardsSimulate(const FString& Scenario);
    UFUNCTION(exec) void RotorlineAwardsEvaluate();
    UFUNCTION(exec) void RotorlineAwardsMissingArtTest();

    void ReturnToOperations();
    void RecordMissionCompletion(const FRotorlineMissionDefinition& Mission, float ElapsedSeconds);
    void NotifyWeaponFired();
    void NotifyWeaponHit(bool bDestroyed, bool bAircraft);
    void NotifyCivilianRescued(int32 Count = 1);
    void NotifyCargoDelivered(int32 Count = 1, bool bSlingLoad = false);
    void NotifyDamageTaken(float Damage);
    void NotifyObjectiveCompleted(bool bOptional = false);
    void NotifyAircraftCondition(float CurrentHealth, float MaximumHealth);
    void NotifyMissileDodged(float ClosestDistanceMeters);
    void NotifyEnemyFire(float ExposureSeconds = 1.0f);
    void NotifyDetection(float ExposureSeconds = 1.0f);
    void ApplyMouseMode(bool bCaptureForMouseLook);
    void BeginCaveJeepTransition();

private:
    bool bHardwareRayTracingEnabled = false;
    bool bTurboGraphicsMode = false;
    void LogOceanDiagnosticRuntimeState(const TCHAR* Phase);
    void SetM25AircraftAudioSuppressed(bool bSuppressed);
    void UpdateOperationsInput();
    void ApplyRayTracingMode(bool bEnableHardwareRayTracing, bool bNotifyPlayer);
    void ApplySimpleGraphicsMode(bool bEnableTurboMode, bool bNotifyPlayer, bool bPersist);
    void ToggleSimpleGraphicsMode();
    void ToggleRayTracingMode();
    void UpdateHangarInput(
        bool bMoveLeft,
        bool bMoveRight);
    void UpdateFlightPauseInput();
    void UpdateMissionFailureInput();
    void UpdateMissionCompleteInput();
    void UpdateAwardPresentationInput();
    void UpdatePatchWallInput(
        bool bMoveUp,
        bool bMoveDown,
        bool bMoveLeft,
        bool bMoveRight);
    void MovePatchWallSelection(int32 HorizontalDirection, int32 VerticalDirection);
    void OpenMissionCompletePreview();
    void MoveMissionSelection(int32 Direction);
    void MoveCraftSelection(int32 Direction);
    void LaunchSelection();
    void OpenHangar();
    void CloseHangar();
    void MoveAircraftSelection(int32 Direction);
    void RefreshHangarPreview();
    void DeploySelectedAircraft();
    void DeploySelectedGroundVehicle(const FRotorlineAircraftDefinition& Definition);
    void CompleteCaveJeepTransition();
    void SetFlightPauseMenuOpen(bool bOpen);
    void MovePauseSelection(int32 Direction);
    void ActivatePauseSelection();
    void OpenMissionFailureScreen();
    void MoveMissionFailureSelection(int32 Direction);
    void ActivateMissionFailureSelection();
    void OpenMissionCompleteScreen(const FRotorlineMissionDefinition& Mission, float ElapsedSeconds);
    void MoveMissionCompleteSelection(int32 Direction);
    void ActivateMissionCompleteSelection();
    void ResetMissionResults(const TCHAR* Reason);
    void ReturnToHangarAfterMission();
    void ReturnToMainMenu();
    void RestartSelectedMission();
    void ClearMissionSpawnedActors();
    void RefreshActiveAudioMix();
    void RefreshEnvironmentAudioMix();
    void PulseController(float Intensity, float Duration);
    void LoadProfile();
    bool SaveProfile();
    void LoadAwardDefinitions();
    bool ApplyRunningOnFumesTelemetryRepair();
    void UpdateMissionTelemetry(float DeltaTime);
    void FinalizeMissionStatistics(bool bSuccess, const FRotorlineMissionDefinition& Mission);
    void ApplyMissionStatisticsToProfile(const FRotorlineMissionDefinition& Mission);
    void EvaluateMissionAwards();
    void BeginAwardPresentation();
    void AdvanceAwardPresentation();
    void TogglePatchWall();
    void ForceUnlockAward(const FString& AwardId, const FString& Reason);
    void RunAwardsSelfTest();
    void LoadCastDefinitions();
    void LoadAircraftBlueprintTextures();
    void MoveCastSelection(int32 Direction);
    void SelectCastMember(int32 Index, int32 DirectionHint = 0);
    int32 GetCastIndexAtPosition(float X, float Y) const;
    void QueueSelectedCastVoice(float DelaySeconds, const TCHAR* Reason);
    void PlaySelectedCastVoice(const TCHAR* Reason);
    void StopCastVoice(const TCHAR* Reason);
    void InitializeStartupFlow();
    void TickStartupFlow(float DeltaTime);
    bool HandleStartupInput(const FInputKeyEventArgs& Params);
    void EnterStartupState(ERotorlineStartupState NewState);
    void BeginStartupTransition(ERotorlineStartupState TargetState, const TCHAR* Reason);
    void OpenStartupMedia(bool bCredits);
    void CloseStartupMedia(const TCHAR* Reason);
    void UpdatePreGameMenuMusic();
    void StopPreGameMenuMusic(const TCHAR* Reason);
    void MoveStartupMenuSelection(int32 Direction, const TCHAR* Source);
    void ActivateStartupSelection();
    void ReturnToStartupMenu(const TCHAR* Reason);
    int32 GetStartupMenuIndexAtPosition(float X, float Y) const;
    void RunStartupQualificationTick();
    UFUNCTION() void HandleStartupMediaOpened(FString OpenedUrl);
    UFUNCTION() void HandleStartupMediaOpenFailed(FString FailedUrl);
    UFUNCTION() void HandleStartupMediaEndReached();
    void ToggleGraphicsSettings();
    void UpdateGraphicsSettingsInput();
    void MoveGraphicsSelection(int32 Direction);
    void ActivateGraphicsSelection();
    void ToggleAudioSettings();
    void MoveAudioSelection(int32 Direction);
    void AdjustAudioSetting(int32 Direction);
    void ResetAudioSettings();
    void ToggleControlsSettings(bool bFirstTimePrompt = false);
    void UpdateControlsInput();
    bool QueueControlsSettingsInput(const FInputKeyEventArgs& Params);
    void CaptureCompatibleGamepadControlsInput();
    void TickControlsCapture(float DeltaTime);
    void MoveControlsSelection(int32 Direction);
    void ActivateControlsSelection();
    void ToggleGamepadPitchInvert();
    void BeginControlsCalibration();
    void AcceptCalibratedAxis();
    void BeginControlsButtonBinding();
    void AdvanceControlsButtonWizard();
    void ClearWorkingControllerActionBindings(FName Action);
    void AssignCapturedButtonBinding(int32 NativeButton, FName Action, bool bKeepDuplicate);
    void AssignCapturedHatBinding(int32 NativeHat, int32 Direction, FName Action, bool bKeepDuplicate);
    void AssignCapturedTriggerBinding(int32 NativeAxis, FName Action, bool bKeepDuplicate);
    bool HasPendingControlsDuplicate() const;
    bool CancelPendingControlsDuplicate();
    void ClearPendingControlsDuplicate();
    void ApplyWorkingControllerProfile();
    bool SaveWorkingControllerProfile();
    void CancelWorkingControllerChanges();
    void SelectControlsDevice(int32 FlightDeviceIndex);
    void CaptureControlsSnapshot();
    void ResetCurrentDeviceProfile();
    void ResetWorkingControllerProfile();
    void RefreshControllerSemanticState();
    void CheckFlightControllerConnection();
    void RunFlightControllerQualification();
    void RunFlightControllerRestartQualification(const FString& Mode);
    void ConfigureGameWindow();
    void SpawnEnemyFlightTest();
    void SpawnCombatProvingGround();
    void SpawnOperationalHelipads(const FRotorlineMissionDefinition& Mission);
    void SpawnIslandGroundDefenseNetwork(bool bPreviewOnly);
    void StartDamageIntegrityQualification();
    void FinishDamageIntegrityQualification();

    TArray<FRotorlineMissionDefinition> Missions;
    TArray<FRotorlineAircraftDefinition> Aircraft;
    TArray<FRotorlineCastMember> CastMembers;
    FString CatalogError;
    FString AircraftCatalogError;
    int32 SelectedMissionIndex = 0;
    int32 SelectedAircraftIndex = 0;
    ERotorlineCraftType SelectedCraft = ERotorlineCraftType::SupportHuey;
    bool bOperationsMenuOpen = true;
    bool bHangarOpen = false;
    bool bVerticalAxisLatched = false;
    bool bHorizontalAxisLatched = false;
    bool bGamepadInputSeen = false;
    bool bStartupControllerFocusActive = false;
    bool bAudioSettingsOpen = false;
    bool bGraphicsSettingsOpen = false;
    bool bGraphicsInputArmed = false;
    bool bControlsSettingsOpen = false;
    bool bTacticalMapVisible = true;
    bool bFlightPauseMenuOpen = false;
    bool bMissionFailureScreenOpen = false;
    bool bMissionCompleteScreenOpen = false;
    bool bAwardPresentationOpen = false;
    bool bPatchWallOpen = false;
    ERotorlineStartupState StartupState = ERotorlineStartupState::Inactive;
    ERotorlineStartupState StartupTransitionTarget = ERotorlineStartupState::Inactive;
    int32 SelectedStartupMenuIndex = 0;
    int32 SelectedCastIndex = 0;
    int32 HoveredStartupMenuIndex = INDEX_NONE;
    int32 PressedStartupMenuIndex = INDEX_NONE;
    float StartupStateElapsed = 0.0f;
    float StartupFadeAlpha = 0.0f;
    float StartupPressedTimer = 0.0f;
    float StartupMediaOpenElapsed = 0.0f;
    float CastVoiceElapsed = 0.0f;
    float CastVoiceDuration = 0.0f;
    float CastVoiceStartDelay = -1.0f;
    float CastTransitionAlpha = 0.0f;
    int32 CastTransitionDirection = 0;
    bool bStartupFadeToBlack = false;
    bool bStartupFadeFromBlack = false;
    bool bStartupMediaReady = false;
    bool bPlayingSplashIntro = false;
    bool bPlayingLoreIntro = false;
    bool bM25FinalCreditsSequenceActive = false;
    bool bM25FinalCreditsSequenceCompleted = false;
    bool bM25FinalCreditsSequenceFailed = false;
    bool bM25CreditsRollStartedLogged = false;
    bool bStartupQualificationActionIssued = false;
    bool bStartupMousePositionValid = false;
    int32 StartupQualificationPhase = 0;
    FString StartupQualificationScenario;
    FString CastCatalogError;
    FString PendingCastVoiceReason;
    FVector2D LastStartupMousePosition = FVector2D::ZeroVector;
    FVector2D StartupControllerMouseAnchor = FVector2D::ZeroVector;
    bool bAbortMissionPending = false;
    bool bResumeMouseCapture = false;
    bool bGameWindowConfigured = false;
    bool bFleetQualificationMode = false;
    bool bFleetQualificationSkipStartup = false;
    bool bCombatPreview = false;
    bool bCombatPreviewAutoExit = false;
    bool bGroundDefensePreview = false;
    bool bDamageIntegrityQualification = false;
    bool bDamageIntegrityMatrixPassed = false;
    float CombatPreviewElapsed = 0.0f;
    bool bQuickDeploy = false;
    bool bQuickDeploySkipStartup = false;
    bool bCombatLoopTestActionActivated = false;
    FString CombatLoopTestScenario;
    bool bMissionLoopTestActionActivated = false;
    FString MissionLoopTestScenario;
    int32 MissionLoopCompletedRuns = 0;
    FString EnemyFlightTestAirframe;
    int32 SelectedPauseRow = 0;
    int32 SelectedMissionFailureAction = 0;
    int32 SelectedMissionCompleteAction = 0;
    int32 AwardPresentationIndex = 0;
    int32 PatchWallSelection = 0;
    int32 SelectedAudioRow = 0;
    int32 SelectedGraphicsRow = 0;
    int32 SelectedControlsTab = 2;
    int32 SelectedControlsRow = 0;
    int32 ControlsLiveTestPage = 0;
    ERotorlineControlsMode ControlsMode = ERotorlineControlsMode::Home;
    int32 ControlsWizardStep = 0;
    FString ControlsStatus;
    FString ControlsCaptureFeedback;
    FString ControlsDeviceId;
    FString LastObservedControllerDeviceId;
    FString FlightControllerNotification;
    float FlightControllerNotificationSeconds = 0.0f;
    bool bControllerFirstTimePromptChecked = false;
    bool bWorkingControllerProfileDirty = false;
    FRotorlineFlightControllerProfile WorkingControllerProfile;
    FRotorlineFlightControllerProfile ControlsSnapshotProfile;
    FString ControlsSnapshotDeviceId;
    bool bControlsSnapshotHadProfile = false;
    TArray<float> ControlsAxisBaseline;
    TArray<float> ControlsAxisMinimum;
    TArray<float> ControlsAxisMaximum;
    TArray<float> ControlsAxisRestMinimum;
    TArray<float> ControlsAxisRestMaximum;
    TArray<int8> ControlsAxisFirstExcursionSign;
    float ControlsCaptureElapsed = 0.0f;
    float ControlsTriggerArmingSeconds = 0.0f;
    float ControlsInputSuppressionSeconds = 0.0f;
    bool bControlsInputArmed = false;
    TArray<bool> ControlsPreviousButtons;
    TArray<float> ControlsPreviousHats;
    TArray<bool> ControlsPreviousAxisTriggers;
    bool bNamingControlsAxis = false;
    FString PendingControlsAxisLabel;
    int32 ControlsDetectedAxis = INDEX_NONE;
    int32 PendingDuplicateButton = INDEX_NONE;
    FName PendingDuplicateAction = NAME_None;
    int32 PendingDuplicateAxis = INDEX_NONE;
    int32 PendingDuplicateHat = INDEX_NONE;
    int32 PendingDuplicateHatDirection = INDEX_NONE;
    TMap<FName, bool> ControllerActionCurrent;
    TMap<FName, bool> ControllerActionPrevious;
    TSet<FName> ControllerAcceptanceActionsLogged;
    TArray<FKey> PendingControlsInputKeys;
    TArray<bool> ControlsGamepadButtonPrevious;
    FString ControlsMenuGamepadDeviceId;
    int32 ControlsGamepadPreviousHatDirection = INDEX_NONE;
    bool bControlsLeftStickVerticalLatched = false;
    bool bControlsLeftStickHorizontalLatched = false;
    bool bControlsCompatibleGamepadConfirmHeld = false;
    float EnvironmentAudioUpdateAccumulator = 0.0f;
    float LastLoggedEnvironmentMix = -1.0f;
    TMap<TWeakObjectPtr<UAudioComponent>, float> EnvironmentBaseVolumes;
    FRotorlineMissionResults MissionResults;
    FRotorlineAwardSystem AwardSystem;
    TArray<FRotorlineAwardEvaluation> NewlyEarnedAwards;
    FString AwardsCatalogError;
    FVector LastTelemetryLocation = FVector::ZeroVector;
    FVector LastTelemetryVelocity = FVector::ZeroVector;
    float CurrentStableHoverSeconds = 0.0f;
    float LastTelemetryAltitudeAgl = 0.0f;
    float LastAirborneVerticalSpeedMps = 0.0f;
    float ObstacleTraceAccumulator = 0.0f;
    double LastNearMissTime = -1000.0;
    bool bTelemetryLocationValid = false;
    bool bTelemetryWasAirborne = false;
    bool bTelemetryTakeoffArmed = true;
    bool bTelemetryLandingRecorded = false;
    int32 MissionResetGeneration = 0;

    UPROPERTY()
    TObjectPtr<URotorlineProfileSave> ProfileSave;

    UPROPERTY()
    TMap<FString, TObjectPtr<UTexture2D>> AwardPatchTextures;

    UPROPERTY()
    TMap<FString, TObjectPtr<UTexture2D>> CastCardTextures;

    UPROPERTY()
    TMap<FString, TObjectPtr<UTexture2D>> AircraftBlueprintTextures;

    UPROPERTY()
    TMap<FString, TObjectPtr<USoundBase>> CastVoiceProfiles;

    UPROPERTY()
    TObjectPtr<UAudioComponent> CastVoiceAudio;

    UPROPERTY()
    TObjectPtr<UAudioComponent> PreGameMenuMusicAudio;

    UPROPERTY()
    TObjectPtr<USoundBase> PreGameMenuMusicSound;

    UPROPERTY()
    TObjectPtr<UMediaPlayer> StartupMediaPlayer;

    UPROPERTY()
    TObjectPtr<UMediaTexture> StartupMediaTexture;

    UPROPERTY()
    TObjectPtr<UMediaSoundComponent> StartupMediaSound;

    UPROPERTY()
    TObjectPtr<UFileMediaSource> SplashIntroMediaSource;

    UPROPERTY()
    TObjectPtr<UFileMediaSource> LoreIntroMediaSource;

    UPROPERTY()
    TObjectPtr<UFileMediaSource> IntroMediaSource;

    UPROPERTY()
    TObjectPtr<UFileMediaSource> CreditsMediaSource;

    UPROPERTY()
    TObjectPtr<UFileMediaSource> M25FinalMediaSource;

    UPROPERTY()
    TObjectPtr<UTexture2D> StartupBackgroundTexture;

    UPROPERTY()
    TObjectPtr<UTexture2D> OperationsBoardBackgroundTexture;

    UPROPERTY()
    TObjectPtr<ARotorlineHangarPreviewActor> HangarPreviewActor;

    UPROPERTY()
    TObjectPtr<ARotorlineHelipadBeaconActor> HomeHelipadBeaconActor;

    UPROPERTY()
    TObjectPtr<ARotorlineHelipadBeaconActor> CityServiceHelipadActor;

    UPROPERTY()
    TObjectPtr<ARotorlineHelipadBeaconActor> HospitalHelipadActor;

    UPROPERTY()
    TObjectPtr<ARotorlineBellLairActor> BellLairActor;

    UPROPERTY()
    TObjectPtr<ARotorlineCaveTransitionActor> CaveTransitionActor;

    bool bCaveTransitionPending = false;
    bool bCaveAccessSequenceComplete = false;

    FVector ActiveHomePadLocation = FVector::ZeroVector;

    UPROPERTY()
    TObjectPtr<ARotorlineMissionObjectiveActor> EnemyFlightQualificationActor;

    UPROPERTY()
    TArray<TObjectPtr<ARotorlineMissionObjectiveActor>> CombatPreviewActors;

    UPROPERTY()
    TArray<TObjectPtr<ARotorlineMissionObjectiveActor>> GroundDefenseActors;

    UPROPERTY()
    TObjectPtr<ARotorlineMissionObjectiveActor> DamageIntegrityRocketTarget;

    UPROPERTY()
    TObjectPtr<ARotorlineMissionObjectiveActor> DamageIntegrityCannonTarget;
};
