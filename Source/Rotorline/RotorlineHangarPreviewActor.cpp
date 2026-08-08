#include "RotorlineHangarPreviewActor.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineHangar
{
    constexpr float TargetAircraftSpanCm = 980.0f;
    constexpr float MinimumAcceptedSpanCm = 620.0f;
    constexpr float MaximumAcceptedSpanCm = 1380.0f;
    constexpr int32 MaximumPreviewMeshParts = 220;

    bool IsRotorPath(const FString& Path)
    {
        const FString Lower = Path.ToLower();
        return Lower.Contains(TEXT("rotor"))
            || Lower.Contains(TEXT("blade"))
            || Lower.Contains(TEXT("prop"));
    }

    bool IsTailRotorPath(const FString& Path)
    {
        const FString Lower = Path.ToLower();
        return Lower.Contains(TEXT("tail"))
            || Lower.Contains(TEXT("backrotor"))
            || Lower.Contains(TEXT("back_rotor"))
            || Lower.Contains(TEXT("backprop"));
    }

    FString ModelRootFromObjectPath(const FString& ObjectPath)
    {
        const int32 StaticIndex = ObjectPath.Find(TEXT("/StaticMeshes/"));
        const int32 SkeletalIndex = ObjectPath.Find(TEXT("/SkeletalMeshes/"));
        int32 RootEnd = INDEX_NONE;
        if (StaticIndex != INDEX_NONE)
        {
            RootEnd = StaticIndex;
        }
        if (SkeletalIndex != INDEX_NONE && (RootEnd == INDEX_NONE || SkeletalIndex < RootEnd))
        {
            RootEnd = SkeletalIndex;
        }
        return RootEnd == INDEX_NONE ? FString() : ObjectPath.Left(RootEnd);
    }
}

