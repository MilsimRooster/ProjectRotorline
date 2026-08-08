#include "RotorlineEnemyIslandAssaultActor.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "RotorlineMissionCatalog.h"
#include "RotorlineMissionObjectiveActor.h"
#include "TimerManager.h"

namespace
{
    const FVector EnemyIslandCenter(650000.0f, 500000.0f, 0.0f);
}

ARotorlineEnemyIslandAssaultActor::ARotorlineEnemyIslandAssaultActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.5f;
    SetActorHiddenInGame(true);
    SetCanBeDamaged(false);
    Tags.Add(TEXT("RotorlineM22AssaultManager"));
}

void ARotorlineEnemyIslandAssaultActor::Deploy(UWorld* World)
{
    if (!World) return;
    Clear(World);

    FActorSpawnParameters SpawnParameters;
    // Destroyed actors retain their UObject names until end-of-frame cleanup.
    // A same-frame mission restart must therefore request a unique manager
    // name instead of fatally reusing the previous deployment's fixed name.
    SpawnParameters.Name = MakeUniqueObjectName(
        World->PersistentLevel,
        ARotorlineEnemyIslandAssaultActor::StaticClass(),
        TEXT("RotorlineM22EnemyIslandAssault"));
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    World->SpawnActor<ARotorlineEnemyIslandAssaultActor>(
        ARotorlineEnemyIslandAssaultActor::StaticClass(),
        EnemyIslandCenter,
        FRotator::ZeroRotator,
        SpawnParameters);
}

void ARotorlineEnemyIslandAssaultActor::Clear(UWorld* World)
{
    if (!World) return;
    TArray<ARotorlineEnemyIslandAssaultActor*> ExistingManagers;
    for (TActorIterator<ARotorlineEnemyIslandAssaultActor> It(World); It; ++It)
    {
        ExistingManagers.Add(*It);
    }
    for (ARotorlineEnemyIslandAssaultActor* Manager : ExistingManagers)
    {
        if (IsValid(Manager))
        {
            Manager->Destroy();
        }
    }
}

void ARotorlineEnemyIslandAssaultActor::BeginPlay()
{
    Super::BeginPlay();
    SpawnGroundGarrison();
}

void ARotorlineEnemyIslandAssaultActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bAerialAssaultStarted || !GetWorld()) return;

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerPawn) return;

    if (FVector::Dist2D(PlayerPawn->GetActorLocation(), EnemyIslandCenter) <= 170000.0f)
    {
        StartAerialAssault();
    }
}

void ARotorlineEnemyIslandAssaultActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(WaveTwoTimer);
        GetWorldTimerManager().ClearTimer(WaveThreeTimer);
    }
    for (const TWeakObjectPtr<ARotorlineMissionObjectiveActor>& Threat : SpawnedThreats)
    {
        if (Threat.IsValid())
        {
            Threat->Destroy();
        }
    }
    SpawnedThreats.Reset();
    Super::EndPlay(EndPlayReason);
}

ARotorlineMissionObjectiveActor* ARotorlineEnemyIslandAssaultActor::SpawnThreat(
    const FString& Label,
    const FString& Target,
    const FVector& Location)
{
    if (!GetWorld()) return nullptr;

    const FRotator Heading = (EnemyIslandCenter - Location).Rotation();
    ARotorlineMissionObjectiveActor* Threat =
        GetWorld()->SpawnActor<ARotorlineMissionObjectiveActor>(
            ARotorlineMissionObjectiveActor::StaticClass(),
            Location,
            FRotator(0.0f, Heading.Yaw, 0.0f));
    if (!Threat) return nullptr;

    FRotorlineObjectiveDefinition Definition;
    Definition.Kind = TEXT("destroy");
    Definition.Text = Label;
    Definition.Target = Target;
    Definition.Site = TEXT("enemy-island-support-force");
    Definition.bHasLocation = true;
    Definition.bHasWorldLocation = true;
    Definition.WorldLocation = Location;
    Definition.Radius = 90.0f;
    Threat->Configure(Definition, Location);
    Threat->SetMissionMarkerVisibility(false);
    Threat->Tags.AddUnique(TEXT("RotorlineM22SupportThreat"));
    SpawnedThreats.Add(Threat);
    return Threat;
}

