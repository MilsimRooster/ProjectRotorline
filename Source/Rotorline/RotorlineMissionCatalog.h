#pragma once

#include "CoreMinimal.h"

enum class ERotorlineCraftType : uint8
{
    SupportHuey,
    AttackMD500
};

struct FRotorlineObjectiveDefinition
{
    FString Kind;
    FString Text;
    bool bHasLocation = false;
    FVector BrowserLocation = FVector::ZeroVector;
    // The UE5 island is much larger and more detailed than the legacy browser
    // map.  Explicit world coordinates keep authored routes on real landmarks
    // and level mission pads instead of re-projecting every stop at runtime.
    bool bHasWorldLocation = false;
    FVector WorldLocation = FVector::ZeroVector;
    FString Site;
    float Radius = 35.0f;
    FString Target;
    // Mission-result credit is authored data. Never infer gameplay state from
    // player-facing objective prose; copy edits must not disable awards.
    int32 RescueCount = 0;
    int32 CargoDeliveryCount = 0;
    bool bSlingLoadDelivery = false;
    bool bHasMaxAltitude = false;
    float MaxAltitude = 0.0f;
};

struct FRotorlineMissionDefinition
{
    FString Id;
    FString Title;
    FString Callsign;
    FString Type;
    FString Briefing;
    FString Weather;
    FString TimeOfDay;
    FString RecommendedCraft;
    int32 Difficulty = 1;
    int32 TimeTarget = 0;
    int32 Reward = 0;
    int32 Unlock = 0;
    bool bRequiresWeapons = false;
    // Normal missions deliberately serialize hostile helicopter encounters.
    // Zero disables ambient/transit aircraft; values above one opt into a
    // multi-aircraft fight.
    int32 MaxConcurrentEnemyHelicopters = 1;
    TArray<FRotorlineObjectiveDefinition> Objectives;
};

class FRotorlineMissionCatalog
{
public:
    static bool Load(TArray<FRotorlineMissionDefinition>& OutMissions, FString& OutError);
    static FString CraftDisplayName(ERotorlineCraftType Craft);
};
