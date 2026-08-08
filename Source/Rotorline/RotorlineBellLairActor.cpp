#include "RotorlineBellLairActor.h"

#include "Components/PointLightComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RotorlineSupportLocations.h"
#include "RotorlineHelicopterPawn.h"
#include "RotorlineHiddenLairDressingActor.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineBellLair
{
    // The launch chamber is buried below the true Landscape surface. Only a
    // compact 30 m hatch and irregular rock lip are exposed at summit level.
    constexpr float RoofHeightCm = RotorlineSupportLocations::BellLairBurialDepthCm;
    constexpr float ApproachRadiusCm = 15000.0f;
    constexpr float ReleaseRadiusCm = 18500.0f;
    constexpr float HatchTravelCm = 9000.0f;
    constexpr float ChamberCeilingDepthCm = 900.0f;
    constexpr float HatchSpeed = 0.72f;

    void ConfigureBlock(
        UStaticMeshComponent* Component,
        UStaticMesh* Cube,
        UMaterialInterface* Material,
        const FVector& Location,
        const FVector& DimensionsCm,
        const FRotator& Rotation = FRotator::ZeroRotator,
        ECollisionEnabled::Type Collision = ECollisionEnabled::QueryAndPhysics)
    {
        if (!Component) return;
        Component->SetStaticMesh(Cube);
        Component->SetMaterial(0, Material);
        Component->SetRelativeLocation(Location);
        Component->SetRelativeRotation(Rotation);
        Component->SetRelativeScale3D(DimensionsCm / 100.0f);
        Component->SetCollisionEnabled(Collision);
    }
}

