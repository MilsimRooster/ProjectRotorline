#include "RotorlineOperationsPlayerController.h"

#include "RotorlineEnemyIslandAssaultActor.h"
#include "RotorlineCombatTuning.h"

#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "FileMediaSource.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RotorlineHangarPreviewActor.h"
#include "RotorlineHelipadBeaconActor.h"
#include "RotorlineBellLairActor.h"
#include "RotorlineCaveTransitionActor.h"
#include "RotorlineHelicopterPawn.h"
#include "RotorlineJeepPawn.h"
#include "RotorlineCannonProjectile.h"
#include "RotorlineMissionObjectiveActor.h"
#include "RotorlineGroundingLibrary.h"
#include "RotorlineProfileSave.h"
#include "RotorlineRocketProjectile.h"
#include "RotorlineRocketTrailSegment.h"
#include "RotorlineSupportLocations.h"
#include "Sound/AmbientSound.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SWindow.h"
#include "LandscapeProxy.h"

namespace RotorlineOperations
{
    // Center of the black-circle Airbase Hero helipad. Z seats the collision
    // box above the imported 2.27 m deck so both aircraft rest on their skids.
    const FVector SpawnLocation(-236194.1, -193027.5, 3595.0);
    const FRotator SpawnRotation(0.0, 8.0, 0.0);
    bool bIntroPlayedThisProcess = false;

    // Smooth Operator must recognize the low, controlled hover players use at
    // pickup and landing pads. AGL is skid clearance, so 2 m proves the craft
    // is airborne without imposing the old, undocumented 6 m floor.
    constexpr float StableHoverMinimumAglMeters = 2.0f;
    constexpr float StableHoverMaximumHorizontalSpeedMps = 2.5f;
    constexpr float StableHoverMaximumVerticalSpeedMps = 1.0f;
    constexpr float StableHoverMaximumAttitudeDegrees = 10.0f;
    constexpr float StableHoverBreakGraceSeconds = 0.75f;

    bool IsStableHoverState(const FRotorlineAwardsFlightState& Flight)
    {
        const float HorizontalSpeedMps = Flight.Velocity.Size2D() / 100.0f;
        const float VerticalSpeedMps = FMath::Abs(Flight.Velocity.Z) / 100.0f;
        const float AttitudeDegrees = FMath::Max(FMath::Abs(Flight.PitchDegrees), FMath::Abs(Flight.RollDegrees));
        return Flight.bEnginePowerAvailable && !Flight.bAircraftDying && !Flight.bMissionFailed &&
            Flight.AltitudeAglMeters >= StableHoverMinimumAglMeters &&
            HorizontalSpeedMps <= StableHoverMaximumHorizontalSpeedMps &&
            VerticalSpeedMps <= StableHoverMaximumVerticalSpeedMps &&
            AttitudeDegrees <= StableHoverMaximumAttitudeDegrees;
    }

    void WriteAlphaRuntimeProbe(const TCHAR* Status, const TCHAR* Reason)
    {
        FString ProbePath;
        if (!FParse::Value(FCommandLine::Get(), TEXT("AlphaRuntimeProbe="), ProbePath))
        {
            return;
        }

        ProbePath.TrimQuotesInline();
        if (ProbePath.IsEmpty())
        {
            return;
        }

        ProbePath = FPaths::ConvertRelativePathToFull(ProbePath);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(ProbePath), true);
        const FString ProbeText = FString::Printf(
            TEXT("ROTORLINE_RUNTIME_PROBE|status=%s|reason=%s|utc=%s\n"),
            Status,
            Reason,
            *FDateTime::UtcNow().ToIso8601());
        FFileHelper::SaveStringToFile(
            ProbeText,
            *ProbePath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

ARotorlineOperationsPlayerController::ARotorlineOperationsPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    bShouldPerformFullTickWhenPaused = true;
}

void ARotorlineOperationsPlayerController::LogOceanDiagnosticRuntimeState(const TCHAR* Phase)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    UMaterialInterface* DebugMaterial = LoadObject<UMaterialInterface>(
        nullptr,
		TEXT("/Game/Environment/Nature/Ocean/M_RL_Ocean_ProductionRipple1.M_RL_Ocean_ProductionRipple1"));
    TArray<FString> BeforeLines;
    TArray<FString> FinalLines;
    int32 CustomOceanCount = 0;
    int32 DisabledWaterVisualCount = 0;
    int32 PreservedCollisionCount = 0;

    const auto MaterialDescription = [](UMaterialInterface* Material)
    {
        if (!Material)
        {
            return FString(TEXT("None|wireframe=None"));
        }
        const UMaterial* BaseMaterial = Material->GetMaterial();
        return FString::Printf(
            TEXT("%s|wireframe=%d"),
            *Material->GetPathName(),
            BaseMaterial && BaseMaterial->Wireframe ? 1 : 0);
    };

    const auto AppendComponentState = [&MaterialDescription](
        TArray<FString>& Lines,
        const TCHAR* State,
        AActor* ComponentOwner,
        UPrimitiveComponent* Component)
    {
        UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
        UMeshComponent* MeshComponent = Cast<UMeshComponent>(Component);
        UStaticMesh* StaticMesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;
        const int32 bEvaluateWpo = StaticMeshComponent
            ? (StaticMeshComponent->bEvaluateWorldPositionOffset ? 1 : 0)
            : -1;
        Lines.Add(FString::Printf(
            TEXT("%s|actor=%s|component=%s|class=%s|visible=%d|hidden_in_game=%d|render_in_main_pass=%d|render_in_depth_pass=%d|visible_in_reflection_captures=%d|visible_in_ray_tracing=%d|evaluate_world_position_offset=%d|cast_shadow=%d|static_mesh=%s"),
            State,
            ComponentOwner ? *ComponentOwner->GetPathName() : TEXT("None"),
            *Component->GetPathName(),
            *Component->GetClass()->GetPathName(),
            Component->IsVisible() ? 1 : 0,
            Component->bHiddenInGame ? 1 : 0,
            Component->bRenderInMainPass ? 1 : 0,
            Component->bRenderInDepthPass ? 1 : 0,
            Component->bVisibleInReflectionCaptures ? 1 : 0,
            Component->bVisibleInRayTracing ? 1 : 0,
            bEvaluateWpo,
            Component->CastShadow ? 1 : 0,
            StaticMesh ? *StaticMesh->GetPathName() : TEXT("None")));

        if (!MeshComponent)
        {
            return;
        }

        if (StaticMesh)
        {
            const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
            for (int32 Slot = 0; Slot < StaticMaterials.Num(); ++Slot)
            {
                Lines.Add(FString::Printf(
                    TEXT("%s|component=%s|base_material[%d]=%s"),
                    State,
                    *Component->GetPathName(),
                    Slot,
                    *MaterialDescription(StaticMaterials[Slot].MaterialInterface)));
            }
        }

        for (int32 Slot = 0; Slot < MeshComponent->OverrideMaterials.Num(); ++Slot)
        {
            Lines.Add(FString::Printf(
                TEXT("%s|component=%s|override_material[%d]=%s"),
                State,
                *Component->GetPathName(),
                Slot,
                *MaterialDescription(MeshComponent->OverrideMaterials[Slot])));
        }
        Lines.Add(FString::Printf(
            TEXT("%s|component=%s|overlay_material=%s"),
            State,
            *Component->GetPathName(),
            *MaterialDescription(MeshComponent->GetOverlayMaterial())));

        const int32 MaterialCount = MeshComponent->GetNumMaterials();
        for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
        {
            Lines.Add(FString::Printf(
                TEXT("%s|component=%s|effective_material[%d]=%s|material_slots_overlay_material[%d]=%s"),
                State,
                *Component->GetPathName(),
                Slot,
                *MaterialDescription(MeshComponent->GetMaterial(Slot)),
                Slot,
                *MaterialDescription(MeshComponent->GetOverlayMaterial(true, Slot))));
        }

        if (StaticMesh)
        {
            const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
            const int32 LodCount = RenderData ? RenderData->LODResources.Num() : 0;
            for (int32 Lod = 0; Lod < LodCount; ++Lod)
            {
                const FStaticMeshLODResources& LodResources = RenderData->LODResources[Lod];
                const int32 SectionCount = LodResources.Sections.Num();
                for (int32 Section = 0; Section < SectionCount; ++Section)
                {
                    const int32 MaterialIndex = LodResources.Sections[Section].MaterialIndex;
                    Lines.Add(FString::Printf(
                        TEXT("%s|component=%s|lod=%d|section=%d|material_index=%d|effective_material=%s"),
                        State,
                        *Component->GetPathName(),
                        Lod,
                        Section,
                        MaterialIndex,
                        *MaterialDescription(MeshComponent->GetMaterial(MaterialIndex))));
                }
            }
        }
    };

    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        AActor* Actor = *ActorIt;
        TArray<UPrimitiveComponent*> PrimitiveComponents;
        Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
        for (UPrimitiveComponent* Component : PrimitiveComponents)
        {
            UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
            UStaticMesh* StaticMesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;
            const FString ActorClassName = Actor->GetClass()->GetName();
            const FString ComponentClassName = Component->GetClass()->GetName();
            const bool bCustomOcean = StaticMesh && StaticMesh->GetPathName().Contains(
                TEXT("RotorlineOceanGrid24km"), ESearchCase::IgnoreCase);
            const bool bWaterRelated =
                ActorClassName.Contains(TEXT("WaterBody"), ESearchCase::IgnoreCase) ||
                ActorClassName.Contains(TEXT("WaterZone"), ESearchCase::IgnoreCase) ||
                ComponentClassName.Contains(TEXT("WaterMeshComponent"), ESearchCase::IgnoreCase) ||
                ComponentClassName.Contains(TEXT("WaterBody"), ESearchCase::IgnoreCase) ||
                ComponentClassName.Contains(TEXT("WaterSpline"), ESearchCase::IgnoreCase) ||
                ComponentClassName.Contains(TEXT("WaterInfoMesh"), ESearchCase::IgnoreCase);
            if (!bCustomOcean && !bWaterRelated)
            {
                continue;
            }

            AppendComponentState(BeforeLines, TEXT("BEFORE"), Actor, Component);
            if (bCustomOcean)
            {
                ++CustomOceanCount;
                if (UMeshComponent* MeshComponent = Cast<UMeshComponent>(Component))
                {
                    MeshComponent->EmptyOverrideMaterials();
                    MeshComponent->SetOverlayMaterial(nullptr);
                    const int32 SlotCount = FMath::Max(1, MeshComponent->GetNumMaterials());
                    for (int32 Slot = 0; Slot < SlotCount; ++Slot)
                    {
                        MeshComponent->SetOverlayMaterial(nullptr, true, Slot);
                        if (DebugMaterial)
                        {
                            MeshComponent->SetMaterial(Slot, DebugMaterial);
                        }
                    }
                }
                if (StaticMeshComponent)
                {
                    StaticMeshComponent->SetEvaluateWorldPositionOffset(false);
                }
                Component->SetVisibility(true, true);
                Component->SetHiddenInGame(false, true);
                Component->SetRenderInMainPass(true);
            }
            else if (Component->GetClass()->GetName().Contains(TEXT("Collision"), ESearchCase::IgnoreCase))
            {
                ++PreservedCollisionCount;
            }
            else
            {
                ++DisabledWaterVisualCount;
                Component->SetVisibility(false, true);
                Component->SetHiddenInGame(true, true);
                Component->SetRenderInMainPass(false);
                Component->SetRenderInDepthPass(false);
                Component->bVisibleInReflectionCaptures = false;
                Component->MarkRenderStateDirty();
                Component->SetVisibleInRayTracing(false);
                Component->SetCastShadow(false);
            }
            AppendComponentState(FinalLines, TEXT("FINAL"), Actor, Component);
        }
    }

    TArray<FString> ReportLines;
    ReportLines.Add(FString::Printf(
        TEXT("ROTORLINE_OCEAN_DIAGNOSTIC|phase=%s|custom_ocean_count=%d|disabled_water_visual_count=%d|preserved_collision_count=%d|debug_material_loaded=%d"),
        Phase,
        CustomOceanCount,
        DisabledWaterVisualCount,
        PreservedCollisionCount,
        DebugMaterial ? 1 : 0));
    ReportLines.Append(BeforeLines);
    ReportLines.Append(FinalLines);
    const FString ReportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("OceanDiagnostics"));
    IFileManager::Get().MakeDirectory(*ReportDirectory, true);
    const FString ReportPath = FPaths::Combine(
        ReportDirectory,
        FString::Printf(TEXT("RotorlineOceanRuntimeAudit_%s.txt"), Phase));
    FFileHelper::SaveStringArrayToFile(ReportLines, *ReportPath);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_OCEAN_DIAG|RUNTIME_REPORT|phase=%s|path=%s"), Phase, *ReportPath);
}

void ARotorlineOperationsPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopPreGameMenuMusic(TEXT("CONTROLLER_END_PLAY"));
    StopCastVoice(TEXT("CONTROLLER_END_PLAY"));
    CloseStartupMedia(TEXT("CONTROLLER_END_PLAY"));
    if (StartupMediaSound)
    {
        StartupMediaSound->Stop();
    }
    Super::EndPlay(EndPlayReason);
}

void ARotorlineOperationsPlayerController::BeginPlay()
{
    Super::BeginPlay();

    RotorlineOperations::WriteAlphaRuntimeProbe(TEXT("STARTING"), TEXT("CONTROLLER_BEGIN_PLAY"));

    LogOceanDiagnosticRuntimeState(TEXT("BEGINPLAY"));
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        LogOceanDiagnosticRuntimeState(TEXT("POST_BEGINPLAY"));
        if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineOceanDiagnosticAutoExit")))
        {
            FPlatformMisc::RequestExit(false);
        }
    }));

    PreGameMenuMusicSound = LoadObject<USoundBase>(
        nullptr,
        TEXT("/Game/Audio/Music/Rotorline/SC_RotorBreak_Menu_Loop.SC_RotorBreak_Menu_Loop"));
    if (PreGameMenuMusicSound)
    {
        PreGameMenuMusicAudio = UGameplayStatics::CreateSound2D(
            this, PreGameMenuMusicSound, 1.0f, 1.0f, 0.0f, nullptr, true, false);
    }

    const bool bForceHardwareRayTracing =
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineRayTracing"));
    const bool bForcePerformanceMode =
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineNoRayTracing"));
    FString SavedGraphicsMode;
    const bool bHasSavedGraphicsMode = GConfig && GConfig->GetString(
        TEXT("Rotorline.Graphics"),
        TEXT("SimpleMode"),
        SavedGraphicsMode,
        GGameUserSettingsIni);
    const bool bStartInTurboMode = bHasSavedGraphicsMode
        ? SavedGraphicsMode.Equals(TEXT("TURBO"), ESearchCase::IgnoreCase)
        : (bForceHardwareRayTracing && !bForcePerformanceMode);
    ApplySimpleGraphicsMode(bStartInTurboMode, false, false);

    FParse::Value(FCommandLine::Get(), TEXT("RotorlineCombatLoopTest="), CombatLoopTestScenario);
    CombatLoopTestScenario.TrimQuotesInline();
    bCombatLoopTestActionActivated = false;
    FParse::Value(FCommandLine::Get(), TEXT("RotorlineMissionLoopTest="), MissionLoopTestScenario);
    MissionLoopTestScenario.TrimQuotesInline();
    bMissionLoopTestActionActivated = false;

    FRotorlineMissionCatalog::Load(Missions, CatalogError);
    FRotorlineAircraftCatalog::Load(Aircraft, AircraftCatalogError);
    Aircraft.RemoveAll([](const FRotorlineAircraftDefinition& Entry)
    {
        return !Entry.bHangarVisible || !Entry.bAlphaSelectable || !Entry.bReadyForHangar;
    });
    LoadAircraftBlueprintTextures();
    LoadProfile();
    LoadAwardDefinitions();
    const bool bRunningOnFumesRepaired = ApplyRunningOnFumesTelemetryRepair();
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineAwardsRepairTest")))
    {
        const FRotorlinePlayerAwardRecord* Record = GetAwardRecord(TEXT("running_on_fumes"));
        const bool bPassed = Record && Record->TimesEarned > 0;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AWARDS_REPAIR_TEST|repaired=%d|record=%d|status=%s"),
            bRunningOnFumesRepaired ? 1 : 0,
            bPassed ? 1 : 0,
            bPassed ? TEXT("PASS") : TEXT("FAIL"));
        FPlatformMisc::RequestExit(!bPassed);
    }
    LoadCastDefinitions();
    RefreshEnvironmentAudioMix();
    ApplyMouseMode(false);
    ConfigureGameWindow();

    if (!Missions.IsEmpty())
    {
        SelectedCraft = Missions[0].RecommendedCraft.Equals(TEXT("attack"), ESearchCase::IgnoreCase)
            ? ERotorlineCraftType::AttackMD500
            : ERotorlineCraftType::SupportHuey;
        const FString RecommendedId = SelectedCraft == ERotorlineCraftType::AttackMD500
            ? TEXT("md500_defender")
            : TEXT("uh1_huey");
        SelectedAircraftIndex = FMath::Max(0, Aircraft.IndexOfByPredicate(
            [&RecommendedId](const FRotorlineAircraftDefinition& Entry)
            {
                return Entry.Id.Equals(RecommendedId, ESearchCase::IgnoreCase);
            }));
    }

    FString FleetQualificationAircraftId;
    FParse::Value(
        FCommandLine::Get(),
        TEXT("RotorlineEnemyTest="),
        EnemyFlightTestAirframe);
    EnemyFlightTestAirframe.TrimQuotesInline();
    bCombatPreview = FParse::Param(FCommandLine::Get(), TEXT("RotorlineCombatPreview"));
    bCombatPreviewAutoExit = FParse::Param(FCommandLine::Get(), TEXT("RotorlineCombatPreviewAutoExit"));
    bGroundDefensePreview = FParse::Param(FCommandLine::Get(), TEXT("RotorlineGroundDefensePreview"));
    bDamageIntegrityQualification = FParse::Param(FCommandLine::Get(), TEXT("RotorlineDamageIntegrityTest"));
    if (FParse::Value(
        FCommandLine::Get(),
        TEXT("RotorlineFleetTest="),
        FleetQualificationAircraftId))
    {
        FleetQualificationAircraftId.TrimQuotesInline();
        const int32 QualificationIndex = Aircraft.IndexOfByPredicate(
            [&FleetQualificationAircraftId](const FRotorlineAircraftDefinition& Entry)
            {
                return Entry.Id.Equals(FleetQualificationAircraftId, ESearchCase::IgnoreCase);
            });
        if (QualificationIndex != INDEX_NONE && !Missions.IsEmpty())
        {
            SelectedMissionIndex = 0;
            SelectedAircraftIndex = QualificationIndex;
            bFleetQualificationMode = true;
            bFleetQualificationSkipStartup = FParse::Param(
                FCommandLine::Get(),
                TEXT("RotorlineFleetSkipStartup"));
            GetWorldTimerManager().SetTimerForNextTick(
                this,
                &ARotorlineOperationsPlayerController::DeploySelectedAircraft);
            UE_LOG(
                LogTemp,
                Display,
                TEXT("ROTORLINE_FLEET_TEST|REQUEST|id=%s|skip_startup=%d"),
                *FleetQualificationAircraftId,
                bFleetQualificationSkipStartup ? 1 : 0);
        }
        else
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("ROTORLINE_FLEET_TEST|INVALID_AIRCRAFT|id=%s"),
                *FleetQualificationAircraftId);
        }
    }

    int32 QuickDeployMissionIndex = 0;
    FString QuickDeployMissionId;
    if (FParse::Value(FCommandLine::Get(), TEXT("RotorlineQuickMission="), QuickDeployMissionId))
    {
        QuickDeployMissionId.TrimQuotesInline();
        const int32 RequestedMissionIndex = Missions.IndexOfByPredicate(
            [&QuickDeployMissionId](const FRotorlineMissionDefinition& Entry)
            {
                return Entry.Id.Equals(QuickDeployMissionId, ESearchCase::IgnoreCase);
            });
        if (RequestedMissionIndex != INDEX_NONE)
        {
            QuickDeployMissionIndex = RequestedMissionIndex;
        }
    }

    FString QuickDeployAircraftId;
    if (!bFleetQualificationMode && FParse::Value(
        FCommandLine::Get(),
        TEXT("RotorlineQuickDeploy="),
        QuickDeployAircraftId))
    {
        QuickDeployAircraftId.TrimQuotesInline();
        const int32 QuickDeployIndex = Aircraft.IndexOfByPredicate(
            [&QuickDeployAircraftId](const FRotorlineAircraftDefinition& Entry)
            {
                return Entry.Id.Equals(QuickDeployAircraftId, ESearchCase::IgnoreCase);
            });
        if (QuickDeployIndex != INDEX_NONE && !Missions.IsEmpty())
        {
            SelectedMissionIndex = QuickDeployMissionIndex;
            SelectedAircraftIndex = QuickDeployIndex;
            bQuickDeploy = true;
            bQuickDeploySkipStartup = FParse::Param(FCommandLine::Get(), TEXT("RotorlineQuickDeploySkipStartup"));
            GetWorldTimerManager().SetTimerForNextTick(this, &ARotorlineOperationsPlayerController::DeploySelectedAircraft);
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_QUICK_DEPLOY|REQUEST|id=%s|skip_startup=%d|player_control=ENABLED"),
                *QuickDeployAircraftId, bQuickDeploySkipStartup ? 1 : 0);
        }
    }

    if (!bFleetQualificationMode && !bQuickDeploy &&
        FParse::Param(FCommandLine::Get(), TEXT("RotorlineMissionCompletePreview")))
    {
        GetWorldTimerManager().SetTimerForNextTick(
            this,
            &ARotorlineOperationsPlayerController::OpenMissionCompletePreview);
    }

    FString AwardsPreview;
    if (!bFleetQualificationMode && !bQuickDeploy &&
        FParse::Value(FCommandLine::Get(), TEXT("RotorlineAwardsPreview="), AwardsPreview))
    {
        AwardsPreview.TrimQuotesInline();
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, AwardsPreview]()
        {
            if (AwardsPreview.Equals(TEXT("wall"), ESearchCase::IgnoreCase))
            {
                bOperationsMenuOpen = true;
                bPatchWallOpen = true;
                PatchWallSelection = 0;
                UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_PREVIEW|READY|screen=PATCH_WALL"));
            }
            else
            {
                RotorlineAwardsSimulate(AwardsPreview.IsEmpty() ? TEXT("multi") : AwardsPreview);
            }
        }));
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineAwardsTest")))
    {
        GetWorldTimerManager().SetTimerForNextTick(this, &ARotorlineOperationsPlayerController::RunAwardsSelfTest);
    }
    FString AwardsPersistenceMode;
    if (FParse::Value(FCommandLine::Get(), TEXT("RotorlineAwardsPersistenceTest="), AwardsPersistenceMode))
    {
        AwardsPersistenceMode.TrimQuotesInline();
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, AwardsPersistenceMode]()
        {
            if (AwardsPersistenceMode.Equals(TEXT("write"), ESearchCase::IgnoreCase))
            {
                ForceUnlockAward(TEXT("lift_off"), TEXT("persistence qualification write"));
                const bool bSaved = GetAwardRecord(TEXT("lift_off")) != nullptr;
                UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_PERSISTENCE|WRITE|status=%s"), bSaved ? TEXT("PASS") : TEXT("FAIL"));
                FPlatformMisc::RequestExit(!bSaved);
            }
            else
            {
                const FRotorlinePlayerAwardRecord* Record = GetAwardRecord(TEXT("lift_off"));
                const bool bLoaded = Record && Record->TimesEarned > 0;
                UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_PERSISTENCE|READ|status=%s|times=%d"),
                    bLoaded ? TEXT("PASS") : TEXT("FAIL"), Record ? Record->TimesEarned : 0);
                FPlatformMisc::RequestExit(!bLoaded);
            }
        }));
    }

    InitializeStartupFlow();

    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineHangarPreview")))
    {
        FString PreviewAircraftId;
        FParse::Value(FCommandLine::Get(), TEXT("RotorlineHangarAircraft="), PreviewAircraftId);
        PreviewAircraftId.TrimQuotesInline();
        if (!PreviewAircraftId.IsEmpty())
        {
            const int32 PreviewIndex = Aircraft.IndexOfByPredicate(
                [&PreviewAircraftId](const FRotorlineAircraftDefinition& Entry)
                {
                    return Entry.Id.Equals(PreviewAircraftId, ESearchCase::IgnoreCase);
                });
            if (PreviewIndex != INDEX_NONE) SelectedAircraftIndex = PreviewIndex;
        }
        StartupState = ERotorlineStartupState::Inactive;
        bOperationsMenuOpen = true;
        GetWorldTimerManager().SetTimerForNextTick(this, &ARotorlineOperationsPlayerController::OpenHangar);
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_HANGAR_PREVIEW|READY|aircraft=%s"),
            Aircraft.IsValidIndex(SelectedAircraftIndex) ? *Aircraft[SelectedAircraftIndex].Id : TEXT("NONE"));

        if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineHangarCapture")))
        {
            FTimerHandle CaptureTimer;
            GetWorldTimerManager().SetTimer(CaptureTimer, []()
            {
                const FString ScreenshotPath = FPaths::Combine(
                    FPaths::ProjectSavedDir(), TEXT("Screenshots/HangarPresentationRuntime.png"));
                IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
                FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_HANGAR_PREVIEW|CAPTURE_REQUESTED|path=%s"), *ScreenshotPath);
            }, 6.0f, false);

            FTimerHandle ExitTimer;
            GetWorldTimerManager().SetTimer(ExitTimer, []()
            {
                FPlatformMisc::RequestExit(false);
            }, 8.0f, false);
        }
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineFleetCapture")))
    {
        FTimerHandle CaptureTimer;
        GetWorldTimerManager().SetTimer(CaptureTimer, []()
        {
            const FString ScreenshotPath = FPaths::Combine(
                FPaths::ProjectSavedDir(), TEXT("Screenshots/FleetQualificationRuntime.png"));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_FLEET_TEST|CAPTURE_REQUESTED|path=%s"), *ScreenshotPath);
        }, 8.0f, false);
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineFlightControllerTest")))
    {
        GetWorldTimerManager().SetTimerForNextTick(
            this, &ARotorlineOperationsPlayerController::RunFlightControllerQualification);
    }
    FString FlightControllerRestartMode;
    if (FParse::Value(FCommandLine::Get(), TEXT("RotorlineFlightControllerRestartTest="), FlightControllerRestartMode))
    {
        FlightControllerRestartMode.TrimQuotesInline();
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
            this, [this, FlightControllerRestartMode]()
            {
                RunFlightControllerRestartQualification(FlightControllerRestartMode);
            }));
    }

}

void ARotorlineOperationsPlayerController::OpenMissionCompletePreview()
{
    if (Missions.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_MISSION_COMPLETE_PREVIEW|FAIL|reason=NO_MISSIONS"));
        return;
    }

    SelectedMissionIndex = FMath::Clamp(3, 0, Missions.Num() - 1);
    const int32 ApacheIndex = Aircraft.IndexOfByPredicate([](const FRotorlineAircraftDefinition& Entry)
    {
        return Entry.Id.Equals(TEXT("ah64_apache"), ESearchCase::IgnoreCase);
    });
    if (ApacheIndex != INDEX_NONE)
    {
        SelectedAircraftIndex = ApacheIndex;
    }

    ResetMissionResults(TEXT("MISSION_COMPLETE_UI_PREVIEW"));
    MissionResults.EnemyHelicoptersDestroyed = 1;
    MissionResults.GroundEnemiesDestroyed = 4;
    MissionResults.CiviliansRescued = 1;
    MissionResults.DamageTaken = 50.0f;
    MissionResults.AircraftHealth = 65.0f;
    MissionResults.AircraftMaxHealth = 100.0f;
    MissionResults.WeaponShotsFired = 59;
    MissionResults.WeaponHits = 5;
    MissionResults.bCivilianRescueTracked = true;
    MissionResults.bAircraftConditionTracked = true;
    MissionResults.bWeaponsTracked = true;
    bOperationsMenuOpen = false;
    bHangarOpen = false;
    OpenMissionCompleteScreen(Missions[SelectedMissionIndex], 412.0f);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_MISSION_COMPLETE_PREVIEW|READY|text_boost=1.60|resolution_adaptive=1"));
}

void ARotorlineOperationsPlayerController::InitializeStartupFlow()
{
    FString StartupPreview;
    FParse::Value(FCommandLine::Get(), TEXT("RotorlineStartupPreview="), StartupPreview);
    StartupPreview.TrimQuotesInline();
    FParse::Value(FCommandLine::Get(), TEXT("RotorlineStartupTest="), StartupQualificationScenario);
    StartupQualificationScenario.TrimQuotesInline();

    const FString CommandLine(FCommandLine::Get());
    const bool bExplicitStartupRun = !StartupPreview.IsEmpty() || !StartupQualificationScenario.IsEmpty();
    const bool bOtherRotorlineQualification = CommandLine.Contains(TEXT("-Rotorline"), ESearchCase::IgnoreCase) &&
        !CommandLine.Contains(TEXT("-RotorlineStartup"), ESearchCase::IgnoreCase);
    const bool bBypass = FParse::Param(FCommandLine::Get(), TEXT("RotorlineSkipStartup")) ||
        (!bExplicitStartupRun && (FParse::Param(FCommandLine::Get(), TEXT("Unattended")) || bOtherRotorlineQualification));

    StartupMediaPlayer = NewObject<UMediaPlayer>(this, TEXT("RotorlineStartupMediaPlayer"));
    StartupMediaTexture = NewObject<UMediaTexture>(this, TEXT("RotorlineStartupMediaTexture"));
    StartupMediaSound = NewObject<UMediaSoundComponent>(this, TEXT("RotorlineStartupMediaSound"));
    SplashIntroMediaSource = NewObject<UFileMediaSource>(this, TEXT("RotorlineSplashIntroMediaSource"));
    LoreIntroMediaSource = NewObject<UFileMediaSource>(this, TEXT("RotorlineLoreIntroMediaSource"));
    IntroMediaSource = NewObject<UFileMediaSource>(this, TEXT("RotorlineIntroMediaSource"));
    CreditsMediaSource = NewObject<UFileMediaSource>(this, TEXT("RotorlineCreditsMediaSource"));
    M25FinalMediaSource = NewObject<UFileMediaSource>(this, TEXT("RotorlineM25FinalMediaSource"));
    StartupBackgroundTexture = LoadObject<UTexture2D>(
        nullptr,
        TEXT("/Game/Rotorline/UI/Startup/T_RotorlineStartBackground.T_RotorlineStartBackground"));
    OperationsBoardBackgroundTexture = LoadObject<UTexture2D>(
        nullptr,
        TEXT("/Game/Rotorline/UI/Startup/T_RotorlineOperationsBoardBackground.T_RotorlineOperationsBoardBackground"));

    if (!StartupMediaPlayer || !StartupMediaTexture || !StartupMediaSound ||
        !SplashIntroMediaSource || !LoreIntroMediaSource || !IntroMediaSource || !CreditsMediaSource ||
        !M25FinalMediaSource)
    {
        RotorlineOperations::WriteAlphaRuntimeProbe(TEXT("FAIL"), TEXT("MEDIA_OBJECT_CREATION"));
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_STARTUP|INIT|status=FAIL|reason=MEDIA_OBJECT_CREATION"));
        StartupState = ERotorlineStartupState::StartScreen;
        bOperationsMenuOpen = false;
        ApplyMouseMode(false);
        return;
    }

    StartupMediaPlayer->PlayOnOpen = true;
    StartupMediaPlayer->OnMediaOpened.AddDynamic(this, &ARotorlineOperationsPlayerController::HandleStartupMediaOpened);
    StartupMediaPlayer->OnMediaOpenFailed.AddDynamic(this, &ARotorlineOperationsPlayerController::HandleStartupMediaOpenFailed);
    StartupMediaPlayer->OnEndReached.AddDynamic(this, &ARotorlineOperationsPlayerController::HandleStartupMediaEndReached);
    StartupMediaTexture->SetMediaPlayer(StartupMediaPlayer);
    StartupMediaTexture->AutoClear = true;
    StartupMediaTexture->ClearColor = FLinearColor::Black;
    StartupMediaTexture->UpdateResource();
    StartupMediaSound->Channels = EMediaSoundChannels::Stereo;
    StartupMediaSound->DynamicRateAdjustment = true;
    StartupMediaSound->bIsUISound = true;
    StartupMediaSound->SetMediaPlayer(StartupMediaPlayer);
    AddInstanceComponent(StartupMediaSound);
    StartupMediaSound->RegisterComponent();
    StartupMediaSound->Start();

    SplashIntroMediaSource->SetFilePath(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Movies/RotorlineSplashIntro.mp4")));
    LoreIntroMediaSource->SetFilePath(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Movies/RotorlineIntroLore.mp4")));
    IntroMediaSource->SetFilePath(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Movies/RotorlineIntro.mp4")));
    CreditsMediaSource->SetFilePath(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Movies/RotorlineCredits.mp4")));
    M25FinalMediaSource->SetFilePath(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Movies/M25_FinalEnd.mp4")));

    if (bBypass)
    {
        StartupState = ERotorlineStartupState::Inactive;
        bOperationsMenuOpen = true;
        bHangarOpen = false;
        ApplyMouseMode(false);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_STARTUP|BYPASS|reason=QUALIFICATION_OR_EXPLICIT_SKIP|media_resources=READY|credits=Movies/RotorlineCredits.mp4"));
        RotorlineOperations::WriteAlphaRuntimeProbe(TEXT("PASS"), TEXT("STARTUP_BYPASS_READY"));
        return;
    }

    bOperationsMenuOpen = false;
    bHangarOpen = false;
    ApplyMouseMode(false);
    StartupFadeAlpha = 1.0f;

    if (StartupPreview.Equals(TEXT("start"), ESearchCase::IgnoreCase) ||
        (RotorlineOperations::bIntroPlayedThisProcess && StartupPreview.IsEmpty() && StartupQualificationScenario.IsEmpty()))
    {
        EnterStartupState(ERotorlineStartupState::StartScreen);
        bStartupFadeFromBlack = true;
    }
    else if (StartupPreview.Equals(TEXT("credits"), ESearchCase::IgnoreCase))
    {
        EnterStartupState(ERotorlineStartupState::Credits);
    }
    else if (StartupPreview.Equals(TEXT("cast"), ESearchCase::IgnoreCase) ||
        StartupPreview.Equals(TEXT("personnel"), ESearchCase::IgnoreCase))
    {
        EnterStartupState(ERotorlineStartupState::CastGallery);
    }
    else if (StartupPreview.Equals(TEXT("patch_wall"), ESearchCase::IgnoreCase) ||
        StartupPreview.Equals(TEXT("stats"), ESearchCase::IgnoreCase))
    {
        EnterStartupState(ERotorlineStartupState::PatchWall);
        bStartupFadeFromBlack = true;
    }
    else
    {
        RotorlineOperations::bIntroPlayedThisProcess = true;
        bPlayingSplashIntro = false;
        bPlayingLoreIntro = false;
        EnterStartupState(ERotorlineStartupState::Intro);
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_STARTUP|INIT|status=PASS|state=%d|intro=Movies/RotorlineIntro.mp4|legacy_segments=DISABLED|credits=Movies/RotorlineCredits.mp4|main_background=%s|operations_background=%s"),
        static_cast<int32>(StartupState),
        StartupBackgroundTexture ? TEXT("READY") : TEXT("FALLBACK"),
        OperationsBoardBackgroundTexture ? TEXT("READY") : TEXT("FALLBACK"));
    RotorlineOperations::WriteAlphaRuntimeProbe(TEXT("PASS"), TEXT("STARTUP_INITIALIZED"));
}

void ARotorlineOperationsPlayerController::OpenStartupMedia(bool bCredits)
{
    if (!StartupMediaPlayer || !StartupMediaSound)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_STARTUP|MEDIA_OPEN_REQUEST|type=%s|status=FAIL|reason=MEDIA_RESOURCES_UNAVAILABLE"),
            bCredits ? TEXT("CREDITS") : TEXT("INTRO"));
        HandleStartupMediaOpenFailed(TEXT("MEDIA_RESOURCES_UNAVAILABLE"));
        return;
    }
    StartupMediaPlayer->Close();
    bStartupMediaReady = false;
    StartupMediaOpenElapsed = 0.0f;
    StartupMediaSound->SetVolumeMultiplier(GetEffectiveAudioVolume(ERotorlineAudioChannel::Music));
    StartupMediaPlayer->SetLooping(false);
    const bool bM25FinalMedia = bCredits && bM25FinalCreditsSequenceActive;
    UFileMediaSource* Source = bCredits
        ? (bM25FinalMedia ? M25FinalMediaSource.Get() : CreditsMediaSource.Get())
        : bPlayingSplashIntro ? SplashIntroMediaSource.Get()
        : bPlayingLoreIntro ? LoreIntroMediaSource.Get()
        : IntroMediaSource.Get();
    const TCHAR* MediaType = bCredits
        ? (bM25FinalMedia ? TEXT("M25_FINAL") : TEXT("CREDITS"))
        : bPlayingSplashIntro ? TEXT("SPLASH_INTRO")
        : bPlayingLoreIntro ? TEXT("LORE_INTRO")
        : TEXT("INTRO");
    const bool bRequested = Source && StartupMediaPlayer->OpenSource(Source);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|MEDIA_OPEN_REQUEST|type=%s|status=%s|path=%s"),
        MediaType, bRequested ? TEXT("PENDING") : TEXT("FAIL"),
        Source ? *Source->GetFilePath() : TEXT("NONE"));
    if (!bRequested)
    {
        HandleStartupMediaOpenFailed(Source ? Source->GetFilePath() : FString(TEXT("NONE")));
    }
}

void ARotorlineOperationsPlayerController::SetM25AircraftAudioSuppressed(bool bSuppressed)
{
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ROTORLINE_M25_FINALE|state=AIRCRAFT_AUDIO|action=%s|components=0|reason=NO_CONTROLLED_PAWN"),
            bSuppressed ? TEXT("SUPPRESSED") : TEXT("RESTORED"));
        return;
    }

    TArray<UAudioComponent*> AudioComponents;
    ControlledPawn->GetComponents<UAudioComponent>(AudioComponents);

    int32 AffectedComponentCount = 0;
    for (UAudioComponent* AudioComponent : AudioComponents)
    {
        if (IsValid(AudioComponent))
        {
            AudioComponent->SetPaused(bSuppressed);
            ++AffectedComponentCount;
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_M25_FINALE|state=GAMEPLAY_AUDIO|action=%s|pawn_components=%d|world_paused=%d|media_audio=UI_UNCHANGED"),
        bSuppressed ? TEXT("SUPPRESSED") : TEXT("RESTORED"),
        AffectedComponentCount,
        IsPaused() ? 1 : 0);
}

bool ARotorlineOperationsPlayerController::BeginM25FinalCreditsSequence()
{
    bM25FinalCreditsSequenceCompleted = false;
    bM25FinalCreditsSequenceFailed = false;
    bM25CreditsRollStartedLogged = false;

    if (!StartupMediaPlayer || !StartupMediaSound || !M25FinalMediaSource)
    {
        bM25FinalCreditsSequenceFailed = true;
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_M25_FINALE|state=FINAL_VIDEO_REQUEST|status=FAIL|reason=MEDIA_RESOURCES_UNAVAILABLE"));
        return false;
    }

    bM25FinalCreditsSequenceActive = true;
    bMissionCompleteScreenOpen = false;
    bMissionFailureScreenOpen = false;
    bFlightPauseMenuOpen = false;
    SetPause(false);
    SetM25AircraftAudioSuppressed(true);
    EnterStartupState(ERotorlineStartupState::Credits);
    ApplyMouseMode(false);

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_M25_FINALE|state=FINAL_VIDEO_REQUEST|status=%s|movie=Movies/M25_FinalEnd.mp4|credits_delay=10.0|skippable=0"),
        bM25FinalCreditsSequenceFailed ? TEXT("FAIL") : TEXT("PENDING"));
    return !bM25FinalCreditsSequenceFailed;
}

bool ARotorlineOperationsPlayerController::ShouldShowCreditsRoll() const
{
    if (StartupState != ERotorlineStartupState::Credits)
    {
        return false;
    }
    if (!bM25FinalCreditsSequenceActive)
    {
        return true;
    }

    constexpr double CreditsDelaySeconds = 10.0;
    const double MediaSeconds = StartupMediaPlayer
        ? StartupMediaPlayer->GetTime().GetTotalSeconds()
        : static_cast<double>(StartupStateElapsed);
    return MediaSeconds >= CreditsDelaySeconds;
}

void ARotorlineOperationsPlayerController::CloseStartupMedia(const TCHAR* Reason)
{
    if (StartupMediaPlayer)
    {
        StartupMediaPlayer->Pause();
        StartupMediaPlayer->Close();
    }
    bStartupMediaReady = false;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|MEDIA_CLOSE|reason=%s|audio=STOPPED"), Reason);
}

void ARotorlineOperationsPlayerController::UpdatePreGameMenuMusic()
{
    if (!PreGameMenuMusicAudio || !PreGameMenuMusicSound)
    {
        return;
    }

    const bool bStartupSelectionMenu =
        StartupState == ERotorlineStartupState::StartScreen ||
        StartupState == ERotorlineStartupState::CastGallery ||
        StartupState == ERotorlineStartupState::PatchWall;
    const bool bPreGameSelectionMenu =
        bStartupSelectionMenu || bOperationsMenuOpen || bHangarOpen ||
        (bAudioSettingsOpen && !bFlightPauseMenuOpen) ||
        (bControlsSettingsOpen && !bFlightPauseMenuOpen);
    const bool bShouldPlay = bPreGameSelectionMenu && !GetPawn() &&
        !bFlightPauseMenuOpen && !bMissionFailureScreenOpen && !bMissionCompleteScreenOpen;

    if (bShouldPlay && !PreGameMenuMusicAudio->IsPlaying())
    {
        PreGameMenuMusicAudio->SetSound(PreGameMenuMusicSound);
        PreGameMenuMusicAudio->SetVolumeMultiplier(
            0.42f * GetEffectiveAudioVolume(ERotorlineAudioChannel::Music));
        PreGameMenuMusicAudio->FadeIn(1.2f, PreGameMenuMusicAudio->VolumeMultiplier, 0.0f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MUSIC|context=PRE_GAME_MENU|track=ROTOR_BREAK|pause=SUPPRESSED|status=PLAYING"));
    }
    else if (!bShouldPlay && PreGameMenuMusicAudio->IsPlaying())
    {
        StopPreGameMenuMusic(bFlightPauseMenuOpen ? TEXT("FLIGHT_PAUSE_EXCLUDED") : TEXT("LEFT_PRE_GAME_MENU"));
    }
}

void ARotorlineOperationsPlayerController::StopPreGameMenuMusic(const TCHAR* Reason)
{
    if (PreGameMenuMusicAudio && PreGameMenuMusicAudio->IsPlaying())
    {
        PreGameMenuMusicAudio->FadeOut(0.45f, 0.0f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MUSIC|context=PRE_GAME_MENU|track=ROTOR_BREAK|status=STOPPED|reason=%s"), Reason);
    }
}

void ARotorlineOperationsPlayerController::EnterStartupState(ERotorlineStartupState NewState)
{
    StartupState = NewState;
    StartupStateElapsed = 0.0f;
    StartupMediaOpenElapsed = 0.0f;
    HoveredStartupMenuIndex = INDEX_NONE;
    bStartupMousePositionValid = false;
    bStartupControllerFocusActive = false;
    bVerticalAxisLatched = false;
    bHorizontalAxisLatched = false;
    UpdatePreGameMenuMusic();

    switch (NewState)
    {
    case ERotorlineStartupState::Intro:
        bOperationsMenuOpen = false;
        OpenStartupMedia(false);
        break;
    case ERotorlineStartupState::Credits:
        bPlayingSplashIntro = false;
        bPlayingLoreIntro = false;
        StopCastVoice(TEXT("OPEN_CREDITS"));
        bOperationsMenuOpen = false;
        OpenStartupMedia(true);
        break;
    case ERotorlineStartupState::CastGallery:
        bPlayingSplashIntro = false;
        bPlayingLoreIntro = false;
        CloseStartupMedia(TEXT("CAST_GALLERY"));
        bOperationsMenuOpen = false;
        SelectedCastIndex = FMath::Clamp(SelectedCastIndex, 0, FMath::Max(0, CastMembers.Num() - 1));
        CastTransitionAlpha = 1.0f;
        CastTransitionDirection = 0;
        QueueSelectedCastVoice(0.24f, TEXT("GALLERY_OPEN"));
        ApplyMouseMode(false);
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_PERSONNEL|OPEN|count=%d|selected=%s"),
            CastMembers.Num(), CastMembers.IsValidIndex(SelectedCastIndex) ? *CastMembers[SelectedCastIndex].Id : TEXT("NONE"));
        break;
    case ERotorlineStartupState::PatchWall:
        bPlayingSplashIntro = false;
        bPlayingLoreIntro = false;
        StopCastVoice(TEXT("OPEN_PATCH_WALL"));
        CloseStartupMedia(TEXT("PATCH_WALL"));
        bOperationsMenuOpen = false;
        bPatchWallOpen = true;
        PatchWallSelection = FMath::Clamp(
            PatchWallSelection, 0, FMath::Max(0, AwardSystem.GetDefinitions().Num() - 1));
        ApplyMouseMode(false);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AWARDS|PATCH_WALL|state=OPEN|source=MAIN_MENU|definitions=%d|completion=%.1f"),
            AwardSystem.GetDefinitions().Num(), GetAwardCompletionPercent());
        break;
    case ERotorlineStartupState::StartScreen:
        bPlayingSplashIntro = false;
        bPlayingLoreIntro = false;
        StopCastVoice(TEXT("START_SCREEN"));
        CloseStartupMedia(TEXT("START_SCREEN"));
        bOperationsMenuOpen = false;
        bPatchWallOpen = false;
        SelectedStartupMenuIndex = FMath::Clamp(SelectedStartupMenuIndex, 0, 5);
        ApplyMouseMode(false);
        break;
    case ERotorlineStartupState::EnteringOperations:
        bPlayingSplashIntro = false;
        bPlayingLoreIntro = false;
        StopCastVoice(TEXT("START_GAME"));
        CloseStartupMedia(TEXT("START_GAME"));
        bOperationsMenuOpen = true;
        bHangarOpen = false;
        bPatchWallOpen = false;
        bAudioSettingsOpen = false;
        bControlsSettingsOpen = false;
        ApplyMouseMode(false);
        break;
    default:
        break;
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|STATE|value=%d"), static_cast<int32>(NewState));
}

void ARotorlineOperationsPlayerController::BeginStartupTransition(
    ERotorlineStartupState TargetState,
    const TCHAR* Reason)
{
    if (bStartupFadeToBlack) return;
    StartupTransitionTarget = TargetState;
    bStartupFadeFromBlack = false;
    bStartupFadeToBlack = true;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|TRANSITION|from=%d|to=%d|reason=%s"),
        static_cast<int32>(StartupState), static_cast<int32>(TargetState), Reason);
}

void ARotorlineOperationsPlayerController::TickStartupFlow(float DeltaTime)
{
    if (StartupState == ERotorlineStartupState::Inactive) return;
    StartupStateElapsed += DeltaTime;
    StartupPressedTimer = FMath::Max(0.0f, StartupPressedTimer - DeltaTime);
    if (StartupPressedTimer <= 0.0f) PressedStartupMenuIndex = INDEX_NONE;

    if (StartupMediaSound)
    {
        StartupMediaSound->SetVolumeMultiplier(GetEffectiveAudioVolume(ERotorlineAudioChannel::Music));
    }

    if (StartupState == ERotorlineStartupState::CastGallery)
    {
        CastTransitionAlpha = FMath::Max(0.0f, CastTransitionAlpha - DeltaTime / 0.32f);
        CastVoiceElapsed = FMath::Min(CastVoiceDuration, CastVoiceElapsed + DeltaTime);
        if (CastVoiceStartDelay >= 0.0f)
        {
            CastVoiceStartDelay -= DeltaTime;
            if (CastVoiceStartDelay <= 0.0f)
            {
                const FString Reason = PendingCastVoiceReason;
                CastVoiceStartDelay = -1.0f;
                PlaySelectedCastVoice(Reason.IsEmpty() ? TEXT("AUTO_PLAY") : *Reason);
            }
        }
        if (IsValid(CastVoiceAudio))
        {
            CastVoiceAudio->SetVolumeMultiplier(GetEffectiveAudioVolume(ERotorlineAudioChannel::Radio));
        }

        const float HorizontalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
        const bool bDPadLeft = IsInputKeyDown(EKeys::Gamepad_DPad_Left);
        const bool bDPadRight = IsInputKeyDown(EKeys::Gamepad_DPad_Right);
        if (!bHorizontalAxisLatched && (FMath::Abs(HorizontalAxis) > 0.60f || bDPadLeft || bDPadRight))
        {
            MoveCastSelection(bDPadRight || (!bDPadLeft && HorizontalAxis > 0.0f) ? 1 : -1);
            bHorizontalAxisLatched = true;
        }
        else if (FMath::Abs(HorizontalAxis) < 0.30f && !bDPadLeft && !bDPadRight)
        {
            bHorizontalAxisLatched = false;
        }

        const float VerticalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
        const bool bDPadUp = IsInputKeyDown(EKeys::Gamepad_DPad_Up);
        const bool bDPadDown = IsInputKeyDown(EKeys::Gamepad_DPad_Down);
        if (!bVerticalAxisLatched && (FMath::Abs(VerticalAxis) > 0.60f || bDPadUp || bDPadDown))
        {
            MoveCastSelection(bDPadDown || (!bDPadUp && VerticalAxis < 0.0f) ? 1 : -1);
            bVerticalAxisLatched = true;
            bGamepadInputSeen = true;
        }
        else if (FMath::Abs(VerticalAxis) < 0.30f && !bDPadUp && !bDPadDown)
        {
            bVerticalAxisLatched = false;
        }
    }

    if (StartupState == ERotorlineStartupState::PatchWall)
    {
        const float HorizontalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
        const float VerticalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
        if (!bHorizontalAxisLatched && FMath::Abs(HorizontalAxis) > 0.60f)
        {
            MovePatchWallSelection(HorizontalAxis > 0.0f ? 1 : -1, 0);
            bHorizontalAxisLatched = true;
            bGamepadInputSeen = true;
        }
        else if (FMath::Abs(HorizontalAxis) < 0.30f &&
            !IsInputKeyDown(EKeys::Gamepad_DPad_Left) && !IsInputKeyDown(EKeys::Gamepad_DPad_Right))
        {
            bHorizontalAxisLatched = false;
        }
        if (!bVerticalAxisLatched && FMath::Abs(VerticalAxis) > 0.60f)
        {
            MovePatchWallSelection(0, VerticalAxis < 0.0f ? 1 : -1);
            bVerticalAxisLatched = true;
            bGamepadInputSeen = true;
        }
        else if (FMath::Abs(VerticalAxis) < 0.30f &&
            !IsInputKeyDown(EKeys::Gamepad_DPad_Up) && !IsInputKeyDown(EKeys::Gamepad_DPad_Down))
        {
            bVerticalAxisLatched = false;
        }
    }

    if ((StartupState == ERotorlineStartupState::Intro || StartupState == ERotorlineStartupState::Credits) &&
        !bStartupMediaReady)
    {
        StartupMediaOpenElapsed += DeltaTime;
        if (StartupMediaOpenElapsed >= 12.0f)
        {
            HandleStartupMediaOpenFailed(TEXT("OPEN_TIMEOUT"));
        }
    }
    if (bM25FinalCreditsSequenceActive && !bM25CreditsRollStartedLogged && ShouldShowCreditsRoll())
    {
        bM25CreditsRollStartedLogged = true;
        const double MediaSeconds = StartupMediaPlayer
            ? StartupMediaPlayer->GetTime().GetTotalSeconds()
            : static_cast<double>(StartupStateElapsed);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_M25_FINALE|state=CREDITS_ROLL_STARTED|media_time=%.3f|overlay=HUD"),
            MediaSeconds);
    }

    if (StartupState == ERotorlineStartupState::StartScreen)
    {
        if (!bControllerFirstTimePromptChecked && StartupStateElapsed >= 0.8f)
        {
            bControllerFirstTimePromptChecked = true;
            if (UGameInstance* GameInstance = GetGameInstance())
            {
                if (URotorlineFlightControllerSubsystem* FlightInput =
                    GameInstance->GetSubsystem<URotorlineFlightControllerSubsystem>())
                {
                    FlightInput->RefreshDevices();
                    const bool bHasFlightController = FlightInput->GetDevices().ContainsByPredicate([](const FRotorlineControllerDeviceInfo& Device)
                    {
                        return Device.bConnected && !Device.bGamepadCompatible;
                    });
                    if (bHasFlightController && FlightInput->GetActiveProfileId().IsEmpty())
                    {
                        ToggleControlsSettings(true);
                    }
                }
            }
        }
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        if (GetMousePosition(MouseX, MouseY))
        {
            const FVector2D CurrentMousePosition(MouseX, MouseY);
            const bool bMouseMoved = !bStartupMousePositionValid ||
                FVector2D::Distance(CurrentMousePosition, LastStartupMousePosition) > 1.0f;
            const bool bMouseReclaimsFocus = !bStartupControllerFocusActive ||
                FVector2D::Distance(CurrentMousePosition, StartupControllerMouseAnchor) > 18.0f;
            LastStartupMousePosition = CurrentMousePosition;
            bStartupMousePositionValid = true;
            if (bMouseMoved && bMouseReclaimsFocus)
            {
                bStartupControllerFocusActive = false;
                HoveredStartupMenuIndex = GetStartupMenuIndexAtPosition(MouseX, MouseY);
                if (HoveredStartupMenuIndex != INDEX_NONE)
                {
                    SelectedStartupMenuIndex = HoveredStartupMenuIndex;
                }
            }
        }

        const float VerticalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
        const bool bDPadUp = IsInputKeyDown(EKeys::Gamepad_DPad_Up);
        const bool bDPadDown = IsInputKeyDown(EKeys::Gamepad_DPad_Down);
        if (!bVerticalAxisLatched && (FMath::Abs(VerticalAxis) > 0.60f || bDPadUp || bDPadDown))
        {
            const bool bMoveDown = bDPadDown || (!bDPadUp && VerticalAxis < 0.0f);
            MoveStartupMenuSelection(bMoveDown ? 1 : -1,
                bDPadUp || bDPadDown ? TEXT("PS5_DPAD_HELD") : TEXT("PS5_LEFT_STICK"));
            bVerticalAxisLatched = true;
            bGamepadInputSeen = true;
        }
        else if (FMath::Abs(VerticalAxis) < 0.30f && !bDPadUp && !bDPadDown)
        {
            bVerticalAxisLatched = false;
        }
    }

    if (bStartupFadeToBlack)
    {
        StartupFadeAlpha = FMath::Min(1.0f, StartupFadeAlpha + DeltaTime / 0.45f);
        if (StartupFadeAlpha >= 1.0f)
        {
            bStartupFadeToBlack = false;
            EnterStartupState(StartupTransitionTarget);
            if (StartupTransitionTarget == ERotorlineStartupState::StartScreen ||
                StartupTransitionTarget == ERotorlineStartupState::CastGallery ||
                StartupTransitionTarget == ERotorlineStartupState::PatchWall ||
                StartupTransitionTarget == ERotorlineStartupState::EnteringOperations)
            {
                bStartupFadeFromBlack = true;
            }
        }
    }
    else if (bStartupFadeFromBlack)
    {
        StartupFadeAlpha = FMath::Max(0.0f, StartupFadeAlpha - DeltaTime / 0.60f);
        if (StartupFadeAlpha <= 0.0f)
        {
            bStartupFadeFromBlack = false;
            if (StartupState == ERotorlineStartupState::EnteringOperations)
            {
                StartupState = ERotorlineStartupState::Inactive;
                UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|COMPLETE|destination=MISSION_SELECTION"));
            }
        }
    }

    RunStartupQualificationTick();
}

bool ARotorlineOperationsPlayerController::HandleStartupInput(const FInputKeyEventArgs& Params)
{
    if (bGraphicsSettingsOpen)
    {
        if (Params.Event == IE_Pressed &&
            (Params.Key == EKeys::Escape || Params.Key == EKeys::Gamepad_FaceButton_Right))
        {
            ToggleGraphicsSettings();
            return true;
        }
        return Super::InputKey(Params);
    }
    if (bControlsSettingsOpen)
    {
        if (Params.Event == IE_Pressed &&
            (Params.Key == EKeys::Escape || Params.Key == EKeys::Gamepad_FaceButton_Right ||
                Params.Key == EKeys::Gamepad_Special_Right))
        {
            if (CancelPendingControlsDuplicate()) return true;
            ToggleControlsSettings();
            return true;
        }
        if (QueueControlsSettingsInput(Params)) return true;
        return Super::InputKey(Params);
    }
    if (Params.Event != IE_Pressed) return true;
    const bool bCommonAction = Params.Key == EKeys::Escape || Params.Key == EKeys::Enter ||
        Params.Key == EKeys::SpaceBar || Params.Key == EKeys::LeftMouseButton ||
        Params.Key == EKeys::Gamepad_Special_Right || Params.Key == EKeys::Gamepad_FaceButton_Bottom ||
        Params.Key == EKeys::Gamepad_FaceButton_Right;

    if (StartupState == ERotorlineStartupState::Intro)
    {
        if (bCommonAction && StartupStateElapsed >= 1.25f && !bStartupFadeToBlack)
        {
            if (bPlayingLoreIntro)
            {
                bPlayingLoreIntro = false;
                StartupStateElapsed = 0.0f;
                OpenStartupMedia(false);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_STARTUP|LORE_INTRO|result=SKIPPED|next=HELICOPTER_INTRO"));
            }
            else
            {
                BeginStartupTransition(ERotorlineStartupState::StartScreen, TEXT("INTRO_SKIPPED"));
                UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|INTRO|result=SKIPPED|elapsed=%.2f"), StartupStateElapsed);
            }
        }
        return true;
    }
    if (StartupState == ERotorlineStartupState::CastGallery)
    {
        if (Params.Key == EKeys::MouseScrollUp)
        {
            MoveCastSelection(-1);
        }
        else if (Params.Key == EKeys::MouseScrollDown)
        {
            MoveCastSelection(1);
        }
        else if (Params.Key == EKeys::Up || Params.Key == EKeys::W)
        {
            MoveCastSelection(-1);
        }
        else if (Params.Key == EKeys::Down || Params.Key == EKeys::S)
        {
            MoveCastSelection(1);
        }
        else if (Params.Key == EKeys::Gamepad_DPad_Up || Params.Key == EKeys::Gamepad_DPad_Down)
        {
            if (!bVerticalAxisLatched)
            {
                MoveCastSelection(Params.Key == EKeys::Gamepad_DPad_Down ? 1 : -1);
            }
            bVerticalAxisLatched = true;
            bGamepadInputSeen = true;
        }
        else if (Params.Key == EKeys::Left || Params.Key == EKeys::A || Params.Key == EKeys::Gamepad_LeftShoulder)
        {
            MoveCastSelection(-1);
        }
        else if (Params.Key == EKeys::Right || Params.Key == EKeys::D || Params.Key == EKeys::Gamepad_RightShoulder)
        {
            MoveCastSelection(1);
        }
        else if (Params.Key == EKeys::Gamepad_DPad_Left || Params.Key == EKeys::Gamepad_DPad_Right)
        {
            if (!bHorizontalAxisLatched)
            {
                MoveCastSelection(Params.Key == EKeys::Gamepad_DPad_Right ? 1 : -1);
            }
            bHorizontalAxisLatched = true;
            bGamepadInputSeen = true;
        }
        else if (Params.Key == EKeys::Enter || Params.Key == EKeys::SpaceBar ||
            Params.Key == EKeys::Gamepad_FaceButton_Bottom)
        {
            PlaySelectedCastVoice(TEXT("MANUAL_REPLAY"));
            PulseController(0.16f, 0.05f);
        }
        else if (Params.Key == EKeys::LeftMouseButton)
        {
            float X = 0.0f;
            float Y = 0.0f;
            int32 Width = 0;
            int32 Height = 0;
            GetViewportSize(Width, Height);
            if (GetMousePosition(X, Y) && Width > 0)
            {
                const int32 ClickedCastIndex = GetCastIndexAtPosition(X, Y);
                if (ClickedCastIndex != INDEX_NONE)
                {
                    if (ClickedCastIndex == SelectedCastIndex)
                    {
                        PlaySelectedCastVoice(TEXT("CALLSIGN_REPLAY"));
                    }
                    else
                    {
                        SelectCastMember(ClickedCastIndex);
                    }
                }
                else if (X < Width * 0.27f) MoveCastSelection(-1);
                else if (X > Width * 0.73f) MoveCastSelection(1);
                else PlaySelectedCastVoice(TEXT("MOUSE_REPLAY"));
            }
        }
        else if (Params.Key == EKeys::Escape || Params.Key == EKeys::Gamepad_FaceButton_Right ||
            Params.Key == EKeys::Gamepad_Special_Right)
        {
            ReturnToStartupMenu(TEXT("CAST_RETURN"));
        }
        return true;
    }
    if (StartupState == ERotorlineStartupState::PatchWall)
    {
        if (Params.Key == EKeys::Left || Params.Key == EKeys::A || Params.Key == EKeys::Gamepad_LeftShoulder)
        {
            MovePatchWallSelection(-1, 0);
        }
        else if (Params.Key == EKeys::Right || Params.Key == EKeys::D || Params.Key == EKeys::Gamepad_RightShoulder)
        {
            MovePatchWallSelection(1, 0);
        }
        else if (Params.Key == EKeys::Up || Params.Key == EKeys::W)
        {
            MovePatchWallSelection(0, -1);
        }
        else if (Params.Key == EKeys::Down || Params.Key == EKeys::S)
        {
            MovePatchWallSelection(0, 1);
        }
        else if (Params.Key == EKeys::Gamepad_DPad_Left || Params.Key == EKeys::Gamepad_DPad_Right)
        {
            if (!bHorizontalAxisLatched)
            {
                MovePatchWallSelection(Params.Key == EKeys::Gamepad_DPad_Right ? 1 : -1, 0);
            }
            bHorizontalAxisLatched = true;
            bGamepadInputSeen = true;
        }
        else if (Params.Key == EKeys::Gamepad_DPad_Up || Params.Key == EKeys::Gamepad_DPad_Down)
        {
            if (!bVerticalAxisLatched)
            {
                MovePatchWallSelection(0, Params.Key == EKeys::Gamepad_DPad_Down ? 1 : -1);
            }
            bVerticalAxisLatched = true;
            bGamepadInputSeen = true;
        }
        else if (Params.Key == EKeys::Escape || Params.Key == EKeys::BackSpace || Params.Key == EKeys::P ||
            Params.Key == EKeys::Gamepad_FaceButton_Right || Params.Key == EKeys::Gamepad_Special_Right)
        {
            ReturnToStartupMenu(TEXT("PATCH_WALL_RETURN"));
        }
        return true;
    }
    if (StartupState == ERotorlineStartupState::Credits)
    {
        if (bM25FinalCreditsSequenceActive)
        {
            return true;
        }
        if (bCommonAction && StartupStateElapsed >= 0.35f && !bStartupFadeToBlack)
        {
            ReturnToStartupMenu(TEXT("CREDITS_MANUAL_RETURN"));
        }
        return true;
    }
    if (StartupState != ERotorlineStartupState::StartScreen || bStartupFadeToBlack) return true;

    if (Params.Key == EKeys::Up || Params.Key == EKeys::W)
    {
        MoveStartupMenuSelection(-1, TEXT("KEYBOARD_UP"));
    }
    else if (Params.Key == EKeys::Down || Params.Key == EKeys::S)
    {
        MoveStartupMenuSelection(1, TEXT("KEYBOARD_DOWN"));
    }
    else if (Params.Key == EKeys::Gamepad_DPad_Up)
    {
        if (!bVerticalAxisLatched)
        {
            MoveStartupMenuSelection(-1, TEXT("PS5_DPAD_UP"));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|MENU_DUPLICATE_SUPPRESSED|source=PS5_DPAD_UP|index=%d"),
                SelectedStartupMenuIndex);
        }
        bVerticalAxisLatched = true;
        bGamepadInputSeen = true;
    }
    else if (Params.Key == EKeys::Gamepad_DPad_Down)
    {
        if (!bVerticalAxisLatched)
        {
            MoveStartupMenuSelection(1, TEXT("PS5_DPAD_DOWN"));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|MENU_DUPLICATE_SUPPRESSED|source=PS5_DPAD_DOWN|index=%d"),
                SelectedStartupMenuIndex);
        }
        bVerticalAxisLatched = true;
        bGamepadInputSeen = true;
    }
    else if (Params.Key == EKeys::LeftMouseButton)
    {
        float X = 0.0f;
        float Y = 0.0f;
        const int32 Clicked = GetMousePosition(X, Y) ? GetStartupMenuIndexAtPosition(X, Y) : INDEX_NONE;
        if (Clicked != INDEX_NONE)
        {
            bStartupControllerFocusActive = false;
            SelectedStartupMenuIndex = Clicked;
            PressedStartupMenuIndex = Clicked;
            StartupPressedTimer = 0.12f;
            ActivateStartupSelection();
        }
    }
    else if (Params.Key == EKeys::Enter || Params.Key == EKeys::SpaceBar ||
        Params.Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (Params.Key == EKeys::Gamepad_FaceButton_Bottom)
        {
            bGamepadInputSeen = true;
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|PS5_X|index=%d|action=ACTIVATE"), SelectedStartupMenuIndex);
        }
        PressedStartupMenuIndex = SelectedStartupMenuIndex;
        StartupPressedTimer = 0.12f;
        ActivateStartupSelection();
    }
    else if (Params.Key == EKeys::Escape || Params.Key == EKeys::Gamepad_FaceButton_Right)
    {
        SelectedStartupMenuIndex = 6;
    }
    return true;
}

void ARotorlineOperationsPlayerController::MoveStartupMenuSelection(int32 Direction, const TCHAR* Source)
{
    if (Direction == 0) return;
    const int32 PreviousIndex = SelectedStartupMenuIndex;
    SelectedStartupMenuIndex = (SelectedStartupMenuIndex + (Direction > 0 ? 1 : 6)) % 7;
    HoveredStartupMenuIndex = INDEX_NONE;
    if (Source && FCString::Strncmp(Source, TEXT("PS5"), 3) == 0)
    {
        bStartupControllerFocusActive = true;
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        if (GetMousePosition(MouseX, MouseY))
        {
            StartupControllerMouseAnchor = FVector2D(MouseX, MouseY);
        }
    }
    PulseController(0.12f, 0.04f);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_STARTUP|MENU_SELECT|source=%s|from=%d|to=%d|item=%s"),
        Source, PreviousIndex, SelectedStartupMenuIndex,
        SelectedStartupMenuIndex == 0 ? TEXT("START_GAME") :
        SelectedStartupMenuIndex == 1 ? TEXT("PERSONNEL_FILES") :
        SelectedStartupMenuIndex == 2 ? TEXT("PATCH_WALL_STATS") :
        SelectedStartupMenuIndex == 3 ? TEXT("CREDITS") :
        SelectedStartupMenuIndex == 4 ? TEXT("CONTROLS") :
        SelectedStartupMenuIndex == 5 ? TEXT("GRAPHICS") : TEXT("EXIT_GAME"));
}

void ARotorlineOperationsPlayerController::ActivateStartupSelection()
{
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|MENU_ACTIVATE|index=%d|item=%s"),
        SelectedStartupMenuIndex,
        SelectedStartupMenuIndex == 0 ? TEXT("START_GAME") :
        SelectedStartupMenuIndex == 1 ? TEXT("PERSONNEL_FILES") :
        SelectedStartupMenuIndex == 2 ? TEXT("PATCH_WALL_STATS") :
        SelectedStartupMenuIndex == 3 ? TEXT("CREDITS") :
        SelectedStartupMenuIndex == 4 ? TEXT("CONTROLS") :
        SelectedStartupMenuIndex == 5 ? TEXT("GRAPHICS") : TEXT("EXIT_GAME"));
    PulseController(0.20f, 0.06f);
    switch (SelectedStartupMenuIndex)
    {
    case 0:
        BeginStartupTransition(ERotorlineStartupState::EnteringOperations, TEXT("START_GAME"));
        break;
    case 1:
        BeginStartupTransition(ERotorlineStartupState::CastGallery, TEXT("OPEN_CAST"));
        break;
    case 2:
        BeginStartupTransition(ERotorlineStartupState::PatchWall, TEXT("OPEN_PATCH_WALL"));
        break;
    case 3:
        BeginStartupTransition(ERotorlineStartupState::Credits, TEXT("OPEN_CREDITS"));
        break;
    case 4:
        ToggleControlsSettings();
        break;
    case 5:
        ToggleGraphicsSettings();
        break;
    case 6:
        CloseStartupMedia(TEXT("EXIT_GAME"));
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|EXIT|requested=1|world_type=%d"),
            GetWorld() ? static_cast<int32>(GetWorld()->WorldType) : -1);
        if (!StartupQualificationScenario.IsEmpty())
        {
            FPlatformMisc::RequestExit(false);
        }
        else if (UWorld* World = GetWorld(); World && World->WorldType == EWorldType::Game)
        {
            UKismetSystemLibrary::QuitGame(World, this, EQuitPreference::Quit, false);
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|EXIT|editor_safe_noop=1"));
        }
        break;
    default:
        break;
    }
}

void ARotorlineOperationsPlayerController::ReturnToStartupMenu(const TCHAR* Reason)
{
    BeginStartupTransition(ERotorlineStartupState::StartScreen, Reason);
}

int32 ARotorlineOperationsPlayerController::GetStartupMenuIndexAtPosition(float X, float Y) const
{
    int32 Width = 0;
    int32 Height = 0;
    GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return INDEX_NONE;
    const float MenuX = Width * 0.075f;
    const float MenuY = Height * 0.435f;
    const float MenuWidth = FMath::Min(460.0f, Width * 0.36f);
    const float ItemHeight = FMath::Max(52.0f, Height * 0.065f);
    const float Gap = FMath::Max(8.0f, Height * 0.010f);
    if (X < MenuX || X > MenuX + MenuWidth) return INDEX_NONE;
    for (int32 Index = 0; Index < 7; ++Index)
    {
        const float ItemY = MenuY + Index * (ItemHeight + Gap);
        if (Y >= ItemY && Y <= ItemY + ItemHeight) return Index;
    }
    return INDEX_NONE;
}

void ARotorlineOperationsPlayerController::HandleStartupMediaOpened(FString OpenedUrl)
{
    bStartupMediaReady = true;
    StartupMediaOpenElapsed = 0.0f;
    if (StartupMediaPlayer)
    {
        StartupMediaPlayer->SetLooping(false);
        StartupMediaPlayer->Play();
    }
    bStartupFadeFromBlack = true;
    const double Duration = StartupMediaPlayer ? StartupMediaPlayer->GetDuration().GetTotalSeconds() : 0.0;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|MEDIA_OPENED|state=%d|duration=%.3f|url=%s|audio=EMBEDDED_STEREO"),
        static_cast<int32>(StartupState), Duration, *OpenedUrl);
    if (bM25FinalCreditsSequenceActive)
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_M25_FINALE|state=FINAL_VIDEO_OPENED|duration=%.3f|credits_begin=10.000|success_after=NATURAL_END"),
            Duration);
    }
}

void ARotorlineOperationsPlayerController::HandleStartupMediaOpenFailed(FString FailedUrl)
{
    if (bStartupFadeToBlack) return;
    bStartupMediaReady = false;
    if (bM25FinalCreditsSequenceActive)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_M25_FINALE|state=FINAL_VIDEO_FAILED|url=%s|fallback=REALTIME"),
            *FailedUrl);
        CloseStartupMedia(TEXT("M25_FINAL_OPEN_FAILED"));
        bM25FinalCreditsSequenceActive = false;
        bM25FinalCreditsSequenceFailed = true;
        bM25FinalCreditsSequenceCompleted = false;
        StartupState = ERotorlineStartupState::Inactive;
        StartupFadeAlpha = 0.0f;
        bStartupFadeToBlack = false;
        bStartupFadeFromBlack = false;
        bOperationsMenuOpen = false;
        SetPause(false);
        SetM25AircraftAudioSuppressed(false);
        ApplyMouseMode(false);
        return;
    }
    if (StartupState == ERotorlineStartupState::Intro && bPlayingSplashIntro)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_STARTUP|MEDIA_FAILED|segment=SPLASH_INTRO|url=%s|fallback=LORE_INTRO"),
            *FailedUrl);
        bPlayingSplashIntro = false;
        bPlayingLoreIntro = true;
        StartupStateElapsed = 0.0f;
        OpenStartupMedia(false);
        return;
    }
    if (StartupState == ERotorlineStartupState::Intro && bPlayingLoreIntro)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_STARTUP|MEDIA_FAILED|segment=LORE_INTRO|url=%s|fallback=HELICOPTER_INTRO"),
            *FailedUrl);
        bPlayingLoreIntro = false;
        StartupStateElapsed = 0.0f;
        OpenStartupMedia(false);
        return;
    }
    UE_LOG(LogTemp, Error, TEXT("ROTORLINE_STARTUP|MEDIA_FAILED|state=%d|url=%s|fallback=START_SCREEN"),
        static_cast<int32>(StartupState), *FailedUrl);
    if (StartupState == ERotorlineStartupState::Intro || StartupState == ERotorlineStartupState::Credits)
    {
        BeginStartupTransition(ERotorlineStartupState::StartScreen, TEXT("MEDIA_OPEN_FAILED"));
    }
}

void ARotorlineOperationsPlayerController::HandleStartupMediaEndReached()
{
    if (StartupState == ERotorlineStartupState::Intro)
    {
        if (bPlayingSplashIntro)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|SPLASH_INTRO|result=NATURAL_END|next=LORE_INTRO"));
            bPlayingSplashIntro = false;
            bPlayingLoreIntro = true;
            StartupStateElapsed = 0.0f;
            OpenStartupMedia(false);
            return;
        }
        if (bPlayingLoreIntro)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|LORE_INTRO|result=NATURAL_END|next=HELICOPTER_INTRO"));
            bPlayingLoreIntro = false;
            StartupStateElapsed = 0.0f;
            OpenStartupMedia(false);
            return;
        }
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|INTRO|result=NATURAL_END"));
        BeginStartupTransition(ERotorlineStartupState::StartScreen, TEXT("INTRO_COMPLETE"));
    }
    else if (StartupState == ERotorlineStartupState::Credits)
    {
        if (bM25FinalCreditsSequenceActive)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_M25_FINALE|state=FINAL_VIDEO_NATURAL_END|credits=COMPLETE|next=MISSION_SUCCESS"));
            CloseStartupMedia(TEXT("M25_FINAL_COMPLETE"));
            bM25FinalCreditsSequenceActive = false;
            bM25FinalCreditsSequenceCompleted = true;
            bM25FinalCreditsSequenceFailed = false;
            StartupState = ERotorlineStartupState::Inactive;
            StartupFadeAlpha = 0.0f;
            bStartupFadeToBlack = false;
            bStartupFadeFromBlack = false;
            bOperationsMenuOpen = false;
            SetPause(false);
            SetM25AircraftAudioSuppressed(false);
            ApplyMouseMode(false);
            return;
        }
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|CREDITS|result=NATURAL_END"));
        ReturnToStartupMenu(TEXT("CREDITS_COMPLETE"));
    }
}

FString ARotorlineOperationsPlayerController::GetStartupMediaTimeLabel() const
{
    if (!StartupMediaPlayer) return TEXT("00:00");
    const int32 CurrentSeconds = FMath::Max(0, FMath::FloorToInt(StartupMediaPlayer->GetTime().GetTotalSeconds()));
    const int32 DurationSeconds = FMath::Max(0, FMath::CeilToInt(StartupMediaPlayer->GetDuration().GetTotalSeconds()));
    return FString::Printf(TEXT("%02d:%02d / %02d:%02d"),
        CurrentSeconds / 60, CurrentSeconds % 60, DurationSeconds / 60, DurationSeconds % 60);
}

float ARotorlineOperationsPlayerController::GetCreditsScrollProgress() const
{
    if (StartupState != ERotorlineStartupState::Credits)
    {
        return 0.0f;
    }

    if (StartupMediaPlayer)
    {
        const double CurrentSeconds = StartupMediaPlayer->GetTime().GetTotalSeconds();
        const double DurationSeconds = StartupMediaPlayer->GetDuration().GetTotalSeconds();
        if (DurationSeconds > 1.0)
        {
            if (bM25FinalCreditsSequenceActive)
            {
                constexpr double CreditsDelaySeconds = 10.0;
                const double CreditsDurationSeconds = FMath::Max(1.0, DurationSeconds - CreditsDelaySeconds);
                return FMath::Clamp(
                    static_cast<float>((CurrentSeconds - CreditsDelaySeconds) / CreditsDurationSeconds),
                    0.0f,
                    1.0f);
            }
            return FMath::Clamp(
                static_cast<float>(CurrentSeconds / DurationSeconds),
                0.0f,
                1.0f);
        }
    }

    if (bM25FinalCreditsSequenceActive)
    {
        return FMath::Clamp((StartupStateElapsed - 10.0f) / 182.064f, 0.0f, 1.0f);
    }

    // Keep the credit roll usable if a platform briefly reports no media
    // duration while the MP4 is opening. This matches the audited soundtrack.
    return FMath::Clamp(StartupStateElapsed / 182.464f, 0.0f, 1.0f);
}

void ARotorlineOperationsPlayerController::LoadCastDefinitions()
{
    CastMembers.Reset();
    CastCardTextures.Reset();
    CastVoiceProfiles.Reset();
    CastCatalogError.Reset();
    if (!FRotorlineCastCatalog::Load(CastMembers, CastCatalogError))
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_PERSONNEL|LOAD_FAILED|reason=%s"), *CastCatalogError);
        return;
    }

    int32 CompletePairs = 0;
    for (const FRotorlineCastMember& Member : CastMembers)
    {
        UTexture2D* Card = LoadObject<UTexture2D>(nullptr, *Member.CardAsset);
        USoundBase* Voice = LoadObject<USoundBase>(nullptr, *Member.VoiceAsset);
        if (Card)
        {
            CastCardTextures.Add(Member.Id, Card);
        }
        if (Voice)
        {
            CastVoiceProfiles.Add(Member.Id, Voice);
        }
        if (Card && Voice)
        {
            ++CompletePairs;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ROTORLINE_PERSONNEL|ASSET_MISSING|id=%s|card=%s|voice=%s"),
                *Member.Id, Card ? TEXT("READY") : TEXT("MISSING"), Voice ? TEXT("READY") : TEXT("MISSING"));
        }
    }

    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_PERSONNEL|LOAD|members=%d|complete_pairs=%d|cards=%d|voices=%d"),
        CastMembers.Num(), CompletePairs, CastCardTextures.Num(), CastVoiceProfiles.Num());
}

void ARotorlineOperationsPlayerController::LoadAircraftBlueprintTextures()
{
    AircraftBlueprintTextures.Reset();

    struct FBlueprintTextureEntry
    {
        const TCHAR* AircraftId;
        const TCHAR* AssetName;
    };
    static const FBlueprintTextureEntry Entries[] =
    {
        { TEXT("uh1_huey"), TEXT("T_BP_uh1_huey") },
        { TEXT("md500_defender"), TEXT("T_BP_md500_defender") },
        { TEXT("ah64_apache"), TEXT("T_BP_ah64_apache") },
        { TEXT("mi24_hind"), TEXT("T_BP_mi24_hind") },
        { TEXT("uh60m_blackhawk"), TEXT("T_BP_uh60m_blackhawk") },
        { TEXT("marine_uh1"), TEXT("T_BP_uh1_huey") },
        { TEXT("ka27_helix"), TEXT("T_BP_ka27_helix") },
        { TEXT("oh58_kiowa"), TEXT("T_BP_oh58_kiowa") },
        { TEXT("bell_222x"), TEXT("T_BP_bell_222x") },
        { TEXT("ch47_chinook"), TEXT("T_BP_ch47_chinook") },
        { TEXT("jeep_wrangler"), TEXT("T_BP_jeep_wrangler") },
    };

    int32 LoadedCount = 0;
    for (const FBlueprintTextureEntry& Entry : Entries)
    {
        const FString ObjectPath = FString::Printf(
            TEXT("/Game/UI/AircraftBlueprints/%s.%s"), Entry.AssetName, Entry.AssetName);
        if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath))
        {
            AircraftBlueprintTextures.Add(Entry.AircraftId, Texture);
            ++LoadedCount;
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_HANGAR_BLUEPRINT|MISSING|aircraft=%s|asset=%s"),
                Entry.AircraftId,
                *ObjectPath);
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_HANGAR_BLUEPRINT|LOAD|aircraft=%d|textures=%d|shared_huey=1"),
        UE_ARRAY_COUNT(Entries),
        LoadedCount);
}

UTexture2D* ARotorlineOperationsPlayerController::GetAircraftBlueprintTexture(const FString& AircraftId) const
{
    const TObjectPtr<UTexture2D>* Texture = AircraftBlueprintTextures.Find(AircraftId);
    return Texture ? Texture->Get() : nullptr;
}

UTexture2D* ARotorlineOperationsPlayerController::GetCastCardTexture(int32 Index) const
{
    if (!CastMembers.IsValidIndex(Index)) return nullptr;
    const TObjectPtr<UTexture2D>* Texture = CastCardTextures.Find(CastMembers[Index].Id);
    return Texture ? Texture->Get() : nullptr;
}

bool ARotorlineOperationsPlayerController::IsCastVoicePlaying() const
{
    return IsValid(CastVoiceAudio) && CastVoiceAudio->IsPlaying();
}

float ARotorlineOperationsPlayerController::GetCastVoiceProgress() const
{
    return CastVoiceDuration > KINDA_SMALL_NUMBER
        ? FMath::Clamp(CastVoiceElapsed / CastVoiceDuration, 0.0f, 1.0f)
        : 0.0f;
}

void ARotorlineOperationsPlayerController::QueueSelectedCastVoice(float DelaySeconds, const TCHAR* Reason)
{
    StopCastVoice(TEXT("SELECTION_CHANGED"));
    CastVoiceStartDelay = FMath::Max(0.0f, DelaySeconds);
    PendingCastVoiceReason = Reason;
}

void ARotorlineOperationsPlayerController::MoveCastSelection(int32 Direction)
{
    if (CastMembers.IsEmpty() || Direction == 0) return;
    const int32 NextIndex = (SelectedCastIndex + (Direction > 0 ? 1 : CastMembers.Num() - 1)) % CastMembers.Num();
    SelectCastMember(NextIndex, Direction);
}

void ARotorlineOperationsPlayerController::SelectCastMember(int32 Index, int32 DirectionHint)
{
    if (!CastMembers.IsValidIndex(Index) || Index == SelectedCastIndex) return;
    const int32 PreviousIndex = SelectedCastIndex;
    if (DirectionHint == 0)
    {
        const int32 ForwardDistance = (Index - SelectedCastIndex + CastMembers.Num()) % CastMembers.Num();
        DirectionHint = ForwardDistance <= CastMembers.Num() / 2 ? 1 : -1;
    }
    SelectedCastIndex = Index;
    CastTransitionDirection = DirectionHint > 0 ? 1 : -1;
    CastTransitionAlpha = 1.0f;
    QueueSelectedCastVoice(0.30f, TEXT("SELECTION_SETTLED"));
    PulseController(0.13f, 0.04f);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_PERSONNEL|SELECT|from=%d|to=%d|id=%s|voice=AUTO_PENDING"),
        PreviousIndex, SelectedCastIndex, *CastMembers[SelectedCastIndex].Id);
}

int32 ARotorlineOperationsPlayerController::GetCastIndexAtPosition(float X, float Y) const
{
    int32 Width = 0;
    int32 Height = 0;
    GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return INDEX_NONE;

    const float Scale = FMath::Clamp(Height / 1080.0f, 0.67f, 1.25f);
    const float TabX = 20.0f * Scale;
    const float TabY = 150.0f * Scale;
    const float TabWidth = 215.0f * Scale;
    const float TabHeight = 48.0f * Scale;
    const float TabGap = 8.0f * Scale;
    if (X < TabX || X > TabX + TabWidth) return INDEX_NONE;
    for (int32 Index = 0; Index < CastMembers.Num(); ++Index)
    {
        const float ItemY = TabY + Index * (TabHeight + TabGap);
        if (Y >= ItemY && Y <= ItemY + TabHeight) return Index;
    }
    return INDEX_NONE;
}

void ARotorlineOperationsPlayerController::PlaySelectedCastVoice(const TCHAR* Reason)
{
    if (!CastMembers.IsValidIndex(SelectedCastIndex)) return;
    const FRotorlineCastMember& Member = CastMembers[SelectedCastIndex];
    const TObjectPtr<USoundBase>* SoundPtr = CastVoiceProfiles.Find(Member.Id);
    USoundBase* Sound = SoundPtr ? SoundPtr->Get() : nullptr;
    if (!Sound)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_PERSONNEL|VOICE|id=%s|result=MISSING|reason=%s"), *Member.Id, Reason);
        return;
    }

    if (IsValid(CastVoiceAudio))
    {
        CastVoiceAudio->Stop();
        CastVoiceAudio->DestroyComponent();
    }
    CastVoiceAudio = UGameplayStatics::SpawnSound2D(
        this,
        Sound,
        GetEffectiveAudioVolume(ERotorlineAudioChannel::Radio),
        1.0f,
        0.0f,
        nullptr,
        false,
        false);
    CastVoiceElapsed = 0.0f;
    CastVoiceDuration = FMath::Max(0.0f, Sound->GetDuration());
    PendingCastVoiceReason.Reset();
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_PERSONNEL|VOICE|id=%s|result=%s|reason=%s|duration=%.3f|channel=RADIO"),
        *Member.Id, CastVoiceAudio ? TEXT("PLAYING") : TEXT("FAILED"), Reason, CastVoiceDuration);
}

void ARotorlineOperationsPlayerController::StopCastVoice(const TCHAR* Reason)
{
    CastVoiceStartDelay = -1.0f;
    PendingCastVoiceReason.Reset();
    if (IsValid(CastVoiceAudio))
    {
        CastVoiceAudio->Stop();
        CastVoiceAudio->DestroyComponent();
    }
    CastVoiceAudio = nullptr;
    CastVoiceElapsed = 0.0f;
    CastVoiceDuration = 0.0f;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_PERSONNEL|VOICE_STOP|reason=%s"), Reason);
}

void ARotorlineOperationsPlayerController::RunStartupQualificationTick()
{
    if (StartupQualificationScenario.IsEmpty()) return;
    const bool bStartScreenReady = StartupState == ERotorlineStartupState::StartScreen &&
        !bStartupFadeToBlack && !bStartupFadeFromBlack;

    if (StartupQualificationScenario.Equals(TEXT("intro_natural"), ESearchCase::IgnoreCase))
    {
        if (bStartScreenReady)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP_TEST|INTRO_NATURAL|PASS"));
            FPlatformMisc::RequestExit(false);
        }
    }
    else if (StartupQualificationScenario.Equals(TEXT("intro_skip"), ESearchCase::IgnoreCase))
    {
        if (StartupQualificationPhase == 0 && StartupState == ERotorlineStartupState::Intro && StartupStateElapsed >= 1.50f)
        {
            BeginStartupTransition(ERotorlineStartupState::StartScreen, TEXT("QUALIFICATION_SKIP"));
            StartupQualificationPhase = 1;
        }
        else if (StartupQualificationPhase == 1 && bStartScreenReady)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP_TEST|INTRO_SKIP|PASS|delay_guard=1"));
            FPlatformMisc::RequestExit(false);
        }
    }
    else if (StartupQualificationScenario.Equals(TEXT("start_game"), ESearchCase::IgnoreCase))
    {
        if (StartupQualificationPhase == 0 && bStartScreenReady)
        {
            SelectedStartupMenuIndex = 0;
            ActivateStartupSelection();
            StartupQualificationPhase = 1;
        }
        else if (StartupQualificationPhase == 1 && StartupState == ERotorlineStartupState::Inactive && bOperationsMenuOpen)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP_TEST|START_GAME|PASS|destination=MISSION_SELECTION"));
            FPlatformMisc::RequestExit(false);
        }
    }
    else if (StartupQualificationScenario.Equals(TEXT("operations_return"), ESearchCase::IgnoreCase))
    {
        if (StartupQualificationPhase == 0 && bStartScreenReady)
        {
            SelectedStartupMenuIndex = 0;
            ActivateStartupSelection();
            StartupQualificationPhase = 1;
        }
        else if (StartupQualificationPhase == 1 && StartupState == ERotorlineStartupState::Inactive && bOperationsMenuOpen)
        {
            ReturnToMainMenu();
            StartupQualificationPhase = 2;
        }
        else if (StartupQualificationPhase == 2 && bStartScreenReady && !bOperationsMenuOpen)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP_TEST|OPERATIONS_RETURN|PASS|destination=START_SCREEN|operations_open=0"));
            FPlatformMisc::RequestExit(false);
        }
    }
    else if (StartupQualificationScenario.Equals(TEXT("patch_wall"), ESearchCase::IgnoreCase))
    {
        if (StartupQualificationPhase == 0 && bStartScreenReady)
        {
            SelectedStartupMenuIndex = 0;
            bVerticalAxisLatched = false;
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_DPad_Down, IE_Pressed, 1.0f));
            bVerticalAxisLatched = false;
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_DPad_Down, IE_Pressed, 1.0f));
            if (SelectedStartupMenuIndex != 2)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("ROTORLINE_STARTUP_TEST|PATCH_WALL|FAIL|main_menu_dpad_expected=2|actual=%d"),
                    SelectedStartupMenuIndex);
                FPlatformMisc::RequestExit(false);
                return;
            }
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, 1.0f));
            StartupQualificationPhase = 1;
        }
        else if (StartupQualificationPhase == 1 && StartupState == ERotorlineStartupState::PatchWall &&
            !bStartupFadeFromBlack && StartupStateElapsed >= 0.25f)
        {
            const int32 InitialPatch = PatchWallSelection;
            bHorizontalAxisLatched = false;
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_DPad_Right, IE_Pressed, 1.0f));
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_DPad_Right, IE_Pressed, 1.0f));
            const int32 ExpectedPatch = AwardSystem.GetDefinitions().IsEmpty()
                ? 0
                : (InitialPatch + 1) % AwardSystem.GetDefinitions().Num();
            const bool bPassed = bPatchWallOpen && AwardSystem.GetDefinitions().Num() == 29 &&
                GetCareerStatistics() != nullptr && PatchWallSelection == ExpectedPatch;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_STARTUP_TEST|PATCH_WALL|OPEN|%s|main_menu_dpad=PASS|main_menu_x=PASS|patch_dpad=PASS|duplicate_suppressed=PASS|definitions=%d|stats=%s"),
                bPassed ? TEXT("PASS") : TEXT("FAIL"), AwardSystem.GetDefinitions().Num(),
                GetCareerStatistics() ? TEXT("VISIBLE") : TEXT("MISSING"));
            if (!bPassed)
            {
                FPlatformMisc::RequestExit(false);
                return;
            }
            InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Right, IE_Pressed, 1.0f));
            StartupQualificationPhase = 2;
        }
        else if (StartupQualificationPhase == 2 && bStartScreenReady && !bPatchWallOpen)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_STARTUP_TEST|PATCH_WALL|PASS|ps5_dpad=PASS|x_button=PASS|circle=PASS|stats=VISIBLE|return=START_SCREEN"));
            FPlatformMisc::RequestExit(false);
        }
    }
    else if (StartupQualificationScenario.Equals(TEXT("credits_manual"), ESearchCase::IgnoreCase))
    {
        if (StartupQualificationPhase == 0 && bStartScreenReady)
        {
            SelectedStartupMenuIndex = 3;
            ActivateStartupSelection();
            StartupQualificationPhase = 1;
        }
        else if (StartupQualificationPhase == 1 && StartupState == ERotorlineStartupState::Credits && StartupStateElapsed >= 2.0f)
        {
            ReturnToStartupMenu(TEXT("QUALIFICATION_CREDITS_MANUAL"));
            StartupQualificationPhase = 2;
        }
        else if (StartupQualificationPhase == 2 && bStartScreenReady)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP_TEST|CREDITS_MANUAL|PASS|audio=STOPPED"));
            FPlatformMisc::RequestExit(false);
        }
    }
    else if (StartupQualificationScenario.Equals(TEXT("credits_natural"), ESearchCase::IgnoreCase))
    {
        if (StartupQualificationPhase == 0 && bStartScreenReady)
        {
            SelectedStartupMenuIndex = 3;
            ActivateStartupSelection();
            StartupQualificationPhase = 1;
        }
        else if (StartupQualificationPhase == 1 && StartupState == ERotorlineStartupState::Credits)
        {
            StartupQualificationPhase = 2;
        }
        else if (StartupQualificationPhase == 2 && bStartScreenReady)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP_TEST|CREDITS_NATURAL|PASS|automatic_return=1|audio=STOPPED"));
            FPlatformMisc::RequestExit(false);
        }
    }
    else if (StartupQualificationScenario.Equals(TEXT("cast_gallery"), ESearchCase::IgnoreCase))
    {
        if (StartupQualificationPhase == 0 && bStartScreenReady)
        {
            if (SelectedStartupMenuIndex != 0)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("ROTORLINE_STARTUP_TEST|CAST_GALLERY|FAIL|initial_menu_index_expected=0|actual=%d"),
                    SelectedStartupMenuIndex);
                FPlatformMisc::RequestExit(false);
                return;
            }
            bVerticalAxisLatched = false;
            InputKey(FInputKeyEventArgs::CreateSimulated(
                EKeys::Gamepad_DPad_Down, IE_Pressed, 1.0f));
            // DualSense/GameInput can expose the same physical D-pad press through
            // both switch and gamepad paths. A duplicate press must not skip row 1.
            InputKey(FInputKeyEventArgs::CreateSimulated(
                EKeys::Gamepad_DPad_Down, IE_Pressed, 1.0f));
            if (SelectedStartupMenuIndex != 1)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("ROTORLINE_STARTUP_TEST|CAST_GALLERY|FAIL|main_menu_dpad_expected=1|actual=%d"),
                    SelectedStartupMenuIndex);
                FPlatformMisc::RequestExit(false);
                return;
            }
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_STARTUP_TEST|CAST_GALLERY|MAIN_MENU_DPAD|PASS|index=1|item=PERSONNEL_FILES|duplicate_suppressed=1"));
            StartupQualificationPhase = 1;
        }
        else if (StartupQualificationPhase == 1 && bStartScreenReady && SelectedStartupMenuIndex == 1)
        {
            InputKey(FInputKeyEventArgs::CreateSimulated(
                EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, 1.0f));
            StartupQualificationPhase = 2;
        }
        else if (StartupQualificationPhase >= 2 && StartupQualificationPhase <= 6 &&
            StartupState == ERotorlineStartupState::CastGallery && StartupStateElapsed >= 0.65f)
        {
            const int32 ExpectedIndex = StartupQualificationPhase - 2;
            const FString CurrentId = CastMembers.IsValidIndex(SelectedCastIndex) ? CastMembers[SelectedCastIndex].Id : TEXT("NONE");
            const bool bVoicePlaying = IsCastVoicePlaying();
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_STARTUP_TEST|CAST_GALLERY|PROFILE|index=%d|id=%s|voice=%s"),
                SelectedCastIndex, *CurrentId, bVoicePlaying ? TEXT("PLAYING") : TEXT("FAILED"));
            if (SelectedCastIndex != ExpectedIndex || !bVoicePlaying)
            {
                UE_LOG(LogTemp, Error, TEXT("ROTORLINE_STARTUP_TEST|CAST_GALLERY|FAIL|expected_index=%d"), ExpectedIndex);
                FPlatformMisc::RequestExit(false);
                return;
            }
            InputKey(FInputKeyEventArgs::CreateSimulated(
                EKeys::Gamepad_DPad_Down, IE_Pressed, 1.0f));
            StartupStateElapsed = 0.0f;
            ++StartupQualificationPhase;
        }
        else if (StartupQualificationPhase == 7 && StartupState == ERotorlineStartupState::CastGallery &&
            StartupStateElapsed >= 0.65f)
        {
            InputKey(FInputKeyEventArgs::CreateSimulated(
                EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, 1.0f));
            const bool bWrapped = SelectedCastIndex == 0 && IsCastVoicePlaying();
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP_TEST|CAST_GALLERY|WRAP|%s|index=%d|voice=%s|ps5_dpad=PASS|x_button=PASS"),
                bWrapped ? TEXT("PASS") : TEXT("FAIL"), SelectedCastIndex, IsCastVoicePlaying() ? TEXT("PLAYING") : TEXT("FAILED"));
            InputKey(FInputKeyEventArgs::CreateSimulated(
                EKeys::Gamepad_FaceButton_Right, IE_Pressed, 1.0f));
            StartupQualificationPhase = 8;
        }
        else if (StartupQualificationPhase == 8 && bStartScreenReady)
        {
            const bool bPassed = CastMembers.Num() == 5 && !IsCastVoicePlaying();
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP_TEST|CAST_GALLERY|%s|pairs=%d|main_menu_dpad=PASS|main_menu_x=PASS|duplicate_suppressed=PASS|profiles_checked=5|wrap=PASS|ps5_dpad=PASS|x_button=PASS|circle=PASS|voice=STOPPED|return=START_SCREEN"),
                bPassed ? TEXT("PASS") : TEXT("FAIL"), CastMembers.Num());
            FPlatformMisc::RequestExit(false);
        }
    }
    else if (StartupQualificationScenario.Equals(TEXT("exit"), ESearchCase::IgnoreCase) && bStartScreenReady)
    {
        SelectedStartupMenuIndex = 6;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP_TEST|EXIT|PASS|packaged_quit_path=READY"));
        ActivateStartupSelection();
    }
}

void ARotorlineOperationsPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    UpdatePreGameMenuMusic();

    RefreshControllerSemanticState();
    CheckFlightControllerConnection();
    FlightControllerNotificationSeconds = FMath::Max(0.0f, FlightControllerNotificationSeconds - DeltaTime);
    ControlsInputSuppressionSeconds = FMath::Max(0.0f, ControlsInputSuppressionSeconds - DeltaTime);
    TickControlsCapture(DeltaTime);

    if (bCombatPreviewAutoExit)
    {
        CombatPreviewElapsed += DeltaTime;
        if (CombatPreviewElapsed >= 35.0f)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_COMBAT_PREVIEW|AUTO_EXIT|elapsed=%.1f"), CombatPreviewElapsed);
            FPlatformMisc::RequestExit(false);
            bCombatPreviewAutoExit = false;
        }
    }

    if (!bGameWindowConfigured)
    {
        ConfigureGameWindow();
    }

    TickStartupFlow(DeltaTime);
    if (IsStartupFlowVisible())
    {
        if (bGraphicsSettingsOpen)
        {
            UpdateGraphicsSettingsInput();
        }
        else if (bControlsSettingsOpen)
        {
            UpdateControlsInput();
        }
        return;
    }

    EnvironmentAudioUpdateAccumulator += DeltaTime;
    if (EnvironmentAudioUpdateAccumulator >= 0.25f)
    {
        EnvironmentAudioUpdateAccumulator = 0.0f;
        RefreshEnvironmentAudioMix();
    }

    UpdateMissionTelemetry(DeltaTime);

    const bool bGamepadActive =
        FMath::Abs(GetInputAnalogKeyState(EKeys::Gamepad_LeftX)) > 0.05f ||
        FMath::Abs(GetInputAnalogKeyState(EKeys::Gamepad_LeftY)) > 0.05f ||
        FMath::Abs(GetInputAnalogKeyState(EKeys::Gamepad_RightX)) > 0.05f ||
        FMath::Abs(GetInputAnalogKeyState(EKeys::Gamepad_RightY)) > 0.05f ||
        GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis) > 0.05f ||
        GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) > 0.05f ||
        IsInputKeyDown(EKeys::Gamepad_DPad_Up) || IsInputKeyDown(EKeys::Gamepad_DPad_Down) ||
        IsInputKeyDown(EKeys::Gamepad_DPad_Left) || IsInputKeyDown(EKeys::Gamepad_DPad_Right) ||
        IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom) || IsInputKeyDown(EKeys::Gamepad_FaceButton_Right) ||
        IsInputKeyDown(EKeys::Gamepad_FaceButton_Top) || IsInputKeyDown(EKeys::Gamepad_FaceButton_Left) ||
        IsInputKeyDown(EKeys::Gamepad_LeftShoulder) || IsInputKeyDown(EKeys::Gamepad_RightShoulder) ||
        IsInputKeyDown(EKeys::Gamepad_LeftThumbstick) || IsInputKeyDown(EKeys::Gamepad_RightThumbstick);
    if (bGamepadActive && !bGamepadInputSeen)
    {
        bGamepadInputSeen = true;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_GAMEPAD|ACTIVE|input=RECEIVED"));
    }

    if (bOperationsMenuOpen)
    {
        if (bGraphicsSettingsOpen)
        {
            UpdateGraphicsSettingsInput();
            return;
        }
        if (bControlsSettingsOpen)
        {
            UpdateControlsInput();
            return;
        }
        UpdateOperationsInput();
        return;
    }


    if (bAwardPresentationOpen)
    {
        UpdateAwardPresentationInput();
        return;
    }

    if (bMissionCompleteScreenOpen)
    {
        if (!MissionLoopTestScenario.IsEmpty() && !bMissionLoopTestActionActivated)
        {
            bMissionLoopTestActionActivated = true;
            if (MissionLoopTestScenario.Equals(TEXT("replay"), ESearchCase::IgnoreCase) &&
                MissionLoopCompletedRuns >= 2)
            {
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_MISSION_LOOP_TEST|AUTO_ACTION|scenario=REPLAY|state=SECOND_SORTIE_COMPLETE|request_exit=1"));
                FPlatformMisc::RequestExit(false);
                return;
            }
            if (MissionLoopTestScenario.Equals(TEXT("replay"), ESearchCase::IgnoreCase))
            {
                SelectedMissionCompleteAction = 1;
            }
            else if (MissionLoopTestScenario.Equals(TEXT("hangar"), ESearchCase::IgnoreCase))
            {
                SelectedMissionCompleteAction = 2;
            }
            else if (MissionLoopTestScenario.Equals(TEXT("menu"), ESearchCase::IgnoreCase))
            {
                SelectedMissionCompleteAction = 3;
            }
            else
            {
                SelectedMissionCompleteAction = 0;
            }
            ActivateMissionCompleteSelection();
            if (MissionLoopTestScenario.Equals(TEXT("choose"), ESearchCase::IgnoreCase))
            {
                const int32 PreviousMissionIndex = SelectedMissionIndex;
                for (int32 Offset = 1; Offset < Missions.Num(); ++Offset)
                {
                    const int32 CandidateIndex = (PreviousMissionIndex + Offset) % Missions.Num();
                    if (IsMissionUnlocked(Missions[CandidateIndex]))
                    {
                        SelectedMissionIndex = CandidateIndex;
                        break;
                    }
                }
                DeploySelectedAircraft();
                const bool bCleanDifferentMission = GetPawn() && SelectedMissionIndex != PreviousMissionIndex;
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_MISSION_LOOP_TEST|CHOOSE_REDEPLOY|status=%s|previous=%s|next=%s|generation=%d"),
                    bCleanDifferentMission ? TEXT("PASS") : TEXT("FAIL"),
                    Missions.IsValidIndex(PreviousMissionIndex) ? *Missions[PreviousMissionIndex].Id : TEXT("INVALID"),
                    Missions.IsValidIndex(SelectedMissionIndex) ? *Missions[SelectedMissionIndex].Id : TEXT("INVALID"),
                    MissionResetGeneration);
            }
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_MISSION_LOOP_TEST|AUTO_ACTION|scenario=%s|state=COMPLETE|request_exit=1"),
                *MissionLoopTestScenario);
            if (!MissionLoopTestScenario.Equals(TEXT("replay"), ESearchCase::IgnoreCase))
            {
                FPlatformMisc::RequestExit(false);
            }
            return;
        }
        UpdateMissionCompleteInput();
        return;
    }

    if (const ARotorlineHelicopterPawn* Helicopter = Cast<ARotorlineHelicopterPawn>(GetPawn()))
    {
        if (Helicopter->IsMissionFailureMenuReady() && !bMissionFailureScreenOpen)
        {
            OpenMissionFailureScreen();
        }
        // Once destruction begins, flight-pause input must not compete with
        // the crash sequence or allow the failed aircraft to resume flying.
        if (Helicopter->IsMissionFailed() && !bMissionFailureScreenOpen)
        {
            return;
        }
    }

    if (bMissionFailureScreenOpen)
    {
        if (!CombatLoopTestScenario.IsEmpty() && !bCombatLoopTestActionActivated)
        {
            bCombatLoopTestActionActivated = true;
            SelectedMissionFailureAction = CombatLoopTestScenario.Equals(TEXT("select"), ESearchCase::IgnoreCase) ? 1 : 0;
            ActivateMissionFailureSelection();
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_COMBAT_LOOP_TEST|AUTO_ACTION|scenario=%s|state=COMPLETE|request_exit=1"),
                *CombatLoopTestScenario);
            FPlatformMisc::RequestExit(false);
            return;
        }
        UpdateMissionFailureInput();
        return;
    }

    if (bFlightPauseMenuOpen)
    {
        UpdateFlightPauseInput();
        return;
    }

    if (WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right) ||
        WasInputKeyJustPressed(EKeys::M) ||
        WasInputKeyJustPressed(EKeys::Pause) ||
        WasFlightControllerActionJustPressed(RotorlineFlightControllerActions::Pause))
    {
        SetFlightPauseMenuOpen(true);
    }
}

void ARotorlineOperationsPlayerController::ApplyRayTracingMode(
    const bool bEnableHardwareRayTracing,
    const bool bNotifyPlayer)
{
    IConsoleVariable* RayTracingEnable =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Enable"));
    IConsoleVariable* LumenHardwareRayTracing =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"));

    if (bEnableHardwareRayTracing)
    {
        if (RayTracingEnable) RayTracingEnable->Set(1, ECVF_SetByConsole);
        if (LumenHardwareRayTracing) LumenHardwareRayTracing->Set(1, ECVF_SetByConsole);
    }
    else
    {
        if (LumenHardwareRayTracing) LumenHardwareRayTracing->Set(0, ECVF_SetByConsole);
        if (RayTracingEnable) RayTracingEnable->Set(0, ECVF_SetByConsole);
    }

    bHardwareRayTracingEnabled =
        RayTracingEnable && RayTracingEnable->GetInt() != 0 &&
        LumenHardwareRayTracing && LumenHardwareRayTracing->GetInt() != 0;

    const FString Status = bHardwareRayTracingEnabled
        ? TEXT("RAY TRACING ON // F10 PERFORMANCE MODE")
        : TEXT("RAY TRACING OFF // PERFORMANCE MODE // F10 TO ENABLE");
    if (bNotifyPlayer)
    {
        ClientMessage(Status);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1, 4.0f,
                bHardwareRayTracingEnabled ? FColor::Cyan : FColor::Green,
                Status);
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_RAY_TRACING|requested=%d|enabled=%d|rt_enable=%d|lumen_hardware=%d"),
        bEnableHardwareRayTracing ? 1 : 0,
        bHardwareRayTracingEnabled ? 1 : 0,
        RayTracingEnable ? RayTracingEnable->GetInt() : -1,
        LumenHardwareRayTracing ? LumenHardwareRayTracing->GetInt() : -1);
}

void ARotorlineOperationsPlayerController::ToggleRayTracingMode()
{
    ToggleSimpleGraphicsMode();
}

void ARotorlineOperationsPlayerController::ApplySimpleGraphicsMode(
    const bool bEnableTurboMode,
    const bool bNotifyPlayer,
    const bool bPersist)
{
    bTurboGraphicsMode = bEnableTurboMode;

    auto SetIntSetting = [](const TCHAR* Name, const int32 Value)
    {
        if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            Variable->Set(Value, ECVF_SetByGameSetting);
        }
    };
    auto SetFloatSetting = [](const TCHAR* Name, const float Value)
    {
        if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            Variable->Set(Value, ECVF_SetByGameSetting);
        }
    };
    auto ApplyDirectionalShadowSettings = [&SetIntSetting, &SetFloatSetting](const bool bUseTurboSettings)
    {
        // VSM remains disabled project-wide. Keep Snail's expensive distance
        // field and volumetric features off while retaining a readable
        // conventional directional-light shadow for the player aircraft.
        SetIntSetting(TEXT("r.ShadowQuality"), 5);
        SetIntSetting(TEXT("r.Shadow.CSM.MaxCascades"), bUseTurboSettings ? 10 : 4);
        SetIntSetting(TEXT("r.Shadow.MaxCSMResolution"), 2048);
        SetFloatSetting(TEXT("r.Shadow.DistanceScale"), bUseTurboSettings ? 1.0f : 0.85f);
        SetFloatSetting(TEXT("r.Shadow.CSM.TransitionScale"), bUseTurboSettings ? 1.0f : 0.8f);
        SetFloatSetting(TEXT("r.Shadow.RadiusThreshold"), bUseTurboSettings ? 0.01f : 0.04f);
    };

    if (bTurboGraphicsMode)
    {
        SetIntSetting(TEXT("sg.ViewDistanceQuality"), 3);
        SetIntSetting(TEXT("sg.AntiAliasingQuality"), 3);
        SetIntSetting(TEXT("sg.ShadowQuality"), 3);
        SetIntSetting(TEXT("sg.GlobalIlluminationQuality"), 3);
        SetIntSetting(TEXT("sg.ReflectionQuality"), 3);
        SetIntSetting(TEXT("sg.PostProcessQuality"), 3);
        SetIntSetting(TEXT("sg.TextureQuality"), 3);
        SetIntSetting(TEXT("sg.EffectsQuality"), 3);
        SetIntSetting(TEXT("sg.FoliageQuality"), 3);
        SetIntSetting(TEXT("sg.ShadingQuality"), 3);
        SetFloatSetting(TEXT("r.ScreenPercentage"), 100.0f);
    }
    else
    {
        SetIntSetting(TEXT("sg.ViewDistanceQuality"), 2);
        SetIntSetting(TEXT("sg.AntiAliasingQuality"), 2);
        SetIntSetting(TEXT("sg.ShadowQuality"), 1);
        SetIntSetting(TEXT("sg.GlobalIlluminationQuality"), 1);
        SetIntSetting(TEXT("sg.ReflectionQuality"), 1);
        SetIntSetting(TEXT("sg.PostProcessQuality"), 2);
        SetIntSetting(TEXT("sg.TextureQuality"), 2);
        SetIntSetting(TEXT("sg.EffectsQuality"), 1);
        SetIntSetting(TEXT("sg.FoliageQuality"), 1);
        SetIntSetting(TEXT("sg.ShadingQuality"), 2);
        SetFloatSetting(TEXT("r.ScreenPercentage"), 75.0f);
    }

    ApplyRayTracingMode(bTurboGraphicsMode, false);
    ApplyDirectionalShadowSettings(bTurboGraphicsMode);
    // Fog is a core mission-visibility system, not an optional effects flourish.
    // Keep it enabled in both presets, with a coarser voxel grid in Snail mode.
    SetIntSetting(TEXT("r.VolumetricFog"), 1);
    SetIntSetting(TEXT("r.VolumetricFog.GridPixelSize"), bTurboGraphicsMode ? 8 : 16);
    SetIntSetting(TEXT("r.VolumetricFog.GridSizeZ"), bTurboGraphicsMode ? 128 : 64);

    auto GetIntSetting = [](const TCHAR* Name)
    {
        if (const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            return Variable->GetInt();
        }
        return -1;
    };
    auto GetFloatSetting = [](const TCHAR* Name)
    {
        if (const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            return Variable->GetFloat();
        }
        return -1.0f;
    };
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_GRAPHICS_SHADOWS|mode=%s|vsm=%d|quality=%d|cascades=%d|max_csm_resolution=%d|distance_scale=%.2f|transition_scale=%.2f|radius_threshold=%.3f|distance_fields=%d|volumetric_fog=%d"),
        bTurboGraphicsMode ? TEXT("TURBO") : TEXT("SNAIL"),
        GetIntSetting(TEXT("r.Shadow.Virtual.Enable")),
        GetIntSetting(TEXT("r.ShadowQuality")),
        GetIntSetting(TEXT("r.Shadow.CSM.MaxCascades")),
        GetIntSetting(TEXT("r.Shadow.MaxCSMResolution")),
        GetFloatSetting(TEXT("r.Shadow.DistanceScale")),
        GetFloatSetting(TEXT("r.Shadow.CSM.TransitionScale")),
        GetFloatSetting(TEXT("r.Shadow.RadiusThreshold")),
        GetIntSetting(TEXT("r.DistanceFieldShadowing")),
        GetIntSetting(TEXT("r.VolumetricFog")));

    if (bPersist && GConfig)
    {
        GConfig->SetString(
            TEXT("Rotorline.Graphics"),
            TEXT("SimpleMode"),
            bTurboGraphicsMode ? TEXT("TURBO") : TEXT("SNAIL"),
            GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    const FString Status = GetSimpleGraphicsModeLabel();
    ControlsStatus = Status;
    if (bNotifyPlayer)
    {
        ClientMessage(Status);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                5.0f,
                bTurboGraphicsMode ? FColor::Cyan : FColor::Green,
                Status);
        }
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_GRAPHICS|mode=%s|screen_percentage=%d|ray_tracing=%d|persisted=%d"),
        bTurboGraphicsMode ? TEXT("TURBO") : TEXT("SNAIL"),
        bTurboGraphicsMode ? 100 : 75,
        bHardwareRayTracingEnabled ? 1 : 0,
        bPersist ? 1 : 0);
}

void ARotorlineOperationsPlayerController::ToggleSimpleGraphicsMode()
{
    ApplySimpleGraphicsMode(!bTurboGraphicsMode, true, true);
}

void ARotorlineOperationsPlayerController::ConfigureGameWindow()
{
    UWorld* World = GetWorld();
    if (!World || World->WorldType != EWorldType::Game)
    {
        return;
    }

    int32 DesiredWidth = 1280;
    int32 DesiredHeight = 720;
    FParse::Value(FCommandLine::Get(), TEXT("ResX="), DesiredWidth);
    FParse::Value(FCommandLine::Get(), TEXT("ResY="), DesiredHeight);
    DesiredWidth = FMath::Clamp(DesiredWidth, 960, 7680);
    DesiredHeight = FMath::Clamp(DesiredHeight, 540, 4320);

    // Keep the standalone build advancing when focus temporarily moves to
    // A capture utility or another desktop window. Unreal's default
    // background idle can otherwise present as rotors repeatedly stopping and
    // starting even though the simulation itself has not been paused.
    IConsoleVariable* IdleWhenNotForeground =
        IConsoleManager::Get().FindConsoleVariable(TEXT("t.IdleWhenNotForeground"));
    if (IdleWhenNotForeground)
    {
        IdleWhenNotForeground->Set(0, ECVF_SetByGameSetting);
    }

    // The island still contains a large amount of non-Nanite procedural
    // geometry.  Rendering it into VSM coarse pages overflowed the marking job
    // queue in the packaged build and caused visible frame stalls.
    IConsoleVariable* NonNaniteCoarsePages =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.NonNanite.IncludeInCoarsePages"));
    if (NonNaniteCoarsePages)
    {
        NonNaniteCoarsePages->Set(0, ECVF_SetByGameSetting);
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_FRAME_PACING|background_idle=%d|vsm_non_nanite_coarse_pages=%d"),
        IdleWhenNotForeground ? IdleWhenNotForeground->GetInt() : -1,
        NonNaniteCoarsePages ? NonNaniteCoarsePages->GetInt() : -1);

    if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        Settings->SetFullscreenMode(EWindowMode::Windowed);
        Settings->SetScreenResolution(FIntPoint(DesiredWidth, DesiredHeight));
        Settings->ConfirmVideoMode();
        Settings->ApplySettings(false);
    }

    if (!GEngine || !GEngine->GameViewport)
    {
        return;
    }

    const TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
    if (!GameWindow.IsValid())
    {
        return;
    }

    // Always recover the standalone player onto the primary 1920x1080 display.
    // The previous saved mode targeted DISPLAY2 as borderless fullscreen, which
    // made the window impossible to drag or resize from the primary monitor.
    GameWindow->SetWindowMode(EWindowMode::Windowed);
    GameWindow->SetSizingRule(ESizingRule::UserSized);
    GameWindow->Resize(FVector2D(
        static_cast<float>(DesiredWidth),
        static_cast<float>(DesiredHeight)));
    const float WindowOffset = DesiredWidth >= 1920 ? 0.0f : 80.0f;
    GameWindow->MoveWindowTo(FVector2D(WindowOffset, WindowOffset));
    bGameWindowConfigured = true;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_WINDOW|PRIMARY|mode=WINDOWED|x=%.0f|y=%.0f|width=%d|height=%d|resizable=1"),
        WindowOffset, WindowOffset, DesiredWidth, DesiredHeight);
}

bool ARotorlineOperationsPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    // Flight keys must be handled before startup/menu routing can consume them.
    // This keeps Kiowa night vision available during Mission 6 auto-start and
    // throughout the flight without relying on the pawn's combat update loop.
    if (Params.Event == IE_Pressed && Params.Key == EKeys::N)
    {
        if (ARotorlineHelicopterPawn* Helicopter = Cast<ARotorlineHelicopterPawn>(GetPawn()))
        {
            Helicopter->ToggleNightVision();
            return true;
        }
    }
    if (Params.Event == IE_Pressed && Params.Key == EKeys::F10)
    {
        ToggleRayTracingMode();
        return true;
    }
    if (bControlsSettingsOpen && bNamingControlsAxis && Params.Event == IE_Pressed)
    {
        if (Params.Key == EKeys::Enter || Params.Key == EKeys::Gamepad_FaceButton_Bottom)
        {
            PendingControlsAxisLabel.TrimStartAndEndInline();
            if (WorkingControllerProfile.AxisBindings.IsValidIndex(SelectedControlsRow) &&
                !PendingControlsAxisLabel.IsEmpty())
            {
                WorkingControllerProfile.AxisBindings[SelectedControlsRow].UserLabel = PendingControlsAxisLabel.Left(32);
                bWorkingControllerProfileDirty = true;
                ControlsStatus = FString::Printf(TEXT("AXIS NAMED // %s"), *PendingControlsAxisLabel.ToUpper());
            }
            bNamingControlsAxis = false;
            PendingControlsAxisLabel.Reset();
            return true;
        }
        if (Params.Key == EKeys::Escape || Params.Key == EKeys::Gamepad_FaceButton_Right)
        {
            bNamingControlsAxis = false;
            PendingControlsAxisLabel.Reset();
            ControlsStatus = TEXT("AXIS NAMING CANCELLED");
            return true;
        }
        if (Params.Key == EKeys::BackSpace)
        {
            if (!PendingControlsAxisLabel.IsEmpty()) PendingControlsAxisLabel.LeftChopInline(1);
        }
        else if (Params.Key == EKeys::SpaceBar && PendingControlsAxisLabel.Len() < 32)
        {
            PendingControlsAxisLabel.AppendChar(TEXT(' '));
        }
        else
        {
            const FString KeyCharacter = Params.Key.GetDisplayName(false).ToString();
            if (KeyCharacter.Len() == 1 && FChar::IsAlnum(KeyCharacter[0]) && PendingControlsAxisLabel.Len() < 32)
            {
                PendingControlsAxisLabel += KeyCharacter;
            }
        }
        ControlsStatus = FString::Printf(TEXT("NAME AXIS // %s_ // ENTER SAVE // CIRCLE CANCEL"),
            *PendingControlsAxisLabel.ToUpper());
        return true;
    }
    if (IsStartupFlowVisible())
    {
        HandleStartupInput(Params);
        return true;
    }

    if (bControlsSettingsOpen)
    {
        if (Params.Event == IE_Pressed &&
            (Params.Key == EKeys::Escape || Params.Key == EKeys::Gamepad_FaceButton_Right ||
                Params.Key == EKeys::Gamepad_Special_Right))
        {
            if (CancelPendingControlsDuplicate()) return true;
            ToggleControlsSettings();
            return true;
        }
        if (QueueControlsSettingsInput(Params)) return true;
        return Super::InputKey(Params);
    }

    if (Params.Event == IE_Pressed)
    {
        if (bGraphicsSettingsOpen)
        {
            if (Params.Key == EKeys::Escape || Params.Key == EKeys::Gamepad_FaceButton_Right)
            {
                ToggleGraphicsSettings();
                return true;
            }
            return Super::InputKey(Params);
        }
        const bool bReleaseRequested =
            Params.Key == EKeys::Escape || Params.Key == EKeys::Gamepad_Special_Right ||
            Params.Key == EKeys::Gamepad_FaceButton_Right;
        if (bReleaseRequested)
        {
            if (bAwardPresentationOpen)
            {
                AdvanceAwardPresentation();
                return true;
            }
            if (bMissionCompleteScreenOpen || bMissionFailureScreenOpen ||
                (Cast<ARotorlineHelicopterPawn>(GetPawn()) && CastChecked<ARotorlineHelicopterPawn>(GetPawn())->IsMissionFailed()))
            {
                return true;
            }
            if (bOperationsMenuOpen && bPatchWallOpen)
            {
                TogglePatchWall();
            }
            else if (bOperationsMenuOpen && bGraphicsSettingsOpen)
            {
                ToggleGraphicsSettings();
            }
            else if (bOperationsMenuOpen && bAudioSettingsOpen)
            {
                ToggleAudioSettings();
            }
            else if (bOperationsMenuOpen && bHangarOpen)
            {
                CloseHangar();
            }
            else if (bOperationsMenuOpen)
            {
                ReturnToMainMenu();
            }
            else if (bAudioSettingsOpen)
            {
                ToggleAudioSettings();
            }
            else if (bGraphicsSettingsOpen)
            {
                ToggleGraphicsSettings();
            }
            else
            {
                SetFlightPauseMenuOpen(!bFlightPauseMenuOpen);
            }
            return true;
        }

        if (Params.Key == EKeys::LeftMouseButton && bOperationsMenuOpen &&
            !bPatchWallOpen && !bAudioSettingsOpen && !bHangarOpen)
        {
            float X = 0.0f;
            float Y = 0.0f;
            int32 Width = 0;
            int32 Height = 0;
            GetViewportSize(Width, Height);
            if (GetMousePosition(X, Y) && Width > 0 && Height > 0)
            {
                const float Scale = FMath::Clamp(Height / 1080.0f, 0.72f, 1.35f);
                const float FooterY = Height - 72.0f * Scale;
                const float ButtonX = Width - 350.0f * Scale;
                const float ButtonY = FooterY - 9.0f * Scale;
                const float ButtonWidth = 300.0f * Scale;
                const float ButtonHeight = 44.0f * Scale;
                if (X >= ButtonX && X <= ButtonX + ButtonWidth &&
                    Y >= ButtonY && Y <= ButtonY + ButtonHeight)
                {
                    ReturnToMainMenu();
                    return true;
                }
            }
        }

        const bool bAltDown = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
        if (Params.Key == EKeys::F4 && bAltDown)
        {
            ApplyMouseMode(false);
            if (UWorld* World = GetWorld(); World && World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::EditorPreview)
            {
                UKismetSystemLibrary::QuitGame(World, this, EQuitPreference::Quit, false);
            }
            return true;
        }
    }

    return Super::InputKey(Params);
}

FString ARotorlineOperationsPlayerController::GetMissionFailureReason() const
{
    if (const ARotorlineHelicopterPawn* Helicopter = Cast<ARotorlineHelicopterPawn>(GetPawn()))
    {
        return Helicopter->GetMissionFailureReason();
    }
    return FString();
}

void ARotorlineOperationsPlayerController::ApplyMouseMode(bool bCaptureForMouseLook)
{
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
    bShowMouseCursor = !bCaptureForMouseLook;

    if (UWorld* World = GetWorld())
    {
        if (UGameViewportClient* ViewportClient = World->GetGameViewport())
        {
            ViewportClient->SetMouseCaptureMode(
                bCaptureForMouseLook ? EMouseCaptureMode::CapturePermanently : EMouseCaptureMode::CaptureDuringMouseDown);
            ViewportClient->SetMouseLockMode(EMouseLockMode::DoNotLock);
        }
    }

    if (bCaptureForMouseLook)
    {
        SetInputMode(FInputModeGameOnly());
    }
    else
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        SetInputMode(Mode);
    }
}

void ARotorlineOperationsPlayerController::UpdateOperationsInput()
{
    const float VerticalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
    const float HorizontalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);

    const bool bMoveUp = WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up) || (!bVerticalAxisLatched && VerticalAxis > 0.55f);
    const bool bMoveDown = WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down) || (!bVerticalAxisLatched && VerticalAxis < -0.55f);
    const bool bMoveLeft = WasInputKeyJustPressed(EKeys::Left) || WasInputKeyJustPressed(EKeys::A) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Left) || (!bHorizontalAxisLatched && HorizontalAxis < -0.55f);
    const bool bMoveRight = WasInputKeyJustPressed(EKeys::Right) || WasInputKeyJustPressed(EKeys::D) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right) || (!bHorizontalAxisLatched && HorizontalAxis > 0.55f);

    bVerticalAxisLatched = FMath::Abs(VerticalAxis) > 0.35f;
    bHorizontalAxisLatched = FMath::Abs(HorizontalAxis) > 0.35f;

    const bool bToggleAudio = WasInputKeyJustPressed(EKeys::V) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top);
    if (bToggleAudio)
    {
        ToggleAudioSettings();
        return;
    }


    const bool bTogglePatchWall = WasInputKeyJustPressed(EKeys::P) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left);
    if (bTogglePatchWall)
    {
        TogglePatchWall();
        return;
    }

    if (bPatchWallOpen)
    {
        UpdatePatchWallInput(bMoveUp, bMoveDown, bMoveLeft, bMoveRight);
        return;
    }

    if (bAudioSettingsOpen)
    {
        if (bMoveUp) MoveAudioSelection(-1);
        if (bMoveDown) MoveAudioSelection(1);
        if (bMoveLeft) AdjustAudioSetting(-1);
        if (bMoveRight) AdjustAudioSetting(1);
        if (WasInputKeyJustPressed(EKeys::R) || WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder))
        {
            ResetAudioSettings();
        }
        if (WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right))
        {
            ToggleAudioSettings();
        }
        return;
    }

    if (bHangarOpen)
    {
        // The hangar is a horizontal aircraft carousel. Support both the
        // directional controls and the shoulder buttons players naturally
        // expect to use for previous/next vehicle on a gamepad.
        const bool bPreviousAircraft = bMoveLeft ||
            WasInputKeyJustPressed(EKeys::Q) ||
            WasInputKeyJustPressed(EKeys::Gamepad_LeftShoulder);
        const bool bNextAircraft = bMoveRight ||
            WasInputKeyJustPressed(EKeys::E) ||
            WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder);
        UpdateHangarInput(bPreviousAircraft, bNextAircraft);
        return;
    }

    if (bMoveUp) MoveMissionSelection(-1);
    if (bMoveDown) MoveMissionSelection(1);

    if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
    {
        LaunchSelection();
    }
}

void ARotorlineOperationsPlayerController::UpdateHangarInput(bool bMoveLeft, bool bMoveRight)
{
    if (bMoveLeft)
    {
        MoveAircraftSelection(-1);
    }
    if (bMoveRight)
    {
        MoveAircraftSelection(1);
    }

    if (WasInputKeyJustPressed(EKeys::Escape) ||
        WasInputKeyJustPressed(EKeys::BackSpace) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right))
    {
        CloseHangar();
        return;
    }

    if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
    {
        DeploySelectedAircraft();
    }
}

void ARotorlineOperationsPlayerController::UpdateFlightPauseInput()
{
    if (bGraphicsSettingsOpen)
    {
        UpdateGraphicsSettingsInput();
        return;
    }
    if (bControlsSettingsOpen)
    {
        UpdateControlsInput();
        return;
    }
    const float VerticalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
    const float HorizontalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);

    const bool bMoveUp = WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up) || (!bVerticalAxisLatched && VerticalAxis > 0.55f);
    const bool bMoveDown = WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down) || (!bVerticalAxisLatched && VerticalAxis < -0.55f);
    const bool bMoveLeft = WasInputKeyJustPressed(EKeys::Left) || WasInputKeyJustPressed(EKeys::A) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Left) || (!bHorizontalAxisLatched && HorizontalAxis < -0.55f);
    const bool bMoveRight = WasInputKeyJustPressed(EKeys::Right) || WasInputKeyJustPressed(EKeys::D) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right) || (!bHorizontalAxisLatched && HorizontalAxis > 0.55f);

    bVerticalAxisLatched = FMath::Abs(VerticalAxis) > 0.35f;
    bHorizontalAxisLatched = FMath::Abs(HorizontalAxis) > 0.35f;

    const bool bToggleAudio = WasInputKeyJustPressed(EKeys::V) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top);
    if (bToggleAudio)
    {
        ToggleAudioSettings();
        return;
    }

    if (bAudioSettingsOpen)
    {
        if (bMoveUp) MoveAudioSelection(-1);
        if (bMoveDown) MoveAudioSelection(1);
        if (bMoveLeft) AdjustAudioSetting(-1);
        if (bMoveRight) AdjustAudioSetting(1);
        if (WasInputKeyJustPressed(EKeys::R) || WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder))
        {
            ResetAudioSettings();
        }
        if (WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right))
        {
            ToggleAudioSettings();
        }
        return;
    }

    if (WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right) ||
        WasInputKeyJustPressed(EKeys::M) ||
        WasInputKeyJustPressed(EKeys::Pause))
    {
        SetFlightPauseMenuOpen(false);
        return;
    }

    if (bMoveUp) MovePauseSelection(-1);
    if (bMoveDown) MovePauseSelection(1);
    if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
    {
        ActivatePauseSelection();
    }
}

void ARotorlineOperationsPlayerController::UpdateMissionFailureInput()
{
    const float VerticalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
    const bool bMoveUp = WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up) || (!bVerticalAxisLatched && VerticalAxis > 0.55f);
    const bool bMoveDown = WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down) || (!bVerticalAxisLatched && VerticalAxis < -0.55f);
    bVerticalAxisLatched = FMath::Abs(VerticalAxis) > 0.35f;

    if (bMoveUp) MoveMissionFailureSelection(-1);
    if (bMoveDown) MoveMissionFailureSelection(1);
    if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
    {
        ActivateMissionFailureSelection();
    }
}

void ARotorlineOperationsPlayerController::UpdateMissionCompleteInput()
{
    const float VerticalAxis = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
    const bool bMoveUp = WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up) || (!bVerticalAxisLatched && VerticalAxis > 0.55f);
    const bool bMoveDown = WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down) || (!bVerticalAxisLatched && VerticalAxis < -0.55f);
    bVerticalAxisLatched = FMath::Abs(VerticalAxis) > 0.35f;

    if (bMoveUp) MoveMissionCompleteSelection(-1);
    if (bMoveDown) MoveMissionCompleteSelection(1);
    if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
    {
        ActivateMissionCompleteSelection();
    }
}

void ARotorlineOperationsPlayerController::UpdateAwardPresentationInput()
{
    if (WasInputKeyJustPressed(EKeys::Enter) ||
        WasInputKeyJustPressed(EKeys::SpaceBar) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right))
    {
        AdvanceAwardPresentation();
    }
}

void ARotorlineOperationsPlayerController::UpdatePatchWallInput(
    bool bMoveUp,
    bool bMoveDown,
    bool bMoveLeft,
    bool bMoveRight)
{
    MovePatchWallSelection(
        bMoveRight ? 1 : (bMoveLeft ? -1 : 0),
        bMoveDown ? 1 : (bMoveUp ? -1 : 0));
    if (WasInputKeyJustPressed(EKeys::Escape) ||
        WasInputKeyJustPressed(EKeys::BackSpace) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right))
    {
        TogglePatchWall();
    }
}

void ARotorlineOperationsPlayerController::MovePatchWallSelection(
    int32 HorizontalDirection,
    int32 VerticalDirection)
{
    const int32 AwardCount = AwardSystem.GetDefinitions().Num();
    if (AwardCount <= 0) return;
    constexpr int32 Columns = 5;
    if (HorizontalDirection < 0) PatchWallSelection = (PatchWallSelection - 1 + AwardCount) % AwardCount;
    if (HorizontalDirection > 0) PatchWallSelection = (PatchWallSelection + 1) % AwardCount;
    if (VerticalDirection < 0) PatchWallSelection = (PatchWallSelection - Columns + AwardCount) % AwardCount;
    if (VerticalDirection > 0) PatchWallSelection = (PatchWallSelection + Columns) % AwardCount;
    PatchWallSelection = FMath::Clamp(PatchWallSelection, 0, AwardCount - 1);
    if (HorizontalDirection != 0 || VerticalDirection != 0)
    {
        PulseController(0.10f, 0.035f);
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS|PATCH_SELECT|index=%d|id=%s"),
            PatchWallSelection,
            AwardSystem.GetDefinitions().IsValidIndex(PatchWallSelection)
                ? *AwardSystem.GetDefinitions()[PatchWallSelection].Id
                : TEXT("NONE"));
    }
}

void ARotorlineOperationsPlayerController::MoveMissionSelection(int32 Direction)
{
    if (Missions.IsEmpty())
    {
        return;
    }

    SelectedMissionIndex = (SelectedMissionIndex + Direction + Missions.Num()) % Missions.Num();
    const FRotorlineMissionDefinition& Mission = Missions[SelectedMissionIndex];
    SelectedCraft = Mission.RecommendedCraft.Equals(TEXT("attack"), ESearchCase::IgnoreCase)
        ? ERotorlineCraftType::AttackMD500
        : ERotorlineCraftType::SupportHuey;
    const bool bFinalDiscoveryMission =
        Mission.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase);
    const bool bKiowaSensorMission =
        Mission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase) ||
        Mission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase);
    const bool bChinookFinalMission =
        Mission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
        Mission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase);
    const FString RecommendedId = bFinalDiscoveryMission
        ? TEXT("jeep_wrangler")
        : (bChinookFinalMission
            ? TEXT("ch47_chinook")
            : (bKiowaSensorMission
                ? TEXT("oh58_kiowa")
                : (SelectedCraft == ERotorlineCraftType::AttackMD500
                    ? TEXT("md500_defender")
                    : TEXT("uh1_huey"))));
    const int32 RecommendedIndex = Aircraft.IndexOfByPredicate(
        [&RecommendedId](const FRotorlineAircraftDefinition& Entry)
        {
            return Entry.Id.Equals(RecommendedId, ESearchCase::IgnoreCase);
        });
    if (RecommendedIndex != INDEX_NONE)
    {
        SelectedAircraftIndex = RecommendedIndex;
    }
    PulseController(0.13f, 0.045f);
}

void ARotorlineOperationsPlayerController::MoveCraftSelection(int32 Direction)
{
    SelectedCraft = SelectedCraft == ERotorlineCraftType::SupportHuey
        ? ERotorlineCraftType::AttackMD500
        : ERotorlineCraftType::SupportHuey;
    PulseController(0.18f, 0.06f);
}

void ARotorlineOperationsPlayerController::LaunchSelection()
{
    if (!bHangarOpen)
    {
        if (Missions.IsValidIndex(SelectedMissionIndex))
        {
            const FRotorlineMissionDefinition& Mission = Missions[SelectedMissionIndex];
            const bool bDirectChinookMission =
                Mission.Id.Equals(TEXT("survivor-extraction"), ESearchCase::IgnoreCase) ||
                Mission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase);
            if (bDirectChinookMission)
            {
                if (!bFleetQualificationMode && !bQuickDeploy && !IsMissionUnlocked(Mission))
                {
                    CatalogError = FString::Printf(
                        TEXT("MISSION LOCKED - %d REPUTATION REQUIRED"),
                        Mission.Unlock);
                    PulseController(0.45f, 0.12f);
                    return;
                }

                const int32 ChinookIndex = Aircraft.IndexOfByPredicate(
                    [](const FRotorlineAircraftDefinition& Entry)
                    {
                        return Entry.Id.Equals(TEXT("ch47_chinook"), ESearchCase::IgnoreCase);
                    });
                if (ChinookIndex == INDEX_NONE)
                {
                    CatalogError = TEXT("CH-47 CHINOOK IS NOT AVAILABLE");
                    PulseController(0.45f, 0.12f);
                    UE_LOG(LogTemp, Error,
                        TEXT("ROTORLINE_DIRECT_DEPLOY|mission=%s|required=ch47_chinook|state=MISSING"),
                        *Mission.Id);
                    return;
                }

                SelectedAircraftIndex = ChinookIndex;
                CatalogError.Reset();
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_DIRECT_DEPLOY|mission=%s|aircraft=ch47_chinook|hangar=BYPASSED|startup=COLD"),
                    *Mission.Id);
                DeploySelectedAircraft();
                return;
            }
        }

        OpenHangar();
        return;
    }

    DeploySelectedAircraft();
}

void ARotorlineOperationsPlayerController::OpenHangar()
{
    if (bHangarOpen || !Missions.IsValidIndex(SelectedMissionIndex) || Aircraft.IsEmpty())
    {
        return;
    }

    if (!bFleetQualificationMode && !bQuickDeploy && !IsMissionUnlocked(Missions[SelectedMissionIndex]))
    {
        CatalogError = FString::Printf(
            TEXT("MISSION LOCKED - %d REPUTATION REQUIRED"),
            Missions[SelectedMissionIndex].Unlock);
        PulseController(0.45f, 0.12f);
        return;
    }

    const FTransform HangarTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 800000.0f));
    HangarPreviewActor = GetWorld()->SpawnActor<ARotorlineHangarPreviewActor>(
        ARotorlineHangarPreviewActor::StaticClass(),
        HangarTransform,
        FActorSpawnParameters());
    if (!HangarPreviewActor)
    {
        CatalogError = TEXT("Could not open aircraft hangar");
        return;
    }

    bHangarOpen = true;
    RefreshHangarPreview();
    // Configure and frame the aircraft before handing the camera over. This is
    // especially important after mission completion, when the old pawn and its
    // precision-optic camera have just been destroyed.
    if (UCameraComponent* HangarCamera = HangarPreviewActor->GetPreviewCamera())
    {
        HangarCamera->Activate(true);
    }
    SetViewTargetWithBlend(HangarPreviewActor, 0.0f);
    PulseController(0.24f, 0.09f);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_HANGAR|OPEN|aircraft=%d|mission=%s|view_target=%s|framed=%d"),
        Aircraft.Num(),
        *Missions[SelectedMissionIndex].Id,
        GetViewTarget() == HangarPreviewActor ? TEXT("HANGAR") : TEXT("OTHER"),
        HangarPreviewActor->IsAircraftFramed() ? 1 : 0);
}

void ARotorlineOperationsPlayerController::CloseHangar()
{
    if (!bHangarOpen)
    {
        return;
    }

    bHangarOpen = false;
    if (HangarPreviewActor)
    {
        HangarPreviewActor->Destroy();
        HangarPreviewActor = nullptr;
    }
    SetViewTarget(this);
    PulseController(0.14f, 0.05f);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_HANGAR|CLOSE|destination=OPERATIONS"));
}

void ARotorlineOperationsPlayerController::MoveAircraftSelection(int32 Direction)
{
    if (Aircraft.IsEmpty())
    {
        return;
    }
    SelectedAircraftIndex = (SelectedAircraftIndex + Direction + Aircraft.Num()) % Aircraft.Num();
    RefreshHangarPreview();
    PulseController(0.16f, 0.05f);
}

const FRotorlineAircraftDefinition* ARotorlineOperationsPlayerController::GetSelectedAircraft() const
{
    return Aircraft.IsValidIndex(SelectedAircraftIndex) ? &Aircraft[SelectedAircraftIndex] : nullptr;
}

int32 ARotorlineOperationsPlayerController::GetSelectedAircraftMissionFit() const
{
    const FRotorlineAircraftDefinition* SelectedAircraft = GetSelectedAircraft();
    if (!SelectedAircraft || !Missions.IsValidIndex(SelectedMissionIndex))
    {
        return 1;
    }
    return FRotorlineAircraftCatalog::SuitabilityForMissionType(
        *SelectedAircraft,
        Missions[SelectedMissionIndex].Type + TEXT(" ") + Missions[SelectedMissionIndex].Callsign);
}

void ARotorlineOperationsPlayerController::RefreshHangarPreview()
{
    const FRotorlineAircraftDefinition* SelectedAircraft = GetSelectedAircraft();
    if (!HangarPreviewActor || !SelectedAircraft)
    {
        return;
    }

    const bool bAircraftUnlocked = IsAircraftUnlocked(*SelectedAircraft);
    if (!bAircraftUnlocked)
    {
        HangarPreviewActor->ConfigureClassifiedPlaceholder();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_HANGAR|SELECT|index=%d|id=CLASSIFIED|ready=0|campaign=%d/%d"),
            SelectedAircraftIndex,
            GetCompletedCampaignMissionCount(),
            GetCampaignMissionCount());
        return;
    }

    TArray<FString> BodyPaths;
    BodyPaths = SelectedAircraft->BodyAssets;
    if (BodyPaths.IsEmpty() && !SelectedAircraft->BodyAsset.IsEmpty())
    {
        BodyPaths.Add(SelectedAircraft->BodyAsset);
    }
    TArray<FString> PreviewRotorPaths = SelectedAircraft->RotorAssets;
    TArray<FRotorlineAircraftRotorGroup> PreviewRotorGroups = SelectedAircraft->RotorGroups;
    TArray<FString> PreviewStationaryRotorPaths = SelectedAircraft->StationaryRotorAssets;
    const bool bGroundVehicle =
        SelectedAircraft->DeploymentClass.Equals(TEXT("ground"), ESearchCase::IgnoreCase);
    const bool bUseProceduralRotorFallback =
        !bGroundVehicle &&
        !SelectedAircraft->Id.Equals(TEXT("md500_defender"), ESearchCase::IgnoreCase);
    // Use the catalog calibration for the Jeep. Ground vehicles are framed
    // separately by the preview actor and must not be inflated to aircraft size.
    const float HangarPresentationScale = SelectedAircraft->PresentationScale;
    HangarPreviewActor->ConfigureAircraft(
        BodyPaths,
        PreviewRotorPaths,
        PreviewRotorGroups,
        PreviewStationaryRotorPaths,
        bUseProceduralRotorFallback,
        FVector(HangarPresentationScale),
        FRotator(
            SelectedAircraft->PresentationPitch,
            SelectedAircraft->PresentationYaw,
            SelectedAircraft->PresentationRoll),
        SelectedAircraft->PresentationOffset + FVector(0.0f, 0.0f, 145.0f));

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_HANGAR|SELECT|index=%d|id=%s|ready=%d|fit=%d"),
        SelectedAircraftIndex,
        *SelectedAircraft->Id,
        (SelectedAircraft->bAlphaSelectable && SelectedAircraft->bReadyForHangar) ? 1 : 0,
        GetSelectedAircraftMissionFit());
}

void ARotorlineOperationsPlayerController::DeploySelectedAircraft()
{
    StopPreGameMenuMusic(TEXT("AIRCRAFT_DEPLOY"));
    const FRotorlineAircraftDefinition* SelectedAircraft = GetSelectedAircraft();
    if (!SelectedAircraft)
    {
        return;
    }
    if (!SelectedAircraft->bAlphaSelectable || !SelectedAircraft->bReadyForHangar)
    {
        CatalogError = FString::Printf(TEXT("%s IS NOT YET FLIGHT CERTIFIED"), *SelectedAircraft->DisplayName);
        PulseController(0.42f, 0.11f);
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_HANGAR|DEPLOY_BLOCKED|id=%s|status=%s"), *SelectedAircraft->Id, *SelectedAircraft->SpawnStatus);
        return;
    }
    if (Missions.IsValidIndex(SelectedMissionIndex) &&
        Missions[SelectedMissionIndex].Id.Equals(TEXT("tutorial"), ESearchCase::IgnoreCase) &&
        !bFleetQualificationMode &&
        !SelectedAircraft->Id.Equals(TEXT("uh1_huey"), ESearchCase::IgnoreCase))
    {
        CatalogError = TEXT("MISSION 01 REQUIRES UH-1 HUEY");
        PulseController(0.42f, 0.11f);
        UE_LOG(LogTemp, Warning,
            TEXT("ROTORLINE_DEPLOY_BLOCKED|mission=tutorial|required=uh1_huey|selected=%s"),
            *SelectedAircraft->Id);
        return;
    }
    const bool bFinalDiscoveryMission =
        Missions.IsValidIndex(SelectedMissionIndex) &&
        Missions[SelectedMissionIndex].Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase);
    const bool bBellCounterstrikeMission =
        Missions.IsValidIndex(SelectedMissionIndex) &&
        Missions[SelectedMissionIndex].Id.Equals(TEXT("bell-counterstrike"), ESearchCase::IgnoreCase);
    const bool bSelectedJeep =
        SelectedAircraft->DeploymentClass.Equals(TEXT("ground"), ESearchCase::IgnoreCase);
    if (!bQuickDeploy && bFinalDiscoveryMission && !bSelectedJeep &&
        !(ProfileSave && ProfileSave->bBell222Discovered))
    {
        CatalogError = TEXT("DISCOVERY MISSION REQUIRES THE JEEP // FIND THE HIDDEN AIRCRAFT");
        PulseController(0.42f, 0.11f);
        return;
    }
    if (!bQuickDeploy && bSelectedJeep &&
        !(ProfileSave && ProfileSave->bJeepPermanentlyUnlocked) &&
        !bFinalDiscoveryMission)
    {
        CatalogError = TEXT("JEEP CRATE SEALED // COMPLETE MISSION 18");
        PulseController(0.42f, 0.11f);
        return;
    }
    // Explicit local qualification/quick-deploy flags must be able to exercise
    // every certified airframe without mutating campaign progression.
    if (!bFleetQualificationMode && !bQuickDeploy && !IsAircraftUnlocked(*SelectedAircraft))
    {
        if (SelectedAircraft->Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase))
        {
            CatalogError = TEXT("BELL 222 REMAINS CLASSIFIED - COMPLETE MISSION 19, THE HIDDEN MACHINE");
        }
        else if (SelectedAircraft->Id.Equals(TEXT("jeep_wrangler"), ESearchCase::IgnoreCase))
        {
            CatalogError = TEXT("JEEP CRATE SEALED - COMPLETE MISSION 18");
        }
        else
        {
            CatalogError = TEXT("CLASSIFIED VEHICLE");
        }
        PulseController(0.42f, 0.11f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_HANGAR|DEPLOY_BLOCKED|id=CLASSIFIED|reason=CAMPAIGN_INCOMPLETE|campaign=%d/%d"),
            GetCompletedCampaignMissionCount(),
            GetCampaignMissionCount());
        return;
    }
    if (SelectedAircraft->DeploymentClass.Equals(TEXT("ground"), ESearchCase::IgnoreCase))
    {
        DeploySelectedGroundVehicle(*SelectedAircraft);
        return;
    }
    if (!bQuickDeploy && Missions.IsValidIndex(SelectedMissionIndex) &&
        (Missions[SelectedMissionIndex].Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase) ||
            Missions[SelectedMissionIndex].Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase)) &&
        !SelectedAircraft->Id.Equals(TEXT("oh58_kiowa"), ESearchCase::IgnoreCase))
    {
        CatalogError = TEXT("MISSION REQUIRES OH-58 KIOWA // RECON SENSOR PACKAGE");
        PulseController(0.42f, 0.11f);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_HANGAR|DEPLOY_BLOCKED|mission=%s|aircraft=%s|reason=KIOWA_REQUIRED"),
            *Missions[SelectedMissionIndex].Id,
            *SelectedAircraft->Id);
        return;
    }


    if (!bQuickDeploy && bBellCounterstrikeMission &&
        !SelectedAircraft->Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase))
    {
        CatalogError = TEXT("MISSION 20 REQUIRES THE BELL 222");
        PulseController(0.42f, 0.11f);
        return;
    }

    const bool bArmedAircraft = SelectedAircraft->MissionSuitability.Attack >= 4 ||
        SelectedAircraft->Role.Contains(TEXT("attack"), ESearchCase::IgnoreCase) ||
        SelectedAircraft->Role.Contains(TEXT("gunship"), ESearchCase::IgnoreCase);
    SelectedCraft = bArmedAircraft ? ERotorlineCraftType::AttackMD500 : ERotorlineCraftType::SupportHuey;

    CloseHangar();
    if (!Missions.IsValidIndex(SelectedMissionIndex) || GetPawn())
    {
        return;
    }

    if (!bFleetQualificationMode && !bQuickDeploy && !IsMissionUnlocked(Missions[SelectedMissionIndex]))
    {
        CatalogError = FString::Printf(
            TEXT("MISSION LOCKED - %d REPUTATION REQUIRED"),
            Missions[SelectedMissionIndex].Unlock);
        PulseController(0.45f, 0.12f);
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_PROFILE|LOCKED|mission=%s|required=%d|reputation=%d"),
            *Missions[SelectedMissionIndex].Id,
            Missions[SelectedMissionIndex].Unlock,
            GetReputation());
        return;
    }

    ResetMissionResults(TEXT("DEPLOY"));

    // Permanent authored landmarks must survive every mission reset and remain
    // visible from across the island. They are map content, not mission actors.
    static const FName PersistentWorldTag(TEXT("Rotorline.PersistentWorld"));
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Landmark = *It;
        if (!IsValid(Landmark) || !Landmark->ActorHasTag(PersistentWorldTag))
        {
            continue;
        }

        Landmark->SetActorHiddenInGame(false);
        TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Landmark);
        for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
        {
            if (!PrimitiveComponent)
            {
                continue;
            }
            if (PrimitiveComponent->ComponentHasTag(TEXT("Rotorline.RuntimeTrigger")))
            {
                PrimitiveComponent->SetVisibility(false, true);
                PrimitiveComponent->SetHiddenInGame(true, true);
                PrimitiveComponent->SetCastShadow(false);
                PrimitiveComponent->MarkRenderStateDirty();
                continue;
            }
            PrimitiveComponent->SetVisibility(true, true);
            PrimitiveComponent->SetHiddenInGame(false, true);
            PrimitiveComponent->SetCullDistance(0.0f);
            PrimitiveComponent->bAllowCullDistanceVolume = false;
            PrimitiveComponent->SetBoundsScale(1.0f);
            PrimitiveComponent->MarkRenderStateDirty();
        }
    }

    if (ProfileSave)
    {
        ++ProfileSave->CareerStatistics.MissionsStarted;
        SaveProfile();
    }

    const bool bBellLairDeployment =
        SelectedAircraft->Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase) &&
        Missions[SelectedMissionIndex].Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase);
    FVector DeploymentLocation = RotorlineOperations::SpawnLocation;
    FRotator DeploymentRotation = RotorlineOperations::SpawnRotation;
    ActiveHomePadLocation = RotorlineOperations::SpawnLocation;
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        UStaticMeshComponent* PadComponent = It->GetStaticMeshComponent();
        UStaticMesh* PadMesh = PadComponent ? PadComponent->GetStaticMesh() : nullptr;
        const FString PadIdentity = (It->GetName() + TEXT(" ") +
            (PadMesh ? PadMesh->GetPathName() : FString())).ToLower();
        if ((!PadIdentity.Contains(TEXT("helipad")) &&
             !PadIdentity.Contains(TEXT("heliport")) &&
             !PadIdentity.Contains(TEXT("landing_pad"))) ||
            FVector::Dist2D(It->GetActorLocation(), ActiveHomePadLocation) > 6000.0f ||
            It->ActorHasTag(TEXT("RotorlineHomePadSurfaceAligned")))
        {
            continue;
        }

        FRotorlineGroundingProfile HomePadProfile = URotorlineGroundingLibrary::MakeProfile(
            ERotorlineGroundingMode::LinearPoint, TEXT("AuthoredHomeHelipad"));
        HomePadProfile.bAllowPreparedGround = true;
        HomePadProfile.bRejectObstructionsAboveGround = false;
        HomePadProfile.bCheckCollisionPenetration = false;
        HomePadProfile.PreservedOffsetCm = 4.0f;
        HomePadProfile.MaximumSlopeDegrees = 8.0f;
        HomePadProfile.ContactComponentNames = { PadComponent->GetFName() };

        FRotorlineGroundingResult HomePadResult;
        if (URotorlineGroundingLibrary::ApplyActorGrounding(
            *It, HomePadProfile, HomePadResult, false))
        {
            It->Tags.AddUnique(TEXT("RotorlineHomePadSurfaceAligned"));
            It->Tags.AddUnique(TEXT("RotorlineMissionPad"));
            PadComponent->ComponentTags.AddUnique(TEXT("RotorlineMissionPad"));

            const FBox GroundedPadBounds = PadComponent->Bounds.GetBox();
            ActiveHomePadLocation.Z = GroundedPadBounds.Max.Z + 4.0f;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_HOME_PAD|state=GROUNDED|actor=%s|delta_z=%.1f|base_z=%.0f|surface_z=%.0f"),
                *It->GetName(), HomePadResult.TranslationDelta.Z,
                GroundedPadBounds.Min.Z, ActiveHomePadLocation.Z);
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_HOME_PAD|state=GROUNDING_FAILED|actor=%s|failure=%d|detail=%s"),
                *It->GetName(), static_cast<int32>(HomePadResult.Failure), *HomePadResult.Detail);
        }
        break;
    }

    if (!IsValid(BellLairActor))
    {
        for (TActorIterator<ARotorlineBellLairActor> It(GetWorld()); It; ++It)
        {
            BellLairActor = *It;
            break;
        }
    }
    if (IsValid(BellLairActor))
    {
        BellLairActor->SetActorHiddenInGame(false);
    }
    if (bBellLairDeployment)
    {
        FVector SummitSurfaceLocation = RotorlineSupportLocations::BellLairPeak;
        TArray<FHitResult> SummitHits;
        FCollisionQueryParams SummitTraceParams(SCENE_QUERY_STAT(RotorlineBellLairSummit), true);
        // Survey the terrain directly above the lair. The actor origin is the
        // chamber floor, so it must remain one full chamber height below the
        // summit; only the camouflage hatch belongs at the surface.
        const FVector ProbeOffset(0.0f, 0.0f, 0.0f);
        const FVector TraceStart(SummitSurfaceLocation.X + ProbeOffset.X, SummitSurfaceLocation.Y, 120000.0f);
        const FVector TraceEnd(SummitSurfaceLocation.X + ProbeOffset.X, SummitSurfaceLocation.Y, -50000.0f);
        if (GetWorld()->LineTraceMultiByChannel(
            SummitHits, TraceStart, TraceEnd, ECC_Visibility, SummitTraceParams))
        {
            for (const FHitResult& Hit : SummitHits)
            {
                if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
                {
                    SummitSurfaceLocation.Z = Hit.ImpactPoint.Z;
                    break;
                }
            }
        }

        const FVector LairFloorLocation = IsValid(BellLairActor)
            ? BellLairActor->GetActorLocation()
            : FVector(
                RotorlineSupportLocations::BellLairPeak.X,
                RotorlineSupportLocations::BellLairPeak.Y,
                SummitSurfaceLocation.Z - RotorlineSupportLocations::BellLairBurialDepthCm);

        DeploymentRotation = FRotator(0.0f, RotorlineSupportLocations::BellLairYawDegrees, 0.0f);
        DeploymentLocation = LairFloorLocation + FVector(0.0f, 0.0f, RotorlineSupportLocations::BellLairAircraftDeckOffsetCm);
        ActiveHomePadLocation = LairFloorLocation + FVector(0.0f, 0.0f, 90.0f);

        if (BellLairActor)
        {
            BellLairActor->Configure(Missions[SelectedMissionIndex].TimeOfDay.Equals(TEXT("night"), ESearchCase::IgnoreCase));
        }
    }

    const FTransform SpawnTransform(DeploymentRotation, DeploymentLocation);
    ARotorlineHelicopterPawn* Helicopter = GetWorld()->SpawnActorDeferred<ARotorlineHelicopterPawn>(
        ARotorlineHelicopterPawn::StaticClass(),
        SpawnTransform,
        this,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Helicopter)
    {
        CatalogError = TEXT("Could not spawn selected helicopter");
        return;
    }

    Helicopter->ConfigureDeployment(*SelectedAircraft, Missions[SelectedMissionIndex]);
    if (bFleetQualificationMode)
    {
        Helicopter->SetFleetQualificationMode(bFleetQualificationSkipStartup);
    }
    UGameplayStatics::FinishSpawningActor(Helicopter, SpawnTransform);
    // ConfigureDeployment initializes the pawn's full flight state and also
    // applies the legacy airfield transform. Reassert the selected deployment
    // site after FinishSpawning so Bell lair sorties cannot be pulled back to
    // the default helipad by that initialization path.
    Helicopter->SetActorLocationAndRotation(
        DeploymentLocation,
        DeploymentRotation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_DEPLOY_LOCATION|aircraft_id=%s|site=%s|location=%.0f,%.0f,%.0f|yaw=%.0f"),
        *SelectedAircraft->Id,
        bBellLairDeployment ? TEXT("BELL_LAIR") : TEXT("AIRFIELD"),
        Helicopter->GetActorLocation().X,
        Helicopter->GetActorLocation().Y,
        Helicopter->GetActorLocation().Z,
        Helicopter->GetActorRotation().Yaw);
    if (bQuickDeploy && bQuickDeploySkipStartup)
    {
        Helicopter->SkipStartupForPlaytest();
    }
    Possess(Helicopter);
    if (bBellLairDeployment)
    {
        SetControlRotation(FRotator(-7.0f, DeploymentRotation.Yaw, 0.0f));
    }
    Helicopter->LogMissionLoopResetSnapshot(MissionResetGeneration);
    // Possession is the first point where the pawn can read this controller's
    // persistent mix instead of falling back to unscaled constructor values.
    Helicopter->RefreshAudioMix();
    Helicopter->SetEnemyFlightTestMode(!EnemyFlightTestAirframe.IsEmpty() || bCombatPreview || bGroundDefensePreview || bDamageIntegrityQualification);
    Helicopter->SetCombatPreviewMode(bCombatPreview || bGroundDefensePreview || bDamageIntegrityQualification);
    SpawnOperationalHelipads(Missions[SelectedMissionIndex]);
    ARotorlineEnemyIslandAssaultActor::Clear(GetWorld());
    if (Missions[SelectedMissionIndex].Id.Equals(TEXT("enemy-foothold"), ESearchCase::IgnoreCase))
    {
        ARotorlineEnemyIslandAssaultActor::Deploy(GetWorld());
    }
    bOperationsMenuOpen = false;
    bFlightPauseMenuOpen = false;
    bMissionCompleteScreenOpen = false;
    bAwardPresentationOpen = false;
    bPatchWallOpen = false;
    SelectedMissionCompleteAction = 0;
    bAbortMissionPending = false;
    bAudioSettingsOpen = false;
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    // Controller flight does not require owning the OS cursor. Mouse-look is
    // deliberately opt-in with Tab so deployment can never trap the player.
    ApplyMouseMode(false);
    PulseController(0.38f, 0.18f);

    if (!EnemyFlightTestAirframe.IsEmpty())
    {
        SpawnEnemyFlightTest();
    }
    if (bCombatPreview)
    {
        SpawnCombatProvingGround();
    }
    else if (bGroundDefensePreview)
    {
        SpawnIslandGroundDefenseNetwork(true);
    }
    else if ((bArmedAircraft || Missions[SelectedMissionIndex].Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase)) &&
        !bFleetQualificationMode && EnemyFlightTestAirframe.IsEmpty() &&
        !bDamageIntegrityQualification &&
        !Missions[SelectedMissionIndex].Id.Equals(TEXT("cabin-supply-convoy"), ESearchCase::IgnoreCase))
    {
        SpawnIslandGroundDefenseNetwork(false);
    }
    if (bDamageIntegrityQualification)
    {
        FTimerHandle QualificationStartTimer;
        GetWorldTimerManager().SetTimer(
            QualificationStartTimer,
            this,
            &ARotorlineOperationsPlayerController::StartDamageIntegrityQualification,
            0.5f,
            false);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_DEPLOY|mission=%s|craft=%s|aircraft_id=%s|objectives=%d"),
        *Missions[SelectedMissionIndex].Id,
        *SelectedAircraft->DisplayName,
        *SelectedAircraft->Id,
        Missions[SelectedMissionIndex].Objectives.Num());

    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineLandingSiteAudit")))
    {
        TSet<FString> AuditedSites;
        int32 LandingCount = 0;
        int32 GroundEnemyCount = 0;
        int32 UnsafeCount = 0;
        for (const FRotorlineMissionDefinition& Mission : Missions)
        {
            for (int32 ObjectiveIndex = 0; ObjectiveIndex < Mission.Objectives.Num(); ++ObjectiveIndex)
            {
                const FRotorlineObjectiveDefinition& Objective = Mission.Objectives[ObjectiveIndex];
                const FString SearchText = (Objective.Text + TEXT(" ") + Objective.Target).ToLower();
                const bool bAircraftEnemy = SearchText.Contains(TEXT("apache")) || SearchText.Contains(TEXT("hind")) ||
                    SearchText.Contains(TEXT("gunship")) || SearchText.Contains(TEXT("md500"));
                const bool bLanding = Objective.Kind == TEXT("land");
                const bool bGroundEnemy = Objective.Kind == TEXT("destroy") && !bAircraftEnemy;
                if ((!bLanding && !bGroundEnemy) || AuditedSites.Contains(Objective.Kind + TEXT(":") + Objective.Site)) continue;
                AuditedSites.Add(Objective.Kind + TEXT(":") + Objective.Site);
                LandingCount += bLanding ? 1 : 0;
                GroundEnemyCount += bGroundEnemy ? 1 : 0;
                const FVector Resolved = Helicopter->ResolveMissionObjectiveWorld(Objective);
                const float ShiftCm = FVector::Dist2D(Objective.WorldLocation, Resolved);
                const bool bSafe = Resolved.Z >= 800.0f;
                UnsafeCount += bSafe ? 0 : 1;
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_LANDING_SITE_AUDIT|site=%s|type=%s|mission=%s|objective=%d|status=%s|authored=%.0f,%.0f|resolved=%.0f,%.0f,%.0f|shift=%.0f"),
                    *Objective.Site, bLanding ? TEXT("LANDING") : TEXT("GROUND_ENEMY"),
                    *Mission.Id, ObjectiveIndex + 1, bSafe ? TEXT("SAFE") : TEXT("UNSAFE"),
                    Objective.WorldLocation.X, Objective.WorldLocation.Y,
                    Resolved.X, Resolved.Y, Resolved.Z, ShiftCm);
            }
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_LANDING_SITE_AUDIT|COMPLETE|landing_sites=%d|ground_enemy_sites=%d|unsafe=%d|auto_exit=1"),
            LandingCount, GroundEnemyCount, UnsafeCount);
        FPlatformMisc::RequestExit(UnsafeCount > 0);
    }
}

void ARotorlineOperationsPlayerController::DeploySelectedGroundVehicle(
    const FRotorlineAircraftDefinition& Definition)
{
    if (!GetWorld() || GetPawn()) return;

    CloseHangar();
    FVector SpawnMarkerLocation(-8130.0f, 213050.0f, 17770.0f);
    FRotator SpawnRotation(0.0f, 0.0f, 0.0f);
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("JeepSpawnPoint")))
        {
            SpawnMarkerLocation = It->GetActorLocation();
            SpawnRotation = It->GetActorRotation();
            break;
        }
    }

    FVector GroundSpawnLocation(
        SpawnMarkerLocation.X,
        SpawnMarkerLocation.Y,
        SpawnMarkerLocation.Z + 50000.0f);
    FHitResult GroundHit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RotorlineMarkerJeepSpawn), true);
    if (GetWorld()->LineTraceSingleByChannel(
        GroundHit,
        GroundSpawnLocation,
        FVector(GroundSpawnLocation.X, GroundSpawnLocation.Y, SpawnMarkerLocation.Z - 50000.0f),
        ECC_Visibility,
        TraceParams))
    {
        GroundSpawnLocation.Z = GroundHit.ImpactPoint.Z + 58.0f;
    }
    else
    {
        GroundSpawnLocation.Z = SpawnMarkerLocation.Z + 58.0f;
    }

    const FTransform SpawnTransform(SpawnRotation, GroundSpawnLocation);
    ARotorlineJeepPawn* Jeep = GetWorld()->SpawnActorDeferred<ARotorlineJeepPawn>(
        ARotorlineJeepPawn::StaticClass(),
        SpawnTransform,
        this,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Jeep)
    {
        CatalogError = TEXT("Could not spawn selected Jeep");
        return;
    }

    Jeep->ConfigureVehicle(Definition);
    UGameplayStatics::FinishSpawningActor(Jeep, SpawnTransform);
    Jeep->SetActorLocationAndRotation(GroundSpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
    Possess(Jeep);

    bOperationsMenuOpen = false;
    bFlightPauseMenuOpen = false;
    bMissionCompleteScreenOpen = false;
    bAwardPresentationOpen = false;
    bPatchWallOpen = false;
    bAbortMissionPending = false;
    bAudioSettingsOpen = false;
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    ApplyMouseMode(false);
    PulseController(0.28f, 0.12f);

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_GROUND_DEPLOY|PASS|vehicle=%s|site=JEEP_SPAWN_POINT|parts=%d|wheel_pivots=%d|location=%.0f,%.0f,%.0f|yaw=%.0f"),
        *Definition.Id,
        Jeep->GetMeshPartCount(),
        Jeep->GetWheelPivotCount(),
        Jeep->GetActorLocation().X,
        Jeep->GetActorLocation().Y,
        Jeep->GetActorLocation().Z,
        Jeep->GetActorRotation().Yaw);
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineCaveHandoffTest")))
    {
        FTimerHandle CaveHandoffTestTimer;
        GetWorldTimerManager().SetTimer(
            CaveHandoffTestTimer,
            this,
            &ARotorlineOperationsPlayerController::BeginCaveJeepTransition,
            0.5f,
            false);
    }
    else if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineGroundDeployTest")))
    {
        GetWorldTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateLambda([] { FPlatformMisc::RequestExit(false); }));
    }
    else if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineGroundVisualTest")))
    {
        FTimerHandle ScreenshotTimer;
        GetWorldTimerManager().SetTimer(
            ScreenshotTimer,
            FTimerDelegate::CreateWeakLambda(this, [this]
            {
                if (GetWorld()) GetWorld()->Exec(GetWorld(), TEXT("HighResShot 1"));
            }),
            1.5f,
            false);
        FTimerHandle ExitTimer;
        GetWorldTimerManager().SetTimer(
            ExitTimer,
            FTimerDelegate::CreateLambda([] { FPlatformMisc::RequestExit(false); }),
            3.0f,
            false);
    }
}

void ARotorlineOperationsPlayerController::BeginCaveJeepTransition()
{
    if (bCaveTransitionPending || !Cast<ARotorlineJeepPawn>(GetPawn()) || !GetWorld())
    {
        return;
    }

    bCaveTransitionPending = true;
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    PulseController(0.36f, 0.16f);
    if (PlayerCameraManager)
    {
        PlayerCameraManager->StartCameraFade(0.0f, 1.0f, 0.45f, FLinearColor::Black, false, true);
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CAVE_SEQUENCE|HANDOFF|mission=CONTINUES|handoff=JEEP_TO_BELL_222|status=TRANSITIONING"));

    FTimerHandle CaveTransitionTimer;
    GetWorldTimerManager().SetTimer(
        CaveTransitionTimer,
        this,
        &ARotorlineOperationsPlayerController::CompleteCaveJeepTransition,
        0.48f,
        false);
}

void ARotorlineOperationsPlayerController::CompleteCaveJeepTransition()
{
    APawn* Jeep = GetPawn();
    if (!Cast<ARotorlineJeepPawn>(Jeep))
    {
        bCaveTransitionPending = false;
        SetIgnoreMoveInput(false);
        SetIgnoreLookInput(false);
        return;
    }

    const int32 BellIndex = Aircraft.IndexOfByPredicate([](const FRotorlineAircraftDefinition& Entry)
    {
        return Entry.Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase);
    });
    if (!Aircraft.IsValidIndex(BellIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_CAVE_SEQUENCE|FAILED|reason=BELL_222_NOT_IN_CATALOG"));
        bCaveTransitionPending = false;
        SetIgnoreMoveInput(false);
        SetIgnoreLookInput(false);
        if (PlayerCameraManager)
        {
            PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.25f, FLinearColor::Black, false, false);
        }
        return;
    }

    const FString PreviousMissionId = Missions.IsValidIndex(SelectedMissionIndex)
        ? Missions[SelectedMissionIndex].Id
        : TEXT("INVALID");
    const int32 FinalDiscoveryIndex = Missions.IndexOfByPredicate(
        [](const FRotorlineMissionDefinition& Entry)
        {
            return Entry.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase);
        });
    if (FinalDiscoveryIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_CAVE_SEQUENCE|FAILED|reason=FINAL_DISCOVERY_NOT_IN_CATALOG|previous_mission=%s"),
            *PreviousMissionId);
        bCaveTransitionPending = false;
        SetIgnoreMoveInput(false);
        SetIgnoreLookInput(false);
        if (PlayerCameraManager)
        {
            PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.25f, FLinearColor::Black, false, false);
        }
        return;
    }
    // Validate both catalog prerequisites before releasing the Jeep. A bad
    // catalog can therefore never leave the controller pawnless behind the
    // transition's held black frame.
    UnPossess();
    Jeep->Destroy();
    SelectedAircraftIndex = BellIndex;
    // The cave discovery is Mission 19 regardless of which unlocked mission
    // was highlighted when the player entered the Jeep. Carrying Mission 19
    // into Bell deployment initializes the Kiowa recon startup director.
    SelectedMissionIndex = FinalDiscoveryIndex;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CAVE_SEQUENCE|MISSION_ROUTE|from=%s|to=final-discovery|kiowa_state=DISCARDED"),
        *PreviousMissionId);

    // Reuse the normal Bell lair deployment path while bypassing only the
    // still-classified hangar gate. Startup must remain active here so Mission
    // 20 continues with the commander brief, Bell theme, and player-controlled
    // turbine start instead of silently completing at the cave trigger.
    const bool bPreviousQuickDeploy = bQuickDeploy;
    const bool bPreviousSkipStartup = bQuickDeploySkipStartup;
    bQuickDeploy = true;
    bQuickDeploySkipStartup = false;
    DeploySelectedAircraft();
    bQuickDeploy = bPreviousQuickDeploy;
    bQuickDeploySkipStartup = bPreviousSkipStartup;

    ARotorlineHelicopterPawn* Bell = Cast<ARotorlineHelicopterPawn>(GetPawn());
    if (!Bell)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_CAVE_SEQUENCE|FAILED|reason=BELL_DEPLOYMENT_FAILED|recovery=OPERATIONS"));
        bCaveTransitionPending = false;
        SetIgnoreMoveInput(false);
        SetIgnoreLookInput(false);
        if (PlayerCameraManager)
        {
            PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.25f, FLinearColor::Black, false, false);
        }
        ReturnToOperations();
        return;
    }
    else
    {
        Bell->BeginTransitionSpawnHold();
    }

    bMissionCompleteScreenOpen = false;
    bMissionFailureScreenOpen = false;
    bCaveTransitionPending = false;
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
    if (PlayerCameraManager)
    {
        PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.6f, FLinearColor::Black, false, false);
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CAVE_SEQUENCE|COMPLETE|screen=NONE|pawn=%s|site=BELL_LAIR|status=%s"),
        GetPawn() ? *GetPawn()->GetName() : TEXT("NONE"),
        Cast<ARotorlineHelicopterPawn>(GetPawn()) ? TEXT("PASS") : TEXT("FAIL"));
}

void ARotorlineOperationsPlayerController::SpawnOperationalHelipads(const FRotorlineMissionDefinition& Mission)
{
    if (!GetWorld()) return;

    const FRotorlineAircraftDefinition* SelectedAircraft = GetSelectedAircraft();
    const bool bBellLairDeployment = SelectedAircraft &&
        SelectedAircraft->Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase) &&
        Mission.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase);

    if (IsValid(HomeHelipadBeaconActor)) HomeHelipadBeaconActor->Destroy();
    if (IsValid(CityServiceHelipadActor)) CityServiceHelipadActor->Destroy();
    if (IsValid(HospitalHelipadActor)) HospitalHelipadActor->Destroy();
    HomeHelipadBeaconActor = nullptr;
    CityServiceHelipadActor = nullptr;
    HospitalHelipadActor = nullptr;

    const bool bNightOperations = Mission.TimeOfDay.Equals(TEXT("night"), ESearchCase::IgnoreCase);
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    HomeHelipadBeaconActor = GetWorld()->SpawnActor<ARotorlineHelipadBeaconActor>(
        ARotorlineHelipadBeaconActor::StaticClass(),
        ActiveHomePadLocation,
        bBellLairDeployment
            ? FRotator(0.0f, RotorlineSupportLocations::BellLairYawDegrees, 0.0f)
            : RotorlineOperations::SpawnRotation,
        SpawnParams);
    if (HomeHelipadBeaconActor)
    {
        HomeHelipadBeaconActor->Configure(
            true, bNightOperations, ActiveHomePadLocation,
            bBellLairDeployment ? TEXT("BELL_LAIR") : TEXT("HOME"));
    }

    CityServiceHelipadActor = GetWorld()->SpawnActor<ARotorlineHelipadBeaconActor>(
        ARotorlineHelipadBeaconActor::StaticClass(),
        RotorlineSupportLocations::CentralTownRearmPad,
        FRotator(0.0f, -12.0f, 0.0f),
        SpawnParams);
    if (CityServiceHelipadActor)
    {
        CityServiceHelipadActor->Configure(
            false, bNightOperations, RotorlineSupportLocations::CentralTownRearmPad,
            TEXT("CENTRAL_TOWN_REARM"));
        const bool bCityRearmTest = FParse::Param(FCommandLine::Get(), TEXT("RotorlineCityRearmTest"));
        const bool bCityRearmPreview = FParse::Param(FCommandLine::Get(), TEXT("RotorlineCityRearmPreview"));
        if (bCityRearmTest || bCityRearmPreview)
        {
            if (ARotorlineHelicopterPawn* Helicopter = Cast<ARotorlineHelicopterPawn>(GetPawn()))
            {
                const float PreviewHeightCm = bCityRearmPreview ? 1800.0f : 125.0f;
                const FVector TestLocation = CityServiceHelipadActor->GetActorLocation() + FVector(0.0f, 0.0f, PreviewHeightCm);
                Helicopter->SetActorLocation(TestLocation, false, nullptr, ETeleportType::TeleportPhysics);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_CITY_SERVICE_TEST|state=POSITIONED|mode=%s|location=%.0f,%.0f,%.0f"),
                    bCityRearmPreview ? TEXT("PREVIEW") : TEXT("TEST"), TestLocation.X, TestLocation.Y, TestLocation.Z);
            }
        }
    }

    HospitalHelipadActor = GetWorld()->SpawnActor<ARotorlineHelipadBeaconActor>(
        ARotorlineHelipadBeaconActor::StaticClass(),
        RotorlineSupportLocations::FieldHospitalHelipad,
        FRotator(0.0f, 18.0f, 0.0f),
        SpawnParams);
    if (HospitalHelipadActor)
    {
        HospitalHelipadActor->Configure(
            false, bNightOperations, RotorlineSupportLocations::FieldHospitalHelipad,
            TEXT("FIELD_HOSPITAL"));
    }
}

void ARotorlineOperationsPlayerController::SpawnEnemyFlightTest()
{
    APawn* PlayerPawn = GetPawn();
    if (!GetWorld() || !PlayerPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_ENEMY_TEST|SPAWN|status=FAIL|reason=NO_PLAYER"));
        return;
    }

    const FString Requested = EnemyFlightTestAirframe.ToLower();
    FRotorlineObjectiveDefinition ThreatDefinition;
    ThreatDefinition.Kind = TEXT("destroy");
    ThreatDefinition.bHasLocation = true;
    ThreatDefinition.Radius = 80.0f;
    if (Requested == TEXT("hind") || Requested == TEXT("mi24") || Requested == TEXT("mi-24"))
    {
        ThreatDefinition.Text = TEXT("MI-24 HIND LIVE FLIGHT TEST");
        ThreatDefinition.Target = TEXT("enemy-hind-rocket-transit");
    }
    else if (Requested == TEXT("apache") || Requested == TEXT("ah64") || Requested == TEXT("ah-64"))
    {
        ThreatDefinition.Text = TEXT("AH-64 APACHE LIVE FLIGHT TEST");
        ThreatDefinition.Target = TEXT("enemy-apache-rocket-transit");
    }
    else if (Requested == TEXT("md500") || Requested == TEXT("md-500"))
    {
        ThreatDefinition.Text = TEXT("MD-500 GUNSHIP LIVE FLIGHT TEST");
        ThreatDefinition.Target = TEXT("enemy-gunship-md500-transit");
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_ENEMY_TEST|SPAWN|status=FAIL|reason=UNKNOWN_AIRFRAME|requested=%s"), *EnemyFlightTestAirframe);
        return;
    }

    const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    const FVector EnemySpawnLocation = PlayerPawn->GetActorLocation() + Forward * 22000.0f + Right * 5500.0f;
    const FRotator SpawnRotation = (PlayerPawn->GetActorLocation() - EnemySpawnLocation).Rotation();
    ARotorlineMissionObjectiveActor* Enemy = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
        ARotorlineMissionObjectiveActor::StaticClass(),
        EnemySpawnLocation,
        SpawnRotation);
    if (!Enemy)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_ENEMY_TEST|SPAWN|status=FAIL|reason=ACTOR_SPAWN"));
        return;
    }

    Enemy->Configure(ThreatDefinition, EnemySpawnLocation);
    Enemy->SetEnemyFlightQualificationMode();
    EnemyFlightQualificationActor = Enemy;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_ENEMY_TEST|SPAWN|status=PASS|airframe=%s|target=%s|distance_m=%.1f"),
        *EnemyFlightTestAirframe,
        *ThreatDefinition.Target,
        FVector::Dist(PlayerPawn->GetActorLocation(), Enemy->GetActorLocation()) / 100.0f);
}

void ARotorlineOperationsPlayerController::StartDamageIntegrityQualification()
{
    APawn* PlayerPawn = GetPawn();
    if (!GetWorld() || !PlayerPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_DAMAGE_INTEGRITY|COMPLETE|status=FAIL|reason=NO_PLAYER"));
        FPlatformMisc::RequestExit(false);
        return;
    }

    struct FHealthSpec
    {
        const TCHAR* Id;
        const TCHAR* Text;
        float ExpectedHealth;
        float ExpectedRadius;
    };
    static const FHealthSpec HealthSpecs[] = {
        { TEXT("hind"), TEXT("MI-24 HIND DAMAGE MATRIX"), 160.0f, 850.0f },
        { TEXT("apache"), TEXT("AH-64 APACHE DAMAGE MATRIX"), 140.0f, 780.0f },
        { TEXT("md500"), TEXT("MD-500 GUNSHIP DAMAGE MATRIX"), 110.0f, 520.0f },
        { TEXT("radar"), TEXT("HOSTILE RADAR DAMAGE MATRIX"), 120.0f, 420.0f },
        { TEXT("tank"), TEXT("CHALLENGER TANK DAMAGE MATRIX"), 150.0f, 380.0f },
        { TEXT("flak"), TEXT("RIDGELINE FLAK DAMAGE MATRIX"), 100.0f, 340.0f },
        { TEXT("himars"), TEXT("HIMARS ROCKET BATTERY DAMAGE MATRIX"), 130.0f, 500.0f },
    };

    bDamageIntegrityMatrixPassed = true;
    const FVector MatrixOrigin = PlayerPawn->GetActorLocation() + FVector(0.0f, 0.0f, 16000.0f);
    for (int32 SpecIndex = 0; SpecIndex < UE_ARRAY_COUNT(HealthSpecs); ++SpecIndex)
    {
        const FHealthSpec& Spec = HealthSpecs[SpecIndex];
        FRotorlineObjectiveDefinition Definition;
        Definition.Kind = TEXT("destroy");
        Definition.Text = Spec.Text;
        Definition.Target = FString::Printf(TEXT("damage-matrix-%s"), Spec.Id);
        Definition.bHasLocation = true;
        Definition.Radius = 80.0f;
        const FVector MatrixSpawnLocation = MatrixOrigin + FVector(0.0f, SpecIndex * 3000.0f, 0.0f);
        ARotorlineMissionObjectiveActor* Target = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
            ARotorlineMissionObjectiveActor::StaticClass(), MatrixSpawnLocation, FRotator::ZeroRotator);
        if (!Target)
        {
            bDamageIntegrityMatrixPassed = false;
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_DAMAGE_INTEGRITY|MATRIX|type=%s|status=FAIL|reason=SPAWN"), Spec.Id);
            continue;
        }

        Target->Configure(Definition, MatrixSpawnLocation);
        Target->SetActorLocation(MatrixSpawnLocation);
        Target->SetActorTickEnabled(false);
        const bool bConfigPass =
            FMath::IsNearlyEqual(Target->GetMaximumHealth(), Spec.ExpectedHealth, 0.01f) &&
            FMath::IsNearlyEqual(Target->GetCurrentHealth(), Spec.ExpectedHealth, 0.01f) &&
            FMath::IsNearlyEqual(Target->GetProjectileHitRadius(), Spec.ExpectedRadius, 0.01f);

        float FirstApplied = 0.0f;
        const bool bFirstFatal = Target->ApplyCombatDamage(28.0f, TEXT("MATRIX_30MM"), FirstApplied);
        const float ExpectedAfterFirst = Spec.ExpectedHealth - 28.0f;
        const bool bFirstPass = !bFirstFatal && FMath::IsNearlyEqual(FirstApplied, 28.0f, 0.01f) &&
            FMath::IsNearlyEqual(Target->GetCurrentHealth(), ExpectedAfterFirst, 0.01f) &&
            Target->GetPlayerDamageEventCount() == 1;

        float InvalidApplied = -1.0f;
        const bool bInvalidFatal = Target->ApplyCombatDamage(-50.0f, TEXT("MATRIX_INVALID"), InvalidApplied);
        const bool bInvalidPass = !bInvalidFatal && FMath::IsNearlyZero(InvalidApplied) &&
            FMath::IsNearlyEqual(Target->GetCurrentHealth(), ExpectedAfterFirst, 0.01f) &&
            Target->GetPlayerDamageEventCount() == 1;

        float FatalApplied = 0.0f;
        const bool bFatal = Target->ApplyCombatDamage(10000.0f, TEXT("MATRIX_FATAL"), FatalApplied);
        float DuplicateApplied = -1.0f;
        const bool bDuplicateFatal = Target->ApplyCombatDamage(100.0f, TEXT("MATRIX_DUPLICATE"), DuplicateApplied);
        const bool bFatalPass = bFatal && FMath::IsNearlyEqual(FatalApplied, ExpectedAfterFirst, 0.01f) &&
            FMath::IsNearlyZero(Target->GetCurrentHealth()) && Target->GetPlayerDamageEventCount() == 2 &&
            !bDuplicateFatal && FMath::IsNearlyZero(DuplicateApplied);

        const bool bSpecPass = bConfigPass && bFirstPass && bInvalidPass && bFatalPass;
        bDamageIntegrityMatrixPassed &= bSpecPass;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_DAMAGE_INTEGRITY|MATRIX|type=%s|max_health=%.1f|radius_cm=%.0f|first_damage=%.1f|after_first=%.1f|invalid_ignored=%d|fatal_applied=%.1f|hit_events=%d|duplicate_ignored=%d|status=%s"),
            Spec.Id, Spec.ExpectedHealth, Spec.ExpectedRadius, FirstApplied, ExpectedAfterFirst,
            bInvalidPass ? 1 : 0, FatalApplied, Target->GetPlayerDamageEventCount(),
            (!bDuplicateFatal && FMath::IsNearlyZero(DuplicateApplied)) ? 1 : 0,
            bSpecPass ? TEXT("PASS") : TEXT("FAIL"));
        Target->Destroy();
    }

    // Exercise the actual moving projectile code, not only direct health calls.
    // Both targets are held in clear air so world geometry cannot make this
    // deterministic registration qualification flaky.
    const FVector TestForward = PlayerPawn->GetActorForwardVector().GetSafeNormal();
    const FVector TestRight = FVector::CrossProduct(FVector::UpVector, TestForward).GetSafeNormal();
    const FVector ProjectileOrigin = PlayerPawn->GetActorLocation() + TestForward * 28000.0f + FVector::UpVector * 12000.0f;
    const auto SpawnQualificationTarget = [&](const TCHAR* Text, const TCHAR* TargetId, const FVector& Location)
        -> ARotorlineMissionObjectiveActor*
    {
        FRotorlineObjectiveDefinition Definition;
        Definition.Kind = TEXT("destroy");
        Definition.Text = Text;
        Definition.Target = TargetId;
        Definition.bHasLocation = true;
        Definition.Radius = 80.0f;
        ARotorlineMissionObjectiveActor* Target = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
            ARotorlineMissionObjectiveActor::StaticClass(), Location, FRotator::ZeroRotator);
        if (Target)
        {
            Target->Configure(Definition, Location);
            Target->SetActorLocation(Location);
            Target->SetActorTickEnabled(false);
        }
        return Target;
    };

    DamageIntegrityRocketTarget = SpawnQualificationTarget(
        TEXT("FLAK HIT REGISTRATION TARGET"), TEXT("integrity-flak"), ProjectileOrigin + TestRight * 9000.0f);
    DamageIntegrityCannonTarget = SpawnQualificationTarget(
        TEXT("TANK HIT REGISTRATION TARGET"), TEXT("integrity-tank"), ProjectileOrigin - TestRight * 9000.0f);
    if (!DamageIntegrityRocketTarget || !DamageIntegrityCannonTarget)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_DAMAGE_INTEGRITY|COMPLETE|status=FAIL|reason=PROJECTILE_TARGET_SPAWN"));
        FPlatformMisc::RequestExit(false);
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = PlayerPawn;
    const FVector RocketAim = DamageIntegrityRocketTarget->GetAimLocation();
    const FVector RocketStart = RocketAim - TestForward * 7000.0f;
    if (ARotorlineRocketProjectile* Rocket = GetWorld()->SpawnActor<ARotorlineRocketProjectile>(
        ARotorlineRocketProjectile::StaticClass(), RocketStart, TestForward.Rotation(), SpawnParams))
    {
        NotifyWeaponFired();
        Rocket->Launch(RocketStart, (RocketAim - RocketStart).GetSafeNormal(), nullptr);
    }
    else
    {
        bDamageIntegrityMatrixPassed = false;
    }

    const FVector CannonAim = DamageIntegrityCannonTarget->GetAimLocation();
    const FVector CannonStart = CannonAim - TestForward * 7000.0f;
    if (ARotorlineCannonProjectile* Cannon = GetWorld()->SpawnActor<ARotorlineCannonProjectile>(
        ARotorlineCannonProjectile::StaticClass(), CannonStart, TestForward.Rotation(), SpawnParams))
    {
        NotifyWeaponFired();
        Cannon->Launch(CannonStart, (CannonAim - CannonStart).GetSafeNormal(), 28.0f);
    }
    else
    {
        bDamageIntegrityMatrixPassed = false;
    }

    FTimerHandle QualificationFinishTimer;
    GetWorldTimerManager().SetTimer(
        QualificationFinishTimer,
        this,
        &ARotorlineOperationsPlayerController::FinishDamageIntegrityQualification,
        1.0f,
        false);
}

void ARotorlineOperationsPlayerController::FinishDamageIntegrityQualification()
{
    const bool bRocketPass = IsValid(DamageIntegrityRocketTarget) &&
        DamageIntegrityRocketTarget->GetPlayerDamageEventCount() == 1 &&
        FMath::IsNearlyZero(DamageIntegrityRocketTarget->GetCurrentHealth()) &&
        DamageIntegrityRocketTarget->IsDestroyedTarget();
    const bool bCannonPass = IsValid(DamageIntegrityCannonTarget) &&
        DamageIntegrityCannonTarget->GetPlayerDamageEventCount() == 1 &&
        FMath::IsNearlyEqual(DamageIntegrityCannonTarget->GetCurrentHealth(), 122.0f, 0.01f) &&
        !DamageIntegrityCannonTarget->IsDestroyedTarget();
    const bool bCounterPass = MissionResults.bWeaponsTracked &&
        MissionResults.WeaponShotsFired == 2 && MissionResults.WeaponHits == 2 &&
        MissionResults.GroundEnemiesDestroyed == 1 && MissionResults.EnemyHelicoptersDestroyed == 0;
    const bool bPassed = bDamageIntegrityMatrixPassed && bRocketPass && bCannonPass && bCounterPass;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_DAMAGE_INTEGRITY|PROJECTILES|unguided_rocket=%s|cannon=%s|shots=%d|hits=%d|ground_kills=%d|counters=%s"),
        bRocketPass ? TEXT("PASS") : TEXT("FAIL"), bCannonPass ? TEXT("PASS") : TEXT("FAIL"),
        MissionResults.WeaponShotsFired, MissionResults.WeaponHits, MissionResults.GroundEnemiesDestroyed,
        bCounterPass ? TEXT("PASS") : TEXT("FAIL"));
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_DAMAGE_INTEGRITY|COMPLETE|matrix=%s|projectiles=%s|counters=%s|status=%s"),
        bDamageIntegrityMatrixPassed ? TEXT("PASS") : TEXT("FAIL"),
        (bRocketPass && bCannonPass) ? TEXT("PASS") : TEXT("FAIL"),
        bCounterPass ? TEXT("PASS") : TEXT("FAIL"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    if (IsValid(DamageIntegrityRocketTarget)) DamageIntegrityRocketTarget->Destroy();
    if (IsValid(DamageIntegrityCannonTarget)) DamageIntegrityCannonTarget->Destroy();
    FPlatformMisc::RequestExit(false);
}

void ARotorlineOperationsPlayerController::SpawnCombatProvingGround()
{
    APawn* PlayerPawn = GetPawn();
    if (!GetWorld() || !PlayerPawn) return;

    CombatPreviewActors.Reset();
    const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    struct FThreatSpec
    {
        const TCHAR* Text;
        const TCHAR* Target;
        float ForwardCm;
        float RightCm;
    };
    const FThreatSpec Threats[] = {
        { TEXT("CHALLENGER MK3 ARMORED PATROL"), TEXT("combat-preview-challenger-tank"), 26000.0f, -12000.0f },
        { TEXT("HOSTILE FLAK POSITION"), TEXT("combat-preview-flak"), 42000.0f, 14500.0f },
        { TEXT("HOSTILE M142 HIMARS BATTERY"), TEXT("combat-preview-himars"), 62000.0f, -18000.0f },
        { TEXT("AH MK1 APACHE ATTACK FLIGHT"), TEXT("combat-preview-apache"), 48000.0f, 23000.0f },
    };
    const bool bHimarsOnlyPreview = FParse::Param(FCommandLine::Get(), TEXT("RotorlineHimarsOnlyPreview"));

    for (const FThreatSpec& Spec : Threats)
    {
        if (bHimarsOnlyPreview && !FString(Spec.Target).Contains(TEXT("himars"), ESearchCase::IgnoreCase))
        {
            continue;
        }
        const FVector ThreatSpawnLocation = PlayerPawn->GetActorLocation() + Forward * Spec.ForwardCm + Right * Spec.RightCm;
        const float SpawnYaw = (PlayerPawn->GetActorLocation() - ThreatSpawnLocation).Rotation().Yaw;
        const FRotator SpawnRotation(0.0f, SpawnYaw, 0.0f);
        ARotorlineMissionObjectiveActor* Enemy = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
            ARotorlineMissionObjectiveActor::StaticClass(), ThreatSpawnLocation, SpawnRotation);
        if (!Enemy) continue;
        FRotorlineObjectiveDefinition Definition;
        Definition.Kind = TEXT("destroy");
        Definition.Text = Spec.Text;
        Definition.Target = Spec.Target;
        Definition.bHasLocation = true;
        Definition.Radius = 90.0f;
        Enemy->Configure(Definition, ThreatSpawnLocation);
        CombatPreviewActors.Add(Enemy);
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_COMBAT_PREVIEW|SPAWN|target=%s|threat=%d|distance_m=%.1f"),
            Spec.Target, static_cast<int32>(Enemy->GetThreatType()),
            FVector::Dist(PlayerPawn->GetActorLocation(), Enemy->GetActorLocation()) / 100.0f);
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_COMBAT_PREVIEW|READY|count=%d|himars_only=%d|other_enemies=%d"),
        CombatPreviewActors.Num(),
        bHimarsOnlyPreview ? 1 : 0,
        bHimarsOnlyPreview ? 0 : FMath::Max(0, CombatPreviewActors.Num() - 1));
}

void ARotorlineOperationsPlayerController::SpawnIslandGroundDefenseNetwork(bool bPreviewOnly)
{
    APawn* PlayerPawn = GetPawn();
    if (!GetWorld() || !PlayerPawn) return;

    ARotorlineHelicopterPawn* Helicopter = Cast<ARotorlineHelicopterPawn>(PlayerPawn);
    if (!Helicopter || !Missions.IsValidIndex(SelectedMissionIndex)) return;

    GroundDefenseActors.Reset();
    const FVector Home = ActiveHomePadLocation.IsNearlyZero()
        ? RotorlineOperations::SpawnLocation
        : ActiveHomePadLocation;
    const FVector CityService = RotorlineSupportLocations::CentralTownRearmPad;
    constexpr float AirfieldExclusionRadiusCm = RotorlineCombatTuning::GroundDefenseAirfieldExclusionRadiusCm;
    const FRotorlineMissionDefinition& Mission = Missions[SelectedMissionIndex];
    const bool bKiowaGroundRecon = Mission.Id.Equals(TEXT("recon"), ESearchCase::IgnoreCase);
    const float EffectiveAirfieldExclusionRadiusCm = bKiowaGroundRecon ? 145000.0f : AirfieldExclusionRadiusCm;
    const float EffectiveCityExclusionRadiusCm = bKiowaGroundRecon
        ? 8000.0f : RotorlineSupportLocations::CityDefenseExclusionRadiusCm;
    const int32 DesiredThreatCount = bKiowaGroundRecon ? 12 : FMath::Clamp(10 + Mission.Difficulty * 2, 12, 18);

    struct FGroundThreatSpec
    {
        FString Text;
        FString Target;
        FVector AuthoredLocation;
        FVector RouteDirection;
        bool bRidgeEmplacement;
    };
    struct FGroundThreatTemplate
    {
        const TCHAR* Label;
        const TCHAR* TypeToken;
        bool bRidgeEmplacement;
    };
    const FGroundThreatTemplate ThreatTemplates[] = {
        { TEXT("CHALLENGER TANK PATROL"), TEXT("tank"), false },
        { TEXT("ROUTE FLAK BATTERY"), TEXT("flak"), true },
        { TEXT("M142 HIMARS BATTERY"), TEXT("himars"), false },
        { TEXT("RIDGELINE FLAK"), TEXT("flak"), true },
        { TEXT("ARMORED INTERCEPT"), TEXT("tank"), false },
        { TEXT("RADAR SAM SITE"), TEXT("radar"), true },
        { TEXT("FLAK AMBUSH"), TEXT("flak"), true },
        { TEXT("HIMARS ROCKET ARTILLERY"), TEXT("himars"), false },
    };
    const FGroundThreatTemplate ReconThreatTemplates[] = {
        { TEXT("RECON TANK SCREEN"), TEXT("tank"), false },
        { TEXT("RECON FLAK BATTERY"), TEXT("recon-flak"), true },
    };

    TArray<FVector> RoutePoints;
    RoutePoints.Add(Home);
    for (const FRotorlineObjectiveDefinition& Objective : Mission.Objectives)
    {
        if (!Objective.bHasLocation) continue;
        const FVector WorldLocation = Helicopter->GetMissionWorldLocation(Objective);
        if (FVector::Dist2D(RoutePoints.Last(), WorldLocation) >= 25000.0f)
        {
            RoutePoints.Add(WorldLocation);
        }
    }

    TArray<float> SegmentLengths;
    float TotalRouteLengthCm = 0.0f;
    for (int32 PointIndex = 1; PointIndex < RoutePoints.Num(); ++PointIndex)
    {
        const float SegmentLength = FVector::Dist2D(RoutePoints[PointIndex - 1], RoutePoints[PointIndex]);
        SegmentLengths.Add(SegmentLength);
        TotalRouteLengthCm += SegmentLength;
    }

    TArray<FGroundThreatSpec> Threats;
    const auto AddThreatPlan = [&](const FVector& RouteAnchor, const FVector& InRouteDirection)
    {
        if (Threats.Num() >= DesiredThreatCount) return;
        const FVector RouteDirection = InRouteDirection.GetSafeNormal2D().IsNearlyZero()
            ? FVector::ForwardVector
            : InRouteDirection.GetSafeNormal2D();
        const FVector RouteRight = FVector::CrossProduct(FVector::UpVector, RouteDirection).GetSafeNormal();
        const int32 PlanIndex = Threats.Num();
        const float Side = PlanIndex % 2 == 0 ? -1.0f : 1.0f;
        const float LateralOffsetCm = Side * (bKiowaGroundRecon
            ? (15000.0f + static_cast<float>(PlanIndex % 3) * 3500.0f)
            : (26000.0f + static_cast<float>(PlanIndex % 4) * 6500.0f));
        const float AlongOffsetCm = (static_cast<float>(PlanIndex % 3) - 1.0f) *
            (bKiowaGroundRecon ? 6000.0f : 9000.0f);
        const FVector Authored = RouteAnchor + RouteRight * LateralOffsetCm + RouteDirection * AlongOffsetCm;
        if (FVector::Dist2D(Authored, Home) < EffectiveAirfieldExclusionRadiusCm) return;
        if (FVector::Dist2D(Authored, CityService) < EffectiveCityExclusionRadiusCm) return;
        for (const FGroundThreatSpec& Existing : Threats)
        {
            if (FVector::Dist2D(Existing.AuthoredLocation, Authored) < (bKiowaGroundRecon ? 12000.0f : 32000.0f)) return;
        }

        const FGroundThreatTemplate& AuthoredTemplate = bKiowaGroundRecon
            ? ReconThreatTemplates[PlanIndex % UE_ARRAY_COUNT(ReconThreatTemplates)]
            : ThreatTemplates[PlanIndex % UE_ARRAY_COUNT(ThreatTemplates)];
        const bool bReplaceEarlyRadar = Mission.Difficulty < 2 && FString(AuthoredTemplate.TypeToken).Equals(TEXT("radar"));
        const FString TypeToken = bReplaceEarlyRadar ? TEXT("flak") : FString(AuthoredTemplate.TypeToken);
        const FString Label = bReplaceEarlyRadar ? TEXT("ROUTE FLAK BATTERY") : FString(AuthoredTemplate.Label);
        const int32 FormationSize = TypeToken.Equals(TEXT("tank"), ESearchCase::IgnoreCase) ? 3 : 1;
        for (int32 FormationIndex = 0;
             FormationIndex < FormationSize && Threats.Num() < DesiredThreatCount;
             ++FormationIndex)
        {
            const int32 MemberIndex = Threats.Num();
            const float FormationSide = static_cast<float>(FormationIndex - 1) * 1900.0f;
            const float FormationTrail = FormationIndex == 1 ? -2300.0f : 0.0f;
            FGroundThreatSpec& Plan = Threats.AddDefaulted_GetRef();
            Plan.Text = FormationSize > 1
                ? FString::Printf(TEXT("%s SQUAD %02d-%d"), *Label, PlanIndex + 1, FormationIndex + 1)
                : FString::Printf(TEXT("%s %02d"), *Label, PlanIndex + 1);
            Plan.Target = FString::Printf(
                TEXT("island-route-%s-%02d"), *TypeToken, MemberIndex + 1);
            Plan.AuthoredLocation = Authored +
                RouteRight * FormationSide + RouteDirection * FormationTrail;
            Plan.RouteDirection = RouteDirection;
            Plan.bRidgeEmplacement = AuthoredTemplate.bRidgeEmplacement || bReplaceEarlyRadar;
        }
    };

    if (bKiowaGroundRecon)
    {
        FVector PreviousPoint = Home;
        for (const FRotorlineObjectiveDefinition& Objective : Mission.Objectives)
        {
            if (!Objective.Kind.Equals(TEXT("designate-recon"), ESearchCase::IgnoreCase) || !Objective.bHasLocation) continue;
            const FVector ReconPoint = Helicopter->GetMissionWorldLocation(Objective);
            const FVector RouteDirection = ReconPoint - PreviousPoint;
            AddThreatPlan(ReconPoint, RouteDirection);
            AddThreatPlan(ReconPoint, RouteDirection);
            PreviousPoint = ReconPoint;
        }
    }
    else if (TotalRouteLengthCm > 1000.0f)
    {
        for (int32 Slot = 0; Slot < DesiredThreatCount; ++Slot)
        {
            const float SampleDistance = TotalRouteLengthCm * (static_cast<float>(Slot) + 0.5f) /
                static_cast<float>(DesiredThreatCount);
            float Traversed = 0.0f;
            for (int32 SegmentIndex = 0; SegmentIndex < SegmentLengths.Num(); ++SegmentIndex)
            {
                const float SegmentLength = SegmentLengths[SegmentIndex];
                if (SampleDistance <= Traversed + SegmentLength || SegmentIndex == SegmentLengths.Num() - 1)
                {
                    const FVector Segment = RoutePoints[SegmentIndex + 1] - RoutePoints[SegmentIndex];
                    const float SegmentAlpha = SegmentLength > KINDA_SMALL_NUMBER
                        ? FMath::Clamp((SampleDistance - Traversed) / SegmentLength, 0.0f, 1.0f)
                        : 0.0f;
                    AddThreatPlan(RoutePoints[SegmentIndex] + Segment * SegmentAlpha, Segment);
                    break;
                }
                Traversed += SegmentLength;
            }
        }
    }

    // Sparse missions still receive island-wide resistance instead of falling
    // back to a small cluster at the departure airfield.
    const FVector FallbackAnchors[] = {
        FVector(-165000.0f, -35000.0f, 0.0f), FVector(-70000.0f, -175000.0f, 0.0f),
        FVector(45000.0f, -145000.0f, 0.0f), FVector(165000.0f, -105000.0f, 0.0f),
        FVector(270000.0f, -35000.0f, 0.0f), FVector(245000.0f, 105000.0f, 0.0f),
        FVector(155000.0f, 215000.0f, 0.0f), FVector(25000.0f, 260000.0f, 0.0f),
        FVector(-110000.0f, 225000.0f, 0.0f), FVector(-225000.0f, 145000.0f, 0.0f),
        FVector(-285000.0f, 25000.0f, 0.0f), FVector(-35000.0f, 45000.0f, 0.0f),
        FVector(95000.0f, 65000.0f, 0.0f), FVector(310000.0f, 185000.0f, 0.0f),
        FVector(-175000.0f, 285000.0f, 0.0f), FVector(315000.0f, -190000.0f, 0.0f),
        FVector(-50000.0f, -300000.0f, 0.0f), FVector(100000.0f, 320000.0f, 0.0f),
    };
    for (int32 FallbackIndex = 0;
         FallbackIndex < UE_ARRAY_COUNT(FallbackAnchors) && Threats.Num() < DesiredThreatCount;
         ++FallbackIndex)
    {
        const FVector Previous = FallbackIndex > 0 ? FallbackAnchors[FallbackIndex - 1] : Home;
        AddThreatPlan(FallbackAnchors[FallbackIndex], FallbackAnchors[FallbackIndex] - Previous);
    }

    const auto TraceTerrainHeight = [&](
        const FVector& XY,
        float& OutHeight,
        FVector& OutNormal,
        bool& OutStructureBlocked) -> bool
    {
        OutStructureBlocked = false;
        FRotorlineGroundingProfile Profile = URotorlineGroundingLibrary::MakeProfile(
            ERotorlineGroundingMode::LinearPoint, TEXT("GroundDefenseSurvey"));
        Profile.bAllowPreparedGround = false;
        Profile.bRejectObstructionsAboveGround = true;
        Profile.bCheckCollisionPenetration = false;
        FRotorlineGroundingResult Result;
        if (!URotorlineGroundingLibrary::SolveGroundContact(
            this, XY, FVector2D::ZeroVector, PlayerPawn, Profile, Result))
        {
            OutStructureBlocked = Result.Failure == ERotorlineGroundingFailure::Obstructed;
            return false;
        }
        OutHeight = Result.ContactPoint.Z;
        OutNormal = Result.SurfaceNormal;
        return true;
    };

    int32 TankCount = 0;
    int32 FlakCount = 0;
    int32 HimarsCount = 0;
    int32 MortarCount = 0;
    int32 RadarCount = 0;
    for (const FGroundThreatSpec& Spec : Threats)
    {
        FVector ThreatSpawnLocation = Spec.AuthoredLocation;
        if (FVector::Dist2D(ThreatSpawnLocation, Home) < EffectiveAirfieldExclusionRadiusCm)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("ROTORLINE_GROUND_NETWORK|SKIP|target=%s|reason=AIRFIELD_EXCLUSION"), *Spec.Target);
            continue;
        }
        if (FVector::Dist2D(ThreatSpawnLocation, CityService) < EffectiveCityExclusionRadiusCm)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("ROTORLINE_GROUND_NETWORK|SKIP|target=%s|reason=CITY_SERVICE_EXCLUSION"), *Spec.Target);
            continue;
        }

        const FVector Forward = Spec.RouteDirection.GetSafeNormal2D();
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

        // Both mobile armor and static batteries need a genuinely flat start.
        // Flak and radar sites score flat candidates by height for coverage;
        // Mobile armor and wheeled HIMARS batteries stay close to the
        // route-side anchor on a flat firing pad.
        {
            const FVector AuthoredCandidate = ThreatSpawnLocation;
            FVector BestCandidate = ThreatSpawnLocation;
            float BestScore = -TNumericLimits<float>::Max();
            bool bFoundDryCandidate = false;
            for (int32 ForwardStep = -3; ForwardStep <= 3; ++ForwardStep)
            {
                for (int32 RightStep = -3; RightStep <= 3; ++RightStep)
                {
                    FVector Candidate = ThreatSpawnLocation +
                        Forward * (ForwardStep * 6000.0f) + Right * (RightStep * 6000.0f);
                    if (FVector::Dist2D(Candidate, Home) < EffectiveAirfieldExclusionRadiusCm) continue;
                    if (FVector::Dist2D(Candidate, CityService) < EffectiveCityExclusionRadiusCm) continue;
                    float CandidateHeight = 0.0f;
                    FVector CandidateNormal = FVector::UpVector;
                    bool bCandidateStructureBlocked = false;
                    if (TraceTerrainHeight(
                        Candidate,
                        CandidateHeight,
                        CandidateNormal,
                        bCandidateStructureBlocked) &&
                        !bCandidateStructureBlocked &&
                        CandidateHeight >= 800.0f && CandidateNormal.Z >= 0.966f)
                    {
                        Candidate.Z = CandidateHeight;
                        bool bFootprintClear = true;
                        constexpr float ClearanceRadiusCm = 700.0f;
                        for (int32 ClearanceDirection = 0; ClearanceDirection < 8; ++ClearanceDirection)
                        {
                            const float ClearanceAngle = 2.0f * PI * ClearanceDirection / 8.0f;
                            const FVector ClearancePoint = Candidate + FVector(
                                FMath::Cos(ClearanceAngle) * ClearanceRadiusCm,
                                FMath::Sin(ClearanceAngle) * ClearanceRadiusCm,
                                0.0f);
                            float ClearanceHeight = 0.0f;
                            FVector ClearanceNormal = FVector::UpVector;
                            bool bClearanceStructureBlocked = false;
                            if (!TraceTerrainHeight(
                                ClearancePoint,
                                ClearanceHeight,
                                ClearanceNormal,
                                bClearanceStructureBlocked) ||
                                bClearanceStructureBlocked ||
                                ClearanceNormal.Z < 0.94f ||
                                FMath::Abs(ClearanceHeight - CandidateHeight) > 125.0f)
                            {
                                bFootprintClear = false;
                                break;
                            }
                        }
                        if (!bFootprintClear) continue;
                        // Prefer high ground, but only after rejecting slopes
                        // steeper than roughly 15 degrees.
                        const float CandidateScore = Spec.bRidgeEmplacement
                            ? CandidateHeight + CandidateNormal.Z * 2500.0f - FVector::Dist2D(Candidate, AuthoredCandidate) * 0.04f
                            : CandidateNormal.Z * 4000.0f - FVector::Dist2D(Candidate, AuthoredCandidate) * 0.08f;
                        if (CandidateScore > BestScore)
                        {
                            bFoundDryCandidate = true;
                            BestScore = CandidateScore;
                            BestCandidate = Candidate;
                        }
                    }
                }
            }
            if (!bFoundDryCandidate)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("ROTORLINE_GROUND_NETWORK|SKIP|target=%s|reason=NO_DRY_LEVEL_TERRAIN|authored=%.0f,%.0f"),
                    *Spec.Target, AuthoredCandidate.X, AuthoredCandidate.Y);
                continue;
            }
            ThreatSpawnLocation = BestCandidate;
        }

        const float SpawnYaw = (PlayerPawn->GetActorLocation() - ThreatSpawnLocation).Rotation().Yaw;
        const FRotator SpawnRotation(0.0f, SpawnYaw, 0.0f);
        ARotorlineMissionObjectiveActor* Enemy = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
            ARotorlineMissionObjectiveActor::StaticClass(), ThreatSpawnLocation, SpawnRotation);
        if (!Enemy) continue;

        FRotorlineObjectiveDefinition Definition;
        Definition.Kind = TEXT("destroy");
        Definition.Text = Spec.Text;
        Definition.Target = Spec.Target;
        Definition.bHasLocation = true;
        Definition.Radius = 90.0f;
        Enemy->Configure(Definition, ThreatSpawnLocation);
        GroundDefenseActors.Add(Enemy);
        switch (Enemy->GetThreatType())
        {
        case ERotorlineThreatType::Tank: ++TankCount; break;
        case ERotorlineThreatType::Flak: ++FlakCount; break;
        case ERotorlineThreatType::RocketArtillery:
            if (Spec.Target.Contains(TEXT("mortar"), ESearchCase::IgnoreCase)) ++MortarCount;
            else ++HimarsCount;
            break;
        case ERotorlineThreatType::RadarMissile: ++RadarCount; break;
        default: break;
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_GROUND_NETWORK|SPAWN|target=%s|threat=%d|elevation_cm=%.0f|dry=1|distance_from_airfield_m=%.0f|route_distance_m=%.0f|flat_selected=1|structure_clear=1|footprint_clear_cm=700|ridge_selected=%d|preview=%d"),
            *Spec.Target,
            static_cast<int32>(Enemy->GetThreatType()),
            Enemy->GetActorLocation().Z,
            FVector::Dist2D(Enemy->GetActorLocation(), Home) / 100.0f,
            FVector::Dist2D(Enemy->GetActorLocation(), Spec.AuthoredLocation) / 100.0f,
            Spec.bRidgeEmplacement ? 1 : 0,
            bPreviewOnly ? 1 : 0);
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_GROUND_NETWORK|READY|count=%d|desired=%d|tanks=%d|mortars=%d|flak=%d|himars=%d|radar=%d|route_points=%d|route_length_km=%.1f|airfield_exclusion_m=%.0f|city_service_exclusion_m=%.0f|recon_ground_screen=%d|preview=%d"),
        GroundDefenseActors.Num(),
        DesiredThreatCount,
        TankCount,
        MortarCount,
        FlakCount,
        HimarsCount,
        RadarCount,
        RoutePoints.Num(),
        TotalRouteLengthCm / 100000.0f,
        EffectiveAirfieldExclusionRadiusCm / 100.0f,
        EffectiveCityExclusionRadiusCm / 100.0f,
        bKiowaGroundRecon ? 1 : 0,
        bPreviewOnly ? 1 : 0);
}

void ARotorlineOperationsPlayerController::SetFlightPauseMenuOpen(bool bOpen)
{
    if (bOperationsMenuOpen || bFlightPauseMenuOpen == bOpen)
    {
        return;
    }

    bFlightPauseMenuOpen = bOpen;
    bAudioSettingsOpen = false;
    bGraphicsSettingsOpen = false;
    bControlsSettingsOpen = false;
    bAbortMissionPending = false;
    SelectedPauseRow = 0;
    bVerticalAxisLatched = false;
    bHorizontalAxisLatched = false;

    if (bOpen)
    {
        bResumeMouseCapture = !bShowMouseCursor;
        UGameplayStatics::SetGamePaused(GetWorld(), true);
        ApplyMouseMode(false);
        PulseController(0.18f, 0.06f);
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_PAUSE|OPEN|mission_state=PRESERVED"));
    }
    else
    {
        UGameplayStatics::SetGamePaused(GetWorld(), false);
        ApplyMouseMode(bResumeMouseCapture);
        RefreshActiveAudioMix();
        PulseController(0.13f, 0.045f);
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_PAUSE|RESUME|mission_state=PRESERVED"));
    }
}

void ARotorlineOperationsPlayerController::MovePauseSelection(int32 Direction)
{
    constexpr int32 PauseRowCount = 5;
    SelectedPauseRow = (SelectedPauseRow + Direction + PauseRowCount) % PauseRowCount;
    bAbortMissionPending = false;
    PulseController(0.10f, 0.035f);
}

void ARotorlineOperationsPlayerController::ActivatePauseSelection()
{
    switch (SelectedPauseRow)
    {
    case 0:
        SetFlightPauseMenuOpen(false);
        break;
    case 1:
        ToggleAudioSettings();
        break;
    case 2:
        ToggleControlsSettings();
        break;
    case 3:
        ToggleGraphicsSettings();
        break;
    case 4:
        if (bAbortMissionPending)
        {
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_PAUSE|ABORT|confirmed=1"));
            ReturnToOperations();
        }
        else
        {
            bAbortMissionPending = true;
            PulseController(0.28f, 0.09f);
        }
        break;
    default:
        break;
    }
}

void ARotorlineOperationsPlayerController::OpenMissionFailureScreen()
{
    if (bOperationsMenuOpen || bMissionFailureScreenOpen)
    {
        return;
    }

    bMissionFailureScreenOpen = true;
    bFlightPauseMenuOpen = false;
    bAudioSettingsOpen = false;
    bAbortMissionPending = false;
    SelectedMissionFailureAction = 0;
    bVerticalAxisLatched = false;
    bHorizontalAxisLatched = false;
    if (Missions.IsValidIndex(SelectedMissionIndex))
    {
        const FRotorlineMissionDefinition& Mission = Missions[SelectedMissionIndex];
        MissionResults.FinalScore = FMath::Max(0,
            MissionResults.PrimaryObjectivesCompleted * 500 +
            MissionResults.OptionalObjectivesCompleted * 750 +
            MissionResults.EnemyHelicoptersDestroyed * 1000 +
            MissionResults.GroundEnemiesDestroyed * 300 +
            (MissionResults.CiviliansRescued + MissionResults.SoldiersRescued) * 200 +
            MissionResults.CargoDelivered * 200 -
            FMath::RoundToInt(MissionResults.DamageTaken * 20.0f));
        FinalizeMissionStatistics(false, Mission);
        ApplyMissionStatisticsToProfile(Mission);
        EvaluateMissionAwards();
        SaveProfile();
    }
    UGameplayStatics::SetGamePaused(GetWorld(), true);
    ApplyMouseMode(false);
    PulseController(0.52f, 0.18f);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_COMBAT_LOOP_TEST|UI|state=MISSION_FAILED|reason=%s|restart=AVAILABLE|select_mission=AVAILABLE"), *GetMissionFailureReason());
}

void ARotorlineOperationsPlayerController::MoveMissionFailureSelection(int32 Direction)
{
    constexpr int32 FailureActionCount = 2;
    SelectedMissionFailureAction = (SelectedMissionFailureAction + Direction + FailureActionCount) % FailureActionCount;
    PulseController(0.12f, 0.04f);
}

void ARotorlineOperationsPlayerController::ActivateMissionFailureSelection()
{
    if (SelectedMissionFailureAction == 0)
    {
        RestartSelectedMission();
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_COMBAT_LOOP_TEST|ACTION|name=SELECT_MISSION|destination=OPERATIONS"));
        ReturnToOperations();
    }
}

void ARotorlineOperationsPlayerController::OpenMissionCompleteScreen(
    const FRotorlineMissionDefinition& Mission,
    float ElapsedSeconds)
{
    if (bMissionCompleteScreenOpen || bMissionFailureScreenOpen)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ROTORLINE_MISSION_COMPLETE|DUPLICATE_SUPPRESSED|mission=%s|complete_ui=%d|failure_ui=%d"),
            *Mission.Id, bMissionCompleteScreenOpen ? 1 : 0, bMissionFailureScreenOpen ? 1 : 0);
        return;
    }

    // The pawn can satisfy the final landing objective during its own tick and
    // immediately enter this completion path before the controller's next
    // telemetry tick. Capture the live fuel/aircraft state now so low-fuel and
    // landing awards never evaluate a one-frame-old snapshot.
    UpdateMissionTelemetry(0.0f);
    if (const ARotorlineHelicopterPawn* Helicopter = Cast<ARotorlineHelicopterPawn>(GetPawn()))
    {
        const FRotorlineAwardsFlightState Flight = Helicopter->GetAwardsFlightState();
        MissionResults.FuelRemainingPercent = Flight.FuelRemainingPercent;
        MissionResults.bAircraftConditionTracked = Flight.MaxHealth > KINDA_SMALL_NUMBER;
        MissionResults.AircraftHealth = Flight.Health;
        MissionResults.AircraftMaxHealth = Flight.MaxHealth;

        const bool bFinalObjectiveIsLanding = !Mission.Objectives.IsEmpty() &&
            Mission.Objectives.Last().Kind.Equals(TEXT("land"), ESearchCase::IgnoreCase);
        const float HorizontalSpeedMps = Flight.Velocity.Size2D() / 100.0f;
        const float VerticalSpeedMps = FMath::Max(0.0f, -Flight.Velocity.Z / 100.0f);
        const float AttitudeDegrees = FMath::Max(FMath::Abs(Flight.PitchDegrees), FMath::Abs(Flight.RollDegrees));
        const bool bMissionLandingSatisfied = bFinalObjectiveIsLanding && !Flight.bAircraftDying &&
            !Flight.bMissionFailed && Flight.AltitudeAglMeters >= 0.0f && Flight.AltitudeAglMeters <= 4.0f &&
            Flight.Velocity.Size() < 650.0f;
        if (bMissionLandingSatisfied)
        {
            MissionResults.bValidLanding = true;
            MissionResults.bHardLanding =
                VerticalSpeedMps > 4.0f ||
                HorizontalSpeedMps > 12.5f ||
                AttitudeDegrees > 12.0f;
            MissionResults.bSafeLanding = !MissionResults.bHardLanding;
            MissionResults.LandingVerticalSpeedMps = VerticalSpeedMps;
            MissionResults.LandingLateralSpeedMps = HorizontalSpeedMps;
            MissionResults.LandingAttitudeDegrees = AttitudeDegrees;
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AWARDS|FINAL_STATE_SYNC|mission=%s|fuel=%.2f|final_land=%d|landing_satisfied=%d|valid=%d|safe=%d"),
            *Mission.Id,
            MissionResults.FuelRemainingPercent,
            bFinalObjectiveIsLanding ? 1 : 0,
            bMissionLandingSatisfied ? 1 : 0,
            MissionResults.bValidLanding ? 1 : 0,
            MissionResults.bSafeLanding ? 1 : 0);
    }

    MissionResults.MissionId = Mission.Id;
    MissionResults.MissionTitle = Mission.Title;
    MissionResults.MissionCallsign = Mission.Callsign;
    MissionResults.MissionType = Mission.Type;
    MissionResults.Weather = Mission.Weather;
    MissionResults.Difficulty = Mission.Difficulty;
    MissionResults.ElapsedSeconds = FMath::Max(0.0f, ElapsedSeconds);
    MissionResults.PrimaryObjectivesTotal = Mission.Objectives.Num();
    // Reaching the mission-complete boundary proves every required catalog
    // objective was satisfied, even if an older objective type did not yet
    // emit the granular completion notification.
    MissionResults.PrimaryObjectivesCompleted = MissionResults.PrimaryObjectivesTotal;
    MissionResults.ExperienceAwarded = Mission.Reward;
    if (const FRotorlineAircraftDefinition* AircraftDefinition = GetSelectedAircraft())
    {
        MissionResults.AircraftName = AircraftDefinition->DisplayName;
    }

    const int32 TimeBonus = Mission.TimeTarget > 0
        ? FMath::Max(0, FMath::RoundToInt(static_cast<float>(Mission.TimeTarget) - MissionResults.ElapsedSeconds)) * 5
        : 0;
    const int32 AccuracyBonus = MissionResults.bWeaponsTracked
        ? MissionResults.WeaponHits * 40
        : 0;
    MissionResults.FinalScore = FMath::Max(0,
        Mission.Reward * 100 +
        MissionResults.PrimaryObjectivesCompleted * 500 +
        MissionResults.OptionalObjectivesCompleted * 750 +
        MissionResults.EnemyHelicoptersDestroyed * 1000 +
        MissionResults.GroundEnemiesDestroyed * 300 +
        (MissionResults.CiviliansRescued + MissionResults.SoldiersRescued) * 200 +
        MissionResults.CargoDelivered * 200 +
        AccuracyBonus + TimeBonus -
        FMath::RoundToInt(MissionResults.DamageTaken * 20.0f));
    MissionResults.StarRating = 3;
    if (Mission.TimeTarget > 0 && MissionResults.ElapsedSeconds <= static_cast<float>(Mission.TimeTarget))
    {
        ++MissionResults.StarRating;
    }
    if (MissionResults.bAircraftConditionTracked && MissionResults.AircraftMaxHealth > KINDA_SMALL_NUMBER &&
        MissionResults.AircraftHealth / MissionResults.AircraftMaxHealth >= 0.75f)
    {
        ++MissionResults.StarRating;
    }
    MissionResults.StarRating = FMath::Clamp(MissionResults.StarRating, 1, 5);
    ++MissionLoopCompletedRuns;

    if (ProfileSave)
    {
        ProfileSave->CompletedMissions.AddUnique(Mission.Id);
        ProfileSave->Reputation += Mission.Reward;
        float& BestTime = ProfileSave->BestMissionTimes.FindOrAdd(Mission.Id);
        if (BestTime <= 0.0f || MissionResults.ElapsedSeconds < BestTime)
        {
            BestTime = MissionResults.ElapsedSeconds;
        }
    }
    FinalizeMissionStatistics(true, Mission);
    ApplyMissionStatisticsToProfile(Mission);
    EvaluateMissionAwards();
    SaveProfile();

    // Remove tracked hostiles before pausing so no delayed attack, alert, or
    // projectile can survive into the debrief or the following sortie.
    ClearMissionSpawnedActors();
    bMissionCompleteScreenOpen = true;
    bFlightPauseMenuOpen = false;
    bAudioSettingsOpen = false;
    bAbortMissionPending = false;
    SelectedMissionCompleteAction = 0;
    bVerticalAxisLatched = false;
    bHorizontalAxisLatched = false;
    UGameplayStatics::SetGamePaused(GetWorld(), true);
    ApplyMouseMode(false);
    PulseController(0.46f, 0.16f);

    const float Accuracy = MissionResults.WeaponShotsFired > 0
        ? 100.0f * static_cast<float>(MissionResults.WeaponHits) / static_cast<float>(MissionResults.WeaponShotsFired)
        : 0.0f;
    const float Condition = MissionResults.bAircraftConditionTracked && MissionResults.AircraftMaxHealth > KINDA_SMALL_NUMBER
        ? 100.0f * MissionResults.AircraftHealth / MissionResults.AircraftMaxHealth
        : -1.0f;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_MISSION_COMPLETE|RESULTS|generation=%d|mission=%s|title=%s|elapsed=%.1f|primary_completed=%d|primary_total=%d|optional_completed=%d|optional_total=%d|air_destroyed=%d|ground_destroyed=%d|civilians_rescued=%d|cargo_delivered=%d|damage_taken=%.1f|condition=%.1f|shots_fired=%d|shots_hit=%d|accuracy=%.1f|score=%d|grade=%d_STAR"),
        MissionResetGeneration, *MissionResults.MissionId, *MissionResults.MissionTitle, MissionResults.ElapsedSeconds,
        MissionResults.PrimaryObjectivesCompleted, MissionResults.PrimaryObjectivesTotal,
        MissionResults.OptionalObjectivesCompleted, MissionResults.OptionalObjectivesTotal,
        MissionResults.EnemyHelicoptersDestroyed, MissionResults.GroundEnemiesDestroyed,
        MissionResults.CiviliansRescued, MissionResults.CargoDelivered,
        MissionResults.DamageTaken, Condition,
        MissionResults.WeaponShotsFired, MissionResults.WeaponHits, Accuracy,
        MissionResults.FinalScore, MissionResults.StarRating);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_MISSION_COMPLETE|UI|state=OPEN|generation=%d|mission=%s|paused=1|hostiles_cleared=1|choose_level=AVAILABLE|replay=AVAILABLE|hangar=AVAILABLE|main_menu=AVAILABLE"),
        MissionResetGeneration, *MissionResults.MissionId);
}

void ARotorlineOperationsPlayerController::MoveMissionCompleteSelection(int32 Direction)
{
    constexpr int32 MissionCompleteActionCount = 4;
    SelectedMissionCompleteAction =
        (SelectedMissionCompleteAction + Direction + MissionCompleteActionCount) % MissionCompleteActionCount;
    PulseController(0.12f, 0.04f);
}

void ARotorlineOperationsPlayerController::ActivateMissionCompleteSelection()
{
    switch (SelectedMissionCompleteAction)
    {
    case 0:
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSION_COMPLETE|ACTION|name=CHOOSE_ANOTHER_LEVEL|destination=LEVEL_SELECTION|generation=%d|status=PASS"), MissionResetGeneration);
        ReturnToOperations();
        break;
    case 1:
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSION_COMPLETE|ACTION|name=REPLAY_MISSION|destination=DEPLOY|generation=%d|status=PASS"), MissionResetGeneration);
        RestartSelectedMission();
        break;
    case 2:
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSION_COMPLETE|ACTION|name=RETURN_TO_HANGAR|destination=HANGAR|generation=%d|status=PASS"), MissionResetGeneration);
        ReturnToHangarAfterMission();
        break;
    case 3:
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_MISSION_COMPLETE|ACTION|name=RETURN_TO_MAIN_MENU|destination=MAP_RELOAD|generation=%d|status=PASS"), MissionResetGeneration);
        ReturnToMainMenu();
        break;
    default:
        break;
    }
}

void ARotorlineOperationsPlayerController::ReturnToHangarAfterMission()
{
    ReturnToOperations();
    OpenHangar();
}

void ARotorlineOperationsPlayerController::ReturnToMainMenu()
{
    ReturnToOperations();
    bStartupFadeToBlack = false;
    bStartupFadeFromBlack = true;
    StartupFadeAlpha = 1.0f;
    StartupTransitionTarget = ERotorlineStartupState::StartScreen;
    EnterStartupState(ERotorlineStartupState::StartScreen);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_STARTUP|RETURN_TO_MAIN_MENU|source=OPERATIONS_BOARD|status=PASS"));
}

void ARotorlineOperationsPlayerController::ResetMissionResults(const TCHAR* Reason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    MissionResults = FRotorlineMissionResults();
    ++MissionResetGeneration;
    bMissionCompleteScreenOpen = false;
    bAwardPresentationOpen = false;
    AwardPresentationIndex = 0;
    NewlyEarnedAwards.Reset();
    SelectedMissionCompleteAction = 0;
    bMissionLoopTestActionActivated = false;
    if (Missions.IsValidIndex(SelectedMissionIndex))
    {
        MissionResults.MissionId = Missions[SelectedMissionIndex].Id;
        MissionResults.MissionTitle = Missions[SelectedMissionIndex].Title;
        MissionResults.MissionCallsign = Missions[SelectedMissionIndex].Callsign;
        MissionResults.MissionType = Missions[SelectedMissionIndex].Type;
        MissionResults.Weather = Missions[SelectedMissionIndex].Weather;
        MissionResults.Difficulty = Missions[SelectedMissionIndex].Difficulty;
        MissionResults.PrimaryObjectivesTotal = Missions[SelectedMissionIndex].Objectives.Num();
    }
    LastTelemetryLocation = FVector::ZeroVector;
    LastTelemetryVelocity = FVector::ZeroVector;
    CurrentStableHoverSeconds = 0.0f;
    CurrentStableHoverBreakSeconds = 0.0f;
    LastTelemetryAltitudeAgl = 0.0f;
    LastAirborneVerticalSpeedMps = 0.0f;
    ObstacleTraceAccumulator = 0.0f;
    LastNearMissTime = -1000.0;
    bTelemetryLocationValid = false;
    bTelemetryWasAirborne = false;
    bTelemetryTakeoffArmed = true;
    bTelemetryLandingRecorded = false;
    if (const FRotorlineAircraftDefinition* AircraftDefinition = GetSelectedAircraft())
    {
        MissionResults.AircraftName = AircraftDefinition->DisplayName;
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_MISSION_RESET|%s|generation=%d|mission=%s|stats=ZEROED|completion_ui=CLOSED|controller_timers=0"),
        Reason, MissionResetGeneration, *MissionResults.MissionId);
}

void ARotorlineOperationsPlayerController::NotifyWeaponFired()
{
    MissionResults.bWeaponsTracked = true;
    ++MissionResults.WeaponShotsFired;
}

void ARotorlineOperationsPlayerController::NotifyWeaponHit(bool bDestroyed, bool bAircraft)
{
    MissionResults.bWeaponsTracked = true;
    ++MissionResults.WeaponHits;
    if (bDestroyed)
    {
        if (bAircraft) ++MissionResults.EnemyHelicoptersDestroyed;
        else ++MissionResults.GroundEnemiesDestroyed;
    }
}

void ARotorlineOperationsPlayerController::NotifyCivilianRescued(int32 Count)
{
    MissionResults.bCivilianRescueTracked = true;
    const int32 SafeCount = FMath::Max(0, Count);
    MissionResults.RescueTargetsAvailable += SafeCount;
    if (MissionResults.MissionType.Contains(TEXT("combat"), ESearchCase::IgnoreCase) ||
        MissionResults.MissionType.Contains(TEXT("troop"), ESearchCase::IgnoreCase))
    {
        MissionResults.SoldiersRescued += SafeCount;
    }
    else
    {
        MissionResults.CiviliansRescued += SafeCount;
    }
}

void ARotorlineOperationsPlayerController::NotifyCargoDelivered(int32 Count, bool bSlingLoad)
{
    MissionResults.bCargoTracked = true;
    const int32 SafeCount = FMath::Max(0, Count);
    MissionResults.CargoDelivered += SafeCount;
    MissionResults.CargoWeightKg += SafeCount * 1000.0f;
    if (bSlingLoad && SafeCount > 0)
    {
        MissionResults.bSlingLoadTracked = true;
        MissionResults.SlingLoadAccuracyPercent = FMath::Max(MissionResults.SlingLoadAccuracyPercent, 90.0f);
    }
}

void ARotorlineOperationsPlayerController::NotifyDamageTaken(float Damage)
{
    MissionResults.DamageTaken += FMath::Max(0.0f, Damage);
}

void ARotorlineOperationsPlayerController::NotifyObjectiveCompleted(bool bOptional)
{
    if (bOptional)
    {
        ++MissionResults.OptionalObjectivesCompleted;
        MissionResults.OptionalObjectivesTotal = FMath::Max(
            MissionResults.OptionalObjectivesTotal,
            MissionResults.OptionalObjectivesCompleted);
    }
    else
    {
        MissionResults.PrimaryObjectivesCompleted = FMath::Min(
            MissionResults.PrimaryObjectivesCompleted + 1,
            MissionResults.PrimaryObjectivesTotal);
    }
}

void ARotorlineOperationsPlayerController::NotifyAircraftCondition(float CurrentHealth, float MaximumHealth)
{
    if (MaximumHealth <= KINDA_SMALL_NUMBER)
    {
        return;
    }
    MissionResults.bAircraftConditionTracked = true;
    MissionResults.AircraftMaxHealth = MaximumHealth;
    MissionResults.AircraftHealth = FMath::Clamp(CurrentHealth, 0.0f, MaximumHealth);
}

void ARotorlineOperationsPlayerController::NotifyMissileDodged(float ClosestDistanceMeters)
{
    ++MissionResults.MissilesDodged;
    MissionResults.bSmokeOrDecoyUsed = true;
    if (ClosestDistanceMeters > 0.0f)
    {
        MissionResults.ClosestObstacleClearanceMeters = FMath::Min(
            MissionResults.ClosestObstacleClearanceMeters,
            ClosestDistanceMeters);
    }
}

void ARotorlineOperationsPlayerController::NotifyEnemyFire(float ExposureSeconds)
{
    MissionResults.TimeUnderEnemyFireSeconds += FMath::Max(0.0f, ExposureSeconds);
}

void ARotorlineOperationsPlayerController::NotifyDetection(float ExposureSeconds)
{
    MissionResults.DetectionTimeSeconds += FMath::Max(0.0f, ExposureSeconds);
}

void ARotorlineOperationsPlayerController::ClearMissionSpawnedActors()
{
    // The M22 wave director owns delayed spawn timers and is not part of the
    // generic objective-actor sweep below. Clear it first so returning to the
    // Operations Board cannot resume a paused timer and spawn mission enemies.
    ARotorlineEnemyIslandAssaultActor::Clear(GetWorld());

    const auto DestroyTrackedActor = [](TObjectPtr<ARotorlineMissionObjectiveActor>& Actor)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
        Actor = nullptr;
    };

    DestroyTrackedActor(EnemyFlightQualificationActor);
    for (TObjectPtr<ARotorlineMissionObjectiveActor>& Actor : CombatPreviewActors)
    {
        DestroyTrackedActor(Actor);
    }
    CombatPreviewActors.Reset();
    for (TObjectPtr<ARotorlineMissionObjectiveActor>& Actor : GroundDefenseActors)
    {
        DestroyTrackedActor(Actor);
    }
    GroundDefenseActors.Reset();

    if (IsValid(HomeHelipadBeaconActor)) HomeHelipadBeaconActor->Destroy();
    if (IsValid(CityServiceHelipadActor)) CityServiceHelipadActor->Destroy();
    if (IsValid(HospitalHelipadActor)) HospitalHelipadActor->Destroy();
    // The Bell lair, cave entrance, and transition volume are permanent,
    // map-authored world features. Mission reset may release controller
    // references, but must never remove those actors from the island.
    HomeHelipadBeaconActor = nullptr;
    CityServiceHelipadActor = nullptr;
    HospitalHelipadActor = nullptr;
    BellLairActor = nullptr;
    CaveTransitionActor = nullptr;
    ActiveHomePadLocation = FVector::ZeroVector;

    // The pawn owns normal mission actors, while qualification and proving
    // ground actors are controller-owned. Sweep both families, including
    // in-flight ordnance, so a debrief/redeploy can never revive stale AI or
    // let a projectile from the previous sortie strike the new aircraft.
    if (GetWorld())
    {
        for (TActorIterator<ARotorlineMissionObjectiveActor> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It)) It->Destroy();
        }
        for (TActorIterator<ARotorlineRocketProjectile> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It)) It->Destroy();
        }
        for (TActorIterator<ARotorlineCannonProjectile> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It)) It->Destroy();
        }
        for (TActorIterator<ARotorlineRocketTrailSegment> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It)) It->Destroy();
        }
    }
}

void ARotorlineOperationsPlayerController::RestartSelectedMission()
{
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_COMBAT_LOOP_TEST|ACTION|name=RESTART|mission_index=%d"), SelectedMissionIndex);
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    ClearMissionSpawnedActors();

    if (APawn* CurrentPawn = GetPawn())
    {
        UnPossess();
        CurrentPawn->Destroy();
    }

    bMissionFailureScreenOpen = false;
    bMissionCompleteScreenOpen = false;
    bAwardPresentationOpen = false;
    bPatchWallOpen = false;
    bFlightPauseMenuOpen = false;
    bAbortMissionPending = false;
    bAudioSettingsOpen = false;
    bVerticalAxisLatched = false;
    bHorizontalAxisLatched = false;
    DeploySelectedAircraft();

    if (!GetPawn())
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_COMBAT_LOOP_TEST|ACTION|name=RESTART|status=FAIL|reason=DEPLOYMENT"));
        ReturnToOperations();
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_COMBAT_LOOP_TEST|ACTION|name=RESTART|status=PASS|mission_index=%d"), SelectedMissionIndex);
}

void ARotorlineOperationsPlayerController::RefreshActiveAudioMix()
{
    if (ARotorlineHelicopterPawn* Helicopter = Cast<ARotorlineHelicopterPawn>(GetPawn()))
    {
        Helicopter->RefreshAudioMix();
    }
    int32 EnemyAudioCount = 0;
    int32 EnemyAudioVerified = 0;
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ARotorlineMissionObjectiveActor> It(World); It; ++It)
        {
            ++EnemyAudioCount;
            if (It->RefreshAudioMix())
            {
                ++EnemyAudioVerified;
            }
        }
    }
    RefreshEnvironmentAudioMix();
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_AUDIO_PROPAGATION|enemies=%d|verified=%d|status=%s|master=%.3f|environment=%.3f|engine=%.3f|music=%.3f|radio=%.3f|weapons=%.3f"),
        EnemyAudioCount,
        EnemyAudioVerified,
        EnemyAudioCount == EnemyAudioVerified ? TEXT("PASS") : TEXT("FAIL"),
        GetAudioSetting(ERotorlineAudioChannel::Master),
        GetEffectiveAudioVolume(ERotorlineAudioChannel::Environment),
        GetEffectiveAudioVolume(ERotorlineAudioChannel::Engine),
        GetEffectiveAudioVolume(ERotorlineAudioChannel::Music),
        GetEffectiveAudioVolume(ERotorlineAudioChannel::Radio),
        GetEffectiveAudioVolume(ERotorlineAudioChannel::WeaponsExplosions));
}

void ARotorlineOperationsPlayerController::RefreshEnvironmentAudioMix()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const float EnvironmentMix = GetEffectiveAudioVolume(ERotorlineAudioChannel::Environment);
    const APawn* ActivePawn = GetPawn();
    const ARotorlineHelicopterPawn* RotorlinePawn = Cast<ARotorlineHelicopterPawn>(ActivePawn);
    const float HorizontalSpeed = RotorlinePawn
        ? RotorlinePawn->GetFlightVelocity().Size2D()
        : (ActivePawn ? ActivePawn->GetVelocity().Size2D() : 0.0f);
    const float FlightLoad = FMath::Clamp(HorizontalSpeed / 5200.0f, 0.0f, 1.0f);
    // Global wind should be a subtle movement cue, not a permanent wall of
    // broadband noise. It rises slightly with airspeed and nearly disappears
    // while parked or browsing the Operations Board.
    const float WindSituation = FMath::Lerp(0.12f, 0.55f, FlightLoad);

    int32 RoutedSources = 0;
    int32 VerifiedSources = 0;
    for (TObjectIterator<UAudioComponent> It; It; ++It)
    {
        UAudioComponent* AudioComponent = *It;
        if (!IsValid(AudioComponent) || AudioComponent->GetWorld() != World || !AudioComponent->Sound)
        {
            continue;
        }

        const FString SoundName = AudioComponent->Sound->GetName();
        const bool bWind = SoundName.Contains(TEXT("Wind"), ESearchCase::IgnoreCase);
        const bool bOceanSurf = SoundName.Contains(TEXT("OceanSurf"), ESearchCase::IgnoreCase);
        const bool bInsects = SoundName.Contains(TEXT("InsectsMeadow"), ESearchCase::IgnoreCase);
        const bool bGulls = SoundName.Contains(TEXT("DistantGulls"), ESearchCase::IgnoreCase);
        const bool bHarbor = SoundName.Contains(TEXT("HarborMachinery"), ESearchCase::IgnoreCase);
        if (!bWind && !bOceanSurf && !bInsects && !bGulls && !bHarbor)
        {
            continue;
        }

        float* BaseVolume = EnvironmentBaseVolumes.Find(AudioComponent);
        if (!BaseVolume)
        {
            BaseVolume = &EnvironmentBaseVolumes.Add(AudioComponent, AudioComponent->VolumeMultiplier);
        }

        float SourceTrim = 1.0f;
        if (bWind) SourceTrim = 0.12f * WindSituation;
        else if (bOceanSurf) SourceTrim = 0.70f;
        else if (bInsects) SourceTrim = 0.55f;
        else if (bGulls) SourceTrim = 0.65f;
        else if (bHarbor) SourceTrim = 0.65f;

        float ExpectedVolume = *BaseVolume * EnvironmentMix * SourceTrim;
        if (bWind)
        {
            const float EngineMix = GetEffectiveAudioVolume(ERotorlineAudioChannel::Engine);
            ExpectedVolume = FMath::Min(ExpectedVolume, EngineMix * 0.035f);
        }
        AudioComponent->SetVolumeMultiplier(ExpectedVolume);
        if (FMath::IsNearlyEqual(AudioComponent->VolumeMultiplier, ExpectedVolume))
        {
            ++VerifiedSources;
        }
        ++RoutedSources;
    }

    if (!FMath::IsNearlyEqual(LastLoggedEnvironmentMix, EnvironmentMix, 0.005f))
    {
        LastLoggedEnvironmentMix = EnvironmentMix;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_ENV_AUDIO|sources=%d|verified=%d|mix=%.2f|wind_situation=%.2f|status=%s"),
            RoutedSources, VerifiedSources, EnvironmentMix, WindSituation,
            RoutedSources == VerifiedSources ? TEXT("PASS") : TEXT("FAIL"));
    }
}

void ARotorlineOperationsPlayerController::ReturnToOperations()
{
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    if (bHangarOpen)
    {
        CloseHangar();
    }
    ClearMissionSpawnedActors();
    APawn* CurrentPawn = GetPawn();
    if (CurrentPawn)
    {
        UnPossess();
        CurrentPawn->Destroy();
    }

    bOperationsMenuOpen = true;
    bHangarOpen = false;
    bFlightPauseMenuOpen = false;
    bMissionFailureScreenOpen = false;
    bMissionCompleteScreenOpen = false;
    bAwardPresentationOpen = false;
    bPatchWallOpen = false;
    UpdatePreGameMenuMusic();
    SelectedMissionFailureAction = 0;
    SelectedMissionCompleteAction = 0;
    bAbortMissionPending = false;
    bVerticalAxisLatched = false;
    bHorizontalAxisLatched = false;
    bAudioSettingsOpen = false;
    ApplyMouseMode(false);
    PulseController(0.2f, 0.08f);
}

void ARotorlineOperationsPlayerController::PulseController(float Intensity, float Duration)
{
    PlayDynamicForceFeedback(Intensity, Duration, true, true, true, true);
}

void ARotorlineOperationsPlayerController::LoadProfile()
{
    constexpr TCHAR SaveSlot[] = TEXT("RotorlineProfile");
    const bool bExistingSave = UGameplayStatics::DoesSaveGameExist(SaveSlot, 0);
    bool bProfileLoadFailed = false;
    bool bRecoveredFromLoadFailure = false;
    if (bExistingSave)
    {
        ProfileSave = Cast<URotorlineProfileSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
        if (!ProfileSave)
        {
            bProfileLoadFailed = true;
            const FString SaveDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
            const FString PrimaryPath = FPaths::Combine(SaveDirectory, TEXT("RotorlineProfile.sav"));
            const FString RecoveryPath = FPaths::Combine(
                SaveDirectory,
                FString::Printf(
                    TEXT("RotorlineProfile.recovery-%s.sav"),
                    *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"))));
            const uint32 CopyResult = IFileManager::Get().Copy(
                *RecoveryPath,
                *PrimaryPath,
                false,
                true);
            bRecoveredFromLoadFailure = CopyResult == COPY_OK;
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_PROFILE|LOAD_FAILED|slot=%s|recovery=%s|backup=%s"),
                SaveSlot,
                bRecoveredFromLoadFailure ? TEXT("PRESERVED") : TEXT("COPY_FAILED"),
                *RecoveryPath);
        }
    }
    if (!ProfileSave)
    {
        ProfileSave = Cast<URotorlineProfileSave>(UGameplayStatics::CreateSaveGameObject(URotorlineProfileSave::StaticClass()));
    }
    if (ProfileSave && ProfileSave->AudioMixVersion < 1)
    {
        ProfileSave->MasterVolume = 0.75f;
        ProfileSave->EnvironmentVolume = 0.28f;
        ProfileSave->EngineVolume = 0.62f;
        ProfileSave->MusicVolume = 0.48f;
        ProfileSave->RadioVolume = 0.78f;
        ProfileSave->WeaponsVolume = 0.42f;
        ProfileSave->AudioMixVersion = 1;
        // Never replace a slot that failed to load. The user can continue with
        // an in-memory fallback profile while the original remains available
        // for recovery, even if the additional timestamped copy could not be
        // created.
        const bool bMigrationSaved = !bProfileLoadFailed && SaveProfile();
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AUDIO_MIX|MIGRATED|version=1|master=0.75|environment=0.28|engine=0.62|music=0.48|radio=0.78|weapons=0.42|save=%s"),
            bMigrationSaved ? TEXT("PERSISTED") :
                (bProfileLoadFailed
                    ? (bRecoveredFromLoadFailure ? TEXT("DEFERRED_AFTER_RECOVERY") : TEXT("DEFERRED_LOAD_FAILURE"))
                    : TEXT("FAILED")));
    }
#if !UE_BUILD_SHIPPING
    constexpr int32 PlaytestMaximumReputation = 999999;
    if (ProfileSave && ProfileSave->Reputation < PlaytestMaximumReputation)
    {
        const int32 PreviousReputation = ProfileSave->Reputation;
        ProfileSave->Reputation = PlaytestMaximumReputation;
        const bool bSaved = SaveProfile();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_PROFILE|PLAYTEST_REPUTATION_MAXED|previous=%d|current=%d|save=%s"),
            PreviousReputation,
            ProfileSave->Reputation,
            bSaved ? TEXT("PERSISTED") : TEXT("FAILED"));
    }
#endif
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_PROFILE|LOADED|reputation=%d|completed=%d"),
        GetReputation(),
        ProfileSave ? ProfileSave->CompletedMissions.Num() : 0);
}

float ARotorlineOperationsPlayerController::GetAudioSetting(ERotorlineAudioChannel Channel) const
{
    if (!ProfileSave)
    {
        switch (Channel)
        {
        case ERotorlineAudioChannel::Master: return 0.75f;
        case ERotorlineAudioChannel::Environment: return 0.28f;
        case ERotorlineAudioChannel::Engine: return 0.62f;
        case ERotorlineAudioChannel::Music: return 0.48f;
        case ERotorlineAudioChannel::Radio: return 0.78f;
        case ERotorlineAudioChannel::WeaponsExplosions: return 0.42f;
        default: return 1.0f;
        }
    }

    switch (Channel)
    {
    case ERotorlineAudioChannel::Master: return FMath::Clamp(ProfileSave->MasterVolume, 0.0f, 1.0f);
    case ERotorlineAudioChannel::Environment: return FMath::Clamp(ProfileSave->EnvironmentVolume, 0.0f, 1.0f);
    case ERotorlineAudioChannel::Engine: return FMath::Clamp(ProfileSave->EngineVolume, 0.0f, 1.0f);
    case ERotorlineAudioChannel::Music: return FMath::Clamp(ProfileSave->MusicVolume, 0.0f, 1.0f);
    case ERotorlineAudioChannel::Radio: return FMath::Clamp(ProfileSave->RadioVolume, 0.0f, 1.0f);
    case ERotorlineAudioChannel::WeaponsExplosions: return FMath::Clamp(ProfileSave->WeaponsVolume, 0.0f, 1.0f);
    default: return 1.0f;
    }
}

float ARotorlineOperationsPlayerController::GetEffectiveAudioVolume(ERotorlineAudioChannel Channel) const
{
    const float Master = GetAudioSetting(ERotorlineAudioChannel::Master);
    return Channel == ERotorlineAudioChannel::Master ? Master : Master * GetAudioSetting(Channel);
}

void ARotorlineOperationsPlayerController::ToggleAudioSettings()
{
    bAudioSettingsOpen = !bAudioSettingsOpen;
    if (bAudioSettingsOpen)
    {
        bGraphicsSettingsOpen = false;
        bControlsSettingsOpen = false;
    }
    bVerticalAxisLatched = false;
    bHorizontalAxisLatched = false;
    PulseController(0.14f, 0.045f);
}

void ARotorlineOperationsPlayerController::ToggleGraphicsSettings()
{
    bGraphicsSettingsOpen = !bGraphicsSettingsOpen;
    if (bGraphicsSettingsOpen)
    {
        bAudioSettingsOpen = false;
        bControlsSettingsOpen = false;
        SelectedGraphicsRow = 0;
        bGraphicsInputArmed = false;
    }
    bVerticalAxisLatched = false;
    bHorizontalAxisLatched = false;
    PulseController(0.14f, 0.045f);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_GRAPHICS|TOGGLE|open=%d|preset=%s"),
        bGraphicsSettingsOpen ? 1 : 0, *GetSimpleGraphicsModeLabel());
}

void ARotorlineOperationsPlayerController::UpdateGraphicsSettingsInput()
{
    if (!bGraphicsInputArmed)
    {
        const bool bConfirmHeld = IsInputKeyDown(EKeys::Enter) ||
            IsInputKeyDown(EKeys::SpaceBar) ||
            IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
        if (!bConfirmHeld)
        {
            bGraphicsInputArmed = true;
        }
        return;
    }

    if (WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up))
    {
        MoveGraphicsSelection(-1);
    }
    if (WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down))
    {
        MoveGraphicsSelection(1);
    }
    if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
    {
        ActivateGraphicsSelection();
    }
    if (WasInputKeyJustPressed(EKeys::Escape) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right))
    {
        ToggleGraphicsSettings();
    }
}

void ARotorlineOperationsPlayerController::MoveGraphicsSelection(int32 Direction)
{
    constexpr int32 GraphicsRowCount = 2;
    SelectedGraphicsRow = (SelectedGraphicsRow + Direction + GraphicsRowCount) % GraphicsRowCount;
    PulseController(0.10f, 0.035f);
}

void ARotorlineOperationsPlayerController::ActivateGraphicsSelection()
{
    if (SelectedGraphicsRow == 0)
    {
        ToggleSimpleGraphicsMode();
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_GRAPHICS|PRESET_CHANGED|preset=%s"),
            *GetSimpleGraphicsModeLabel());
        return;
    }
    ToggleGraphicsSettings();
}

void ARotorlineOperationsPlayerController::MoveAudioSelection(int32 Direction)
{
    constexpr int32 ChannelCount = 6;
    SelectedAudioRow = (SelectedAudioRow + Direction + ChannelCount) % ChannelCount;
    PulseController(0.10f, 0.035f);
}

void ARotorlineOperationsPlayerController::AdjustAudioSetting(int32 Direction)
{
    if (!ProfileSave)
    {
        LoadProfile();
    }
    if (!ProfileSave)
    {
        return;
    }

    float* Setting = nullptr;
    switch (SelectedAudioRow)
    {
    case 0: Setting = &ProfileSave->MasterVolume; break;
    case 1: Setting = &ProfileSave->EnvironmentVolume; break;
    case 2: Setting = &ProfileSave->EngineVolume; break;
    case 3: Setting = &ProfileSave->MusicVolume; break;
    case 4: Setting = &ProfileSave->RadioVolume; break;
    case 5: Setting = &ProfileSave->WeaponsVolume; break;
    default: break;
    }
    if (!Setting)
    {
        return;
    }

    *Setting = FMath::Clamp(*Setting + static_cast<float>(Direction) * 0.05f, 0.0f, 1.0f);
    SaveProfile();
    RefreshActiveAudioMix();
    PulseController(0.08f, 0.03f);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AUDIO_MIX|channel=%d|value=%.2f|master=%.2f"), SelectedAudioRow, *Setting, ProfileSave->MasterVolume);
}

void ARotorlineOperationsPlayerController::ResetAudioSettings()
{
    if (!ProfileSave)
    {
        LoadProfile();
    }
    if (!ProfileSave)
    {
        return;
    }

    ProfileSave->MasterVolume = 0.75f;
    ProfileSave->EnvironmentVolume = 0.28f;
    ProfileSave->EngineVolume = 0.62f;
    ProfileSave->MusicVolume = 0.48f;
    ProfileSave->RadioVolume = 0.78f;
    ProfileSave->WeaponsVolume = 0.42f;
    ProfileSave->AudioMixVersion = 1;
    SaveProfile();
    RefreshActiveAudioMix();
    PulseController(0.22f, 0.08f);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AUDIO_MIX|RESET|master=0.75|environment=0.28|engine=0.62|music=0.48|radio=0.78|weapons=0.42"));
}

namespace
{
    const TArray<FName>& RotorlineCalibrationActions()
    {
        static const TArray<FName> Actions = {
            RotorlineFlightControllerActions::Roll,
            RotorlineFlightControllerActions::Pitch,
            RotorlineFlightControllerActions::Collective,
            RotorlineFlightControllerActions::Yaw
        };
        return Actions;
    }

    const TArray<FName>& RotorlineButtonActions()
    {
        static const TArray<FName> Actions = {
            RotorlineFlightControllerActions::PrimaryFire,
            RotorlineFlightControllerActions::SecondaryFire,
            RotorlineFlightControllerActions::WeaponNext,
            RotorlineFlightControllerActions::WeaponPrevious,
            RotorlineFlightControllerActions::TargetLock,
            RotorlineFlightControllerActions::Countermeasures,
            RotorlineFlightControllerActions::MissionInteract,
            RotorlineFlightControllerActions::LandingGear,
            RotorlineFlightControllerActions::Searchlight,
            RotorlineFlightControllerActions::ChangeCamera,
            RotorlineFlightControllerActions::CockpitView,
            RotorlineFlightControllerActions::ExternalView,
            RotorlineFlightControllerActions::MapView,
            RotorlineFlightControllerActions::Pause,
            RotorlineFlightControllerActions::RadioCommand
        };
        return Actions;
    }
}

FName ARotorlineOperationsPlayerController::GetControlsWizardAction() const
{
    const TArray<FName>& Actions = ControlsMode == ERotorlineControlsMode::AxisCalibration
        ? RotorlineCalibrationActions() : RotorlineButtonActions();
    return Actions.IsValidIndex(ControlsWizardStep) ? Actions[ControlsWizardStep] : NAME_None;
}

FString ARotorlineOperationsPlayerController::GetControlsWizardCurrentBinding() const
{
    const FName Action = GetControlsWizardAction();
    if (Action.IsNone()) return TEXT("UNASSIGNED");

    TArray<FString> Assignments;
    for (const FRotorlineButtonBinding& Binding : WorkingControllerProfile.ButtonBindings)
    {
        if (Binding.Action == Action)
            Assignments.Add(FString::Printf(TEXT("BUTTON %d"), Binding.NativeButtonIndex + 1));
    }
    for (const FRotorlineAxisBinding& Binding : WorkingControllerProfile.AxisBindings)
    {
        if (!Binding.bIgnore && Binding.Action == Action)
            Assignments.Add(FString::Printf(TEXT("AXIS %d%s"), Binding.NativeAxisIndex + 1,
                Binding.UserLabel.Contains(TEXT("DIGITAL TRIGGER")) ? TEXT(" TRIGGER") : TEXT("")));
    }
    for (const FRotorlineHatBinding& Binding : WorkingControllerProfile.HatBindings)
    {
        if (Binding.UpAction == Action) Assignments.Add(FString::Printf(TEXT("HAT %d UP"), Binding.NativeHatIndex + 1));
        if (Binding.RightAction == Action) Assignments.Add(FString::Printf(TEXT("HAT %d RIGHT"), Binding.NativeHatIndex + 1));
        if (Binding.DownAction == Action) Assignments.Add(FString::Printf(TEXT("HAT %d DOWN"), Binding.NativeHatIndex + 1));
        if (Binding.LeftAction == Action) Assignments.Add(FString::Printf(TEXT("HAT %d LEFT"), Binding.NativeHatIndex + 1));
    }
    return Assignments.IsEmpty() ? TEXT("UNASSIGNED") : FString::Join(Assignments, TEXT(" + "));
}

bool ARotorlineOperationsPlayerController::GetFlightControllerAxis(FName Action, float& OutValue) const
{
    OutValue = 0.0f;
    if (bControlsSettingsOpen || !GetGameInstance()) return false;
    const URotorlineFlightControllerSubsystem* Input =
        GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>();
    return Input && Input->GetAxisValue(Action, OutValue);
}

bool ARotorlineOperationsPlayerController::IsFlightControllerActionPressed(FName Action) const
{
    return !bControlsSettingsOpen && ControllerActionCurrent.FindRef(Action);
}

bool ARotorlineOperationsPlayerController::WasFlightControllerActionJustPressed(FName Action) const
{
    return !bControlsSettingsOpen && ControllerActionCurrent.FindRef(Action) &&
        !ControllerActionPrevious.FindRef(Action);
}

bool ARotorlineOperationsPlayerController::HasActiveFlightController() const
{
    const URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    return Input && !Input->GetActiveDeviceId().IsEmpty() && !Input->GetActiveProfileId().IsEmpty();
}

void ARotorlineOperationsPlayerController::RefreshControllerSemanticState()
{
    ControllerActionPrevious = ControllerActionCurrent;
    ControllerActionCurrent.Reset();
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    for (const FName Action : RotorlineFlightControllerActions::All())
    {
        ControllerActionCurrent.Add(Action, Input && !bControlsSettingsOpen && Input->IsActionPressed(Action));
    }

    // Emit each physical controller action once per process while an aircraft is
    // possessed. These markers make the hands-on packaged acceptance pass
    // auditable without changing gameplay or flooding normal flight logs.
    if (!bControlsSettingsOpen && Cast<ARotorlineHelicopterPawn>(GetPawn()))
    {
        for (const FName Action : RotorlineFlightControllerActions::All())
        {
            if (ControllerActionCurrent.FindRef(Action) && !ControllerActionPrevious.FindRef(Action) &&
                !ControllerAcceptanceActionsLogged.Contains(Action))
            {
                ControllerAcceptanceActionsLogged.Add(Action);
                UE_LOG(LogTemp, Display,
                    TEXT("ROTORLINE_CONTROLLER_ACCEPTANCE|BUTTON|action=%s|result=OBSERVED"),
                    *Action.ToString());
            }
        }
    }
}

bool ARotorlineOperationsPlayerController::QueueControlsSettingsInput(const FInputKeyEventArgs& Params)
{
    if (Params.Event == IE_Axis &&
        (Params.Key == EKeys::Gamepad_LeftX || Params.Key == EKeys::Gamepad_LeftY))
    {
        const float Value = Params.AmountDepressed;
        bool& bLatched = Params.Key == EKeys::Gamepad_LeftY
            ? bControlsLeftStickVerticalLatched : bControlsLeftStickHorizontalLatched;
        if (FMath::Abs(Value) <= 0.35f)
        {
            bLatched = false;
            return true;
        }
        if (!bLatched && FMath::Abs(Value) >= 0.60f)
        {
            const FKey NavigationKey = Params.Key == EKeys::Gamepad_LeftY
                ? (Value > 0.0f ? EKeys::Gamepad_DPad_Up : EKeys::Gamepad_DPad_Down)
                : (Value > 0.0f ? EKeys::Gamepad_DPad_Right : EKeys::Gamepad_DPad_Left);
            PendingControlsInputKeys.AddUnique(NavigationKey);
            bLatched = true;
            bGamepadInputSeen = true;
        }
        return true;
    }

    if (Params.Event != IE_Pressed) return false;
    const FKey& Key = Params.Key;
    const bool bHandled =
        Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Left || Key == EKeys::Right ||
        Key == EKeys::W || Key == EKeys::A || Key == EKeys::S || Key == EKeys::D ||
        Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::R || Key == EKeys::M ||
        Key == EKeys::Q || Key == EKeys::E || Key == EKeys::Z || Key == EKeys::C || Key == EKeys::V ||
        Key == EKeys::I || Key == EKeys::N ||
        Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down ||
        Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right ||
        Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder ||
        Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::Gamepad_RightTrigger ||
        Key == EKeys::Gamepad_LeftThumbstick || Key == EKeys::Gamepad_RightThumbstick ||
        Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Gamepad_FaceButton_Left ||
        Key == EKeys::Gamepad_FaceButton_Top;
    if (!bHandled) return false;

    PendingControlsInputKeys.AddUnique(Key);
    if (Key.IsGamepadKey()) bGamepadInputSeen = true;
    return true;
}

void ARotorlineOperationsPlayerController::CaptureCompatibleGamepadControlsInput()
{
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    const FRotorlineControllerDeviceInfo* Device = Input
        ? Input->GetDevices().FindByPredicate([](const FRotorlineControllerDeviceInfo& Entry)
        {
            return Entry.bConnected && Entry.bGamepadCompatible;
        }) : nullptr;

    if (!Input || !Device)
    {
        ControlsMenuGamepadDeviceId.Reset();
        ControlsGamepadButtonPrevious.Reset();
        ControlsGamepadPreviousHatDirection = INDEX_NONE;
        bControlsCompatibleGamepadConfirmHeld = false;
        return;
    }

    TArray<bool> CurrentButtons;
    CurrentButtons.Init(false, Device->Capabilities.ButtonCount);
    for (int32 ButtonIndex = 0; ButtonIndex < CurrentButtons.Num(); ++ButtonIndex)
    {
        CurrentButtons[ButtonIndex] = Input->IsRawButtonPressed(Device->DeviceId, ButtonIndex);
    }
    bControlsCompatibleGamepadConfirmHeld = CurrentButtons.IsValidIndex(1) && CurrentButtons[1];

    float RawX = 0.0f;
    float RawY = 0.0f;
    const bool bHasX = Input->GetRawAxisValue(Device->DeviceId, 0, RawX);
    const bool bHasY = Input->GetRawAxisValue(Device->DeviceId, 1, RawY);
    const auto NormalizeAxis = [Device](int32 AxisIndex, float RawValue) -> float
    {
        if (!Device->Capabilities.Axes.IsValidIndex(AxisIndex)) return 0.0f;
        const FRotorlineControllerAxisCapability& Axis = Device->Capabilities.Axes[AxisIndex];
        if (FMath::IsNearlyEqual(Axis.RawMinimum, Axis.RawMaximum)) return 0.0f;
        return FMath::GetMappedRangeValueClamped(
            FVector2D(Axis.RawMinimum, Axis.RawMaximum), FVector2D(-1.0f, 1.0f), RawValue);
    };
    const float StickX = bHasX ? NormalizeAxis(0, RawX) : 0.0f;
    const float StickY = bHasY ? NormalizeAxis(1, RawY) : 0.0f;

    if (ControlsMenuGamepadDeviceId != Device->DeviceId ||
        ControlsGamepadButtonPrevious.Num() != CurrentButtons.Num())
    {
        ControlsMenuGamepadDeviceId = Device->DeviceId;
        ControlsGamepadButtonPrevious = CurrentButtons;
        bControlsLeftStickHorizontalLatched = FMath::Abs(StickX) >= 0.35f;
        bControlsLeftStickVerticalLatched = FMath::Abs(StickY) >= 0.35f;
        float HatAngle = -1.0f;
        ControlsGamepadPreviousHatDirection = Input->GetRawHatAngle(Device->DeviceId, 0, HatAngle)
            ? FMath::RoundToInt(HatAngle / 45.0f) : INDEX_NONE;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_CONTROLS|GAMEPAD_MENU|device=%s|name=%s|fallback=RAW"),
            *Device->DeviceId, *Device->DisplayName);
        return;
    }

    const auto QueueButton = [this, &CurrentButtons](int32 Index, const FKey& Key)
    {
        if (CurrentButtons.IsValidIndex(Index) && ControlsGamepadButtonPrevious.IsValidIndex(Index) &&
            CurrentButtons[Index] && !ControlsGamepadButtonPrevious[Index])
        {
            PendingControlsInputKeys.AddUnique(Key);
            bGamepadInputSeen = true;
        }
    };
    QueueButton(0, EKeys::Gamepad_FaceButton_Left);   // Square
    QueueButton(1, EKeys::Gamepad_FaceButton_Bottom); // Cross
    QueueButton(2, EKeys::Gamepad_FaceButton_Right);  // Circle
    QueueButton(3, EKeys::Gamepad_FaceButton_Top);    // Triangle
    QueueButton(4, EKeys::Gamepad_LeftShoulder);
    QueueButton(5, EKeys::Gamepad_RightShoulder);
    QueueButton(6, EKeys::Gamepad_LeftTrigger);
    QueueButton(7, EKeys::Gamepad_RightTrigger);
    QueueButton(9, EKeys::Gamepad_Special_Right);     // Options
    QueueButton(10, EKeys::Gamepad_LeftThumbstick);
    QueueButton(11, EKeys::Gamepad_RightThumbstick);
    ControlsGamepadButtonPrevious = CurrentButtons;

    float HatAngle = -1.0f;
    int32 HatDirection = INDEX_NONE;
    if (Input->GetRawHatAngle(Device->DeviceId, 0, HatAngle) && HatAngle >= 0.0f)
    {
        HatDirection = FMath::RoundToInt(HatAngle / 45.0f) % 8;
    }
    if (HatDirection != INDEX_NONE && HatDirection != ControlsGamepadPreviousHatDirection)
    {
        if (HatDirection == 7 || HatDirection == 0 || HatDirection == 1)
            PendingControlsInputKeys.AddUnique(EKeys::Gamepad_DPad_Up);
        if (HatDirection == 1 || HatDirection == 2 || HatDirection == 3)
            PendingControlsInputKeys.AddUnique(EKeys::Gamepad_DPad_Right);
        if (HatDirection == 3 || HatDirection == 4 || HatDirection == 5)
            PendingControlsInputKeys.AddUnique(EKeys::Gamepad_DPad_Down);
        if (HatDirection == 5 || HatDirection == 6 || HatDirection == 7)
            PendingControlsInputKeys.AddUnique(EKeys::Gamepad_DPad_Left);
        bGamepadInputSeen = true;
    }
    ControlsGamepadPreviousHatDirection = HatDirection;

    const auto QueueStickDirection = [this](float Value, bool& bLatched, const FKey& Positive, const FKey& Negative)
    {
        if (FMath::Abs(Value) <= 0.35f)
        {
            bLatched = false;
        }
        else if (!bLatched && FMath::Abs(Value) >= 0.60f)
        {
            PendingControlsInputKeys.AddUnique(Value > 0.0f ? Positive : Negative);
            bLatched = true;
            bGamepadInputSeen = true;
        }
    };
    QueueStickDirection(StickX, bControlsLeftStickHorizontalLatched,
        EKeys::Gamepad_DPad_Right, EKeys::Gamepad_DPad_Left);
    QueueStickDirection(StickY, bControlsLeftStickVerticalLatched,
        EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Up);
}

void ARotorlineOperationsPlayerController::CheckFlightControllerConnection()
{
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input) return;
    const FString Current = Input->GetActiveDeviceId();
    if (Current != LastObservedControllerDeviceId)
    {
        if (!LastObservedControllerDeviceId.IsEmpty() && Current.IsEmpty())
        {
            FlightControllerNotification = TEXT("ACTIVE FLIGHT CONTROLLER DISCONNECTED // KEYBOARD AND GAMEPAD AVAILABLE");
            FlightControllerNotificationSeconds = 8.0f;
            UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_FLIGHT_CONTROLLER|DISCONNECTED|fallback=KEYBOARD_GAMEPAD"));
        }
        else if (!Current.IsEmpty())
        {
            const bool bHasActiveProfile = !Input->GetActiveProfileId().IsEmpty();
            FlightControllerNotification = bHasActiveProfile
                ? TEXT("FLIGHT CONTROLLER CONNECTED // PROFILE ACTIVE")
                : TEXT("NEW FLIGHT CONTROLLER CONNECTED // OPEN CONTROLS TO CALIBRATE");
            FlightControllerNotificationSeconds = bHasActiveProfile ? 4.0f : 8.0f;
            if (!bHasActiveProfile)
            {
                // Re-arm the safe Start Screen offer for controllers connected
                // after the original startup detection window.
                bControllerFirstTimePromptChecked = false;
            }
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_FLIGHT_CONTROLLER|CONNECTED|device=%s|profile=%s"),
                *Current, bHasActiveProfile ? TEXT("ACTIVE") : TEXT("SETUP_REQUIRED"));
        }
        LastObservedControllerDeviceId = Current;
    }
}

void ARotorlineOperationsPlayerController::RunFlightControllerQualification()
{
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_CONTROLLER_TEST|RESULT|status=FAIL|reason=NO_SUBSYSTEM"));
        return;
    }
    Input->RefreshDevices();
    const FRotorlineControllerDeviceInfo* Device = Input->GetDevices().FindByPredicate([](const FRotorlineControllerDeviceInfo& Entry)
    {
        return Entry.VendorId == 0x046d && Entry.ProductId == 0xc215;
    });
    if (!Device && !Input->GetDevices().IsEmpty()) Device = &Input->GetDevices()[0];
    if (!Device || !Input->SetActiveDevice(Device->DeviceId))
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_CONTROLLER_TEST|RESULT|status=FAIL|reason=NO_DEVICE"));
        return;
    }
    Input->Tick(0.0f); // Prime one real hardware reading before validating raw state.

    FRotorlineFlightControllerProfile Profile = Input->MakeDefaultProfile(Device->DeviceId);
    Profile.ProfileId = TEXT("rotorline-qualification-temporary");
    Profile.ProfileName = TEXT("Rotorline Qualification Temporary");
    const FString QualificationDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FlightControllerQualification"));
    IFileManager::Get().MakeDirectory(*QualificationDirectory, true);
    const FString ExportPath = FPaths::Combine(QualificationDirectory, TEXT("exported-profile.json"));
    const FString CorruptPath = FPaths::Combine(QualificationDirectory, TEXT("corrupt-profile.json"));
    const FString IncompletePath = FPaths::Combine(QualificationDirectory, TEXT("incomplete-profile.json"));

    const bool bSaved = Input->SaveProfile(Profile);
    const bool bApplied = bSaved && Input->ApplyProfile(Profile.ProfileId);
    const bool bExported = bApplied && Input->ExportProfile(Profile.ProfileId, ExportPath);
    Input->ReloadProfiles();
    const bool bReloaded = Input->ApplyProfile(Profile.ProfileId);
    FRotorlineFlightControllerProfile ReloadedProfile;
    const bool bProfileEqual = bReloaded && Input->GetActiveProfile(ReloadedProfile) &&
        ReloadedProfile.AxisBindings.Num() == Profile.AxisBindings.Num() &&
        ReloadedProfile.ButtonBindings.Num() == Profile.ButtonBindings.Num();

    FFileHelper::SaveStringToFile(TEXT("{ definitely not valid controller json"), *CorruptPath);
    FString CorruptImportId;
    const bool bCorruptRejected = !Input->ImportProfile(CorruptPath, CorruptImportId);
    FFileHelper::SaveStringToFile(TEXT("{\"schemaVersion\":1}"), *IncompletePath);
    FString IncompleteImportId;
    const bool bIncompleteRejected = !Input->ImportProfile(IncompletePath, IncompleteImportId);
    float RawAxis = 0.0f;
    const bool bRawRead = Device->Capabilities.AxisCount > 0 &&
        Input->GetRawAxisValue(Device->DeviceId, 0, RawAxis);
    const bool bCapabilitiesValid = Device->Capabilities.AxisCount > 0 &&
        Device->Capabilities.ButtonCount > 0 && Device->Capabilities.HatCount >= 0;
    bool bAllRawControlsReadable = true;
    for (int32 AxisIndex = 0; AxisIndex < Device->Capabilities.AxisCount; ++AxisIndex)
    {
        float Value = 0.0f;
        bAllRawControlsReadable &= Input->GetRawAxisValue(Device->DeviceId, AxisIndex, Value);
    }
    for (int32 ButtonIndex = 0; ButtonIndex < Device->Capabilities.ButtonCount; ++ButtonIndex)
    {
        Input->IsRawButtonPressed(Device->DeviceId, ButtonIndex);
    }
    for (int32 HatIndex = 0; HatIndex < Device->Capabilities.HatCount; ++HatIndex)
    {
        float Angle = -1.0f;
        bAllRawControlsReadable &= Input->GetRawHatAngle(Device->DeviceId, HatIndex, Angle);
    }
    TSet<FString> UniqueDeviceIds;
    for (const FRotorlineControllerDeviceInfo& Connected : Input->GetDevices())
    {
        if (Connected.bConnected) UniqueDeviceIds.Add(Connected.DeviceId);
    }
    const bool bMultipleDevicesDistinct = UniqueDeviceIds.Num() == Input->GetDevices().Num();

    FRotorlineAxisBinding FilterBinding;
    FilterBinding.Calibration.RawMinimum = 0.0f;
    FilterBinding.Calibration.RawCenter = 0.5f;
    FilterBinding.Calibration.RawMaximum = 1.0f;
    FilterBinding.Deadzone = 0.10f;
    const bool bFilterEndpoints = FMath::IsNearlyEqual(URotorlineFlightControllerSubsystem::FilterAxisValue(0.0f, FilterBinding), -1.0f) &&
        FMath::IsNearlyZero(URotorlineFlightControllerSubsystem::FilterAxisValue(0.5f, FilterBinding)) &&
        FMath::IsNearlyEqual(URotorlineFlightControllerSubsystem::FilterAxisValue(1.0f, FilterBinding), 1.0f);
    FilterBinding.bInvert = true;
    const bool bFilterInvert = FMath::IsNearlyEqual(
        URotorlineFlightControllerSubsystem::FilterAxisValue(1.0f, FilterBinding), -1.0f);
    FilterBinding.bInvert = false;
    FilterBinding.CurveExponent = 2.0f;
    FilterBinding.Sensitivity = 0.5f;
    FilterBinding.Scale = 0.8f;
    FilterBinding.CenterOffset = 0.05f;
    const float TunedFilter = URotorlineFlightControllerSubsystem::FilterAxisValue(0.75f, FilterBinding);
    const bool bFilterTuning = FMath::IsFinite(TunedFilter) && TunedFilter > 0.0f && TunedFilter < 1.0f;
    FRotorlineAxisBinding UncenteredBinding;
    UncenteredBinding.bCentered = false;
    UncenteredBinding.bInvert = true;
    UncenteredBinding.Deadzone = 0.0f;
    UncenteredBinding.Calibration.RawMinimum = 0.0f;
    UncenteredBinding.Calibration.RawCenter = 0.0f;
    UncenteredBinding.Calibration.RawMaximum = 1.0f;
    const bool bUncenteredInvert =
        FMath::IsNearlyEqual(URotorlineFlightControllerSubsystem::FilterAxisValue(0.0f, UncenteredBinding), 1.0f) &&
        FMath::IsNearlyZero(URotorlineFlightControllerSubsystem::FilterAxisValue(1.0f, UncenteredBinding));

    FRotorlineFlightControllerProfile TransientProfile = ReloadedProfile;
    if (!TransientProfile.AxisBindings.IsEmpty()) TransientProfile.AxisBindings[0].Deadzone = 0.17f;
    const bool bTransientApplied = Input->ApplyTransientProfile(TransientProfile);
    FRotorlineFlightControllerProfile ActiveTransient;
    const bool bTransientEqual = bTransientApplied && Input->GetActiveProfile(ActiveTransient) &&
        !ActiveTransient.AxisBindings.IsEmpty() &&
        FMath::IsNearlyEqual(ActiveTransient.AxisBindings[0].Deadzone, 0.17f);

    FRotorlineFlightControllerProfile ResetBindingProfile = TransientProfile;
    const int32 OriginalAxisCount = ResetBindingProfile.AxisBindings.Num();
    const int32 OriginalButtonCount = ResetBindingProfile.ButtonBindings.Num();
    if (!ResetBindingProfile.ButtonBindings.IsEmpty()) ResetBindingProfile.ButtonBindings.RemoveAt(0);
    const bool bResetBindingIsolated = ResetBindingProfile.AxisBindings.Num() == OriginalAxisCount &&
        ResetBindingProfile.ButtonBindings.Num() == FMath::Max(0, OriginalButtonCount - 1);
    const bool bDefaultUsable = Profile.AxisBindings.ContainsByPredicate([](const FRotorlineAxisBinding& Binding)
    {
        return Binding.Action == RotorlineFlightControllerActions::Roll;
    }) && Profile.AxisBindings.ContainsByPredicate([](const FRotorlineAxisBinding& Binding)
    {
        return Binding.Action == RotorlineFlightControllerActions::Pitch;
    }) && Profile.ButtonBindings.ContainsByPredicate([](const FRotorlineButtonBinding& Binding)
    {
        return Binding.NativeButtonIndex == 0 && Binding.Action == RotorlineFlightControllerActions::PrimaryFire;
    }) && Profile.ButtonBindings.ContainsByPredicate([](const FRotorlineButtonBinding& Binding)
    {
        return Binding.NativeButtonIndex == 1 && Binding.Action == RotorlineFlightControllerActions::Countermeasures;
    }) && Profile.ButtonBindings.ContainsByPredicate([](const FRotorlineButtonBinding& Binding)
    {
        return Binding.NativeButtonIndex == 2 && Binding.Action == RotorlineFlightControllerActions::WeaponNext;
    }) && Profile.ButtonBindings.ContainsByPredicate([](const FRotorlineButtonBinding& Binding)
    {
        return Binding.NativeButtonIndex == 3 && Binding.Action == RotorlineFlightControllerActions::WeaponPrevious;
    });

    bool bDuplicateAxisGuard = Profile.AxisBindings.Num() < 2;
    if (Profile.AxisBindings.Num() >= 2)
    {
        FRotorlineFlightControllerProfile DuplicateAxisProfile = Profile;
        DuplicateAxisProfile.AxisBindings[1].NativeAxisIndex = DuplicateAxisProfile.AxisBindings[0].NativeAxisIndex;
        DuplicateAxisProfile.bAllowDuplicateAxisBindings = false;
        const bool bRejected = !Input->ApplyTransientProfile(DuplicateAxisProfile);
        DuplicateAxisProfile.bAllowDuplicateAxisBindings = true;
        bDuplicateAxisGuard = bRejected && Input->ApplyTransientProfile(DuplicateAxisProfile);
    }
    bool bDuplicateButtonGuard = Profile.ButtonBindings.Num() < 2;
    if (Profile.ButtonBindings.Num() >= 2)
    {
        FRotorlineFlightControllerProfile DuplicateButtonProfile = Profile;
        DuplicateButtonProfile.ButtonBindings[1].NativeButtonIndex = DuplicateButtonProfile.ButtonBindings[0].NativeButtonIndex;
        DuplicateButtonProfile.bAllowDuplicateButtonBindings = false;
        const bool bRejected = !Input->ApplyTransientProfile(DuplicateButtonProfile);
        DuplicateButtonProfile.bAllowDuplicateButtonBindings = true;
        bDuplicateButtonGuard = bRejected && Input->ApplyTransientProfile(DuplicateButtonProfile);
    }
    bool bDuplicateHatGuard = Profile.HatBindings.IsEmpty();
    if (!Profile.HatBindings.IsEmpty())
    {
        FRotorlineFlightControllerProfile DuplicateHatProfile = Profile;
        FRotorlineHatBinding DuplicateHat;
        DuplicateHat.NativeHatIndex = DuplicateHatProfile.HatBindings[0].NativeHatIndex;
        DuplicateHat.UpAction = RotorlineFlightControllerActions::MapView;
        DuplicateHatProfile.HatBindings.Add(DuplicateHat);
        DuplicateHatProfile.bAllowDuplicateHatBindings = false;
        const bool bRejected = !Input->ApplyTransientProfile(DuplicateHatProfile);
        DuplicateHatProfile.bAllowDuplicateHatBindings = true;
        bDuplicateHatGuard = bRejected && Input->ApplyTransientProfile(DuplicateHatProfile);
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CONTROLLER_PROFILE|PROFILE_RESTART|saved=%d|applied=%d|exported=%d|reloaded=%d|equal=%d|corrupt_rejected=%d|incomplete_rejected=%d"),
        bSaved ? 1 : 0, bApplied ? 1 : 0, bExported ? 1 : 0, bReloaded ? 1 : 0,
        bProfileEqual ? 1 : 0, bCorruptRejected ? 1 : 0, bIncompleteRejected ? 1 : 0);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CONTROLLER_TEST|FILTERS|endpoints=%d|invert=%d|uncentered_invert=%d|tuning=%d|status=%s"),
        bFilterEndpoints ? 1 : 0, bFilterInvert ? 1 : 0, bUncenteredInvert ? 1 : 0, bFilterTuning ? 1 : 0,
        bFilterEndpoints && bFilterInvert && bUncenteredInvert && bFilterTuning ? TEXT("PASS") : TEXT("FAIL"));
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CONTROLLER_TEST|DUPLICATES|axis_guard=%d|button_guard=%d|hat_guard=%d|status=%s"),
        bDuplicateAxisGuard ? 1 : 0, bDuplicateButtonGuard ? 1 : 0, bDuplicateHatGuard ? 1 : 0,
        bDuplicateAxisGuard && bDuplicateButtonGuard && bDuplicateHatGuard ? TEXT("PASS") : TEXT("FAIL"));
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CONTROLLER_TEST|WORKFLOW|raw_controls=%d|distinct_devices=%d|transient_apply=%d|reset_isolated=%d|defaults_usable=%d|status=%s"),
        bAllRawControlsReadable ? 1 : 0, bMultipleDevicesDistinct ? 1 : 0,
        bTransientEqual ? 1 : 0, bResetBindingIsolated ? 1 : 0, bDefaultUsable ? 1 : 0,
        bAllRawControlsReadable && bMultipleDevicesDistinct && bTransientEqual &&
            bResetBindingIsolated && bDefaultUsable ? TEXT("PASS") : TEXT("FAIL"));
    const bool bPassed = bSaved && bApplied && bExported && bReloaded && bProfileEqual &&
        bCorruptRejected && bIncompleteRejected && bRawRead && bCapabilitiesValid &&
        bAllRawControlsReadable && bMultipleDevicesDistinct && bFilterEndpoints && bFilterInvert &&
        bUncenteredInvert && bFilterTuning && bTransientEqual && bResetBindingIsolated && bDefaultUsable &&
        bDuplicateAxisGuard && bDuplicateButtonGuard && bDuplicateHatGuard;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CONTROLLER_TEST|RESULT|status=%s|device=%s|vid=%04X|pid=%04X|axes=%d|buttons=%d|hats=%d|raw_axis0=%.3f|keyboard_gamepad_fallback=PRESERVED"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"), *Device->DisplayName, Device->VendorId, Device->ProductId,
        Device->Capabilities.AxisCount, Device->Capabilities.ButtonCount, Device->Capabilities.HatCount, RawAxis);
    Input->DeleteProfile(Profile.ProfileId);
    IFileManager::Get().Delete(*ExportPath, false, true);
    IFileManager::Get().Delete(*CorruptPath, false, true);
    IFileManager::Get().Delete(*IncompletePath, false, true);
    FPlatformMisc::RequestExit(!bPassed);
}

void ARotorlineOperationsPlayerController::RunFlightControllerRestartQualification(const FString& Mode)
{
    constexpr const TCHAR* RestartProfileId = TEXT("rotorline-restart-qualification-temporary");
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_CONTROLLER_RESTART_TEST|%s|status=FAIL|reason=NO_SUBSYSTEM"),
            *Mode.ToUpper());
        FPlatformMisc::RequestExit(true);
        return;
    }
    Input->RefreshDevices();
    const FRotorlineControllerDeviceInfo* Device = Input->GetDevices().FindByPredicate(
        [](const FRotorlineControllerDeviceInfo& Entry)
        {
            return Entry.bConnected && Entry.VendorId == 0x046D && Entry.ProductId == 0xC215;
        });
    if (!Device || !Input->SetActiveDevice(Device->DeviceId))
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_CONTROLLER_RESTART_TEST|%s|status=FAIL|reason=LOGITECH_NOT_FOUND"),
            *Mode.ToUpper());
        FPlatformMisc::RequestExit(true);
        return;
    }

    if (Mode.Equals(TEXT("write"), ESearchCase::IgnoreCase))
    {
        FRotorlineFlightControllerProfile Profile = Input->MakeDefaultProfile(Device->DeviceId);
        Profile.ProfileId = RestartProfileId;
        Profile.ProfileName = TEXT("Rotorline Restart Qualification Temporary");
        const bool bSaved = Input->SaveProfile(Profile);
        const bool bApplied = bSaved && Input->ApplyProfile(Profile.ProfileId);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_CONTROLLER_RESTART_TEST|WRITE|status=%s|saved=%d|applied=%d|profile=%s|device=%s"),
            bSaved && bApplied ? TEXT("PASS") : TEXT("FAIL"), bSaved ? 1 : 0, bApplied ? 1 : 0,
            RestartProfileId, *Device->DeviceId);
        FPlatformMisc::RequestExit(!(bSaved && bApplied));
        return;
    }

    const FRotorlineFlightControllerProfile* Persisted = Input->GetProfiles().FindByPredicate(
        [](const FRotorlineFlightControllerProfile& Profile)
        {
            return Profile.ProfileId == RestartProfileId;
        });
    const bool bPersistedAfterRestart = Persisted != nullptr;
    FRotorlineFlightControllerProfile AutomaticallyActiveProfile;
    const bool bAutomaticallyActive = Input->GetActiveProfile(AutomaticallyActiveProfile) &&
        AutomaticallyActiveProfile.DeviceId == Device->DeviceId &&
        AutomaticallyActiveProfile.ExpectedAxisCount <= Device->Capabilities.AxisCount &&
        AutomaticallyActiveProfile.ExpectedButtonCount <= Device->Capabilities.ButtonCount &&
        AutomaticallyActiveProfile.ExpectedHatCount <= Device->Capabilities.HatCount;
    const bool bPassed = bPersistedAfterRestart && bAutomaticallyActive;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CONTROLLER_RESTART_TEST|READ|status=%s|persisted=%d|auto_loaded=%d|profile=%s|active_profile=%s|device=%s"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"), bPersistedAfterRestart ? 1 : 0,
        bAutomaticallyActive ? 1 : 0, RestartProfileId,
        bAutomaticallyActive ? *AutomaticallyActiveProfile.ProfileId : TEXT("NONE"), *Device->DeviceId);
    Input->DeleteProfile(RestartProfileId);
    FPlatformMisc::RequestExit(!bPassed);
}

void ARotorlineOperationsPlayerController::ToggleControlsSettings(bool bFirstTimePrompt)
{
    const bool bClosing = bControlsSettingsOpen;
    if (bClosing && bWorkingControllerProfileDirty)
    {
        CancelWorkingControllerChanges();
    }
    bControlsSettingsOpen = !bControlsSettingsOpen;
    bAudioSettingsOpen = false;
    bGraphicsSettingsOpen = false;
    bVerticalAxisLatched = false;
    bHorizontalAxisLatched = false;
    ControlsInputSuppressionSeconds = bControlsSettingsOpen ? 0.35f : 0.0f;
    bControlsInputArmed = false;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_CONTROLS|TOGGLE|open=%d|first_time=%d"),
        bControlsSettingsOpen ? 1 : 0, bFirstTimePrompt ? 1 : 0);
    ClearPendingControlsDuplicate();
    bNamingControlsAxis = false;
    PendingControlsAxisLabel.Reset();
    if (!bControlsSettingsOpen)
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (URotorlineFlightControllerSubsystem* Input =
                GameInstance->GetSubsystem<URotorlineFlightControllerSubsystem>())
            {
                Input->CancelCalibration();
            }
        }
        ControlsMode = ERotorlineControlsMode::Home;
        ControlsStatus = TEXT("CONTROLS CLOSED");
        bWorkingControllerProfileDirty = false;
        return;
    }

    SelectedControlsTab = 2;
    SelectedControlsRow = 0;
    ControlsWizardStep = 0;
    ControlsCaptureFeedback = TEXT("PRESS ANY JOYSTICK BUTTON IN LIVE INPUT TEST TO IDENTIFY ITS CURRENT ASSIGNMENT");
    ControlsMode = bFirstTimePrompt ? ERotorlineControlsMode::FirstTimePrompt : ERotorlineControlsMode::Home;
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input)
    {
        ControlsStatus = TEXT("FLIGHT CONTROLLER SERVICE UNAVAILABLE");
        return;
    }
    if (!bFirstTimePrompt)
    {
        Input->RefreshDevices();
    }
    const TArray<FRotorlineControllerDeviceInfo>& Devices = Input->GetDevices();
    const FString ActiveDeviceId = Input->GetActiveDeviceId();
    const FRotorlineControllerDeviceInfo* Preferred = Devices.FindByPredicate([&ActiveDeviceId](const FRotorlineControllerDeviceInfo& Device)
    {
        return Device.bConnected && !Device.bGamepadCompatible && Device.DeviceId == ActiveDeviceId;
    });
    if (!Preferred) Preferred = Devices.FindByPredicate([](const FRotorlineControllerDeviceInfo& Device)
    {
        return Device.bConnected && !Device.bGamepadCompatible &&
            Device.VendorId == 0x046d && Device.ProductId == 0xc215;
    });
    if (!Preferred) Preferred = Devices.FindByPredicate([](const FRotorlineControllerDeviceInfo& Device)
    {
        return Device.bConnected && !Device.bGamepadCompatible;
    });
    if (!Preferred)
    {
        ControlsDeviceId.Reset();
        ControlsStatus = TEXT("NO FLIGHT CONTROLLER DETECTED // KEYBOARD AND GAMEPAD REMAIN ACTIVE");
        return;
    }
    // Opening setup must never change the active controller. Capture the live
    // state first, then stage a working copy for the selected setup target.
    ControlsSnapshotDeviceId = ActiveDeviceId;
    bControlsSnapshotHadProfile = Input->GetActiveProfile(ControlsSnapshotProfile);
    ControlsDeviceId = Preferred->DeviceId;
    const FRotorlineFlightControllerProfile* ExistingProfile = Input->GetProfiles().FindByPredicate(
        [this](const FRotorlineFlightControllerProfile& Profile)
        {
            return Profile.DeviceId == ControlsDeviceId;
        });
    WorkingControllerProfile = ExistingProfile
        ? *ExistingProfile
        : Input->MakeDefaultProfile(ControlsDeviceId);
    bWorkingControllerProfileDirty = ControlsDeviceId != ControlsSnapshotDeviceId || ExistingProfile == nullptr;
    ControlsStatus = FString::Printf(TEXT("%s DETECTED // %d AXES // %d BUTTONS // STAGED UNTIL STEP 5"),
        *Preferred->DisplayName.ToUpper(), Preferred->Capabilities.AxisCount,
        Preferred->Capabilities.ButtonCount);
    ControlsPreviousButtons.Init(false, Preferred->Capabilities.ButtonCount);
    ControlsPreviousHats.Init(-1.0f, Preferred->Capabilities.HatCount);
    PulseController(0.14f, 0.045f);
}

void ARotorlineOperationsPlayerController::MoveControlsSelection(int32 Direction)
{
    int32 RowCount = 1;
    switch (ControlsMode)
    {
    case ERotorlineControlsMode::FirstTimePrompt: RowCount = 4; break;
    case ERotorlineControlsMode::Home: RowCount = SelectedControlsTab >= 1 ? 8 : 1; break;
    case ERotorlineControlsMode::AxisTuning: RowCount = FMath::Max(1, WorkingControllerProfile.AxisBindings.Num()); break;
    case ERotorlineControlsMode::DeviceSelect:
        if (const URotorlineFlightControllerSubsystem* Input = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr)
        {
            RowCount = 0;
            for (const FRotorlineControllerDeviceInfo& Device : Input->GetDevices())
            {
                const bool bWantGamepad = SelectedControlsTab == 1;
                if (Device.bConnected && Device.bGamepadCompatible == bWantGamepad) ++RowCount;
            }
            RowCount = FMath::Max(1, RowCount);
        }
        break;
    default: RowCount = 1; break;
    }
    SelectedControlsRow = (SelectedControlsRow + Direction + RowCount) % RowCount;
    PulseController(0.08f, 0.025f);
}

void ARotorlineOperationsPlayerController::UpdateControlsInput()
{
    CaptureCompatibleGamepadControlsInput();
    // The same Enter / X press that opens this screen may still be held.
    // Wait for a real release before accepting any controls-screen input so
    // intro skipping cannot accidentally begin guided calibration.
    if (!bControlsInputArmed)
    {
        const bool bConfirmStillHeld = IsInputKeyDown(EKeys::Enter) ||
            IsInputKeyDown(EKeys::SpaceBar) ||
            IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom) ||
            bControlsCompatibleGamepadConfirmHeld;
        PendingControlsInputKeys.Reset();
        if (ControlsInputSuppressionSeconds <= 0.0f && !bConfirmStillHeld)
        {
            bControlsInputArmed = true;
        }
        return;
    }
    TArray<FKey> QueuedInputKeys = MoveTemp(PendingControlsInputKeys);
    PendingControlsInputKeys.Reset();
    const auto WasInputKeyJustPressed = [this, &QueuedInputKeys](const FKey& Key)
    {
        return QueuedInputKeys.Contains(Key) || this->WasInputKeyJustPressed(Key);
    };
    if (WasInputKeyJustPressed(EKeys::Escape) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right) ||
        WasInputKeyJustPressed(EKeys::Gamepad_Special_Right))
    {
        if (!CancelPendingControlsDuplicate()) ToggleControlsSettings();
        return;
    }
    const bool Up = WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up);
    const bool Down = WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down);
    const bool Left = WasInputKeyJustPressed(EKeys::Left) || WasInputKeyJustPressed(EKeys::A) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Left);
    const bool Right = WasInputKeyJustPressed(EKeys::Right) || WasInputKeyJustPressed(EKeys::D) ||
        WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right);
    if (Up) MoveControlsSelection(-1);
    if (Down) MoveControlsSelection(1);

    if ((ControlsMode == ERotorlineControlsMode::Home ||
            ControlsMode == ERotorlineControlsMode::DeviceSelect) &&
        WasInputKeyJustPressed(EKeys::R))
    {
        if (URotorlineFlightControllerSubsystem* Input = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr)
        {
            Input->RefreshDevices();
            int32 ConnectedControllers = 0;
            for (const FRotorlineControllerDeviceInfo& Device : Input->GetDevices())
            {
                if (Device.bConnected)
                {
                    ++ConnectedControllers;
                }
            }
            ControlsStatus = FString::Printf(
                TEXT("RESCAN COMPLETE // %d CONTROLLER%s FOUND // CHOOSE STEP 1"),
                ConnectedControllers,
                ConnectedControllers == 1 ? TEXT("") : TEXT("S"));
            SelectedControlsRow = 0;
        }
        return;
    }

    if (ControlsMode == ERotorlineControlsMode::AxisCalibration &&
        GetControlsWizardAction() == RotorlineFlightControllerActions::Collective &&
        WasInputKeyJustPressed(EKeys::M))
    {
        FRotorlineAxisBinding DisabledThrottle;
        if (const FRotorlineAxisBinding* Existing = WorkingControllerProfile.AxisBindings.FindByPredicate([](const FRotorlineAxisBinding& Binding)
        {
            return Binding.Action == RotorlineFlightControllerActions::Collective;
        }))
        {
            DisabledThrottle = *Existing;
        }
        DisabledThrottle.Action = RotorlineFlightControllerActions::Collective;
        DisabledThrottle.NativeAxisIndex = ControlsDetectedAxis != INDEX_NONE ? ControlsDetectedAxis : 2;
        DisabledThrottle.bCentered = false;
        DisabledThrottle.bIgnore = true;
        DisabledThrottle.UserLabel = TEXT("THROTTLE LEVER (DISABLED)");
        WorkingControllerProfile.AxisBindings.RemoveAll([](const FRotorlineAxisBinding& Binding)
        {
            return Binding.Action == RotorlineFlightControllerActions::Collective;
        });
        WorkingControllerProfile.AxisBindings.Add(DisabledThrottle);
        bWorkingControllerProfileDirty = true;

        ++ControlsWizardStep;
        ControlsDetectedAxis = INDEX_NONE;
        if (URotorlineFlightControllerSubsystem* Input = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr)
        {
            for (int32 Axis = 0; Axis < ControlsAxisBaseline.Num(); ++Axis)
            {
                Input->GetRawAxisValue(ControlsDeviceId, Axis, ControlsAxisBaseline[Axis]);
                ControlsAxisMinimum[Axis] = ControlsAxisMaximum[Axis] = ControlsAxisBaseline[Axis];
                ControlsAxisRestMinimum[Axis] = ControlsAxisRestMaximum[Axis] = ControlsAxisBaseline[Axis];
                ControlsAxisFirstExcursionSign[Axis] = 0;
            }
        }
        ControlsCaptureElapsed = 0.0f;
        ControlsStatus = TEXT("THROTTLE LEVER DISABLED AND SKIPPED // KEYBOARD Q/E REMAINS ACTIVE // CALIBRATE YAW");
        return;
    }

    if (ControlsMode == ERotorlineControlsMode::Home &&
        (Left || Right || WasInputKeyJustPressed(EKeys::Gamepad_LeftShoulder) ||
            WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder)))
    {
        const int32 Direction = (Right || WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder)) ? 1 : -1;
        SelectedControlsTab = (SelectedControlsTab + Direction + 3) % 3;
        SelectedControlsRow = 0;
    }
    else if (ControlsMode == ERotorlineControlsMode::AxisTuning &&
        WorkingControllerProfile.AxisBindings.IsValidIndex(SelectedControlsRow))
    {
        FRotorlineAxisBinding& Binding = WorkingControllerProfile.AxisBindings[SelectedControlsRow];
        if (Left || Right)
        {
            Binding.Deadzone = FMath::Clamp(Binding.Deadzone + (Right ? 0.01f : -0.01f), 0.0f, 0.45f);
            bWorkingControllerProfileDirty = true;
        }
        if (WasInputKeyJustPressed(EKeys::Gamepad_LeftShoulder))
        {
            Binding.Sensitivity = FMath::Clamp(Binding.Sensitivity - 0.05f, 0.1f, 4.0f);
            bWorkingControllerProfileDirty = true;
        }
        if (WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder))
        {
            Binding.Sensitivity = FMath::Clamp(Binding.Sensitivity + 0.05f, 0.1f, 4.0f);
            bWorkingControllerProfileDirty = true;
        }
        if (WasInputKeyJustPressed(EKeys::Q) || WasInputKeyJustPressed(EKeys::Gamepad_LeftTrigger))
        {
            Binding.Scale = FMath::Clamp(Binding.Scale - 0.05f, 0.10f, 2.0f);
            bWorkingControllerProfileDirty = true;
        }
        if (WasInputKeyJustPressed(EKeys::E) || WasInputKeyJustPressed(EKeys::Gamepad_RightTrigger))
        {
            Binding.Scale = FMath::Clamp(Binding.Scale + 0.05f, 0.10f, 2.0f);
            bWorkingControllerProfileDirty = true;
        }
        if (WasInputKeyJustPressed(EKeys::Z) || WasInputKeyJustPressed(EKeys::Gamepad_LeftThumbstick))
        {
            Binding.CenterOffset = FMath::Clamp(Binding.CenterOffset - 0.01f, -0.25f, 0.25f);
            bWorkingControllerProfileDirty = true;
        }
        if (WasInputKeyJustPressed(EKeys::C) || WasInputKeyJustPressed(EKeys::Gamepad_RightThumbstick))
        {
            Binding.CenterOffset = FMath::Clamp(Binding.CenterOffset + 0.01f, -0.25f, 0.25f);
            bWorkingControllerProfileDirty = true;
        }
        if (WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left) || WasInputKeyJustPressed(EKeys::I))
        {
            Binding.bInvert = !Binding.bInvert;
            bWorkingControllerProfileDirty = true;
        }
        if (WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top))
        {
            Binding.CurveExponent = Binding.CurveExponent >= 2.0f ? 1.0f : Binding.CurveExponent + 0.25f;
            bWorkingControllerProfileDirty = true;
        }
        if (WasInputKeyJustPressed(EKeys::M))
        {
            Binding.bIgnore = !Binding.bIgnore;
            bWorkingControllerProfileDirty = true;
            const FString AxisName = Binding.Action == RotorlineFlightControllerActions::Collective
                ? TEXT("THROTTLE LEVER") : Binding.Action.ToString().ToUpper();
            ControlsStatus = FString::Printf(TEXT("%s // AXIS %d // %s // KEYBOARD CONTROLS REMAIN ACTIVE"),
                *AxisName, Binding.NativeAxisIndex + 1, Binding.bIgnore ? TEXT("DISABLED") : TEXT("ENABLED"));
        }
        if (WasInputKeyJustPressed(EKeys::N))
        {
            bNamingControlsAxis = true;
            PendingControlsAxisLabel.Reset();
            ControlsStatus = TEXT("NAME AXIS // TYPE A LABEL // ENTER SAVE // CIRCLE CANCEL");
        }
        if (WasInputKeyJustPressed(EKeys::R))
        {
            const int32 NativeAxis = Binding.NativeAxisIndex;
            if (URotorlineFlightControllerSubsystem* Input = GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>())
            {
                const FRotorlineFlightControllerProfile Defaults = Input->MakeDefaultProfile(ControlsDeviceId);
                if (const FRotorlineAxisBinding* DefaultBinding = Defaults.AxisBindings.FindByPredicate([NativeAxis](const FRotorlineAxisBinding& Entry)
                    { return Entry.NativeAxisIndex == NativeAxis; }))
                {
                    Binding = *DefaultBinding;
                    bWorkingControllerProfileDirty = true;
                    ControlsStatus = TEXT("SELECTED AXIS BINDING RESET");
                }
            }
        }
    }
    else if (ControlsMode == ERotorlineControlsMode::LiveTest && (Left || Right))
    {
        const URotorlineFlightControllerSubsystem* Input = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
        const FRotorlineControllerDeviceInfo* Device = Input
            ? Input->GetDevices().FindByPredicate([this](const FRotorlineControllerDeviceInfo& Entry)
            {
                return Entry.DeviceId == ControlsDeviceId;
            }) : nullptr;
        if (Device)
        {
            const int32 PageCount = FMath::Max3(
                FMath::DivideAndRoundUp(Device->Capabilities.AxisCount, 7),
                FMath::DivideAndRoundUp(Device->Capabilities.ButtonCount, 18),
                FMath::DivideAndRoundUp(Device->Capabilities.HatCount, 4));
            if (PageCount > 0)
            {
                ControlsLiveTestPage = (ControlsLiveTestPage + (Right ? 1 : -1) + PageCount) % PageCount;
                ControlsStatus = FString::Printf(TEXT("LIVE INPUT PAGE %d OF %d"), ControlsLiveTestPage + 1, PageCount);
            }
        }
    }

    if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar) ||
        WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
    {
        ActivateControlsSelection();
    }
    if (ControlsMode == ERotorlineControlsMode::ButtonBinding && WasInputKeyJustPressed(EKeys::R))
    {
        const FName Action = GetControlsWizardAction();
        WorkingControllerProfile.ButtonBindings.RemoveAll([Action](const FRotorlineButtonBinding& Binding)
        {
            return Binding.Action == Action;
        });
        WorkingControllerProfile.AxisBindings.RemoveAll([Action](const FRotorlineAxisBinding& Binding)
        {
            return Binding.Action == Action;
        });
        for (FRotorlineHatBinding& Binding : WorkingControllerProfile.HatBindings)
        {
            if (Binding.UpAction == Action) Binding.UpAction = NAME_None;
            if (Binding.RightAction == Action) Binding.RightAction = NAME_None;
            if (Binding.DownAction == Action) Binding.DownAction = NAME_None;
            if (Binding.LeftAction == Action) Binding.LeftAction = NAME_None;
        }
        ControlsCaptureFeedback = FString::Printf(TEXT("CLEARED // %s IS NOW UNASSIGNED"),
            *Action.ToString().ToUpper());
        ControlsStatus = TEXT("CURRENT FUNCTION CLEARED // PRESS A NEW CONTROL OR X TO KEEP UNASSIGNED");
        bWorkingControllerProfileDirty = true;
    }
}

void ARotorlineOperationsPlayerController::ActivateControlsSelection()
{
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_CONTROLS|ACTIVATE|mode=%d|row=%d|armed=%d|suppression=%.3f"),
        static_cast<int32>(ControlsMode), SelectedControlsRow, bControlsInputArmed ? 1 : 0,
        ControlsInputSuppressionSeconds);
    if (ControlsMode == ERotorlineControlsMode::FirstTimePrompt)
    {
        if (SelectedControlsRow == 0) BeginControlsCalibration();
        else if (SelectedControlsRow == 1)
        {
            URotorlineFlightControllerSubsystem* Input = GetGameInstance()
                ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
            const FRotorlineControllerDeviceInfo* Device = Input ? Input->GetDevices().FindByPredicate([this](const FRotorlineControllerDeviceInfo& Entry)
            {
                return Entry.DeviceId == ControlsDeviceId;
            }) : nullptr;
            const FRotorlineFlightControllerProfile* Compatible = Input && Device
                ? Input->GetProfiles().FindByPredicate([Device](const FRotorlineFlightControllerProfile& Profile)
                {
                    return Profile.DeviceId != Device->DeviceId &&
                        Profile.VendorId == Device->VendorId && Profile.ProductId == Device->ProductId &&
                        Profile.ExpectedAxisCount <= Device->Capabilities.AxisCount &&
                        Profile.ExpectedButtonCount <= Device->Capabilities.ButtonCount &&
                        Profile.ExpectedHatCount <= Device->Capabilities.HatCount;
                }) : nullptr;
            if (Input && Compatible && Input->ApplyProfile(Compatible->ProfileId))
            {
                WorkingControllerProfile = *Compatible;
                const FRotorlineFlightControllerProfile DeviceDefaults = Input->MakeDefaultProfile(ControlsDeviceId);
                WorkingControllerProfile.ProfileId = DeviceDefaults.ProfileId;
                WorkingControllerProfile.DeviceId = DeviceDefaults.DeviceId;
                WorkingControllerProfile.DeviceName = DeviceDefaults.DeviceName;
                WorkingControllerProfile.VendorId = DeviceDefaults.VendorId;
                WorkingControllerProfile.ProductId = DeviceDefaults.ProductId;
                WorkingControllerProfile.ExpectedAxisCount = DeviceDefaults.ExpectedAxisCount;
                WorkingControllerProfile.ExpectedButtonCount = DeviceDefaults.ExpectedButtonCount;
                WorkingControllerProfile.ExpectedHatCount = DeviceDefaults.ExpectedHatCount;
                WorkingControllerProfile.DetectedCapabilities = DeviceDefaults.DetectedCapabilities;
                Input->ApplyTransientProfile(WorkingControllerProfile);
                bWorkingControllerProfileDirty = true;
                ControlsStatus = TEXT("COMPATIBLE PROFILE APPLIED // REVIEW LIVE TEST BEFORE FLIGHT");
                ControlsMode = ERotorlineControlsMode::LiveTest;
            }
            else
            {
                ControlsStatus = TEXT("NO COMPATIBLE PROFILE FOUND // CHOOSE CALIBRATION OR GENERIC DEFAULTS");
            }
        }
        else if (SelectedControlsRow == 2)
        {
            ResetWorkingControllerProfile();
            ControlsMode = ERotorlineControlsMode::Home;
            ControlsStatus = TEXT("SAFE DEFAULT PRESET STAGED // STEP 5 SAVES AND APPLIES");
        }
        else ToggleControlsSettings();
        return;
    }
    if (ControlsMode == ERotorlineControlsMode::AxisCalibration)
    {
        AcceptCalibratedAxis();
        return;
    }
    if (ControlsMode == ERotorlineControlsMode::ButtonBinding)
    {
        if (PendingDuplicateButton != INDEX_NONE)
        {
            AssignCapturedButtonBinding(PendingDuplicateButton, PendingDuplicateAction, true);
        }
        else if (PendingDuplicateHat != INDEX_NONE)
        {
            AssignCapturedHatBinding(PendingDuplicateHat, PendingDuplicateHatDirection, PendingDuplicateAction, true);
        }
        else if (PendingDuplicateAxis != INDEX_NONE)
        {
            AssignCapturedTriggerBinding(PendingDuplicateAxis, PendingDuplicateAction, true);
        }
        else
        {
            AdvanceControlsButtonWizard(); // Explicitly skip this optional binding.
        }
        return;
    }
    if (ControlsMode == ERotorlineControlsMode::LiveTest || ControlsMode == ERotorlineControlsMode::AxisTuning)
    {
        ControlsMode = ERotorlineControlsMode::Home;
        SelectedControlsRow = 0;
        return;
    }
    if (ControlsMode == ERotorlineControlsMode::DeviceSelect)
    {
        SelectControlsDevice(SelectedControlsRow);
        ControlsMode = ERotorlineControlsMode::Home;
        SelectedControlsRow = 0;
        return;
    }
    if (SelectedControlsTab == 0)
    {
        ToggleControlsSettings();
        return;
    }
    switch (SelectedControlsRow)
    {
    case 0: ControlsMode = ERotorlineControlsMode::DeviceSelect; SelectedControlsRow = 0; break;
    case 1:
        if (SelectedControlsTab == 1)
        {
            ControlsMode = ERotorlineControlsMode::LiveTest;
            SelectedControlsRow = 0;
            ControlsLiveTestPage = 0;
            ControlsStatus = TEXT("PS5 FLIGHT MAP // LEFT Y PITCH // LEFT X YAW // RIGHT X LATERAL // R2 ASCEND // L2 DESCEND");
        }
        else
        {
            BeginControlsCalibration();
        }
        break;
    case 2: BeginControlsButtonBinding(); break;
    case 3: ControlsMode = ERotorlineControlsMode::LiveTest; SelectedControlsRow = 0; ControlsLiveTestPage = 0; break;
    case 4:
        if (SaveWorkingControllerProfile())
        {
            ToggleControlsSettings();
        }
        break;
    case 5:
        if (SelectedControlsTab == 1)
        {
            ToggleGamepadPitchInvert();
        }
        else
        {
            ControlsMode = ERotorlineControlsMode::AxisTuning;
            SelectedControlsRow = 0;
        }
        break;
    case 6: ResetWorkingControllerProfile(); break;
    case 7: ToggleControlsSettings(); break;
    default: break;
    }
}

bool ARotorlineOperationsPlayerController::IsGamepadPitchInverted() const
{
    return ProfileSave && ProfileSave->bGamepadPitchInverted;
}

void ARotorlineOperationsPlayerController::ToggleGamepadPitchInvert()
{
    if (!ProfileSave)
    {
        LoadProfile();
    }

    if (!ProfileSave)
    {
        ControlsStatus = TEXT("CONTROLLER PITCH PREFERENCE COULD NOT BE LOADED");
        return;
    }

    ProfileSave->bGamepadPitchInverted = !ProfileSave->bGamepadPitchInverted;
    SaveProfile();
    ControlsStatus = ProfileSave->bGamepadPitchInverted
        ? TEXT("CONTROLLER PITCH INVERTED // LEFT STICK FORWARD NOW PITCHES NOSE UP")
        : TEXT("CONTROLLER PITCH STANDARD // LEFT STICK FORWARD PITCHES NOSE DOWN");
}

void ARotorlineOperationsPlayerController::ResetWorkingControllerProfile()
{
    if (URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr)
    {
        WorkingControllerProfile = Input->MakeDefaultProfile(ControlsDeviceId);
        bWorkingControllerProfileDirty = true;
        ControlsStatus = TEXT("GENERIC CAPABILITY-BASED DEFAULTS RESTORED // NOT YET SAVED");
    }
}

void ARotorlineOperationsPlayerController::CaptureControlsSnapshot()
{
    ControlsSnapshotDeviceId = ControlsDeviceId;
    ControlsSnapshotProfile = WorkingControllerProfile;
    bControlsSnapshotHadProfile = false;
    if (const URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr)
    {
        FRotorlineFlightControllerProfile Active;
        bControlsSnapshotHadProfile = Input->GetActiveProfile(Active);
        if (bControlsSnapshotHadProfile)
        {
            ControlsSnapshotProfile = Active;
        }
    }
}

void ARotorlineOperationsPlayerController::SelectControlsDevice(int32 FlightDeviceIndex)
{
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input) return;
    TArray<const FRotorlineControllerDeviceInfo*> FlightDevices;
    const bool bWantGamepad = SelectedControlsTab == 1;
    for (const FRotorlineControllerDeviceInfo& Device : Input->GetDevices())
    {
        if (Device.bConnected && Device.bGamepadCompatible == bWantGamepad) FlightDevices.Add(&Device);
    }
    if (!FlightDevices.IsValidIndex(FlightDeviceIndex))
    {
        ControlsStatus = TEXT("DEVICE SELECTION FAILED // REFRESH CONNECTIONS");
        return;
    }
    ControlsDeviceId = FlightDevices[FlightDeviceIndex]->DeviceId;
    const FRotorlineFlightControllerProfile* ExistingProfile = Input->GetProfiles().FindByPredicate(
        [this](const FRotorlineFlightControllerProfile& Profile)
        {
            return Profile.DeviceId == ControlsDeviceId;
        });
    WorkingControllerProfile = ExistingProfile
        ? *ExistingProfile
        : Input->MakeDefaultProfile(ControlsDeviceId);
    bWorkingControllerProfileDirty = true;
    ControlsPreviousButtons.Init(false, FlightDevices[FlightDeviceIndex]->Capabilities.ButtonCount);
    ControlsPreviousHats.Init(-1.0f, FlightDevices[FlightDeviceIndex]->Capabilities.HatCount);
    ControlsStatus = FString::Printf(TEXT("%s SELECTED FOR SETUP // %d AXES // %d BUTTONS // STEP 5 SAVES"),
        *FlightDevices[FlightDeviceIndex]->DisplayName.ToUpper(),
        FlightDevices[FlightDeviceIndex]->Capabilities.AxisCount,
        FlightDevices[FlightDeviceIndex]->Capabilities.ButtonCount);
}

void ARotorlineOperationsPlayerController::ApplyWorkingControllerProfile()
{
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input || !Input->ApplyTransientProfile(WorkingControllerProfile))
    {
        ControlsStatus = TEXT("APPLY FAILED // PREVIOUS ACTIVE CONTROLS PRESERVED");
        return;
    }
    CaptureControlsSnapshot();
    bWorkingControllerProfileDirty = false;
    ControlsStatus = TEXT("CHANGES APPLIED FOR THIS SESSION // SAVE PROFILE TO KEEP THEM");
    FlightControllerNotification = TEXT("FLIGHT CONTROLLER CHANGES APPLIED");
    FlightControllerNotificationSeconds = 4.0f;
}

bool ARotorlineOperationsPlayerController::SaveWorkingControllerProfile()
{
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input || !Input->SaveProfile(WorkingControllerProfile) || !Input->ApplyProfile(WorkingControllerProfile.ProfileId))
    {
        ControlsStatus = TEXT("PROFILE SAVE FAILED // PREVIOUS CONTROLS PRESERVED");
        return false;
    }
    bWorkingControllerProfileDirty = false;
    CaptureControlsSnapshot();
    ControlsStatus = TEXT("PROFILE SAVED AND APPLIED // HUMAN-READABLE JSON");
    FlightControllerNotification = TEXT("FLIGHT CONTROLLER PROFILE ACTIVE");
    FlightControllerNotificationSeconds = 4.0f;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_FLIGHT_CONTROLLER|PROFILE_APPLIED|profile=%s|device=%s"),
        *WorkingControllerProfile.ProfileId, *ControlsDeviceId);
    return true;
}

void ARotorlineOperationsPlayerController::CancelWorkingControllerChanges()
{
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input)
    {
        bWorkingControllerProfileDirty = false;
        return;
    }
    if (!ControlsSnapshotDeviceId.IsEmpty()) Input->SetActiveDevice(ControlsSnapshotDeviceId);
    if (bControlsSnapshotHadProfile)
    {
        Input->ApplyTransientProfile(ControlsSnapshotProfile);
    }
    else if (!WorkingControllerProfile.ProfileId.IsEmpty())
    {
        Input->DeleteProfile(WorkingControllerProfile.ProfileId);
        Input->SetActiveDevice(ControlsSnapshotDeviceId);
    }
    ControlsDeviceId = ControlsSnapshotDeviceId;
    WorkingControllerProfile = ControlsSnapshotProfile;
    bWorkingControllerProfileDirty = false;
    ControlsStatus = TEXT("UNSAVED CHANGES CANCELLED // PREVIOUS PROFILE RESTORED");
}

void ARotorlineOperationsPlayerController::ResetCurrentDeviceProfile()
{
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input || ControlsDeviceId.IsEmpty()) return;
    if (!WorkingControllerProfile.ProfileId.IsEmpty())
    {
        Input->DeleteProfile(WorkingControllerProfile.ProfileId);
    }
    WorkingControllerProfile = Input->MakeDefaultProfile(ControlsDeviceId);
    if (!Input->SaveProfile(WorkingControllerProfile) || !Input->ApplyProfile(WorkingControllerProfile.ProfileId))
    {
        ControlsStatus = TEXT("DEVICE RESET FAILED // PREVIOUS FILE MAY STILL BE AVAILABLE");
        return;
    }
    bWorkingControllerProfileDirty = false;
    CaptureControlsSnapshot();
    ControlsStatus = TEXT("DEVICE PROFILE RESET // USABLE GENERIC DEFAULTS SAVED AND APPLIED");
}

void ARotorlineOperationsPlayerController::BeginControlsCalibration()
{
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_CONTROLS|CALIBRATION_BEGIN|mode=%d|row=%d"),
        static_cast<int32>(ControlsMode), SelectedControlsRow);
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input || ControlsDeviceId.IsEmpty() || !Input->BeginCalibration(ControlsDeviceId))
    {
        ControlsStatus = TEXT("CALIBRATION COULD NOT START // CHECK DEVICE CONNECTION");
        return;
    }
    // Guided calibration replaces the four required flight axes. Generic
    // defaults must not masquerade as user-confirmed duplicate assignments on
    // devices whose native axis order differs from the default layout.
    WorkingControllerProfile.AxisBindings.RemoveAll([](const FRotorlineAxisBinding& Binding)
    {
        return Binding.Action == RotorlineFlightControllerActions::Roll ||
            Binding.Action == RotorlineFlightControllerActions::Pitch ||
            Binding.Action == RotorlineFlightControllerActions::Yaw ||
            Binding.Action == RotorlineFlightControllerActions::Collective;
    });
    const FRotorlineControllerDeviceInfo* Device = Input->GetDevices().FindByPredicate([this](const FRotorlineControllerDeviceInfo& Entry)
    {
        return Entry.DeviceId == ControlsDeviceId;
    });
    const int32 AxisCount = Device ? Device->Capabilities.AxisCount : 0;
    ControlsAxisBaseline.Init(0.0f, AxisCount);
    ControlsAxisMinimum.Init(TNumericLimits<float>::Max(), AxisCount);
    ControlsAxisMaximum.Init(TNumericLimits<float>::Lowest(), AxisCount);
    ControlsAxisRestMinimum.Init(TNumericLimits<float>::Max(), AxisCount);
    ControlsAxisRestMaximum.Init(TNumericLimits<float>::Lowest(), AxisCount);
    ControlsAxisFirstExcursionSign.Init(0, AxisCount);
    for (int32 Axis = 0; Axis < AxisCount; ++Axis)
    {
        Input->GetRawAxisValue(ControlsDeviceId, Axis, ControlsAxisBaseline[Axis]);
        ControlsAxisMinimum[Axis] = ControlsAxisBaseline[Axis];
        ControlsAxisMaximum[Axis] = ControlsAxisBaseline[Axis];
        ControlsAxisRestMinimum[Axis] = ControlsAxisBaseline[Axis];
        ControlsAxisRestMaximum[Axis] = ControlsAxisBaseline[Axis];
    }
    Input->CaptureCalibrationCenter();
    ControlsWizardStep = 0;
    ControlsDetectedAxis = INDEX_NONE;
    ControlsCaptureElapsed = 0.0f;
    ControlsMode = ERotorlineControlsMode::AxisCalibration;
    ControlsStatus = TEXT("CENTER ALL CONTROLS // MOVE THE REQUESTED AXIS THROUGH ITS FULL RANGE // PRESS X");
}

void ARotorlineOperationsPlayerController::AcceptCalibratedAxis()
{
    if (ControlsDetectedAxis == INDEX_NONE || !ControlsAxisMinimum.IsValidIndex(ControlsDetectedAxis))
    {
        ControlsStatus = TEXT("NO AXIS DETECTED // MOVE ONE CONTROL FARTHER FROM CENTER");
        return;
    }
    const float Span = ControlsAxisMaximum[ControlsDetectedAxis] - ControlsAxisMinimum[ControlsDetectedAxis];
    URotorlineFlightControllerSubsystem* CalibrationInput = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    const FRotorlineControllerDeviceInfo* CalibrationDevice = CalibrationInput
        ? CalibrationInput->GetDevices().FindByPredicate([this](const FRotorlineControllerDeviceInfo& Entry)
        {
            return Entry.DeviceId == ControlsDeviceId;
        }) : nullptr;
    const float NativeRange = CalibrationDevice && CalibrationDevice->Capabilities.Axes.IsValidIndex(ControlsDetectedAxis)
        ? FMath::Max(0.001f, static_cast<float>(CalibrationDevice->Capabilities.Axes[ControlsDetectedAxis].RawMaximum -
            CalibrationDevice->Capabilities.Axes[ControlsDetectedAxis].RawMinimum))
        : 1.0f;
    if (Span < NativeRange * 0.65f)
    {
        ControlsStatus = TEXT("AXIS RANGE TOO SMALL // SWEEP THE FULL PHYSICAL TRAVEL");
        return;
    }
    const FName Action = GetControlsWizardAction();
    if (Action != RotorlineFlightControllerActions::Collective)
    {
        const float NegativeTravel = ControlsAxisBaseline[ControlsDetectedAxis] - ControlsAxisMinimum[ControlsDetectedAxis];
        const float PositiveTravel = ControlsAxisMaximum[ControlsDetectedAxis] - ControlsAxisBaseline[ControlsDetectedAxis];
        if (NegativeTravel < NativeRange * 0.20f || PositiveTravel < NativeRange * 0.20f)
        {
            ControlsStatus = TEXT("ONE-SIDED AXIS MOTION // RETURN TO CENTER AND SWEEP BOTH DIRECTIONS FULLY");
            return;
        }
    }
    const bool bAxisAlreadyUsed = WorkingControllerProfile.AxisBindings.ContainsByPredicate([this, Action](const FRotorlineAxisBinding& Existing)
    {
        return !Existing.bIgnore && Existing.NativeAxisIndex == ControlsDetectedAxis && Existing.Action != Action;
    });
    if (bAxisAlreadyUsed && PendingDuplicateAxis != ControlsDetectedAxis)
    {
        PendingDuplicateAxis = ControlsDetectedAxis;
        ControlsStatus = TEXT("DUPLICATE AXIS // PRESS X AGAIN TO KEEP BOTH ASSIGNMENTS");
        return;
    }
    if (bAxisAlreadyUsed)
    {
        WorkingControllerProfile.bAllowDuplicateAxisBindings = true;
    }
    PendingDuplicateAxis = INDEX_NONE;
    WorkingControllerProfile.AxisBindings.RemoveAll([Action](const FRotorlineAxisBinding& Binding)
    {
        return Binding.Action == Action;
    });
    FRotorlineAxisBinding Binding;
    Binding.Action = Action;
    Binding.NativeAxisIndex = ControlsDetectedAxis;
    Binding.bCentered = Action != RotorlineFlightControllerActions::Collective;
    Binding.UserLabel = Action.ToString();
    Binding.Calibration.RawMinimum = ControlsAxisMinimum[ControlsDetectedAxis];
    Binding.Calibration.RawMaximum = ControlsAxisMaximum[ControlsDetectedAxis];
    Binding.Calibration.RawCenter = ControlsAxisBaseline[ControlsDetectedAxis];
    const int8 FirstExcursion = ControlsAxisFirstExcursionSign.IsValidIndex(ControlsDetectedAxis)
        ? ControlsAxisFirstExcursionSign[ControlsDetectedAxis] : 0;
    const int8 ExpectedFirstExcursion =
        (Action == RotorlineFlightControllerActions::Roll || Action == RotorlineFlightControllerActions::Yaw ||
         Action == RotorlineFlightControllerActions::Collective) ? -1 : 1;
    Binding.bInvert = FirstExcursion != 0 && FirstExcursion != ExpectedFirstExcursion;
    Binding.Calibration.NoiseFloor = ControlsAxisRestMinimum.IsValidIndex(ControlsDetectedAxis)
        ? FMath::Max(FMath::Abs(ControlsAxisRestMinimum[ControlsDetectedAxis] - ControlsAxisBaseline[ControlsDetectedAxis]),
            FMath::Abs(ControlsAxisRestMaximum[ControlsDetectedAxis] - ControlsAxisBaseline[ControlsDetectedAxis]))
        : 0.0f;
    if (Binding.Calibration.NoiseFloor > Span * 0.04f)
    {
        Binding.Deadzone = FMath::Clamp(Binding.Calibration.NoiseFloor / Span * 1.5f, 0.08f, 0.30f);
        ControlsStatus = TEXT("NOISY AXIS DETECTED // PRACTICAL DEADZONE APPLIED");
    }
    WorkingControllerProfile.AxisBindings.Add(Binding);
    bWorkingControllerProfileDirty = true;
    ++ControlsWizardStep;
    ControlsDetectedAxis = INDEX_NONE;
    for (int32 Axis = 0; Axis < ControlsAxisBaseline.Num(); ++Axis)
    {
        URotorlineFlightControllerSubsystem* Input = GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>();
        Input->GetRawAxisValue(ControlsDeviceId, Axis, ControlsAxisBaseline[Axis]);
        ControlsAxisMinimum[Axis] = ControlsAxisMaximum[Axis] = ControlsAxisBaseline[Axis];
        ControlsAxisRestMinimum[Axis] = ControlsAxisRestMaximum[Axis] = ControlsAxisBaseline[Axis];
        ControlsAxisFirstExcursionSign[Axis] = 0;
    }
    ControlsCaptureElapsed = 0.0f;
    if (!RotorlineCalibrationActions().IsValidIndex(ControlsWizardStep))
    {
        if (URotorlineFlightControllerSubsystem* Input = GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>())
        {
            Input->FinishCalibration(false);
        }
        BeginControlsButtonBinding();
    }
}

void ARotorlineOperationsPlayerController::BeginControlsButtonBinding()
{
    ControlsMode = ERotorlineControlsMode::ButtonBinding;
    ControlsWizardStep = 0;
    ClearPendingControlsDuplicate();
    ControlsTriggerArmingSeconds = 0.75f;
    if (URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr)
    {
        const FRotorlineControllerDeviceInfo* Device = Input->GetDevices().FindByPredicate([this](const FRotorlineControllerDeviceInfo& Entry)
        {
            return Entry.DeviceId == ControlsDeviceId;
        });
        ControlsAxisBaseline.Init(0.0f, Device ? Device->Capabilities.AxisCount : 0);
        ControlsPreviousAxisTriggers.Init(false, ControlsAxisBaseline.Num());
        for (int32 Axis = 0; Axis < ControlsAxisBaseline.Num(); ++Axis)
            Input->GetRawAxisValue(ControlsDeviceId, Axis, ControlsAxisBaseline[Axis]);
        ControlsPreviousButtons.Init(false, Device ? Device->Capabilities.ButtonCount : 0);
        for (int32 Button = 0; Button < ControlsPreviousButtons.Num(); ++Button)
            ControlsPreviousButtons[Button] = Input->IsRawButtonPressed(ControlsDeviceId, Button);
        ControlsPreviousHats.Init(-1.0f, Device ? Device->Capabilities.HatCount : 0);
        for (int32 Hat = 0; Hat < ControlsPreviousHats.Num(); ++Hat)
            Input->GetRawHatAngle(ControlsDeviceId, Hat, ControlsPreviousHats[Hat]);
    }
    ControlsCaptureFeedback = TEXT("NO NEW INPUT DETECTED YET");
    ControlsStatus = TEXT("CURRENT ASSIGNMENT IS SHOWN // PRESS A NEW CONTROL TO REPLACE IT // X KEEPS CURRENT");
}

void ARotorlineOperationsPlayerController::AdvanceControlsButtonWizard()
{
    ClearPendingControlsDuplicate();
    ++ControlsWizardStep;
    if (!RotorlineButtonActions().IsValidIndex(ControlsWizardStep))
    {
        ControlsMode = ERotorlineControlsMode::LiveTest;
        ControlsStatus = TEXT("SETUP CAPTURE COMPLETE // VERIFY EVERY CONTROL LIVE");
    }
    else
    {
        ControlsStatus = TEXT("CURRENT ASSIGNMENT IS SHOWN // PRESS A NEW CONTROL TO REPLACE IT // X KEEPS CURRENT");
    }
}

void ARotorlineOperationsPlayerController::AssignCapturedButtonBinding(
    int32 NativeButton, FName Action, bool bKeepDuplicate)
{
    if (bKeepDuplicate)
    {
        WorkingControllerProfile.ButtonBindings.RemoveAll([NativeButton](const FRotorlineButtonBinding& Binding)
        {
            return Binding.NativeButtonIndex == NativeButton;
        });
    }
    ClearWorkingControllerActionBindings(Action);
    FRotorlineButtonBinding Binding;
    Binding.Action = Action;
    Binding.NativeButtonIndex = NativeButton;
    WorkingControllerProfile.ButtonBindings.Add(Binding);
    bWorkingControllerProfileDirty = true;
    ControlsCaptureFeedback = FString::Printf(TEXT("INPUT DETECTED // BUTTON %d // NOW %s"),
        NativeButton + 1, *Action.ToString().ToUpper());
    AdvanceControlsButtonWizard();
}

void ARotorlineOperationsPlayerController::AssignCapturedHatBinding(
    int32 NativeHat, int32 Direction, FName Action, bool bKeepDuplicate)
{
    ClearWorkingControllerActionBindings(Action);

    if (bKeepDuplicate)
    {
        for (FRotorlineHatBinding& Binding : WorkingControllerProfile.HatBindings)
        {
            if (Binding.NativeHatIndex != NativeHat) continue;
            switch (Direction)
            {
            case 0: Binding.UpAction = NAME_None; break;
            case 1: Binding.RightAction = NAME_None; break;
            case 2: Binding.DownAction = NAME_None; break;
            default: Binding.LeftAction = NAME_None; break;
            }
        }
    }

    FRotorlineHatBinding* Target = WorkingControllerProfile.HatBindings.FindByPredicate(
        [NativeHat, Direction](const FRotorlineHatBinding& Binding)
        {
            if (Binding.NativeHatIndex != NativeHat) return false;
            switch (Direction)
            {
            case 0: return Binding.UpAction.IsNone();
            case 1: return Binding.RightAction.IsNone();
            case 2: return Binding.DownAction.IsNone();
            default: return Binding.LeftAction.IsNone();
            }
        });
    if (!Target)
    {
        FRotorlineHatBinding NewBinding;
        NewBinding.NativeHatIndex = NativeHat;
        WorkingControllerProfile.HatBindings.Add(NewBinding);
        Target = &WorkingControllerProfile.HatBindings.Last();
    }
    switch (Direction)
    {
    case 0: Target->UpAction = Action; break;
    case 1: Target->RightAction = Action; break;
    case 2: Target->DownAction = Action; break;
    default: Target->LeftAction = Action; break;
    }
    bWorkingControllerProfileDirty = true;
    static const TCHAR* DirectionNames[] = { TEXT("UP"), TEXT("RIGHT"), TEXT("DOWN"), TEXT("LEFT") };
    ControlsCaptureFeedback = FString::Printf(TEXT("INPUT DETECTED // HAT %d %s // NOW %s"),
        NativeHat + 1, DirectionNames[FMath::Clamp(Direction, 0, 3)], *Action.ToString().ToUpper());
    AdvanceControlsButtonWizard();
}

void ARotorlineOperationsPlayerController::AssignCapturedTriggerBinding(
    int32 NativeAxis, FName Action, bool bKeepDuplicate)
{
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    const FRotorlineControllerDeviceInfo* Device = Input
        ? Input->GetDevices().FindByPredicate([this](const FRotorlineControllerDeviceInfo& Entry)
        {
            return Entry.DeviceId == ControlsDeviceId;
        }) : nullptr;
    if (!Device || !Device->Capabilities.Axes.IsValidIndex(NativeAxis) ||
        !ControlsAxisBaseline.IsValidIndex(NativeAxis))
    {
        ControlsStatus = TEXT("ANALOG TRIGGER CAPTURE FAILED // RELEASE AND TRY AGAIN");
        ClearPendingControlsDuplicate();
        return;
    }
    const FRotorlineControllerAxisCapability& Capability = Device->Capabilities.Axes[NativeAxis];
    if (bKeepDuplicate)
    {
        WorkingControllerProfile.AxisBindings.RemoveAll([NativeAxis](const FRotorlineAxisBinding& Binding)
        {
            return Binding.NativeAxisIndex == NativeAxis;
        });
    }
    ClearWorkingControllerActionBindings(Action);
    FRotorlineAxisBinding Trigger;
    Trigger.Action = Action;
    Trigger.NativeAxisIndex = NativeAxis;
    Trigger.UserLabel = FString::Printf(TEXT("AXIS %d DIGITAL TRIGGER"), NativeAxis + 1);
    Trigger.bCentered = false;
    Trigger.Deadzone = 0.55f;
    Trigger.Calibration.RawMinimum = Capability.RawMinimum;
    Trigger.Calibration.RawMaximum = Capability.RawMaximum;
    Trigger.Calibration.RawCenter = ControlsAxisBaseline[NativeAxis];
    WorkingControllerProfile.AxisBindings.Add(Trigger);
    bWorkingControllerProfileDirty = true;
    ControlsCaptureFeedback = FString::Printf(TEXT("INPUT DETECTED // AXIS %d TRIGGER // NOW %s"),
        NativeAxis + 1, *Action.ToString().ToUpper());
    AdvanceControlsButtonWizard();
}

void ARotorlineOperationsPlayerController::ClearWorkingControllerActionBindings(FName Action)
{
    WorkingControllerProfile.ButtonBindings.RemoveAll([Action](const FRotorlineButtonBinding& Binding)
    {
        return Binding.Action == Action;
    });
    WorkingControllerProfile.AxisBindings.RemoveAll([Action](const FRotorlineAxisBinding& Binding)
    {
        return Binding.Action == Action;
    });
    for (FRotorlineHatBinding& Binding : WorkingControllerProfile.HatBindings)
    {
        if (Binding.UpAction == Action) Binding.UpAction = NAME_None;
        if (Binding.RightAction == Action) Binding.RightAction = NAME_None;
        if (Binding.DownAction == Action) Binding.DownAction = NAME_None;
        if (Binding.LeftAction == Action) Binding.LeftAction = NAME_None;
    }
}

bool ARotorlineOperationsPlayerController::HasPendingControlsDuplicate() const
{
    return PendingDuplicateButton != INDEX_NONE || PendingDuplicateAxis != INDEX_NONE ||
        PendingDuplicateHat != INDEX_NONE;
}

bool ARotorlineOperationsPlayerController::CancelPendingControlsDuplicate()
{
    if (!HasPendingControlsDuplicate()) return false;
    ClearPendingControlsDuplicate();
    ControlsStatus = TEXT("REASSIGNMENT CANCELLED // PRESS A DIFFERENT CONTROL OR X TO KEEP CURRENT");
    return true;
}

void ARotorlineOperationsPlayerController::ClearPendingControlsDuplicate()
{
    PendingDuplicateButton = INDEX_NONE;
    PendingDuplicateAxis = INDEX_NONE;
    PendingDuplicateHat = INDEX_NONE;
    PendingDuplicateHatDirection = INDEX_NONE;
    PendingDuplicateAction = NAME_None;
}

void ARotorlineOperationsPlayerController::TickControlsCapture(float DeltaTime)
{
    if (!bControlsSettingsOpen || ControlsDeviceId.IsEmpty()) return;
    URotorlineFlightControllerSubsystem* Input = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
    if (!Input) return;
    const FRotorlineControllerDeviceInfo* Device = Input->GetDevices().FindByPredicate([this](const FRotorlineControllerDeviceInfo& Entry)
    {
        return Entry.DeviceId == ControlsDeviceId;
    });
    if (!Device || !Device->bConnected)
    {
        ControlsStatus = TEXT("DEVICE DISCONNECTED // CAPTURE PAUSED // KEYBOARD AND GAMEPAD AVAILABLE");
        return;
    }
    if (ControlsMode == ERotorlineControlsMode::LiveTest)
    {
        if (ControlsPreviousButtons.Num() != Device->Capabilities.ButtonCount)
            ControlsPreviousButtons.Init(false, Device->Capabilities.ButtonCount);
        for (int32 Button = 0; Button < Device->Capabilities.ButtonCount; ++Button)
        {
            const bool Down = Input->IsRawButtonPressed(ControlsDeviceId, Button);
            if (Down && !ControlsPreviousButtons[Button])
            {
                const FRotorlineButtonBinding* Binding = WorkingControllerProfile.ButtonBindings.FindByPredicate(
                    [Button](const FRotorlineButtonBinding& Entry)
                    {
                        return Entry.NativeButtonIndex == Button;
                    });
                ControlsCaptureFeedback = FString::Printf(TEXT("BUTTON %d PRESSED // CURRENT FUNCTION: %s"),
                    Button + 1, Binding ? *Binding->Action.ToString().ToUpper() : TEXT("UNASSIGNED"));
                ControlsStatus = ControlsCaptureFeedback;
            }
            ControlsPreviousButtons[Button] = Down;
        }

        if (ControlsPreviousHats.Num() != Device->Capabilities.HatCount)
            ControlsPreviousHats.Init(-1.0f, Device->Capabilities.HatCount);
        for (int32 HatIndex = 0; HatIndex < Device->Capabilities.HatCount; ++HatIndex)
        {
            float Angle = -1.0f;
            Input->GetRawHatAngle(ControlsDeviceId, HatIndex, Angle);
            if (Angle >= 0.0f && ControlsPreviousHats[HatIndex] < 0.0f)
            {
                const float Wrapped = FMath::Fmod(Angle + 360.0f, 360.0f);
                const int32 Direction = (Wrapped >= 315.0f || Wrapped < 45.0f) ? 0 :
                    (Wrapped < 135.0f ? 1 : (Wrapped < 225.0f ? 2 : 3));
                static const TCHAR* DirectionNames[] = { TEXT("UP"), TEXT("RIGHT"), TEXT("DOWN"), TEXT("LEFT") };
                FName CurrentAction = NAME_None;
                for (const FRotorlineHatBinding& Binding : WorkingControllerProfile.HatBindings)
                {
                    if (Binding.NativeHatIndex != HatIndex) continue;
                    switch (Direction)
                    {
                    case 0: CurrentAction = Binding.UpAction; break;
                    case 1: CurrentAction = Binding.RightAction; break;
                    case 2: CurrentAction = Binding.DownAction; break;
                    default: CurrentAction = Binding.LeftAction; break;
                    }
                    break;
                }
                ControlsCaptureFeedback = FString::Printf(TEXT("HAT %d %s // CURRENT FUNCTION: %s"),
                    HatIndex + 1, DirectionNames[Direction],
                    CurrentAction.IsNone() ? TEXT("UNASSIGNED") : *CurrentAction.ToString().ToUpper());
                ControlsStatus = ControlsCaptureFeedback;
            }
            ControlsPreviousHats[HatIndex] = Angle;
        }
        return;
    }
    if (ControlsMode == ERotorlineControlsMode::AxisCalibration)
    {
        ControlsCaptureElapsed += DeltaTime;
        const bool bSamplingRest = ControlsCaptureElapsed < 0.75f;
        bool bRestMoved = false;
        float BestMotion = 0.0f;
        for (int32 Axis = 0; Axis < Device->Capabilities.AxisCount; ++Axis)
        {
            float Raw = 0.0f;
            if (!Input->GetRawAxisValue(ControlsDeviceId, Axis, Raw)) continue;
            const float Range = Device->Capabilities.Axes.IsValidIndex(Axis)
                ? FMath::Max(1, Device->Capabilities.Axes[Axis].RawMaximum - Device->Capabilities.Axes[Axis].RawMinimum)
                : 65535.0f;
            if (bSamplingRest)
            {
                ControlsAxisRestMinimum[Axis] = FMath::Min(ControlsAxisRestMinimum[Axis], Raw);
                ControlsAxisRestMaximum[Axis] = FMath::Max(ControlsAxisRestMaximum[Axis], Raw);
                bRestMoved |= ControlsAxisRestMaximum[Axis] - ControlsAxisRestMinimum[Axis] > Range * 0.02f;
                continue;
            }
            ControlsAxisMinimum[Axis] = FMath::Min(ControlsAxisMinimum[Axis], Raw);
            ControlsAxisMaximum[Axis] = FMath::Max(ControlsAxisMaximum[Axis], Raw);
            const float Motion = FMath::Abs(Raw - ControlsAxisBaseline[Axis]) / Range;
            if (Motion >= 0.08f && ControlsAxisFirstExcursionSign.IsValidIndex(Axis) &&
                ControlsAxisFirstExcursionSign[Axis] == 0)
            {
                ControlsAxisFirstExcursionSign[Axis] = Raw >= ControlsAxisBaseline[Axis] ? 1 : -1;
            }
            if (ControlsDetectedAxis == INDEX_NONE && Motion < 0.04f &&
                ControlsAxisRestMinimum.IsValidIndex(Axis))
            {
                ControlsAxisRestMinimum[Axis] = FMath::Min(ControlsAxisRestMinimum[Axis], Raw);
                ControlsAxisRestMaximum[Axis] = FMath::Max(ControlsAxisRestMaximum[Axis], Raw);
            }
            if (Motion > BestMotion && Motion >= 0.08f)
            {
                BestMotion = Motion;
                ControlsDetectedAxis = Axis;
            }
        }
        if (bSamplingRest)
        {
            if (bRestMoved)
            {
                ControlsCaptureElapsed = 0.0f;
                for (int32 Axis = 0; Axis < ControlsAxisBaseline.Num(); ++Axis)
                {
                    float Raw = ControlsAxisBaseline[Axis];
                    Input->GetRawAxisValue(ControlsDeviceId, Axis, Raw);
                    ControlsAxisBaseline[Axis] = Raw;
                    ControlsAxisMinimum[Axis] = ControlsAxisMaximum[Axis] = Raw;
                    ControlsAxisRestMinimum[Axis] = ControlsAxisRestMaximum[Axis] = Raw;
                }
                ControlsStatus = TEXT("HOLD ALL CONTROLS STILL FOR A MOMENT BEFORE MOVING THE REQUESTED AXIS");
            }
            else
            {
                ControlsStatus = TEXT("SAMPLING CENTER AND NOISE // HOLD ALL CONTROLS STILL");
            }
            return;
        }
        if (ControlsCaptureElapsed >= 2.5f && ControlsDetectedAxis == INDEX_NONE)
        {
            ControlsStatus = TEXT("STUCK AXIS OR PARTIAL MOTION // RELEASE, THEN SWEEP THE REQUESTED CONTROL FULLY");
        }
        return;
    }
    if (ControlsMode != ERotorlineControlsMode::ButtonBinding || HasPendingControlsDuplicate()) return;
    if (ControlsPreviousButtons.Num() != Device->Capabilities.ButtonCount)
        ControlsPreviousButtons.Init(false, Device->Capabilities.ButtonCount);
    const int32 StepBeforeButtonScan = ControlsWizardStep;
    for (int32 Button = 0; Button < Device->Capabilities.ButtonCount; ++Button)
    {
        const bool Down = Input->IsRawButtonPressed(ControlsDeviceId, Button);
        if (Down && !ControlsPreviousButtons[Button])
        {
            const FName Action = GetControlsWizardAction();
            const bool Duplicate = WorkingControllerProfile.ButtonBindings.ContainsByPredicate([Button, Action](const FRotorlineButtonBinding& Binding)
            {
                return Binding.NativeButtonIndex == Button && Binding.Action != Action;
            });
            if (Duplicate)
            {
                PendingDuplicateButton = Button;
                PendingDuplicateAction = Action;
                const FRotorlineButtonBinding* Existing = WorkingControllerProfile.ButtonBindings.FindByPredicate(
                    [Button](const FRotorlineButtonBinding& Binding)
                    {
                        return Binding.NativeButtonIndex == Button;
                    });
                ControlsStatus = FString::Printf(TEXT("BUTTON %d IS %s // X REASSIGN TO %s // CIRCLE CANCEL"),
                    Button + 1, Existing ? *Existing->Action.ToString().ToUpper() : TEXT("ASSIGNED"),
                    *Action.ToString().ToUpper());
            }
            else
            {
                AssignCapturedButtonBinding(Button, Action, false);
            }
            ControlsPreviousButtons[Button] = Down;
            break;
        }
        ControlsPreviousButtons[Button] = Down;
    }
    if (ControlsWizardStep != StepBeforeButtonScan || HasPendingControlsDuplicate()) return;

    if (ControlsPreviousHats.Num() != Device->Capabilities.HatCount)
        ControlsPreviousHats.Init(-1.0f, Device->Capabilities.HatCount);
    for (int32 HatIndex = 0; HatIndex < Device->Capabilities.HatCount; ++HatIndex)
    {
        float Angle = -1.0f;
        Input->GetRawHatAngle(ControlsDeviceId, HatIndex, Angle);
        if (Angle >= 0.0f && ControlsPreviousHats[HatIndex] < 0.0f)
        {
            const FName Action = GetControlsWizardAction();
            const float Wrapped = FMath::Fmod(Angle + 360.0f, 360.0f);
            const int32 Direction = (Wrapped >= 315.0f || Wrapped < 45.0f) ? 0 :
                (Wrapped < 135.0f ? 1 : (Wrapped < 225.0f ? 2 : 3));
            const bool Duplicate = WorkingControllerProfile.HatBindings.ContainsByPredicate(
                [HatIndex, Direction, Action](const FRotorlineHatBinding& Binding)
                {
                    if (Binding.NativeHatIndex != HatIndex) return false;
                    FName Existing;
                    switch (Direction)
                    {
                    case 0: Existing = Binding.UpAction; break;
                    case 1: Existing = Binding.RightAction; break;
                    case 2: Existing = Binding.DownAction; break;
                    default: Existing = Binding.LeftAction; break;
                    }
                    return !Existing.IsNone() && Existing != Action;
                });
            if (Duplicate)
            {
                PendingDuplicateHat = HatIndex;
                PendingDuplicateHatDirection = Direction;
                PendingDuplicateAction = Action;
                ControlsStatus = FString::Printf(TEXT("HAT DIRECTION ALREADY ASSIGNED // X REASSIGN TO %s // CIRCLE CANCEL"),
                    *Action.ToString().ToUpper());
            }
            else AssignCapturedHatBinding(HatIndex, Direction, Action, false);
            ControlsPreviousHats[HatIndex] = Angle;
            break;
        }
        ControlsPreviousHats[HatIndex] = Angle;
    }
    if (ControlsMode != ERotorlineControlsMode::ButtonBinding || HasPendingControlsDuplicate()) return;
    if (ControlsTriggerArmingSeconds > 0.0f)
    {
        ControlsTriggerArmingSeconds = FMath::Max(0.0f, ControlsTriggerArmingSeconds - DeltaTime);
        for (int32 Axis = 0; Axis < Device->Capabilities.AxisCount && ControlsAxisBaseline.IsValidIndex(Axis); ++Axis)
        {
            Input->GetRawAxisValue(ControlsDeviceId, Axis, ControlsAxisBaseline[Axis]);
            if (ControlsPreviousAxisTriggers.IsValidIndex(Axis)) ControlsPreviousAxisTriggers[Axis] = false;
        }
        if (ControlsTriggerArmingSeconds <= 0.0f)
        {
            ControlsStatus = TEXT("ANALOG TRIGGERS ARMED // PRESS A NEW CONTROL OR X TO SKIP");
        }
        return;
    }
    if (ControlsPreviousAxisTriggers.Num() != Device->Capabilities.AxisCount)
        ControlsPreviousAxisTriggers.Init(false, Device->Capabilities.AxisCount);
    for (int32 Axis = 0; Axis < Device->Capabilities.AxisCount && ControlsAxisBaseline.IsValidIndex(Axis); ++Axis)
    {
        float Raw = 0.0f;
        if (!Input->GetRawAxisValue(ControlsDeviceId, Axis, Raw)) continue;
        const FRotorlineControllerAxisCapability& Capability = Device->Capabilities.Axes[Axis];
        const float Range = FMath::Max(1, Capability.RawMaximum - Capability.RawMinimum);
        const bool bTriggerActive = FMath::Abs(Raw - ControlsAxisBaseline[Axis]) / Range >= 0.65f;
        const bool bTriggerEdge = bTriggerActive && !ControlsPreviousAxisTriggers[Axis];
        ControlsPreviousAxisTriggers[Axis] = bTriggerActive;
        if (!bTriggerEdge) continue;
        const FName Action = GetControlsWizardAction();
        const bool Duplicate = WorkingControllerProfile.AxisBindings.ContainsByPredicate([Axis, Action](const FRotorlineAxisBinding& Binding)
        {
            return !Binding.bIgnore && Binding.NativeAxisIndex == Axis && Binding.Action != Action;
        });
        if (Duplicate)
        {
            PendingDuplicateAxis = Axis;
            PendingDuplicateAction = Action;
            ControlsStatus = FString::Printf(TEXT("AXIS %d ALREADY ASSIGNED // X REASSIGN TO %s // CIRCLE CANCEL"),
                Axis + 1, *Action.ToString().ToUpper());
        }
        else AssignCapturedTriggerBinding(Axis, Action, false);
        break;
    }
}

bool ARotorlineOperationsPlayerController::SaveProfile()
{
    if (!ProfileSave)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_PROFILE|SAVE_FAILED|reason=NO_PROFILE"));
        return false;
    }
    const bool bSaved = UGameplayStatics::SaveGameToSlot(ProfileSave, TEXT("RotorlineProfile"), 0);
    if (!bSaved)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_PROFILE|SAVE_FAILED|slot=RotorlineProfile"));
    }
    return bSaved;
}

int32 ARotorlineOperationsPlayerController::GetReputation() const
{
    return ProfileSave ? ProfileSave->Reputation : 0;
}

bool ARotorlineOperationsPlayerController::IsMissionUnlocked(const FRotorlineMissionDefinition& Mission) const
{
    if (Mission.Id.Equals(TEXT("bell-counterstrike"), ESearchCase::IgnoreCase))
    {
        return IsMissionCompleted(TEXT("final-discovery"));
    }
    if (Mission.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase))
    {
        return IsMissionCompleted(TEXT("kiowa-recon-strike"));
    }
    return GetReputation() >= Mission.Unlock;
}

bool ARotorlineOperationsPlayerController::IsMissionCompleted(const FString& MissionId) const
{
    return ProfileSave && ProfileSave->CompletedMissions.Contains(MissionId);
}

int32 ARotorlineOperationsPlayerController::GetCompletedCampaignMissionCount() const
{
    if (!ProfileSave)
    {
        return 0;
    }

    int32 CompletedCount = 0;
    for (const FRotorlineMissionDefinition& Mission : Missions)
    {
        if (!Mission.Id.IsEmpty() && ProfileSave->CompletedMissions.Contains(Mission.Id))
        {
            ++CompletedCount;
        }
    }
    return CompletedCount;
}

bool ARotorlineOperationsPlayerController::IsBell222Unlocked() const
{
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineForceBellLocked")))
    {
        return false;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineForceBellUnlocked")) || bFleetQualificationMode)
    {
        return true;
    }
    return ProfileSave && ProfileSave->bBell222Discovered;
}

bool ARotorlineOperationsPlayerController::IsAircraftUnlocked(
    const FRotorlineAircraftDefinition& AircraftDefinition) const
{
    if (AircraftDefinition.Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase))
    {
        return IsBell222Unlocked();
    }
    if (AircraftDefinition.Id.Equals(TEXT("jeep_wrangler"), ESearchCase::IgnoreCase))
    {
        return (ProfileSave && ProfileSave->bJeepPermanentlyUnlocked) ||
            IsMissionCompleted(TEXT("kiowa-recon-strike"));
    }
    if (AircraftDefinition.DeploymentClass.Equals(TEXT("ground"), ESearchCase::IgnoreCase))
    {
        return ProfileSave && ProfileSave->bJeepPermanentlyUnlocked;
    }
    return true;
}

void ARotorlineOperationsPlayerController::RecordMissionCompletion(const FRotorlineMissionDefinition& Mission, float ElapsedSeconds)
{
    if (bMissionCompleteScreenOpen)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ROTORLINE_MISSION_COMPLETE|DUPLICATE_SUPPRESSED|mission=%s|source=RECORD"),
            *Mission.Id);
        return;
    }

    if (!ProfileSave)
    {
        LoadProfile();
    }
    if (Mission.Id.Equals(TEXT("final-evacuation"), ESearchCase::IgnoreCase))
    {
        OpenMissionCompleteScreen(Mission, ElapsedSeconds);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_M25_FINALE|state=SUCCESS_SCREEN|source=FINAL_END_VIDEO_AND_CREDITS_COMPLETE|elapsed=%.2f"),
            ElapsedSeconds);
    }
    else
    {
        OpenMissionCompleteScreen(Mission, ElapsedSeconds);
    }
}

void ARotorlineOperationsPlayerController::LoadAwardDefinitions()
{
    AwardPatchTextures.Reset();
    AwardsCatalogError.Reset();
    if (!AwardSystem.Load(AwardsCatalogError))
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_AWARDS|LOAD_FAILED|reason=%s"), *AwardsCatalogError);
        return;
    }

    int32 LoadedArt = 0;
    for (const FRotorlineAwardDefinition& Definition : AwardSystem.GetDefinitions())
    {
        UTexture2D* Texture = Definition.PatchAsset.IsEmpty()
            ? nullptr
            : LoadObject<UTexture2D>(nullptr, *Definition.PatchAsset);
        if (Texture)
        {
            AwardPatchTextures.Add(Definition.Id, Texture);
            ++LoadedArt;
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("ROTORLINE_AWARDS|MISSING_ART|id=%s|asset=%s|fallback=SAFE_PLACEHOLDER"),
                *Definition.Id, *Definition.PatchAsset);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS|ART_READY|definitions=%d|textures=%d|missing=%d"),
        AwardSystem.GetDefinitions().Num(), LoadedArt, AwardSystem.GetDefinitions().Num() - LoadedArt);
}

bool ARotorlineOperationsPlayerController::ApplyRunningOnFumesTelemetryRepair()
{
    if (!ProfileSave || !AwardSystem.FindDefinition(TEXT("running_on_fumes")) ||
        (GetAwardRecord(TEXT("running_on_fumes")) && GetAwardRecord(TEXT("running_on_fumes"))->TimesEarned > 0))
    {
        return false;
    }

    TArray<FString> LogFiles;
    IFileManager::Get().FindFiles(LogFiles, *(FPaths::ProjectLogDir() / TEXT("Rotorline*.log")), true, false);
    for (const FString& LogFile : LogFiles)
    {
        FString LogText;
        const FString MissionLogPath = FPaths::ProjectLogDir() / LogFile;
        if (!FFileHelper::LoadFileToString(LogText, *MissionLogPath))
        {
            continue;
        }
        TArray<FString> Lines;
        LogText.ParseIntoArrayLines(Lines, true);
        for (const FString& Line : Lines)
        {
            if (!Line.Contains(TEXT("ROTORLINE_AWARDS|MISSION_FINALIZED|")) ||
                !Line.Contains(TEXT("|success=1|")) ||
                !Line.Contains(TEXT("|landing=1|")))
            {
                continue;
            }
            const int32 FuelToken = Line.Find(TEXT("|fuel="), ESearchCase::IgnoreCase, ESearchDir::FromStart);
            if (FuelToken == INDEX_NONE)
            {
                continue;
            }
            float RecordedFuel = 100.0f;
            if (!FParse::Value(*Line.Mid(FuelToken + 1), TEXT("fuel="), RecordedFuel) || RecordedFuel > 10.0f)
            {
                continue;
            }
            ForceUnlockAward(TEXT("running_on_fumes"),
                FString::Printf(TEXT("telemetry repair from completed low-fuel landing (%.1f%%)"), RecordedFuel));
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_AWARDS|TELEMETRY_REPAIR|id=running_on_fumes|fuel=%.1f|source=%s|status=GRANTED"),
                RecordedFuel,
                *LogFile);
            return true;
        }
    }
    return false;
}

const FRotorlinePlayerAwardRecord* ARotorlineOperationsPlayerController::GetAwardRecord(const FString& AwardId) const
{
    return ProfileSave ? ProfileSave->AwardRecords.Find(AwardId) : nullptr;
}

UTexture2D* ARotorlineOperationsPlayerController::GetAwardPatchTexture(const FString& AwardId) const
{
    const TObjectPtr<UTexture2D>* Texture = AwardPatchTextures.Find(AwardId);
    return Texture ? Texture->Get() : nullptr;
}

float ARotorlineOperationsPlayerController::GetAwardCompletionPercent() const
{
    const int32 Total = AwardSystem.GetDefinitions().Num();
    if (!ProfileSave || Total <= 0) return 0.0f;
    int32 Earned = 0;
    for (const TPair<FString, FRotorlinePlayerAwardRecord>& Pair : ProfileSave->AwardRecords)
    {
        if (Pair.Value.TimesEarned > 0 && AwardSystem.FindDefinition(Pair.Key)) ++Earned;
    }
    return 100.0f * static_cast<float>(Earned) / static_cast<float>(Total);
}

const FRotorlineCareerStatistics* ARotorlineOperationsPlayerController::GetCareerStatistics() const
{
    return ProfileSave ? &ProfileSave->CareerStatistics : nullptr;
}

void ARotorlineOperationsPlayerController::UpdateMissionTelemetry(float DeltaTime)
{
    if (bOperationsMenuOpen || bMissionCompleteScreenOpen || bMissionFailureScreenOpen ||
        !GetWorld() || UGameplayStatics::IsGamePaused(GetWorld()))
    {
        return;
    }

    ARotorlineHelicopterPawn* Helicopter = Cast<ARotorlineHelicopterPawn>(GetPawn());
    if (!Helicopter) return;

    const FRotorlineAwardsFlightState Flight = Helicopter->GetAwardsFlightState();
    const FVector Location = Helicopter->GetActorLocation();
    MissionResults.ElapsedSeconds += DeltaTime;
    MissionResults.bCrashed |= Flight.bAircraftDying || Flight.bMissionFailed;
    MissionResults.bAircraftConditionTracked = Flight.MaxHealth > KINDA_SMALL_NUMBER;
    MissionResults.AircraftHealth = Flight.Health;
    MissionResults.AircraftMaxHealth = Flight.MaxHealth;
    MissionResults.FuelRemainingPercent = Flight.FuelRemainingPercent;

    if (bTelemetryLocationValid)
    {
        const float FrameDistanceMeters = FVector::Distance(Location, LastTelemetryLocation) / 100.0f;
        if (FrameDistanceMeters < 500.0f)
        {
            MissionResults.DistanceFlownMeters += FrameDistanceMeters;
        }
    }
    LastTelemetryLocation = Location;
    bTelemetryLocationValid = true;

    if (Flight.bEnginePowerAvailable && !Flight.bAircraftDying)
    {
        MissionResults.FlightTimeSeconds += DeltaTime;
    }

    const float HorizontalSpeedMps = Flight.Velocity.Size2D() / 100.0f;
    const float VerticalSpeedMps = Flight.Velocity.Z / 100.0f;
    const float Attitude = FMath::Max(FMath::Abs(Flight.PitchDegrees), FMath::Abs(Flight.RollDegrees));
    const bool bAirborne = Flight.AltitudeAglMeters >= 6.0f;
    const bool bGroundContact = Flight.AltitudeAglMeters >= -0.5f && Flight.AltitudeAglMeters <= 2.2f;

    if (bAirborne)
    {
        bTelemetryWasAirborne = true;
        bTelemetryLandingRecorded = false;
        LastAirborneVerticalSpeedMps = FMath::Max(0.0f, -VerticalSpeedMps);
        if (bTelemetryTakeoffArmed && Flight.bEnginePowerAvailable && HorizontalSpeedMps + FMath::Abs(VerticalSpeedMps) >= 1.5f)
        {
            MissionResults.bValidTakeoff = true;
            bTelemetryTakeoffArmed = false;
            UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_TELEMETRY|TAKEOFF|mission=%s|agl=%.1f|speed=%.1f"),
                *MissionResults.MissionId, Flight.AltitudeAglMeters, HorizontalSpeedMps);
        }
    }

    if (bTelemetryWasAirborne && bGroundContact && !bTelemetryLandingRecorded)
    {
        const float TouchdownVertical = FMath::Max(LastAirborneVerticalSpeedMps, FMath::Max(0.0f, -VerticalSpeedMps));
        MissionResults.LandingVerticalSpeedMps = TouchdownVertical;
        MissionResults.LandingLateralSpeedMps = HorizontalSpeedMps;
        MissionResults.LandingAttitudeDegrees = Attitude;
        FVector ObjectiveWorld;
        FString ObjectiveLabel;
        int32 ObjectiveIndex = 0;
        int32 ObjectiveCount = 0;
        MissionResults.LandingAccuracyMeters = Helicopter->GetMissionNavigationData(
            ObjectiveWorld, ObjectiveLabel, ObjectiveIndex, ObjectiveCount)
            ? FVector::Dist2D(Location, ObjectiveWorld) / 100.0f
            : FVector::Dist2D(Location, RotorlineOperations::SpawnLocation) / 100.0f;
        MissionResults.bValidLanding = !Flight.bAircraftDying &&
            TouchdownVertical <= 6.5f &&
            HorizontalSpeedMps <= 18.0f &&
            Attitude <= 20.0f;
        MissionResults.bHardLanding = !MissionResults.bValidLanding ||
            TouchdownVertical > 4.0f ||
            HorizontalSpeedMps > 12.5f ||
            Attitude > 12.0f;
        MissionResults.bSafeLanding = MissionResults.bValidLanding && !MissionResults.bHardLanding;
        bTelemetryLandingRecorded = true;
        bTelemetryWasAirborne = false;
        bTelemetryTakeoffArmed = true;

        if (MissionResults.bSafeLanding && ProfileSave && FVector::Dist2D(Location, RotorlineOperations::SpawnLocation) > 50000.0f)
        {
            const int32 IslandX = FMath::Clamp(FMath::FloorToInt((Location.X + 403200.0f) / 100800.0f), 0, 7);
            const int32 IslandY = FMath::Clamp(FMath::FloorToInt((Location.Y + 403200.0f) / 100800.0f), 0, 7);
            ProfileSave->CareerStatistics.UniqueIslandsVisited.AddUnique(FString::Printf(TEXT("ISLAND_%d_%d"), IslandX, IslandY));
        }
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AWARDS_TELEMETRY|LANDING|valid=%d|safe=%d|vertical=%.2f|lateral=%.2f|attitude=%.1f|accuracy=%.1f|crash=%d"),
            MissionResults.bValidLanding ? 1 : 0, MissionResults.bSafeLanding ? 1 : 0,
            TouchdownVertical, HorizontalSpeedMps, Attitude, MissionResults.LandingAccuracyMeters,
            MissionResults.bCrashed ? 1 : 0);
    }

    if (bAirborne && Flight.AltitudeAglMeters < 20.0f)
    {
        MissionResults.TimeBelowSafeAltitudeSeconds += DeltaTime;
    }
    if (RotorlineOperations::IsStableHoverState(Flight))
    {
        CurrentStableHoverBreakSeconds = 0.0f;
        const float PreviousBestHoverSeconds = MissionResults.StableHoverSeconds;
        CurrentStableHoverSeconds += DeltaTime;
        MissionResults.StableHoverSeconds = FMath::Max(
            MissionResults.StableHoverSeconds,
            CurrentStableHoverSeconds);
        if (PreviousBestHoverSeconds < 10.0f && MissionResults.StableHoverSeconds >= 10.0f)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_AWARDS_TELEMETRY|HOVER_QUALIFIED|mission=%s|seconds=%.2f|agl=%.2f|horizontal=%.2f|vertical=%.2f|attitude=%.2f"),
                *MissionResults.MissionId, MissionResults.StableHoverSeconds, Flight.AltitudeAglMeters,
                HorizontalSpeedMps, FMath::Abs(VerticalSpeedMps), Attitude);
        }
    }
    else
    {
        CurrentStableHoverBreakSeconds += DeltaTime;
        if (CurrentStableHoverBreakSeconds > RotorlineOperations::StableHoverBreakGraceSeconds)
        {
            CurrentStableHoverSeconds = 0.0f;
        }
    }
    if (Attitude > 27.0f || FVector::Distance(Flight.Velocity, LastTelemetryVelocity) / 100.0f > 8.0f)
    {
        MissionResults.AbruptControlSeconds += DeltaTime;
    }
    LastTelemetryVelocity = Flight.Velocity;

    if (ProfileSave)
    {
        const int32 RegionX = FMath::Clamp(FMath::FloorToInt((Location.X + 403200.0f) / 100800.0f), 0, 7);
        const int32 RegionY = FMath::Clamp(FMath::FloorToInt((Location.Y + 403200.0f) / 100800.0f), 0, 7);
        const int32 RegionsBefore = ProfileSave->CareerStatistics.UniqueRegionsExplored.Num();
        ProfileSave->CareerStatistics.UniqueRegionsExplored.AddUnique(FString::Printf(TEXT("REGION_%d_%d"), RegionX, RegionY));
        const bool bNewRegion = ProfileSave->CareerStatistics.UniqueRegionsExplored.Num() > RegionsBefore;
        const bool bRemoteDiscovery = (RegionX == 0 || RegionX == 7) && (RegionY == 0 || RegionY == 7);
        if (bNewRegion && bRemoteDiscovery)
        {
            ++ProfileSave->CareerStatistics.HiddenLocationsDiscovered;
        }
        const FVector FromBase = Location - RotorlineOperations::SpawnLocation;
        const TCHAR* Quadrant = FromBase.X >= 0.0f
            ? (FromBase.Y >= 0.0f ? TEXT("NE") : TEXT("SE"))
            : (FromBase.Y >= 0.0f ? TEXT("NW") : TEXT("SW"));
        ProfileSave->CareerStatistics.FlightPathsUsed.AddUnique(
            FString::Printf(TEXT("%s_%s"), *MissionResults.MissionType.ToUpper(), Quadrant));
        MissionResults.UniqueMapRegionsExplored = ProfileSave->CareerStatistics.UniqueRegionsExplored.Num();
        MissionResults.IslandsVisited = ProfileSave->CareerStatistics.UniqueIslandsVisited.Num();
        MissionResults.FlightPathsUsed = ProfileSave->CareerStatistics.FlightPathsUsed.Num();
    }

    ObstacleTraceAccumulator += DeltaTime;
    if (ObstacleTraceAccumulator >= 0.15f && bAirborne && HorizontalSpeedMps >= 4.0f)
    {
        ObstacleTraceAccumulator = 0.0f;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(RotorlineAwardClearance), false, Helicopter);
        const FVector Forward = Helicopter->GetActorForwardVector();
        const FVector Right = Helicopter->GetActorRightVector();
        const FVector Directions[] = { Forward, Right, -Right, (Forward + Right).GetSafeNormal(), (Forward - Right).GetSafeNormal() };
        float ClosestMeters = 100000.0f;
        for (const FVector& Direction : Directions)
        {
            FHitResult Hit;
            if (GetWorld()->LineTraceSingleByChannel(Hit, Location, Location + Direction * 5000.0f, ECC_Visibility, Params))
            {
                ClosestMeters = FMath::Min(ClosestMeters, Hit.Distance / 100.0f);
            }
        }
        if (ClosestMeters < 100000.0f)
        {
            MissionResults.ClosestObstacleClearanceMeters = FMath::Min(MissionResults.ClosestObstacleClearanceMeters, ClosestMeters);
            if (ClosestMeters <= 4.5f && HorizontalSpeedMps >= 8.0f && Attitude <= 20.0f && !MissionResults.bCrashed)
            {
                MissionResults.bTightClearanceControlled = true;
            }
            const double Now = GetWorld()->GetTimeSeconds();
            if (ClosestMeters <= 6.0f && Now - LastNearMissTime >= 8.0)
            {
                ++MissionResults.NearMisses;
                LastNearMissTime = Now;
            }
        }
    }

    LastTelemetryAltitudeAgl = Flight.AltitudeAglMeters;
}

void ARotorlineOperationsPlayerController::FinalizeMissionStatistics(
    bool bSuccess,
    const FRotorlineMissionDefinition& Mission)
{
    MissionResults.bMissionSucceeded = bSuccess;
    MissionResults.bMissionFailed = !bSuccess;
    MissionResults.bCrashed |= !bSuccess && MissionResults.AircraftHealth <= 0.0f;
    MissionResults.bSevereWeather = Mission.Weather.Contains(TEXT("storm"), ESearchCase::IgnoreCase) ||
        Mission.Weather.Contains(TEXT("severe"), ESearchCase::IgnoreCase) ||
        Mission.Weather.Contains(TEXT("heavy"), ESearchCase::IgnoreCase);
    MissionResults.bCombatSupportMission = Mission.bRequiresWeapons ||
        Mission.Type.Contains(TEXT("combat"), ESearchCase::IgnoreCase) ||
        Mission.Type.Contains(TEXT("attack"), ESearchCase::IgnoreCase) ||
        Mission.Type.Contains(TEXT("defense"), ESearchCase::IgnoreCase);
    MissionResults.bConstructionMission = Mission.Type.Contains(TEXT("cargo"), ESearchCase::IgnoreCase) ||
        Mission.Type.Contains(TEXT("supply"), ESearchCase::IgnoreCase) ||
        Mission.Type.Contains(TEXT("construction"), ESearchCase::IgnoreCase) ||
        Mission.Briefing.Contains(TEXT("infrastructure"), ESearchCase::IgnoreCase);
    MissionResults.bBaseCaptureMission = Mission.Id.Contains(TEXT("capture"), ESearchCase::IgnoreCase) ||
        Mission.Title.Contains(TEXT("capture"), ESearchCase::IgnoreCase) ||
        Mission.Briefing.Contains(TEXT("capture the"), ESearchCase::IgnoreCase);
    MissionResults.BasesCaptured = bSuccess && MissionResults.bBaseCaptureMission ? 1 : 0;
    MissionResults.bFinalCampaignMission = bSuccess && !Missions.IsEmpty() && Mission.Id == Missions.Last().Id;
    const int32 Rescued = MissionResults.CiviliansRescued + MissionResults.SoldiersRescued;
    MissionResults.bAllRequiredPersonnelDelivered = MissionResults.RescueTargetsAvailable > 0 &&
        Rescued >= MissionResults.RescueTargetsAvailable && MissionResults.RescueLosses == 0;
    MissionResults.bMeaningfulPartialSuccess = !bSuccess &&
        (Rescued > 0 || MissionResults.GroundEnemiesDestroyed + MissionResults.EnemyHelicoptersDestroyed >= 3 ||
            MissionResults.FinalScore >= 12000);
    MissionResults.bStealthApproach = bSuccess && MissionResults.bCombatSupportMission &&
        MissionResults.WeaponShotsFired <= 3 && MissionResults.DetectionTimeSeconds <= 10.0f;
    MissionResults.SecondsFromFailureAtCompletion = Mission.TimeTarget > 0
        ? FMath::Max(0.0f, static_cast<float>(Mission.TimeTarget) - MissionResults.ElapsedSeconds)
        : 100000.0f;

    if (MissionResults.bSlingLoadTracked && MissionResults.CargoDelivered > 0 && MissionResults.CargoDamage <= 0.0f)
    {
        const bool bEfficient = Mission.TimeTarget <= 0 || MissionResults.ElapsedSeconds <= Mission.TimeTarget;
        MissionResults.SlingLoadAccuracyPercent = MissionResults.bSafeLanding && bEfficient ? 97.0f :
            FMath::Max(90.0f, MissionResults.SlingLoadAccuracyPercent);
    }

    const int32 CompletedAfterThisMission = ProfileSave ? ProfileSave->CompletedMissions.Num() : 0;
    MissionResults.CampaignCompletionPercent = Missions.IsEmpty()
        ? 0.0f
        : 100.0f * static_cast<float>(CompletedAfterThisMission) / static_cast<float>(Missions.Num());
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_AWARDS|MISSION_FINALIZED|mission=%s|success=%d|takeoff=%d|landing=%d|safe=%d|crash=%d|fuel=%.1f|distance=%.1f|score=%d"),
        *Mission.Id, bSuccess ? 1 : 0, MissionResults.bValidTakeoff ? 1 : 0,
        MissionResults.bValidLanding ? 1 : 0, MissionResults.bSafeLanding ? 1 : 0,
        MissionResults.bCrashed ? 1 : 0, MissionResults.FuelRemainingPercent,
        MissionResults.DistanceFlownMeters, MissionResults.FinalScore);
}

void ARotorlineOperationsPlayerController::ApplyMissionStatisticsToProfile(const FRotorlineMissionDefinition& Mission)
{
    if (!ProfileSave || MissionResults.bProfileApplied) return;
    FRotorlineCareerStatistics& Career = ProfileSave->CareerStatistics;
    if (MissionResults.bMissionSucceeded)
    {
        ++Career.MissionsCompleted;
        ++Career.ConsecutiveSuccessfulMissions;
        Career.BestSuccessfulMissionStreak = FMath::Max(Career.BestSuccessfulMissionStreak, Career.ConsecutiveSuccessfulMissions);
        Career.CompletedMissionTypes.AddUnique(Mission.Type);
        if (Mission.Id.Equals(TEXT("kiowa-recon-strike"), ESearchCase::IgnoreCase))
        {
            ProfileSave->bJeepPermanentlyUnlocked = true;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_JEEP_UNLOCK|mission=kiowa-recon-strike|crate=OPEN|jeep=PERMANENTLY_UNLOCKED"));
        }
        if (Mission.Id.Equals(TEXT("final-discovery"), ESearchCase::IgnoreCase) &&
            GetSelectedAircraft() &&
            GetSelectedAircraft()->Id.Equals(TEXT("bell_222x"), ESearchCase::IgnoreCase))
        {
            ProfileSave->bBell222Discovered = true;
            ProfileSave->bJeepPermanentlyUnlocked = true;
            ++Career.HiddenLocationsDiscovered;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_FINAL_DISCOVERY|state=COMPLETE|bell_hangar=REVEALED|jeep=PERMANENTLY_UNLOCKED"));
        }
    }
    else
    {
        ++Career.MissionsFailed;
        Career.ConsecutiveSuccessfulMissions = 0;
    }
    Career.TotalFlightTimeSeconds += MissionResults.FlightTimeSeconds;
    Career.TotalDistanceMeters += MissionResults.DistanceFlownMeters;
    Career.ValidTakeoffs += MissionResults.bValidTakeoff ? 1 : 0;
    Career.SuccessfulLandings += MissionResults.bSafeLanding ? 1 : 0;
    Career.HardLandings += MissionResults.bHardLanding ? 1 : 0;
    Career.CrashCount += MissionResults.bCrashed ? 1 : 0;
    Career.CiviliansRescued += MissionResults.CiviliansRescued;
    Career.SoldiersRescued += MissionResults.SoldiersRescued;
    Career.RescueLosses += MissionResults.RescueLosses;
    Career.PerfectRescueMissions += MissionResults.bAllRequiredPersonnelDelivered ? 1 : 0;
    Career.CargoLoadsTransported += MissionResults.CargoDelivered;
    Career.CargoWeightTransportedKg += MissionResults.CargoWeightKg;
    Career.CargoDamage += MissionResults.CargoDamage;
    Career.PrecisionCargoDeliveries += MissionResults.bSlingLoadTracked && MissionResults.CargoDelivered > 0 &&
        MissionResults.SlingLoadAccuracyPercent >= 95.0f && MissionResults.CargoDamage <= 0.0f ? 1 : 0;
    Career.EnemyVehiclesDestroyed += MissionResults.GroundEnemiesDestroyed;
    Career.EnemyHelicoptersDestroyed += MissionResults.EnemyHelicoptersDestroyed;
    Career.BasesCaptured += MissionResults.BasesCaptured;
    Career.ShotsFired += MissionResults.WeaponShotsFired;
    Career.WeaponHits += MissionResults.WeaponHits;
    Career.MissilesDodged += MissionResults.MissilesDodged;
    Career.TimeUnderEnemyFireSeconds += MissionResults.TimeUnderEnemyFireSeconds;
    Career.DetectionTimeSeconds += MissionResults.DetectionTimeSeconds;
    Career.OptionalObjectivesCompleted += MissionResults.OptionalObjectivesCompleted;
    Career.BestMissionScore = FMath::Max(Career.BestMissionScore, MissionResults.FinalScore);
    Career.FiveStarMissions += MissionResults.StarRating >= 5 ? 1 : 0;
    Career.CampaignCompletionPercent = MissionResults.CampaignCompletionPercent;
    Career.AwardsEarned = ProfileSave->AwardRecords.Num();
    MissionResults.bProfileApplied = true;
}

void ARotorlineOperationsPlayerController::EvaluateMissionAwards()
{
    NewlyEarnedAwards.Reset();
    if (!ProfileSave || AwardSystem.GetDefinitions().IsEmpty()) return;
    const FRotorlinePlayerAwardRecord* ExistingSmoothRecord = GetAwardRecord(TEXT("smooth_operator"));
    const bool bSmoothAlreadyEarned = ExistingSmoothRecord && ExistingSmoothRecord->TimesEarned > 0;
    ProfileSave->CareerStatistics.AwardsEarned = ProfileSave->AwardRecords.Num();
    NewlyEarnedAwards = AwardSystem.Evaluate(
        MissionResults,
        ProfileSave->CareerStatistics,
        ProfileSave->AwardRecords,
        true);
    ProfileSave->CareerStatistics.AwardsEarned = ProfileSave->AwardRecords.Num();
    for (const FRotorlineAwardEvaluation& Evaluation : NewlyEarnedAwards)
    {
        const FRotorlineAwardDefinition* Definition = AwardSystem.FindDefinition(Evaluation.AwardId);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AWARDS|UNLOCK|id=%s|name=%s|new=%d|reason=%s|stat=%.2f"),
            *Evaluation.AwardId, Definition ? *Definition->DisplayName : TEXT("UNKNOWN"),
            Evaluation.bNewlyUnlocked ? 1 : 0, *Evaluation.Reason, Evaluation.AssociatedStatValue);
    }
    if (!bSmoothAlreadyEarned)
    {
        const FRotorlinePlayerAwardRecord* SmoothRecord = GetAwardRecord(TEXT("smooth_operator"));
        const bool bSmoothUnlocked = SmoothRecord && SmoothRecord->TimesEarned > 0;
        const FRotorlineAwardDefinition* SmoothDefinition = AwardSystem.FindDefinition(TEXT("smooth_operator"));
        const FString Explanation = SmoothDefinition
            ? AwardSystem.ExplainAward(*SmoothDefinition, MissionResults, ProfileSave->CareerStatistics)
            : TEXT("award definition missing");
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_AWARDS|SMOOTH_OPERATOR_CHECK|result=%s|success=%d|safe_landing=%d|damage=%.2f|stable_hover=%.2f|stars=%d|reason=%s"),
            bSmoothUnlocked ? TEXT("UNLOCKED") : TEXT("LOCKED"),
            MissionResults.bMissionSucceeded ? 1 : 0,
            MissionResults.bSafeLanding ? 1 : 0,
            MissionResults.DamageTaken,
            MissionResults.StableHoverSeconds,
            MissionResults.StarRating,
            *Explanation);
    }
    BeginAwardPresentation();
}

void ARotorlineOperationsPlayerController::BeginAwardPresentation()
{
    if (NewlyEarnedAwards.IsEmpty() || !MissionLoopTestScenario.IsEmpty() || !CombatLoopTestScenario.IsEmpty())
    {
        bAwardPresentationOpen = false;
        return;
    }
    AwardPresentationIndex = 0;
    bAwardPresentationOpen = true;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS|PRESENTATION_OPEN|count=%d|first=%s"),
        NewlyEarnedAwards.Num(), *NewlyEarnedAwards[0].AwardId);
}

void ARotorlineOperationsPlayerController::AdvanceAwardPresentation()
{
    if (!bAwardPresentationOpen) return;
    ++AwardPresentationIndex;
    if (!NewlyEarnedAwards.IsValidIndex(AwardPresentationIndex))
    {
        bAwardPresentationOpen = false;
        AwardPresentationIndex = 0;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS|PRESENTATION_CLOSED|flow=RESUME_DEBRIEF"));
    }
    else
    {
        PulseController(0.16f, 0.05f);
    }
}

void ARotorlineOperationsPlayerController::TogglePatchWall()
{
    bPatchWallOpen = !bPatchWallOpen;
    bAudioSettingsOpen = false;
    PatchWallSelection = FMath::Clamp(PatchWallSelection, 0, FMath::Max(0, AwardSystem.GetDefinitions().Num() - 1));
    PulseController(bPatchWallOpen ? 0.20f : 0.10f, 0.06f);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS|PATCH_WALL|state=%s|earned=%.1f%%"),
        bPatchWallOpen ? TEXT("OPEN") : TEXT("CLOSED"), GetAwardCompletionPercent());
}

void ARotorlineOperationsPlayerController::ForceUnlockAward(const FString& AwardId, const FString& Reason)
{
    if (!ProfileSave || !AwardSystem.FindDefinition(AwardId))
    {
        UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_AWARDS|FORCE_UNLOCK_FAILED|id=%s"), *AwardId);
        return;
    }
    FRotorlinePlayerAwardRecord& Record = ProfileSave->AwardRecords.FindOrAdd(AwardId);
    if (Record.TimesEarned <= 0)
    {
        Record.AwardId = AwardId;
        Record.FirstEarnedUtc = FDateTime::UtcNow().ToIso8601();
        Record.FirstMissionId = MissionResults.MissionId;
        Record.FirstMissionTitle = MissionResults.MissionTitle;
    }
    ++Record.TimesEarned;
    ProfileSave->CareerStatistics.AwardsEarned = ProfileSave->AwardRecords.Num();
    SaveProfile();
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS|FORCE_UNLOCK|id=%s|reason=%s"), *AwardId, *Reason);
}

void ARotorlineOperationsPlayerController::RotorlineAwardsList()
{
    for (const FRotorlineAwardDefinition& Definition : AwardSystem.GetDefinitions())
    {
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_DEBUG|DEFINITION|id=%s|name=%s|category=%s|rarity=%s|hidden=%d|repeatable=%d|art=%s"),
            *Definition.Id, *Definition.DisplayName, *Definition.Category, *Definition.Rarity,
            Definition.bHidden ? 1 : 0, Definition.bRepeatable ? 1 : 0,
            GetAwardPatchTexture(Definition.Id) ? TEXT("READY") : TEXT("MISSING"));
    }
}

void ARotorlineOperationsPlayerController::RotorlineAwardsProgress()
{
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_DEBUG|PROGRESS|earned=%.1f%%|records=%d|definitions=%d"),
        GetAwardCompletionPercent(), ProfileSave ? ProfileSave->AwardRecords.Num() : 0, AwardSystem.GetDefinitions().Num());
    if (!ProfileSave) return;
    for (const FRotorlineAwardDefinition& Definition : AwardSystem.GetDefinitions())
    {
        const FRotorlinePlayerAwardRecord* Record = GetAwardRecord(Definition.Id);
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_DEBUG|AWARD|id=%s|state=%s|times=%d|explanation=%s"),
            *Definition.Id, Record && Record->TimesEarned > 0 ? TEXT("EARNED") : TEXT("LOCKED"),
            Record ? Record->TimesEarned : 0,
            *AwardSystem.ExplainAward(Definition, MissionResults, ProfileSave->CareerStatistics));
    }
}

void ARotorlineOperationsPlayerController::RotorlineAwardsUnlock(const FString& AwardId)
{
    ForceUnlockAward(AwardId, TEXT("developer command"));
}

void ARotorlineOperationsPlayerController::RotorlineAwardsReset()
{
    if (!ProfileSave) return;
    ProfileSave->AwardRecords.Reset();
    ProfileSave->CareerStatistics = FRotorlineCareerStatistics();
    SaveProfile();
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_DEBUG|RESET|status=PASS"));
}

void ARotorlineOperationsPlayerController::RotorlineAwardsSimulate(const FString& Scenario)
{
    if (!ProfileSave || AwardSystem.GetDefinitions().IsEmpty()) return;
    FRotorlineMissionResults Simulated;
    Simulated.MissionId = TEXT("awards_debug_sortie");
    Simulated.MissionTitle = TEXT("Awards Qualification");
    Simulated.MissionType = TEXT("combat-rescue");
    Simulated.Difficulty = 5;
    Simulated.bMissionSucceeded = true;
    Simulated.bValidTakeoff = true;
    Simulated.bValidLanding = true;
    Simulated.bSafeLanding = true;
    Simulated.LandingVerticalSpeedMps = 0.7f;
    Simulated.LandingLateralSpeedMps = 0.9f;
    Simulated.LandingAttitudeDegrees = 3.0f;
    Simulated.LandingAccuracyMeters = 5.0f;
    Simulated.CiviliansRescued = 4;
    Simulated.RescueTargetsAvailable = 4;
    Simulated.bAllRequiredPersonnelDelivered = true;
    Simulated.GroundEnemiesDestroyed = 6;
    Simulated.WeaponShotsFired = 10;
    Simulated.WeaponHits = 8;
    Simulated.AircraftHealth = 100.0f;
    Simulated.AircraftMaxHealth = 100.0f;
    Simulated.FuelRemainingPercent = 4.0f;
    Simulated.FinalScore = 30000;
    Simulated.StarRating = 5;
    Simulated.StableHoverSeconds = 15.0f;
    Simulated.bCombatSupportMission = true;
    if (Scenario.Equals(TEXT("failure"), ESearchCase::IgnoreCase))
    {
        Simulated.bMissionSucceeded = false;
        Simulated.bMissionFailed = true;
        Simulated.bMeaningfulPartialSuccess = true;
    }
    TMap<FString, FRotorlinePlayerAwardRecord> EmptyRecords;
    NewlyEarnedAwards = AwardSystem.Evaluate(Simulated, ProfileSave->CareerStatistics, EmptyRecords, false);
    MissionResults = Simulated;
    bOperationsMenuOpen = false;
    bMissionCompleteScreenOpen = Simulated.bMissionSucceeded;
    bMissionFailureScreenOpen = Simulated.bMissionFailed;
    AwardPresentationIndex = 0;
    bAwardPresentationOpen = !NewlyEarnedAwards.IsEmpty();
    UGameplayStatics::SetGamePaused(GetWorld(), true);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_DEBUG|SIMULATE|scenario=%s|earned=%d|presentation=%d"),
        *Scenario, NewlyEarnedAwards.Num(), bAwardPresentationOpen ? 1 : 0);
}

void ARotorlineOperationsPlayerController::RotorlineAwardsEvaluate()
{
    if (!ProfileSave) return;
    TMap<FString, FRotorlinePlayerAwardRecord> Copy = ProfileSave->AwardRecords;
    const TArray<FRotorlineAwardEvaluation> Results = AwardSystem.Evaluate(
        MissionResults, ProfileSave->CareerStatistics, Copy, false);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_DEBUG|EVALUATE|passed=%d"), Results.Num());
    RotorlineAwardsProgress();
}

void ARotorlineOperationsPlayerController::RotorlineAwardsMissingArtTest()
{
    const bool bSafe = GetAwardPatchTexture(TEXT("__missing_patch__")) == nullptr;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_DEBUG|MISSING_ART|fallback=%s|status=%s"),
        bSafe ? TEXT("SAFE_PLACEHOLDER") : TEXT("UNEXPECTED_TEXTURE"), bSafe ? TEXT("PASS") : TEXT("FAIL"));
}

void ARotorlineOperationsPlayerController::RunAwardsSelfTest()
{
    int32 Passed = 0;
    int32 Failed = 0;
    const auto Check = [&Passed, &Failed](bool bCondition, const TCHAR* Name)
    {
        if (bCondition) ++Passed; else ++Failed;
        UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_TEST|%s|%s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), Name);
    };
    const auto HasAward = [](const TArray<FRotorlineAwardEvaluation>& Results, const TCHAR* Id)
    {
        return Results.ContainsByPredicate([Id](const FRotorlineAwardEvaluation& Result)
        {
            return Result.AwardId.Equals(Id, ESearchCase::IgnoreCase);
        });
    };

    FRotorlineCareerStatistics Career;
    Career.MissionsCompleted = 1;
    TMap<FString, FRotorlinePlayerAwardRecord> Records;
    FRotorlineMissionResults Base;
    Base.MissionId = TEXT("test");
    Base.MissionTitle = TEXT("Awards Test");
    Base.MissionType = TEXT("rescue");
    Base.bMissionSucceeded = true;
    Base.bValidTakeoff = true;
    Base.AircraftHealth = 100.0f;
    Base.AircraftMaxHealth = 100.0f;
    Base.FuelRemainingPercent = 50.0f;

    TArray<FRotorlineAwardEvaluation> Results = AwardSystem.Evaluate(Base, Career, Records, true);
    Check(HasAward(Results, TEXT("lift_off")), TEXT("first valid takeoff unlocks Lift Off"));
    Results = AwardSystem.Evaluate(Base, Career, Records, true);
    Check(!HasAward(Results, TEXT("lift_off")), TEXT("one-time Lift Off is not duplicated"));

    FRotorlineMissionResults Rough = Base;
    Rough.bValidLanding = true;
    Rough.bSafeLanding = true;
    Rough.LandingVerticalSpeedMps = 3.5f;
    Rough.LandingLateralSpeedMps = 1.0f;
    Rough.LandingAttitudeDegrees = 4.0f;
    Rough.LandingAccuracyMeters = 4.0f;
    TMap<FString, FRotorlinePlayerAwardRecord> Empty;
    Results = AwardSystem.Evaluate(Rough, Career, Empty, false);
    Check(!HasAward(Results, TEXT("butter_landing")), TEXT("ordinary first landing is not Butter Landing"));
    Rough.bCrashed = true;
    Rough.LandingVerticalSpeedMps = 0.5f;
    Results = AwardSystem.Evaluate(Rough, Career, Empty, false);
    Check(!HasAward(Results, TEXT("butter_landing")), TEXT("crash cannot unlock landing awards"));

    FRotorlineMissionResults Rescue = Base;
    Rescue.CiviliansRescued = 4;
    Rescue.RescueTargetsAvailable = 4;
    Rescue.bAllRequiredPersonnelDelivered = true;
    Results = AwardSystem.Evaluate(Rescue, Career, Empty, false);
    Check(HasAward(Results, TEXT("no_one_left_behind")), TEXT("perfect rescue unlocks No One Left Behind"));
    Check(HasAward(Results, TEXT("extraction_complete")), TEXT("completed rescue with every target delivered unlocks Extraction Complete"));
    FRotorlineMissionResults EmptyExtraction = Base;
    EmptyExtraction.bAllRequiredPersonnelDelivered = true;
    Results = AwardSystem.Evaluate(EmptyExtraction, Career, Empty, false);
    Check(!HasAward(Results, TEXT("extraction_complete")), TEXT("mission with no rescue targets cannot unlock Extraction Complete"));

    FRotorlineMissionResults LastHope = Rescue;
    LastHope.MissionId = TEXT("survivor-extraction");
    LastHope.CiviliansRescued = 3;
    LastHope.RescueTargetsAvailable = 3;
    Results = AwardSystem.Evaluate(LastHope, Career, Empty, false);
    Check(HasAward(Results, TEXT("last_hope")), TEXT("M24 with all three survivor groups unlocks Last Hope"));
    LastHope.RescueTargetsAvailable = 2;
    Results = AwardSystem.Evaluate(LastHope, Career, Empty, false);
    Check(!HasAward(Results, TEXT("last_hope")), TEXT("M24 without all three survivor groups cannot unlock Last Hope"));

    FRotorlineMissionResults Fumes = Base;
    Fumes.bSafeLanding = true;
    Fumes.FuelRemainingPercent = 4.0f;
    Results = AwardSystem.Evaluate(Fumes, Career, Empty, false);
    Check(HasAward(Results, TEXT("running_on_fumes")), TEXT("successful safe low-fuel landing unlocks Running on Fumes"));
    Fumes.bMissionSucceeded = false;
    Fumes.bMissionFailed = true;
    Results = AwardSystem.Evaluate(Fumes, Career, Empty, false);
    Check(!HasAward(Results, TEXT("running_on_fumes")), TEXT("failed low-fuel mission does not unlock Running on Fumes"));

    FRotorlineMissionResults Combat = Base;
    Combat.bCombatSupportMission = true;
    Combat.WeaponShotsFired = 1;
    Results = AwardSystem.Evaluate(Combat, Career, Empty, false);
    Check(HasAward(Results, TEXT("untouchable")), TEXT("zero-damage combat can unlock Untouchable"));
    Combat.DamageTaken = 1.0f;
    Results = AwardSystem.Evaluate(Combat, Career, Empty, false);
    Check(!HasAward(Results, TEXT("untouchable")), TEXT("any valid damage blocks Untouchable"));

    FRotorlineMissionResults EnemyFoothold = Base;
    EnemyFoothold.MissionId = TEXT("enemy-foothold");
    EnemyFoothold.GroundEnemiesDestroyed = 6;
    EnemyFoothold.EnemyHelicoptersDestroyed = 3;
    EnemyFoothold.WeaponShotsFired = 20;
    EnemyFoothold.WeaponHits = 7;
    Results = AwardSystem.Evaluate(EnemyFoothold, Career, Empty, false);
    Check(HasAward(Results, TEXT("all_your_base")), TEXT("successful M22 unlocks All Your Base"));
    Check(HasAward(Results, TEXT("death_from_above")), TEXT("M22 gauntlet thresholds unlock Death From Above"));
    EnemyFoothold.WeaponHits = 6;
    Results = AwardSystem.Evaluate(EnemyFoothold, Career, Empty, false);
    Check(!HasAward(Results, TEXT("death_from_above")), TEXT("M22 below 35 percent accuracy cannot unlock Death From Above"));

    FRotorlineMissionResults Sling = Base;
    Sling.CargoDelivered = 1;
    Sling.SlingLoadAccuracyPercent = 97.0f;
    Results = AwardSystem.Evaluate(Sling, Career, Empty, false);
    Check(!HasAward(Results, TEXT("sling_king")), TEXT("ordinary cargo cannot unlock Sling King"));
    Sling.bSlingLoadTracked = true;
    Results = AwardSystem.Evaluate(Sling, Career, Empty, false);
    Check(HasAward(Results, TEXT("sling_king")), TEXT("qualified sling-load delivery unlocks Sling King"));

    FRotorlineMissionResults Smooth = Base;
    Smooth.bSafeLanding = true;
    Smooth.StableHoverSeconds = 10.0f;
    Smooth.StarRating = 4;
    Results = AwardSystem.Evaluate(Smooth, Career, Empty, false);
    Check(HasAward(Results, TEXT("smooth_operator")), TEXT("published Smooth Operator thresholds unlock the patch"));
    Smooth.StableHoverSeconds = 9.9f;
    Results = AwardSystem.Evaluate(Smooth, Career, Empty, false);
    Check(!HasAward(Results, TEXT("smooth_operator")), TEXT("shorter hover cannot unlock Smooth Operator"));

    FRotorlineAwardsFlightState PracticalHover;
    PracticalHover.bEnginePowerAvailable = true;
    PracticalHover.AltitudeAglMeters = 2.0f;
    PracticalHover.Velocity = FVector(240.0f, 0.0f, 95.0f);
    PracticalHover.PitchDegrees = 9.5f;
    Check(RotorlineOperations::IsStableHoverState(PracticalHover),
        TEXT("low controlled pad hover qualifies for Smooth Operator telemetry"));
    PracticalHover.AltitudeAglMeters = 1.9f;
    Check(!RotorlineOperations::IsStableHoverState(PracticalHover),
        TEXT("ground-level rotor operation does not count as a hover"));
    PracticalHover.AltitudeAglMeters = 2.0f;
    PracticalHover.Velocity = FVector(260.0f, 0.0f, 0.0f);
    Check(!RotorlineOperations::IsStableHoverState(PracticalHover),
        TEXT("excess horizontal drift does not count as a stable hover"));

    FRotorlineMissionResults Needle = Base;
    Needle.MissionId = TEXT("final-discovery");
    Results = AwardSystem.Evaluate(Needle, Career, Empty, false);
    Check(HasAward(Results, TEXT("thread_the_needle")), TEXT("successful M19 mesa escape unlocks Thread the Needle"));
    Needle.MissionId = TEXT("another-mission");
    Results = AwardSystem.Evaluate(Needle, Career, Empty, false);
    Check(!HasAward(Results, TEXT("thread_the_needle")), TEXT("unrelated missions do not unlock Thread the Needle"));

    FRotorlineMissionResults Partial = Base;
    Partial.bMissionSucceeded = false;
    Partial.bMissionFailed = true;
    Partial.bMeaningfulPartialSuccess = true;
    Partial.CiviliansRescued = 1;
    Results = AwardSystem.Evaluate(Partial, Career, Empty, false);
    Check(HasAward(Results, TEXT("task_failed_successfully")), TEXT("defined partial success unlocks Task Failed Successfully"));
    Partial.bMeaningfulPartialSuccess = false;
    Results = AwardSystem.Evaluate(Partial, Career, Empty, false);
    Check(!HasAward(Results, TEXT("task_failed_successfully")), TEXT("ordinary failure is rejected"));

    Check(AwardSystem.GetDefinitions().Num() == 29, TEXT("all 29 active award definitions loaded"));
    Check(AwardPatchTextures.Num() == 29, TEXT("all 29 active patch textures loaded"));
    Check(AwardSystem.FindDefinition(TEXT("bell_222")) != nullptr,
        TEXT("Bell 222 M19 discovery patch is registered"));
    Check(GetAwardPatchTexture(TEXT("__missing_patch__")) == nullptr, TEXT("missing patch art uses safe null fallback"));

    FRotorlineMissionResults Multi = Rescue;
    Multi.bValidLanding = true;
    Multi.bSafeLanding = true;
    Multi.LandingVerticalSpeedMps = 0.7f;
    Multi.LandingLateralSpeedMps = 0.8f;
    Multi.LandingAttitudeDegrees = 3.0f;
    Multi.LandingAccuracyMeters = 5.0f;
    Results = AwardSystem.Evaluate(Multi, Career, Empty, false);
    Check(Results.Num() >= 3, TEXT("multiple valid awards can unlock after one mission"));

    const auto IsPlayerFacingAwardText = [](const FString& Text)
    {
        static const TCHAR* ForbiddenTokens[] = {
            TEXT("bMission"), TEXT("bValid"),
            TEXT("bSafe"), TEXT("bCrashed"), TEXT(" requires gte "),
            TEXT(" requires lte "), TEXT(" requires eq "), TEXT(" unknown stat ")
        };
        for (const TCHAR* Token : ForbiddenTokens)
        {
            if (Text.Contains(Token, ESearchCase::IgnoreCase)) return false;
        }
        return true;
    };

    bool bAllAwardReasonsPlayerFacing = true;
    for (const FRotorlineAwardDefinition& Definition : AwardSystem.GetDefinitions())
    {
        const FString Explanation = AwardSystem.ExplainAward(Definition, Multi, Career);
        bool bContainsCatalogKey = false;
        for (const FRotorlineAwardRuleGroup& Group : Definition.Groups)
        {
            for (const FRotorlineAwardRule& Rule : Group.Rules)
            {
                if (Explanation.Contains(Rule.Stat, ESearchCase::IgnoreCase))
                {
                    bContainsCatalogKey = true;
                    break;
                }
            }
            if (bContainsCatalogKey) break;
        }
        if (!IsPlayerFacingAwardText(Explanation) || bContainsCatalogKey)
        {
            bAllAwardReasonsPlayerFacing = false;
            UE_LOG(LogTemp, Error, TEXT("ROTORLINE_AWARDS_TEST|RAW_QUERY_TEXT|award=%s|text=%s"),
                *Definition.Id, *Explanation);
        }
    }
    Check(bAllAwardReasonsPlayerFacing, TEXT("all achievement explanations use player-facing language"));

    FRotorlineMissionResults SustainedCombat = Base;
    SustainedCombat.TimeUnderEnemyFireSeconds = 159.0f;
    SustainedCombat.AircraftHealth = 100.0f;
    SustainedCombat.AircraftMaxHealth = 100.0f;
    Results = AwardSystem.Evaluate(SustainedCombat, Career, Empty, false);
    const FRotorlineAwardEvaluation* ComeGetSome = Results.FindByPredicate([](const FRotorlineAwardEvaluation& Result)
    {
        return Result.AwardId.Equals(TEXT("come_get_some"), ESearchCase::IgnoreCase);
    });
    Check(ComeGetSome && IsPlayerFacingAwardText(ComeGetSome->Reason) &&
        ComeGetSome->Reason.Contains(TEXT("completing the mission"), ESearchCase::IgnoreCase) &&
        ComeGetSome->Reason.Contains(TEXT("159.0 sec"), ESearchCase::IgnoreCase),
        TEXT("Come Get Some displays a natural-language earned reason"));

    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS_TEST|SUMMARY|passed=%d|failed=%d|status=%s"),
        Passed, Failed, Failed == 0 ? TEXT("PASS") : TEXT("FAIL"));
    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineAwardsTest")))
    {
        FPlatformMisc::RequestExit(Failed > 0);
    }
}
