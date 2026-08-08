#include "RotorlineCabinSupplyConvoyActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RotorlineHelicopterPawn.h"
#include "RotorlineMissionCatalog.h"
#include "RotorlineMissionObjectiveActor.h"
#include "RotorlineRoadNetworkActor.h"

namespace RotorlineCabinConvoy
{
    const FVector AirfieldAssembly(-236194.0, -193028.0, 0.0);
    const FVector SurveyedWarehouseRoadside(192243.6, 131234.1, 0.0);
    constexpr float EndpointJoinDistanceCm = 6500.0f;
    constexpr int32 SupplyTruckCount = 4;
    const TCHAR* UralAsset = TEXT("/Game/Vehicles/UserAdded/CombatReady/Ural4320_Body/Ural4320_Body/StaticMeshes/Ural4320_Body.Ural4320_Body");
    const TCHAR* MrapBodyAsset = TEXT("/Game/Vehicles/UserAdded/CombatReady/MilitaryTruck_Body/MilitaryTruck_Body/StaticMeshes/MilitaryTruck_Body.MilitaryTruck_Body");
    const TCHAR* MrapTurretAsset = TEXT("/Game/Vehicles/UserAdded/CombatReady/MilitaryTruck_Turret/MilitaryTruck_Turret/StaticMeshes/MilitaryTruck_Turret.MilitaryTruck_Turret");
}

ARotorlineCabinSupplyConvoyActor::ARotorlineCabinSupplyConvoyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
}

bool ARotorlineCabinSupplyConvoyActor::ConfigureAndStart(ARotorlineHelicopterPawn* InPlayer)
{
    Player = InPlayer;
    if (!BuildProductionRoadRoute())
    {
        bMissionFailed = true;
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_M23_CONVOY|ROUTE_FAILED|source=PRODUCTION_PAVEMENT"));
        return false;
    }

    SpawnSupplyColumn();
    bStarted = VehicleRoots.Num() == 6;
    bMissionFailed = !bStarted;
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_M23_CONVOY|DEPLOYED|vehicles=%d|supply_trucks=%d|route_km=%.2f|speed_kmh=%.1f|destination=CABIN"),
        VehicleRoots.Num(), RotorlineCabinConvoy::SupplyTruckCount,
        RouteLengthCm / 100000.0f, ConvoySpeedCmPerSecond * 0.036f);
    return bStarted;
}

