#include "RotorlineRuntimePopulation.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"
#include "RotorlineBuildingClusterActor.h"
#include "RotorlineRoadNetworkActor.h"

namespace
{
    constexpr double AirfieldX = -233856.0;
    constexpr double AirfieldY = -209664.0;
    constexpr double AirfieldExclusionCm = 115000.0;
    constexpr double RoadClearanceCm = 2000.0;

    enum class EDistrictKind : uint8
    {
        Mixed,
        Rural,
        Work,
        Farm,
    };

    struct FDistrict
    {
        const TCHAR* Name;
        double X;
        double Y;
        double Yaw;
        EDistrictKind Kind;
        int32 Count;
    };

    struct FRoadSample
    {
        FVector2D Center;
        double Yaw = 0.0;
        double HalfLengthCm = 0.0;
        double HalfWidthCm = 0.0;
    };

    const FDistrict Districts[] =
    {
        { TEXT("WestCrossroads"), -116000.0, -116000.0, 43.0, EDistrictKind::Rural, 24 },
        { TEXT("CentralOutskirts"), -61000.0, -65000.0, 45.0, EDistrictKind::Mixed, 32 },
        { TEXT("NorthJunction"), -13500.0, 47000.0, 96.0, EDistrictKind::Mixed, 28 },
        { TEXT("NorthValley"), -24500.0, 104000.0, 98.0, EDistrictKind::Rural, 24 },
        { TEXT("NorthernStationVillage"), -35500.0, 164000.0, 98.0, EDistrictKind::Mixed, 24 },
        { TEXT("HarborWest"), 52000.0, -43000.0, -42.0, EDistrictKind::Mixed, 32 },
        { TEXT("RiverMarket"), 104000.0, -79000.0, -34.0, EDistrictKind::Mixed, 28 },
        { TEXT("HarborSuburb"), 158000.0, -119000.0, -29.0, EDistrictKind::Mixed, 34 },
        { TEXT("HarborWorks"), 188000.0, -176000.0, -4.0, EDistrictKind::Work, 20 },
        { TEXT("EastJunction"), 92000.0, -17500.0, 47.0, EDistrictKind::Mixed, 28 },
        { TEXT("EastPlateauTown"), 139000.0, 33000.0, 47.0, EDistrictKind::Mixed, 30 },
        { TEXT("EastDepotVillage"), 178000.0, 73000.0, 49.0, EDistrictKind::Rural, 24 },
        { TEXT("SouthCoastFarms"), 48000.0, -245000.0, 6.0, EDistrictKind::Farm, 22 },
        { TEXT("SoutheastFarms"), 164000.0, -232000.0, -8.0, EDistrictKind::Farm, 24 },
        { TEXT("WesternRanch"), -268000.0, 68000.0, 18.0, EDistrictKind::Farm, 18 },
        { TEXT("NortheastForestry"), 111000.0, 211000.0, -12.0, EDistrictKind::Work, 18 },
    };

    bool InsideAirfieldExclusion(const FVector2D& Point)
    {
        return FVector2D::Distance(Point, FVector2D(AirfieldX, AirfieldY)) < AirfieldExclusionCm;
    }

    FVector2D OffsetPoint(const FVector2D& Center, double ForwardMeters, double RightMeters, double YawDegrees)
    {
        const double Radians = FMath::DegreesToRadians(YawDegrees);
        return Center + FVector2D(
            (ForwardMeters * FMath::Cos(Radians) - RightMeters * FMath::Sin(Radians)) * 100.0,
            (ForwardMeters * FMath::Sin(Radians) + RightMeters * FMath::Cos(Radians)) * 100.0);
    }

