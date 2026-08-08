#include "RotorlineWorldBuildCommandlet.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Factories/WorldFactory.h"
#include "HAL/FileManager.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Engine/Level.h"
#include "GameFramework/WorldSettings.h"

namespace RotorlineWorldBuild
{
    constexpr int32 Resolution = 4033;
    constexpr int32 SectionsPerComponent = 2;
    constexpr int32 QuadsPerSection = 63;
    constexpr double XYScaleCentimeters = 200.0;
    constexpr double ZScale = 300.0;
    constexpr double HalfWorldCentimeters = (Resolution - 1) * XYScaleCentimeters * 0.5;

    struct FSite
    {
        const TCHAR* Name;
        double X;
        double Y;
        double ZMeters;
    };

    const FSite Sites[] =
    {
        { TEXT("District_Air_Operations_Base"), -0.58, -0.52, 28.0 },
        { TEXT("District_Harbor_City"), 0.54, -0.38, 22.0 },
        { TEXT("District_Central_Valley"), -0.02, -0.02, 74.0 },
        { TEXT("District_Northern_Ridge"), -0.10, 0.52, 420.0 },
        { TEXT("District_Western_Wild_Coast"), -0.70, 0.05, 160.0 },
        { TEXT("District_Eastern_Plateau"), 0.48, 0.24, 215.0 },
    };

    template <typename TActor>
    TActor* SpawnNamed(UWorld* World, const TCHAR* Label, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator)
    {
        FActorSpawnParameters Parameters;
        Parameters.Name = MakeUniqueObjectName(World->PersistentLevel, TActor::StaticClass(), FName(Label));
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        TActor* Actor = World->SpawnActor<TActor>(TActor::StaticClass(), Location, Rotation, Parameters);
        if (Actor != nullptr)
        {
            Actor->SetActorLabel(Label);
        }
        return Actor;
    }
}

