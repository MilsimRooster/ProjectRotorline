#include "RotorlineMissionCatalog.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString MissionStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        FString Value;
        Object->TryGetStringField(Name, Value);
        return Value;
    }

    int32 MissionIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        double Value = 0.0;
        Object->TryGetNumberField(Name, Value);
        return FMath::RoundToInt(Value);
    }
}

bool FRotorlineMissionCatalog::Load(TArray<FRotorlineMissionDefinition>& OutMissions, FString& OutError)
{
    OutMissions.Reset();
    const FString MissionPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/BrowserMissions.json"));
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *MissionPath))
    {
        OutError = FString::Printf(TEXT("Could not read %s"), *MissionPath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Mission catalog JSON is invalid");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* MissionValues = nullptr;
    if (!Root->TryGetArrayField(TEXT("missions"), MissionValues) || MissionValues == nullptr)
    {
        OutError = TEXT("Mission catalog has no missions array");
        return false;
    }

    for (const TSharedPtr<FJsonValue>& MissionValue : *MissionValues)
    {
        const TSharedPtr<FJsonObject> MissionObject = MissionValue->AsObject();
        if (!MissionObject.IsValid())
        {
            continue;
        }

        FRotorlineMissionDefinition Mission;
        Mission.Id = MissionStringField(MissionObject, TEXT("id"));
        Mission.Title = MissionStringField(MissionObject, TEXT("title"));
        Mission.Callsign = MissionStringField(MissionObject, TEXT("callsign"));
        Mission.Type = MissionStringField(MissionObject, TEXT("type"));
        Mission.Briefing = MissionStringField(MissionObject, TEXT("briefing"));
        Mission.Weather = MissionStringField(MissionObject, TEXT("weather"));
        Mission.TimeOfDay = MissionStringField(MissionObject, TEXT("time"));
        Mission.RecommendedCraft = MissionStringField(MissionObject, TEXT("recommendedCraft"));
        Mission.Difficulty = MissionIntField(MissionObject, TEXT("difficulty"));
        Mission.TimeTarget = MissionIntField(MissionObject, TEXT("timeTarget"));
        Mission.Reward = MissionIntField(MissionObject, TEXT("reward"));
        Mission.Unlock = MissionIntField(MissionObject, TEXT("unlock"));
        MissionObject->TryGetBoolField(TEXT("requiresWeapons"), Mission.bRequiresWeapons);
        double MaxConcurrentEnemyHelicopters = 1.0;
        if (MissionObject->TryGetNumberField(TEXT("maxConcurrentEnemyHelicopters"), MaxConcurrentEnemyHelicopters))
        {
            Mission.MaxConcurrentEnemyHelicopters = FMath::Max(0, FMath::RoundToInt(MaxConcurrentEnemyHelicopters));
        }

        const TArray<TSharedPtr<FJsonValue>>* ObjectiveValues = nullptr;
        if (MissionObject->TryGetArrayField(TEXT("objectives"), ObjectiveValues) && ObjectiveValues)
        {
            for (const TSharedPtr<FJsonValue>& ObjectiveValue : *ObjectiveValues)
            {
                const TSharedPtr<FJsonObject> ObjectiveObject = ObjectiveValue->AsObject();
                if (!ObjectiveObject.IsValid())
                {
                    continue;
                }

                FRotorlineObjectiveDefinition Objective;
                Objective.Kind = MissionStringField(ObjectiveObject, TEXT("kind"));
                Objective.Text = MissionStringField(ObjectiveObject, TEXT("text"));
                Objective.Text.ReplaceInline(TEXT("[I]"), TEXT("[AUTO START]"));
                Objective.Text.ReplaceInline(TEXT("Hold E"), TEXT("Hold R2"));
                Objective.Text.ReplaceInline(TEXT("[F]"), TEXT("[X]"));
                Objective.Text.ReplaceInline(TEXT("[R1 / SPACE]"), TEXT("[R1]"));
                Objective.Target = MissionStringField(ObjectiveObject, TEXT("target"));
                Objective.Site = MissionStringField(ObjectiveObject, TEXT("site"));
                double RescueCount = 0.0;
                if (ObjectiveObject->TryGetNumberField(TEXT("rescueCount"), RescueCount))
                {
                    Objective.RescueCount = FMath::Max(0, FMath::RoundToInt(RescueCount));
                }
                double CargoDeliveryCount = 0.0;
                if (ObjectiveObject->TryGetNumberField(TEXT("cargoDeliveryCount"), CargoDeliveryCount))
                {
                    Objective.CargoDeliveryCount = FMath::Max(0, FMath::RoundToInt(CargoDeliveryCount));
                }
                ObjectiveObject->TryGetBoolField(TEXT("slingLoadDelivery"), Objective.bSlingLoadDelivery);
                double Radius = Objective.Radius;
                if (ObjectiveObject->TryGetNumberField(TEXT("radius"), Radius))
                {
                    Objective.Radius = static_cast<float>(Radius);
                }
                double MaxAltitude = 0.0;
                if (ObjectiveObject->TryGetNumberField(TEXT("maxAltitude"), MaxAltitude))
                {
                    Objective.bHasMaxAltitude = true;
                    Objective.MaxAltitude = static_cast<float>(MaxAltitude);
                }

                const TSharedPtr<FJsonObject>* LocationObject = nullptr;
                if (ObjectiveObject->TryGetObjectField(TEXT("location"), LocationObject) && LocationObject && LocationObject->IsValid())
                {
                    double X = 0.0;
                    double Y = 0.0;
                    double Z = 0.0;
                    (*LocationObject)->TryGetNumberField(TEXT("x"), X);
                    (*LocationObject)->TryGetNumberField(TEXT("y"), Y);
                    (*LocationObject)->TryGetNumberField(TEXT("z"), Z);
                    Objective.bHasLocation = true;
                    Objective.BrowserLocation = FVector(X, Y, Z);
                }
                const TSharedPtr<FJsonObject>* WorldLocationObject = nullptr;
                if (ObjectiveObject->TryGetObjectField(TEXT("worldLocation"), WorldLocationObject) &&
                    WorldLocationObject && WorldLocationObject->IsValid())
                {
                    double X = 0.0;
                    double Y = 0.0;
                    double Z = 0.0;
                    (*WorldLocationObject)->TryGetNumberField(TEXT("x"), X);
                    (*WorldLocationObject)->TryGetNumberField(TEXT("y"), Y);
                    (*WorldLocationObject)->TryGetNumberField(TEXT("z"), Z);
                    // Explicit UE world coordinates are a complete objective
                    // location, not merely an alternate coordinate payload.
                    // Without this flag, worldLocation-only reach/land steps
                    // are retained but intentionally skipped by runtime POI
                    // spawning and completion checks.
                    Objective.bHasLocation = true;
                    Objective.bHasWorldLocation = true;
                    Objective.WorldLocation = FVector(X, Y, Z);
                }
                Mission.Objectives.Add(MoveTemp(Objective));
            }
        }

        if (!Mission.Id.IsEmpty() && Mission.Objectives.Num() > 0)
        {
            OutMissions.Add(MoveTemp(Mission));
        }
    }

    if (OutMissions.IsEmpty())
    {
        OutError = TEXT("Mission catalog contained no usable missions");
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_MISSIONS|LOADED|count=%d|path=%s"), OutMissions.Num(), *MissionPath);
    return true;
}

FString FRotorlineMissionCatalog::CraftDisplayName(ERotorlineCraftType Craft)
{
    return Craft == ERotorlineCraftType::AttackMD500 ? TEXT("MD-500 DEFENDER") : TEXT("UH-1 HUEY");
}
