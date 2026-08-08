#include "RotorlineAircraftCatalog.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString StringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        FString Value;
        if (Object.IsValid())
        {
            Object->TryGetStringField(Name, Value);
        }
        return Value;
    }

    int32 RatingField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        double Value = 1.0;
        if (Object.IsValid())
        {
            Object->TryGetNumberField(Name, Value);
        }
        return FMath::Clamp(FMath::RoundToInt(Value), 1, 5);
    }

    bool BoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        bool Value = false;
        if (Object.IsValid())
        {
            Object->TryGetBoolField(Name, Value);
        }
        return Value;
    }

    float FloatField(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Name,
        float DefaultValue)
    {
        double Value = DefaultValue;
        if (Object.IsValid())
        {
            Object->TryGetNumberField(Name, Value);
        }
        return static_cast<float>(Value);
    }

    TSharedPtr<FJsonObject> ObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        if (!Object.IsValid())
        {
            return nullptr;
        }
        const TSharedPtr<FJsonObject>* Value = nullptr;
        return Object->TryGetObjectField(Name, Value) && Value ? *Value : nullptr;
    }

    void StringArrayField(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Name,
        TArray<FString>& OutValues)
    {
        OutValues.Reset();
        if (!Object.IsValid())
        {
            return;
        }

        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Name, Values) || !Values)
        {
            return;
        }

        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Text;
            if (Value.IsValid() && Value->TryGetString(Text) && !Text.IsEmpty())
            {
                OutValues.Add(MoveTemp(Text));
            }
        }
    }

    FVector VectorField(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Name,
        const FVector& DefaultValue)
    {
        if (!Object.IsValid()) return DefaultValue;
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Name, Values) || !Values || Values->Num() != 3)
        {
            return DefaultValue;
        }
        double X = DefaultValue.X;
        double Y = DefaultValue.Y;
        double Z = DefaultValue.Z;
        if (!(*Values)[0].IsValid() || !(*Values)[0]->TryGetNumber(X) ||
            !(*Values)[1].IsValid() || !(*Values)[1]->TryGetNumber(Y) ||
            !(*Values)[2].IsValid() || !(*Values)[2]->TryGetNumber(Z))
        {
            return DefaultValue;
        }
        return FVector(X, Y, Z);
    }
}