ARotorlineHangarPreviewActor::ARotorlineHangarPreviewActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    ShowcaseRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShowcaseRoot"));
    ShowcaseRoot->SetupAttachment(SceneRoot);

    PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
    PreviewCamera->SetupAttachment(SceneRoot);
    PreviewCamera->bAutoActivate = true;
    const FVector CameraLocation(-1650.0f, 1050.0f, 430.0f);
    const FVector CameraTarget(0.0f, 0.0f, 180.0f);
    PreviewCamera->SetRelativeLocation(CameraLocation);
    PreviewCamera->SetRelativeRotation((CameraTarget - CameraLocation).Rotation());
    PreviewCamera->SetFieldOfView(48.0f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SteelFinder(TEXT("/Game/Environment/Materials/Blockout/M_Hangar_BlueSteel.M_Hangar_BlueSteel"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ConcreteFinder(TEXT("/Game/Environment/Materials/Blockout/M_Concrete.M_Concrete"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarkingFinder(TEXT("/Game/Environment/Materials/Blockout/M_Marking_Yellow.M_Marking_Yellow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteMarkingFinder(TEXT("/Game/Environment/Materials/Blockout/M_Marking_White.M_Marking_White"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RotorBlurFinder(TEXT("/Game/Environment/Materials/Blockout/M_Glass_Blockout.M_Glass_Blockout"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ApronFinder(TEXT("/Game/Environment/Materials/Urban/M_Airfield_Apron_Final.M_Airfield_Apron_Final"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalFinder(TEXT("/Game/Environment/Materials/Urban/M_Urban_Metal.M_Urban_Metal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrimFinder(TEXT("/Game/Environment/Materials/Urban/M_Urban_Trim.M_Urban_Trim"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> LitPanelFinder(TEXT("/Game/Environment/Materials/Urban/M_Urban_Window_Lit.M_Urban_Window_Lit"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ClassifiedCrateFinder(
        TEXT("/Game/Environment/Imported/Hangar/ClassifiedAirframeCrate/rotorline_airframe_shipping_crate/StaticMeshes/SM_Rotorline_AirframeShippingCrate.SM_Rotorline_AirframeShippingCrate"));

    ClassifiedAirframeCrateMesh = ClassifiedCrateFinder.Succeeded()
        ? ClassifiedCrateFinder.Object
        : nullptr;

    const auto AddStageMesh = [this](
        const FName Name,
        UStaticMesh* Mesh,
        UMaterialInterface* Material,
        const FVector& Location,
        const FVector& Scale)
    {
        UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Component->SetupAttachment(SceneRoot);
        Component->SetStaticMesh(Mesh);
        Component->SetRelativeLocation(Location);
        Component->SetRelativeScale3D(Scale);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(true);
        if (Material)
        {
            Component->SetMaterial(0, Material);
        }
    };

    if (CubeFinder.Succeeded())
    {
        UMaterialInterface* Steel = SteelFinder.Succeeded() ? SteelFinder.Object : nullptr;
        UMaterialInterface* Concrete = ConcreteFinder.Succeeded() ? ConcreteFinder.Object : nullptr;
        UMaterialInterface* Apron = ApronFinder.Succeeded() ? ApronFinder.Object.Get() : Concrete;
        UMaterialInterface* Metal = MetalFinder.Succeeded() ? MetalFinder.Object.Get() : Steel;
        UMaterialInterface* Trim = TrimFinder.Succeeded() ? TrimFinder.Object.Get() : Metal;
        UMaterialInterface* LitPanel = LitPanelFinder.Succeeded() ? LitPanelFinder.Object.Get() : RotorBlurFinder.Object.Get();
        UMaterialInterface* WhiteMarking = WhiteMarkingFinder.Succeeded() ? WhiteMarkingFinder.Object.Get() : Concrete;
        AddStageMesh(TEXT("HangarFloor"), CubeFinder.Object, Apron, FVector(0.0f, 0.0f, -55.0f), FVector(58.0f, 44.0f, 0.55f));
        // BasicShapes/Cube is 100 cm tall. The old half-height shell left a
        // large gap above the floor and made the walls read as floating panels.
        // Fill the full apron-to-ceiling volume so the preview is an enclosed
        // hangar rather than a skybox with suspended wall sections.
        AddStageMesh(TEXT("HangarBackWall"), CubeFinder.Object, Steel, FVector(2500.0f, 0.0f, 1500.0f), FVector(0.35f, 44.0f, 30.0f));
        AddStageMesh(TEXT("HangarLeftWall"), CubeFinder.Object, Steel, FVector(0.0f, -2200.0f, 1500.0f), FVector(58.0f, 0.35f, 30.0f));
        AddStageMesh(TEXT("HangarRightWall"), CubeFinder.Object, Steel, FVector(0.0f, 2200.0f, 1500.0f), FVector(58.0f, 0.35f, 30.0f));
        AddStageMesh(TEXT("HangarCeiling"), CubeFinder.Object, Metal, FVector(0.0f, 0.0f, 3000.0f), FVector(58.0f, 44.0f, 0.35f));
        AddStageMesh(TEXT("HangarRoofBeamA"), CubeFinder.Object, Trim, FVector(-1450.0f, 0.0f, 2820.0f), FVector(0.28f, 44.0f, 0.28f));
        AddStageMesh(TEXT("HangarRoofBeamB"), CubeFinder.Object, Trim, FVector(250.0f, 0.0f, 2820.0f), FVector(0.28f, 44.0f, 0.28f));
        AddStageMesh(TEXT("HangarRoofBeamC"), CubeFinder.Object, Trim, FVector(1950.0f, 0.0f, 2820.0f), FVector(0.28f, 44.0f, 0.28f));

        // Break up the empty blockout shell with an illuminated maintenance
        // wall, structural columns, floor guidance, and service-bay details.
        AddStageMesh(TEXT("MaintenanceWallPanelLeft"), CubeFinder.Object, LitPanel,
            FVector(2450.0f, -1040.0f, 1500.0f), FVector(0.12f, 7.2f, 6.3f));
        AddStageMesh(TEXT("MaintenanceWallPanelRight"), CubeFinder.Object, LitPanel,
            FVector(2450.0f, 1040.0f, 1500.0f), FVector(0.12f, 7.2f, 6.3f));
        for (int32 ColumnIndex = -2; ColumnIndex <= 2; ++ColumnIndex)
        {
            AddStageMesh(
                FName(*FString::Printf(TEXT("MaintenanceColumn%d"), ColumnIndex + 2)),
                CubeFinder.Object,
                Trim,
                FVector(2380.0f, ColumnIndex * 880.0f, 1500.0f),
                FVector(0.48f, 0.34f, 30.0f));
        }
        AddStageMesh(TEXT("FloorGuideLeft"), CubeFinder.Object, WhiteMarking,
            FVector(40.0f, -1030.0f, 3.0f), FVector(30.0f, 0.055f, 0.025f));
        AddStageMesh(TEXT("FloorGuideRight"), CubeFinder.Object, WhiteMarking,
            FVector(40.0f, 1030.0f, 3.0f), FVector(30.0f, 0.055f, 0.025f));
        AddStageMesh(TEXT("FloorSafetyStripeLeft"), CubeFinder.Object, MarkingFinder.Object,
            FVector(850.0f, -1460.0f, 4.0f), FVector(13.0f, 0.07f, 0.025f));
        AddStageMesh(TEXT("FloorSafetyStripeRight"), CubeFinder.Object, MarkingFinder.Object,
            FVector(850.0f, 1460.0f, 4.0f), FVector(13.0f, 0.07f, 0.025f));
    }

    if (CylinderFinder.Succeeded())
    {
        FallbackBladeMesh = CylinderFinder.Object;
        FallbackRotorMaterial = RotorBlurFinder.Succeeded() ? RotorBlurFinder.Object : nullptr;
        UStaticMesh* CylinderMesh = CylinderFinder.Object.Get();
        const auto AddTurntableLayer = [this, CylinderMesh](
            const FName Name,
            UMaterialInterface* Material,
            float RadiusScale,
            float HeightScale,
            float Z)
        {
            UStaticMeshComponent* Layer = CreateDefaultSubobject<UStaticMeshComponent>(Name);
            Layer->SetupAttachment(SceneRoot);
            Layer->SetStaticMesh(CylinderMesh);
            Layer->SetRelativeLocation(FVector(0.0f, 0.0f, Z));
            Layer->SetRelativeScale3D(FVector(RadiusScale, RadiusScale, HeightScale));
            Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Layer->SetCastShadow(true);
            if (Material) Layer->SetMaterial(0, Material);
        };
        UMaterialInterface* Metal = MetalFinder.Succeeded() ? MetalFinder.Object.Get() : ConcreteFinder.Object.Get();
        UMaterialInterface* Accent = MarkingFinder.Succeeded() ? MarkingFinder.Object.Get() : Metal;
        UMaterialInterface* Deck = ApronFinder.Succeeded() ? ApronFinder.Object.Get() : Metal;
        AddTurntableLayer(TEXT("TurntableBase"), Metal, 14.4f, 0.18f, 2.0f);
        AddTurntableLayer(TEXT("TurntableAccentRing"), Accent, 13.35f, 0.13f, 18.0f);
        AddTurntableLayer(TEXT("TurntableDeck"), Deck, 12.65f, 0.15f, 27.0f);
        AddTurntableLayer(TEXT("TurntableCenter"), Metal, 3.6f, 0.17f, 38.0f);
    }

    const FVector LightLocations[] =
    {
        FVector(-650.0f, -950.0f, 2100.0f),
        FVector(-100.0f, 950.0f, 1950.0f),
        FVector(1300.0f, 0.0f, 2200.0f)
    };
    const FLinearColor LightColors[] =
    {
        FLinearColor(0.72f, 0.88f, 1.0f),
        FLinearColor(1.0f, 0.72f, 0.42f),
        FLinearColor(0.62f, 0.86f, 1.0f)
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(LightLocations); ++Index)
    {
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("HangarLight%d"), Index));
        Light->SetupAttachment(SceneRoot);
        Light->SetRelativeLocation(LightLocations[Index]);
        Light->SetLightColor(LightColors[Index]);
        Light->SetIntensity(Index == 0 ? 9000.0f : 6500.0f);
        Light->SetAttenuationRadius(Index == 0 ? 2500.0f : 1900.0f);
        Light->SetCastShadows(Index == 0);
    }
}

void ARotorlineHangarPreviewActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    ShowcaseYaw = FMath::Fmod(ShowcaseYaw + DeltaSeconds * 4.0f, 360.0f);
    ShowcaseRoot->SetRelativeRotation(AircraftBaseRotation + FRotator(0.0f, ShowcaseYaw, 0.0f));

    for (int32 Index = 0; Index < SpinningMainRotorParts.Num(); ++Index)
    {
        USceneComponent* Part = SpinningMainRotorParts[Index];
        if (Part)
        {
            const FVector Axis = SpinningMainRotorAxes.IsValidIndex(Index)
                ? SpinningMainRotorAxes[Index].GetSafeNormal()
                : FVector::UpVector;
            Part->AddLocalRotation(FQuat(Axis, FMath::DegreesToRadians(DeltaSeconds * 420.0f)));
        }
    }
    for (int32 Index = 0; Index < SpinningTailRotorParts.Num(); ++Index)
    {
        USceneComponent* Part = SpinningTailRotorParts[Index];
        if (Part)
        {
            const FVector Axis = SpinningTailRotorAxes.IsValidIndex(Index)
                ? SpinningTailRotorAxes[Index].GetSafeNormal()
                : FVector::ForwardVector;
            Part->AddLocalRotation(FQuat(Axis, FMath::DegreesToRadians(DeltaSeconds * 680.0f)));
        }
    }
}

void ARotorlineHangarPreviewActor::ConfigureClassifiedPlaceholder()
{
    ResetAircraftComponents();
    ShowcaseYaw = 0.0f;
    AircraftBaseRotation = FRotator(0.0f, -18.0f, 0.0f);
    ShowcaseRoot->SetRelativeLocation(FVector::ZeroVector);
    ShowcaseRoot->SetRelativeScale3D(FVector::OneVector);
    ShowcaseRoot->SetRelativeRotation(AircraftBaseRotation);

    const auto AddCoverPart = [this](
        UStaticMesh* Mesh,
        UMaterialInterface* Material,
        const FVector& Location,
        const FVector& Scale)
    {
        if (!Mesh)
        {
            return;
        }
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
        AddInstanceComponent(Part);
        Part->SetupAttachment(ShowcaseRoot);
        Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(true);
        if (Material)
        {
            Part->SetMaterial(0, Material);
        }
        Part->RegisterComponent();
        AircraftStaticParts.Add(Part);
    };

    // The authored crate is imported at uniform scale 1.0 and already measures
    // 820 x 380 x 320 cm. Its pivot is at floor level, so this small Z offset
    // seats it on the existing turntable without altering the source scale.
    AddCoverPart(
        ClassifiedAirframeCrateMesh,
        nullptr,
        FVector(0.0f, 0.0f, 70.0f),
        FVector::OneVector);

    const FBox PlaceholderBounds(
        FVector(-720.0f, -560.0f, 30.0f),
        FVector(720.0f, 560.0f, 650.0f));
    FramePreviewCamera(PlaceholderBounds, 1.0f, FVector::ZeroVector, FVector::ZeroVector);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_HANGAR|CLASSIFIED_PLACEHOLDER|state=READY|asset=AIRFRAME_SHIPPING_CRATE|scale=1.0"));
}

void ARotorlineHangarPreviewActor::ConfigureAircraft(
    const TArray<FString>& StaticMeshPaths,
    const TArray<FString>& RotorMeshPaths,
    const TArray<FRotorlineAircraftRotorGroup>& RotorGroups,
    const TArray<FString>& StationaryRotorAssetPaths,
    bool bEnableFallbackRotors,
    const FVector& AircraftScale,
    const FRotator& AircraftRotation,
    const FVector& AircraftOffset)
{
    ResetAircraftComponents();
    ShowcaseYaw = 0.0f;
    AircraftBaseRotation = AircraftRotation;
    ShowcaseRoot->SetRelativeLocation(AircraftOffset);
    ShowcaseRoot->SetRelativeScale3D(FVector::OneVector);
    ShowcaseRoot->SetRelativeRotation(AircraftBaseRotation);

    TMap<FString, int32> DeclaredRotorOrder;
    for (int32 Index = 0; Index < RotorMeshPaths.Num(); ++Index)
    {
        if (!RotorMeshPaths[Index].IsEmpty())
        {
            DeclaredRotorOrder.Add(RotorMeshPaths[Index], Index);
        }
    }

    TMap<FString, int32> ExplicitRotorGroupByPath;
    TArray<bool> ExplicitRotorGroupIsTail;
    TArray<FVector> ExplicitRotorGroupAxes;
    TArray<bool> ExplicitRotorGroupHasPivot;
    TArray<FVector> ExplicitRotorGroupPivots;
    TArray<bool> ExplicitRotorGroupHasMeshPivot;
    TArray<FVector> ExplicitRotorGroupMeshPivots;
    TArray<FRotator> ExplicitRotorGroupAlignmentRotations;
    for (int32 GroupIndex = 0; GroupIndex < RotorGroups.Num(); ++GroupIndex)
    {
        const FRotorlineAircraftRotorGroup& Group = RotorGroups[GroupIndex];
        const bool bTail = Group.Role.Equals(TEXT("tail"), ESearchCase::IgnoreCase);
        ExplicitRotorGroupIsTail.Add(bTail);
        const FString AxisName = Group.SpinAxis.ToUpper();
        ExplicitRotorGroupAxes.Add(
            AxisName == TEXT("-X") ? -FVector::ForwardVector
            : AxisName == TEXT("-Y") ? -FVector::RightVector
            : AxisName == TEXT("-Z") ? -FVector::UpVector
            : AxisName == TEXT("X") ? FVector::ForwardVector
            : AxisName == TEXT("Y") ? FVector::RightVector
            : FVector::UpVector);
        ExplicitRotorGroupHasPivot.Add(Group.bHasExplicitPivot);
        ExplicitRotorGroupPivots.Add(Group.Pivot);
        ExplicitRotorGroupHasMeshPivot.Add(Group.bHasExplicitMeshPivot);
        ExplicitRotorGroupMeshPivots.Add(Group.MeshPivot);
        ExplicitRotorGroupAlignmentRotations.Add(Group.AlignmentRotation);
        for (const FString& AssetPath : Group.Assets)
        {
            if (!AssetPath.IsEmpty()) ExplicitRotorGroupByPath.Add(AssetPath, GroupIndex);
        }
    }

    TSet<FString> StationaryRotorAssets;
    for (const FString& StationaryPath : StationaryRotorAssetPaths)
    {
        if (!StationaryPath.IsEmpty()) StationaryRotorAssets.Add(StationaryPath);
    }

    TSet<FString> CandidatePaths;
    TSet<FString> ModelRoots;
    for (const FString& Path : StaticMeshPaths)
    {
        if (!Path.IsEmpty())
        {
            CandidatePaths.Add(Path);
            const FString ModelRoot = RotorlineHangar::ModelRootFromObjectPath(Path);
            if (!ModelRoot.IsEmpty())
            {
                ModelRoots.Add(ModelRoot);
            }
        }
    }
    for (const FString& Path : RotorMeshPaths)
    {
        if (!Path.IsEmpty())
        {
            CandidatePaths.Add(Path);
        }
    }
    for (const FRotorlineAircraftRotorGroup& Group : RotorGroups)
    {
        for (const FString& Path : Group.Assets)
        {
            if (!Path.IsEmpty()) CandidatePaths.Add(Path);
        }
    }

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    for (const FString& ModelRoot : ModelRoots)
    {
        const FString MeshFolders[] =
        {
            ModelRoot + TEXT("/StaticMeshes"),
            ModelRoot + TEXT("/SkeletalMeshes")
        };
        for (const FString& MeshFolder : MeshFolders)
        {
            TArray<FAssetData> Assets;
            AssetRegistry.GetAssetsByPath(FName(*MeshFolder), Assets, true);
            for (const FAssetData& Asset : Assets)
            {
                CandidatePaths.Add(Asset.GetSoftObjectPath().ToString());
            }
        }
    }

    TArray<FString> OrderedPaths = CandidatePaths.Array();
    OrderedPaths.Sort();
    FBox BodyBounds(ForceInit);
    TSet<FString> LoadedPaths;
    int32 LoadedBodyParts = 0;
    int32 LoadedRotorParts = 0;
    const int32 ImplicitMainRotorGroup = RotorGroups.Num();
    const int32 ImplicitTailRotorGroup = ImplicitMainRotorGroup + 1;
    TMap<int32, USceneComponent*> RotorGroupPivots;
    TMap<int32, FBox> RotorGroupBounds;
    TMap<int32, TArray<USceneComponent*>> RotorGroupParts;
    TMap<int32, bool> RotorGroupIsTail;
    TMap<int32, FVector> RotorGroupAxes;
    TMap<int32, bool> RotorGroupHasExplicitPivot;
    TMap<int32, FVector> RotorGroupExplicitPivots;
    TMap<int32, bool> RotorGroupHasExplicitMeshPivot;
    TMap<int32, FVector> RotorGroupExplicitMeshPivots;
    TMap<int32, FRotator> RotorGroupAlignmentRotations;
    const auto IsChinookAssetPath = [](const FString& Path)
    {
        return Path.Contains(TEXT("CH47Chinook"), ESearchCase::IgnoreCase);
    };
    const bool bUseTandemRotorPresentationAnchor =
        StaticMeshPaths.ContainsByPredicate(IsChinookAssetPath)
        || RotorMeshPaths.ContainsByPredicate(IsChinookAssetPath);
    bool bHasMainRotorPresentationAnchor = false;
    FVector MainRotorPresentationAnchor = FVector::ZeroVector;
    int32 TandemRotorPresentationAnchorCount = 0;
    FVector TandemRotorPresentationAnchorSum = FVector::ZeroVector;

    for (const FString& Path : OrderedPaths)
    {
        if (LoadedPaths.Contains(Path)
            || AircraftStaticParts.Num() + AircraftRotorParts.Num() >= RotorlineHangar::MaximumPreviewMeshParts)
        {
            continue;
        }

        const int32* ExplicitRotorGroup = ExplicitRotorGroupByPath.Find(Path);
        const int32* DeclaredRotorIndex = DeclaredRotorOrder.Find(Path);
        const bool bStationaryRotorAsset = StationaryRotorAssets.Contains(Path);
        const bool bImplicitRotor = !bStationaryRotorAsset
            && (DeclaredRotorIndex != nullptr || RotorlineHangar::IsRotorPath(Path));
        const bool bRotor = !bStationaryRotorAsset && (ExplicitRotorGroup != nullptr || bImplicitRotor);
        const bool bTailRotor = ExplicitRotorGroup
            ? ExplicitRotorGroupIsTail[*ExplicitRotorGroup]
            : RotorlineHangar::IsTailRotorPath(Path) || (DeclaredRotorIndex && *DeclaredRotorIndex > 0);
        const int32 RotorGroupIndex = ExplicitRotorGroup
            ? *ExplicitRotorGroup
            : bRotor ? (bTailRotor ? ImplicitTailRotorGroup : ImplicitMainRotorGroup) : INDEX_NONE;

        USceneComponent* RotorPivot = nullptr;
        if (bRotor)
        {
            if (USceneComponent** ExistingPivot = RotorGroupPivots.Find(RotorGroupIndex))
            {
                RotorPivot = *ExistingPivot;
            }
            else
            {
                RotorPivot = NewObject<USceneComponent>(this);
                AddInstanceComponent(RotorPivot);
                RotorPivot->SetupAttachment(ShowcaseRoot);
                RotorPivot->RegisterComponent();
                AircraftRotorPivots.Add(RotorPivot);
                RotorGroupPivots.Add(RotorGroupIndex, RotorPivot);
                RotorGroupBounds.Add(RotorGroupIndex, FBox(ForceInit));
                RotorGroupIsTail.Add(RotorGroupIndex, bTailRotor);
                RotorGroupAxes.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup
                        ? ExplicitRotorGroupAxes[*ExplicitRotorGroup]
                        : bTailRotor ? FVector::ForwardVector : FVector::UpVector);
                RotorGroupHasExplicitPivot.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup && ExplicitRotorGroupHasPivot[*ExplicitRotorGroup]);
                RotorGroupExplicitPivots.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup ? ExplicitRotorGroupPivots[*ExplicitRotorGroup] : FVector::ZeroVector);
                RotorGroupHasExplicitMeshPivot.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup && ExplicitRotorGroupHasMeshPivot[*ExplicitRotorGroup]);
                RotorGroupExplicitMeshPivots.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup ? ExplicitRotorGroupMeshPivots[*ExplicitRotorGroup] : FVector::ZeroVector);
                RotorGroupAlignmentRotations.Add(
                    RotorGroupIndex,
                    ExplicitRotorGroup ? ExplicitRotorGroupAlignmentRotations[*ExplicitRotorGroup] : FRotator::ZeroRotator);
            }
        }

        if (UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *Path))
        {
            UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
            AddInstanceComponent(Part);
            Part->SetupAttachment(RotorPivot ? RotorPivot : ShowcaseRoot.Get());
            Part->SetStaticMesh(StaticMesh);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Part->SetCastShadow(true);
            Part->RegisterComponent();
            AircraftStaticParts.Add(Part);
            LoadedPaths.Add(Path);
            if (bRotor)
            {
                RotorGroupBounds.FindChecked(RotorGroupIndex) += StaticMesh->GetBoundingBox();
                RotorGroupParts.FindOrAdd(RotorGroupIndex).Add(Part);
                ++LoadedRotorParts;
            }
            else
            {
                BodyBounds += StaticMesh->GetBoundingBox();
                ++LoadedBodyParts;
            }
            continue;
        }
        if (USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *Path))
        {
            USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(this);
            AddInstanceComponent(Part);
            Part->SetupAttachment(RotorPivot ? RotorPivot : ShowcaseRoot.Get());
            Part->SetSkeletalMesh(SkeletalMesh);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Part->SetCastShadow(true);
            Part->RegisterComponent();
            if (Path.Contains(TEXT("mh-6_little_bird_helicopter_animated"), ESearchCase::IgnoreCase))
            {
                static const TCHAR* MH6RotorAnimationPath =
                    TEXT("/Game/Qualification/LittleBird/Imported/mh-6_little_bird_helicopter_animated/SkeletalMeshes/mh-6_little_bird_helicopter_animated_Anim.mh-6_little_bird_helicopter_animated_Anim");
                if (UAnimSequence* RotorAnimation = LoadObject<UAnimSequence>(nullptr, MH6RotorAnimationPath))
                {
                    Part->SetAnimationMode(EAnimationMode::AnimationSingleNode);
                    Part->SetAnimation(RotorAnimation);
                    Part->SetPlayRate(1.0f);
                    Part->Play(true);
                }
            }
            AircraftRotorParts.Add(Part);
            LoadedPaths.Add(Path);
            if (bRotor)
            {
                RotorGroupBounds.FindChecked(RotorGroupIndex) += SkeletalMesh->GetBounds().GetBox();
                RotorGroupParts.FindOrAdd(RotorGroupIndex).Add(Part);
                ++LoadedRotorParts;
            }
            else
            {
                BodyBounds += SkeletalMesh->GetBounds().GetBox();
                ++LoadedBodyParts;
            }
        }
    }

    for (const TPair<int32, USceneComponent*>& Group : RotorGroupPivots)
    {
        const TArray<USceneComponent*>* GroupParts = RotorGroupParts.Find(Group.Key);
        if (!GroupParts || GroupParts->IsEmpty())
        {
            if (Group.Value) Group.Value->DestroyComponent();
            continue;
        }
        const FBox& Bounds = RotorGroupBounds.FindChecked(Group.Key);
        const bool bExplicitPivot = RotorGroupHasExplicitPivot.FindRef(Group.Key);
        const FVector PivotCenter = bExplicitPivot
            ? RotorGroupExplicitPivots.FindChecked(Group.Key)
            : Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;
        const bool bExplicitMeshPivot = RotorGroupHasExplicitMeshPivot.FindRef(Group.Key);
        const FVector MeshCenter = bExplicitMeshPivot
            ? RotorGroupExplicitMeshPivots.FindChecked(Group.Key)
            : PivotCenter;
        const FRotator AlignmentRotation = RotorGroupAlignmentRotations.FindRef(Group.Key);
        Group.Value->SetRelativeLocation(PivotCenter);
        if (!RotorGroupIsTail.FindChecked(Group.Key))
        {
            const FVector RotatedPivotCenter = AircraftBaseRotation.RotateVector(PivotCenter);
            if (bUseTandemRotorPresentationAnchor)
            {
                TandemRotorPresentationAnchorSum += RotatedPivotCenter;
                ++TandemRotorPresentationAnchorCount;
            }
            else if (!bHasMainRotorPresentationAnchor)
            {
                MainRotorPresentationAnchor = RotatedPivotCenter;
                bHasMainRotorPresentationAnchor = true;
            }
        }
        for (USceneComponent* Part : *GroupParts)
        {
            Part->SetRelativeRotation(AlignmentRotation);
            Part->SetRelativeLocation(-AlignmentRotation.RotateVector(MeshCenter));
        }
        if (RotorGroupIsTail.FindChecked(Group.Key))
        {
            SpinningTailRotorParts.Add(Group.Value);
            SpinningTailRotorAxes.Add(RotorGroupAxes.FindChecked(Group.Key));
        }
        else
        {
            SpinningMainRotorParts.Add(Group.Value);
            SpinningMainRotorAxes.Add(RotorGroupAxes.FindChecked(Group.Key));
        }
        UE_LOG(
            LogTemp,
            Display,
            TEXT("ROTORLINE_HANGAR_ROTOR_PIVOT|role=%s|parts=%d|source=%s|pivot=%.3f,%.3f,%.3f|mesh_pivot=%.3f,%.3f,%.3f"),
            RotorGroupIsTail.FindChecked(Group.Key) ? TEXT("tail") : TEXT("main"),
            GroupParts->Num(),
            bExplicitPivot ? TEXT("explicit") : TEXT("bounds"),
            PivotCenter.X,
            PivotCenter.Y,
            PivotCenter.Z,
            MeshCenter.X,
            MeshCenter.Y,
            MeshCenter.Z);
    }

    if (!BodyBounds.IsValid)
    {
        BodyBounds = FBox(FVector(-350.0f, -180.0f, 0.0f), FVector(350.0f, 180.0f, 260.0f));
    }
    const bool bIntegratedSkeletalRotorGeometry = !AircraftRotorParts.IsEmpty();
    int32 DeclaredMainRotorGroups = 0;
    int32 DeclaredTailRotorGroups = 0;
    for (const FRotorlineAircraftRotorGroup& Group : RotorGroups)
    {
        if (Group.Role.Equals(TEXT("tail"), ESearchCase::IgnoreCase))
        {
            ++DeclaredTailRotorGroups;
        }
        else
        {
            ++DeclaredMainRotorGroups;
        }
    }
    const bool bDeclaredCoaxialRotor = DeclaredMainRotorGroups >= 2 && DeclaredTailRotorGroups == 0;
    if (bEnableFallbackRotors && SpinningMainRotorParts.IsEmpty() && !bIntegratedSkeletalRotorGeometry)
    {
        AddFallbackMainRotor(BodyBounds);
    }
    if (bEnableFallbackRotors
        && SpinningTailRotorParts.IsEmpty()
        && !bIntegratedSkeletalRotorGeometry
        && !bDeclaredCoaxialRotor)
    {
        AddFallbackTailRotor(BodyBounds);
    }

    const float RequestedScale = FMath::Max3(
        FMath::Abs(AircraftScale.X),
        FMath::Abs(AircraftScale.Y),
        FMath::Abs(AircraftScale.Z));
    const float RawSpan = FMath::Max(BodyBounds.GetSize().GetMax(), 1.0f);
    const float RequestedSpan = RawSpan * FMath::Max(RequestedScale, KINDA_SMALL_NUMBER);
    const bool bGroundVehiclePreview =
        !bEnableFallbackRotors && RotorMeshPaths.IsEmpty() && StationaryRotorAssetPaths.IsEmpty();
    const bool bRequestedScaleFits = FMath::IsFinite(RequestedSpan)
        && (bGroundVehiclePreview
            ? RequestedSpan >= 1.0f
            : RequestedSpan >= RotorlineHangar::MinimumAcceptedSpanCm)
        && RequestedSpan <= RotorlineHangar::MaximumAcceptedSpanCm;
    const float EffectiveScale = bRequestedScaleFits
        ? RequestedScale
        : RotorlineHangar::TargetAircraftSpanCm / RawSpan;
    ShowcaseRoot->SetRelativeScale3D(FVector(EffectiveScale));

    FBox RotatedBodyBounds(ForceInit);
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        const FVector Corner(
            (CornerIndex & 1) ? BodyBounds.Max.X : BodyBounds.Min.X,
            (CornerIndex & 2) ? BodyBounds.Max.Y : BodyBounds.Min.Y,
            (CornerIndex & 4) ? BodyBounds.Max.Z : BodyBounds.Min.Z);
        RotatedBodyBounds += AircraftBaseRotation.RotateVector(Corner);
    }

    FVector CenteredOffset = AircraftOffset;
    const FVector BodyCenter = RotatedBodyBounds.GetCenter();
    const FVector PresentationAnchor =
        bUseTandemRotorPresentationAnchor && TandemRotorPresentationAnchorCount > 0
            ? TandemRotorPresentationAnchorSum / static_cast<float>(TandemRotorPresentationAnchorCount)
            : bHasMainRotorPresentationAnchor
                ? MainRotorPresentationAnchor
                : BodyCenter;
    CenteredOffset.X -= PresentationAnchor.X * EffectiveScale;
    CenteredOffset.Y -= PresentationAnchor.Y * EffectiveScale;
    CenteredOffset.Z -= RotatedBodyBounds.Min.Z * EffectiveScale;
    ShowcaseRoot->SetRelativeLocation(CenteredOffset);
    FramePreviewCamera(RotatedBodyBounds, EffectiveScale, CenteredOffset, AircraftOffset);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_HANGAR|PREVIEW|body_parts=%d|rotor_parts=%d|static=%d|skeletal=%d|fallback_main=%d|fallback_tail=%d|scale=%.5f|raw_span=%.1f"),
        LoadedBodyParts,
        LoadedRotorParts,
        AircraftStaticParts.Num(),
        AircraftRotorParts.Num(),
        FallbackMainRotorPivot ? 1 : 0,
        FallbackTailRotorPivot ? 1 : 0,
        EffectiveScale,
        RawSpan);
}

