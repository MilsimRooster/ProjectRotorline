#include "RotorlineRoadNetworkActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

namespace RotorlineRoads
{
    void Configure(
        UHierarchicalInstancedStaticMeshComponent* Component,
        UStaticMesh* Mesh,
        UMaterialInterface* Material,
        const bool bCollision,
        const bool bCastShadow,
        const int32 CullDistance)
    {
        Component->SetStaticMesh(Mesh);
        Component->SetMobility(EComponentMobility::Static);
        Component->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Component->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
        Component->SetCastShadow(bCastShadow);
        Component->bCastDynamicShadow = bCastShadow;
        Component->bCastContactShadow = false;
        Component->SetCullDistances(0, CullDistance);
        Component->bEnableDensityScaling = false;
        if (Material)
        {
            Component->SetMaterial(0, Material);
        }
    }
}

ARotorlineRoadNetworkActor::ARotorlineRoadNetworkActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetMobility(EComponentMobility::Static);
    RootComponent = SceneRoot;

    Shoulders = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Shoulders"));
    Pavement = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Pavement"));
    Centerlines = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Centerlines"));
    EdgeLines = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("EdgeLines"));
    Ditches = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Ditches"));
    Guardrails = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Guardrails"));
    Culverts = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Culverts"));
    Reflectors = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Reflectors"));

    for (UHierarchicalInstancedStaticMeshComponent* Component : {
        Shoulders.Get(), Pavement.Get(), Centerlines.Get(), EdgeLines.Get(),
        Ditches.Get(), Guardrails.Get(), Culverts.Get(), Reflectors.Get()})
    {
        Component->SetupAttachment(SceneRoot);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShoulderFinder(
        TEXT("/Game/Environment/Materials/Roads/M_RoadShoulder_Visibility.M_RoadShoulder_Visibility"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AsphaltFinder(
        TEXT("/Game/Environment/Materials/Roads/M_RoadAsphalt_Visibility.M_RoadAsphalt_Visibility"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> YellowFinder(
        TEXT("/Game/Environment/Materials/Roads/M_RoadCenterline_Visibility.M_RoadCenterline_Visibility"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteFinder(
        TEXT("/Game/Environment/Materials/Roads/M_RoadEdgeLine_Visibility.M_RoadEdgeLine_Visibility"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SteelFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Hangar_BlueSteel.M_Hangar_BlueSteel"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ConcreteFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Concrete.M_Concrete"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ReflectorFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Road_Reflector_Final.M_Road_Reflector_Final"));

    RotorlineRoads::Configure(Shoulders, CubeFinder.Object, ShoulderFinder.Object, true, false, 700000);
    RotorlineRoads::Configure(Pavement, CubeFinder.Object, AsphaltFinder.Object, true, false, 700000);
    RotorlineRoads::Configure(Centerlines, CubeFinder.Object, YellowFinder.Object, false, false, 500000);
    RotorlineRoads::Configure(EdgeLines, CubeFinder.Object, WhiteFinder.Object, false, false, 500000);
    RotorlineRoads::Configure(Ditches, CubeFinder.Object, ShoulderFinder.Object, false, false, 350000);
    RotorlineRoads::Configure(Guardrails, CubeFinder.Object, SteelFinder.Object, true, true, 400000);
    RotorlineRoads::Configure(Culverts, CylinderFinder.Object, ConcreteFinder.Object, true, true, 300000);
    RotorlineRoads::Configure(Reflectors, CubeFinder.Object, ReflectorFinder.Object, false, false, 220000);
}

void ARotorlineRoadNetworkActor::BeginPlay()
{
    Super::BeginPlay();

    // World Partition actors can retain component templates from an older
    // Blueprint package even after the native constructor changes. Enforce
    // the shipped road surface and regenerate the paint from the serialized
    // pavement transforms in the actual game world.
    ApplyProductionMaterials();
    BuildRuntimeMarkingsFromPavement();
    RebuildRoadInstances();

    if (FParse::Param(FCommandLine::Get(), TEXT("RotorlineEnvironmentRuntimeAudit")))
    {
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_ROAD_RUNTIME|%s|pavement=%d|center=%d|edges=%d|reflectors=%d|material=%s"),
            *GetActorNameOrLabel(), PavementInstances.Num(), CenterlineInstances.Num(),
            EdgeLineInstances.Num(), ReflectorInstances.Num(),
            Pavement && Pavement->GetMaterial(0) ? *Pavement->GetMaterial(0)->GetPathName() : TEXT("NONE"));
    }
}

void ARotorlineRoadNetworkActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildRoadInstances();
}

void ARotorlineRoadNetworkActor::ApplyProductionMaterials()
{
    struct FMaterialBinding
    {
        UHierarchicalInstancedStaticMeshComponent* Component;
        const TCHAR* Path;
    };

    const FMaterialBinding Bindings[] = {
        {Shoulders, TEXT("/Game/Environment/Materials/Roads/M_RoadShoulder_Visibility.M_RoadShoulder_Visibility")},
        {Pavement, TEXT("/Game/Environment/Materials/Roads/M_RoadAsphalt_Visibility.M_RoadAsphalt_Visibility")},
        {Centerlines, TEXT("/Game/Environment/Materials/Roads/M_RoadCenterline_Visibility.M_RoadCenterline_Visibility")},
        {EdgeLines, TEXT("/Game/Environment/Materials/Roads/M_RoadEdgeLine_Visibility.M_RoadEdgeLine_Visibility")},
        {Ditches, TEXT("/Game/Environment/Materials/Urban/M_Road_Shoulder_Final.M_Road_Shoulder_Final")},
        {Reflectors, TEXT("/Game/Environment/Materials/Urban/M_Road_Reflector_Final.M_Road_Reflector_Final")},
    };

    for (const FMaterialBinding& Binding : Bindings)
    {
        if (Binding.Component)
        {
            if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Binding.Path))
            {
                Binding.Component->SetMaterial(0, Material);
            }
        }
    }
}

void ARotorlineRoadNetworkActor::BuildRuntimeMarkingsFromPavement()
{
    CenterlineInstances.Reset(PavementInstances.Num());
    EdgeLineInstances.Reset(PavementInstances.Num() * 2);
    ReflectorInstances.Reset(PavementInstances.Num());

    for (int32 Index = 0; Index < PavementInstances.Num(); ++Index)
    {
        const FTransform& Road = PavementInstances[Index];
        const FVector RoadScale = Road.GetScale3D();
        const FVector UpOffset = Road.GetRotation().RotateVector(
            FVector(0.0f, 0.0f, FMath::Abs(RoadScale.Z) * 50.0f + 6.0f));
        const FVector SideDirection = Road.GetRotation().RotateVector(FVector::RightVector);
        const float SideOffsetCm = FMath::Max(180.0f, FMath::Abs(RoadScale.Y) * 50.0f - 42.0f);

        // Each source pavement slab becomes one clearly separated yellow dash.
        FTransform Center = Road;
        Center.SetLocation(Road.GetLocation() + UpOffset);
        Center.SetScale3D(FVector(FMath::Max(0.8f, FMath::Abs(RoadScale.X) * 0.42f), 0.24f, 0.045f));
        CenterlineInstances.Add(Center);

        for (const float Side : {-1.0f, 1.0f})
        {
            FTransform Edge = Road;
            Edge.SetLocation(Road.GetLocation() + UpOffset + SideDirection * SideOffsetCm * Side);
            Edge.SetScale3D(FVector(FMath::Max(0.8f, FMath::Abs(RoadScale.X) * 0.96f), 0.18f, 0.04f));
            EdgeLineInstances.Add(Edge);

            if ((Index % 2) == 0)
            {
                FTransform Reflector = Road;
                Reflector.SetLocation(Road.GetLocation() + UpOffset + SideDirection * (SideOffsetCm + 18.0f) * Side);
                Reflector.SetScale3D(FVector(0.13f, 0.07f, 0.055f));
                ReflectorInstances.Add(Reflector);
            }
        }
    }
}

void ARotorlineRoadNetworkActor::Populate(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const TArray<FTransform>& Instances)
{
    if (!Component)
    {
        return;
    }
    Component->ClearInstances();
    Component->PreAllocateInstancesMemory(Instances.Num());
    for (const FTransform& Instance : Instances)
    {
        Component->AddInstance(Instance, false);
    }
    Component->BuildTreeIfOutdated(true, true);
}

void ARotorlineRoadNetworkActor::RebuildRoadInstances()
{
    Populate(Shoulders, ShoulderInstances);
    Populate(Pavement, PavementInstances);
    Populate(Centerlines, CenterlineInstances);
    Populate(EdgeLines, EdgeLineInstances);
    Populate(Ditches, DitchInstances);
    Populate(Guardrails, GuardrailInstances);
    Populate(Culverts, CulvertInstances);
    Populate(Reflectors, ReflectorInstances);
}