void ARotorlineEnemyIslandAssaultActor::SpawnGroundGarrison()
{
    struct FGroundThreat
    {
        FVector Location;
        const TCHAR* Label;
        const TCHAR* Target;
    };

    const FGroundThreat GroundThreats[] = {
        { FVector(636000.0f, 486000.0f, 1200.0f), TEXT("Enemy-island armor Alpha One"), TEXT("enemy-island tank") },
        { FVector(644000.0f, 484500.0f, 1200.0f), TEXT("Enemy-island armor Alpha Two"), TEXT("enemy-island tank") },
        { FVector(651000.0f, 485500.0f, 1200.0f), TEXT("Enemy-island armor Alpha Three"), TEXT("enemy-island tank") },
        { FVector(658000.0f, 513000.0f, 1200.0f), TEXT("Enemy-island armor Bravo One"), TEXT("enemy-island tank") },
        { FVector(665000.0f, 514000.0f, 1200.0f), TEXT("Enemy-island armor Bravo Two"), TEXT("enemy-island tank") },
        { FVector(670000.0f, 510000.0f, 1200.0f), TEXT("Enemy-island armor Bravo Three"), TEXT("enemy-island tank") },
        { FVector(638000.0f, 515000.0f, 1200.0f), TEXT("Enemy-island west flak section"), TEXT("enemy-island flak battery") },
        { FVector(667000.0f, 515000.0f, 1200.0f), TEXT("Enemy-island east flak section"), TEXT("enemy-island flak battery") },
        { FVector(673000.0f, 490000.0f, 1200.0f), TEXT("Enemy-island reserve rocket battery"), TEXT("enemy-island HIMARS rocket artillery") }
    };

    int32 Spawned = 0;
    for (const FGroundThreat& Spec : GroundThreats)
    {
        FVector GroundedLocation = Spec.Location;
        FHitResult GroundHit;
        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(RotorlineM22Grounding),
            false,
            this);
        const FVector TraceStart(Spec.Location.X, Spec.Location.Y, 12000.0f);
        const FVector TraceEnd(Spec.Location.X, Spec.Location.Y, -3000.0f);
        if (GetWorld()->LineTraceSingleByChannel(
                GroundHit,
                TraceStart,
                TraceEnd,
                ECC_Visibility,
                QueryParams))
        {
            GroundedLocation.Z = GroundHit.ImpactPoint.Z + 35.0f;
        }
        Spawned += SpawnThreat(
            Spec.Label,
            Spec.Target,
            GroundedLocation) ? 1 : 0;
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_M22_ASSAULT|GROUND_GARRISON|spawned=%d|tanks=6|flak=2|himars=1"),
        Spawned);
}

void ARotorlineEnemyIslandAssaultActor::StartAerialAssault()
{
    if (bAerialAssaultStarted || !GetWorld()) return;
    bAerialAssaultStarted = true;
    SpawnAerialWave(1);

    FTimerDelegate WaveTwoDelegate;
    WaveTwoDelegate.BindUObject(this, &ARotorlineEnemyIslandAssaultActor::SpawnAerialWave, 2);
    GetWorldTimerManager().SetTimer(WaveTwoTimer, WaveTwoDelegate, 40.0f, false);

    FTimerDelegate WaveThreeDelegate;
    WaveThreeDelegate.BindUObject(this, &ARotorlineEnemyIslandAssaultActor::SpawnAerialWave, 3);
    GetWorldTimerManager().SetTimer(WaveThreeTimer, WaveThreeDelegate, 85.0f, false);

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_M22_ASSAULT|AERIAL_SEQUENCE_STARTED|trigger_range_m=1700|waves=3|aircraft_per_wave=2"));
}

void ARotorlineEnemyIslandAssaultActor::SpawnAerialWave(int32 WaveIndex)
{
    struct FAirThreat
    {
        FVector Location;
        const TCHAR* Target;
        const TCHAR* Label;
    };

    TArray<FAirThreat> Wave;
    if (WaveIndex == 1)
    {
        Wave = {
            { FVector(625000.0f, 455000.0f, 6500.0f), TEXT("MD500 enemy light gunship"), TEXT("MD-500 raider One") },
            { FVector(680000.0f, 460000.0f, 7000.0f), TEXT("MD500 enemy light gunship"), TEXT("MD-500 raider Two") }
        };
    }
    else if (WaveIndex == 2)
    {
        Wave = {
            { FVector(700000.0f, 495000.0f, 7600.0f), TEXT("AH-64 Apache enemy gunship"), TEXT("Apache hunter One") },
            { FVector(685000.0f, 535000.0f, 7200.0f), TEXT("AH-64 Apache enemy gunship"), TEXT("Apache hunter Two") }
        };
    }
    else
    {
        Wave = {
            { FVector(645000.0f, 555000.0f, 8200.0f), TEXT("Mi-24 Hind enemy gunship"), TEXT("Hind assault One") },
            { FVector(610000.0f, 532000.0f, 7800.0f), TEXT("Mi-24 Hind enemy gunship"), TEXT("Hind assault Two") }
        };
    }

    int32 Spawned = 0;
    for (const FAirThreat& Spec : Wave)
    {
        Spawned += SpawnThreat(Spec.Label, Spec.Target, Spec.Location) ? 1 : 0;
    }
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_M22_ASSAULT|AERIAL_WAVE|wave=%d|spawned=%d|airframe=%s"),
        WaveIndex,
        Spawned,
        WaveIndex == 1 ? TEXT("MD500") : (WaveIndex == 2 ? TEXT("APACHE") : TEXT("HIND")));
}