void ARotorlineHangarPreviewActor::FramePreviewCamera(
    const FBox& RotatedBodyBounds,
    float EffectiveScale,
    const FVector& CenteredOffset,
    const FVector& PresentationCenter)
{
    bAircraftFramed = false;
    if (!PreviewCamera || !RotatedBodyBounds.IsValid || !FMath::IsFinite(EffectiveScale) || EffectiveScale <= 0.0f)
    {
        return;
    }

    // Build the shot from the aircraft's normalized display bounds instead of
    // relying on the old fixed camera target. This keeps every qualified craft
    // on the turntable after deployment, debrief, and Return to Hangar.
    const FVector DisplayBodyCenter = CenteredOffset + RotatedBodyBounds.GetCenter() * EffectiveScale;
    const FVector DisplayCenter(PresentationCenter.X, PresentationCenter.Y, DisplayBodyCenter.Z);
    const FVector DisplayExtent = RotatedBodyBounds.GetExtent() * EffectiveScale;
    const FVector CompositionOffset = (DisplayBodyCenter - DisplayCenter).GetAbs();
    const float FramingRadius = FMath::Max((DisplayExtent + CompositionOffset).Size(), 380.0f);
    const float HorizontalHalfFov = FMath::DegreesToRadians(PreviewCamera->FieldOfView * 0.5f);
    constexpr float PreviewAspectRatio = 16.0f / 9.0f;
    const float VerticalHalfFov = FMath::Atan(FMath::Tan(HorizontalHalfFov) / PreviewAspectRatio);
    const float CameraDistance = FMath::Clamp(
        FramingRadius / FMath::Max(FMath::Tan(VerticalHalfFov), 0.05f) * 1.08f,
        2050.0f,
        3400.0f);
    const FVector CameraDirection = FVector(-0.82f, 0.55f, 0.16f).GetSafeNormal();
    const FVector CameraLocation = DisplayCenter + CameraDirection * CameraDistance;
    const FVector CameraTarget = DisplayCenter + FVector(0.0f, 0.0f, DisplayExtent.Z * 0.04f);

    PreviewCamera->SetRelativeLocation(CameraLocation);
    PreviewCamera->SetRelativeRotation((CameraTarget - CameraLocation).Rotation());
    PreviewCamera->SetFieldOfView(48.0f);
    PreviewCamera->Activate(true);
    bAircraftFramed = true;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_HANGAR_CAMERA|state=FRAMED|center=%.1f,%.1f,%.1f|extent=%.1f,%.1f,%.1f|distance=%.1f|fov=%.1f"),
        DisplayCenter.X,
        DisplayCenter.Y,
        DisplayCenter.Z,
        DisplayExtent.X,
        DisplayExtent.Y,
        DisplayExtent.Z,
        CameraDistance,
        PreviewCamera->FieldOfView);
}

