#pragma once

#include "CoreMinimal.h"
#include "RotorlineAwards.generated.h"

USTRUCT()
struct FRotorlinePlayerAwardRecord
{
    GENERATED_BODY()

    UPROPERTY()
    FString AwardId;

    UPROPERTY()
    FString FirstEarnedUtc;

    UPROPERTY()
    FString FirstMissionId;

    UPROPERTY()
    FString FirstMissionTitle;

    UPROPERTY()
    float BestAssociatedStat = 0.0f;

    UPROPERTY()
    int32 TimesEarned = 0;
};

USTRUCT()
struct FRotorlineCareerStatistics
{
    GENERATED_BODY()

    UPROPERTY() int32 MissionsStarted = 0;
    UPROPERTY() int32 MissionsCompleted = 0;
    UPROPERTY() int32 MissionsFailed = 0;
    UPROPERTY() float TotalFlightTimeSeconds = 0.0f;
    UPROPERTY() float TotalDistanceMeters = 0.0f;
    UPROPERTY() int32 ValidTakeoffs = 0;
    UPROPERTY() int32 SuccessfulLandings = 0;
    UPROPERTY() int32 HardLandings = 0;
    UPROPERTY() int32 CrashCount = 0;
    UPROPERTY() int32 CiviliansRescued = 0;
    UPROPERTY() int32 SoldiersRescued = 0;
    UPROPERTY() int32 RescueLosses = 0;
    UPROPERTY() int32 PerfectRescueMissions = 0;
    UPROPERTY() int32 CargoLoadsTransported = 0;
    UPROPERTY() float CargoWeightTransportedKg = 0.0f;
    UPROPERTY() int32 PrecisionCargoDeliveries = 0;
    UPROPERTY() float CargoDamage = 0.0f;
    UPROPERTY() int32 EnemyVehiclesDestroyed = 0;
    UPROPERTY() int32 EnemyHelicoptersDestroyed = 0;
    UPROPERTY() int32 BasesCaptured = 0;
    UPROPERTY() int32 ShotsFired = 0;
    UPROPERTY() int32 WeaponHits = 0;
    UPROPERTY() int32 MissilesDodged = 0;
    UPROPERTY() float TimeUnderEnemyFireSeconds = 0.0f;
    UPROPERTY() float DetectionTimeSeconds = 0.0f;
    UPROPERTY() int32 OptionalObjectivesCompleted = 0;
    UPROPERTY() int32 HiddenLocationsDiscovered = 0;
    UPROPERTY() int32 ConsecutiveSuccessfulMissions = 0;
    UPROPERTY() int32 BestSuccessfulMissionStreak = 0;
    UPROPERTY() int32 BestMissionScore = 0;
    UPROPERTY() int32 FiveStarMissions = 0;
    UPROPERTY() int32 AwardsEarned = 0;
    UPROPERTY() float CampaignCompletionPercent = 0.0f;
    UPROPERTY() TArray<FString> CompletedMissionTypes;
    UPROPERTY() TArray<FString> UniqueRegionsExplored;
    UPROPERTY() TArray<FString> UniqueIslandsVisited;
    UPROPERTY() TArray<FString> FlightPathsUsed;
};

