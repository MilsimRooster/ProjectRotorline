#include "RotorlineAwards.h"

#include "Dom/JsonObject.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString AwardStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        FString Value;
        Object->TryGetStringField(Name, Value);
        return Value;
    }

    double AwardNumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, double DefaultValue = 0.0)
    {
        double Value = DefaultValue;
        Object->TryGetNumberField(Name, Value);
        return Value;
    }

    FString FormatStat(const FString& Stat, double Value)
    {
        if (Stat.Contains(TEXT("percent"), ESearchCase::IgnoreCase) ||
            Stat.Contains(TEXT("accuracy"), ESearchCase::IgnoreCase))
        {
            return FString::Printf(TEXT("%.1f%%"), Value);
        }
        if (Stat.Contains(TEXT("seconds"), ESearchCase::IgnoreCase))
        {
            return FString::Printf(TEXT("%.1f sec"), Value);
        }
        if (Stat.Contains(TEXT("meters"), ESearchCase::IgnoreCase) ||
            Stat.Contains(TEXT("clearance"), ESearchCase::IgnoreCase))
        {
            return FString::Printf(TEXT("%.1f m"), Value);
        }
        if (Stat.Contains(TEXT("mps"), ESearchCase::IgnoreCase))
        {
            return FString::Printf(TEXT("%.1f m/s"), Value);
        }
        if (Stat.Contains(TEXT("kg"), ESearchCase::IgnoreCase))
        {
            return FString::Printf(TEXT("%.1f kg"), Value);
        }
        return FMath::IsNearlyEqual(Value, FMath::RoundToDouble(Value), 0.001)
            ? FString::Printf(TEXT("%.0f"), Value)
            : FString::Printf(TEXT("%.1f"), Value);
    }

    FString HumanizeStatName(const FString& Stat)
    {
        FString Field = Stat;
        int32 DotIndex = INDEX_NONE;
        if (Field.FindLastChar(TEXT('.'), DotIndex))
        {
            Field = Field.Mid(DotIndex + 1);
        }
        if (Field.Len() > 1 && Field[0] == TEXT('b') && FChar::IsUpper(Field[1]))
        {
            Field.RightChopInline(1);
        }

        FString Result;
        for (int32 Index = 0; Index < Field.Len(); ++Index)
        {
            const TCHAR Character = Field[Index];
            if (Index > 0 && FChar::IsUpper(Character) &&
                (FChar::IsLower(Field[Index - 1]) || FChar::IsDigit(Field[Index - 1])))
            {
                Result.AppendChar(TEXT(' '));
            }
            Result.AppendChar(FChar::ToLower(Character));
        }
        return Result;
    }

    bool IsBooleanStat(const FString& Stat)
    {
        int32 DotIndex = INDEX_NONE;
        const FString Field = Stat.FindLastChar(TEXT('.'), DotIndex) ? Stat.Mid(DotIndex + 1) : Stat;
        return Field.Len() > 1 && Field[0] == TEXT('b') && FChar::IsUpper(Field[1]);
    }

    FString BooleanAchievementPhrase(const FString& Stat, bool bExpectedTrue, const FString& AuthoredText)
    {
        if (!AuthoredText.IsEmpty())
        {
            return AuthoredText;
        }

        if (Stat.Equals(TEXT("mission.bMissionSucceeded"), ESearchCase::IgnoreCase)) return TEXT("completing the mission");
        if (Stat.Equals(TEXT("mission.bMissionFailed"), ESearchCase::IgnoreCase)) return TEXT("reaching the failed-objective outcome");
        if (Stat.Equals(TEXT("mission.bMeaningfulPartialSuccess"), ESearchCase::IgnoreCase)) return TEXT("securing meaningful results despite the failed objective");
        if (Stat.Equals(TEXT("mission.bValidTakeoff"), ESearchCase::IgnoreCase)) return TEXT("completing a valid takeoff");
        if (Stat.Equals(TEXT("mission.bValidLanding"), ESearchCase::IgnoreCase)) return TEXT("completing a valid landing");
        if (Stat.Equals(TEXT("mission.bSafeLanding"), ESearchCase::IgnoreCase)) return TEXT("landing safely");
        if (Stat.Equals(TEXT("mission.bCrashed"), ESearchCase::IgnoreCase)) return bExpectedTrue ? TEXT("surviving a crash") : TEXT("avoiding a crash");
        if (Stat.Equals(TEXT("mission.bAllRequiredPersonnelDelivered"), ESearchCase::IgnoreCase)) return TEXT("delivering all required personnel");
        if (Stat.Equals(TEXT("mission.bSevereWeather"), ESearchCase::IgnoreCase)) return TEXT("operating in severe weather");
        if (Stat.Equals(TEXT("mission.bTightClearanceControlled"), ESearchCase::IgnoreCase)) return TEXT("holding control through a tight clearance");
        if (Stat.Equals(TEXT("mission.bStealthApproach"), ESearchCase::IgnoreCase)) return TEXT("using a stealth approach");
        if (Stat.Equals(TEXT("mission.bSmokeOrDecoyUsed"), ESearchCase::IgnoreCase)) return TEXT("using smoke or decoys");
        if (Stat.Equals(TEXT("mission.bCombatSupportMission"), ESearchCase::IgnoreCase)) return TEXT("completing combat-support duty");
        if (Stat.Equals(TEXT("mission.bConstructionMission"), ESearchCase::IgnoreCase)) return TEXT("completing a construction operation");
        if (Stat.Equals(TEXT("mission.bBaseCaptureMission"), ESearchCase::IgnoreCase)) return TEXT("completing a base-capture operation");
        if (Stat.Equals(TEXT("mission.bFinalCampaignMission"), ESearchCase::IgnoreCase)) return TEXT("completing the final campaign operation");
        if (Stat.Equals(TEXT("mission.bHardLanding"), ESearchCase::IgnoreCase)) return bExpectedTrue ? TEXT("making a hard landing") : TEXT("avoiding a hard landing");

        const FString Label = HumanizeStatName(Stat);
        return bExpectedTrue ? Label : FString::Printf(TEXT("no %s"), *Label);
    }

    FString JoinAchievementReasons(const TArray<FString>& Reasons)
    {
        if (Reasons.IsEmpty()) return FString();
        if (Reasons.Num() == 1) return Reasons[0];
        if (Reasons.Num() == 2) return FString::Printf(TEXT("%s and %s"), *Reasons[0], *Reasons[1]);

        FString Result;
        for (int32 Index = 0; Index < Reasons.Num(); ++Index)
        {
            if (Index > 0)
            {
                Result += Index == Reasons.Num() - 1 ? TEXT(", and ") : TEXT(", ");
            }
            Result += Reasons[Index];
        }
        return Result;
    }

    FString RequirementOperatorText(const FString& Op)
    {
        if (Op.Equals(TEXT("gte"), ESearchCase::IgnoreCase)) return TEXT("at least");
        if (Op.Equals(TEXT("lte"), ESearchCase::IgnoreCase)) return TEXT("no more than");
        if (Op.Equals(TEXT("gt"), ESearchCase::IgnoreCase)) return TEXT("more than");
        if (Op.Equals(TEXT("lt"), ESearchCase::IgnoreCase)) return TEXT("less than");
        if (Op.Equals(TEXT("eq"), ESearchCase::IgnoreCase)) return TEXT("exactly");
        return TEXT("within");
    }
}

