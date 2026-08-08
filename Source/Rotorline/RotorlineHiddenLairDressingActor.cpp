#include "RotorlineHiddenLairDressingActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineHiddenLairDressing
{
    constexpr float DeckTopCm = 80.0f;
    constexpr float RotorClearanceRadiusCm = 1200.0f;
    constexpr float LaunchColumnHeightCm = 3800.0f;
    const TCHAR* KitRoot = TEXT("/Game/Rotorline/Environment/HiddenLair/DressingKit01");

    FString MeshPath(const FString& Module, const FString& Part)
    {
        return FString::Printf(TEXT("%s/%s/StaticMeshes/%s_%s.%s_%s"),
            KitRoot, *Module, *Module, *Part, *Module, *Part);
    }
}

ARotorlineHiddenLairDressingActor::ARotorlineHiddenLairDressingActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("BP_RL_HiddenLair_Dressing"));
    Root->SetMobility(EComponentMobility::Static);
    SetRootComponent(Root);

    const auto AddAnchor = [this](const TCHAR* Name, const FVector& Location, float YawDegrees, USceneComponent* Parent = nullptr)
    {
        USceneComponent* Anchor = CreateDefaultSubobject<USceneComponent>(Name);
        Anchor->SetupAttachment(Parent ? Parent : Root.Get());
        Anchor->SetMobility(EComponentMobility::Static);
        Anchor->SetRelativeLocation(Location);
        Anchor->SetRelativeRotation(FRotator(0.0f, YawDegrees, 0.0f));
        return Anchor;
    };

    // Keep the modules just inboard of the legacy wall-panel plane so their
    // authored consoles, racks, tires, and hazard trim remain readable.
    AnchorMainDoor = AddAnchor(TEXT("Anchor_MainDoor"), FVector(0.0f, 3000.0f, RotorlineHiddenLairDressing::DeckTopCm), 0.0f);
    AnchorCommandBay = AddAnchor(TEXT("Anchor_CommandBay"), FVector(3000.0f, 1500.0f, RotorlineHiddenLairDressing::DeckTopCm), -90.0f);
    AnchorMaintenanceBay = AddAnchor(TEXT("Anchor_MaintenanceBay"), FVector(-3000.0f, -1650.0f, RotorlineHiddenLairDressing::DeckTopCm), 90.0f);
    AnchorUtilityBay = AddAnchor(TEXT("Anchor_UtilityBay"), FVector(3000.0f, -1750.0f, RotorlineHiddenLairDressing::DeckTopCm), -90.0f);
    AnchorLogisticsBay = AddAnchor(TEXT("Anchor_LogisticsBay"), FVector(-900.0f, -3000.0f, RotorlineHiddenLairDressing::DeckTopCm), 180.0f);
    AnchorUpperGantryA = AddAnchor(TEXT("Anchor_UpperGantry_A"), FVector(1550.0f, 3250.0f, 2240.0f), 0.0f);
    AnchorUpperGantryB = AddAnchor(TEXT("Anchor_UpperGantry_B"), FVector(-1450.0f, -3250.0f, 2100.0f), 180.0f);
    AnchorStructuralRibs = AddAnchor(TEXT("Anchor_StructuralRibs"), FVector::ZeroVector, 0.0f);
    AnchorPipeRacks = AddAnchor(TEXT("Anchor_PipeRacks"), FVector::ZeroVector, 0.0f);
    AnchorDecalsAndCables = AddAnchor(TEXT("Anchor_DecalsAndCables"), FVector::ZeroVector, 0.0f);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AmberGlowFinder(
        TEXT("/Game/Missions/Presentation/M_ObjectiveAmberGlow.M_ObjectiveAmberGlow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> TealGlowFinder(
        TEXT("/Game/Missions/Presentation/M_SuccessGreenGlow.M_SuccessGreenGlow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedGlowFinder(
        TEXT("/Game/Missions/Presentation/M_TargetRedGlow.M_TargetRedGlow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> StructuralMetalFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Urban_Metal.M_Urban_Metal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> StructuralConcreteFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Concrete.M_Concrete"));
    const auto MaterialForFunctionalPart = [&](const FString& Part) -> UMaterialInterface*
    {
        if (Part.Equals(TEXT("HazardYellow"))) return AmberGlowFinder.Object;
        if (Part.Equals(TEXT("ScreenAmber"))) return AmberGlowFinder.Object;
        if (Part.Equals(TEXT("ScreenTeal"))) return TealGlowFinder.Object;
        if (Part.Equals(TEXT("EmergencyRed"))) return RedGlowFinder.Object;
        if (Part.Equals(TEXT("PaintedSteel"))) return StructuralConcreteFinder.Object;
        if (Part.Equals(TEXT("Gunmetal"))) return StructuralMetalFinder.Object;
        return nullptr;
    };

    const auto AddModule = [this, &MaterialForFunctionalPart](
        const FString& Module,
        USceneComponent* Anchor,
        const TArray<FString>& Parts,
        const FVector& Scale = FVector::OneVector,
        bool bDoorFrame = false)
    {
        for (const FString& Part : Parts)
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr,
                *RotorlineHiddenLairDressing::MeshPath(Module, Part));
            if (!Mesh)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("ROTORLINE_LAIR_DRESSING|ASSET_MISSING|module=%s|part=%s"), *Module, *Part);
                continue;
            }
            const FName ComponentName(*FString::Printf(TEXT("%s_%s"), *Anchor->GetName(), *Part));
            UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
            Component->SetupAttachment(Anchor);
            Component->SetMobility(EComponentMobility::Static);
            Component->SetStaticMesh(Mesh);
            // Interchange maps authored Z-height into local Y. The roll
            // restores Z-up; the local half-turn presents the detailed face
            // toward the room instead of burying it in the legacy wall.
            Component->SetRelativeRotation(FRotator(0.0f, 180.0f, 90.0f));
            Component->SetRelativeScale3D(Scale);
            if (UMaterialInterface* FunctionalMaterial = MaterialForFunctionalPart(Part))
            {
                Component->SetMaterial(0, FunctionalMaterial);
            }
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Component->SetCastShadow(true);
            Component->SetCullDistance(180000.0f);
            if (bDoorFrame) Component->ComponentTags.Add(TEXT("LairDoorFrame"));
            DressingGeometry.Add(Component);
        }
    };

    AddModule(TEXT("RL_Lair_BlastDoorFrame_10m"), AnchorMainDoor,
        {TEXT("BlackRubber"), TEXT("EmergencyRed"), TEXT("Gunmetal"), TEXT("HazardYellow"), TEXT("PaintedSteel")},
        FVector(1.58f, 1.0f, 1.0f), true);
    AddModule(TEXT("RL_Lair_CommandWall_12m"), AnchorCommandBay,
        {TEXT("BlackRubber"), TEXT("DarkPanel"), TEXT("Gunmetal"), TEXT("HazardYellow"), TEXT("PaintedSteel"), TEXT("ScreenAmber"), TEXT("ScreenTeal")});
    AddModule(TEXT("RL_Lair_ServiceBay_12m"), AnchorMaintenanceBay,
        {TEXT("BlackRubber"), TEXT("DarkPanel"), TEXT("Gunmetal"), TEXT("HazardYellow"), TEXT("PaintedSteel"), TEXT("ScreenAmber")});
    AddModule(TEXT("RL_Lair_UtilityWall_10m"), AnchorUtilityBay,
        {TEXT("BlackRubber"), TEXT("Concrete"), TEXT("DarkPanel"), TEXT("EmergencyRed"), TEXT("Gunmetal"), TEXT("HazardYellow"), TEXT("PaintedSteel"), TEXT("ScreenAmber")});
    AddModule(TEXT("RL_Lair_StorageBay_12m"), AnchorLogisticsBay,
        {TEXT("BlackRubber"), TEXT("Concrete"), TEXT("DarkPanel"), TEXT("Gunmetal"), TEXT("HazardYellow"), TEXT("PaintedSteel")});
    AddModule(TEXT("RL_Lair_OverheadGantry_14m"), AnchorUpperGantryA,
        {TEXT("DarkPanel"), TEXT("Gunmetal"), TEXT("PaintedSteel"), TEXT("ScreenTeal")});
    AddModule(TEXT("RL_Lair_OverheadGantry_14m"), AnchorUpperGantryB,
        {TEXT("DarkPanel"), TEXT("Gunmetal"), TEXT("PaintedSteel"), TEXT("ScreenTeal")});

    const auto AddRepeatedModule = [this](
        const FString& Module,
        USceneComponent* Anchor,
        const TArray<FString>& Parts,
        const TArray<FTransform>& Instances)
    {
        for (const FString& Part : Parts)
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr,
                *RotorlineHiddenLairDressing::MeshPath(Module, Part));
            if (!Mesh) continue;
            const FName ComponentName(*FString::Printf(TEXT("HISM_%s_%s"), *Module, *Part));
            UInstancedStaticMeshComponent* Component =
                CreateDefaultSubobject<UInstancedStaticMeshComponent>(ComponentName);
            Component->SetupAttachment(Anchor);
            Component->SetMobility(EComponentMobility::Static);
            Component->SetStaticMesh(Mesh);
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Component->SetCastShadow(true);
            Component->SetCullDistances(0, 180000);
            for (const FTransform& Instance : Instances) Component->AddInstance(Instance);
            DressingGeometry.Add(Component);
        }
    };

    const TArray<FTransform> RibTransforms = {
        FTransform(FRotator(0.0f, 0.0f, 90.0f), FVector(-2600.0f, 3340.0f, 80.0f)),
        FTransform(FRotator(0.0f, 0.0f, 90.0f), FVector(2600.0f, 3340.0f, 80.0f)),
        FTransform(FRotator(0.0f, 180.0f, 90.0f), FVector(-2550.0f, -3340.0f, 80.0f)),
        FTransform(FRotator(0.0f, 180.0f, 90.0f), FVector(2550.0f, -3340.0f, 80.0f)),
        FTransform(FRotator(0.0f, -90.0f, 90.0f), FVector(3340.0f, -500.0f, 80.0f)),
        FTransform(FRotator(0.0f, -90.0f, 90.0f), FVector(3340.0f, 2650.0f, 80.0f)),
        FTransform(FRotator(0.0f, 90.0f, 90.0f), FVector(-3340.0f, -2850.0f, 80.0f)),
        FTransform(FRotator(0.0f, 90.0f, 90.0f), FVector(-3340.0f, 150.0f, 80.0f)),
        FTransform(FRotator(0.0f, 90.0f, 90.0f), FVector(-3340.0f, 2750.0f, 80.0f)),
    };
    AddRepeatedModule(TEXT("RL_Lair_StructuralRib_7m"), AnchorStructuralRibs,
        {TEXT("DarkPanel"), TEXT("Gunmetal"), TEXT("HazardYellow"), TEXT("PaintedSteel"), TEXT("ScreenAmber")},
        RibTransforms);

    const TArray<FTransform> PipeRackTransforms = {
        FTransform(FRotator(0.0f, -90.0f, 90.0f), FVector(3260.0f, 250.0f, 1980.0f)),
        FTransform(FRotator(0.0f, 90.0f, 90.0f), FVector(-3260.0f, 450.0f, 2140.0f)),
    };
    AddRepeatedModule(TEXT("RL_Lair_PipeRack_8m"), AnchorPipeRacks,
        {TEXT("DarkPanel"), TEXT("Gunmetal"), TEXT("HazardYellow"), TEXT("PaintedSteel"), TEXT("ScreenTeal")},
        PipeRackTransforms);

    const auto AddWorkLight = [this](const TCHAR* Name, const FVector& Location, const FLinearColor& Color, float Intensity)
    {
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
        Light->SetupAttachment(Root);
        Light->SetMobility(EComponentMobility::Static);
        Light->SetRelativeLocation(Location);
        Light->SetLightColor(Color);
        Light->SetIntensity(Intensity);
        Light->SetAttenuationRadius(1150.0f);
        Light->SetCastShadows(false);
        DressingLights.Add(Light);
    };
    AddWorkLight(TEXT("CommandTaskLight"), FVector(2500.0f, 1500.0f, 820.0f), FLinearColor(0.18f, 0.66f, 0.72f), 9800.0f);
    AddWorkLight(TEXT("MaintenanceTaskLight"), FVector(-2500.0f, -1650.0f, 840.0f), FLinearColor(1.0f, 0.54f, 0.22f), 10400.0f);
    AddWorkLight(TEXT("UtilityTaskLight"), FVector(2500.0f, -1750.0f, 980.0f), FLinearColor(0.92f, 0.42f, 0.16f), 9200.0f);
    AddWorkLight(TEXT("LogisticsTaskLight"), FVector(-900.0f, -2500.0f, 840.0f), FLinearColor(0.35f, 0.66f, 0.76f), 9400.0f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DebugFinder(TEXT("/Game/Missions/Presentation/M_TargetRedGlow.M_TargetRedGlow"));
    RotorClearanceDebug = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Debug_RotorClearanceCylinder"));
    RotorClearanceDebug->SetupAttachment(Root);
    RotorClearanceDebug->SetStaticMesh(CylinderFinder.Object);
    RotorClearanceDebug->SetMaterial(0, DebugFinder.Object);
    RotorClearanceDebug->SetRelativeLocation(FVector(0.0f, 0.0f, RotorlineHiddenLairDressing::LaunchColumnHeightCm * 0.5f));
    RotorClearanceDebug->SetRelativeScale3D(FVector(
        RotorlineHiddenLairDressing::RotorClearanceRadiusCm / 50.0f,
        RotorlineHiddenLairDressing::RotorClearanceRadiusCm / 50.0f,
        RotorlineHiddenLairDressing::LaunchColumnHeightCm / 100.0f));
    RotorClearanceDebug->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RotorClearanceDebug->SetVisibility(false, true);

    DoorCorridorDebug = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Debug_DoorFlightCorridor"));
    DoorCorridorDebug->SetupAttachment(Root);
    DoorCorridorDebug->SetStaticMesh(CubeFinder.Object);
    DoorCorridorDebug->SetMaterial(0, DebugFinder.Object);
    DoorCorridorDebug->SetRelativeLocation(FVector(0.0f, 2450.0f, 900.0f));
    DoorCorridorDebug->SetRelativeScale3D(FVector(16.0f, 21.0f, 18.0f));
    DoorCorridorDebug->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DoorCorridorDebug->SetVisibility(false, true);

    bClearanceDebugVisible = FParse::Param(FCommandLine::Get(), TEXT("RotorlineLairClearanceDebug"));
    SetDressingVisible(bDressingVisible);
    SetClearanceDebugVisible(bClearanceDebugVisible);
}