// Controller-owned, sortie-scoped telemetry. Values are collected during live
// flight and finalized exactly once at the mission success/failure boundary.
struct FRotorlineMissionResults
{
    FString MissionId;
    FString MissionTitle;
    FString MissionCallsign;
    FString MissionType;
    FString AircraftName;
    FString Weather;
    float ElapsedSeconds = 0.0f;
    float FlightTimeSeconds = 0.0f;
    float DistanceFlownMeters = 0.0f;
    int32 Difficulty = 1;
    int32 PrimaryObjectivesCompleted = 0;
    int32 PrimaryObjectivesTotal = 0;
    int32 OptionalObjectivesCompleted = 0;
    int32 OptionalObjectivesTotal = 0;
    int32 EnemyHelicoptersDestroyed = 0;
    int32 GroundEnemiesDestroyed = 0;
    int32 BasesCaptured = 0;
    int32 CiviliansRescued = 0;
    int32 SoldiersRescued = 0;
    int32 RescueTargetsAvailable = 0;
    int32 RescueLosses = 0;
    int32 CargoDelivered = 0;
    float CargoWeightKg = 0.0f;
    float CargoDamage = 0.0f;
    float SlingLoadAccuracyPercent = 0.0f;
    float DamageTaken = 0.0f;
    float AircraftHealth = 0.0f;
    float AircraftMaxHealth = 0.0f;
    float FuelRemainingPercent = 100.0f;
    float TimeBelowSafeAltitudeSeconds = 0.0f;
    float ClosestObstacleClearanceMeters = 100000.0f;
    int32 NearMisses = 0;
    int32 WeaponShotsFired = 0;
    int32 WeaponHits = 0;
    int32 MissilesDodged = 0;
    float TimeUnderEnemyFireSeconds = 0.0f;
    float DetectionTimeSeconds = 0.0f;
    float LandingVerticalSpeedMps = 0.0f;
    float LandingLateralSpeedMps = 0.0f;
    float LandingAttitudeDegrees = 0.0f;
    float LandingAccuracyMeters = 100000.0f;
    float AbruptControlSeconds = 0.0f;
    float StableHoverSeconds = 0.0f;
    float SecondsFromFailureAtCompletion = 100000.0f;
    int32 UniqueMapRegionsExplored = 0;
    int32 IslandsVisited = 0;
    int32 FlightPathsUsed = 0;
    int32 ExperienceAwarded = 0;
    int32 FinalScore = 0;
    int32 StarRating = 0;
    float CampaignCompletionPercent = 0.0f;
    bool bMissionSucceeded = false;
    bool bMissionFailed = false;
    bool bMeaningfulPartialSuccess = false;
    bool bValidTakeoff = false;
    bool bValidLanding = false;
    bool bSafeLanding = false;
    bool bHardLanding = false;
    bool bCrashed = false;
    bool bAllRequiredPersonnelDelivered = false;
    bool bSevereWeather = false;
    bool bTightClearanceControlled = false;
    bool bStealthApproach = false;
    bool bSmokeOrDecoyUsed = false;
    bool bCombatSupportMission = false;
    bool bConstructionMission = false;
    bool bBaseCaptureMission = false;
    bool bFinalCampaignMission = false;
    bool bAircraftConditionTracked = false;
    bool bWeaponsTracked = false;
    bool bCivilianRescueTracked = false;
    bool bCargoTracked = false;
    bool bSlingLoadTracked = false;
    bool bProfileApplied = false;
};

struct FRotorlineAwardRule
{
    FString Stat;
    FString Op;
    double Value = 0.0;
    double Max = 0.0;
    FString Text;
};

struct FRotorlineAwardRuleGroup
{
    FString Mode = TEXT("all");
    TArray<FRotorlineAwardRule> Rules;
};

struct FRotorlineAwardDefinition
{
    FString Id;
    FString DisplayName;
    FString Description;
    FString Hint;
    FString PatchAsset;
    FString Category;
    FString Rarity;
    FString GroupMode = TEXT("all");
    FString AssociatedStat;
    bool bHidden = false;
    bool bRepeatable = false;
    TArray<FRotorlineAwardRuleGroup> Groups;
};

struct FRotorlineAwardEvaluation
{
    FString AwardId;
    FString Reason;
    FString RelevantStat;
    float AssociatedStatValue = 0.0f;
    bool bNewlyUnlocked = false;
};

class FRotorlineAwardSystem
{
public:
    bool Load(FString& OutError);
    const TArray<FRotorlineAwardDefinition>& GetDefinitions() const { return Definitions; }
    const FRotorlineAwardDefinition* FindDefinition(const FString& AwardId) const;
    TArray<FRotorlineAwardEvaluation> Evaluate(
        const FRotorlineMissionResults& Mission,
        const FRotorlineCareerStatistics& Career,
        TMap<FString, FRotorlinePlayerAwardRecord>& AwardRecords,
        bool bCommitRecords) const;
    FString ExplainAward(
        const FRotorlineAwardDefinition& Definition,
        const FRotorlineMissionResults& Mission,
        const FRotorlineCareerStatistics& Career) const;

private:
    bool EvaluateDefinition(
        const FRotorlineAwardDefinition& Definition,
        const FRotorlineMissionResults& Mission,
        const FRotorlineCareerStatistics& Career,
        FString& OutReason,
        float& OutAssociatedValue) const;
    bool EvaluateRule(
        const FRotorlineAwardRule& Rule,
        const FRotorlineMissionResults& Mission,
        const FRotorlineCareerStatistics& Career,
        FString& OutExplanation) const;
    bool ResolveNumericStat(
        const FString& Stat,
        const FRotorlineMissionResults& Mission,
        const FRotorlineCareerStatistics& Career,
        double& OutValue) const;
    bool ResolveTextStat(
        const FString& Stat,
        const FRotorlineMissionResults& Mission,
        FString& OutValue) const;

    TArray<FRotorlineAwardDefinition> Definitions;
};