void ARotorlineHangarPreviewActor::ResetAircraftComponents()
{
    bAircraftFramed = false;
    SpinningMainRotorParts.Reset();
    SpinningMainRotorAxes.Reset();
    SpinningTailRotorParts.Reset();
    SpinningTailRotorAxes.Reset();
    for (UStaticMeshComponent* Part : AircraftStaticParts)
    {
        if (Part)
        {
            Part->DestroyComponent();
        }
    }
    for (USkeletalMeshComponent* Part : AircraftRotorParts)
    {
        if (Part)
        {
            Part->DestroyComponent();
        }
    }
    AircraftStaticParts.Reset();
    AircraftRotorParts.Reset();
    for (USceneComponent* Pivot : AircraftRotorPivots)
    {
        if (Pivot)
        {
            Pivot->DestroyComponent();
        }
    }
    AircraftRotorPivots.Reset();
    if (FallbackMainRotorPivot)
    {
        FallbackMainRotorPivot->DestroyComponent();
        FallbackMainRotorPivot = nullptr;
    }
    if (FallbackTailRotorPivot)
    {
        FallbackTailRotorPivot->DestroyComponent();
        FallbackTailRotorPivot = nullptr;
    }
}

void ARotorlineHangarPreviewActor::AddFallbackMainRotor(const FBox& Bounds)
{
    if (!FallbackBladeMesh)
    {
        return;
    }

    const FVector Size = Bounds.GetSize();
    const FVector Center = Bounds.GetCenter();
    const float RotorRadius = FMath::Max(Size.X, Size.Y) * 0.58f;
    const float BladeThickness = FMath::Max(RotorRadius * 0.008f, 1.0f);

    FallbackMainRotorPivot = NewObject<USceneComponent>(this);
    AddInstanceComponent(FallbackMainRotorPivot);
    FallbackMainRotorPivot->SetupAttachment(ShowcaseRoot);
    FallbackMainRotorPivot->SetRelativeLocation(FVector(Center.X, Center.Y, Bounds.Max.Z + Size.Z * 0.04f));
    FallbackMainRotorPivot->RegisterComponent();

    UStaticMeshComponent* Disc = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Disc);
    Disc->SetupAttachment(FallbackMainRotorPivot);
    Disc->SetStaticMesh(FallbackBladeMesh);
    Disc->SetRelativeScale3D(FVector(RotorRadius / 50.0f, RotorRadius / 50.0f, BladeThickness / 100.0f));
    Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Disc->SetCastShadow(false);
    if (FallbackRotorMaterial)
    {
        Disc->SetMaterial(0, FallbackRotorMaterial);
    }
    Disc->RegisterComponent();
    AircraftStaticParts.Add(Disc);
    SpinningMainRotorParts.Add(FallbackMainRotorPivot);
    SpinningMainRotorAxes.Add(FVector::UpVector);
}

