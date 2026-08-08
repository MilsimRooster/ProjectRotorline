#include "RotorlineEnemyIslandActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float IslandTopZ = 1200.0f;

    void ConfigureInstancer(
        UHierarchicalInstancedStaticMeshComponent* Component,
        UStaticMesh* Mesh,
        UMaterialInterface* Material,
        bool bCollision,
        bool bCastShadow,
        int32 EndCullDistance)
    {
        if (!Component) return;
        Component->SetStaticMesh(Mesh);
        if (Material)
        {
            Component->SetMaterial(0, Material);
        }
        Component->SetCollisionEnabled(
            bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Component->SetCollisionProfileName(
            bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCastShadow(bCastShadow);
        Component->SetCullDistances(0, EndCullDistance);
    }

    FTransform BoxTransform(
        const FVector& Location,
        const FVector& DimensionsCm,
        const FRotator& Rotation = FRotator::ZeroRotator)
    {
        return FTransform(Rotation, Location, DimensionsCm / 100.0f);
    }
}

ARotorlineEnemyIslandActor::ARotorlineEnemyIslandActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);
    Tags.Add(TEXT("RotorlineEnemyIsland"));
    Tags.Add(TEXT("RotorlineRuntimePopulation"));

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("EnemyIslandRoot"));
    RootComponent = Root;
    Root->SetMobility(EComponentMobility::Static);

    IslandMasses = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("IslandMasses"));
    IslandMasses->SetupAttachment(Root);
    IslandTopMasses = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("IslandTopMasses"));
    IslandTopMasses->SetupAttachment(Root);
    RunwaySurfaces = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RunwaySurfaces"));
    RunwaySurfaces->SetupAttachment(Root);
    RunwayMarkings = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RunwayMarkings"));
    RunwayMarkings->SetupAttachment(Root);
    Structures = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("EnemyBaseStructures"));
    Structures->SetupAttachment(Root);
    FuelTanks = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("FuelTanks"));
    FuelTanks->SetupAttachment(Root);
    RockRim = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("IslandRockRim"));
    RockRim->SetupAttachment(Root);
    Grass = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("IslandGrass"));
    Grass->SetupAttachment(Root);
    ParkedTankBodies = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ParkedTankBodies"));
    ParkedTankBodies->SetupAttachment(Root);
    ParkedTankTurrets = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ParkedTankTurrets"));
    ParkedTankTurrets->SetupAttachment(Root);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RockFinder(
        TEXT("/Game/Environment/Imported/StandaloneRocks/low-poly-sculpted-boulder/StaticMeshes/low-poly-sculpted-boulder.low-poly-sculpted-boulder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassFinder(
        TEXT("/Game/Environment/Nature/Grass/GeneratedMesh/SM_GrassClump/StaticMeshes/SM_GrassClump.SM_GrassClump"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TankBodyFinder(
        TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyChallengerMk3_Body/EnemyChallengerMk3_Body/StaticMeshes/EnemyChallengerMk3_Body.EnemyChallengerMk3_Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> TankTurretFinder(
        TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyChallengerMk3_Turret/EnemyChallengerMk3_Turret/StaticMeshes/EnemyChallengerMk3_Turret.EnemyChallengerMk3_Turret"));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> OliveFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Landmark_AirfieldOlive.M_Landmark_AirfieldOlive"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShoulderFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Road_Shoulder.M_Road_Shoulder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ApronFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Airfield_Apron_Final.M_Airfield_Apron_Final"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SteelFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Hangar_BlueSteel.M_Hangar_BlueSteel"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Marking_White.M_Marking_White"));

    ConfigureInstancer(IslandMasses, CylinderFinder.Object, ShoulderFinder.Object, true, true, 550000);
    ConfigureInstancer(IslandTopMasses, CylinderFinder.Object, OliveFinder.Object, true, true, 550000);
    ConfigureInstancer(RunwaySurfaces, CubeFinder.Object, ApronFinder.Object, true, true, 500000);
    ConfigureInstancer(RunwayMarkings, CubeFinder.Object, WhiteFinder.Object, false, false, 400000);
    ConfigureInstancer(Structures, CubeFinder.Object, SteelFinder.Object, true, true, 450000);
    ConfigureInstancer(FuelTanks, CylinderFinder.Object, OliveFinder.Object, true, true, 400000);
    ConfigureInstancer(RockRim, RockFinder.Object, nullptr, false, true, 450000);
    ConfigureInstancer(Grass, GrassFinder.Object, nullptr, false, false, 180000);
    ConfigureInstancer(ParkedTankBodies, TankBodyFinder.Object, nullptr, false, true, 350000);
    ConfigureInstancer(ParkedTankTurrets, TankTurretFinder.Object, nullptr, false, true, 350000);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ApacheBodyFinder(
        TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyApacheMk1_Body/EnemyApacheMk1_Body/StaticMeshes/EnemyApacheMk1_Body.EnemyApacheMk1_Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ApacheMainRotorFinder(
        TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyApacheMk1_MainRotor/EnemyApacheMk1_MainRotor/StaticMeshes/EnemyApacheMk1_MainRotor.EnemyApacheMk1_MainRotor"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ApacheTailRotorFinder(
        TEXT("/Game/Vehicles/Hostile/CombatReady/EnemyApacheMk1_TailRotor/EnemyApacheMk1_TailRotor/StaticMeshes/EnemyApacheMk1_TailRotor.EnemyApacheMk1_TailRotor"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> HindBodyFinder(
        TEXT("/Game/Vehicles/Hostile/EnemyHind_Body/EnemyHind_Body/StaticMeshes/EnemyHind_Body.EnemyHind_Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> HindMainRotorFinder(
        TEXT("/Game/Vehicles/Hostile/EnemyHind_MainRotor/EnemyHind_MainRotor/StaticMeshes/EnemyHind_MainRotor.EnemyHind_MainRotor"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MdBodyFinder(
        TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/StaticMeshes/Helicopter_Helicopter_Material_0.Helicopter_Helicopter_Material_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MdCockpitFinder(
        TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/StaticMeshes/Cockpit_Cockpit_Material_0.Cockpit_Cockpit_Material_0"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MdGlassFinder(
        TEXT("/Game/Vehicles/Playable/MD500/md-500_defender_helicopter/StaticMeshes/Glass_Glass_Material_0.Glass_Glass_Material_0"));

    const auto AddParkedAircraftPart = [this](
        const TCHAR* Name,
        UStaticMesh* Mesh,
        const FVector& Location,
        const FVector& Scale)
    {
        if (!Mesh) return;
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(FName(Name));
        Part->SetupAttachment(Root);
        Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCastShadow(true);
        ParkedAircraftParts.Add(Part);
    };

    const FVector ApacheLocation(-10500.0f, -6500.0f, IslandTopZ + 90.0f);
    AddParkedAircraftPart(TEXT("ParkedApacheBody"), ApacheBodyFinder.Object, ApacheLocation, FVector(1.0f));
    AddParkedAircraftPart(TEXT("ParkedApacheMainRotor"), ApacheMainRotorFinder.Object, ApacheLocation, FVector(1.0f));
    AddParkedAircraftPart(TEXT("ParkedApacheTailRotor"), ApacheTailRotorFinder.Object, ApacheLocation, FVector(1.0f));

    const FVector HindLocation(0.0f, -6500.0f, IslandTopZ + 90.0f);
    AddParkedAircraftPart(TEXT("ParkedHindBody"), HindBodyFinder.Object, HindLocation, FVector(0.01f));
    AddParkedAircraftPart(TEXT("ParkedHindMainRotor"), HindMainRotorFinder.Object, HindLocation, FVector(0.01f));

    const FVector MdLocation(10500.0f, -6500.0f, IslandTopZ + 90.0f);
    AddParkedAircraftPart(TEXT("ParkedMdBody"), MdBodyFinder.Object, MdLocation, FVector(1.0f));
    AddParkedAircraftPart(TEXT("ParkedMdCockpit"), MdCockpitFinder.Object, MdLocation, FVector(1.0f));
    AddParkedAircraftPart(TEXT("ParkedMdGlass"), MdGlassFinder.Object, MdLocation, FVector(1.0f));
}

void ARotorlineEnemyIslandActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BuildIsland();
}

void ARotorlineEnemyIslandActor::BeginPlay()
{
    Super::BeginPlay();
    BuildIsland();
}

void ARotorlineEnemyIslandActor::BuildIsland()
{
    IslandMasses->ClearInstances();
    IslandTopMasses->ClearInstances();
    RunwaySurfaces->ClearInstances();
    RunwayMarkings->ClearInstances();
    Structures->ClearInstances();
    FuelTanks->ClearInstances();
    RockRim->ClearInstances();
    Grass->ClearInstances();
    ParkedTankBodies->ClearInstances();
    ParkedTankTurrets->ClearInstances();

    for (UStaticMeshComponent* Part : ParkedAircraftParts)
    {
        if (!Part) continue;
        Part->SetVisibility(bGenerateProceduralVisuals, true);
        Part->SetHiddenInGame(!bGenerateProceduralVisuals);
    }

    if (!bGenerateProceduralVisuals)
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_ENEMY_ISLAND|EDITOR_LAYOUT|state=ACTIVE|procedural_visuals=DISABLED"));
        return;
    }

    // A broad beach shelf and three progressively smaller dirt/rock terraces
    // replace the old single vertical cylinder. The staggered lobes break the
    // silhouette while preserving a reliable 12 m high airfield surface.
    IslandMasses->AddInstance(FTransform(
        FRotator(0.0f, 4.0f, 0.0f), FVector(0.0f, 0.0f, -460.0f), FVector(760.0f, 560.0f, 10.8f)));
    IslandMasses->AddInstance(FTransform(
        FRotator(0.0f, -7.0f, 0.0f), FVector(-1500.0f, 800.0f, 110.0f), FVector(730.0f, 535.0f, 5.8f)));
    IslandMasses->AddInstance(FTransform(
        FRotator(0.0f, 8.0f, 0.0f), FVector(900.0f, -700.0f, 460.0f), FVector(690.0f, 500.0f, 6.0f)));
    IslandMasses->AddInstance(FTransform(
        FRotator(0.0f, -5.0f, 0.0f), FVector(-700.0f, 500.0f, 825.0f), FVector(650.0f, 465.0f, 7.5f)));

    // Thin, same-height overlapping caps produce an uneven natural perimeter
    // without tilting the runway, buildings, or combat grounding surface.
    IslandTopMasses->AddInstance(FTransform(
        FRotator(0.0f, 2.0f, 0.0f), FVector(0.0f, 0.0f, 1150.0f), FVector(620.0f, 425.0f, 1.0f)));
    IslandTopMasses->AddInstance(FTransform(
        FRotator(0.0f, -14.0f, 0.0f), FVector(-24500.0f, 7600.0f, 1150.0f), FVector(190.0f, 175.0f, 1.0f)));
    IslandTopMasses->AddInstance(FTransform(
        FRotator(0.0f, 19.0f, 0.0f), FVector(23500.0f, -8500.0f, 1150.0f), FVector(185.0f, 165.0f, 1.0f)));
    IslandTopMasses->AddInstance(FTransform(
        FRotator(0.0f, 31.0f, 0.0f), FVector(18500.0f, 13500.0f, 1150.0f), FVector(150.0f, 135.0f, 1.0f)));
    IslandTopMasses->AddInstance(FTransform(
        FRotator(0.0f, -26.0f, 0.0f), FVector(-18000.0f, -14000.0f, 1150.0f), FVector(155.0f, 130.0f, 1.0f)));

    RunwaySurfaces->AddInstance(BoxTransform(
        FVector(0.0f, 0.0f, IslandTopZ + 40.0f), FVector(50000.0f, 4200.0f, 80.0f)));
    RunwaySurfaces->AddInstance(BoxTransform(
        FVector(-2500.0f, 6100.0f, IslandTopZ + 35.0f), FVector(25000.0f, 9000.0f, 70.0f)));
    RunwaySurfaces->AddInstance(BoxTransform(
        FVector(0.0f, -6500.0f, IslandTopZ + 35.0f), FVector(32000.0f, 8000.0f, 70.0f)));

    for (int32 Dash = -10; Dash <= 10; ++Dash)
    {
        RunwayMarkings->AddInstance(BoxTransform(
            FVector(Dash * 2200.0f, 0.0f, IslandTopZ + 83.0f),
            FVector(1100.0f, 70.0f, 8.0f)));
    }
    for (float Side : { -1.0f, 1.0f })
    {
        for (int32 Stripe = -2; Stripe <= 2; ++Stripe)
        {
            RunwayMarkings->AddInstance(BoxTransform(
                FVector(Side * 23800.0f, Stripe * 540.0f, IslandTopZ + 84.0f),
                FVector(1500.0f, 190.0f, 8.0f)));
        }
    }

    const struct FBuildingSpec
    {
        FVector Location;
        FVector Dimensions;
        float Yaw;
    } Buildings[] = {
        { FVector(-10500.0f, 7800.0f, IslandTopZ + 850.0f), FVector(8500.0f, 4300.0f, 1700.0f), 0.0f },
        { FVector(0.0f, 8200.0f, IslandTopZ + 900.0f), FVector(9000.0f, 4600.0f, 1800.0f), 0.0f },
        { FVector(11000.0f, 7900.0f, IslandTopZ + 800.0f), FVector(7600.0f, 4200.0f, 1600.0f), 0.0f },
        { FVector(-17500.0f, -11500.0f, IslandTopZ + 500.0f), FVector(4200.0f, 3000.0f, 1000.0f), 12.0f },
        { FVector(-10500.0f, -12500.0f, IslandTopZ + 450.0f), FVector(3600.0f, 2600.0f, 900.0f), -8.0f },
        { FVector(17000.0f, 11800.0f, IslandTopZ + 650.0f), FVector(5200.0f, 3500.0f, 1300.0f), -18.0f }
    };
    for (const FBuildingSpec& Building : Buildings)
    {
        Structures->AddInstance(BoxTransform(
            Building.Location, Building.Dimensions, FRotator(0.0f, Building.Yaw, 0.0f)));
    }

    for (int32 TankIndex = 0; TankIndex < 8; ++TankIndex)
    {
        const int32 Row = TankIndex / 4;
        const int32 Column = TankIndex % 4;
        const FVector TankLocation(
            -10500.0f + Column * 5200.0f,
            14200.0f + Row * 3000.0f,
            IslandTopZ - 20.0f);
        const FRotator TankRotation(0.0f, Row == 0 ? -12.0f : 168.0f, 0.0f);
        const FTransform TankTransform(TankRotation, TankLocation, FVector(1.0f));
        ParkedTankBodies->AddInstance(TankTransform);
        ParkedTankTurrets->AddInstance(TankTransform);
    }

    for (const FVector2D FuelLocation : {
        FVector2D(12000.0f, -12500.0f),
        FVector2D(15000.0f, -12500.0f),
        FVector2D(18000.0f, -12500.0f),
        FVector2D(13500.0f, -15800.0f),
        FVector2D(16500.0f, -15800.0f) })
    {
        FuelTanks->AddInstance(FTransform(
            FRotator::ZeroRotator,
            FVector(FuelLocation.X, FuelLocation.Y, IslandTopZ + 500.0f),
            FVector(12.0f, 12.0f, 10.0f)));
    }

    FRandomStream Random(22026);
    for (int32 RockIndex = 0; RockIndex < 64; ++RockIndex)
    {
        const float Angle = (2.0f * PI * RockIndex / 64.0f) + Random.FRandRange(-0.035f, 0.035f);
        const float Radius = Random.FRandRange(0.92f, 1.04f);
        const FVector Location(
            FMath::Cos(Angle) * 30700.0f * Radius,
            FMath::Sin(Angle) * 20700.0f * Radius,
            IslandTopZ - Random.FRandRange(160.0f, 340.0f));
        const FVector Scale(
            Random.FRandRange(1.6f, 3.8f),
            Random.FRandRange(1.5f, 3.4f),
            Random.FRandRange(1.3f, 3.0f));
        RockRim->AddInstance(FTransform(
            FRotator(Random.FRandRange(-8.0f, 8.0f), Random.FRandRange(0.0f, 360.0f), Random.FRandRange(-8.0f, 8.0f)),
            Location,
            Scale));
    }

    for (int32 RockIndex = 0; RockIndex < 52; ++RockIndex)
    {
        const float Angle = (2.0f * PI * RockIndex / 52.0f) + Random.FRandRange(-0.05f, 0.05f);
        const float Radius = Random.FRandRange(0.93f, 1.05f);
        const FVector Location(
            FMath::Cos(Angle) * 34500.0f * Radius,
            FMath::Sin(Angle) * 24700.0f * Radius,
            Random.FRandRange(260.0f, 680.0f));
        const FVector Scale(
            Random.FRandRange(2.0f, 4.6f),
            Random.FRandRange(1.8f, 4.0f),
            Random.FRandRange(1.7f, 3.7f));
        RockRim->AddInstance(FTransform(
            FRotator(Random.FRandRange(-12.0f, 12.0f), Random.FRandRange(0.0f, 360.0f), Random.FRandRange(-12.0f, 12.0f)),
            Location,
            Scale));
    }

    for (int32 GrassIndex = 0; GrassIndex < 150; ++GrassIndex)
    {
        const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
        const float Radius = FMath::Sqrt(Random.FRandRange(0.0f, 1.0f));
        const FVector Location(
            FMath::Cos(Angle) * 29500.0f * Radius,
            FMath::Sin(Angle) * 19500.0f * Radius,
            IslandTopZ - 8.0f);
        const bool bOnRunway = FMath::Abs(Location.X) < 26000.0f && FMath::Abs(Location.Y) < 2800.0f;
        const bool bOnNorthApron = Location.X > -16000.0f && Location.X < 14000.0f &&
            Location.Y > 3000.0f && Location.Y < 13000.0f;
        const bool bOnSouthApron = FMath::Abs(Location.X) < 17000.0f &&
            Location.Y > -11000.0f && Location.Y < -2200.0f;
        if (bOnRunway || bOnNorthApron || bOnSouthApron) continue;
        const float Scale = Random.FRandRange(0.8f, 1.8f);
        Grass->AddInstance(FTransform(
            FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
            Location,
            FVector(Scale)));
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_ENEMY_ISLAND|READY|center=(%.0f,%.0f,%.0f)|top_z=%.0f|runway_m=500|aircraft=3|tanks=8|rocks=%d|grass=%d"),
        GetActorLocation().X,
        GetActorLocation().Y,
        GetActorLocation().Z,
        GetActorLocation().Z + IslandTopZ,
        RockRim->GetInstanceCount(),
        Grass->GetInstanceCount());
}