void ARotorlineHiddenLairDressingActor::SetDressingVisible(bool bVisible)
{
    bDressingVisible = bVisible;
    for (UPrimitiveComponent* Component : DressingGeometry)
    {
        if (Component) Component->SetVisibility(bVisible, true);
    }
    for (UPointLightComponent* Light : DressingLights)
    {
        if (Light) Light->SetVisibility(bVisible, true);
    }
}

void ARotorlineHiddenLairDressingActor::SetClearanceDebugVisible(bool bVisible)
{
    bClearanceDebugVisible = bVisible;
    if (RotorClearanceDebug) RotorClearanceDebug->SetVisibility(bVisible, true);
    if (DoorCorridorDebug) DoorCorridorDebug->SetVisibility(bVisible, true);
}

bool ARotorlineHiddenLairDressingActor::ValidateClearance(FString& OutDetail) const
{
    float MinimumRadialClearanceCm = TNumericLimits<float>::Max();
    int32 CheckedComponents = 0;
    int32 CheckedInstances = 0;
    const auto AccumulateBounds = [this, &MinimumRadialClearanceCm](const FBoxSphereBounds& Bounds)
    {
        const FVector LocalCenter = GetActorTransform().InverseTransformPosition(Bounds.Origin);
        const float ConservativeRadius = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y);
        MinimumRadialClearanceCm = FMath::Min(
            MinimumRadialClearanceCm,
            FVector2D(LocalCenter.X, LocalCenter.Y).Size() - ConservativeRadius);
    };
    for (const UPrimitiveComponent* Component : DressingGeometry)
    {
        if (!Component || Component->ComponentHasTag(TEXT("LairDoorFrame"))) continue;
        if (const UInstancedStaticMeshComponent* Instances =
            Cast<UInstancedStaticMeshComponent>(Component))
        {
            const UStaticMesh* Mesh = Instances->GetStaticMesh();
            if (!Mesh) continue;
            for (int32 InstanceIndex = 0; InstanceIndex < Instances->GetInstanceCount(); ++InstanceIndex)
            {
                FTransform InstanceWorldTransform;
                if (!Instances->GetInstanceTransform(InstanceIndex, InstanceWorldTransform, true)) continue;
                const FBox InstanceWorldBox = Mesh->GetBoundingBox().TransformBy(InstanceWorldTransform);
                AccumulateBounds(FBoxSphereBounds(InstanceWorldBox));
                ++CheckedInstances;
            }
        }
        else
        {
            AccumulateBounds(Component->Bounds);
        }
        ++CheckedComponents;
    }
    const bool bClear = MinimumRadialClearanceCm >= RotorlineHiddenLairDressing::RotorClearanceRadiusCm;
    OutDetail = FString::Printf(
        TEXT("checked=%d|instances=%d|min_clearance_cm=%.0f|required_cm=%.0f|door_corridor=FRAME_OPEN"),
        CheckedComponents,
        CheckedInstances,
        MinimumRadialClearanceCm,
        RotorlineHiddenLairDressing::RotorClearanceRadiusCm);
    return bClear;
}
