#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RotorlineAwards.h"
#include "RotorlineProfileSave.generated.h"

UCLASS()
class ROTORLINE_API URotorlineProfileSave : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    int32 Reputation = 0;

    UPROPERTY()
    TArray<FString> CompletedMissions;

    UPROPERTY()
    TMap<FString, float> BestMissionTimes;

    // Mission 19 unlocks the final Jeep search. Mission 20's first successful
    // Bell flight permanently reveals both vehicles in the hangar.
    UPROPERTY()
    bool bJeepPermanentlyUnlocked = false;

    UPROPERTY()
    bool bBell222Discovered = false;

    // Versioned additively so profiles created before the awards system remain
    // readable with zero-initialized career and patch data.
    UPROPERTY()
    int32 AwardsSaveVersion = 1;

    UPROPERTY()
    FRotorlineCareerStatistics CareerStatistics;

    UPROPERTY()
    TMap<FString, FRotorlinePlayerAwardRecord> AwardRecords;

    // Versioned so existing profiles receive the balanced mix once without
    // overwriting later player adjustments.
    UPROPERTY()
    int32 AudioMixVersion = 0;

    // Persistent player mix. Radio remains prominent while engines, music,
    // ambience, and weapons sit at comfortable supporting levels.
    UPROPERTY()
    float MasterVolume = 0.75f;

    // World ambience is intentionally conservative. The original global wind
    // actor bypassed the player mixer and dominated every other channel.
    UPROPERTY()
    float EnvironmentVolume = 0.28f;

    UPROPERTY()
    float EngineVolume = 0.62f;

    UPROPERTY()
    float MusicVolume = 0.48f;

    UPROPERTY()
    float RadioVolume = 0.78f;

    UPROPERTY()
    float WeaponsVolume = 0.42f;

    // DualSense/controller preference only. Standard flight behavior maps a
    // forward left-stick push to nose-down pitch; players may explicitly
    // reverse that single axis from the controller setup page.
    UPROPERTY()
    bool bGamepadPitchInverted = false;
};