    TArray<FRoadSample> GatherRoadSamples(UWorld* World)
    {
        TArray<FRoadSample> Samples;
        for (TActorIterator<ARotorlineRoadNetworkActor> It(World); It; ++It)
        {
            const FTransform ActorTransform = It->GetActorTransform();
            const FVector ActorScale = ActorTransform.GetScale3D();
            for (const FTransform& Pavement : It->PavementInstances)
            {
                const FVector WorldLocation = ActorTransform.TransformPosition(Pavement.GetLocation());
                const FQuat WorldRotation = ActorTransform.GetRotation() * Pavement.GetRotation();
                const FVector LocalScale = Pavement.GetScale3D();
                FRoadSample& Sample = Samples.AddDefaulted_GetRef();
                Sample.Center = FVector2D(WorldLocation.X, WorldLocation.Y);
                Sample.Yaw = WorldRotation.Rotator().Yaw;
                Sample.HalfLengthCm = FMath::Abs(LocalScale.X * ActorScale.X) * 50.0;
                Sample.HalfWidthCm = FMath::Abs(LocalScale.Y * ActorScale.Y) * 50.0;
            }
        }
        return Samples;
    }

    void FootprintAxes(double Yaw, FVector2D& OutForward, FVector2D& OutRight)
    {
        const double Radians = FMath::DegreesToRadians(Yaw);
        OutForward = FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
        OutRight = FVector2D(-FMath::Sin(Radians), FMath::Cos(Radians));
    }

    bool FootprintsOverlap(
        const FVector2D& CenterA, double YawA, double HalfLengthA, double HalfWidthA,
        const FVector2D& CenterB, double YawB, double HalfLengthB, double HalfWidthB)
    {
        FVector2D ForwardA, RightA, ForwardB, RightB;
        FootprintAxes(YawA, ForwardA, RightA);
        FootprintAxes(YawB, ForwardB, RightB);
        const FVector2D Delta = CenterB - CenterA;
        for (const FVector2D& Axis : { ForwardA, RightA, ForwardB, RightB })
        {
            const double Distance = FMath::Abs(FVector2D::DotProduct(Delta, Axis));
            const double RadiusA = HalfLengthA * FMath::Abs(FVector2D::DotProduct(ForwardA, Axis)) +
                HalfWidthA * FMath::Abs(FVector2D::DotProduct(RightA, Axis));
            const double RadiusB = HalfLengthB * FMath::Abs(FVector2D::DotProduct(ForwardB, Axis)) +
                HalfWidthB * FMath::Abs(FVector2D::DotProduct(RightB, Axis));
            if (Distance > RadiusA + RadiusB) return false;
        }
        return true;
    }

    bool IntersectsAnyRoadCorridor(
        const FVector2D& Center, double Yaw, double WidthMeters, double DepthMeters,
        double RoofOverhangMeters, const TArray<FRoadSample>& Roads)
    {
        const double HalfLengthCm = (WidthMeters * 0.5 + RoofOverhangMeters) * 100.0;
        const double HalfWidthCm = (DepthMeters * 0.5 + RoofOverhangMeters) * 100.0;
        for (const FRoadSample& Road : Roads)
        {
            if (FootprintsOverlap(
                Center, Yaw, HalfLengthCm, HalfWidthCm,
                Road.Center, Road.Yaw, Road.HalfLengthCm, Road.HalfWidthCm + RoadClearanceCm))
            {
                return true;
            }
        }
        return false;
    }

