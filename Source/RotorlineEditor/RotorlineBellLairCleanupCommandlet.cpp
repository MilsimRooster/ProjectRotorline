#include "RotorlineBellLairCleanupCommandlet.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "LandscapeProxy.h"
#include "Misc/PackageName.h"
#include "../Rotorline/RotorlineBellLairActor.h"
#include "../Rotorline/RotorlineSupportLocations.h"
#include "WorldPartition/ActorDescContainerInstance.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"

namespace RotorlineBellLairCleanup
{
    const TCHAR* MapAsset = TEXT("/Game/Maps/RotorlineIsland");
    const FVector2D TargetLocation(54800.0, 185200.0);
}

URotorlineBellLairCleanupCommandlet::URotorlineBellLairCleanupCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 URotorlineBellLairCleanupCommandlet::Main(const FString& Params)
{
    using namespace RotorlineBellLairCleanup;

    const FString MapFilename = FPackageName::LongPackageNameToFilename(
        MapAsset, FPackageName::GetMapPackageExtension());
    if (!FEditorFileUtils::LoadMap(MapFilename, false, false) || !GEditor)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_LAIR_CLEANUP|FAIL|reason=load_map|map=%s"), MapAsset);
        return 1;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_LAIR_CLEANUP|FAIL|reason=no_world"));
        return 1;
    }

    TArray<FWorldPartitionReference> LoadedActorReferences;
    if (UWorldPartition* WorldPartition = World->GetWorldPartition())
    {
        WorldPartition->LoadAllActors(LoadedActorReferences);
    }

    TArray<ARotorlineBellLairActor*> Lairs;
    for (TActorIterator<ARotorlineBellLairActor> It(World); It; ++It)
    {
        Lairs.Add(*It);
    }
    Lairs.Sort([](const ARotorlineBellLairActor& Left, const ARotorlineBellLairActor& Right)
    {
        const FVector LeftLocation = Left.GetActorLocation();
        const FVector RightLocation = Right.GetActorLocation();
        return FVector2D::DistSquared(FVector2D(LeftLocation.X, LeftLocation.Y), TargetLocation) <
            FVector2D::DistSquared(FVector2D(RightLocation.X, RightLocation.Y), TargetLocation);
    });

    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_LAIR_CLEANUP|FOUND|map=%s|count=%d"), MapAsset, Lairs.Num());
    if (Lairs.IsEmpty())
    {
        FVector SummitLocation = RotorlineSupportLocations::BellLairPeak;
        TArray<FHitResult> SummitHits;
        FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RotorlineBellLairEditorPreview), true);
        const FVector TraceStart(SummitLocation.X, SummitLocation.Y, 120000.0f);
        const FVector TraceEnd(SummitLocation.X, SummitLocation.Y, -50000.0f);
        if (World->LineTraceMultiByChannel(SummitHits, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
        {
            for (const FHitResult& Hit : SummitHits)
            {
                if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
                {
                    SummitLocation.Z = Hit.ImpactPoint.Z;
                    break;
                }
            }
        }

        const FVector PreviewFloorLocation(
            SummitLocation.X,
            SummitLocation.Y,
            SummitLocation.Z - RotorlineSupportLocations::BellLairBurialDepthCm);
        const FRotator PreviewRotation(
            0.0f, RotorlineSupportLocations::BellLairYawDegrees, 0.0f);
        ARotorlineBellLairActor* Preview = World->SpawnActor<ARotorlineBellLairActor>(
            ARotorlineBellLairActor::StaticClass(), PreviewFloorLocation, PreviewRotation);
        if (!Preview)
        {
            UE_LOG(LogTemp, Error, TEXT("ROTORLINE_LAIR_CLEANUP|FAIL|reason=create_preview"));
            return 1;
        }
        Lairs.Add(Preview);
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_LAIR_CLEANUP|CREATE_PREVIEW|location=%s|summit_z=%.0f"),
            *PreviewFloorLocation.ToCompactString(), SummitLocation.Z);
    }

    ARotorlineBellLairActor* Survivor = Lairs[0];
    Survivor->Modify();
    Survivor->SetIsTemporarilyHiddenInEditor(false);
    Survivor->bIsEditorOnlyActor = true;
    if (Survivor->CanChangeIsSpatiallyLoadedFlag())
    {
        Survivor->SetIsSpatiallyLoaded(false);
    }
    Survivor->SetActorLabel(TEXT("Bell Lair - Editor Preview"));
    Survivor->MarkPackageDirty();

    int32 RemovedCount = 0;
    for (int32 Index = 1; Index < Lairs.Num(); ++Index)
    {
        ARotorlineBellLairActor* Duplicate = Lairs[Index];
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_LAIR_CLEANUP|DELETE_DUPLICATE|actor=%s|location=%s"),
            *Duplicate->GetPathName(), *Duplicate->GetActorLocation().ToCompactString());
        if (World->EditorDestroyActor(Duplicate, true))
        {
            ++RemovedCount;
        }
    }

    const bool bMapSaved = FEditorFileUtils::SaveMap(World, MapFilename);
    const bool bPackagesSaved = FEditorFileUtils::SaveDirtyPackages(
        false, true, true, false, false, false);
    const bool bSaved = bMapSaved || bPackagesSaved;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_LAIR_CLEANUP|SAVED|map=%s|survivor=%s|location=%s|editor_only=1|spatially_loaded=%d|duplicates_removed=%d|map_save=%s|package_save=%s"),
        MapAsset, *Survivor->GetPathName(), *Survivor->GetActorLocation().ToCompactString(),
        Survivor->GetIsSpatiallyLoaded() ? 1 : 0, RemovedCount,
        bMapSaved ? TEXT("PASS") : TEXT("FAIL"),
        bPackagesSaved ? TEXT("PASS") : TEXT("FAIL"));
    return bSaved ? 0 : 1;
}
