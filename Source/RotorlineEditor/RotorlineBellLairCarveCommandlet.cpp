#include "RotorlineBellLairCarveCommandlet.h"

#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Misc/PackageName.h"

namespace RotorlineBellLairTest
{
    constexpr float LairWorldX = 54800.0f;
    constexpr float LairWorldY = 185200.0f;
    constexpr float HoleRadiusCm = 3300.0f;
    constexpr float EditRadiusCm = 4600.0f;
}

URotorlineBellLairCarveCommandlet::URotorlineBellLairCarveCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 URotorlineBellLairCarveCommandlet::Main(const FString& Params)
{
    using namespace RotorlineBellLairTest;
    const FString SourceAsset = TEXT("/Game/Maps/RotorlineIsland");
    const FString TestAsset = TEXT("/Game/Maps/RotorlineIsland_LairTest");

    // The production island is never loaded for editing. Create the isolated
    // test duplicate once, then update that duplicate in place on later runs.
    if (!UEditorAssetLibrary::DoesAssetExist(TestAsset) &&
        !UEditorAssetLibrary::DuplicateAsset(SourceAsset, TestAsset))
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_LAIR_TEST|FAIL|reason=duplicate_map"));
        return 1;
    }

    const FString TestFilename = FPackageName::LongPackageNameToFilename(
        TestAsset, FPackageName::GetMapPackageExtension());
    if (!FEditorFileUtils::LoadMap(TestFilename, false, false) || !GEditor)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_LAIR_TEST|FAIL|reason=load_duplicate"));
        return 1;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    ALandscape* Landscape = nullptr;
    if (World)
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            Landscape = *It;
            break;
        }
    }
    ULandscapeInfo* LandscapeInfo = Landscape ? Landscape->GetLandscapeInfo() : nullptr;
    if (!World || !Landscape || !LandscapeInfo || !ALandscapeProxy::VisibilityLayer)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_LAIR_TEST|FAIL|reason=landscape"));
        return 1;
    }

    const FVector LocalCenter = Landscape->GetActorTransform().InverseTransformPosition(
        FVector(LairWorldX, LairWorldY, Landscape->GetActorLocation().Z));
    const int32 CenterX = FMath::RoundToInt(LocalCenter.X);
    const int32 CenterY = FMath::RoundToInt(LocalCenter.Y);
    const float VertexSpacingCm = FMath::Abs(Landscape->GetActorScale3D().X);
    const int32 RadiusVertices = FMath::CeilToInt(EditRadiusCm / VertexSpacingCm) + 1;
    const int32 MinX = CenterX - RadiusVertices;
    const int32 MinY = CenterY - RadiusVertices;
    const int32 MaxX = CenterX + RadiusVertices;
    const int32 MaxY = CenterY + RadiusVertices;
    const int32 Width = MaxX - MinX + 1;
    const int32 Height = MaxY - MinY + 1;

    TArray<uint8> Visibility;
    Visibility.SetNumUninitialized(Width * Height);
    int32 HiddenVertices = 0;
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            const float DX = static_cast<float>(MinX + X - CenterX) * VertexSpacingCm;
            const float DY = static_cast<float>(MinY + Y - CenterY) * VertexSpacingCm;
            const bool bInsideHole = DX * DX + DY * DY <= HoleRadiusCm * HoleRadiusCm;
            Visibility[Y * Width + X] = bInsideHole ? 0 : 255;
            HiddenVertices += bInsideHole ? 1 : 0;
        }
    }

    FLandscapeEditDataInterface Edit(LandscapeInfo);
    LandscapeInfo->UpdateLayerInfoMap(Landscape);
    Edit.SetAlphaData(
        ALandscapeProxy::VisibilityLayer,
        MinX, MinY, MaxX, MaxY,
        Visibility.GetData(), Width,
        ELandscapeLayerPaintingRestriction::None);
    Edit.Flush();
    LandscapeInfo->RecreateCollisionComponents();

    // A masked Landscape hole can remain an invisible physics lid in a cooked
    // build. Ignore Pawn collision only on the one Landscape component that
    // contains the launch shaft. The chamber deck below retains collision and
    // the rest of the island Landscape is unchanged.
    ULandscapeHeightfieldCollisionComponent* ShaftCollision = nullptr;
    for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
    {
        if (!Component)
        {
            continue;
        }

        const FIntPoint Base = Component->GetSectionBase();
        const int32 Size = Component->ComponentSizeQuads;
        if (CenterX >= Base.X && CenterX <= Base.X + Size &&
            CenterY >= Base.Y && CenterY <= Base.Y + Size)
        {
            ShaftCollision = Component->GetCollisionComponent();
            break;
        }
    }
    if (!ShaftCollision)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_LAIR_TEST|FAIL|reason=shaft_collision_component"));
        return 1;
    }
    ShaftCollision->Modify();
    ShaftCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    ShaftCollision->MarkPackageDirty();

    Landscape->PostEditChange();
    Landscape->MarkPackageDirty();
    World->MarkPackageDirty();

    if (!FEditorFileUtils::SaveMap(World, TestFilename))
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_LAIR_TEST|FAIL|reason=save_duplicate"));
        return 1;
    }

    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_LAIR_TEST|SUCCESS|map=%s|production_untouched=1|center=%d,%d|hole_radius_cm=%.0f|hidden_vertices=%d|material=%s|collision=REBUILT|pawn_collision=IGNORED|component=%s"),
        *TestAsset, CenterX, CenterY, HoleRadiusCm, HiddenVertices,
        *GetNameSafe(Landscape->GetLandscapeMaterial()), *GetNameSafe(ShaftCollision));
    return 0;
}