bool FRotorlineAwardSystem::Load(FString& OutError)
{
    Definitions.Reset();
    const FString AwardPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Awards.json"));
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *AwardPath))
    {
        OutError = FString::Printf(TEXT("Could not read %s"), *AwardPath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Awards catalog JSON is invalid");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* AwardValues = nullptr;
    if (!Root->TryGetArrayField(TEXT("awards"), AwardValues) || !AwardValues)
    {
        OutError = TEXT("Awards catalog has no awards array");
        return false;
    }

    TSet<FString> SeenIds;
    for (const TSharedPtr<FJsonValue>& AwardValue : *AwardValues)
    {
        const TSharedPtr<FJsonObject> AwardObject = AwardValue->AsObject();
        if (!AwardObject.IsValid()) continue;

        FRotorlineAwardDefinition Definition;
        Definition.Id = AwardStringField(AwardObject, TEXT("id"));
        Definition.DisplayName = AwardStringField(AwardObject, TEXT("name"));
        Definition.Description = AwardStringField(AwardObject, TEXT("description"));
        Definition.Hint = AwardStringField(AwardObject, TEXT("hint"));
        Definition.PatchAsset = AwardStringField(AwardObject, TEXT("patchAsset"));
        Definition.Category = AwardStringField(AwardObject, TEXT("category"));
        Definition.Rarity = AwardStringField(AwardObject, TEXT("rarity"));
        Definition.GroupMode = AwardStringField(AwardObject, TEXT("groupMode"));
        Definition.AssociatedStat = AwardStringField(AwardObject, TEXT("associatedStat"));
        if (Definition.GroupMode.IsEmpty()) Definition.GroupMode = TEXT("all");
        AwardObject->TryGetBoolField(TEXT("hidden"), Definition.bHidden);
        AwardObject->TryGetBoolField(TEXT("repeatable"), Definition.bRepeatable);

        const TArray<TSharedPtr<FJsonValue>>* GroupValues = nullptr;
        if (AwardObject->TryGetArrayField(TEXT("groups"), GroupValues) && GroupValues)
        {
            for (const TSharedPtr<FJsonValue>& GroupValue : *GroupValues)
            {
                const TSharedPtr<FJsonObject> GroupObject = GroupValue->AsObject();
                if (!GroupObject.IsValid()) continue;
                FRotorlineAwardRuleGroup Group;
                Group.Mode = AwardStringField(GroupObject, TEXT("mode"));
                if (Group.Mode.IsEmpty()) Group.Mode = TEXT("all");

                const TArray<TSharedPtr<FJsonValue>>* RuleValues = nullptr;
                if (GroupObject->TryGetArrayField(TEXT("conditions"), RuleValues) && RuleValues)
                {
                    for (const TSharedPtr<FJsonValue>& RuleValue : *RuleValues)
                    {
                        const TSharedPtr<FJsonObject> RuleObject = RuleValue->AsObject();
                        if (!RuleObject.IsValid()) continue;
                        FRotorlineAwardRule Rule;
                        Rule.Stat = AwardStringField(RuleObject, TEXT("stat"));
                        Rule.Op = AwardStringField(RuleObject, TEXT("op"));
                        Rule.Value = AwardNumberField(RuleObject, TEXT("value"));
                        Rule.Max = AwardNumberField(RuleObject, TEXT("max"));
                        Rule.Text = AwardStringField(RuleObject, TEXT("text"));
                        if (!Rule.Stat.IsEmpty() && !Rule.Op.IsEmpty()) Group.Rules.Add(MoveTemp(Rule));
                    }
                }
                if (!Group.Rules.IsEmpty()) Definition.Groups.Add(MoveTemp(Group));
            }
        }

        if (Definition.Id.IsEmpty() || Definition.DisplayName.IsEmpty() || Definition.Groups.IsEmpty() || SeenIds.Contains(Definition.Id))
        {
            UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_AWARDS|DEFINITION_SKIPPED|id=%s|name=%s"), *Definition.Id, *Definition.DisplayName);
            continue;
        }
        SeenIds.Add(Definition.Id);
        Definitions.Add(MoveTemp(Definition));
    }

    if (Definitions.IsEmpty())
    {
        OutError = TEXT("Awards catalog contained no usable definitions");
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("ROTORLINE_AWARDS|LOADED|count=%d|path=%s"), Definitions.Num(), *AwardPath);
    return true;
}

const FRotorlineAwardDefinition* FRotorlineAwardSystem::FindDefinition(const FString& AwardId) const
{
    return Definitions.FindByPredicate([&AwardId](const FRotorlineAwardDefinition& Definition)
    {
        return Definition.Id.Equals(AwardId, ESearchCase::IgnoreCase);
    });
}

TArray<FRotorlineAwardEvaluation> FRotorlineAwardSystem::Evaluate(
    const FRotorlineMissionResults& Mission,
    const FRotorlineCareerStatistics& Career,
    TMap<FString, FRotorlinePlayerAwardRecord>& AwardRecords,
    bool bCommitRecords) const
{
    TArray<FRotorlineAwardEvaluation> Earned;
    for (const FRotorlineAwardDefinition& Definition : Definitions)
    {
        const FRotorlinePlayerAwardRecord* Existing = AwardRecords.Find(Definition.Id);
        if (Existing && Existing->TimesEarned > 0 && !Definition.bRepeatable) continue;

        FString Reason;
        float AssociatedValue = 0.0f;
        if (!EvaluateDefinition(Definition, Mission, Career, Reason, AssociatedValue)) continue;

        FRotorlineAwardEvaluation Evaluation;
        Evaluation.AwardId = Definition.Id;
        Evaluation.Reason = Reason;
        Evaluation.RelevantStat = Definition.AssociatedStat;
        Evaluation.AssociatedStatValue = AssociatedValue;
        Evaluation.bNewlyUnlocked = !Existing || Existing->TimesEarned <= 0;
        Earned.Add(Evaluation);

        if (bCommitRecords)
        {
            FRotorlinePlayerAwardRecord& Record = AwardRecords.FindOrAdd(Definition.Id);
            if (Record.TimesEarned <= 0)
            {
                Record.AwardId = Definition.Id;
                Record.FirstEarnedUtc = FDateTime::UtcNow().ToIso8601();
                Record.FirstMissionId = Mission.MissionId;
                Record.FirstMissionTitle = Mission.MissionTitle;
            }
            ++Record.TimesEarned;
            Record.BestAssociatedStat = FMath::Max(Record.BestAssociatedStat, AssociatedValue);
        }
    }
    return Earned;
}

FString FRotorlineAwardSystem::ExplainAward(
    const FRotorlineAwardDefinition& Definition,
    const FRotorlineMissionResults& Mission,
    const FRotorlineCareerStatistics& Career) const
{
    FString Reason;
    float Value = 0.0f;
    if (EvaluateDefinition(Definition, Mission, Career, Reason, Value)) return Reason;
    return Reason.IsEmpty() ? TEXT("Requirements not yet met.") : Reason;
}

bool FRotorlineAwardSystem::EvaluateDefinition(
    const FRotorlineAwardDefinition& Definition,
    const FRotorlineMissionResults& Mission,
    const FRotorlineCareerStatistics& Career,
    FString& OutReason,
    float& OutAssociatedValue) const
{
    TArray<FString> PassingReasons;
    TArray<FString> FailureReasons;
    int32 PassingGroups = 0;
    for (const FRotorlineAwardRuleGroup& Group : Definition.Groups)
    {
        int32 PassingRules = 0;
        TArray<FString> GroupPassing;
        TArray<FString> GroupFailures;
        for (const FRotorlineAwardRule& Rule : Group.Rules)
        {
            FString Explanation;
            if (EvaluateRule(Rule, Mission, Career, Explanation))
            {
                ++PassingRules;
                if (!Explanation.IsEmpty()) GroupPassing.Add(Explanation);
            }
            else if (!Explanation.IsEmpty())
            {
                GroupFailures.Add(Explanation);
            }
        }
        const bool bGroupPass = Group.Mode.Equals(TEXT("any"), ESearchCase::IgnoreCase)
            ? PassingRules > 0
            : PassingRules == Group.Rules.Num();
        if (bGroupPass)
        {
            ++PassingGroups;
            PassingReasons.Append(GroupPassing);
        }
        else
        {
            FailureReasons.Append(GroupFailures);
        }
    }

    const bool bPassed = Definition.GroupMode.Equals(TEXT("any"), ESearchCase::IgnoreCase)
        ? PassingGroups > 0
        : PassingGroups == Definition.Groups.Num();
    double AssociatedValue = 0.0;
    ResolveNumericStat(Definition.AssociatedStat, Mission, Career, AssociatedValue);
    OutAssociatedValue = static_cast<float>(AssociatedValue);
    if (bPassed)
    {
        OutReason = PassingReasons.IsEmpty()
            ? Definition.Description
            : FString::Printf(TEXT("Earned by %s."), *JoinAchievementReasons(PassingReasons));
    }
    else
    {
        OutReason = FailureReasons.IsEmpty()
            ? TEXT("Requirements not yet met.")
            : FString::Join(FailureReasons, TEXT("; "));
    }
    return bPassed;
}

bool FRotorlineAwardSystem::EvaluateRule(
    const FRotorlineAwardRule& Rule,
    const FRotorlineMissionResults& Mission,
    const FRotorlineCareerStatistics& Career,
    FString& OutExplanation) const
{
    if (Rule.Op.Equals(TEXT("contains"), ESearchCase::IgnoreCase) ||
        Rule.Op.Equals(TEXT("equalsText"), ESearchCase::IgnoreCase))
    {
        FString Actual;
        if (!ResolveTextStat(Rule.Stat, Mission, Actual))
        {
            UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_AWARDS|UNKNOWN_TEXT_STAT|stat=%s"), *Rule.Stat);
            OutExplanation = TEXT("achievement data is unavailable");
            return false;
        }
        const bool bPassed = Rule.Op.Equals(TEXT("contains"), ESearchCase::IgnoreCase)
            ? Actual.Contains(Rule.Text, ESearchCase::IgnoreCase)
            : Actual.Equals(Rule.Text, ESearchCase::IgnoreCase);
        if (Rule.Stat.Equals(TEXT("mission.type"), ESearchCase::IgnoreCase))
        {
            OutExplanation = bPassed
                ? FString::Printf(TEXT("completing a %s mission"), *Rule.Text)
                : FString::Printf(TEXT("requires a %s mission"), *Rule.Text);
        }
        else if (Rule.Stat.Equals(TEXT("mission.id"), ESearchCase::IgnoreCase))
        {
            OutExplanation = bPassed
                ? FString::Printf(TEXT("completing %s"), *Mission.MissionTitle)
                : TEXT("requires completion of its designated mission");
        }
        else
        {
            const FString Label = HumanizeStatName(Rule.Stat);
            OutExplanation = bPassed
                ? FString::Printf(TEXT("%s: %s"), *Label, *Actual)
                : FString::Printf(TEXT("%s must match %s"), *Label, *Rule.Text);
        }
        return bPassed;
    }

    double Actual = 0.0;
    if (!ResolveNumericStat(Rule.Stat, Mission, Career, Actual))
    {
        UE_LOG(LogTemp, Warning, TEXT("ROTORLINE_AWARDS|UNKNOWN_NUMERIC_STAT|stat=%s"), *Rule.Stat);
        OutExplanation = TEXT("achievement data is unavailable");
        return false;
    }

    bool bPassed = false;
    if (Rule.Op.Equals(TEXT("gte"), ESearchCase::IgnoreCase)) bPassed = Actual >= Rule.Value;
    else if (Rule.Op.Equals(TEXT("lte"), ESearchCase::IgnoreCase)) bPassed = Actual <= Rule.Value;
    else if (Rule.Op.Equals(TEXT("gt"), ESearchCase::IgnoreCase)) bPassed = Actual > Rule.Value;
    else if (Rule.Op.Equals(TEXT("lt"), ESearchCase::IgnoreCase)) bPassed = Actual < Rule.Value;
    else if (Rule.Op.Equals(TEXT("eq"), ESearchCase::IgnoreCase)) bPassed = FMath::IsNearlyEqual(Actual, Rule.Value, 0.001);
    else if (Rule.Op.Equals(TEXT("range"), ESearchCase::IgnoreCase)) bPassed = Actual >= Rule.Value && Actual <= Rule.Max;

    const FString ActualText = FormatStat(Rule.Stat, Actual);
    const FString ExpectedText = Rule.Op.Equals(TEXT("range"), ESearchCase::IgnoreCase)
        ? FString::Printf(TEXT("%s-%s"), *FormatStat(Rule.Stat, Rule.Value), *FormatStat(Rule.Stat, Rule.Max))
        : FormatStat(Rule.Stat, Rule.Value);

    if (IsBooleanStat(Rule.Stat))
    {
        const bool bExpectedTrue = Rule.Value >= 0.5;
        const FString Phrase = BooleanAchievementPhrase(Rule.Stat, bExpectedTrue, Rule.Text);
        OutExplanation = bPassed ? Phrase : FString::Printf(TEXT("requires %s"), *Phrase);
        return bPassed;
    }

    const FString Label = Rule.Text.IsEmpty() ? HumanizeStatName(Rule.Stat) : Rule.Text;
    if (bPassed)
    {
        if (Rule.Stat.Contains(TEXT("TimeUnderEnemyFireSeconds"), ESearchCase::IgnoreCase))
        {
            OutExplanation = FString::Printf(TEXT("surviving %s under enemy fire"), *ActualText);
        }
        else if (Rule.Stat.Contains(TEXT("HealthPercent"), ESearchCase::IgnoreCase))
        {
            OutExplanation = FString::Printf(TEXT("finishing with %s aircraft condition"), *ActualText);
        }
        else
        {
            OutExplanation = FString::Printf(TEXT("%s: %s"), *Label, *ActualText);
        }
    }
    else
    {
        OutExplanation = Rule.Op.Equals(TEXT("range"), ESearchCase::IgnoreCase)
            ? FString::Printf(TEXT("%s must be between %s"), *Label, *ExpectedText)
            : FString::Printf(TEXT("%s requires %s %s (current: %s)"),
                *Label, *RequirementOperatorText(Rule.Op), *ExpectedText, *ActualText);
    }
    return bPassed;
}

bool FRotorlineAwardSystem::ResolveTextStat(
    const FString& Stat,
    const FRotorlineMissionResults& Mission,
    FString& OutValue) const
{
    if (Stat.Equals(TEXT("mission.type"), ESearchCase::IgnoreCase)) OutValue = Mission.MissionType;
    else if (Stat.Equals(TEXT("mission.id"), ESearchCase::IgnoreCase)) OutValue = Mission.MissionId;
    else if (Stat.Equals(TEXT("mission.title"), ESearchCase::IgnoreCase)) OutValue = Mission.MissionTitle;
    else if (Stat.Equals(TEXT("mission.weather"), ESearchCase::IgnoreCase)) OutValue = Mission.Weather;
    else return false;
    return true;
}

bool FRotorlineAwardSystem::ResolveNumericStat(
    const FString& Stat,
    const FRotorlineMissionResults& M,
    const FRotorlineCareerStatistics& C,
    double& OutValue) const
{
#define MSTAT(Name) if (Stat.Equals(FString(TEXT("mission.")) + ANSI_TO_TCHAR(#Name), ESearchCase::IgnoreCase)) { OutValue = M.Name; return true; }
#define MBSTAT(Name) if (Stat.Equals(FString(TEXT("mission.")) + ANSI_TO_TCHAR(#Name), ESearchCase::IgnoreCase)) { OutValue = M.Name ? 1.0 : 0.0; return true; }
#define CSTAT(Name) if (Stat.Equals(FString(TEXT("career.")) + ANSI_TO_TCHAR(#Name), ESearchCase::IgnoreCase)) { OutValue = C.Name; return true; }
    MSTAT(ElapsedSeconds) MSTAT(FlightTimeSeconds) MSTAT(DistanceFlownMeters) MSTAT(Difficulty)
    MSTAT(PrimaryObjectivesCompleted) MSTAT(PrimaryObjectivesTotal) MSTAT(OptionalObjectivesCompleted)
    MSTAT(OptionalObjectivesTotal) MSTAT(EnemyHelicoptersDestroyed) MSTAT(GroundEnemiesDestroyed)
    MSTAT(BasesCaptured) MSTAT(CiviliansRescued) MSTAT(SoldiersRescued) MSTAT(RescueTargetsAvailable)
    MSTAT(RescueLosses) MSTAT(CargoDelivered) MSTAT(CargoWeightKg) MSTAT(CargoDamage)
    MSTAT(SlingLoadAccuracyPercent) MSTAT(DamageTaken) MSTAT(AircraftHealth) MSTAT(AircraftMaxHealth)
    MSTAT(FuelRemainingPercent) MSTAT(TimeBelowSafeAltitudeSeconds) MSTAT(ClosestObstacleClearanceMeters)
    MSTAT(NearMisses) MSTAT(WeaponShotsFired)
    MSTAT(WeaponHits) MSTAT(MissilesDodged) MSTAT(TimeUnderEnemyFireSeconds) MSTAT(DetectionTimeSeconds)
    MSTAT(LandingVerticalSpeedMps) MSTAT(LandingLateralSpeedMps) MSTAT(LandingAttitudeDegrees)
    MSTAT(LandingAccuracyMeters) MSTAT(AbruptControlSeconds) MSTAT(StableHoverSeconds)
    MSTAT(SecondsFromFailureAtCompletion) MSTAT(UniqueMapRegionsExplored) MSTAT(IslandsVisited)
    MSTAT(FlightPathsUsed) MSTAT(FinalScore) MSTAT(StarRating) MSTAT(CampaignCompletionPercent)
    MBSTAT(bMissionSucceeded) MBSTAT(bMissionFailed) MBSTAT(bMeaningfulPartialSuccess) MBSTAT(bValidTakeoff)
    MBSTAT(bValidLanding) MBSTAT(bSafeLanding) MBSTAT(bHardLanding) MBSTAT(bCrashed)
    MBSTAT(bAllRequiredPersonnelDelivered) MBSTAT(bSevereWeather) MBSTAT(bTightClearanceControlled)
    MBSTAT(bStealthApproach) MBSTAT(bSmokeOrDecoyUsed) MBSTAT(bCombatSupportMission)
    MBSTAT(bConstructionMission) MBSTAT(bBaseCaptureMission) MBSTAT(bFinalCampaignMission)
    MBSTAT(bSlingLoadTracked)
    CSTAT(MissionsStarted) CSTAT(MissionsCompleted) CSTAT(MissionsFailed) CSTAT(TotalFlightTimeSeconds)
    CSTAT(TotalDistanceMeters) CSTAT(ValidTakeoffs) CSTAT(SuccessfulLandings) CSTAT(HardLandings)
    CSTAT(CrashCount)
    CSTAT(CiviliansRescued) CSTAT(SoldiersRescued) CSTAT(RescueLosses) CSTAT(PerfectRescueMissions)
    CSTAT(CargoLoadsTransported) CSTAT(CargoWeightTransportedKg) CSTAT(PrecisionCargoDeliveries)
    CSTAT(CargoDamage) CSTAT(EnemyVehiclesDestroyed) CSTAT(EnemyHelicoptersDestroyed) CSTAT(BasesCaptured)
    CSTAT(ShotsFired) CSTAT(WeaponHits) CSTAT(MissilesDodged) CSTAT(TimeUnderEnemyFireSeconds)
    CSTAT(DetectionTimeSeconds) CSTAT(OptionalObjectivesCompleted) CSTAT(HiddenLocationsDiscovered)
    CSTAT(ConsecutiveSuccessfulMissions) CSTAT(BestSuccessfulMissionStreak) CSTAT(BestMissionScore)
    CSTAT(FiveStarMissions) CSTAT(AwardsEarned) CSTAT(CampaignCompletionPercent)
    if (Stat.Equals(TEXT("mission.AccuracyPercent"), ESearchCase::IgnoreCase))
    {
        OutValue = M.WeaponShotsFired > 0 ? 100.0 * M.WeaponHits / M.WeaponShotsFired : 0.0;
        return true;
    }
    if (Stat.Equals(TEXT("mission.HealthPercent"), ESearchCase::IgnoreCase))
    {
        OutValue = M.AircraftMaxHealth > KINDA_SMALL_NUMBER ? 100.0 * M.AircraftHealth / M.AircraftMaxHealth : 0.0;
        return true;
    }
    if (Stat.Equals(TEXT("mission.TotalPeopleRescued"), ESearchCase::IgnoreCase))
    {
        OutValue = M.CiviliansRescued + M.SoldiersRescued;
        return true;
    }
    if (Stat.Equals(TEXT("career.TotalPeopleRescued"), ESearchCase::IgnoreCase))
    {
        OutValue = C.CiviliansRescued + C.SoldiersRescued;
        return true;
    }
    if (Stat.Equals(TEXT("career.RescueSuccessPercent"), ESearchCase::IgnoreCase))
    {
        const double Total = C.CiviliansRescued + C.SoldiersRescued + C.RescueLosses;
        OutValue = Total > 0.0 ? 100.0 * (C.CiviliansRescued + C.SoldiersRescued) / Total : 0.0;
        return true;
    }
    if (Stat.Equals(TEXT("career.SuccessRatePercent"), ESearchCase::IgnoreCase))
    {
        const double Finished = C.MissionsCompleted + C.MissionsFailed;
        OutValue = Finished > 0.0 ? 100.0 * C.MissionsCompleted / Finished : 0.0;
        return true;
    }
    if (Stat.Equals(TEXT("career.CompletedMissionTypes"), ESearchCase::IgnoreCase)) { OutValue = C.CompletedMissionTypes.Num(); return true; }
    if (Stat.Equals(TEXT("career.UniqueRegionsExplored"), ESearchCase::IgnoreCase)) { OutValue = C.UniqueRegionsExplored.Num(); return true; }
    if (Stat.Equals(TEXT("career.UniqueIslandsVisited"), ESearchCase::IgnoreCase)) { OutValue = C.UniqueIslandsVisited.Num(); return true; }
    if (Stat.Equals(TEXT("career.FlightPathsUsed"), ESearchCase::IgnoreCase)) { OutValue = C.FlightPathsUsed.Num(); return true; }
#undef MSTAT
#undef MBSTAT
#undef CSTAT
    return false;
}