bool ARotorlineCabinSupplyConvoyActor::BuildProductionRoadRoute()
{
    struct FNode
    {
        FVector Location = FVector::ZeroVector;
        TArray<TPair<int32, float>> Links;
    };

    TArray<FNode> Nodes;
    if (!GetWorld()) return false;

    for (TActorIterator<ARotorlineRoadNetworkActor> It(GetWorld()); It; ++It)
    {
        ARotorlineRoadNetworkActor* RoadActor = *It;
        if (!IsValid(RoadActor)) continue;
        for (const FTransform& LocalRoad : RoadActor->PavementInstances)
        {
            const FTransform WorldRoad = LocalRoad * RoadActor->GetActorTransform();
            const float HalfLength = FMath::Max(100.0f, FMath::Abs(WorldRoad.GetScale3D().X) * 50.0f);
            const FVector Along = WorldRoad.GetRotation().RotateVector(FVector::ForwardVector);
            const int32 A = Nodes.AddDefaulted();
            const int32 B = Nodes.AddDefaulted();
            Nodes[A].Location = WorldRoad.GetLocation() - Along * HalfLength;
            Nodes[B].Location = WorldRoad.GetLocation() + Along * HalfLength;
            Nodes[A].Links.Add({B, HalfLength * 2.0f});
            Nodes[B].Links.Add({A, HalfLength * 2.0f});
        }
    }
    if (Nodes.Num() < 4) return false;

    const float JoinDistanceSq = FMath::Square(RotorlineCabinConvoy::EndpointJoinDistanceCm);
    for (int32 A = 0; A < Nodes.Num(); ++A)
    {
        for (int32 B = A + 1; B < Nodes.Num(); ++B)
        {
            const float DistanceSq = FVector::DistSquared2D(Nodes[A].Location, Nodes[B].Location);
            if (DistanceSq <= JoinDistanceSq)
            {
                const float Distance = FMath::Sqrt(DistanceSq);
                Nodes[A].Links.Add({B, Distance});
                Nodes[B].Links.Add({A, Distance});
            }
        }
    }

    auto FindNearestNode = [&Nodes](const FVector& Point)
    {
        int32 Best = INDEX_NONE;
        double BestDistanceSq = TNumericLimits<double>::Max();
        for (int32 Index = 0; Index < Nodes.Num(); ++Index)
        {
            const double DistanceSq = FVector::DistSquared2D(Point, Nodes[Index].Location);
            if (DistanceSq < BestDistanceSq)
            {
                BestDistanceSq = DistanceSq;
                Best = Index;
            }
        }
        return Best;
    };

    const int32 StartNode = FindNearestNode(RotorlineCabinConvoy::AirfieldAssembly);
    const int32 GoalNode = FindNearestNode(RotorlineCabinConvoy::SurveyedWarehouseRoadside);
    if (StartNode == INDEX_NONE || GoalNode == INDEX_NONE) return false;

    TArray<float> Cost;
    TArray<int32> Previous;
    TArray<bool> Visited;
    Cost.Init(TNumericLimits<float>::Max(), Nodes.Num());
    Previous.Init(INDEX_NONE, Nodes.Num());
    Visited.Init(false, Nodes.Num());
    Cost[StartNode] = 0.0f;

    for (int32 Pass = 0; Pass < Nodes.Num(); ++Pass)
    {
        int32 Current = INDEX_NONE;
        float CurrentCost = TNumericLimits<float>::Max();
        for (int32 Index = 0; Index < Nodes.Num(); ++Index)
        {
            if (!Visited[Index] && Cost[Index] < CurrentCost)
            {
                Current = Index;
                CurrentCost = Cost[Index];
            }
        }
        if (Current == INDEX_NONE || Current == GoalNode) break;
        Visited[Current] = true;
        for (const TPair<int32, float>& Link : Nodes[Current].Links)
        {
            const float Candidate = CurrentCost + Link.Value;
            if (Candidate < Cost[Link.Key])
            {
                Cost[Link.Key] = Candidate;
                Previous[Link.Key] = Current;
            }
        }
    }
    if (Previous[GoalNode] == INDEX_NONE && GoalNode != StartNode) return false;

    TArray<int32> ReversePath;
    for (int32 Node = GoalNode; Node != INDEX_NONE; Node = Previous[Node])
    {
        ReversePath.Add(Node);
        if (Node == StartNode) break;
    }
    if (ReversePath.IsEmpty() || ReversePath.Last() != StartNode) return false;

    RoutePoints.Reset();
    for (int32 Index = ReversePath.Num() - 1; Index >= 0; --Index)
    {
        RoutePoints.Add(Nodes[ReversePath[Index]].Location);
    }

    RouteDistances.Reset();
    RouteDistances.Add(0.0f);
    RouteLengthCm = 0.0f;
    for (int32 Index = 1; Index < RoutePoints.Num(); ++Index)
    {
        RouteLengthCm += FVector::Dist2D(RoutePoints[Index - 1], RoutePoints[Index]);
        RouteDistances.Add(RouteLengthCm);
    }
    return RoutePoints.Num() >= 2 && RouteLengthCm > 100000.0f;
}

USceneComponent* ARotorlineCabinSupplyConvoyActor::CreateVehicleRoot(const FString& Name)
{
    USceneComponent* Component = NewObject<USceneComponent>(this, *Name);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetupAttachment(SceneRoot);
    Component->RegisterComponent();
    VehicleRoots.Add(Component);
    return Component;
}

void ARotorlineCabinSupplyConvoyActor::AddVehicleMesh(
    USceneComponent* Parent, const FString& Name, const TCHAR* AssetPath)
{
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, *Name);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetupAttachment(Parent);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, AssetPath));
    Component->RegisterComponent();
    VehicleMeshes.Add(Component);
}

