#include "RotorlineCaveTransitionActor.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "RotorlineJeepPawn.h"
#include "RotorlineOperationsPlayerController.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineCave
{
    void ConfigureSizedMesh(
        UStaticMeshComponent* Component,
        UStaticMesh* Mesh,
        UMaterialInterface* Material,
        const FVector& Location,
        const FVector& DimensionsCm,
        const FRotator& Rotation = FRotator::ZeroRotator)
    {
        if (!Component || !Mesh) return;
        Component->SetStaticMesh(Mesh);
        if (Material) Component->SetMaterial(0, Material);
        Component->SetRelativeLocation(Location);
        Component->SetRelativeRotation(Rotation);
        const FVector MeshSize = Mesh->GetBounds().BoxExtent * 2.0f;
        Component->SetRelativeScale3D(FVector(
            MeshSize.X > 1.0f ? DimensionsCm.X / MeshSize.X : 1.0f,
            MeshSize.Y > 1.0f ? DimensionsCm.Y / MeshSize.Y : 1.0f,
            MeshSize.Z > 1.0f ? DimensionsCm.Z / MeshSize.Z : 1.0f));
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->ComponentTags.Add(TEXT("RotorlineCaveFacade"));
    }
}

ARotorlineCaveTransitionActor::ARotorlineCaveTransitionActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("CaveEntranceRoot"));
    SetRootComponent(Root);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkFinder(
        TEXT("/Game/Environment/Materials/Urban/M_Urban_Window_Dark.M_Urban_Window_Dark"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalFinder(
        TEXT("/Game/Environment/Imported/Hangar/ClassifiedAirframeCrate/rotorline_airframe_shipping_crate/Materials/M_Crate_OlivePaint.M_Crate_OlivePaint"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AmberFinder(
        TEXT("/Game/Missions/Presentation/M_ObjectiveAmberGlow.M_ObjectiveAmberGlow"));
    static ConstructorHelpers::FObjectFinder<USoundBase> WindFinder(
        TEXT("/Game/Audio/Ambience/SC_CoastalWind_Loop.SC_CoastalWind_Loop"));

    UStaticMesh* Cube = CubeFinder.Object.Get();
    UMaterialInterface* Dark = DarkFinder.Object.Get();
    UMaterialInterface* Metal = MetalFinder.Succeeded() ? MetalFinder.Object.Get() : Dark;
    UMaterialInterface* Amber = AmberFinder.Object.Get();

    auto AddPiece = [this](
        const TCHAR* Name,
        UStaticMesh* Mesh,
        UMaterialInterface* Material,
        const FVector& Location,
        const FVector& Dimensions,
        const FRotator& Rotation = FRotator::ZeroRotator)
    {
        UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Piece->SetupAttachment(Root);
        RotorlineCave::ConfigureSizedMesh(Piece, Mesh, Material, Location, Dimensions, Rotation);
        const FString PieceName(Name);
        const bool bGeneratedFacadeFrame =
            PieceName.StartsWith(TEXT("Frame"), ESearchCase::IgnoreCase) ||
            PieceName.StartsWith(TEXT("Guide"), ESearchCase::IgnoreCase) ||
            PieceName.Equals(TEXT("Threshold"), ESearchCase::IgnoreCase);
        Piece->SetHiddenInGame(bGeneratedFacadeFrame);
    };

    // A compact disguised bulkhead that sits inside the player's authored
    // rock cave. Local +X points through the door toward the hidden Lair.
    AddPiece(TEXT("LairDoor"), Cube, Dark,
        FVector(90.0f, 0.0f, 300.0f), FVector(55.0f, 1000.0f, 600.0f));
    AddPiece(TEXT("FrameLeft"), Cube, Metal,
        FVector(35.0f, -575.0f, 325.0f), FVector(140.0f, 110.0f, 720.0f));
    AddPiece(TEXT("FrameRight"), Cube, Metal,
        FVector(35.0f, 575.0f, 325.0f), FVector(140.0f, 110.0f, 720.0f));
    AddPiece(TEXT("FrameHeader"), Cube, Metal,
        FVector(35.0f, 0.0f, 675.0f), FVector(140.0f, 1260.0f, 110.0f));
    AddPiece(TEXT("Threshold"), Cube, Metal,
        FVector(-40.0f, 0.0f, 28.0f), FVector(330.0f, 1120.0f, 56.0f));

    // Small, readable guide lights frame the actual opening without turning it
    // into a glowing mission gate.
    AddPiece(TEXT("GuideLeft"), Cube, Amber,
        FVector(-38.0f, -490.0f, 310.0f), FVector(28.0f, 20.0f, 430.0f));
    AddPiece(TEXT("GuideRight"), Cube, Amber,
        FVector(-38.0f, 490.0f, 310.0f), FVector(28.0f, 20.0f, 430.0f));

    EntryTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("JeepEntranceTrigger"));
    EntryTrigger->SetupAttachment(Root);
    // Cover both sides of the placed door so the handoff occurs before the
    // Jeep's obstacle probes reach cave geometry, regardless of actor facing.
    EntryTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 320.0f));
    EntryTrigger->SetBoxExtent(FVector(1500.0f, 560.0f, 450.0f));
    EntryTrigger->ComponentTags.Add(TEXT("Rotorline.RuntimeTrigger"));
    EntryTrigger->SetVisibility(false, true);
    EntryTrigger->SetHiddenInGame(true, true);
    EntryTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    EntryTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    EntryTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    EntryTrigger->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    EntryTrigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    EntryTrigger->SetGenerateOverlapEvents(true);
    EntryTrigger->OnComponentBeginOverlap.AddDynamic(this, &ARotorlineCaveTransitionActor::HandleEntryOverlap);

    const TArray<FVector> LightLocations = {
        FVector(-250.0f, -430.0f, 390.0f),
        FVector(-250.0f, 430.0f, 390.0f)
    };
    for (int32 Index = 0; Index < LightLocations.Num(); ++Index)
    {
        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(
            *FString::Printf(TEXT("EntranceLight%02d"), Index));
        Light->SetupAttachment(Root);
        Light->SetRelativeLocation(LightLocations[Index]);
        Light->SetIntensity(4200.0f);
        Light->SetAttenuationRadius(1900.0f);
        Light->SetLightColor(FColor(236, 178, 106));
        EntranceLights.Add(Light);
    }

    CaveAmbience = CreateDefaultSubobject<UAudioComponent>(TEXT("CaveMouthAmbience"));
    CaveAmbience->SetupAttachment(Root);
    CaveAmbience->SetRelativeLocation(FVector(40.0f, 0.0f, 300.0f));
    CaveAmbience->bAutoActivate = true;
    CaveAmbience->bAllowSpatialization = true;
    CaveAmbience->SetVolumeMultiplier(0.10f);
    if (WindFinder.Succeeded()) CaveAmbience->SetSound(WindFinder.Object.Get());

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_CAVE|READY|mode=LAIR_BULKHEAD|trigger=AT_DOOR|jeep_channels=PAWN,WORLD_DYNAMIC,VEHICLE"));
}

void ARotorlineCaveTransitionActor::HandleEntryOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    ARotorlineJeepPawn* Jeep = Cast<ARotorlineJeepPawn>(OtherActor);
    if (bTransitionStarted || !Jeep)
    {
        return;
    }

    ARotorlineOperationsPlayerController* Operations =
        Cast<ARotorlineOperationsPlayerController>(Jeep->GetController());
    if (!Operations)
    {
        return;
    }

    bTransitionStarted = true;
    EntryTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Operations->BeginCaveJeepTransition();
}