void ARotorlineHangarPreviewActor::AddFallbackTailRotor(const FBox& Bounds)
{
    if (!FallbackBladeMesh)
    {
        return;
    }

    const FVector Size = Bounds.GetSize();
    const FVector Center = Bounds.GetCenter();
    const float TailRadius = FMath::Max(Size.Z * 0.24f, FMath::Max(Size.X, Size.Y) * 0.07f);
    const float BladeThickness = FMath::Max(TailRadius * 0.025f, 0.8f);
    bFallbackTailUsesXAxis = Size.X >= Size.Y;

    FVector PivotLocation = Center;
    if (bFallbackTailUsesXAxis)
    {
        PivotLocation.X = Bounds.Min.X + Size.X * 0.03f;
    }
    else
    {
        PivotLocation.Y = Bounds.Min.Y + Size.Y * 0.03f;
    }
    PivotLocation.Z = Center.Z + Size.Z * 0.12f;

    FallbackTailRotorPivot = NewObject<USceneComponent>(this);
    AddInstanceComponent(FallbackTailRotorPivot);
    FallbackTailRotorPivot->SetupAttachment(ShowcaseRoot);
    FallbackTailRotorPivot->SetRelativeLocation(PivotLocation);
    FallbackTailRotorPivot->RegisterComponent();

    UStaticMeshComponent* Disc = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Disc);
    Disc->SetupAttachment(FallbackTailRotorPivot);
    Disc->SetStaticMesh(FallbackBladeMesh);
    Disc->SetRelativeScale3D(FVector(TailRadius / 50.0f, TailRadius / 50.0f, BladeThickness / 100.0f));
    Disc->SetRelativeRotation(bFallbackTailUsesXAxis
        ? FRotator(0.0f, 90.0f, 0.0f)
        : FRotator(-90.0f, 0.0f, 0.0f));
    Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Disc->SetCastShadow(false);
    if (FallbackRotorMaterial)
    {
        Disc->SetMaterial(0, FallbackRotorMaterial);
    }
    Disc->RegisterComponent();
    AircraftStaticParts.Add(Disc);
    SpinningTailRotorParts.Add(FallbackTailRotorPivot);
    SpinningTailRotorAxes.Add(bFallbackTailUsesXAxis ? FVector::ForwardVector : FVector::RightVector);
}