ARotorlineBellLairActor::ARotorlineBellLairActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("BellLairRoot"));
    Root->SetMobility(EComponentMobility::Static);
    SetRootComponent(Root);

    DressingActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("BP_RL_HiddenLair_Dressing"));
    DressingActorComponent->SetupAttachment(Root);
    DressingActorComponent->SetMobility(EComponentMobility::Static);
    DressingActorComponent->SetRelativeTransform(FTransform::Identity);
    DressingActorComponent->SetChildActorClass(ARotorlineHiddenLairDressingActor::StaticClass());

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PadFinder(TEXT("/Game/Environment/Imported/Heliports/Compact/SM_Heliport_Compact/StaticMeshes/SM_Heliport_Compact.SM_Heliport_Compact"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RockFinder(TEXT("/Game/Environment/Imported/StandaloneRocks/low-poly-sculpted-boulder/StaticMeshes/low-poly-sculpted-boulder.low-poly-sculpted-boulder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ConcreteFinder(TEXT("/Game/Environment/Materials/Blockout/M_Concrete.M_Concrete"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalFinder(TEXT("/Game/Environment/Materials/Urban/M_Urban_Metal.M_Urban_Metal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> OliveFinder(TEXT("/Game/Environment/Materials/Urban/M_Landmark_AirfieldOlive.M_Landmark_AirfieldOlive"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> YellowFinder(TEXT("/Game/Environment/Materials/Blockout/M_Marking_Yellow.M_Marking_Yellow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkWindowFinder(TEXT("/Game/Environment/Materials/Urban/M_Urban_Window_Dark.M_Urban_Window_Dark"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GrassFinder(TEXT("/Game/Environment/Materials/M_Landscape_IslandBiomes.M_Landscape_IslandBiomes"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AmberFinder(TEXT("/Game/Missions/Presentation/M_ObjectiveAmberGlow.M_ObjectiveAmberGlow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedFinder(TEXT("/Game/Missions/Presentation/M_TargetRedGlow.M_TargetRedGlow"));

    UStaticMesh* Cube = CubeFinder.Object.Get();
    UMaterialInterface* Concrete = ConcreteFinder.Object.Get();
    UMaterialInterface* Metal = MetalFinder.Succeeded() ? MetalFinder.Object.Get() : Concrete;
    UMaterialInterface* Olive = OliveFinder.Succeeded() ? OliveFinder.Object.Get() : Metal;
    UMaterialInterface* Yellow = YellowFinder.Succeeded() ? YellowFinder.Object.Get() : Metal;
    UMaterialInterface* Screen = DarkWindowFinder.Succeeded() ? DarkWindowFinder.Object.Get() : Metal;
    UMaterialInterface* Grass = GrassFinder.Succeeded() ? GrassFinder.Object.Get() : Concrete;

    auto AddBlock = [this, Cube](
        const TCHAR* Name,
        UMaterialInterface* Material,
        const FVector& Location,
        const FVector& Dimensions,
        const FRotator& Rotation = FRotator::ZeroRotator,
        ECollisionEnabled::Type Collision = ECollisionEnabled::QueryAndPhysics)
    {
        UStaticMeshComponent* Block = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Block->SetupAttachment(Root);
        RotorlineBellLair::ConfigureBlock(Block, Cube, Material, Location, Dimensions, Rotation, Collision);
        return Block;
    };

    // Reinforced chamber and deck.
    AddBlock(TEXT("ConcreteDeck"), Concrete, FVector(0.0f, 0.0f, 40.0f), FVector(7200.0f, 7200.0f, 80.0f));
    const float ChamberWallHeight = RotorlineBellLair::RoofHeightCm - RotorlineBellLair::ChamberCeilingDepthCm;
    const float HalfChamberWallHeight = ChamberWallHeight * 0.5f;
    AddBlock(TEXT("NorthWall"), Concrete, FVector(0.0f, 3500.0f, HalfChamberWallHeight), FVector(7200.0f, 180.0f, ChamberWallHeight));
    AddBlock(TEXT("SouthWall"), Concrete, FVector(0.0f, -3500.0f, HalfChamberWallHeight), FVector(7200.0f, 180.0f, ChamberWallHeight));
    AddBlock(TEXT("EastWall"), Concrete, FVector(3500.0f, 0.0f, HalfChamberWallHeight), FVector(180.0f, 7200.0f, ChamberWallHeight));
    AddBlock(TEXT("WestWall"), Concrete, FVector(-3500.0f, 0.0f, HalfChamberWallHeight), FVector(180.0f, 7200.0f, ChamberWallHeight));

    // The actual chamber roof sits nine metres below the summit. The compact
    // mesh-lined shaft is the only connection to the Landscape opening.
    const float SurfaceCeilingZ = RotorlineBellLair::RoofHeightCm - RotorlineBellLair::ChamberCeilingDepthCm;
    AddBlock(TEXT("NorthCaveCeiling"), Metal, FVector(0.0f, 3400.0f, SurfaceCeilingZ), FVector(7200.0f, 400.0f, 240.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
    AddBlock(TEXT("SouthCaveCeiling"), Metal, FVector(0.0f, -3400.0f, SurfaceCeilingZ), FVector(7200.0f, 400.0f, 240.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
    AddBlock(TEXT("EastCaveCeiling"), Metal, FVector(3400.0f, 0.0f, SurfaceCeilingZ), FVector(400.0f, 6400.0f, 240.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
    AddBlock(TEXT("WestCaveCeiling"), Metal, FVector(-3400.0f, 0.0f, SurfaceCeilingZ), FVector(400.0f, 6400.0f, 240.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);

    const float ShaftWallHeight = RotorlineBellLair::ChamberCeilingDepthCm - 140.0f;
    const float ShaftWallCenterZ = SurfaceCeilingZ + ShaftWallHeight * 0.5f;
    constexpr int32 ShaftSegmentCount = 12;
    constexpr float ShaftWallRadius = 3100.0f;
    const float ShaftSegmentLength = 2.0f * ShaftWallRadius * FMath::Tan(PI / ShaftSegmentCount) * 1.08f;
    for (int32 Index = 0; Index < ShaftSegmentCount; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / ShaftSegmentCount;
        AddBlock(
            *FString::Printf(TEXT("ShaftWall%02d"), Index + 1),
            Concrete,
            FVector(FMath::Cos(Angle) * ShaftWallRadius, FMath::Sin(Angle) * ShaftWallRadius, ShaftWallCenterZ),
            FVector(ShaftSegmentLength, 120.0f, ShaftWallHeight),
            FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f),
            ECollisionEnabled::NoCollision);
    }

    // Interior dressing is deliberately non-colliding and confined to the
    // perimeter. The launch deck, rotor cylinder, hatch shaft, spawn point,
    // and service triggers remain exactly as authored above.
    auto AddInstancer = [this](
        const TCHAR* Name,
        UStaticMesh* Mesh,
        UMaterialInterface* Material)
    {
        UInstancedStaticMeshComponent* Instances =
            CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        Instances->SetupAttachment(Root);
        Instances->SetStaticMesh(Mesh);
        if (Material) Instances->SetMaterial(0, Material);
        Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Instances->SetCastShadow(true);
        Instances->SetCullDistances(0, 180000);
        return Instances;
    };
    auto AddCubeInstance = [](UInstancedStaticMeshComponent* Instances,
        const FVector& Location, const FVector& Dimensions, const FRotator& Rotation = FRotator::ZeroRotator)
    {
        if (Instances)
        {
            Instances->AddInstance(FTransform(Rotation, Location, Dimensions / 100.0f));
        }
    };

    UInstancedStaticMeshComponent* StructuralSteel = AddInstancer(TEXT("LairStructuralSteel"), Cube, Metal);
    UInstancedStaticMeshComponent* WallPanels = AddInstancer(TEXT("LairWallPanels"), Cube, Metal);
    UInstancedStaticMeshComponent* UtilityRuns = AddInstancer(TEXT("LairUtilityRuns"), Cube, Olive);
    UInstancedStaticMeshComponent* SafetyMarkings = AddInstancer(TEXT("LairSafetyMarkings"), Cube, Yellow);
    UInstancedStaticMeshComponent* ScreenPanels = AddInstancer(TEXT("LairScreenPanels"), Cube, Screen);
    UInstancedStaticMeshComponent* InteriorRock = AddInstancer(TEXT("LairInteriorRock"), RockFinder.Object, nullptr);

    // Heavy ribs and columns give every wall a reinforced rhythm without
    // repeating an identical tile across the whole chamber.
    for (float X : { -2920.0f, -1740.0f, -420.0f, 1040.0f, 2860.0f })
    {
        AddCubeInstance(StructuralSteel, FVector(X, 3370.0f, 1450.0f), FVector(105.0f, 220.0f, 2800.0f));
        AddCubeInstance(StructuralSteel, FVector(-X * 0.94f, -3370.0f, 1450.0f), FVector(105.0f, 220.0f, 2800.0f));
    }
    for (float Y : { -2840.0f, -1520.0f, 120.0f, 1580.0f, 2920.0f })
    {
        AddCubeInstance(StructuralSteel, FVector(3370.0f, Y, 1450.0f), FVector(220.0f, 105.0f, 2800.0f));
        AddCubeInstance(StructuralSteel, FVector(-3370.0f, -Y * 0.91f, 1450.0f), FVector(220.0f, 105.0f, 2800.0f));
    }
    for (float Offset : { -3000.0f, 3000.0f })
    {
        AddCubeInstance(StructuralSteel, FVector(Offset, 0.0f, 2700.0f), FVector(180.0f, 6800.0f, 180.0f));
        AddCubeInstance(StructuralSteel, FVector(0.0f, Offset, 2580.0f), FVector(6800.0f, 160.0f, 150.0f));
    }

    // Varied bolted wall-panel fields. Gaps are reserved for doors, consoles,
    // and exposed concrete so the room reads as retrofitted over time.
    const TArray<FVector> NorthSouthPanels = {
        FVector(-2350.0f, 3420.0f, 760.0f), FVector(-650.0f, 3420.0f, 1780.0f),
        FVector(1180.0f, 3420.0f, 760.0f), FVector(2450.0f, 3420.0f, 1820.0f),
        FVector(-2500.0f, -3420.0f, 1750.0f), FVector(-950.0f, -3420.0f, 720.0f),
        FVector(850.0f, -3420.0f, 1780.0f), FVector(2450.0f, -3420.0f, 760.0f)
    };
    for (const FVector& PanelLocation : NorthSouthPanels)
    {
        AddCubeInstance(WallPanels, PanelLocation, FVector(1180.0f, 55.0f, 760.0f));
    }
    const TArray<FVector> EastWestPanels = {
        FVector(3420.0f, -2350.0f, 720.0f), FVector(3420.0f, -720.0f, 1760.0f),
        FVector(3420.0f, 1050.0f, 720.0f), FVector(3420.0f, 2440.0f, 1800.0f),
        FVector(-3420.0f, -2450.0f, 1800.0f), FVector(-3420.0f, -900.0f, 740.0f),
        FVector(-3420.0f, 900.0f, 1780.0f), FVector(-3420.0f, 2440.0f, 760.0f)
    };
    for (const FVector& PanelLocation : EastWestPanels)
    {
        AddCubeInstance(WallPanels, PanelLocation, FVector(55.0f, 1180.0f, 760.0f));
    }

    // Overhead ducts, cable trays, and service pipes stay outside the 60 m
    // launch shaft and maintain generous rotor/camera clearance.
    AddCubeInstance(UtilityRuns, FVector(3180.0f, 0.0f, 2350.0f), FVector(260.0f, 6100.0f, 280.0f));
    AddCubeInstance(UtilityRuns, FVector(-3180.0f, 0.0f, 2200.0f), FVector(180.0f, 5800.0f, 180.0f));
    AddCubeInstance(UtilityRuns, FVector(0.0f, 3200.0f, 2300.0f), FVector(5700.0f, 220.0f, 220.0f));
    AddCubeInstance(UtilityRuns, FVector(0.0f, -3210.0f, 2470.0f), FVector(6000.0f, 130.0f, 130.0f));
    AddCubeInstance(UtilityRuns, FVector(3290.0f, 0.0f, 2050.0f), FVector(70.0f, 6200.0f, 70.0f));
    AddCubeInstance(UtilityRuns, FVector(-3290.0f, 0.0f, 1900.0f), FVector(70.0f, 5700.0f, 70.0f));

    // Safety-edge language is restrained to the perimeter; the existing
    // launch platform and its markings are not modified.
    AddCubeInstance(SafetyMarkings, FVector(0.0f, 3260.0f, 90.0f), FVector(6100.0f, 35.0f, 12.0f));
    AddCubeInstance(SafetyMarkings, FVector(0.0f, -3260.0f, 90.0f), FVector(6100.0f, 35.0f, 12.0f));
    AddCubeInstance(SafetyMarkings, FVector(3260.0f, 0.0f, 90.0f), FVector(35.0f, 6100.0f, 12.0f));
    AddCubeInstance(SafetyMarkings, FVector(-3260.0f, 0.0f, 90.0f), FVector(35.0f, 6100.0f, 12.0f));

    // North wall: armored blast door.
    AddBlock(TEXT("LairBlastDoor"), Metal, FVector(0.0f, 3370.0f, 920.0f), FVector(1700.0f, 150.0f, 1700.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
    AddBlock(TEXT("BlastDoorHeader"), Concrete, FVector(0.0f, 3260.0f, 1900.0f), FVector(2150.0f, 260.0f, 240.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
    for (float X : { -620.0f, -310.0f, 0.0f, 310.0f, 620.0f })
    {
        AddCubeInstance(StructuralSteel, FVector(X, 3265.0f, 920.0f), FVector(55.0f, 120.0f, 1500.0f));
    }

    // South wall: active storage and power-service zone with varied cabinet
    // heights, a recessed access door, and restrained equipment stacks.
    AddBlock(TEXT("SouthAccessDoor"), Metal, FVector(2200.0f, -3370.0f, 720.0f), FVector(760.0f, 130.0f, 1320.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const float X = -2550.0f + Index * 520.0f;
        const float Height = Index % 2 == 0 ? 1050.0f : 820.0f;
        AddBlock(*FString::Printf(TEXT("PowerCabinet%02d"), Index + 1), Index == 4 ? Olive : Metal,
            FVector(X, -3230.0f, Height * 0.5f + 80.0f), FVector(390.0f, 420.0f, Height), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
        AddCubeInstance(ScreenPanels, FVector(X, -2995.0f, Height * 0.65f), FVector(220.0f, 24.0f, 150.0f));
    }

    // Emergency cabinets stay readable and clear of both service aisles.
    AddBlock(TEXT("EmergencyCabinetWest"), Metal, FVector(-3290.0f, -2320.0f, 760.0f), FVector(90.0f, 420.0f, 620.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
    AddBlock(TEXT("EmergencyCabinetEast"), Metal, FVector(3290.0f, 2450.0f, 760.0f), FVector(90.0f, 420.0f, 620.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
    AddBlock(TEXT("EmergencyIndicatorWest"), RedFinder.Object, FVector(-3235.0f, -2320.0f, 820.0f), FVector(12.0f, 110.0f, 65.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
    AddBlock(TEXT("EmergencyIndicatorEast"), RedFinder.Object, FVector(3235.0f, 2450.0f, 820.0f), FVector(12.0f, 110.0f, 65.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);

    // Exposed excavation is embedded deeply into the wall/ceiling and
    // wall/floor seams so no rock reads as a loose or floating boulder.
    const TArray<FTransform> RockTransforms = {
        FTransform(FRotator(18.0f, 12.0f, 25.0f), FVector(3490.0f, 3450.0f, 2700.0f), FVector(2.8f, 2.2f, 2.1f)),
        FTransform(FRotator(-12.0f, 78.0f, 9.0f), FVector(-3480.0f, 3440.0f, 2600.0f), FVector(2.4f, 2.7f, 1.9f)),
        FTransform(FRotator(9.0f, 145.0f, -14.0f), FVector(3460.0f, -3480.0f, 2630.0f), FVector(2.7f, 2.1f, 2.0f)),
        FTransform(FRotator(-17.0f, 210.0f, 18.0f), FVector(-3470.0f, -3460.0f, 2710.0f), FVector(2.5f, 2.3f, 2.2f)),
        FTransform(FRotator(24.0f, 38.0f, 5.0f), FVector(3520.0f, 3390.0f, -280.0f), FVector(2.3f, 2.0f, 1.5f)),
        FTransform(FRotator(-8.0f, 126.0f, 20.0f), FVector(-3510.0f, -3370.0f, -320.0f), FVector(2.1f, 2.4f, 1.5f))
    };
    for (const FTransform& RockTransform : RockTransforms)
    {
        InteriorRock->AddInstance(RockTransform);
    }
    // Practical fixtures: visible housings with alternating cool work light
    // and warm maintenance light. Peripheral intensity stays below the four
    // Bell-focused floods.
    const TArray<FVector> UtilityLightLocations = {
        FVector(-2300.0f, 3150.0f, 2350.0f), FVector(2300.0f, 3150.0f, 2350.0f),
        FVector(-2300.0f, -3150.0f, 2250.0f), FVector(2300.0f, -3150.0f, 2250.0f),
        FVector(3150.0f, -2100.0f, 2200.0f), FVector(3150.0f, 1950.0f, 2300.0f),
        FVector(-3150.0f, -1900.0f, 2320.0f), FVector(-3150.0f, 2100.0f, 2180.0f)
    };
    for (int32 Index = 0; Index < UtilityLightLocations.Num(); ++Index)
    {
        const FVector Location = UtilityLightLocations[Index];
        AddBlock(*FString::Printf(TEXT("UtilityFixture%02d"), Index + 1), Metal,
            Location, FVector(260.0f, 110.0f, 70.0f), FRotator::ZeroRotator, ECollisionEnabled::NoCollision);
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(
            FName(*FString::Printf(TEXT("PerimeterUtilityLight%02d"), Index + 1)));
        Light->SetupAttachment(Root);
        Light->SetRelativeLocation(Location - FVector(0.0f, 0.0f, 65.0f));
        Light->SetLightColor(Index % 3 == 0
            ? FLinearColor(1.0f, 0.56f, 0.24f)
            : FLinearColor(0.34f, 0.68f, 0.82f));
        Light->SetIntensity(Index % 3 == 0 ? 7200.0f : 9000.0f);
        // Keep each practical confined to its own maintenance bay. Wide
        // overlapping local-light volumes overflow VSM's per-pixel light list.
        Light->SetAttenuationRadius(1100.0f);
        Light->SetCastShadows(false);
        InteriorLights.Add(Light);
    }

    UStaticMeshComponent* Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CircularCamouflageHatch"));
    Panel->SetupAttachment(Root);
    Panel->SetMobility(EComponentMobility::Movable);
    Panel->SetStaticMesh(CylinderFinder.Object);
    Panel->SetMaterial(0, Grass);
    const FVector Closed(0.0f, 0.0f, RotorlineBellLair::RoofHeightCm - 75.0f);
    const FVector Open(0.0f, RotorlineBellLair::HatchTravelCm, RotorlineBellLair::RoofHeightCm - 75.0f);
    Panel->SetRelativeLocation(Closed);
    Panel->SetRelativeScale3D(FVector(70.0f, 70.0f, 0.9f));
    Panel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    HatchPanels.Add(Panel);
    HatchClosedLocations.Add(Closed);
    HatchOpenLocations.Add(Open);

    PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BellLairHelipad"));
    PadMesh->SetupAttachment(Root);
    PadMesh->SetStaticMesh(PadFinder.Object);
    PadMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 86.0f));
    PadMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    RockRim = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("DisguisedRockRim"));
    RockRim->SetupAttachment(Root);
    RockRim->SetStaticMesh(RockFinder.Object);
    RockRim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RockRim->SetCastShadow(true);
    RockRim->SetCullDistances(0, 250000);

    for (int32 Index = 0; Index < 4; ++Index)
    {
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(
            FName(*FString::Printf(TEXT("InteriorFlood%02d"), Index + 1)));
        Light->SetupAttachment(Root);
        const float X = Index < 2 ? -2500.0f : 2500.0f;
        const float Y = Index % 2 == 0 ? -2500.0f : 2500.0f;
        Light->SetRelativeLocation(FVector(X, Y, 2750.0f));
        Light->SetLightColor(Index % 2 == 0
            ? FLinearColor(0.32f, 0.67f, 0.82f)
            : FLinearColor(0.86f, 0.62f, 0.34f));
        Light->SetIntensity(17000.0f);
        Light->SetAttenuationRadius(5400.0f);
        Light->SetCastShadows(true);
        InteriorLights.Add(Light);
    }

    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / 8.0f;
        const FVector Location(FMath::Cos(Angle) * 1200.0f, FMath::Sin(Angle) * 1200.0f, RotorlineBellLair::RoofHeightCm - 70.0f);
        UStaticMeshComponent* Bulb = CreateDefaultSubobject<UStaticMeshComponent>(
            FName(*FString::Printf(TEXT("WarningBulb%02d"), Index + 1)));
        Bulb->SetupAttachment(Root);
        Bulb->SetStaticMesh(SphereFinder.Object);
        Bulb->SetMaterial(0, AmberFinder.Object);
        Bulb->SetRelativeLocation(Location);
        Bulb->SetRelativeScale3D(FVector(0.18f));
        Bulb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WarningBulbs.Add(Bulb);

        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(
            FName(*FString::Printf(TEXT("WarningLight%02d"), Index + 1)));
        Light->SetupAttachment(Root);
        Light->SetRelativeLocation(Location + FVector(0.0f, 0.0f, 35.0f));
        Light->SetLightColor(FLinearColor(1.0f, 0.15f, 0.02f));
        // Hatch beacons are local fixtures, not room floods. A tight radius
        // keeps all eight from overlapping the same launch-deck pixels.
        Light->SetAttenuationRadius(720.0f);
        Light->SetCastShadows(false);
        WarningLights.Add(Light);
    }
}

#if WITH_EDITOR
void ARotorlineBellLairActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    EnsureEditorPreviewVisible();
}

void ARotorlineBellLairActor::PostLoad()
{
    Super::PostLoad();
    EnsureEditorPreviewVisible();
}

void ARotorlineBellLairActor::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
    EnsureEditorPreviewVisible();
}

void ARotorlineBellLairActor::PostEditUndo()
{
    Super::PostEditUndo();
    EnsureEditorPreviewVisible();
}

void ARotorlineBellLairActor::EnsureEditorPreviewVisible()
{
    const UWorld* World = GetWorld();
    if (!World || World->IsGameWorld())
    {
        return;
    }

    SetIsTemporarilyHiddenInEditor(false);

    TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
    for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
    {
        if (PrimitiveComponent)
        {
            PrimitiveComponent->SetVisibility(true, true);
            PrimitiveComponent->MarkRenderStateDirty();
        }
    }
}
#endif

void ARotorlineBellLairActor::Configure(bool bInNightOperations)
{
    bNightOperations = bInNightOperations;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_BELL_LAIR|SPAWN|location=%.0f,%.0f,%.0f|yaw=%.0f|hatch=CLOSED|night=%d|burial_depth_cm=%.0f|launch_opening_cm=6000|rock_rim=0|terrain_aperture=PHYSICAL"),
        GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z,
        GetActorRotation().Yaw, bNightOperations ? 1 : 0, RotorlineBellLair::RoofHeightCm);
}

void ARotorlineBellLairActor::SetHatchOpen(bool bOpen)
{
    if (bHatchCommandOpen == bOpen) return;
    bHatchCommandOpen = bOpen;
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_BELL_LAIR|HATCH|state=%s"), bOpen ? TEXT("OPENING") : TEXT("CLOSING"));
}

void ARotorlineBellLairActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!PlayerPawn)
    {
        UpdateHatch(DeltaSeconds);
        return;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineLairDressingCapture")))
    {
        DressingCaptureElapsedSeconds += DeltaSeconds;
        static const float LocalYawOffsets[] = {90.0f, 0.0f, -90.0f, 180.0f};
        static const TCHAR* ViewNames[] = {TEXT("North"), TEXT("East"), TEXT("South"), TEXT("West")};
        const float NextCaptureTime = 9.0f + static_cast<float>(DressingCaptureIndex) * 3.0f;
        if (DressingCaptureIndex < UE_ARRAY_COUNT(LocalYawOffsets) &&
            !bDressingCaptureViewPending && DressingCaptureElapsedSeconds >= NextCaptureTime)
        {
            const float TargetYaw = GetActorRotation().Yaw + LocalYawOffsets[DressingCaptureIndex];
            PlayerPawn->SetActorRotation(FRotator(0.0f, TargetYaw, 0.0f), ETeleportType::TeleportPhysics);
            if (AController* PlayerController = PlayerPawn->GetController())
            {
                PlayerController->SetControlRotation(FRotator(0.0f, TargetYaw, 0.0f));
            }
            DressingCaptureRequestTime = DressingCaptureElapsedSeconds + 0.75f;
            bDressingCaptureViewPending = true;
        }
        if (DressingCaptureIndex < UE_ARRAY_COUNT(LocalYawOffsets) && bDressingCaptureViewPending &&
            DressingCaptureElapsedSeconds >= DressingCaptureRequestTime)
        {
            const FString ScreenshotPath = FPaths::Combine(
                FPaths::ProjectSavedDir(),
                FString::Printf(TEXT("Screenshots/HiddenLairDressing_After_%02d_%s.png"),
                    DressingCaptureIndex + 1, ViewNames[DressingCaptureIndex]));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_LAIR_DRESSING|CAPTURE_REQUESTED|view=%s|path=%s"),
                ViewNames[DressingCaptureIndex], *ScreenshotPath);
            ++DressingCaptureIndex;
            bDressingCaptureViewPending = false;
        }
        if (DressingCaptureIndex == UE_ARRAY_COUNT(LocalYawOffsets) && !bDressingCaptureCompleteLogged)
        {
            bDressingCaptureCompleteLogged = true;
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_LAIR_DRESSING|CAPTURE_COMPLETE|views=4|camera=NORMAL_THIRD_PERSON"));
        }
    }

    const ARotorlineHelicopterPawn* HelicopterPawn = Cast<ARotorlineHelicopterPawn>(PlayerPawn);
    const bool bAuthorizedBell =
        HelicopterPawn && HelicopterPawn->IsBellLairAuthorizedAircraft();
    if (!bAuthorizedBell)
    {
        PreviousPlayerZ = PlayerPawn->GetActorLocation().Z;
        SettledCloseTime = 0.0f;
        SetLandscapePassThrough(PlayerPawn, false);
        SetHatchOpen(false);
        UpdateHatch(DeltaSeconds);
        if (!bUnauthorizedAircraftLogged)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT("ROTORLINE_BELL_LAIR|ACCESS_REJECTED|aircraft=%s|required=bell_222x|hatch=CLOSED|landscape_collision=RESTORED"),
                HelicopterPawn ? *HelicopterPawn->GetSelectedAircraftId() : TEXT("NON_HELICOPTER_PAWN"));
            bUnauthorizedAircraftLogged = true;
        }
        return;
    }
    bUnauthorizedAircraftLogged = false;

    const FVector PlayerLocation = PlayerPawn->GetActorLocation();
    const float HorizontalDistance = FVector::Dist2D(PlayerLocation, GetActorLocation());
    const float RelativeZ = PlayerLocation.Z - GetActorLocation().Z;
    const float RisingRate = DeltaSeconds > KINDA_SMALL_NUMBER
        ? (PlayerLocation.Z - PreviousPlayerZ) / DeltaSeconds
        : 0.0f;
    PreviousPlayerZ = PlayerLocation.Z;

    const bool bApproachingFromAbove = HorizontalDistance <= RotorlineBellLair::ApproachRadiusCm &&
        RelativeZ >= RotorlineBellLair::RoofHeightCm - 250.0f &&
        RelativeZ <= RotorlineBellLair::RoofHeightCm + 26000.0f;
    const bool bDepartingFromInside = HorizontalDistance <= 4300.0f &&
        RelativeZ >= 620.0f && RisingRate > 25.0f;
    const bool bInOpenShaft = HorizontalDistance <= 4300.0f &&
        RelativeZ > 900.0f && RelativeZ < RotorlineBellLair::RoofHeightCm + 5000.0f;
    const bool bMustRemainOpen = bApproachingFromAbove || bDepartingFromInside || bInOpenShaft;

    // A cooked Landscape visibility mask can still leave its heightfield as
    // an invisible collision lid. Ignore Landscape movement only while this
    // pawn is centered in the open launch column; terrain collision remains
    // normal everywhere else.
    const bool bInLaunchColumn = HorizontalDistance <= 4300.0f &&
        RelativeZ >= 600.0f && RelativeZ <= RotorlineBellLair::RoofHeightCm + 26000.0f;
    SetLandscapePassThrough(PlayerPawn, bInLaunchColumn && bMustRemainOpen);

    if (bMustRemainOpen)
    {
        SettledCloseTime = 0.0f;
        SetHatchOpen(true);
    }
    else
    {
        const bool bSafelySettledInside = HorizontalDistance <= 4300.0f && RelativeZ < 620.0f;
        const bool bClearOfLair = HorizontalDistance >= RotorlineBellLair::ReleaseRadiusCm;
        if (bSafelySettledInside || bClearOfLair)
        {
            SettledCloseTime += DeltaSeconds;
            if (SettledCloseTime >= 2.5f) SetHatchOpen(false);
        }
    }

    UpdateHatch(DeltaSeconds);
}

void ARotorlineBellLairActor::UpdateHatch(float DeltaSeconds)
{
    const float Target = bHatchCommandOpen ? 1.0f : 0.0f;
    HatchOpenAmount = FMath::FInterpConstantTo(
        HatchOpenAmount, Target, DeltaSeconds, RotorlineBellLair::HatchSpeed);
    const float SmoothAmount = FMath::SmoothStep(0.0f, 1.0f, HatchOpenAmount);
    for (int32 Index = 0; Index < HatchPanels.Num(); ++Index)
    {
        HatchPanels[Index]->SetRelativeLocation(FMath::Lerp(
            HatchClosedLocations[Index], HatchOpenLocations[Index], SmoothAmount));

        // Never leave an invisible collision lid over the launch shaft. The
        // hatch only blocks the opening again after it is fully closed.
        const bool bFullyClosed = !bHatchCommandOpen && HatchOpenAmount <= 0.01f;
        HatchPanels[Index]->SetCollisionEnabled(bFullyClosed
            ? ECollisionEnabled::QueryAndPhysics
            : ECollisionEnabled::NoCollision);
    }

    // The summit must remain visually anonymous; no exposed runway-style
    // warning bulbs or spill lights are allowed around the concealed hatch.
    for (int32 Index = 0; Index < WarningLights.Num(); ++Index)
    {
        WarningLights[Index]->SetIntensity(0.0f);
        WarningBulbs[Index]->SetVisibility(false, true);
    }
    if (!bDressingReadinessLogged)
    {
        FString ClearanceDetail;
        const ARotorlineHiddenLairDressingActor* DressingActor = DressingActorComponent
            ? Cast<ARotorlineHiddenLairDressingActor>(DressingActorComponent->GetChildActor())
            : nullptr;
        const bool bDressingClear = DressingActor && DressingActor->ValidateClearance(ClearanceDetail);
        if (bDressingClear)
        {
            UE_LOG(LogTemp, Display,
                TEXT("ROTORLINE_LAIR_DRESSING|READY|actor=BP_RL_HiddenLair_Dressing|hero_zones=5|gantries=2|pipe_racks=2|ribs=9|tick=0|collision=DECORATION_DISABLED|clearance=PASS|%s"),
                *ClearanceDetail);
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("ROTORLINE_LAIR_DRESSING|READY|actor=BP_RL_HiddenLair_Dressing|hero_zones=5|gantries=2|pipe_racks=2|ribs=9|tick=0|collision=DECORATION_DISABLED|clearance=FAIL|%s"),
                *ClearanceDetail);
        }
        bDressingReadinessLogged = true;
    }
}

void ARotorlineBellLairActor::SetLandscapePassThrough(APawn* PlayerPawn, bool bEnable)
{
    if (!PlayerPawn || !GetWorld())
    {
        return;
    }

    if (bEnable && bLandscapePassThroughEnabled && LandscapePassThroughPawn.Get() == PlayerPawn)
    {
        return;
    }
    if (!bEnable && !bLandscapePassThroughEnabled)
    {
        return;
    }

    APawn* PreviousPassThroughPawn = LandscapePassThroughPawn.Get();
    for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
    {
        if (PreviousPassThroughPawn)
        {
            PreviousPassThroughPawn->MoveIgnoreActorRemove(*It);
        }
        if (bEnable)
        {
            PlayerPawn->MoveIgnoreActorAdd(*It);
        }
    }
    bLandscapePassThroughEnabled = bEnable;
    if (bEnable)
    {
        LandscapePassThroughPawn = PlayerPawn;
    }
    else
    {
        LandscapePassThroughPawn.Reset();
    }
    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_BELL_LAIR|SHAFT_COLLISION|landscape=%s"),
        bEnable ? TEXT("IGNORED_FOR_PLAYER") : TEXT("RESTORED_FOR_PLAYER"));
}