URotorlineWorldBuildCommandlet::URotorlineWorldBuildCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 URotorlineWorldBuildCommandlet::Main(const FString& Params)
{
    using namespace RotorlineWorldBuild;

    FString HeightmapPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt/Heightmaps/Rotorline_Island_4033.r16")));
    FParse::Value(*Params, TEXT("Heightmap="), HeightmapPath);

    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *HeightmapPath))
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_WORLD_ERROR=Unable to read heightmap: %s"), *HeightmapPath);
        return 1;
    }

    const int64 ExpectedBytes = static_cast<int64>(Resolution) * Resolution * sizeof(uint16);
    if (RawBytes.Num() != ExpectedBytes)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_WORLD_ERROR=Heightmap is %d bytes; expected %lld"), RawBytes.Num(), ExpectedBytes);
        return 1;
    }

    TArray<uint16> HeightData;
    HeightData.SetNumUninitialized(Resolution * Resolution);
    FMemory::Memcpy(HeightData.GetData(), RawBytes.GetData(), RawBytes.Num());

    const FString PackageName = TEXT("/Game/Maps/RotorlineIsland");
    const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
    IFileManager::Get().Delete(*PackageFilename, false, true, true);

    UPackage* Package = CreatePackage(*PackageName);
    Package->SetPackageFlags(PKG_NewlyCreated);

    UWorldFactory* Factory = NewObject<UWorldFactory>();
    Factory->WorldType = EWorldType::Editor;
    Factory->bCreateWorldPartition = false;
    Factory->bInformEngineOfWorld = false;

    UWorld* World = Cast<UWorld>(Factory->FactoryCreateNew(
        UWorld::StaticClass(), Package, FName(TEXT("RotorlineIsland")), RF_Public | RF_Standalone, nullptr, GWarn));
    if (World == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_WORLD_ERROR=Unable to create world package"));
        return 1;
    }

    World->AddToRoot();
    World->UpdateWorldComponents(true, true);
    if (AWorldSettings* Settings = World->GetWorldSettings())
    {
        Settings->WorldGravityZ = -980.0f;
        Settings->bEnableWorldBoundsChecks = true;
        Settings->KillZ = -25000.0f;
        Settings->SetActorLabel(TEXT("WorldSettings_RotorlineIsland"));
    }

    const FVector LandscapeLocation(-HalfWorldCentimeters, -HalfWorldCentimeters, 0.0);
    ALandscape* Landscape = SpawnNamed<ALandscape>(World, TEXT("Landscape_RotorlineIsland_8km"), LandscapeLocation);
    if (Landscape == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_WORLD_ERROR=Unable to spawn landscape"));
        return 1;
    }

    Landscape->SetActorScale3D(FVector(XYScaleCentimeters, XYScaleCentimeters, ZScale));
    Landscape->StaticLightingLOD = 2;

    TMap<FGuid, TArray<uint16>> HeightDataByLayer;
    HeightDataByLayer.Add(FGuid(), MoveTemp(HeightData));
    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayersByLayer;
    MaterialLayersByLayer.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

    Landscape->Import(
        FGuid::NewGuid(),
        0,
        0,
        Resolution - 1,
        Resolution - 1,
        SectionsPerComponent,
        QuadsPerSection,
        HeightDataByLayer,
        *HeightmapPath,
        MaterialLayersByLayer,
        ELandscapeImportAlphamapType::Additive,
        TArrayView<const FLandscapeLayer>());

    Landscape->SetActorEnableCollision(true);
    Landscape->bUseDynamicMaterialInstance = true;
    if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
    {
        LandscapeInfo->UpdateLayerInfoMap(Landscape);
    }

    ADirectionalLight* Sun = SpawnNamed<ADirectionalLight>(
        World, TEXT("Sun_Directional"), FVector::ZeroVector, FRotator(-38.0, -32.0, 0.0));
    if (Sun != nullptr)
    {
        Sun->GetLightComponent()->SetIntensity(7.5f);
        Sun->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.92f, 0.78f));
        Sun->GetLightComponent()->SetCastShadows(true);
        Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sun->GetComponent()->SetAtmosphereSunLight(true);
    }

    ASkyLight* Sky = SpawnNamed<ASkyLight>(World, TEXT("Sky_RealTime"), FVector::ZeroVector);
    if (Sky != nullptr)
    {
        Sky->GetLightComponent()->SetIntensity(0.85f);
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetRealTimeCaptureEnabled(true);
    }

    ASkyAtmosphere* Atmosphere = SpawnNamed<ASkyAtmosphere>(World, TEXT("SkyAtmosphere"), FVector::ZeroVector);
    AExponentialHeightFog* Fog = SpawnNamed<AExponentialHeightFog>(World, TEXT("HeightFog_Coastal"), FVector(0.0, 0.0, 3000.0));
    if (Fog != nullptr)
    {
        Fog->GetComponent()->SetFogDensity(0.012f);
        Fog->GetComponent()->SetFogHeightFalloff(0.16f);
        Fog->GetComponent()->SetVolumetricFog(true);
        Fog->GetComponent()->SetVolumetricFogExtinctionScale(0.75f);
        Fog->GetComponent()->SetVolumetricFogDistance(160000.0f);
    }

    AVolumetricCloud* Clouds = SpawnNamed<AVolumetricCloud>(World, TEXT("VolumetricClouds"), FVector(0.0, 0.0, 180000.0));
    APostProcessVolume* PostProcess = SpawnNamed<APostProcessVolume>(World, TEXT("PostProcess_Global"), FVector::ZeroVector);
    if (PostProcess != nullptr)
    {
        PostProcess->bUnbound = true;
        PostProcess->Priority = -10.0f;
    }

    for (const FSite& Site : Sites)
    {
        const FVector Location(
            Site.X * HalfWorldCentimeters,
            Site.Y * HalfWorldCentimeters,
            Site.ZMeters * 100.0 + 1000.0);
        SpawnNamed<ATargetPoint>(World, Site.Name, Location);
    }

    World->UpdateWorldComponents(true, true);
    World->MarkPackageDirty();
    World->PersistentLevel->MarkPackageDirty();
    Package->MarkPackageDirty();

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    SaveArgs.Error = GError;

    if (!UPackage::SavePackage(Package, World, *PackageFilename, SaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_WORLD_ERROR=Failed to save %s"), *PackageFilename);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_WORLD_CREATED=%s"), *PackageFilename);
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_WORLD_SIZE_KM=8.064"));
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_LANDSCAPE_COMPONENTS=32x32"));
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_LANDSCAPE_RESOLUTION=4033x4033"));
    World->CleanupWorld(true, true, nullptr);
    World->RemoveFromRoot();
    return 0;
}