bool FRotorlineAircraftCatalog::Load(
    TArray<FRotorlineAircraftDefinition>& OutAircraft,
    FString& OutError)
{
    OutAircraft.Reset();
    OutError.Reset();

    const FString CatalogPath = FPaths::Combine(
        FPaths::ProjectContentDir(),
        TEXT("Data/HelicopterRoster.json"));
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *CatalogPath))
    {
        OutError = FString::Printf(TEXT("Could not read %s"), *CatalogPath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Aircraft catalog JSON is invalid");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* AircraftValues = nullptr;
    if (!Root->TryGetArrayField(TEXT("aircraft"), AircraftValues) || !AircraftValues)
    {
        OutError = TEXT("Aircraft catalog has no aircraft array");
        return false;
    }

    TSet<FString> SeenIds;
    for (const TSharedPtr<FJsonValue>& AircraftValue : *AircraftValues)
    {
        const TSharedPtr<FJsonObject> Object = AircraftValue->AsObject();
        if (!Object.IsValid())
        {
            continue;
        }

        FRotorlineAircraftDefinition Aircraft;
        Aircraft.Id = StringField(Object, TEXT("id"));
        Aircraft.DisplayName = StringField(Object, TEXT("displayName"));
        Aircraft.Manufacturer = StringField(Object, TEXT("manufacturer"));
        Aircraft.Role = StringField(Object, TEXT("role"));
        Aircraft.Summary = StringField(Object, TEXT("summary"));
        Aircraft.SpawnStatus = StringField(Object, TEXT("spawnStatus"));
        Aircraft.FuelEnduranceSeconds = FMath::Clamp(
            FloatField(Object, TEXT("fuelEnduranceMinutes"), 10.0f) * 60.0f,
            480.0f,
            720.0f);
        Aircraft.bHangarVisible = BoolField(Object, TEXT("hangarVisible"));
        Aircraft.bAlphaSelectable = BoolField(Object, TEXT("alphaSelectable"));
        Aircraft.bDeploymentReady = BoolField(Object, TEXT("deploymentReady"));
        Aircraft.bEnemyEligible = BoolField(Object, TEXT("enemyEligible"));

        const TSharedPtr<FJsonObject> Stats = ObjectField(Object, TEXT("stats"));
        Aircraft.Stats.Speed = RatingField(Stats, TEXT("speed"));
        Aircraft.Stats.Maneuverability = RatingField(Stats, TEXT("maneuverability"));
        Aircraft.Stats.Armor = RatingField(Stats, TEXT("armor"));
        Aircraft.Stats.Cargo = RatingField(Stats, TEXT("cargo"));

        const TSharedPtr<FJsonObject> Suitability = ObjectField(Object, TEXT("missionSuitability"));
        Aircraft.MissionSuitability.Rescue = RatingField(Suitability, TEXT("rescue"));
        Aircraft.MissionSuitability.Medevac = RatingField(Suitability, TEXT("medevac"));
        Aircraft.MissionSuitability.Cargo = RatingField(Suitability, TEXT("cargo"));
        Aircraft.MissionSuitability.Recon = RatingField(Suitability, TEXT("recon"));
        Aircraft.MissionSuitability.Attack = RatingField(Suitability, TEXT("attack"));
        Aircraft.MissionSuitability.Escort = RatingField(Suitability, TEXT("escort"));

        const TSharedPtr<FJsonObject> Source = ObjectField(Object, TEXT("source"));
        Aircraft.PreferredGlb = StringField(Source, TEXT("preferredGlb"));
        StringArrayField(Source, TEXT("variants"), Aircraft.SourceVariants);
        Aircraft.License = StringField(Source, TEXT("license"));
        Aircraft.Credit = StringField(Source, TEXT("credit"));
        Aircraft.SourceUrl = StringField(Source, TEXT("sourceUrl"));

        const TSharedPtr<FJsonObject> Unreal = ObjectField(Object, TEXT("unreal"));
        Aircraft.AssetRoot = StringField(Unreal, TEXT("assetRoot"));
        Aircraft.BodyAsset = StringField(Unreal, TEXT("bodyAsset"));
        StringArrayField(Unreal, TEXT("bodyAssets"), Aircraft.BodyAssets);
        Aircraft.DeploymentClass = StringField(Unreal, TEXT("deploymentClass"));
        StringArrayField(Unreal, TEXT("rotorAssets"), Aircraft.RotorAssets);
        const TArray<TSharedPtr<FJsonValue>>* RotorGroupValues = nullptr;
        if (Unreal.IsValid() && Unreal->TryGetArrayField(TEXT("rotorGroups"), RotorGroupValues) && RotorGroupValues)
        {
            for (const TSharedPtr<FJsonValue>& RotorGroupValue : *RotorGroupValues)
            {
                const TSharedPtr<FJsonObject> RotorGroupObject = RotorGroupValue.IsValid()
                    ? RotorGroupValue->AsObject()
                    : nullptr;
                if (!RotorGroupObject.IsValid())
                {
                    continue;
                }
                FRotorlineAircraftRotorGroup RotorGroup;
                RotorGroup.Role = StringField(RotorGroupObject, TEXT("role")).ToLower();
                RotorGroup.SpinAxis = StringField(RotorGroupObject, TEXT("spinAxis")).ToUpper();
                StringArrayField(RotorGroupObject, TEXT("assets"), RotorGroup.Assets);
                const TArray<TSharedPtr<FJsonValue>>* PivotValues = nullptr;
                if (RotorGroupObject->TryGetArrayField(TEXT("pivot"), PivotValues)
                    && PivotValues
                    && PivotValues->Num() == 3)
                {
                    double X = 0.0;
                    double Y = 0.0;
                    double Z = 0.0;
                    if ((*PivotValues)[0].IsValid() && (*PivotValues)[0]->TryGetNumber(X)
                        && (*PivotValues)[1].IsValid() && (*PivotValues)[1]->TryGetNumber(Y)
                        && (*PivotValues)[2].IsValid() && (*PivotValues)[2]->TryGetNumber(Z))
                    {
                        RotorGroup.Pivot = FVector(X, Y, Z);
                        RotorGroup.bHasExplicitPivot = true;
                    }
                }
                const TArray<TSharedPtr<FJsonValue>>* MeshPivotValues = nullptr;
                if (RotorGroupObject->TryGetArrayField(TEXT("meshPivot"), MeshPivotValues)
                    && MeshPivotValues
                    && MeshPivotValues->Num() == 3)
                {
                    double X = 0.0;
                    double Y = 0.0;
                    double Z = 0.0;
                    if ((*MeshPivotValues)[0].IsValid() && (*MeshPivotValues)[0]->TryGetNumber(X)
                        && (*MeshPivotValues)[1].IsValid() && (*MeshPivotValues)[1]->TryGetNumber(Y)
                        && (*MeshPivotValues)[2].IsValid() && (*MeshPivotValues)[2]->TryGetNumber(Z))
                    {
                        RotorGroup.MeshPivot = FVector(X, Y, Z);
                        RotorGroup.bHasExplicitMeshPivot = true;
                    }
                }
                RotorGroup.AlignmentRotation = FRotator(
                    FloatField(RotorGroupObject, TEXT("alignmentPitch"), 0.0f),
                    FloatField(RotorGroupObject, TEXT("alignmentYaw"), 0.0f),
                    FloatField(RotorGroupObject, TEXT("alignmentRoll"), 0.0f));
                if (RotorGroup.Role.IsEmpty()) RotorGroup.Role = TEXT("main");
                if (RotorGroup.SpinAxis.IsEmpty()) RotorGroup.SpinAxis = RotorGroup.Role == TEXT("tail") ? TEXT("X") : TEXT("Z");
                if (!RotorGroup.Assets.IsEmpty()) Aircraft.RotorGroups.Add(MoveTemp(RotorGroup));
            }
        }
        StringArrayField(Unreal, TEXT("stationaryRotorAssets"), Aircraft.StationaryRotorAssets);
        Aircraft.AnimationAsset = StringField(Unreal, TEXT("animationAsset"));
        Aircraft.bAllowProceduralRotorFallback = BoolField(Unreal, TEXT("allowProceduralRotorFallback"));
        Aircraft.bReadyForHangar = BoolField(Unreal, TEXT("readyForHangar"));

        const TSharedPtr<FJsonObject> Model = ObjectField(Object, TEXT("model"));
        Aircraft.PresentationScale = FloatField(Model, TEXT("presentationScale"), 1.0f);
        Aircraft.PresentationPitch = FloatField(Model, TEXT("presentationPitch"), 0.0f);
        Aircraft.PresentationYaw = FloatField(Model, TEXT("presentationYaw"), 0.0f);
        Aircraft.PresentationRoll = FloatField(Model, TEXT("presentationRoll"), 0.0f);
        const TSharedPtr<FJsonObject> PresentationOffset = ObjectField(Model, TEXT("presentationOffset"));
        Aircraft.PresentationOffset = FVector(
            FloatField(PresentationOffset, TEXT("x"), 0.0f),
            FloatField(PresentationOffset, TEXT("y"), 0.0f),
            FloatField(PresentationOffset, TEXT("z"), 0.0f));
        Aircraft.SourceCenter = VectorField(Model, TEXT("sourceCenter"), FVector::ZeroVector);
        Aircraft.SourceMinimumZ = FloatField(Model, TEXT("sourceMinimumZ"), 0.0f);

        const TSharedPtr<FJsonObject> Audio = ObjectField(Object, TEXT("audio"));
        Aircraft.AudioStatus = StringField(Audio, TEXT("status"));
        Aircraft.PreIgnitionAudio = StringField(Audio, TEXT("preIgnition"));
        Aircraft.StartupAudio = StringField(Audio, TEXT("startup"));
        Aircraft.TakeoffAudio = StringField(Audio, TEXT("takeoff"));
        Aircraft.InflightAudio = StringField(Audio, TEXT("inflight"));
        Aircraft.AutocannonAudio = StringField(Audio, TEXT("autocannon"));

        const TSharedPtr<FJsonObject> Exhaust = ObjectField(Object, TEXT("exhaust"));
        Aircraft.Exhaust.bEnabled = BoolField(Exhaust, TEXT("enabled"));
        Aircraft.Exhaust.PlumeWidth = FMath::Clamp(FloatField(Exhaust, TEXT("plumeWidth"), 1.0f), 0.25f, 4.0f);
        Aircraft.Exhaust.PlumeLength = FMath::Clamp(FloatField(Exhaust, TEXT("plumeLength"), 1.0f), 0.25f, 4.0f);
        Aircraft.Exhaust.VelocityMultiplier = FMath::Clamp(FloatField(Exhaust, TEXT("velocityMultiplier"), 1.0f), 0.25f, 4.0f);
        Aircraft.Exhaust.VaporAmount = FMath::Clamp(FloatField(Exhaust, TEXT("vaporAmount"), 1.0f), 0.0f, 3.0f);
        Aircraft.Exhaust.DistortionIntensity = FMath::Clamp(FloatField(Exhaust, TEXT("distortionIntensity"), 1.0f), 0.0f, 3.0f);
        Aircraft.Exhaust.bStartupPuff = !Exhaust.IsValid() || !Exhaust->HasField(TEXT("startupPuff"))
            ? true
            : BoolField(Exhaust, TEXT("startupPuff"));
        const TArray<TSharedPtr<FJsonValue>>* ExhaustOutlets = nullptr;
        if (Exhaust.IsValid() && Exhaust->TryGetArrayField(TEXT("outlets"), ExhaustOutlets) && ExhaustOutlets)
        {
            for (const TSharedPtr<FJsonValue>& OutletValue : *ExhaustOutlets)
            {
                const TSharedPtr<FJsonObject> OutletObject = OutletValue.IsValid() ? OutletValue->AsObject() : nullptr;
                if (!OutletObject.IsValid()) continue;
                FRotorlineAircraftExhaustOutlet Outlet;
                Outlet.Location = VectorField(OutletObject, TEXT("location"), FVector::ZeroVector);
                const FVector Rotation = VectorField(OutletObject, TEXT("rotation"), FVector(0.0f, 180.0f, 0.0f));
                Outlet.Rotation = FRotator(Rotation.X, Rotation.Y, Rotation.Z);
                Outlet.Diameter = FMath::Clamp(FloatField(OutletObject, TEXT("diameter"), 26.0f), 5.0f, 120.0f);
                Aircraft.Exhaust.Outlets.Add(Outlet);
            }
        }
        Aircraft.Exhaust.bEnabled = Aircraft.Exhaust.bEnabled && !Aircraft.Exhaust.Outlets.IsEmpty();

        const TSharedPtr<FJsonObject> WeaponLoadout = ObjectField(Object, TEXT("weaponLoadout"));
        Aircraft.WeaponLoadout.bEnabled = BoolField(WeaponLoadout, TEXT("enabled"));
        Aircraft.WeaponLoadout.GunDeploymentDuration = FloatField(WeaponLoadout, TEXT("gunDeploymentDuration"), 0.85f);
        Aircraft.WeaponLoadout.GunDeploymentDistance = FloatField(WeaponLoadout, TEXT("gunDeploymentDistance"), 135.0f);
        Aircraft.WeaponLoadout.PodDeploymentDuration = FloatField(WeaponLoadout, TEXT("podDeploymentDuration"), 0.95f);
        Aircraft.WeaponLoadout.PodDeploymentDistance = FloatField(WeaponLoadout, TEXT("podDeploymentDistance"), 105.0f);
        Aircraft.WeaponLoadout.PodArcDegrees = FloatField(WeaponLoadout, TEXT("podArcDegrees"), 90.0f);
        Aircraft.WeaponLoadout.RetractionDelay = FloatField(WeaponLoadout, TEXT("retractionDelay"), 0.25f);
        Aircraft.WeaponLoadout.ConvergenceDistanceMeters = FloatField(WeaponLoadout, TEXT("convergenceDistanceMeters"), 1200.0f);
        Aircraft.WeaponLoadout.LeftGunMount = VectorField(WeaponLoadout, TEXT("leftGunMount"), Aircraft.WeaponLoadout.LeftGunMount);
        Aircraft.WeaponLoadout.RightGunMount = VectorField(WeaponLoadout, TEXT("rightGunMount"), Aircraft.WeaponLoadout.RightGunMount);
        Aircraft.WeaponLoadout.BellyPodMount = VectorField(WeaponLoadout, TEXT("bellyPodMount"), Aircraft.WeaponLoadout.BellyPodMount);
        const TArray<TSharedPtr<FJsonValue>>* WeaponModes = nullptr;
        if (WeaponLoadout.IsValid() && WeaponLoadout->TryGetArrayField(TEXT("modes"), WeaponModes) && WeaponModes)
        {
            for (const TSharedPtr<FJsonValue>& ModeValue : *WeaponModes)
            {
                const TSharedPtr<FJsonObject> ModeObject = ModeValue.IsValid() ? ModeValue->AsObject() : nullptr;
                if (!ModeObject.IsValid()) continue;
                FRotorlineAircraftWeaponModeDefinition Mode;
                Mode.Id = StringField(ModeObject, TEXT("id"));
                Mode.DisplayName = StringField(ModeObject, TEXT("displayName"));
                Mode.TargetClass = StringField(ModeObject, TEXT("targetClass"));
                Mode.ProjectileAsset = StringField(ModeObject, TEXT("projectileAsset"));
                Mode.Ammo = FMath::Max(0, FMath::RoundToInt(FloatField(ModeObject, TEXT("ammo"), 0.0f)));
                Mode.Damage = FMath::Max(0.0f, FloatField(ModeObject, TEXT("damage"), 0.0f));
                Mode.MinimumBlastDamage = FMath::Max(0.0f, FloatField(ModeObject, TEXT("minimumBlastDamage"), 0.0f));
                Mode.BlastRadius = FMath::Max(0.0f, FloatField(ModeObject, TEXT("blastRadius"), 0.0f));
                Mode.FireInterval = FMath::Max(0.03f, FloatField(ModeObject, TEXT("fireInterval"), 0.2f));
                Mode.SpreadDegrees = FMath::Max(0.0f, FloatField(ModeObject, TEXT("spreadDegrees"), 0.0f));
                Mode.LockSeconds = FMath::Max(0.0f, FloatField(ModeObject, TEXT("lockSeconds"), 0.0f));
                Mode.MaxRangeMeters = FMath::Max(100.0f, FloatField(ModeObject, TEXT("maxRangeMeters"), 5000.0f));
                Mode.ProjectileSpeed = FMath::Max(1000.0f, FloatField(ModeObject, TEXT("projectileSpeed"), 36000.0f));
                if (!Mode.Id.IsEmpty()) Aircraft.WeaponLoadout.Modes.Add(MoveTemp(Mode));
            }
        }
        StringArrayField(Object, TEXT("gaps"), Aircraft.Gaps);

        if (Aircraft.Id.IsEmpty() || Aircraft.DisplayName.IsEmpty())
        {
            OutError = TEXT("Aircraft catalog contains a record without id/displayName");
            OutAircraft.Reset();
            return false;
        }
        if (SeenIds.Contains(Aircraft.Id))
        {
            OutError = FString::Printf(TEXT("Duplicate aircraft id: %s"), *Aircraft.Id);
            OutAircraft.Reset();
            return false;
        }
        SeenIds.Add(Aircraft.Id);
        OutAircraft.Add(MoveTemp(Aircraft));
    }

    if (OutAircraft.IsEmpty())
    {
        OutError = TEXT("Aircraft catalog contained no usable aircraft");
        return false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ROTORLINE_AIRCRAFT|LOADED|count=%d|path=%s"),
        OutAircraft.Num(),
        *CatalogPath);
    return true;
}

const FRotorlineAircraftDefinition* FRotorlineAircraftCatalog::FindById(
    const TArray<FRotorlineAircraftDefinition>& Aircraft,
    const FString& Id)
{
    return Aircraft.FindByPredicate(
        [&Id](const FRotorlineAircraftDefinition& Entry)
        {
            return Entry.Id.Equals(Id, ESearchCase::IgnoreCase);
        });
}

int32 FRotorlineAircraftCatalog::SuitabilityForMissionType(
    const FRotorlineAircraftDefinition& Aircraft,
    const FString& MissionType)
{
    const FString Normalized = MissionType.ToLower();
    if (Normalized.Contains(TEXT("medevac")) || Normalized.Contains(TEXT("dustoff")))
    {
        return Aircraft.MissionSuitability.Medevac;
    }
    if (Normalized.Contains(TEXT("rescue")) || Normalized.Contains(TEXT("seeker")))
    {
        return Aircraft.MissionSuitability.Rescue;
    }
    if (Normalized.Contains(TEXT("recovery")) || Normalized.Contains(TEXT("recover")))
    {
        return FMath::Max(Aircraft.MissionSuitability.Rescue, Aircraft.MissionSuitability.Medevac);
    }
    if (Normalized.Contains(TEXT("evacuation")) || Normalized.Contains(TEXT("shepherd")))
    {
        return FMath::Max3(
            Aircraft.MissionSuitability.Rescue,
            Aircraft.MissionSuitability.Medevac,
            Aircraft.MissionSuitability.Cargo);
    }
    if (Normalized.Contains(TEXT("cargo")) || Normalized.Contains(TEXT("supply")) || Normalized.Contains(TEXT("transport")))
    {
        return Aircraft.MissionSuitability.Cargo;
    }
    if (Normalized.Contains(TEXT("fire")) || Normalized.Contains(TEXT("island-hop")) ||
        Normalized.Contains(TEXT("relay")) || Normalized.Contains(TEXT("wayfarer")) ||
        Normalized.Contains(TEXT("lamplight")))
    {
        return Aircraft.MissionSuitability.Cargo;
    }
    if (Normalized.Contains(TEXT("recon")) || Normalized.Contains(TEXT("scout")) || Normalized.Contains(TEXT("patrol")))
    {
        return Aircraft.MissionSuitability.Recon;
    }
    if (Normalized.Contains(TEXT("escort")) || Normalized.Contains(TEXT("protect")))
    {
        return Aircraft.MissionSuitability.Escort;
    }
    if (Normalized.Contains(TEXT("attack")) || Normalized.Contains(TEXT("strike")) || Normalized.Contains(TEXT("combat")))
    {
        return Aircraft.MissionSuitability.Attack;
    }
    if (Normalized.Contains(TEXT("gauntlet")) || Normalized.Contains(TEXT("longsword")))
    {
        return FMath::Max(Aircraft.MissionSuitability.Attack, Aircraft.MissionSuitability.Escort);
    }
    return FMath::Max3(
        Aircraft.MissionSuitability.Rescue,
        Aircraft.MissionSuitability.Recon,
        Aircraft.MissionSuitability.Attack);
}

FString FRotorlineAircraftCatalog::Stars(int32 Rating)
{
    const int32 ClampedRating = FMath::Clamp(Rating, 1, 5);
    return FString::ChrN(ClampedRating, TEXT('*')) +
        FString::ChrN(5 - ClampedRating, TEXT('-'));
}
