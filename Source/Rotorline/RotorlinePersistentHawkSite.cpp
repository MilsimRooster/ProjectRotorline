#include "RotorlinePersistentHawkSite.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARotorlinePersistentHawkSite::ARotorlinePersistentHawkSite()
{
    Tags.AddUnique(TEXT("RotorlinePersistentThreat"));
    Tags.AddUnique(TEXT("RotorlineHawkBattery"));

    PreparedFiringPad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreparedFiringPad"));
    PreparedFiringPad->SetupAttachment(GetRootComponent());
    PreparedFiringPad->SetMobility(EComponentMobility::Static);
    PreparedFiringPad->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PreparedFiringPad->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));
    PreparedFiringPad->SetRelativeScale3D(FVector(7.0f, 6.5f, 2.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> EarthFinder(
        TEXT("/Game/Environment/Materials/Blockout/M_Road_Shoulder.M_Road_Shoulder"));
    if (CubeFinder.Succeeded()) PreparedFiringPad->SetStaticMesh(CubeFinder.Object);
    if (EarthFinder.Succeeded()) PreparedFiringPad->SetMaterial(0, EarthFinder.Object);
}

void ARotorlinePersistentHawkSite::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConfigurePersistentSite();
}

void ARotorlinePersistentHawkSite::BeginPlay()
{
    Super::BeginPlay();
    ConfigurePersistentSite();

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_HAWK_RIDGE|ACTIVE|site=%s|location=%.1f,%.1f,%.1f|upright_dot=%.4f|threat=RADAR_MISSILE"),
        *SiteDesignation,
        GetActorLocation().X,
        GetActorLocation().Y,
        GetActorLocation().Z,
        FVector::DotProduct(GetActorUpVector(), FVector::UpVector));
}

void ARotorlinePersistentHawkSite::ConfigurePersistentSite()
{
    FRotorlineObjectiveDefinition Definition;
    Definition.Kind = TEXT("destroy");
    Definition.Text = FString::Printf(TEXT("MIM-23 HAWK // %s"), *SiteDesignation);
    Definition.Target = FString::Printf(TEXT("hawk-main-valley-ridge-%s"), *SiteDesignation.ToLower().Replace(TEXT(" "), TEXT("-")));
    Definition.Site = SiteDesignation.Equals(TEXT("BRAVO"), ESearchCase::IgnoreCase)
        ? TEXT("main-valley-east-ridge")
        : TEXT("main-valley-west-ridge");
    Definition.Radius = 80.0f;
    Definition.bHasLocation = true;
    Configure(Definition, GetActorLocation());
}