void ARotorlineCabinSupplyConvoyActor::SpawnSupplyColumn()
{
    USceneComponent* Lead = CreateVehicleRoot(TEXT("M23_00_LeadEscort"));
    AddVehicleMesh(Lead, TEXT("M23_00_LeadBody"), RotorlineCabinConvoy::MrapBodyAsset);
    AddVehicleMesh(Lead, TEXT("M23_00_LeadTurret"), RotorlineCabinConvoy::MrapTurretAsset);

    for (int32 Index = 1; Index <= RotorlineCabinConvoy::SupplyTruckCount; ++Index)
    {
        USceneComponent* Truck = CreateVehicleRoot(FString::Printf(TEXT("M23_%02d_SupplyUral"), Index));
        AddVehicleMesh(Truck, FString::Printf(TEXT("M23_%02d_SupplyBody"), Index), RotorlineCabinConvoy::UralAsset);
    }

    USceneComponent* Rear = CreateVehicleRoot(TEXT("M23_05_RearEscort"));
    AddVehicleMesh(Rear, TEXT("M23_05_RearBody"), RotorlineCabinConvoy::MrapBodyAsset);
    AddVehicleMesh(Rear, TEXT("M23_05_RearTurret"), RotorlineCabinConvoy::MrapTurretAsset);

    LeadDistanceCm = VehicleSpacingCm * (VehicleRoots.Num() - 1);
    for (int32 Index = 0; Index < VehicleRoots.Num(); ++Index)
    {
        PlaceVehicle(Index, LeadDistanceCm - VehicleSpacingCm * Index);
    }
}

FVector ARotorlineCabinSupplyConvoyActor::SampleRoute(float DistanceCm, FVector* OutDirection) const
{
    const float Clamped = FMath::Clamp(DistanceCm, 0.0f, RouteLengthCm);
    int32 Segment = 1;
    while (Segment < RouteDistances.Num() && RouteDistances[Segment] < Clamped) ++Segment;
    Segment = FMath::Clamp(Segment, 1, RoutePoints.Num() - 1);
    const float SegmentStart = RouteDistances[Segment - 1];
    const float SegmentLength = FMath::Max(1.0f, RouteDistances[Segment] - SegmentStart);
    const FVector Direction = (RoutePoints[Segment] - RoutePoints[Segment - 1]).GetSafeNormal2D();
    if (OutDirection) *OutDirection = Direction;
    return FMath::Lerp(RoutePoints[Segment - 1], RoutePoints[Segment],
        (Clamped - SegmentStart) / SegmentLength);
}

void ARotorlineCabinSupplyConvoyActor::PlaceVehicle(int32 UnitIndex, float RouteDistanceCm)
{
    if (!VehicleRoots.IsValidIndex(UnitIndex) || !GetWorld()) return;
    FVector Direction;
    FVector Location = SampleRoute(RouteDistanceCm, &Direction);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(M23ConvoyGround), false, this);
    const FVector TraceStart(Location.X, Location.Y, Location.Z + 150000.0f);
    const FVector TraceEnd(Location.X, Location.Y, Location.Z - 50000.0f);
    if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
    {
        Location.Z = Hit.ImpactPoint.Z + 20.0f;
    }
    VehicleRoots[UnitIndex]->SetWorldLocationAndRotation(
        Location, Direction.Rotation() + FRotator(0.0f, 270.0f, 0.0f),
        false, nullptr, ETeleportType::TeleportPhysics);
}

void ARotorlineCabinSupplyConvoyActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bStarted || bMissionFailed || bMissionSucceeded) return;

    LeadDistanceCm = FMath::Min(RouteLengthCm, LeadDistanceCm + ConvoySpeedCmPerSecond * DeltaSeconds);
    for (int32 Index = 0; Index < VehicleRoots.Num(); ++Index)
    {
        PlaceVehicle(Index, LeadDistanceCm - VehicleSpacingCm * Index);
    }
    UpdateThreatStages();

    if (LeadDistanceCm >= RouteLengthCm - 250.0f)
    {
        bMissionSucceeded = true;
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_M23_CONVOY|PARKED|destination=WAREHOUSE|vehicles=6|supply_trucks=4|result=SUCCESS"));
    }
}

float ARotorlineCabinSupplyConvoyActor::GetProgressFraction() const
{
    return RouteLengthCm > 1.0f ? FMath::Clamp(LeadDistanceCm / RouteLengthCm, 0.0f, 1.0f) : 0.0f;
}

FVector ARotorlineCabinSupplyConvoyActor::GetLeadWorldLocation() const
{
    return VehicleRoots.IsEmpty() || !IsValid(VehicleRoots[0])
        ? FVector::ZeroVector
        : VehicleRoots[0]->GetComponentLocation();
}

FString ARotorlineCabinSupplyConvoyActor::GetStatusText() const
{
    if (bMissionFailed) return TEXT("WAREHOUSE SUPPLY COLUMN LOST");
    if (bMissionSucceeded) return TEXT("SUPPLIES SECURED AT THE WAREHOUSE");
    return FString::Printf(TEXT("ESCORT SUPPLY COLUMN TO WAREHOUSE // %d%%"),
        FMath::RoundToInt(GetProgressFraction() * 100.0f));
}