    bool LandscapeHeight(UWorld* World, const FVector2D& Point, double& OutHeight)
    {
        if (!World) return false;
        TArray<FHitResult> Hits;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(RotorlinePopulationGround), true);
        Query.bReturnPhysicalMaterial = false;
        World->LineTraceMultiByChannel(
            Hits,
            FVector(Point.X, Point.Y, 150000.0),
            FVector(Point.X, Point.Y, -40000.0),
            ECC_Visibility,
            Query);
        for (const FHitResult& Hit : Hits)
        {
            if (Cast<ALandscapeProxy>(Hit.GetActor()))
            {
                OutHeight = Hit.ImpactPoint.Z;
                return true;
            }
        }
        return false;
    }

    bool GroundEnvelope(
        UWorld* World,
        const FVector2D& Center,
        double WidthMeters,
        double DepthMeters,
        double Yaw,
        double& OutBottom,
        double& OutTop)
    {
        double Minimum = TNumericLimits<double>::Max();
        double Maximum = TNumericLimits<double>::Lowest();
        for (const double Forward : { -WidthMeters * 0.44, 0.0, WidthMeters * 0.44 })
        {
            for (const double Right : { -DepthMeters * 0.44, 0.0, DepthMeters * 0.44 })
            {
                double Height = 0.0;
                if (!LandscapeHeight(World, OffsetPoint(Center, Forward, Right, Yaw), Height)) return false;
                Minimum = FMath::Min(Minimum, Height);
                Maximum = FMath::Max(Maximum, Height);
            }
        }
        if (Maximum - Minimum > 420.0) return false;
        OutBottom = Minimum - 90.0;
        OutTop = Maximum + 18.0;
        return true;
    }

    void AddBox(
        TArray<FTransform>& Instances,
        const FVector2D& DistrictCenter,
        const FVector2D& WorldCenter,
        double Z,
        const FVector& DimensionsMeters,
        double Yaw,
        double Pitch = 0.0,
        double Roll = 0.0)
    {
        Instances.Emplace(
            FRotator(Pitch, Yaw, Roll),
            FVector(WorldCenter.X - DistrictCenter.X, WorldCenter.Y - DistrictCenter.Y, Z),
            DimensionsMeters);
    }

    double AddFoundation(
        ARotorlineBuildingClusterActor* Actor,
        const FVector2D& DistrictCenter,
        const FVector2D& Center,
        double Width,
        double Depth,
        double Yaw,
        double Bottom,
        double Top)
    {
        AddBox(Actor->FoundationInstances, DistrictCenter, Center, (Top + Bottom) * 0.5,
            FVector(Width + 1.2, Depth + 1.2, FMath::Max(0.35, (Top - Bottom) / 100.0)), Yaw);
        return Top;
    }

    void AddPitchedRoof(
        ARotorlineBuildingClusterActor* Actor,
        const FVector2D& DistrictCenter,
        const FVector2D& Center,
        double Width,
        double Depth,
        double WallTop,
        double Yaw,
        double Angle = 28.0,
        double Overhang = 1.0)
{
    const double AngleRadians = FMath::DegreesToRadians(Angle);
    const double RoofRun = Depth * 0.5 + Overhang;
    const double Span = RoofRun / FMath::Cos(AngleRadians);
    const double RoofTan = FMath::Max(FMath::Tan(AngleRadians), 0.01);
    const double RoofCenterOffset = RoofRun * 50.0 * RoofTan;
    const double PanelCenterOffset = RoofRun * 0.5;
    for (const double Side : { -1.0, 1.0 })
    {
        const FVector2D Panel = OffsetPoint(Center, 0.0, Side * PanelCenterOffset, Yaw);
        AddBox(Actor->RoofInstances, DistrictCenter, Panel, WallTop + RoofCenterOffset,
            FVector(Width + Overhang * 2.0, Span, 0.42), Yaw, 0.0, Side * Angle);

        constexpr int32 GableSteps = 64;
        const double StepHeight = RoofCenterOffset / GableSteps;
        for (int32 Step = 0; Step < GableSteps; ++Step)
        {
            const double HeightAtBottom = Step * StepHeight;
            const double HeightAtCenter = (Step + 0.5) * StepHeight;
            const double HalfWidthCm = FMath::Max(5.0,
                FMath::Min(Depth * 50.0, RoofRun * 50.0 - HeightAtBottom / RoofTan));
            const FVector2D Gable = OffsetPoint(Center, Side * (Width * 0.5 + 0.02), 0.0, Yaw);
            AddBox(Actor->ShellInstances, DistrictCenter, Gable, WallTop + HeightAtCenter,
                FVector(0.26, HalfWidthCm / 50.0 + 0.06, StepHeight / 100.0 + 0.06), Yaw);
        }
    }
}

    bool AddHouse(
        UWorld* World,
        ARotorlineBuildingClusterActor* Actor,
        const FVector2D& DistrictCenter,
        const FVector2D& Center,
        double Yaw,
        int32 Index)
    {
        const double Width = 15.0 + (Index % 4) * 2.2;
        const double Depth = 10.5 + (Index % 3) * 1.6;
        const double Height = 6.4 + (Index % 2) * 0.8;
        double Bottom = 0.0, Top = 0.0;
        if (!GroundEnvelope(World, Center, Width, Depth, Yaw, Bottom, Top)) return false;
        const double Base = AddFoundation(Actor, DistrictCenter, Center, Width, Depth, Yaw, Bottom, Top);
        AddBox(Actor->ShellInstances, DistrictCenter, Center, Base + Height * 50.0,
            FVector(Width, Depth, Height), Yaw);
        AddPitchedRoof(Actor, DistrictCenter, Center, Width, Depth, Base + Height * 100.0, Yaw);
        const FVector2D Door = OffsetPoint(Center, -Width * 0.5 - 0.14, 0.0, Yaw);
        AddBox(Actor->DoorInstances, DistrictCenter, Door, Base + 145.0, FVector(0.30, 2.2, 2.9), Yaw);
        for (const double Side : { -1.0, 1.0 })
        {
            for (const double Forward : { -Width * 0.24, Width * 0.22 })
            {
                const FVector2D Window = OffsetPoint(Center, Forward, Side * (Depth * 0.5 + 0.08), Yaw);
                TArray<FTransform>& Target = ((Index + (Side > 0.0 ? 1 : 0) + (Forward > 0.0 ? 1 : 0)) % 5 == 0)
                    ? Actor->LitWindowInstances : Actor->WindowInstances;
                AddBox(Target, DistrictCenter, Window, Base + 320.0, FVector(2.2, 0.16, 1.8), Yaw);
            }
        }
        const FVector2D Chimney = OffsetPoint(Center, Width * 0.22, Depth * 0.22, Yaw);
        AddBox(Actor->RooftopInstances, DistrictCenter, Chimney, Base + Height * 100.0 + 170.0,
            FVector(0.85, 0.85, 3.4), Yaw);
        return true;
    }

    bool AddShop(
        UWorld* World,
        ARotorlineBuildingClusterActor* Actor,
        const FVector2D& DistrictCenter,
        const FVector2D& Center,
        double Yaw,
        int32 Index)
    {
        const double Width = 23.0 + (Index % 3) * 3.0;
        const double Depth = 15.0 + (Index % 2) * 2.0;
        const double Height = 9.0 + (Index % 2) * 3.8;
        double Bottom = 0.0, Top = 0.0;
        if (!GroundEnvelope(World, Center, Width, Depth, Yaw, Bottom, Top)) return false;
        const double Base = AddFoundation(Actor, DistrictCenter, Center, Width, Depth, Yaw, Bottom, Top);
        AddBox(Actor->ShellInstances, DistrictCenter, Center, Base + Height * 50.0,
            FVector(Width, Depth, Height), Yaw);
        AddBox(Actor->RoofInstances, DistrictCenter, Center, Base + Height * 100.0 + 38.0,
            FVector(Width + 1.8, Depth + 1.8, 0.76), Yaw);
        AddBox(Actor->AccentInstances, DistrictCenter, Center, Base + 48.0,
            FVector(Width + 0.5, Depth + 0.5, 0.9), Yaw);
        const int32 Floors = FMath::Max(2, FMath::RoundToInt(Height / 4.5));
        for (int32 Floor = 0; Floor < Floors; ++Floor)
        {
            for (const double Side : { -1.0, 1.0 })
            {
                for (const double Column : { -0.28, 0.0, 0.28 })
                {
                    const FVector2D Window = OffsetPoint(Center, Column * Width, Side * (Depth * 0.5 + 0.08), Yaw);
                    TArray<FTransform>& Target = ((Index + Floor + (Side > 0.0 ? 1 : 0)) % 6 == 0)
                        ? Actor->LitWindowInstances : Actor->WindowInstances;
                    AddBox(Target, DistrictCenter, Window, Base + 285.0 + Floor * 390.0,
                        FVector(2.8, 0.16, 2.0), Yaw);
                }
            }
        }
        const FVector2D Door = OffsetPoint(Center, -Width * 0.5 - 0.12, 0.0, Yaw);
        AddBox(Actor->DoorInstances, DistrictCenter, Door, Base + 190.0, FVector(0.30, 3.4, 3.8), Yaw);
        return true;
    }

    bool AddWarehouse(
        UWorld* World,
        ARotorlineBuildingClusterActor* Actor,
        const FVector2D& DistrictCenter,
        const FVector2D& Center,
        double Yaw,
        int32 Index)
    {
        const double Width = 38.0 + (Index % 4) * 6.0;
        const double Depth = 24.0 + (Index % 3) * 4.0;
        const double Height = 10.0 + (Index % 2) * 2.0;
        double Bottom = 0.0, Top = 0.0;
        if (!GroundEnvelope(World, Center, Width, Depth, Yaw, Bottom, Top)) return false;
        const double Base = AddFoundation(Actor, DistrictCenter, Center, Width, Depth, Yaw, Bottom, Top);
        AddBox(Actor->ShellInstances, DistrictCenter, Center, Base + Height * 50.0,
            FVector(Width, Depth, Height), Yaw);
        AddPitchedRoof(Actor, DistrictCenter, Center, Width, Depth, Base + Height * 100.0, Yaw, 16.0, 1.5);
        for (const double Right : { -Depth * 0.27, Depth * 0.27 })
        {
            const FVector2D Door = OffsetPoint(Center, -Width * 0.5 - 0.12, Right, Yaw);
            AddBox(Actor->DoorInstances, DistrictCenter, Door, Base + 290.0,
                FVector(0.35, FMath::Min(7.0, Depth * 0.3), 5.8), Yaw);
        }
        return true;
    }

    int32 BuildDistrict(
        UWorld* World,
        const FDistrict& District,
        int32 DistrictIndex,
        FRandomStream& Random,
        const TArray<FRoadSample>& Roads)
    {
        const FVector2D Center(District.X, District.Y);
        const double Spacing = District.Kind == EDistrictKind::Work ? 74.0
            : District.Kind == EDistrictKind::Farm ? 58.0 : 52.0;
        const int32 RequiredSamples = FMath::CeilToInt(District.Count / 2.0);
        TArray<int32> Candidates;
        for (int32 Index = 0; Index < Roads.Num(); ++Index)
        {
            if (!InsideAirfieldExclusion(Roads[Index].Center))
            {
                Candidates.Add(Index);
            }
        }
        Candidates.Sort([&Roads, &Center](const int32 A, const int32 B)
        {
            return FVector2D::DistSquared(Roads[A].Center, Center)
                < FVector2D::DistSquared(Roads[B].Center, Center);
        });

        TArray<int32> Selected;
        const auto AddSeparatedSamples = [&Candidates, &Selected, &Roads, RequiredSamples](const double MinimumSpacingCm)
        {
            for (const int32 Candidate : Candidates)
            {
                if (Selected.Num() >= RequiredSamples) break;
                if (Selected.Contains(Candidate)) continue;
                bool bSeparated = true;
                for (const int32 Existing : Selected)
                {
                    if (FVector2D::Distance(Roads[Candidate].Center, Roads[Existing].Center) < MinimumSpacingCm)
                    {
                        bSeparated = false;
                        break;
                    }
                }
                if (bSeparated) Selected.Add(Candidate);
            }
        };
        AddSeparatedSamples(Spacing * 100.0 * 0.82);
        AddSeparatedSamples(Spacing * 100.0 * 0.48);
        if (Selected.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_RUNTIME_POPULATION|SKIP|name=%s|reason=no_road_samples"), District.Name);
            return 0;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Name = MakeUniqueObjectName(World, ARotorlineBuildingClusterActor::StaticClass(),
            FName(FString::Printf(TEXT("RuntimePopulation_%s"), District.Name)));
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ARotorlineBuildingClusterActor* Actor = World->SpawnActor<ARotorlineBuildingClusterActor>(
            ARotorlineBuildingClusterActor::StaticClass(), FVector(Center.X, Center.Y, 0.0),
            FRotator::ZeroRotator, SpawnParameters);
        if (!Actor) return 0;
        Actor->Tags.AddUnique(TEXT("RotorlineRuntimePopulation"));
#if WITH_EDITOR
        Actor->SetActorLabel(FString::Printf(TEXT("RUNTIME_POPULATION_%s"), District.Name));
#endif

        int32 Built = 0;
        int32 RoadRejected = 0;
        for (int32 Index = 0; Index < District.Count; ++Index)
        {
            const double Side = Index % 2 == 0 ? -1.0 : 1.0;
            const FRoadSample& Road = Roads[Selected[(Index / 2) % Selected.Num()]];
            const bool bWarehouse = District.Kind == EDistrictKind::Work ||
                (District.Kind == EDistrictKind::Farm && Index % 4 == 0) ||
                (District.Kind == EDistrictKind::Mixed && Index % 11 == 0);
            const bool bShop = !bWarehouse &&
                ((District.Kind == EDistrictKind::Rural && Index % 8 == 0) ||
                    (District.Kind == EDistrictKind::Mixed && Index % 5 == 0));
            const double Width = bWarehouse ? 38.0 + (Index % 4) * 6.0
                : bShop ? 23.0 + (Index % 3) * 3.0
                : 15.0 + (Index % 4) * 2.2;
            const double Depth = bWarehouse ? 24.0 + (Index % 3) * 4.0
                : bShop ? 15.0 + (Index % 2) * 2.0
                : 10.5 + (Index % 3) * 1.6;
            // Keep the pitched roof footprint close to the building shell.
            // The roof panels are rectangular boxes, so excessive overhang
            // reads as a second cap protruding through the A-frame.
            const double RoofOverhang = bWarehouse ? 0.65 : 0.35;
            // Local -X is the facade/door side, so this turns each building
            // toward the sampled curved road instead of a district-wide grid.
            const double BuildingYaw = Road.Yaw + Side * 90.0 + Random.FRandRange(-2.0, 2.0);
            const double RelativeYaw = FMath::DegreesToRadians(BuildingYaw - (Road.Yaw + Side * 90.0));
            const double NormalExtent = FMath::Abs(FMath::Cos(RelativeYaw)) * (Width * 0.5 + RoofOverhang) +
                FMath::Abs(FMath::Sin(RelativeYaw)) * (Depth * 0.5 + RoofOverhang);
            const double Right = Side * (Road.HalfWidthCm / 100.0 + RoadClearanceCm / 100.0 +
                NormalExtent + Random.FRandRange(0.0, 7.0));
            const FVector2D Lot = OffsetPoint(Road.Center, Random.FRandRange(-4.0, 4.0), Right, Road.Yaw);
            if (InsideAirfieldExclusion(Lot)) continue;
            if (IntersectsAnyRoadCorridor(Lot, BuildingYaw, Width, Depth, RoofOverhang, Roads))
            {
                ++RoadRejected;
                continue;
            }
            bool Success = false;
            if (bWarehouse)
            {
                Success = AddWarehouse(World, Actor, Center, Lot, BuildingYaw, Index);
            }
            else if (bShop)
            {
                Success = AddShop(World, Actor, Center, Lot, BuildingYaw, Index);
            }
            else
            {
                Success = AddHouse(World, Actor, Center, Lot, BuildingYaw, Index);
            }
            Built += Success ? 1 : 0;
        }

        Actor->RebuildBuildingInstances();
        UE_LOG(LogTemp, Display,
            TEXT("ROTORLINE_RUNTIME_POPULATION|DISTRICT|name=%s|built=%d|index=%d|road_samples=%d|road_rejected=%d|road_clearance_m=%.1f"),
            District.Name, Built, DistrictIndex, Selected.Num(), RoadRejected, RoadClearanceCm / 100.0);
        return Built;
    }
}

void RotorlineRuntimePopulation::Spawn(UWorld* World)
{
    if (!World) return;
    for (TActorIterator<ARotorlineBuildingClusterActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(TEXT("RotorlineRuntimePopulation"))) return;
    }

    FRandomStream Random(72622);
    const TArray<FRoadSample> Roads = GatherRoadSamples(World);
    int32 TotalBuilt = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Districts); ++Index)
    {
        TotalBuilt += BuildDistrict(World, Districts[Index], Index, Random, Roads);
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_RUNTIME_POPULATION|PASS|districts=%d|buildings=%d|generated_roads=0|legacy_road_samples=%d|road_clearance_m=%.1f|all_road_corridors_checked=1"),
        UE_ARRAY_COUNT(Districts), TotalBuilt, Roads.Num(), RoadClearanceCm / 100.0);
}
