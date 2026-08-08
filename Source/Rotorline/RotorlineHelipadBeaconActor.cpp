#include "RotorlineHelipadBeaconActor.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "RotorlineGroundingComponent.h"
#include "RotorlineGroundingLibrary.h"
#include "UObject/ConstructorHelpers.h"

namespace RotorlineHelipadBeacon
{
    constexpr int32 BeaconCount = 8;
    const TCHAR* CompactHeliportPath = TEXT("/Game/Environment/Imported/Heliports/Compact/SM_Heliport_Compact/StaticMeshes/SM_Heliport_Compact.SM_Heliport_Compact");
}

ARotorlineHelipadBeaconActor::ARotorlineHelipadBeaconActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ServiceHelipad"));
    PadMesh->SetupAttachment(Root);
    PadMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    Grounding = CreateDefaultSubobject<URotorlineGroundingComponent>(TEXT("Grounding"));
    Grounding->PlacementOwner = TEXT("HelipadBeacon");
    Tags.AddUnique(TEXT("RotorlineMissionPad"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PadFinder(RotorlineHelipadBeacon::CompactHeliportPath);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AmberFinder(TEXT("/Game/Missions/Presentation/M_ObjectiveAmberGlow.M_ObjectiveAmberGlow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreenFinder(TEXT("/Game/Missions/Presentation/M_SuccessGreenGlow.M_SuccessGreenGlow"));
    if (PadFinder.Succeeded()) PadMesh->SetStaticMesh(PadFinder.Object);
    if (AmberFinder.Succeeded()) AmberGlowMaterial = AmberFinder.Object;
    if (GreenFinder.Succeeded()) GreenGlowMaterial = GreenFinder.Object;

    for (int32 Index = 0; Index < RotorlineHelipadBeacon::BeaconCount; ++Index)
    {
        UStaticMeshComponent* Bulb = CreateDefaultSubobject<UStaticMeshComponent>(
            FName(*FString::Printf(TEXT("BeaconBulb%02d"), Index)));
        Bulb->SetupAttachment(Root);
        Bulb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Bulb->SetCastShadow(false);
        Bulb->ComponentTags.Add(TEXT("RotorlineGroundingIgnore"));
        Bulb->SetRelativeScale3D(FVector(0.28f));
        if (SphereFinder.Succeeded()) Bulb->SetStaticMesh(SphereFinder.Object);
        BeaconBulbs.Add(Bulb);

        UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(
            FName(*FString::Printf(TEXT("ApproachLight%02d"), Index)));
        Light->SetupAttachment(Root);
        Light->SetIntensity(0.0f);
        Light->SetAttenuationRadius(6200.0f);
        Light->SetSourceRadius(20.0f);
        Light->SetCastShadows(false);
        Light->ComponentTags.Add(TEXT("RotorlineGroundingIgnore"));
        BeaconLights.Add(Light);
    }
}

void ARotorlineHelipadBeaconActor::Configure(
    bool bInHomeBeaconMode,
    bool bInNightOperations,
    const FVector& DesiredLocation,
    const FString& InSiteId)
{
    bHomeBeaconMode = bInHomeBeaconMode;
    bNightOperations = bInNightOperations;
    SiteId = InSiteId;
    if (GetWorld() && !ActorHasTag(TEXT("RotorlineMapHelipadBeacon")))
    {
        for (TActorIterator<ARotorlineHelipadBeaconActor> It(GetWorld()); It; ++It)
        {
            ARotorlineHelipadBeaconActor* OtherBeacon = *It;
            if (!IsValid(OtherBeacon) ||
                OtherBeacon == this ||
                !OtherBeacon->ActorHasTag(TEXT("RotorlineMapHelipadBeacon")))
            {
                continue;
            }
            const FVector OtherLocation = OtherBeacon->GetActorLocation();
            const bool bSamePhysicalPad =
                FVector::Dist2D(DesiredLocation, OtherLocation) <= 3000.0 &&
                FMath::Abs(DesiredLocation.Z - OtherLocation.Z) <= 1200.0;
            if (bSamePhysicalPad)
            {
                OtherBeacon->Destroy();
            }
        }
    }
    const bool bConcealedLair = SiteId.Equals(TEXT("BELL_LAIR"), ESearchCase::IgnoreCase);
    const bool bCentralTownServicePad = SiteId.Equals(TEXT("CENTRAL_TOWN_REARM"), ESearchCase::IgnoreCase);
    const bool bAuthoredMapHelipad = ActorHasTag(TEXT("RotorlineMapHelipadBeacon"));

    PadMesh->SetVisibility(!bHomeBeaconMode, true);
    PadMesh->SetCollisionEnabled(bHomeBeaconMode
        ? ECollisionEnabled::NoCollision
        : ECollisionEnabled::QueryAndPhysics);

    SetActorLocation(DesiredLocation, false, nullptr, ETeleportType::TeleportPhysics);
    Grounding->bGroundingApplied = false;
    Grounding->Exclusion = ERotorlineGroundingExclusion::None;
    Grounding->ExclusionDetail.Reset();
    Grounding->Profile = URotorlineGroundingLibrary::MakeProfile(
        (bHomeBeaconMode || bCentralTownServicePad)
            ? ERotorlineGroundingMode::LinearPoint
            : ERotorlineGroundingMode::MultiPointFootprint,
        bHomeBeaconMode
            ? TEXT("HomePadBeacon")
            : (bCentralTownServicePad ? TEXT("CentralTownServiceHelipad") : TEXT("ServiceHelipad")));
    Grounding->Profile.bAllowPreparedGround = true;
    Grounding->Profile.PreservedOffsetCm = bHomeBeaconMode ? 16.0f : 10.0f;
    Grounding->Profile.MaximumSlopeDegrees = 8.0f;
    Grounding->Profile.MaximumFootprintRoughnessCm = 90.0f;
    Grounding->Profile.ContactComponentNames = { TEXT("ServiceHelipad") };
    FRotorlineGroundingResult GroundingResult;
    bool bGrounded = false;
    if (bAuthoredMapHelipad)
    {
        // Discovery already places this ring at the top of an authored static
        // helipad's bounds. A second terrain trace can legitimately find no
        // landscape below rooftop/elevated pads and used to emit a false Error
        // while leaving this same correct transform in place.
        bGrounded = true;
        Grounding->bGroundingApplied = true;
    }
    else if ((bHomeBeaconMode || bCentralTownServicePad) && !bConcealedLair)
    {
        // The town pad sits on prepared terrain beside its road connector.
        // Ground its center independently of the imported pad's wide bounds:
        // one rejected perimeter trace previously left the entire visible pad
        // at authored Z=0, buried more than 80 metres below the landscape.
        bGrounded = URotorlineGroundingLibrary::SolveGroundContact(
            this, DesiredLocation, FVector2D::ZeroVector, this, Grounding->Profile, GroundingResult);
        if (bGrounded)
        {
            SetActorLocation(GroundingResult.ContactPoint, false, nullptr, ETeleportType::TeleportPhysics);
            Grounding->bGroundingApplied = true;
        }
    }
    else if (!bConcealedLair)
    {
        bGrounded = Grounding->ApplyGrounding(true, GroundingResult);
    }
    else
    {
        // Preserve the authored underground location. Terrain grounding is
        // precisely what moved this hidden beacon array onto the summit.
        bGrounded = true;
        Grounding->bGroundingApplied = true;
    }
    if (!bGrounded)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ROTORLINE_GROUNDING_V2|owner=HelipadBeacon|site=%s|state=NO_SAFE_PLACEMENT|failure=%d|detail=%s"),
            *SiteId, static_cast<int32>(GroundingResult.Failure), *GroundingResult.Detail);
    }

    const float RadiusCm = bHomeBeaconMode ? 2150.0f : 1750.0f;
    for (int32 Index = 0; Index < BeaconLights.Num(); ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(BeaconLights.Num());
        const FVector LocalLocation(FMath::Cos(Angle) * RadiusCm, FMath::Sin(Angle) * RadiusCm, 72.0f);
        const bool bWhiteHomeLight = bHomeBeaconMode && Index % 2 == 0;
        const FLinearColor Color = bWhiteHomeLight
            ? FLinearColor(0.82f, 0.92f, 1.0f)
            : (bHomeBeaconMode ? FLinearColor(1.0f, 0.42f, 0.03f) : FLinearColor(0.12f, 1.0f, 0.34f));

        BeaconBulbs[Index]->SetRelativeLocation(LocalLocation);
        BeaconBulbs[Index]->SetMaterial(0, bHomeBeaconMode ? AmberGlowMaterial : GreenGlowMaterial);
        BeaconBulbs[Index]->SetVisibility(!bConcealedLair, true);
        BeaconLights[Index]->SetRelativeLocation(LocalLocation);
        BeaconLights[Index]->SetLightColor(Color);
        if (bConcealedLair)
        {
            BeaconLights[Index]->SetIntensity(0.0f);
            BeaconBulbs[Index]->SetVisibility(false, true);
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_NIGHT_OPS|HELIPAD|site=%s|night=%d|location=%.0f,%.0f,%.0f|beacons=%d|pad_visible=%d|pad_collision=%d|grounding_mode=%s|grounded=%d"),
        *SiteId,
        bNightOperations ? 1 : 0,
        GetActorLocation().X,
        GetActorLocation().Y,
        GetActorLocation().Z,
        BeaconLights.Num(),
        PadMesh->IsVisible() ? 1 : 0,
        bHomeBeaconMode ? 0 : 1,
        bCentralTownServicePad ? TEXT("CENTER_POINT") : (bHomeBeaconMode ? TEXT("CENTER_POINT") : TEXT("FOOTPRINT")),
        bGrounded ? 1 : 0);
}

void ARotorlineHelipadBeaconActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    (void)DeltaSeconds;
    if (!GetWorld()) return;

    if (SiteId.Equals(TEXT("BELL_LAIR"), ESearchCase::IgnoreCase))
    {
        for (int32 Index = 0; Index < BeaconLights.Num(); ++Index)
        {
            BeaconLights[Index]->SetIntensity(0.0f);
            BeaconBulbs[Index]->SetVisibility(false, true);
        }
        return;
    }

    const int32 Phase = FMath::FloorToInt(GetWorld()->GetTimeSeconds() / 0.55f);
    for (int32 Index = 0; Index < BeaconLights.Num(); ++Index)
    {
        const bool bAlternatingOn = ((Phase + Index) % 2) == 0;
        const float BrightIntensity = bHomeBeaconMode ? 68000.0f : 56000.0f;
        const float RestIntensity = bHomeBeaconMode ? 18000.0f : 14000.0f;
        BeaconLights[Index]->SetIntensity(bAlternatingOn ? BrightIntensity : RestIntensity);
        // Keep the emissive fixture visible in daylight. The point-light pulse
        // still alternates, but auto exposure can no longer erase half of the
        // perimeter when those bulbs are switched completely off.
        BeaconBulbs[Index]->SetVisibility(true, true);
    }
}