ARotorlineMissionObjectiveActor* ARotorlineCabinSupplyConvoyActor::SpawnThreat(
    const FString& TargetId,
    const FString& Label,
    float RouteFraction,
    float LateralOffsetCm,
    float HeightOffsetCm)
{
    if (!GetWorld()) return nullptr;
    FVector Direction;
    FVector Location = SampleRoute(RouteLengthCm * RouteFraction, &Direction);
    Location += FVector(-Direction.Y, Direction.X, 0.0f) * LateralOffsetCm;
    Location.Z += HeightOffsetCm;

    FRotorlineObjectiveDefinition Objective;
    Objective.Kind = TEXT("destroy");
    Objective.Text = Label;
    Objective.Target = TargetId;
    Objective.Site = TEXT("m23-warehouse-supply-route");
    Objective.bHasLocation = true;
    Objective.bHasWorldLocation = true;
    Objective.WorldLocation = Location;
    Objective.Radius = 45.0f;

    ARotorlineMissionObjectiveActor* Threat = GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
        ARotorlineMissionObjectiveActor::StaticClass(), Location, FRotator::ZeroRotator);
    if (Threat)
    {
        Threat->Configure(Objective, Location);
        SpawnedThreats.Add(Threat);
    }
    return Threat;
}

void ARotorlineCabinSupplyConvoyActor::UpdateThreatStages()
{
    const float Progress = GetProgressFraction();
    const auto TriggerStage = [this, Progress](uint8 Bit, float Trigger, auto SpawnStage)
    {
        if ((SpawnedThreatStageMask & Bit) == 0 && Progress >= Trigger)
        {
            SpawnedThreatStageMask |= Bit;
            SpawnStage();
        }
    };

    TriggerStage(1 << 0, 0.18f, [this]()
    {
        SpawnThreat(TEXT("m23-tank-roadblock-alpha"), TEXT("WAREHOUSE ROUTE TANK SQUAD ALPHA"), 0.25f, 9000.0f, 0.0f);
        SpawnThreat(TEXT("m23-tank-roadblock-bravo"), TEXT("WAREHOUSE ROUTE TANK SQUAD BRAVO"), 0.27f, -10000.0f, 0.0f);
        SpawnThreat(TEXT("m23-flak-roadside"), TEXT("WAREHOUSE ROUTE FLAK POSITION"), 0.31f, 11000.0f, 0.0f);
    });
    TriggerStage(1 << 1, 0.40f, [this]()
    {
        SpawnThreat(TEXT("m23-md500-gunship"), TEXT("MD500 HIT-AND-RUN INTERCEPTOR"), 0.48f, 42000.0f, 9000.0f);
    });
    TriggerStage(1 << 2, 0.62f, [this]()
    {
        SpawnThreat(TEXT("m23-tank-ambush-charlie"), TEXT("WAREHOUSE ROUTE TANK SQUAD CHARLIE"), 0.69f, -9000.0f, 0.0f);
        SpawnThreat(TEXT("m23-tank-ambush-delta"), TEXT("WAREHOUSE ROUTE TANK SQUAD DELTA"), 0.72f, 10500.0f, 0.0f);
        SpawnThreat(TEXT("m23-flak-ridgeline"), TEXT("WAREHOUSE APPROACH FLAK POSITION"), 0.77f, -12000.0f, 0.0f);
    });
    TriggerStage(1 << 3, 0.78f, [this]()
    {
        SpawnThreat(TEXT("m23-hind-gunship"), TEXT("HIND WAREHOUSE CONVOY INTERCEPTOR"), 0.84f, -45000.0f, 10000.0f);
        SpawnThreat(TEXT("m23-flak-warehouse-final"), TEXT("WAREHOUSE FINAL APPROACH FLAK POSITION"), 0.90f, 18000.0f, 0.0f);
        SpawnThreat(TEXT("m23-tank-warehouse-final"), TEXT("WAREHOUSE FINAL APPROACH TANK"), 0.93f, -22000.0f, 0.0f);
    });
}

void ARotorlineCabinSupplyConvoyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (ARotorlineMissionObjectiveActor* Threat : SpawnedThreats)
    {
        if (IsValid(Threat)) Threat->Destroy();
    }
    SpawnedThreats.Reset();
    Super::EndPlay(EndPlayReason);
}
