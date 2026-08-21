#include "RotorlineOperationsHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "MediaTexture.h"
#include "RotorlineHelicopterPawn.h"
#include "RotorlineJeepPawn.h"
#include "RotorlineMissionCatalog.h"
#include "RotorlineOperationsPlayerController.h"
#include "RotorlineSupportLocations.h"

namespace
{
    enum class ERotorlineCreditStyle : uint8
    {
        Title,
        Subtitle,
        Section,
        Name,
        Detail,
        Spacer
    };

    struct FRotorlineCreditLine
    {
        const TCHAR* Text;
        ERotorlineCreditStyle Style;
    };

    const TArray<FRotorlineCreditLine>& GetRotorlineCreditRoll()
    {
        static const TArray<FRotorlineCreditLine> Lines = {
            {TEXT("ROTORLINE"), ERotorlineCreditStyle::Title},
            {TEXT("VERTICAL OPERATIONS"), ERotorlineCreditStyle::Subtitle},
            {TEXT("A GAME BY PROJECT ROTORLINE"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},

            {TEXT("CREATIVE LEAD"), ERotorlineCreditStyle::Section},
            {TEXT("KEITH LEAGUE"), ERotorlineCreditStyle::Name},
            {TEXT("CALLSIGN  //  ROOSTER"), ERotorlineCreditStyle::Detail},
            {TEXT("GAME CONCEPT  //  DESIGN  //  DIRECTION  //  PRODUCTION"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},
            {TEXT("DEVELOPMENT"), ERotorlineCreditStyle::Section},
            {TEXT("ROTORLINE PROJECT"), ERotorlineCreditStyle::Name},
            {TEXT("GAMEPLAY SYSTEMS  //  WORLD BUILDING  //  UI  //  AUDIO INTEGRATION"), ERotorlineCreditStyle::Detail},
            {TEXT("BUILT WITH UNREAL ENGINE 5"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},

            {TEXT("PLAYER AIRCRAFT"), ERotorlineCreditStyle::Section},
            {TEXT("UH-1 HUEY"), ERotorlineCreditStyle::Name},
            {TEXT("DUANE'S MIND  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("MD-500 DEFENDER"), ERotorlineCreditStyle::Name},
            {TEXT("DUANE'S MIND  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("AH-64 APACHE"), ERotorlineCreditStyle::Name},
            {TEXT("ARION DIGITAL  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("MI-24 HIND"), ERotorlineCreditStyle::Name},
            {TEXT("ASHLEY ASLETT  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("UH-60M BLACK HAWK"), ERotorlineCreditStyle::Name},
            {TEXT("PROJECT ROTORLINE"), ERotorlineCreditStyle::Detail},
            {TEXT("MARINE UH-1  //  KA-27 HELIX  //  OH-58 KIOWA"), ERotorlineCreditStyle::Name},
            {TEXT("PROJECT ROTORLINE"), ERotorlineCreditStyle::Detail},
            {TEXT("BELL 222"), ERotorlineCreditStyle::Name},
            {TEXT("HELIJAH  //  SKETCHFAB STANDARD LICENSE"), ERotorlineCreditStyle::Detail},
            {TEXT("CH-47 CHINOOK"), ERotorlineCreditStyle::Name},
            {TEXT("MUHAMAD MIRZA ARRAFI  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},

            {TEXT("WORLD AND ENVIRONMENT"), ERotorlineCreditStyle::Section},
            {TEXT("HELIPORT AIR BASE  //  AHMAGH2E  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("HELIPORT HELICOPTER 45MB  //  AHMAGH2E  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("LOW POLY FOREST TREE PACK  //  99.MILES  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("UKRAINIAN M142 HIMARS  //  42MANAKO  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("PACIFIC DAWN CRUISE SHIP  //  GMAN THE CRUISE DUDE  //  CC BY-ND 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("BATTLE SHIP  //  GOGIART  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("TYPE-054A FRIGATE  //  MUHAMAD MIRZA ARRAFI  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("LOW POLY FRIGATE  //  DUANE'S MIND  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("HMS INVINCIBLE  //  MACHINE MEZA  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},

            {TEXT("MISSION-WORLD MODELS"), ERotorlineCreditStyle::Section},
            {TEXT("MILITARY SUPPLY CRATE  //  MAX3DD  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("LOW POLY CRASHED PLANE  //  LESMANTHDEV  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("PILOT LOW POLY CHARACTER  //  00AMZA  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("MOBILE TOWER  //  LADYLIONSTUDIOS  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("1S91 STRAIGHT FLUSH  //  RHINE_LAB_MUELSYSE  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("FLAK CANNON WITH ADATS  //  SKYESHARK  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},

            {TEXT("GROUND VEHICLES AND EXTRACTION TEAM"), ERotorlineCreditStyle::Section},
            {TEXT("JEEP CJ-5  //  LUISFILIPE SILVA SANTOS  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("POLISH SOLDIER  //  BUH  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT("UKRAINIAN DMR RIFLEMAN  //  42MANAKO  //  CC BY 4.0"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},

            {TEXT("AUDIO"), ERotorlineCreditStyle::Section},
            {TEXT("AIRCRAFT ENGINE RECORDINGS  //  PROJECT ROTORLINE"), ERotorlineCreditStyle::Detail},
            {TEXT("ENVIRONMENT AMBIENCE  //  PROJECT ROTORLINE"), ERotorlineCreditStyle::Detail},
            {TEXT("MISSION COMMAND BRIEFING  //  PROJECT ROTORLINE"), ERotorlineCreditStyle::Detail},
            {TEXT("END CREDITS MUSIC  //  PROJECT ROTORLINE"), ERotorlineCreditStyle::Detail},
            {TEXT("RUNTIME EDITS  //  48 KHZ GAME-AUDIO MASTERS"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},

            {TEXT("TECHNOLOGY"), ERotorlineCreditStyle::Section},
            {TEXT("UNREAL ENGINE 5  //  EPIC GAMES"), ERotorlineCreditStyle::Name},
            {TEXT("MEDIA FRAMEWORK  //  ENHANCED INPUT  //  AUDIO MIXER"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},

            {TEXT("SPECIAL THANKS"), ERotorlineCreditStyle::Section},
            {TEXT("THE ARTISTS AND CREATORS WHO SHARED THEIR WORK"), ERotorlineCreditStyle::Detail},
            {TEXT("THE UNREAL ENGINE COMMUNITY"), ERotorlineCreditStyle::Detail},
            {TEXT("EVERY TEST PILOT WHO TOOK THE CONTROLS"), ERotorlineCreditStyle::Detail},
            {TEXT("AND YOU  //  FOR FLYING ROTORLINE"), ERotorlineCreditStyle::Name},
            {TEXT(""), ERotorlineCreditStyle::Spacer},
            {TEXT("DEDICATED TO"), ERotorlineCreditStyle::Section},
            {TEXT("CW5 J.V. SPAHN"), ERotorlineCreditStyle::Name},
            {TEXT("CALLSIGN  //  FRO"), ERotorlineCreditStyle::Detail},
            {TEXT(""), ERotorlineCreditStyle::Spacer},
            {TEXT("TAKE THE AIR. HOLD THE LINE."), ERotorlineCreditStyle::Subtitle},
            {TEXT("ROTORLINE"), ERotorlineCreditStyle::Title}
        };
        return Lines;
    }
}

void ARotorlineOperationsHUD::DrawHUD()
{
    Super::DrawHUD();

    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (OperationsController && OperationsController->IsStartupFlowVisible())
    {
        DrawStartupFlow();
        return;
    }
    if (OperationsController && OperationsController->IsOperationsMenuOpen())
    {
        if (OperationsController->IsPatchWallOpen())
        {
            DrawPatchWall();
        }
        else if (OperationsController->IsHangarOpen())
        {
            DrawAircraftHangar();
        }
        else
        {
            DrawOperationsBoard();
        }
        if (OperationsController->IsGraphicsSettingsOpen() && !OperationsController->IsPatchWallOpen())
        {
            DrawGraphicsSettingsOverlay();
        }
        else if (OperationsController->IsAudioSettingsOpen() && !OperationsController->IsPatchWallOpen())
        {
            DrawAudioSettingsOverlay();
        }
        else if (OperationsController->IsControlsSettingsOpen() && !OperationsController->IsPatchWallOpen())
        {
            DrawControlsSettingsOverlay();
        }
    }
    else
    {
        DrawFlightNavigation();
        if (OperationsController && OperationsController->ShouldShowFlightControllerNotification() && Canvas)
        {
            const float BannerW = FMath::Min(880.0f, Canvas->SizeX - 80.0f);
            const float BannerX = (Canvas->SizeX - BannerW) * 0.5f;
            DrawRect(FLinearColor(0.01f, 0.08f, 0.09f, 0.94f), BannerX, 82.0f, BannerW, 58.0f);
            DrawRect(FLinearColor(1.0f, 0.69f, 0.22f, 1.0f), BannerX, 82.0f, 6.0f, 58.0f);
            DrawText(OperationsController->GetFlightControllerNotification(), FLinearColor(0.92f, 0.97f, 0.94f),
                BannerX + 24.0f, 101.0f, GEngine->GetSmallFont(), 0.78f);
        }
        if (OperationsController && OperationsController->IsAwardPresentationOpen())
        {
            DrawAwardPresentationOverlay();
        }
        else if (OperationsController && OperationsController->IsMissionCompleteScreenOpen())
        {
            DrawMissionCompleteOverlay();
        }
        else if (OperationsController && OperationsController->IsMissionFailureScreenOpen())
        {
            DrawMissionFailureOverlay();
        }
        else if (OperationsController && OperationsController->IsFlightPauseMenuOpen())
        {
            DrawFlightPauseOverlay();
            if (OperationsController->IsGraphicsSettingsOpen())
            {
                DrawGraphicsSettingsOverlay();
            }
            else if (OperationsController->IsAudioSettingsOpen())
            {
                DrawAudioSettingsOverlay();
            }
            else if (OperationsController->IsControlsSettingsOpen())
            {
                DrawControlsSettingsOverlay();
            }
        }
    }
}

void ARotorlineOperationsHUD::DrawStartupFlow()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController) return;
    if (OperationsController->IsGraphicsSettingsOpen())
    {
        DrawGraphicsSettingsOverlay();
        return;
    }
    if (OperationsController->IsControlsSettingsOpen())
    {
        DrawControlsSettingsOverlay();
        return;
    }
    if (OperationsController->IsStartupPatchWallOpen())
    {
        DrawPatchWall();
        return;
    }

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.67f, 1.25f);
    const float TextScale = FMath::Max(0.95f, Scale * 1.28f);
    const FLinearColor White(0.92f, 0.96f, 0.94f, 1.0f);
    const FLinearColor Muted(0.58f, 0.70f, 0.68f, 1.0f);
    const FLinearColor Amber(1.0f, 0.69f, 0.22f, 1.0f);
    const FLinearColor Cyan(0.28f, 0.92f, 0.90f, 1.0f);
    DrawRect(FLinearColor::Black, 0.0f, 0.0f, Width, Height);

    if (OperationsController->IsStartupIntroOpen() || OperationsController->IsCreditsOpen())
    {
        if (OperationsController->IsStartupMediaReady())
        {
            if (UMediaTexture* Texture = OperationsController->GetStartupMediaTexture())
            {
                constexpr float SourceAspect = 16.0f / 9.0f;
                const float DrawWidth = FMath::Min(Width, Height * SourceAspect);
                const float DrawHeight = DrawWidth / SourceAspect;
                const float DrawX = (Width - DrawWidth) * 0.5f;
                const float DrawY = (Height - DrawHeight) * 0.5f;
                DrawTexture(Texture, DrawX, DrawY, DrawWidth, DrawHeight,
                    0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, BLEND_Opaque);
            }
        }

        if (OperationsController->IsCreditsOpen() && OperationsController->ShouldShowCreditsRoll())
        {
            DrawCreditsRoll();
        }

        if (!OperationsController->IsM25FinalCreditsSequenceActive())
        {
            DrawRect(FLinearColor(0.0f, 0.035f, 0.040f, 0.55f), 0.0f, Height - 78.0f * Scale, Width, 78.0f * Scale);
            if (OperationsController->IsCreditsOpen())
            {
                DrawText(TEXT("ROTORLINE  //  CREDITS"), White, 34.0f * Scale, Height - 55.0f * Scale,
                    GEngine->GetSmallFont(), 0.70f * TextScale);
                DrawText(OperationsController->GetStartupMediaTimeLabel(), Muted, Width * 0.5f - 58.0f * Scale,
                    Height - 55.0f * Scale, GEngine->GetSmallFont(), 0.66f * TextScale);
                const float CreditsProgress = OperationsController->GetCreditsScrollProgress();
                DrawRect(FLinearColor(0.12f, 0.28f, 0.29f, 0.95f), Width * 0.34f, Height - 17.0f * Scale,
                    Width * 0.32f, 3.0f * Scale);
                DrawRect(Amber, Width * 0.34f, Height - 17.0f * Scale,
                    Width * 0.32f * CreditsProgress, 3.0f * Scale);
                DrawText(TEXT("X / CIRCLE / OPTIONS / ESC  RETURN"), White, Width - 370.0f * Scale,
                    Height - 55.0f * Scale, GEngine->GetSmallFont(), 0.66f * TextScale);
            }
            else if (OperationsController->GetStartupStateElapsed() >= 1.25f)
            {
                const TCHAR* SkipLabel = OperationsController->IsLoreIntroPlaying()
                    ? TEXT("X / ENTER / SPACE / OPTIONS  SKIP LORE")
                    : TEXT("X / ENTER / SPACE / OPTIONS  SKIP");
                DrawText(SkipLabel, White, Width - 390.0f * Scale,
                    Height - 55.0f * Scale, GEngine->GetSmallFont(), 0.66f * TextScale);
            }
        }
    }
    else if (OperationsController->IsCastGalleryOpen())
    {
        DrawCastGallery();
    }
    else if (OperationsController->IsStartupMenuOpen())
    {
        if (UTexture2D* Background = OperationsController->GetStartupBackgroundTexture())
        {
            const float SourceAspect = Background->GetSizeY() > 0
                ? static_cast<float>(Background->GetSizeX()) / static_cast<float>(Background->GetSizeY())
                : 16.0f / 9.0f;
            const float DestinationAspect = Width / FMath::Max(1.0f, Height);
            float U = 0.0f;
            float V = 0.0f;
            float UL = 1.0f;
            float VL = 1.0f;
            if (DestinationAspect > SourceAspect)
            {
                VL = SourceAspect / DestinationAspect;
                V = (1.0f - VL) * 0.5f;
            }
            else
            {
                UL = DestinationAspect / SourceAspect;
                U = (1.0f - UL) * 0.5f;
            }
            DrawTexture(Background, 0.0f, 0.0f, Width, Height, U, V, UL, VL,
                FLinearColor(0.80f, 0.86f, 0.88f, 1.0f), BLEND_Opaque);
        }

        DrawRect(FLinearColor(0.005f, 0.030f, 0.038f, 0.84f), 0.0f, 0.0f, Width * 0.42f, Height);
        DrawRect(FLinearColor(0.005f, 0.020f, 0.025f, 0.45f), 0.0f, Height * 0.88f, Width, Height * 0.12f);
        DrawRect(Amber, Width * 0.052f, Height * 0.105f, Width * 0.145f, 4.0f * Scale);
        DrawText(TEXT("FLIGHT OPERATIONS"), White, Width * 0.052f, Height * 0.145f,
            GEngine->GetLargeFont(), 1.92f * TextScale);
        DrawText(TEXT("KESTREL REACH // ACTIVE THEATER"), Amber, Width * 0.055f, Height * 0.245f,
            GEngine->GetSmallFont(), 0.92f * TextScale);
        DrawText(TEXT("NO FLAGS. NO BACKUP. HOLD THE LINE."), Muted, Width * 0.055f, Height * 0.292f,
            GEngine->GetSmallFont(), 0.72f * TextScale);

        const TCHAR* Labels[] = {
            TEXT("ENTER OPERATIONS"), TEXT("PERSONNEL DOSSIERS"), TEXT("PATCH WALL // FLIGHT RECORD"), TEXT("ROLL CREDITS"),
            TEXT("FLIGHT CONTROLS"), TEXT("GRAPHICS SETTINGS"), TEXT("EXIT TO DESKTOP")
        };
        const int32 Selected = OperationsController->GetSelectedStartupMenuIndex();
        const int32 Hovered = OperationsController->GetHoveredStartupMenuIndex();
        const int32 Pressed = OperationsController->GetPressedStartupMenuIndex();
        const float MenuX = Width * 0.052f;
        const float MenuY = Height * 0.385f;
        const float MenuWidth = FMath::Min(560.0f, Width * 0.31f);
        const float ItemHeight = FMath::Max(52.0f, Height * 0.065f);
        const float Gap = FMath::Max(8.0f, Height * 0.010f);
        for (int32 Index = 0; Index < 7; ++Index)
        {
            const float ItemY = MenuY + Index * (ItemHeight + Gap);
            const bool bSelected = Index == Selected;
            const bool bHovered = Index == Hovered;
            const bool bPressed = Index == Pressed;
            const FLinearColor Fill = bPressed
                ? FLinearColor(0.72f, 0.43f, 0.10f, 0.94f)
                : (bSelected ? FLinearColor(0.04f, 0.19f, 0.19f, 0.94f)
                    : (bHovered ? FLinearColor(0.04f, 0.13f, 0.14f, 0.88f)
                        : FLinearColor(0.012f, 0.055f, 0.065f, 0.78f)));
            DrawRect(Fill, MenuX, ItemY, MenuWidth, ItemHeight);
            DrawRect(bSelected ? Amber : FLinearColor(0.20f, 0.42f, 0.42f, 0.75f),
                MenuX, ItemY, bSelected ? 6.0f * Scale : 2.0f * Scale, ItemHeight);
            DrawText(Labels[Index], bSelected ? White : Muted,
                MenuX + 24.0f * Scale, ItemY + 15.0f * Scale,
                GEngine->GetSmallFont(), 0.80f * TextScale);
            if (bSelected)
            {
                DrawText(TEXT(">"), Amber, MenuX + MenuWidth - 35.0f * Scale,
                    ItemY + 14.0f * Scale, GEngine->GetSmallFont(), 0.86f * TextScale);
            }
        }

        DrawText(TEXT("PS5  D-PAD / LEFT STICK  SELECT     X / ENTER  CONFIRM     MOUSE  SUPPORTED"),
            White, Width * 0.052f, Height * 0.925f, GEngine->GetSmallFont(), 0.62f * TextScale);
        DrawText(TEXT("BUILD ALPHA  //  KESTREL REACH OPERATIONS"),
            Muted, Width - 390.0f * Scale, Height * 0.925f, GEngine->GetSmallFont(), 0.58f * TextScale);
    }

    const float FadeAlpha = FMath::Clamp(OperationsController->GetStartupFadeAlpha(), 0.0f, 1.0f);
    if (FadeAlpha > 0.001f)
    {
        DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FadeAlpha), 0.0f, 0.0f, Width, Height);
    }
}

void ARotorlineOperationsHUD::DrawCreditsRoll()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController) return;

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.67f, 1.25f);
    const float TextScale = FMath::Max(0.95f, Scale * 1.28f);
    const float Progress = OperationsController->GetCreditsScrollProgress();
    const FLinearColor White(0.94f, 0.97f, 0.95f, 1.0f);
    const FLinearColor Muted(0.80f, 0.87f, 0.85f, 1.0f);
    const FLinearColor Amber(1.0f, 0.69f, 0.22f, 1.0f);
    const FLinearColor Cyan(0.30f, 0.92f, 0.90f, 1.0f);

    const TArray<FRotorlineCreditLine>& Lines = GetRotorlineCreditRoll();
    auto LineHeight = [Scale](ERotorlineCreditStyle Style)
    {
        switch (Style)
        {
        case ERotorlineCreditStyle::Title: return 106.0f * Scale;
        case ERotorlineCreditStyle::Subtitle: return 70.0f * Scale;
        case ERotorlineCreditStyle::Section: return 82.0f * Scale;
        case ERotorlineCreditStyle::Name: return 56.0f * Scale;
        case ERotorlineCreditStyle::Detail: return 44.0f * Scale;
        default: return 52.0f * Scale;
        }
    };

    float RollHeight = 0.0f;
    for (const FRotorlineCreditLine& Line : Lines)
    {
        RollHeight += LineHeight(Line.Style);
    }

    const float PanelX = Width * 0.14f;
    const float PanelWidth = Width * 0.72f;
    const float TopSafe = 98.0f * Scale;
    const float BottomSafe = Height - 88.0f * Scale;
    DrawRect(FLinearColor(0.0f, 0.025f, 0.030f, 0.64f), PanelX, 0.0f, PanelWidth, Height);
    DrawRect(FLinearColor(0.18f, 0.82f, 0.80f, 0.38f), PanelX, 0.0f, 2.0f * Scale, Height);
    DrawRect(FLinearColor(0.18f, 0.82f, 0.80f, 0.38f), PanelX + PanelWidth - 2.0f * Scale, 0.0f, 2.0f * Scale, Height);

    float Y = Height + 120.0f * Scale - Progress * (RollHeight + Height + 240.0f * Scale);
    for (const FRotorlineCreditLine& Line : Lines)
    {
        const float HeightForLine = LineHeight(Line.Style);
        if (Line.Style != ERotorlineCreditStyle::Spacer && Y + HeightForLine >= TopSafe && Y <= BottomSafe)
        {
            UFont* Font = GEngine->GetSmallFont();
            float FontScale = 0.98f * TextScale;
            FLinearColor Color = Muted;
            switch (Line.Style)
            {
            case ERotorlineCreditStyle::Title:
                Font = GEngine->GetLargeFont();
                FontScale = 1.72f * TextScale;
                Color = White;
                break;
            case ERotorlineCreditStyle::Subtitle:
                FontScale = 1.12f * TextScale;
                Color = Amber;
                break;
            case ERotorlineCreditStyle::Section:
                FontScale = 1.16f * TextScale;
                Color = Cyan;
                break;
            case ERotorlineCreditStyle::Name:
                FontScale = 1.08f * TextScale;
                Color = White;
                break;
            default:
                break;
            }

            float TextWidth = 0.0f;
            float TextHeight = 0.0f;
            Canvas->StrLen(Font, Line.Text, TextWidth, TextHeight);
            const float DrawX = (Width - TextWidth * FontScale) * 0.5f;
            DrawText(Line.Text, FLinearColor(0.0f, 0.0f, 0.0f, 0.82f),
                DrawX + 2.0f * Scale, Y + 2.0f * Scale, Font, FontScale);
            DrawText(Line.Text, Color, DrawX, Y, Font, FontScale);
            if (Line.Style == ERotorlineCreditStyle::Section)
            {
                const float RuleWidth = FMath::Min(420.0f * Scale, PanelWidth * 0.42f);
                DrawRect(FLinearColor(1.0f, 0.69f, 0.22f, 0.72f),
                    (Width - RuleWidth) * 0.5f, Y + 31.0f * Scale, RuleWidth, 2.0f * Scale);
            }
        }
        Y += HeightForLine;
    }

    DrawRect(FLinearColor(0.0f, 0.025f, 0.030f, 0.86f), 0.0f, 0.0f, Width, TopSafe);
    DrawRect(Amber, Width * 0.36f, TopSafe - 4.0f * Scale, Width * 0.28f, 3.0f * Scale);
    DrawText(TEXT("ROTORLINE  //  OFFICIAL CREDITS"), White, 34.0f * Scale, 30.0f * Scale,
        GEngine->GetSmallFont(), 0.76f * TextScale);
}

void ARotorlineOperationsHUD::DrawCastGallery()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController) return;

    const TArray<FRotorlineCastMember>& Members = OperationsController->GetCastMembers();
    const int32 Count = Members.Num();
    if (Count <= 0)
    {
        DrawText(TEXT("PERSONNEL FILES UNAVAILABLE"), FLinearColor(1.0f, 0.35f, 0.20f, 1.0f),
            48.0f, 80.0f, GEngine->GetMediumFont(), 1.0f);
        DrawText(OperationsController->GetCastCatalogError(), FLinearColor(0.70f, 0.75f, 0.72f, 1.0f),
            48.0f, 125.0f, GEngine->GetSmallFont(), 0.8f);
        return;
    }

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.67f, 1.25f);
    const float TextScale = FMath::Max(0.92f, Scale * 1.15f);
    const FLinearColor Amber(1.0f, 0.68f, 0.18f, 1.0f);
    const FLinearColor Cyan(0.24f, 0.90f, 0.92f, 1.0f);
    const FLinearColor White(0.95f, 0.98f, 0.94f, 1.0f);
    const FLinearColor Muted(0.55f, 0.68f, 0.66f, 1.0f);

    DrawRect(FLinearColor(0.005f, 0.018f, 0.017f, 1.0f), 0.0f, 0.0f, Width, Height);
    for (int32 Line = 0; Line < 12; ++Line)
    {
        const float LineY = (Line + 1) * Height / 13.0f;
        DrawRect(FLinearColor(0.05f, 0.16f, 0.15f, 0.12f), 0.0f, LineY, Width, 1.0f);
    }
    DrawRect(Amber, 0.0f, 0.0f, Width, 6.0f * Scale);
    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.60f), 0.0f, 0.0f, Width, 72.0f * Scale);
    DrawText(TEXT("ROTORLINE  //  PERSONNEL FILES"), White, 48.0f * Scale, 25.0f * Scale,
        GEngine->GetSmallFont(), 0.76f * TextScale);

    const int32 Selected = FMath::Clamp(OperationsController->GetSelectedCastIndex(), 0, Count - 1);
    const FRotorlineCastMember& Member = Members[Selected];
    DrawText(FString::Printf(TEXT("SECURE DOSSIER  //  %02d OF %02d"), Selected + 1, Count),
        Muted, Width - 325.0f * Scale, 25.0f * Scale, GEngine->GetSmallFont(), 0.68f * TextScale);

    constexpr float CardAspect = 1402.0f / 1122.0f;
    const float CardHeight = FMath::Min(910.0f * Scale, Height - 170.0f * Scale);
    const float CardWidth = CardHeight * CardAspect;
    const float CardY = 85.0f * Scale;
    const float SideHeight = CardHeight * 0.62f;
    const float SideWidth = CardWidth * 0.62f;
    const float SideY = CardY + (CardHeight - SideHeight) * 0.50f;
    const int32 Previous = (Selected + Count - 1) % Count;
    const int32 Next = (Selected + 1) % Count;

    auto DrawSideCard = [&](int32 Index, bool bLeft)
    {
        UTexture2D* Texture = OperationsController->GetCastCardTexture(Index);
        if (!Texture) return;
        const float X = bLeft ? 40.0f * Scale - SideWidth * 0.70f : Width - 40.0f * Scale - SideWidth * 0.30f;
        DrawRect(FLinearColor(0.01f, 0.035f, 0.032f, 0.82f), X - 16.0f * Scale, SideY - 16.0f * Scale,
            SideWidth + 32.0f * Scale, SideHeight + 32.0f * Scale);
        DrawRect(FLinearColor(0.20f, 0.44f, 0.42f, 0.36f), X - 8.0f * Scale, SideY - 8.0f * Scale,
            SideWidth + 16.0f * Scale, SideHeight + 16.0f * Scale);
        DrawTexture(Texture, X, SideY, SideWidth, SideHeight, 0.0f, 0.0f, 1.0f, 1.0f,
            FLinearColor(0.28f, 0.34f, 0.32f, 0.32f), BLEND_Translucent);
    };
    DrawSideCard(Previous, true);
    DrawSideCard(Next, false);

    const float Transition = OperationsController->GetCastTransitionAlpha();
    const float Ease = 1.0f - FMath::Pow(Transition, 3.0f);
    const float ActiveScale = FMath::Lerp(0.94f, 1.0f, Ease);
    const float ActiveWidth = CardWidth * ActiveScale;
    const float ActiveHeight = CardHeight * ActiveScale;
    const float Direction = static_cast<float>(OperationsController->GetCastTransitionDirection());
    const float ActiveX = (Width - ActiveWidth) * 0.5f + Direction * 30.0f * Scale * Transition;
    const float ActiveY = CardY + (CardHeight - ActiveHeight) * 0.5f + 28.0f * Scale * Transition;

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.58f), ActiveX + 18.0f * Scale, ActiveY + 20.0f * Scale,
        ActiveWidth, ActiveHeight);
    DrawRect(FLinearColor(0.012f, 0.030f, 0.028f, 1.0f), ActiveX - 12.0f * Scale, ActiveY - 12.0f * Scale,
        ActiveWidth + 24.0f * Scale, ActiveHeight + 24.0f * Scale);
    DrawRect(Cyan, ActiveX - 4.0f * Scale, ActiveY - 4.0f * Scale,
        ActiveWidth + 8.0f * Scale, ActiveHeight + 8.0f * Scale);
    DrawRect(FLinearColor(0.012f, 0.030f, 0.028f, 1.0f), ActiveX - 2.0f * Scale, ActiveY - 2.0f * Scale,
        ActiveWidth + 4.0f * Scale, ActiveHeight + 4.0f * Scale);
    if (UTexture2D* Texture = OperationsController->GetCastCardTexture(Selected))
    {
        DrawTexture(Texture, ActiveX, ActiveY, ActiveWidth, ActiveHeight, 0.0f, 0.0f, 1.0f, 1.0f,
            FLinearColor::White, BLEND_Opaque);
    }
    else
    {
        DrawRect(FLinearColor(0.04f, 0.08f, 0.075f, 1.0f), ActiveX, ActiveY, ActiveWidth, ActiveHeight);
        DrawText(TEXT("DOSSIER IMAGE UNAVAILABLE"), Muted, ActiveX + 70.0f * Scale, ActiveY + ActiveHeight * 0.50f,
            GEngine->GetSmallFont(), 0.76f * TextScale);
    }

    const float TabX = 20.0f * Scale;
    const float TabY = 150.0f * Scale;
    const float TabWidth = 215.0f * Scale;
    const float TabHeight = 48.0f * Scale;
    const float TabGap = 8.0f * Scale;
    const float TabPanelHeight = 40.0f * Scale + Count * (TabHeight + TabGap);
    DrawRect(FLinearColor(0.0f, 0.018f, 0.019f, 0.94f), TabX - 8.0f * Scale, TabY - 40.0f * Scale,
        TabWidth + 16.0f * Scale, TabPanelHeight);
    DrawText(TEXT("SELECT PROFILE"), Amber, TabX + 8.0f * Scale, TabY - 31.0f * Scale,
        GEngine->GetSmallFont(), 0.66f * TextScale);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const bool bSelected = Index == Selected;
        const float ItemY = TabY + Index * (TabHeight + TabGap);
        DrawRect(bSelected ? FLinearColor(0.035f, 0.20f, 0.19f, 0.98f) : FLinearColor(0.015f, 0.055f, 0.055f, 0.94f),
            TabX, ItemY, TabWidth, TabHeight);
        DrawRect(bSelected ? Amber : FLinearColor(0.20f, 0.45f, 0.43f, 0.72f),
            TabX, ItemY, bSelected ? 6.0f * Scale : 2.0f * Scale, TabHeight);
        DrawText(Members[Index].Callsign.ToUpper(), bSelected ? White : Muted,
            TabX + 17.0f * Scale, ItemY + 7.0f * Scale, GEngine->GetSmallFont(), 0.68f * TextScale);
        DrawText(Members[Index].Role.ToUpper(), bSelected ? Cyan : FLinearColor(0.38f, 0.56f, 0.54f, 1.0f),
            TabX + 17.0f * Scale, ItemY + 27.0f * Scale, GEngine->GetSmallFont(), 0.50f * TextScale);
    }
    DrawText(TEXT("CLICK CALLSIGN TO SELECT"), Cyan, TabX, TabY + Count * (TabHeight + TabGap) + 6.0f * Scale,
        GEngine->GetSmallFont(), 0.52f * TextScale);

    DrawRect(FLinearColor(0.0f, 0.025f, 0.026f, 0.92f), 0.0f, Height - 62.0f * Scale, Width, 62.0f * Scale);
    DrawRect(Cyan, 0.0f, Height - 62.0f * Scale, Width * OperationsController->GetCastVoiceProgress(), 3.0f * Scale);
    const FString VoiceStatus = OperationsController->IsCastVoicePlaying() ? TEXT("PLAYING") : TEXT("READY");
    DrawText(FString::Printf(TEXT("%s  //  %s     VOICE PROFILE  //  %s"), *Member.Callsign, *Member.Role, *VoiceStatus),
        White, 48.0f * Scale, Height - 43.0f * Scale, GEngine->GetSmallFont(), 0.70f * TextScale);
    DrawText(TEXT("PS5  D-PAD / LEFT STICK  SELECT     X  SELECT / PLAY VOICE     CIRCLE  RETURN"),
        Muted, Width - 690.0f * Scale, Height - 43.0f * Scale, GEngine->GetSmallFont(), 0.59f * TextScale);

    const float DotWidth = 22.0f * Scale;
    const float DotGap = 10.0f * Scale;
    const float DotsX = Width * 0.5f - (Count * DotWidth + (Count - 1) * DotGap) * 0.5f;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        DrawRect(Index == Selected ? Amber : FLinearColor(0.20f, 0.42f, 0.40f, 0.55f),
            DotsX + Index * (DotWidth + DotGap), Height - 14.0f * Scale,
            DotWidth, 3.0f * Scale);
    }
}

void ARotorlineOperationsHUD::DrawAwardPresentationOverlay()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController) return;
    const TArray<FRotorlineAwardEvaluation>& Earned = OperationsController->GetNewlyEarnedAwards();
    const int32 Index = OperationsController->GetAwardPresentationIndex();
    if (!Earned.IsValidIndex(Index)) return;
    const FRotorlineAwardEvaluation& Evaluation = Earned[Index];
    const FRotorlineAwardDefinition* Definition = OperationsController->GetAwardDefinition(Evaluation.AwardId);
    if (!Definition) return;

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.67f, 1.20f);
    const float ReadableTextScale = FMath::Max(1.0f, Scale * 1.55f);
    const float PanelWidth = FMath::Min(1880.0f * Scale, Width - 48.0f * Scale);
    const float PanelHeight = FMath::Min(950.0f * Scale, Height - 30.0f * Scale);
    const float PanelX = (Width - PanelWidth) * 0.5f;
    const float PanelY = (Height - PanelHeight) * 0.5f;
    const FLinearColor Amber(1.0f, 0.68f, 0.18f, 1.0f);
    const FLinearColor Cyan(0.24f, 0.90f, 0.92f, 1.0f);
    const FLinearColor White(0.95f, 0.98f, 0.94f, 1.0f);
    const FLinearColor Muted(0.55f, 0.68f, 0.66f, 1.0f);

    DrawRect(FLinearColor(0.0f, 0.008f, 0.008f, 0.92f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.015f, 0.035f, 0.033f, 0.995f), PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(Amber, PanelX, PanelY, PanelWidth, 8.0f * Scale);
    DrawText(TEXT("PATCH EARNED"), Amber, PanelX + 52.0f * Scale, PanelY + 32.0f * Scale,
        GEngine->GetLargeFont(), 1.26f * ReadableTextScale);
    DrawText(FString::Printf(TEXT("NEW UNLOCK  //  %d OF %d"), Index + 1, Earned.Num()), Cyan,
        PanelX + 54.0f * Scale, PanelY + 102.0f * Scale, GEngine->GetSmallFont(), 0.82f * ReadableTextScale);

    const float PatchSize = 650.0f * Scale;
    const float PatchX = PanelX + 64.0f * Scale;
    const float PatchY = PanelY + 172.0f * Scale;
    DrawRect(FLinearColor(0.025f, 0.07f, 0.064f, 1.0f), PatchX - 16.0f * Scale, PatchY - 16.0f * Scale,
        PatchSize + 32.0f * Scale, PatchSize + 32.0f * Scale);
    if (UTexture2D* Texture = OperationsController->GetAwardPatchTexture(Definition->Id))
    {
        DrawTexture(Texture, PatchX, PatchY, PatchSize, PatchSize, 0.0f, 0.0f, 1.0f, 1.0f,
            FLinearColor::White, BLEND_Translucent);
    }
    else
    {
        DrawRect(FLinearColor(0.08f, 0.12f, 0.11f, 1.0f), PatchX, PatchY, PatchSize, PatchSize);
        DrawText(TEXT("?"), Muted, PatchX + PatchSize * 0.38f, PatchY + PatchSize * 0.25f,
            GEngine->GetLargeFont(), 4.0f * Scale);
        DrawText(TEXT("PATCH ART UNAVAILABLE"), Muted, PatchX + 40.0f * Scale,
            PatchY + PatchSize - 38.0f * Scale, GEngine->GetSmallFont(), 0.72f * Scale);
    }

    const float DetailX = PatchX + PatchSize + 70.0f * Scale;
    const float DetailWidth = PanelX + PanelWidth - DetailX - 58.0f * Scale;
    DrawText(Definition->DisplayName.ToUpper(), White, DetailX, PanelY + 178.0f * Scale,
        GEngine->GetLargeFont(), 1.18f * ReadableTextScale);
    DrawText(FString::Printf(TEXT("%s  //  %s"), *Definition->Category.ToUpper(), *Definition->Rarity.ToUpper()),
        Amber, DetailX, PanelY + 250.0f * Scale, GEngine->GetSmallFont(), 0.84f * ReadableTextScale);
    DrawWrappedText(Definition->Description, DetailX, PanelY + 304.0f * Scale, DetailWidth,
        0.84f * ReadableTextScale, White, 4);

    DrawRect(FLinearColor(0.025f, 0.065f, 0.060f, 1.0f), DetailX, PanelY + 428.0f * Scale,
        DetailWidth, 270.0f * Scale);
    DrawText(TEXT("WHY YOU EARNED IT"), Cyan, DetailX + 24.0f * Scale, PanelY + 450.0f * Scale,
        GEngine->GetSmallFont(), 0.80f * ReadableTextScale);
    DrawWrappedText(Evaluation.Reason, DetailX + 24.0f * Scale, PanelY + 494.0f * Scale,
        DetailWidth - 48.0f * Scale, 0.80f * ReadableTextScale, White, 5);
    if (!Evaluation.RelevantStat.IsEmpty())
    {
        DrawText(FString::Printf(TEXT("RECORDED STAT  //  %.1f"), Evaluation.AssociatedStatValue), Amber,
            DetailX + 24.0f * Scale, PanelY + 652.0f * Scale,
            GEngine->GetSmallFont(), 0.76f * ReadableTextScale);
    }

    DrawText(Index + 1 < Earned.Num()
            ? TEXT("X / ENTER  NEXT PATCH")
            : TEXT("X / ENTER  CONTINUE TO DEBRIEF"),
        White, PanelX + 54.0f * Scale, PanelY + PanelHeight - 58.0f * Scale,
        GEngine->GetSmallFont(), 0.82f * ReadableTextScale);
}

void ARotorlineOperationsHUD::DrawPatchWall()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController) return;
    const TArray<FRotorlineAwardDefinition>& Definitions = OperationsController->GetAwardDefinitions();
    if (Definitions.IsEmpty()) return;

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 900.0f, 0.80f, 1.20f);
    const float ReadableTextScale = FMath::Max(1.0f, Scale * 1.35f);
    const float PanelX = 24.0f * Scale;
    const float PanelY = 20.0f * Scale;
    const float PanelWidth = Width - 48.0f * Scale;
    const float PanelHeight = Height - 40.0f * Scale;
    const FLinearColor Amber(1.0f, 0.68f, 0.18f, 1.0f);
    const FLinearColor Cyan(0.24f, 0.90f, 0.92f, 1.0f);
    const FLinearColor White(0.95f, 0.98f, 0.94f, 1.0f);
    const FLinearColor Muted(0.48f, 0.59f, 0.57f, 1.0f);

    DrawRect(FLinearColor(0.005f, 0.018f, 0.017f, 0.995f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.018f, 0.043f, 0.040f, 1.0f), PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(Amber, PanelX, PanelY, PanelWidth, 7.0f * Scale);
    DrawText(TEXT("PILOT READY ROOM  //  PATCH WALL"), White, PanelX + 32.0f * Scale,
        PanelY + 24.0f * Scale, GEngine->GetLargeFont(), 1.06f * ReadableTextScale);
    DrawText(FString::Printf(TEXT("COLLECTION %.1f%%  //  %d PATCHES"),
        OperationsController->GetAwardCompletionPercent(), Definitions.Num()), Cyan,
        PanelX + 34.0f * Scale, PanelY + 72.0f * Scale, GEngine->GetSmallFont(), 0.72f * ReadableTextScale);

    constexpr int32 Columns = 5;
    constexpr int32 VisibleRows = 4;
    const int32 Selected = FMath::Clamp(OperationsController->GetPatchWallSelection(), 0, Definitions.Num() - 1);
    const int32 SelectedRow = Selected / Columns;
    const int32 TotalRows = FMath::DivideAndRoundUp(Definitions.Num(), Columns);
    const int32 FirstRow = FMath::Clamp(SelectedRow - VisibleRows / 2, 0, FMath::Max(0, TotalRows - VisibleRows));
    const float GridX = PanelX + 30.0f * Scale;
    const float GridY = PanelY + 118.0f * Scale;
    const float TileWidth = 124.0f * Scale;
    const float TileHeight = 118.0f * Scale;
    const float PatchSize = 104.0f * Scale;

    for (int32 Row = 0; Row < VisibleRows; ++Row)
    {
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            const int32 Index = (FirstRow + Row) * Columns + Column;
            if (!Definitions.IsValidIndex(Index)) continue;
            const FRotorlineAwardDefinition& Definition = Definitions[Index];
            const FRotorlinePlayerAwardRecord* Record = OperationsController->GetAwardRecord(Definition.Id);
            const bool bEarned = Record && Record->TimesEarned > 0;
            const bool bSelected = Index == Selected;
            const float X = GridX + Column * TileWidth;
            const float Y = GridY + Row * TileHeight;
            DrawRect(bSelected ? FLinearColor(0.10f, 0.17f, 0.15f, 1.0f) : FLinearColor(0.025f, 0.065f, 0.060f, 1.0f),
                X, Y, TileWidth - 10.0f * Scale, TileHeight - 10.0f * Scale);
            if (bSelected) DrawRect(Amber, X, Y, 5.0f * Scale, TileHeight - 10.0f * Scale);
            const float ArtX = X + 5.0f * Scale;
            const float ArtY = Y + 2.0f * Scale;
            if (bEarned)
            {
                if (UTexture2D* Texture = OperationsController->GetAwardPatchTexture(Definition.Id))
                {
                    DrawTexture(Texture, ArtX, ArtY, PatchSize, PatchSize, 0.0f, 0.0f, 1.0f, 1.0f,
                        FLinearColor::White, BLEND_Translucent);
                }
                else
                {
                    DrawRect(FLinearColor(0.08f, 0.12f, 0.11f, 1.0f), ArtX, ArtY, PatchSize, PatchSize);
                    DrawText(TEXT("?"), Muted, ArtX + 39.0f * Scale, ArtY + 24.0f * Scale,
                        GEngine->GetLargeFont(), 1.8f * Scale);
                }
            }
            else if (Definition.bHidden)
            {
                DrawRect(FLinearColor(0.018f, 0.025f, 0.024f, 1.0f), ArtX, ArtY, PatchSize, PatchSize);
                DrawText(TEXT("?"), Muted, ArtX + 39.0f * Scale, ArtY + 24.0f * Scale,
                    GEngine->GetLargeFont(), 1.8f * Scale);
            }
            else if (UTexture2D* Texture = OperationsController->GetAwardPatchTexture(Definition.Id))
            {
                DrawTexture(Texture, ArtX, ArtY, PatchSize, PatchSize, 0.0f, 0.0f, 1.0f, 1.0f,
                    FLinearColor(0.22f, 0.24f, 0.22f, 0.95f), BLEND_Translucent);
            }
        }
    }

    const FRotorlineAwardDefinition& SelectedDefinition = Definitions[Selected];
    const FRotorlinePlayerAwardRecord* SelectedRecord = OperationsController->GetAwardRecord(SelectedDefinition.Id);
    const bool bSelectedEarned = SelectedRecord && SelectedRecord->TimesEarned > 0;
    const bool bSelectedClassified = SelectedDefinition.bHidden && !bSelectedEarned;
    const float DetailX = GridX + Columns * TileWidth + 28.0f * Scale;
    const float DetailWidth = PanelX + PanelWidth - DetailX - 30.0f * Scale;
    const float DetailPanelHeight = PanelHeight - 170.0f * Scale;
    const float DetailPanelBottom = GridY + DetailPanelHeight;
    DrawRect(FLinearColor(0.012f, 0.030f, 0.028f, 1.0f), DetailX, GridY,
        DetailWidth, DetailPanelHeight);
    DrawText(bSelectedEarned || !SelectedDefinition.bHidden ? SelectedDefinition.DisplayName.ToUpper() : TEXT("CLASSIFIED PATCH"),
        bSelectedEarned ? White : Muted, DetailX + 24.0f * Scale, GridY + 25.0f * Scale,
        GEngine->GetLargeFont(), 0.96f * ReadableTextScale);
    DrawText(bSelectedClassified
            ? TEXT("CLASSIFIED  //  ACCESS RESTRICTED  //  LOCKED")
            : FString::Printf(TEXT("%s  //  %s  //  %s"), *SelectedDefinition.Category.ToUpper(),
                *SelectedDefinition.Rarity.ToUpper(), bSelectedEarned ? TEXT("EARNED") : TEXT("LOCKED")),
        bSelectedEarned ? Amber : Muted, DetailX + 26.0f * Scale, GridY + 74.0f * Scale,
        GEngine->GetSmallFont(), 0.70f * ReadableTextScale);
    // Make the selected patch the visual focus. Its size is bounded by the
    // information column and career record so the layout remains usable at
    // lower resolutions while filling the available space at 1080p and up.
    const float CareerHeight = 190.0f * Scale;
    const float CareerY = DetailPanelBottom - CareerHeight - 12.0f * Scale;
    const float DetailArtX = DetailX + 26.0f * Scale;
    const float DetailArtY = GridY + 112.0f * Scale;
    const float DetailArtSize = FMath::Max(220.0f * Scale, FMath::Min3(
        370.0f * Scale,
        DetailWidth * 0.46f,
        CareerY - DetailArtY - 16.0f * Scale));
    DrawRect(FLinearColor(0.025f, 0.065f, 0.060f, 1.0f), DetailArtX - 8.0f * Scale,
        DetailArtY - 8.0f * Scale, DetailArtSize + 16.0f * Scale, DetailArtSize + 16.0f * Scale);
    if (bSelectedEarned)
    {
        if (UTexture2D* Texture = OperationsController->GetAwardPatchTexture(SelectedDefinition.Id))
        {
            DrawTexture(Texture, DetailArtX, DetailArtY, DetailArtSize, DetailArtSize,
                0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, BLEND_Translucent);
        }
        else
        {
            DrawRect(FLinearColor(0.018f, 0.025f, 0.024f, 1.0f), DetailArtX, DetailArtY, DetailArtSize, DetailArtSize);
            DrawText(TEXT("?"), Muted, DetailArtX + 68.0f * Scale, DetailArtY + 32.0f * Scale,
                GEngine->GetLargeFont(), 2.6f * Scale);
        }
    }
    else if (SelectedDefinition.bHidden)
    {
        DrawRect(FLinearColor(0.018f, 0.025f, 0.024f, 1.0f), DetailArtX, DetailArtY, DetailArtSize, DetailArtSize);
        DrawText(TEXT("?"), Muted, DetailArtX + 68.0f * Scale, DetailArtY + 32.0f * Scale,
            GEngine->GetLargeFont(), 2.6f * Scale);
    }
    else if (UTexture2D* Texture = OperationsController->GetAwardPatchTexture(SelectedDefinition.Id))
    {
        DrawTexture(Texture, DetailArtX, DetailArtY, DetailArtSize, DetailArtSize,
            0.0f, 0.0f, 1.0f, 1.0f, FLinearColor(0.22f, 0.24f, 0.22f, 0.95f), BLEND_Translucent);
    }
    const float DetailCopyX = DetailArtX + DetailArtSize + 28.0f * Scale;
    const float DetailCopyWidth = DetailWidth - (DetailCopyX - DetailX) - 26.0f * Scale;
    const FString Description = bSelectedEarned || !SelectedDefinition.bHidden
        ? SelectedDefinition.Description
        : TEXT("ENCRYPTED REWARD // Continue campaign operations to declassify this prize.");
    DrawText(TEXT("PATCH DIRECTIVE"), Cyan, DetailCopyX, DetailArtY + 2.0f * Scale,
        GEngine->GetSmallFont(), 0.70f * ReadableTextScale);
    DrawWrappedText(Description, DetailCopyX, DetailArtY + 42.0f * Scale,
        DetailCopyWidth, 0.78f * ReadableTextScale, White, 6);

    const float RecordY = DetailArtY + 168.0f * Scale;
    const float RecordHeight = FMath::Max(142.0f * Scale, CareerY - RecordY - 22.0f * Scale);
    DrawRect(FLinearColor(0.022f, 0.060f, 0.055f, 1.0f), DetailCopyX - 12.0f * Scale,
        RecordY - 12.0f * Scale, DetailCopyWidth + 12.0f * Scale, RecordHeight);
    DrawRect(Cyan, DetailCopyX - 12.0f * Scale, RecordY - 12.0f * Scale,
        4.0f * Scale, RecordHeight);
    if (bSelectedEarned)
    {
        DrawText(TEXT("SERVICE RECORD"), Cyan, DetailCopyX, RecordY,
            GEngine->GetSmallFont(), 0.70f * ReadableTextScale);
        DrawText(FString::Printf(TEXT("FIRST EARNED  //  %s"), *SelectedRecord->FirstEarnedUtc.Left(10)),
            White, DetailCopyX, RecordY + 40.0f * Scale,
            GEngine->GetSmallFont(), 0.68f * ReadableTextScale);
        const FString EarnedMission = SelectedRecord->FirstMissionTitle.IsEmpty()
            ? TEXT("FIELD RECORD")
            : SelectedRecord->FirstMissionTitle.ToUpper();
        DrawText(FString::Printf(TEXT("MISSION  //  %s"), *EarnedMission),
            White, DetailCopyX, RecordY + 76.0f * Scale,
            GEngine->GetSmallFont(), 0.66f * ReadableTextScale);
        DrawText(FString::Printf(TEXT("BEST ASSOCIATED STAT  //  %.1f"), SelectedRecord->BestAssociatedStat),
            Amber, DetailCopyX, RecordY + 112.0f * Scale,
            GEngine->GetSmallFont(), 0.66f * ReadableTextScale);
    }
    else if (!SelectedDefinition.bHidden)
    {
        DrawText(TEXT("UNLOCK HINT"), Cyan, DetailCopyX, RecordY,
            GEngine->GetSmallFont(), 0.70f * ReadableTextScale);
        DrawWrappedText(SelectedDefinition.Hint, DetailCopyX, RecordY + 40.0f * Scale,
            DetailCopyWidth, 0.70f * ReadableTextScale, Muted, 5);
    }
    else
    {
        DrawText(TEXT("CLASSIFIED SERVICE RECORD"), Muted, DetailCopyX, RecordY,
            GEngine->GetSmallFont(), 0.70f * ReadableTextScale);
    }

    if (const FRotorlineCareerStatistics* Career = OperationsController->GetCareerStatistics())
    {
        const float CareerColumnWidth = (DetailWidth - 72.0f * Scale) * 0.5f;
        const int32 FlightHours = FMath::FloorToInt(Career->TotalFlightTimeSeconds / 3600.0f);
        const int32 FlightMinutes = FMath::FloorToInt(FMath::Fmod(Career->TotalFlightTimeSeconds, 3600.0f) / 60.0f);
        const float WeaponAccuracy = Career->ShotsFired > 0
            ? 100.0f * static_cast<float>(Career->WeaponHits) / static_cast<float>(Career->ShotsFired)
            : 0.0f;
        const int32 PersonnelRescued = Career->CiviliansRescued + Career->SoldiersRescued;
        const int32 EnemiesDestroyed = Career->EnemyVehiclesDestroyed + Career->EnemyHelicoptersDestroyed;

        DrawRect(FLinearColor(0.022f, 0.060f, 0.055f, 1.0f), DetailX + 18.0f * Scale, CareerY,
            DetailWidth - 36.0f * Scale, CareerHeight);
        DrawRect(Cyan, DetailX + 18.0f * Scale, CareerY, 5.0f * Scale, CareerHeight);
        DrawText(TEXT("CAREER FLIGHT RECORD"), Cyan, DetailX + 38.0f * Scale, CareerY + 18.0f * Scale,
            GEngine->GetSmallFont(), 0.72f * ReadableTextScale);

        const TArray<FString> LeftCareerStats = {
            FString::Printf(TEXT("MISSIONS COMPLETED   %d / %d"), Career->MissionsCompleted, Career->MissionsStarted),
            FString::Printf(TEXT("FLIGHT TIME          %02dH %02dM"), FlightHours, FlightMinutes),
            FString::Printf(TEXT("DISTANCE FLOWN       %.1f KM"), Career->TotalDistanceMeters / 1000.0f),
            FString::Printf(TEXT("PERSONNEL RESCUED    %d"), PersonnelRescued),
            FString::Printf(TEXT("SUCCESSFUL LANDINGS  %d"), Career->SuccessfulLandings)
        };
        const TArray<FString> RightCareerStats = {
            FString::Printf(TEXT("ENEMIES DESTROYED    %d"), EnemiesDestroyed),
            FString::Printf(TEXT("WEAPON ACCURACY      %.1f%%"), WeaponAccuracy),
            FString::Printf(TEXT("MISSILES DODGED      %d"), Career->MissilesDodged),
            FString::Printf(TEXT("BEST MISSION SCORE   %d"), Career->BestMissionScore),
            FString::Printf(TEXT("BEST SUCCESS STREAK  %d"), Career->BestSuccessfulMissionStreak)
        };
        for (int32 Row = 0; Row < LeftCareerStats.Num(); ++Row)
        {
            const float RowY = CareerY + (48.0f + Row * 27.5f) * Scale;
            DrawText(LeftCareerStats[Row], White, DetailX + 40.0f * Scale, RowY,
                GEngine->GetSmallFont(), 0.61f * ReadableTextScale);
            DrawText(RightCareerStats[Row], White, DetailX + 40.0f * Scale + CareerColumnWidth, RowY,
                GEngine->GetSmallFont(), 0.61f * ReadableTextScale);
        }
    }

    DrawText(OperationsController->IsStartupPatchWallOpen()
            ? TEXT("D-PAD / LEFT STICK  BROWSE     CIRCLE / ESC  RETURN TO MAIN MENU")
            : TEXT("D-PAD / ARROWS  BROWSE     SQUARE / P / CIRCLE / ESC  CLOSE"), White,
        PanelX + 32.0f * Scale, PanelY + PanelHeight - 48.0f * Scale,
        GEngine->GetSmallFont(), 0.70f * ReadableTextScale);
}

void ARotorlineOperationsHUD::DrawMissionCompleteOverlay()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController)
    {
        return;
    }

    const FRotorlineMissionResults& Results = OperationsController->GetMissionResults();
    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.67f, 1.20f);
    // Canvas panels already scale with resolution, but the legacy small-font
    // multipliers made the text physically tiny at 1080p. Keep the layout
    // geometry unchanged and apply an accessibility boost to text only.
    const float ReadableTextScale = Scale * 1.60f;
    const float PanelWidth = FMath::Min(1120.0f * Scale, Width - 36.0f);
    const float PanelHeight = FMath::Min(990.0f * Scale, Height - 28.0f);
    const float PanelX = (Width - PanelWidth) * 0.5f;
    const float PanelY = (Height - PanelHeight) * 0.5f;
    const FLinearColor Green(0.22f, 1.0f, 0.56f, 1.0f);
    const FLinearColor Cyan(0.24f, 0.90f, 0.92f, 1.0f);
    const FLinearColor Amber(1.0f, 0.68f, 0.18f, 1.0f);
    const FLinearColor White(0.95f, 0.98f, 0.94f, 1.0f);
    const FLinearColor Muted(0.58f, 0.68f, 0.65f, 1.0f);

    DrawRect(FLinearColor(0.0f, 0.015f, 0.012f, 0.88f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.018f, 0.045f, 0.040f, 0.99f), PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(Green, PanelX, PanelY, PanelWidth, 7.0f * Scale);
    DrawRect(FLinearColor(0.03f, 0.11f, 0.09f, 1.0f), PanelX, PanelY + 7.0f * Scale, PanelWidth, 145.0f * Scale);

    DrawText(TEXT("MISSION COMPLETE"), Green,
        PanelX + 42.0f * Scale, PanelY + 31.0f * Scale,
        GEngine->GetLargeFont(), 1.35f * Scale);
    DrawText(Results.MissionTitle.ToUpper(), White,
        PanelX + 44.0f * Scale, PanelY + 84.0f * Scale,
        GEngine->GetSmallFont(), 1.00f * ReadableTextScale);
    DrawText(FString::Printf(TEXT("%s  //  %s"), *Results.MissionCallsign.ToUpper(), *Results.AircraftName.ToUpper()),
        Cyan, PanelX + 44.0f * Scale, PanelY + 119.0f * Scale,
        GEngine->GetSmallFont(), 0.76f * ReadableTextScale);

    const int32 Minutes = FMath::FloorToInt(Results.ElapsedSeconds / 60.0f);
    const int32 Seconds = FMath::FloorToInt(FMath::Fmod(Results.ElapsedSeconds, 60.0f));
    const float SummaryY = PanelY + 176.0f * Scale;
    const float SummaryWidth = PanelWidth - 84.0f * Scale;
    DrawRect(FLinearColor(0.025f, 0.075f, 0.068f, 1.0f), PanelX + 42.0f * Scale, SummaryY, SummaryWidth, 116.0f * Scale);
    DrawText(TEXT("COMPLETION TIME"), Muted, PanelX + 68.0f * Scale, SummaryY + 18.0f * Scale, GEngine->GetSmallFont(), 0.68f * ReadableTextScale);
    DrawText(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds), White, PanelX + 68.0f * Scale, SummaryY + 51.0f * Scale, GEngine->GetLargeFont(), 1.04f * Scale);
    DrawText(TEXT("PRIMARY OBJECTIVES"), Muted, PanelX + 386.0f * Scale, SummaryY + 18.0f * Scale, GEngine->GetSmallFont(), 0.68f * ReadableTextScale);
    DrawText(FString::Printf(TEXT("%d / %d"), Results.PrimaryObjectivesCompleted, Results.PrimaryObjectivesTotal), Green,
        PanelX + 386.0f * Scale, SummaryY + 51.0f * Scale, GEngine->GetLargeFont(), 1.04f * Scale);
    DrawText(TEXT("FINAL SCORE"), Muted, PanelX + 740.0f * Scale, SummaryY + 18.0f * Scale, GEngine->GetSmallFont(), 0.68f * ReadableTextScale);
    DrawText(FString::Printf(TEXT("%07d"), Results.FinalScore), Amber,
        PanelX + 740.0f * Scale, SummaryY + 51.0f * Scale, GEngine->GetLargeFont(), 1.04f * Scale);

    const float StatPanelY = PanelY + 316.0f * Scale;
    const float StatPanelHeight = 214.0f * Scale;
    DrawRect(FLinearColor(0.015f, 0.032f, 0.030f, 1.0f), PanelX + 42.0f * Scale, StatPanelY, SummaryWidth, StatPanelHeight);
    DrawText(TEXT("SORTIE RESULTS"), Cyan, PanelX + 66.0f * Scale, StatPanelY + 15.0f * Scale, GEngine->GetSmallFont(), 0.76f * ReadableTextScale);

    TArray<FString> LeftStats;
    TArray<FString> RightStats;
    LeftStats.Add(FString::Printf(TEXT("ENEMY HELICOPTERS DESTROYED   %d"), Results.EnemyHelicoptersDestroyed));
    LeftStats.Add(FString::Printf(TEXT("GROUND ENEMIES DESTROYED      %d"), Results.GroundEnemiesDestroyed));
    LeftStats.Add(FString::Printf(TEXT("DAMAGE TAKEN                 %.0f"), Results.DamageTaken));
    LeftStats.Add(FString::Printf(TEXT("LANDING / STABLE HOVER        %s / %.1f SEC"),
        Results.bSafeLanding ? TEXT("SAFE") : TEXT("NOT SAFE"), Results.StableHoverSeconds));
    if (Results.OptionalObjectivesTotal > 0)
    {
        LeftStats.Add(FString::Printf(TEXT("OPTIONAL OBJECTIVES            %d / %d"), Results.OptionalObjectivesCompleted, Results.OptionalObjectivesTotal));
    }
    if (Results.bCivilianRescueTracked)
    {
        RightStats.Add(FString::Printf(
            TEXT("PERSONNEL RESCUED              %d"),
            Results.CiviliansRescued + Results.SoldiersRescued));
    }
    if (Results.bCargoTracked)
    {
        RightStats.Add(FString::Printf(TEXT("CARGO DELIVERED                %d"), Results.CargoDelivered));
    }
    if (Results.bAircraftConditionTracked && Results.AircraftMaxHealth > KINDA_SMALL_NUMBER)
    {
        RightStats.Add(FString::Printf(TEXT("AIRCRAFT CONDITION           %.0f%%"),
            100.0f * Results.AircraftHealth / Results.AircraftMaxHealth));
    }
    if (Results.bWeaponsTracked)
    {
        const float Accuracy = Results.WeaponShotsFired > 0
            ? 100.0f * static_cast<float>(Results.WeaponHits) / static_cast<float>(Results.WeaponShotsFired)
            : 0.0f;
        RightStats.Add(FString::Printf(TEXT("WEAPON ACCURACY              %.1f%%  (%d/%d)"),
            Accuracy, Results.WeaponHits, Results.WeaponShotsFired));
    }
    RightStats.Add(FString::Printf(TEXT("MISSION REWARD                +%d XP"), Results.ExperienceAwarded));
    RightStats.Add(FString::Printf(TEXT("MISSION RATING                 %d / 5"), Results.StarRating));

    const float StatStartY = StatPanelY + 55.0f * Scale;
    for (int32 Index = 0; Index < LeftStats.Num() && Index < 5; ++Index)
    {
        DrawText(LeftStats[Index], White, PanelX + 66.0f * Scale,
            StatStartY + Index * 35.0f * Scale, GEngine->GetSmallFont(), 0.69f * ReadableTextScale);
    }
    for (int32 Index = 0; Index < RightStats.Num() && Index < 5; ++Index)
    {
        DrawText(RightStats[Index], White, PanelX + 574.0f * Scale,
            StatStartY + Index * 31.0f * Scale, GEngine->GetSmallFont(), 0.65f * ReadableTextScale);
    }

    static const TCHAR* Labels[] =
    {
        TEXT("CHOOSE ANOTHER LEVEL"),
        TEXT("REPLAY MISSION"),
        TEXT("RETURN TO HANGAR"),
        TEXT("RETURN TO MAIN MENU")
    };
    static const TCHAR* Details[] =
    {
        TEXT("Return to the Operations Board and select a new assignment."),
        TEXT("Reset this sortie and redeploy the same aircraft."),
        TEXT("Review or change aircraft for the selected mission."),
        TEXT("Reload the island into its clean startup state.")
    };
    const float ActionX = PanelX + 42.0f * Scale;
    const float ActionY = PanelY + 554.0f * Scale;
    const float ActionWidth = SummaryWidth;
    const float ActionHeight = 83.0f * Scale;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const bool bSelected = Index == OperationsController->GetSelectedMissionCompleteAction();
        const float Y = ActionY + Index * ActionHeight;
        if (bSelected)
        {
            DrawRect(FLinearColor(0.035f, 0.16f, 0.12f, 1.0f), ActionX, Y, ActionWidth, 70.0f * Scale);
            DrawRect(Index == 1 ? Amber : Green, ActionX, Y, 7.0f * Scale, 70.0f * Scale);
        }
        DrawText(Labels[Index], bSelected ? (Index == 1 ? Amber : Green) : Muted,
            ActionX + 23.0f * Scale, Y + 8.0f * Scale, GEngine->GetSmallFont(), 0.82f * ReadableTextScale);
        DrawText(Details[Index], bSelected ? White : Muted,
            ActionX + 372.0f * Scale, Y + 12.0f * Scale, GEngine->GetSmallFont(), 0.62f * ReadableTextScale);
    }

    DrawText(TEXT("UP / DOWN  SELECT     X / ENTER  CONFIRM"), White,
        PanelX + 44.0f * Scale, PanelY + 932.0f * Scale,
        GEngine->GetSmallFont(), 0.72f * ReadableTextScale);
}

void ARotorlineOperationsHUD::DrawMissionFailureOverlay()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController)
    {
        return;
    }

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.72f, 1.35f);
    const float PanelWidth = 780.0f * Scale;
    const float PanelHeight = 540.0f * Scale;
    const float PanelX = (Width - PanelWidth) * 0.5f;
    const float PanelY = (Height - PanelHeight) * 0.5f;
    const FLinearColor Red(1.0f, 0.14f, 0.06f, 1.0f);
    const FLinearColor Amber(1.0f, 0.67f, 0.14f, 1.0f);
    const FLinearColor White(0.95f, 0.97f, 0.92f, 1.0f);

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.025f, 0.012f, 0.010f, 0.99f), PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(Red, PanelX, PanelY, PanelWidth, 7.0f * Scale);
    DrawText(TEXT("MISSION FAILED"), Red, PanelX + 44.0f * Scale, PanelY + 38.0f * Scale, GEngine->GetLargeFont(), 1.42f * Scale);

    FString FailureReason = OperationsController->GetMissionFailureReason().ToUpper();
    if (FailureReason.IsEmpty())
    {
        FailureReason = TEXT("AIRCRAFT LOST");
    }
    DrawText(FailureReason, White, PanelX + 46.0f * Scale, PanelY + 104.0f * Scale, GEngine->GetSmallFont(), 1.0f * Scale);
    DrawText(TEXT("THE SORTIE HAS ENDED // FLIGHT CONTROLS LOCKED"), FLinearColor(0.72f, 0.50f, 0.44f), PanelX + 46.0f * Scale, PanelY + 140.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);

    static const TCHAR* Labels[] =
    {
        TEXT("RESTART MISSION"),
        TEXT("SELECT NEW MISSION")
    };
    static const TCHAR* Details[] =
    {
        TEXT("Redeploy the same aircraft and restart this sortie from base."),
        TEXT("Return to the Operations Board and choose another assignment.")
    };

    const float RowX = PanelX + 44.0f * Scale;
    const float RowY = PanelY + 200.0f * Scale;
    const float RowWidth = PanelWidth - 88.0f * Scale;
    const float RowHeight = 108.0f * Scale;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const bool bSelected = Index == OperationsController->GetSelectedMissionFailureAction();
        const float Y = RowY + Index * RowHeight;
        if (bSelected)
        {
            DrawRect(FLinearColor(0.16f, 0.055f, 0.035f, 1.0f), RowX, Y, RowWidth, 86.0f * Scale);
            DrawRect(Index == 0 ? Red : Amber, RowX, Y, 7.0f * Scale, 86.0f * Scale);
        }
        DrawText(Labels[Index], bSelected ? (Index == 0 ? Red : Amber) : FLinearColor(0.72f, 0.72f, 0.68f), RowX + 24.0f * Scale, Y + 12.0f * Scale, GEngine->GetSmallFont(), 1.0f * Scale);
        DrawText(Details[Index], FLinearColor(0.62f, 0.58f, 0.54f), RowX + 24.0f * Scale, Y + 49.0f * Scale, GEngine->GetSmallFont(), 0.76f * Scale);
    }

    DrawText(TEXT("UP / DOWN  SELECT     X / ENTER  CONFIRM"), White, PanelX + 46.0f * Scale, PanelY + 465.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);
}

void ARotorlineOperationsHUD::DrawFlightNavigation()
{
    const ARotorlineJeepPawn* Jeep = PlayerOwner ? Cast<ARotorlineJeepPawn>(PlayerOwner->GetPawn()) : nullptr;
    if (Canvas && Jeep)
    {
        const float Scale = FMath::Clamp(FMath::Min(Canvas->SizeX / 1920.0f, Canvas->SizeY / 900.0f), 0.65f, 1.15f);
        const float X = 22.0f * Scale;
        const float Y = Canvas->SizeY - 150.0f * Scale;
        DrawRect(FLinearColor(0.01f, 0.07f, 0.07f, 0.82f), X, Y, 390.0f * Scale, 126.0f * Scale);
        DrawRect(FLinearColor(0.25f, 0.92f, 0.92f), X, Y, 4.0f * Scale, 126.0f * Scale);
        DrawText(Jeep->GetVehicleName().ToUpper(), FLinearColor::White, X + 18.0f * Scale, Y + 12.0f * Scale, GEngine->GetSmallFont(), 0.95f * Scale);
        DrawText(FString::Printf(TEXT("SPEED  %03.0f KM/H"), Jeep->GetSpeedKmh()), FLinearColor(0.32f, 1.0f, 0.62f), X + 18.0f * Scale, Y + 48.0f * Scale, GEngine->GetSmallFont(), 0.9f * Scale);
        DrawText(TEXT("W/S DRIVE   A/D STEER   SPACE BRAKE   M MENU"), FLinearColor(0.68f, 0.82f, 0.82f), X + 18.0f * Scale, Y + 87.0f * Scale, GEngine->GetSmallFont(), 0.72f * Scale);
        return;
    }

    const ARotorlineHelicopterPawn* Helicopter = PlayerOwner ? Cast<ARotorlineHelicopterPawn>(PlayerOwner->GetPawn()) : nullptr;
    if (!Canvas || !Helicopter)
    {
        return;
    }

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(FMath::Min(Width / 1920.0f, Height / 900.0f), 0.58f, 1.18f);
    const float TextScale = FMath::Max(Scale, 1.0f);
    const bool bShortViewport = Height < 800.0f;
    const FLinearColor Amber(1.0f, 0.67f, 0.14f, 1.0f);
    const FLinearColor Cyan(0.25f, 0.92f, 0.92f, 1.0f);
    const FLinearColor Red(1.0f, 0.16f, 0.08f, 1.0f);
    const FLinearColor Green(0.30f, 1.0f, 0.54f, 1.0f);
    const FRotorlineCockpitHUDState Cockpit = Helicopter->GetCockpitHUDState();

    FVector ObjectiveWorld = FVector::ZeroVector;
    FString ObjectiveLabel;
    int32 ObjectiveIndex = 0;
    int32 ObjectiveCount = 0;
    const bool bHasObjective = Helicopter->GetMissionNavigationData(
        ObjectiveWorld,
        ObjectiveLabel,
        ObjectiveIndex,
        ObjectiveCount);
    FVector SensorTargetWorld = FVector::ZeroVector;
    FString SensorTargetLabel;
    const bool bHasKiowaSensorTarget = Helicopter->GetKiowaSensorTargetData(
        SensorTargetWorld,
        SensorTargetLabel);
    if (bHasKiowaSensorTarget)
    {
        ObjectiveWorld = SensorTargetWorld;
        ObjectiveLabel = SensorTargetLabel;
    }
    const bool bHostileObjective = ObjectiveLabel.Contains(TEXT("destroy"), ESearchCase::IgnoreCase);
    const FLinearColor ObjectiveColor = bHasKiowaSensorTarget ? Cyan : (bHostileObjective ? Red : Amber);

    // Compact mission card: readable mission context at the edge, never a
    // scrolling debug line across the flight path.
    const float EdgeMargin = 24.0f * Scale;
    const float MissionCardWidth = 430.0f * Scale;
    const float MissionCardHeight = 88.0f * Scale;
    const float MissionCardX = EdgeMargin;
    const float MissionCardY = 22.0f * Scale;
    DrawRect(FLinearColor(0.004f, 0.014f, 0.018f, 0.58f), MissionCardX, MissionCardY, MissionCardWidth, MissionCardHeight);
    DrawRect(ObjectiveColor, MissionCardX, MissionCardY, 4.0f * Scale, MissionCardHeight);
    DrawText(
        FString::Printf(TEXT("%s  //  OBJ %d/%d"), *Cockpit.MissionCallsign, ObjectiveIndex + 1, FMath::Max(ObjectiveCount, 1)),
        ObjectiveColor,
        MissionCardX + 15.0f * Scale,
        MissionCardY + 7.0f * Scale,
        GEngine->GetSmallFont(),
        0.82f * TextScale);
    DrawText(
        ObjectiveLabel.Left(bShortViewport ? 44 : 58).ToUpper(),
        FLinearColor(0.92f, 0.98f, 0.97f),
        MissionCardX + 15.0f * Scale,
        MissionCardY + 31.0f * Scale,
        GEngine->GetMediumFont(),
        0.86f * TextScale);
    if (bHasObjective)
    {
        const FVector ToObjective = ObjectiveWorld - Helicopter->GetActorLocation();
        const float ObjectiveBearing = FMath::RadiansToDegrees(FMath::Atan2(ToObjective.Y, ToObjective.X));
        const float RelativeBearing = FMath::FindDeltaAngleDegrees(Cockpit.HeadingDegrees, ObjectiveBearing);
        const TCHAR* TurnCue = FMath::Abs(RelativeBearing) <= 8.0f
            ? TEXT("AHEAD")
            : (RelativeBearing < 0.0f ? TEXT("LEFT") : TEXT("RIGHT"));
        DrawText(
            FString::Printf(TEXT("%.1f KM  //  %s %.0f"), ToObjective.Size2D() / 100000.0f, TurnCue, FMath::Abs(RelativeBearing)),
            FLinearColor(0.66f, 0.84f, 0.84f),
            MissionCardX + 15.0f * Scale,
            MissionCardY + 61.0f * Scale,
            GEngine->GetSmallFont(),
            0.78f * TextScale);
    }

    // Heading ribbon: the objective caret slides left/right with relative bearing.
    const float CompassWidth = 390.0f * Scale;
    const float CompassX = (Width - CompassWidth) * 0.5f;
    const float CompassY = 20.0f * Scale;
    const float CompassHeight = 38.0f * Scale;
    DrawRect(FLinearColor(0.008f, 0.018f, 0.021f, 0.56f), CompassX, CompassY, CompassWidth, CompassHeight);
    DrawLine(CompassX + CompassWidth * 0.5f, CompassY, CompassX + CompassWidth * 0.5f, CompassY + CompassHeight, Cyan, 2.0f * Scale);

    const float Heading = Cockpit.HeadingDegrees;
    static const TCHAR* CardinalNames[] = { TEXT("E"), TEXT("NE"), TEXT("N"), TEXT("NW"), TEXT("W"), TEXT("SW"), TEXT("S"), TEXT("SE") };
    const int32 CardinalIndex = FMath::RoundToInt(Heading / 45.0f) % 8;
    DrawText(
        FString::Printf(TEXT("%s  %03.0f"), CardinalNames[CardinalIndex], Heading),
        FLinearColor::White,
        CompassX + CompassWidth * 0.5f - 33.0f * Scale,
        CompassY + 5.0f * Scale,
        GEngine->GetMediumFont(),
        0.92f * TextScale);

    if (bHasObjective)
    {
        const FVector ToObjective = ObjectiveWorld - Helicopter->GetActorLocation();
        const float ObjectiveBearing = FMath::RadiansToDegrees(FMath::Atan2(ToObjective.Y, ToObjective.X));
        const float RelativeBearing = FMath::FindDeltaAngleDegrees(Heading, ObjectiveBearing);
        const float CaretX = CompassX + CompassWidth * 0.5f + FMath::Clamp(RelativeBearing / 90.0f, -1.0f, 1.0f) * CompassWidth * 0.42f;
        DrawLine(CaretX - 7.0f * Scale, CompassY + 32.0f * Scale, CaretX, CompassY + 24.0f * Scale, ObjectiveColor, 3.0f * Scale);
        DrawLine(CaretX, CompassY + 24.0f * Scale, CaretX + 7.0f * Scale, CompassY + 32.0f * Scale, ObjectiveColor, 3.0f * Scale);

        FVector2D ObjectiveScreen;
        if (PlayerOwner->ProjectWorldLocationToScreen(ObjectiveWorld + FVector(0.0f, 0.0f, 900.0f), ObjectiveScreen, true) &&
            ObjectiveScreen.X > 45.0f && ObjectiveScreen.X < Width - 45.0f && ObjectiveScreen.Y > 80.0f && ObjectiveScreen.Y < Height - 55.0f)
        {
            const float Bracket = 17.0f * Scale;
            DrawLine(ObjectiveScreen.X - Bracket, ObjectiveScreen.Y, ObjectiveScreen.X - 5.0f * Scale, ObjectiveScreen.Y, ObjectiveColor, 3.0f * Scale);
            DrawLine(ObjectiveScreen.X + 5.0f * Scale, ObjectiveScreen.Y, ObjectiveScreen.X + Bracket, ObjectiveScreen.Y, ObjectiveColor, 3.0f * Scale);
            DrawLine(ObjectiveScreen.X, ObjectiveScreen.Y - Bracket, ObjectiveScreen.X, ObjectiveScreen.Y - 5.0f * Scale, ObjectiveColor, 3.0f * Scale);
            DrawLine(ObjectiveScreen.X, ObjectiveScreen.Y + 5.0f * Scale, ObjectiveScreen.X, ObjectiveScreen.Y + Bracket, ObjectiveColor, 3.0f * Scale);
            const FString WorldCue = bHasKiowaSensorTarget
                ? FString::Printf(TEXT("SENSOR CONTACT  %.1f KM"), ToObjective.Size2D() / 100000.0f)
                : FString::Printf(TEXT("OBJ %d  %.1f KM"), ObjectiveIndex + 1, ToObjective.Size2D() / 100000.0f);
            DrawText(
                WorldCue,
                ObjectiveColor,
                ObjectiveScreen.X + 22.0f * Scale,
                ObjectiveScreen.Y - 10.0f * Scale,
                GEngine->GetSmallFont(),
                0.85f * Scale);
        }
    }

    // Resolve the Apache boresight once and reuse it for both target cue state
    // and the visible sight. This prevents competing aim calculations from
    // making the cue appear to hunt independently of the cannon.
    FVector WeaponMuzzle;
    FVector WeaponDirection;
    FVector WeaponImpact;
    bool bWeaponTraceBlocked = false;
    const bool bHasApacheWeaponSight = Helicopter->GetApacheWeaponAimSolution(
        WeaponMuzzle,
        WeaponDirection,
        WeaponImpact,
        bWeaponTraceBlocked);
    const bool bApacheMissileLockMode = bHasApacheWeaponSight && Helicopter->IsApacheMissileLockMode();

    const auto DrawCombatTargetBox = [&](const FVector& TargetWorld, const FString& TargetName,
                                          FVector2D& SmoothedScreen, bool& bHasSmoothedScreen)
    {
        FVector2D RawScreen;
        if (!PlayerOwner->ProjectWorldLocationToScreen(TargetWorld, RawScreen, true) ||
            RawScreen.X < 85.0f || RawScreen.X > Width - 85.0f || RawScreen.Y < 105.0f || RawScreen.Y > Height - 105.0f)
        {
            bHasSmoothedScreen = false;
            return;
        }

        if (!bHasSmoothedScreen || FVector2D::Distance(SmoothedScreen, RawScreen) > 280.0f * Scale)
        {
            SmoothedScreen = RawScreen;
            bHasSmoothedScreen = true;
        }
        else
        {
            SmoothedScreen = FMath::Vector2DInterpTo(
                SmoothedScreen,
                RawScreen,
                GetWorld()->GetDeltaSeconds(),
                12.0f);
        }
        const FVector2D Screen = SmoothedScreen;
        const FVector CueOrigin = bHasApacheWeaponSight ? WeaponMuzzle : Helicopter->GetActorLocation();
        const FVector ToTarget = (TargetWorld - CueOrigin).GetSafeNormal();
        const float BoresightDot = bHasApacheWeaponSight
            ? FVector::DotProduct(WeaponDirection, ToTarget)
            : -1.0f;
        const bool bCannonOnTarget = bHasApacheWeaponSight && BoresightDot >= 0.99756f; // four-degree cone
        const FLinearColor CueColor = bApacheMissileLockMode
            ? Red
            : (bCannonOnTarget ? FLinearColor(0.30f, 1.0f, 0.54f, 0.98f) : Red);
        const float Half = 58.0f * Scale;
        const float Corner = 22.0f * Scale;
        const float Thick = 4.0f * Scale;
        DrawLine(Screen.X - Half, Screen.Y - Half, Screen.X - Half + Corner, Screen.Y - Half, CueColor, Thick);
        DrawLine(Screen.X - Half, Screen.Y - Half, Screen.X - Half, Screen.Y - Half + Corner, CueColor, Thick);
        DrawLine(Screen.X + Half, Screen.Y - Half, Screen.X + Half - Corner, Screen.Y - Half, CueColor, Thick);
        DrawLine(Screen.X + Half, Screen.Y - Half, Screen.X + Half, Screen.Y - Half + Corner, CueColor, Thick);
        DrawLine(Screen.X - Half, Screen.Y + Half, Screen.X - Half + Corner, Screen.Y + Half, CueColor, Thick);
        DrawLine(Screen.X - Half, Screen.Y + Half, Screen.X - Half, Screen.Y + Half - Corner, CueColor, Thick);
        DrawLine(Screen.X + Half, Screen.Y + Half, Screen.X + Half - Corner, Screen.Y + Half, CueColor, Thick);
        DrawLine(Screen.X + Half, Screen.Y + Half, Screen.X + Half, Screen.Y + Half - Corner, CueColor, Thick);
        DrawText(TargetName.ToUpper(), FLinearColor::White, Screen.X - Half, Screen.Y + Half + 7.0f * Scale, GEngine->GetSmallFont(), 0.70f * TextScale);
        DrawText(FString::Printf(TEXT("%.0f M"), FVector::Dist(Helicopter->GetActorLocation(), TargetWorld) / 100.0f), CueColor, Screen.X + Half - 45.0f * Scale, Screen.Y + Half + 7.0f * Scale, GEngine->GetSmallFont(), 0.70f * TextScale);
    };

    const bool bShowCombatTargetBoxes = !bHasApacheWeaponSight || bApacheMissileLockMode;
    // Bell missile modes draw only their authoritative seeker lock in red.
    // Mission objectives remain available through navigation presentation and
    // must not masquerade as an out-of-range weapon lock.
    if (bShowCombatTargetBoxes && !Cockpit.bBellWeaponSystem && bHasObjective && bHostileObjective)
    {
        DrawCombatTargetBox(
            ObjectiveWorld,
            ObjectiveLabel,
            SmoothedObjectiveTargetScreen,
            bHasSmoothedObjectiveTargetScreen);
    }
    FVector ThreatWorld;
    FString ThreatLabel;
    // During an active Kiowa designation the cyan mission contact owns the
    // acquisition presentation. Hostile auto-selection remains available
    // outside that sensor window, but cannot steal the visible lock cue.
    const bool bHasTransitThreat = !bHasKiowaSensorTarget &&
        Helicopter->GetThreatNavigationData(ThreatWorld, ThreatLabel);
    if (bShowCombatTargetBoxes && bHasTransitThreat)
    {
        DrawCombatTargetBox(
            ThreatWorld,
            ThreatLabel,
            SmoothedThreatTargetScreen,
            bHasSmoothedThreatTargetScreen);
        const FVector ToThreat = ThreatWorld - Helicopter->GetActorLocation();
        const float ThreatBearing = FMath::RadiansToDegrees(FMath::Atan2(ToThreat.Y, ToThreat.X));
        const float RelativeThreatBearing = FMath::FindDeltaAngleDegrees(Heading, ThreatBearing);
        const FString ThreatDirection = FMath::Abs(RelativeThreatBearing) > 135.0f
            ? TEXT("BEHIND")
            : (RelativeThreatBearing > 18.0f ? TEXT("RIGHT") : (RelativeThreatBearing < -18.0f ? TEXT("LEFT") : TEXT("AHEAD")));
        const float AlertWidth = 330.0f * Scale;
        const float AlertX = (Width - AlertWidth) * 0.5f;
        const float AlertY = CompassY + CompassHeight + 8.0f * Scale;
        const float AlertHeight = 28.0f * Scale;
        DrawRect(FLinearColor(0.10f, 0.004f, 0.002f, 0.68f), AlertX, AlertY, AlertWidth, AlertHeight);
        DrawRect(Red, AlertX, AlertY, 4.0f * Scale, AlertHeight);
        DrawText(
            FString::Printf(TEXT("AIR THREAT  //  %s  //  %.1f KM"), *ThreatDirection, ToThreat.Size2D() / 100000.0f),
            FLinearColor::White,
            AlertX + 14.0f * Scale,
            AlertY + 3.0f * Scale,
            GEngine->GetMediumFont(),
            0.72f * TextScale);
    }
    FVector BellTargetWorld;
    FString BellTargetLabel;
    if (Cockpit.bBellWeaponSystem && Helicopter->GetBellWeaponTargetData(BellTargetWorld, BellTargetLabel))
    {
        DrawCombatTargetBox(
            BellTargetWorld,
            BellTargetLabel,
            SmoothedBellTargetScreen,
            bHasSmoothedBellTargetScreen);
    }
    else
    {
        bHasSmoothedBellTargetScreen = false;
    }

    // Attack-aircraft weapon sight. Project the exact impact used by the
    // cannon convergence calculation so third-person camera parallax cannot
    // separate the reticle from the rounds.
    if (bHasApacheWeaponSight && !bApacheMissileLockMode)
    {
        FVector2D SightScreen;
        const FVector SightDisplayWorld = WeaponImpact;
        if (PlayerOwner->ProjectWorldLocationToScreen(SightDisplayWorld, SightScreen, true) &&
            SightScreen.X > 42.0f && SightScreen.X < Width - 42.0f &&
            SightScreen.Y > 92.0f && SightScreen.Y < Height - 42.0f)
        {
            // The MH-6 streams settle just below the projected impact marker.
            // Keep this presentation-only trim isolated from every other airframe.
            if (Helicopter->GetSelectedAircraftId().Equals(TEXT("md500_defender"), ESearchCase::IgnoreCase))
            {
                SightScreen.Y += 4.0f * Scale;
            }

            if (!bApacheReticleAuditLogged)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("ROTORLINE_APACHE_RETICLE|state=VISIBLE|source=WEAPON_TRACE|axis=VISUAL_NOSE|blocked=%d|range_m=%.0f"),
                    bWeaponTraceBlocked ? 1 : 0,
                    FVector::Dist(WeaponMuzzle, WeaponImpact) / 100.0f);
                bApacheReticleAuditLogged = true;
            }
            const FLinearColor SightColor = bWeaponTraceBlocked
                ? FLinearColor(1.0f, 0.68f, 0.12f, 0.96f)
                : FLinearColor(0.30f, 1.0f, 0.54f, 0.92f);
            const float RingRadius = 27.0f * Scale;
            constexpr int32 RingSegments = 24;
            for (int32 Segment = 0; Segment < RingSegments; ++Segment)
            {
                const float AngleA = UE_TWO_PI * static_cast<float>(Segment) / static_cast<float>(RingSegments);
                const float AngleB = UE_TWO_PI * static_cast<float>(Segment + 1) / static_cast<float>(RingSegments);
                DrawLine(
                    SightScreen.X + FMath::Cos(AngleA) * RingRadius,
                    SightScreen.Y + FMath::Sin(AngleA) * RingRadius,
                    SightScreen.X + FMath::Cos(AngleB) * RingRadius,
                    SightScreen.Y + FMath::Sin(AngleB) * RingRadius,
                    SightColor,
                    2.0f * Scale);
            }

            const float Gap = 6.0f * Scale;
            const float TickEnd = 40.0f * Scale;
            DrawLine(SightScreen.X - TickEnd, SightScreen.Y, SightScreen.X - Gap, SightScreen.Y, SightColor, 2.5f * Scale);
            DrawLine(SightScreen.X + Gap, SightScreen.Y, SightScreen.X + TickEnd, SightScreen.Y, SightColor, 2.5f * Scale);
            DrawLine(SightScreen.X, SightScreen.Y - TickEnd, SightScreen.X, SightScreen.Y - Gap, SightColor, 2.5f * Scale);
            DrawLine(SightScreen.X, SightScreen.Y + Gap, SightScreen.X, SightScreen.Y + TickEnd, SightColor, 2.5f * Scale);
            DrawRect(SightColor, SightScreen.X - 2.0f * Scale, SightScreen.Y - 2.0f * Scale, 4.0f * Scale, 4.0f * Scale);

            const float SightRangeMeters = FVector::Distance(WeaponMuzzle, WeaponImpact) / 100.0f;
            DrawText(
                FString::Printf(TEXT("30MM // L1 // %.0f M%s"), SightRangeMeters, bWeaponTraceBlocked ? TEXT(" IMPACT") : TEXT("")),
                SightColor,
                SightScreen.X + 46.0f * Scale,
                SightScreen.Y + 18.0f * Scale,
                GEngine->GetSmallFont(),
                0.68f * TextScale);
        }
    }

    // Battlefield-inspired edge clusters: large digital values remain readable
    // while the aircraft, terrain, reticle, and target corridor stay clear.
    const float ClusterMargin = 22.0f * Scale;
    const float FlightClusterWidth = 372.0f * Scale;
    const float FlightClusterHeight = 190.0f * Scale;
    const float FlightClusterX = ClusterMargin;
    const float FlightClusterY = Height - FlightClusterHeight - ClusterMargin;
    DrawRect(FLinearColor(0.003f, 0.014f, 0.017f, 0.60f), FlightClusterX, FlightClusterY, FlightClusterWidth, FlightClusterHeight);
    DrawRect(Cockpit.bEngineReady ? Green : Amber, FlightClusterX, FlightClusterY, 4.0f * Scale, FlightClusterHeight);
    DrawText(Cockpit.AircraftName, FLinearColor(0.90f, 0.98f, 0.96f),
        FlightClusterX + 15.0f * Scale, FlightClusterY + 7.0f * Scale,
        GEngine->GetMediumFont(), 0.92f * TextScale);
    const FString AircraftState = Cockpit.bPlayerAircraftDying
        ? TEXT("AIRCRAFT LOST")
        : (Cockpit.bEngineReady
            ? TEXT("READY")
            : FString::Printf(TEXT("%s  %.0f%%  //  READY %.0fs"), *Cockpit.StartupPhase, Cockpit.RotorPercent, Cockpit.EngineReadySeconds));
    DrawText(AircraftState, Cockpit.bPlayerAircraftDying ? Red : (Cockpit.bEngineReady ? Green : Amber),
        FlightClusterX + 15.0f * Scale, FlightClusterY + 29.0f * Scale,
        GEngine->GetSmallFont(), 0.78f * TextScale);

    const float FlightCellGap = 6.0f * Scale;
    const float FlightCellWidth = (FlightClusterWidth - 36.0f * Scale - FlightCellGap) * 0.5f;
    const float FlightCellHeight = 51.0f * Scale;
    const float FlightCellX = FlightClusterX + 15.0f * Scale;
    const float FlightCellY = FlightClusterY + 54.0f * Scale;
    DrawDigitalGaugeCell(TEXT("SPD"), FString::Printf(TEXT("%03.0f"), Cockpit.SpeedKph), TEXT("KM/H"),
        FlightCellX, FlightCellY, FlightCellWidth, FlightCellHeight, Scale, TextScale, Cyan);
    DrawDigitalGaugeCell(TEXT("ALT"), FString::Printf(TEXT("%03.0f"), FMath::Max(0.0f, Cockpit.AltitudeAglMeters)), TEXT("M AGL"),
        FlightCellX + FlightCellWidth + FlightCellGap, FlightCellY, FlightCellWidth, FlightCellHeight, Scale, TextScale,
        Cockpit.AltitudeAglMeters < 12.0f ? Amber : Cyan, Cockpit.AltitudeAglMeters < 5.0f && Cockpit.SpeedKph > 30.0f);
    DrawDigitalGaugeCell(TEXT("HDG"), FString::Printf(TEXT("%03.0f"), Cockpit.HeadingDegrees), TEXT("DEG"),
        FlightCellX, FlightCellY + FlightCellHeight + FlightCellGap, FlightCellWidth, FlightCellHeight, Scale, TextScale, Cyan);
    DrawDigitalGaugeCell(TEXT("HULL"), FString::Printf(TEXT("%03.0f"), Cockpit.HullPercent), TEXT("PCT"),
        FlightCellX + FlightCellWidth + FlightCellGap, FlightCellY + FlightCellHeight + FlightCellGap,
        FlightCellWidth, FlightCellHeight, Scale, TextScale, Cockpit.HullPercent < 35.0f ? Red : Green, Cockpit.HullPercent < 35.0f);

    const FLinearColor FuelColor = Cockpit.FuelPercent <= 10.0f
        ? Red
        : (Cockpit.FuelPercent <= 25.0f ? Amber : Green);
    const float FuelBarX = FlightCellX;
    const float FuelBarY = FlightClusterY + 168.0f * Scale;
    const float FuelBarWidth = FlightClusterWidth - 30.0f * Scale;
    DrawText(TEXT("FUEL"), FuelColor, FuelBarX, FuelBarY - 2.0f * Scale,
        GEngine->GetSmallFont(), 0.72f * TextScale);
    DrawRect(FLinearColor(0.02f, 0.05f, 0.055f, 0.90f), FuelBarX + 52.0f * Scale, FuelBarY + 4.0f * Scale,
        FuelBarWidth - 103.0f * Scale, 8.0f * Scale);
    DrawRect(FuelColor, FuelBarX + 52.0f * Scale, FuelBarY + 4.0f * Scale,
        (FuelBarWidth - 103.0f * Scale) * FMath::Clamp(Cockpit.FuelPercent / 100.0f, 0.0f, 1.0f), 8.0f * Scale);
    DrawText(FString::Printf(TEXT("%03.0f%%"), Cockpit.FuelPercent), FuelColor,
        FuelBarX + FuelBarWidth - 43.0f * Scale, FuelBarY - 2.0f * Scale,
        GEngine->GetSmallFont(), 0.72f * TextScale);

    if (Cockpit.bEngineReady && Cockpit.AltitudeAglMeters <= 18.0f)
    {
        const bool bInsideSkidEnvelope =
            Cockpit.SpeedKph <= Cockpit.SafeSkidSpeedKph &&
            Cockpit.DescentRateMps <= Cockpit.SafeDescentRateMps;
        const FLinearColor LandingColor = bInsideSkidEnvelope ? Green : Amber;
        const float LandingCueY = FlightClusterY - 27.0f * Scale;
        DrawRect(FLinearColor(0.003f, 0.014f, 0.017f, 0.72f),
            FlightClusterX, LandingCueY, FlightClusterWidth, 23.0f * Scale);
        DrawRect(LandingColor, FlightClusterX, LandingCueY, 4.0f * Scale, 23.0f * Scale);
        DrawText(
            bInsideSkidEnvelope
                ? FString::Printf(TEXT("SKID ENVELOPE SAFE // <= %.0f KM/H // DESCENT <= %.1f M/S"),
                    Cockpit.SafeSkidSpeedKph, Cockpit.SafeDescentRateMps)
                : FString::Printf(TEXT("LANDING FAST // %.0f KM/H // DESCENT %.1f M/S"),
                    Cockpit.SpeedKph, Cockpit.DescentRateMps),
            LandingColor,
            FlightClusterX + 12.0f * Scale,
            LandingCueY + 3.0f * Scale,
            GEngine->GetSmallFont(),
            0.70f * TextScale);
    }

    if (Cockpit.bArmed && bHasApacheWeaponSight)
    {
        const float WeaponClusterWidth = 404.0f * Scale;
        const float WeaponClusterHeight = 150.0f * Scale;
        const float WeaponClusterX = Width - WeaponClusterWidth - ClusterMargin;
        const float WeaponClusterY = Height - WeaponClusterHeight - ClusterMargin;
        const FLinearColor ModeColor = Cockpit.bBellWeaponSystem
            ? (Cockpit.WeaponSystemState == TEXT("READY") ? Green : Amber)
            : (bApacheMissileLockMode ? Red : Green);
        DrawRect(FLinearColor(0.003f, 0.014f, 0.017f, 0.60f), WeaponClusterX, WeaponClusterY, WeaponClusterWidth, WeaponClusterHeight);
        DrawRect(ModeColor, WeaponClusterX + WeaponClusterWidth - 4.0f * Scale, WeaponClusterY, 4.0f * Scale, WeaponClusterHeight);
        DrawText(Cockpit.bBellWeaponSystem
                ? Cockpit.SelectedWeapon
                : (bApacheMissileLockMode ? TEXT("MISSILE LOCK") : TEXT("30MM CANNON")), ModeColor,
            WeaponClusterX + 15.0f * Scale, WeaponClusterY + 7.0f * Scale,
            GEngine->GetMediumFont(), 0.92f * TextScale);
        const FString OpticState = Cockpit.bCombatZoom
            ? TEXT("OPTIC 2.4X // PRECISION 35% // R3: EXIT")
            : TEXT("R3: 2.4X OPTIC");
        const FString CompactOpticState = bShortViewport
            ? (Cockpit.bCombatZoom ? TEXT("OPTIC 2.4X") : TEXT("OPTIC 1.0X"))
            : OpticState;
        DrawText(CompactOpticState, FLinearColor(0.72f, 0.86f, 0.86f),
            WeaponClusterX + WeaponClusterWidth - (bShortViewport ? 112.0f : 205.0f) * Scale, WeaponClusterY + 9.0f * Scale,
            GEngine->GetSmallFont(), 0.74f * TextScale);

        const float WeaponCellGap = 5.0f * Scale;
        const float WeaponCellWidth = (WeaponClusterWidth - 40.0f * Scale - WeaponCellGap * 2.0f) / 3.0f;
        const float WeaponCellY = WeaponClusterY + 40.0f * Scale;
        const float WeaponCellHeight = 56.0f * Scale;
        DrawDigitalGaugeCell(Cockpit.bBellWeaponSystem ? TEXT("AMMO") : TEXT("RKT"),
            FString::FromInt(Cockpit.bBellWeaponSystem ? Cockpit.SelectedWeaponAmmo : Cockpit.RocketAmmo), TEXT("R1"),
            WeaponClusterX + 15.0f * Scale, WeaponCellY, WeaponCellWidth, WeaponCellHeight, Scale, TextScale,
            (Cockpit.bBellWeaponSystem ? Cockpit.SelectedWeaponAmmo : Cockpit.RocketAmmo) <= 2 ? Amber : Green,
            (Cockpit.bBellWeaponSystem ? Cockpit.SelectedWeaponAmmo : Cockpit.RocketAmmo) <= 0);
        DrawDigitalGaugeCell(Cockpit.bBellWeaponSystem ? TEXT("SYSTEM") : TEXT("30MM"),
            Cockpit.bBellWeaponSystem ? Cockpit.WeaponSystemState : FString::FromInt(Cockpit.CannonAmmo),
            Cockpit.bBellWeaponSystem ? Cockpit.WeaponLockState : TEXT("L1"),
            WeaponClusterX + 15.0f * Scale + WeaponCellWidth + WeaponCellGap, WeaponCellY,
            WeaponCellWidth, WeaponCellHeight, Scale, TextScale,
            Cockpit.bBellWeaponSystem && Cockpit.WeaponSystemState != TEXT("READY") ? Amber :
                (Cockpit.bCannonOverheated ? Red : Green), Cockpit.bCannonOverheated);
        DrawDigitalGaugeCell(TEXT("CM"), FString::FromInt(Cockpit.Countermeasures),
            Cockpit.CountermeasureCooldown > 0.0f ? FString::Printf(TEXT("%.1f S"), Cockpit.CountermeasureCooldown) : TEXT("L3"),
            WeaponClusterX + 15.0f * Scale + (WeaponCellWidth + WeaponCellGap) * 2.0f, WeaponCellY,
            WeaponCellWidth, WeaponCellHeight, Scale, TextScale,
            Cockpit.Countermeasures <= 1 ? Amber : Cyan, Cockpit.Countermeasures <= 0);

        const float HeatBarX = WeaponClusterX + 15.0f * Scale;
        const float HeatBarY = WeaponClusterY + 106.0f * Scale;
        const float HeatBarWidth = WeaponClusterWidth - 34.0f * Scale;
        DrawText(Cockpit.bBellWeaponSystem
                ? (bApacheMissileLockMode ? *FString::Printf(TEXT("LOCK // %s"), *Cockpit.WeaponTarget.Left(22)) : TEXT("LINKED HEAT"))
                : TEXT("30MM HEAT"), FLinearColor(0.66f, 0.82f, 0.82f), HeatBarX, HeatBarY - 2.0f * Scale,
            GEngine->GetSmallFont(), 0.72f * TextScale);
        DrawRect(FLinearColor(0.02f, 0.05f, 0.055f, 0.86f), HeatBarX + 86.0f * Scale, HeatBarY + 5.0f * Scale, HeatBarWidth - 86.0f * Scale, 7.0f * Scale);
        const float StatusPercent = Cockpit.bBellWeaponSystem && bApacheMissileLockMode
            ? Cockpit.WeaponLockProgress * 100.0f : Cockpit.CannonHeatPercent;
        DrawRect(StatusPercent > 80.0f ? (bApacheMissileLockMode ? Green : Red) : Amber,
            HeatBarX + 86.0f * Scale, HeatBarY + 5.0f * Scale,
            (HeatBarWidth - 86.0f * Scale) * StatusPercent / 100.0f, 7.0f * Scale);
        const FString CloakHelp = Cockpit.bStealthActive
            ? FString::Printf(TEXT("B6 CLOAK ACTIVE %.0fS"), Cockpit.StealthSecondsRemaining)
            : Cockpit.StealthCooldownSecondsRemaining > 0.0f
                ? FString::Printf(TEXT("B6 COOLDOWN %.0fS"), Cockpit.StealthCooldownSecondsRemaining)
                : TEXT("B6 CLOAK READY");
        const FString ModeHelp = Cockpit.bBellWeaponSystem
            ? FString::Printf(TEXT("B3 NEXT / B4 PREV // B1 FIRE // B2 CM // %s"), *CloakHelp)
            : TEXT("B3 NEXT / B4 PREV   //   B1 FIRE   //   B2 CM");
        DrawText(ModeHelp, FLinearColor(0.68f, 0.82f, 0.82f),
            WeaponClusterX + 15.0f * Scale, WeaponClusterY + 130.0f * Scale,
            GEngine->GetSmallFont(), 0.72f * TextScale);
    }
    else if (!Cockpit.bArmed)
    {
        if (Cockpit.bReconStrikeSensor)
        {
            const float SensorWidth = 360.0f * Scale;
            const float SensorHeight = 126.0f * Scale;
            const float SensorX = Width - SensorWidth - ClusterMargin;
            const float SensorY = Height - SensorHeight - ClusterMargin;
            const float Progress = FMath::Clamp(Cockpit.ReconDesignationProgress, 0.0f, 1.0f);
            const FLinearColor SensorColor = Progress >= 0.999f ? Green : Cyan;

            DrawRect(FLinearColor(0.003f, 0.014f, 0.017f, 0.70f), SensorX, SensorY, SensorWidth, SensorHeight);
            DrawRect(SensorColor, SensorX + SensorWidth - 4.0f * Scale, SensorY, 4.0f * Scale, SensorHeight);
            DrawText(TEXT("KIOWA MAST SENSOR"), SensorColor,
                SensorX + 15.0f * Scale, SensorY + 8.0f * Scale,
                GEngine->GetMediumFont(), 0.82f * TextScale);
            DrawText(Cockpit.ReconSensorStatus, FLinearColor(0.82f, 0.92f, 0.92f),
                SensorX + 15.0f * Scale, SensorY + 38.0f * Scale,
                GEngine->GetSmallFont(), 0.76f * TextScale);
            DrawRect(FLinearColor(0.12f, 0.20f, 0.20f, 0.92f),
                SensorX + 15.0f * Scale, SensorY + 64.0f * Scale,
                SensorWidth - 34.0f * Scale, 8.0f * Scale);
            DrawRect(SensorColor,
                SensorX + 15.0f * Scale, SensorY + 64.0f * Scale,
                (SensorWidth - 34.0f * Scale) * Progress, 8.0f * Scale);
            DrawText(Cockpit.AlliedStrikeStatus, Amber,
                SensorX + 15.0f * Scale, SensorY + 80.0f * Scale,
                GEngine->GetSmallFont(), 0.76f * TextScale);
            DrawText(TEXT("B1 / X / T // ACQUIRE AND HOLD TARGET"), FLinearColor(0.60f, 0.78f, 0.78f),
                SensorX + 15.0f * Scale, SensorY + 103.0f * Scale,
                GEngine->GetSmallFont(), 0.66f * TextScale);
        }
        else
        {
        // Support aircraft still carry defensive countermeasures even though
        // they do not have a weapon cluster. Keep the remaining bursts visible
        // at all times so the pilot can manage them during an escort or rescue.
        const float DefenseClusterWidth = 250.0f * Scale;
        const float DefenseClusterHeight = 92.0f * Scale;
        const float DefenseClusterX = Width - DefenseClusterWidth - ClusterMargin;
        const float DefenseClusterY = Height - DefenseClusterHeight - ClusterMargin;
        const FLinearColor CountermeasureColor = Cockpit.Countermeasures <= 1 ? Amber : Cyan;
        const bool bCountermeasuresEmpty = Cockpit.Countermeasures <= 0;

        DrawRect(FLinearColor(0.003f, 0.014f, 0.017f, 0.60f),
            DefenseClusterX, DefenseClusterY, DefenseClusterWidth, DefenseClusterHeight);
        DrawRect(CountermeasureColor,
            DefenseClusterX + DefenseClusterWidth - 4.0f * Scale,
            DefenseClusterY,
            4.0f * Scale,
            DefenseClusterHeight);
        DrawText(TEXT("DEFENSIVE SYSTEMS"), CountermeasureColor,
            DefenseClusterX + 15.0f * Scale,
            DefenseClusterY + 7.0f * Scale,
            GEngine->GetMediumFont(),
            0.82f * TextScale);

        DrawDigitalGaugeCell(
            TEXT("CM BURSTS"),
            FString::Printf(TEXT("%d / %d"), Cockpit.Countermeasures, Cockpit.CountermeasureCapacity),
            Cockpit.CountermeasureCooldown > 0.0f
                ? FString::Printf(TEXT("B2 // READY %.1f S"), Cockpit.CountermeasureCooldown)
                : TEXT("B2 // READY"),
            DefenseClusterX + 15.0f * Scale,
            DefenseClusterY + 35.0f * Scale,
            DefenseClusterWidth - 34.0f * Scale,
            48.0f * Scale,
            Scale,
            TextScale,
            CountermeasureColor,
            bCountermeasuresEmpty);
        }
    }

    FString BaseRearmStatus;
    bool bBaseRearmServicing = false;
    if (Helicopter->GetBaseRearmStatus(BaseRearmStatus, bBaseRearmServicing))
    {
        const float ServiceWidth = 390.0f * Scale;
        const float ServiceX = MissionCardX;
        const float ServiceY = MissionCardY + MissionCardHeight + 9.0f * Scale;
        const FLinearColor ServiceColor = bBaseRearmServicing
            ? FLinearColor(0.30f, 1.0f, 0.54f, 1.0f)
            : Amber;
        DrawRect(FLinearColor(0.008f, 0.018f, 0.014f, 0.62f), ServiceX, ServiceY, ServiceWidth, 34.0f * Scale);
        DrawRect(ServiceColor, ServiceX, ServiceY, 4.0f * Scale, 34.0f * Scale);
        DrawText(
            BaseRearmStatus,
            ServiceColor,
            ServiceX + 17.0f * Scale,
            ServiceY + 5.0f * Scale,
            GEngine->GetMediumFont(),
            0.74f * TextScale);
    }

    FString RadioMessage;
    if (Helicopter->GetRadioChatter(RadioMessage))
    {
        FString RadioSpeaker = TEXT("RADIO");
        FString RadioBody = RadioMessage;
        int32 SpeakerSeparator = INDEX_NONE;
        if (RadioMessage.FindChar(TEXT(':'), SpeakerSeparator) && SpeakerSeparator > 0 && SpeakerSeparator < 18)
        {
            RadioSpeaker = RadioMessage.Left(SpeakerSeparator).ToUpper();
            RadioBody = RadioMessage.Mid(SpeakerSeparator + 1).TrimStart();
        }
        const bool bUrgentRadio = RadioBody.Contains(TEXT("MISSILE"), ESearchCase::IgnoreCase) ||
            RadioBody.Contains(TEXT("INCOMING"), ESearchCase::IgnoreCase) ||
            RadioBody.Contains(TEXT("RADAR"), ESearchCase::IgnoreCase) ||
            RadioBody.Contains(TEXT("BREAK"), ESearchCase::IgnoreCase) ||
            RadioBody.Contains(TEXT("FIRING"), ESearchCase::IgnoreCase);
        const float RadioFade = Helicopter->GetRadioChatterFadeAlpha();
        const FLinearColor RadioAccent = bUrgentRadio
            ? FLinearColor(Red.R, Red.G, Red.B, RadioFade)
            : FLinearColor(Cyan.R, Cyan.G, Cyan.B, RadioFade);
        const float RadioCardWidth = FMath::Min(520.0f * Scale, Width * 0.34f);
        const float RadioCardHeight = 82.0f * Scale;
        const float RadioCardX = FlightClusterX;
        const float RadioCardY = FlightClusterY - RadioCardHeight - 11.0f * Scale;
        DrawRect(FLinearColor(0.003f, 0.014f, 0.017f, 0.62f * RadioFade), RadioCardX, RadioCardY, RadioCardWidth, RadioCardHeight);
        DrawRect(RadioAccent, RadioCardX, RadioCardY, 4.0f * Scale, RadioCardHeight);
        DrawText(RadioSpeaker, RadioAccent, RadioCardX + 15.0f * Scale, RadioCardY + 7.0f * Scale,
            GEngine->GetSmallFont(), 0.78f * TextScale);
        DrawWrappedText(RadioBody, RadioCardX + 15.0f * Scale, RadioCardY + 29.0f * Scale,
            RadioCardWidth - 28.0f * Scale, 0.88f * TextScale,
            FLinearColor(0.90f, 0.98f, 0.97f, RadioFade), 2);
    }

    // Tactical island display. Keep it world-locked, but present it as an
    // aviation instrument rather than a debug square full of actor names.
    const ARotorlineOperationsPlayerController* OperationsController =
        Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!OperationsController || OperationsController->IsTacticalMapVisible())
    {
    const float MapSize = FMath::Min(232.0f * Scale, Height * 0.37f);
    const float MapX = Width - MapSize - 28.0f * Scale;
    const float MapY = 88.0f * Scale;
    const float HeaderHeight = 25.0f * Scale;
    const float FooterHeight = 25.0f * Scale;
    const float InnerInset = 8.0f * Scale;
    const float InnerLeft = MapX + InnerInset;
    const float InnerRight = MapX + MapSize - InnerInset;
    const float InnerTop = MapY + HeaderHeight;
    const float InnerBottom = MapY + MapSize - FooterHeight;
    const float InnerWidth = InnerRight - InnerLeft;
    const float InnerHeight = InnerBottom - InnerTop;
    const float MapCenterX = (InnerLeft + InnerRight) * 0.5f;
    const float MapCenterY = (InnerTop + InnerBottom) * 0.5f;

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.30f), MapX - 4.0f * Scale, MapY + 4.0f * Scale, MapSize + 8.0f * Scale, MapSize + 4.0f * Scale);
    DrawRect(FLinearColor(0.004f, 0.018f, 0.022f, 0.91f), MapX, MapY, MapSize, MapSize);
    DrawRect(FLinearColor(0.015f, 0.070f, 0.078f, 0.84f), MapX, MapY, MapSize, HeaderHeight);
    DrawRect(FLinearColor(0.008f, 0.035f, 0.040f, 0.88f), MapX, InnerBottom, MapSize, FooterHeight);

    const FLinearColor FrameColor(0.24f, 0.88f, 0.88f, 0.92f);
    const FLinearColor GridColor(0.11f, 0.28f, 0.30f, 0.54f);
    const FLinearColor ContourColor(0.20f, 0.42f, 0.36f, 0.44f);
    DrawLine(MapX, MapY, MapX + MapSize, MapY, FrameColor, 1.6f * Scale);
    DrawLine(MapX + MapSize, MapY, MapX + MapSize, MapY + MapSize, FrameColor, 1.6f * Scale);
    DrawLine(MapX + MapSize, MapY + MapSize, MapX, MapY + MapSize, FrameColor, 1.6f * Scale);
    DrawLine(MapX, MapY + MapSize, MapX, MapY, FrameColor, 1.6f * Scale);

    // Fine grid and center axes give useful spatial reference without competing
    // with the navigation symbols.
    for (int32 GridIndex = 1; GridIndex < 4; ++GridIndex)
    {
        const float GridAlpha = static_cast<float>(GridIndex) / 4.0f;
        const float GridX = FMath::Lerp(InnerLeft, InnerRight, GridAlpha);
        const float GridY = FMath::Lerp(InnerTop, InnerBottom, GridAlpha);
        const bool bMajorGrid = GridIndex == 2;
        DrawLine(GridX, InnerTop, GridX, InnerBottom, GridColor, (bMajorGrid ? 1.15f : 0.65f) * Scale);
        DrawLine(InnerLeft, GridY, InnerRight, GridY, GridColor, (bMajorGrid ? 1.15f : 0.65f) * Scale);
    }

    DrawText(TEXT("TAC // ISLAND"), FrameColor, MapX + 9.0f * Scale, MapY + 6.0f * Scale,
        GEngine->GetSmallFont(), 0.66f * TextScale);
    DrawText(TEXT("N"), FLinearColor(0.92f, 0.98f, 0.97f), MapCenterX - 3.0f * Scale, MapY + 5.0f * Scale,
        GEngine->GetSmallFont(), 0.68f * TextScale);
    DrawLine(MapCenterX, MapY + 2.0f * Scale, MapCenterX, MapY + 7.0f * Scale, FrameColor, 1.4f * Scale);

    const auto NormalizedToMap = [&](const FVector2D& Normalized) -> FVector2D
    {
        return FVector2D(
            MapCenterX + Normalized.X * InnerWidth * 0.47f,
            MapCenterY - Normalized.Y * InnerHeight * 0.47f);
    };

    // A restrained island/shoreline contour makes the display read as a map
    // while remaining deliberately subordinate to live navigation data.
    const TArray<FVector2D> IslandOutline = {
        FVector2D(-0.93f, 0.15f), FVector2D(-0.82f, 0.52f), FVector2D(-0.55f, 0.79f),
        FVector2D(-0.16f, 0.91f), FVector2D(0.18f, 0.82f), FVector2D(0.46f, 0.66f),
        FVector2D(0.77f, 0.69f), FVector2D(0.94f, 0.36f), FVector2D(0.88f, 0.02f),
        FVector2D(0.97f, -0.28f), FVector2D(0.73f, -0.61f), FVector2D(0.38f, -0.82f),
        FVector2D(0.02f, -0.91f), FVector2D(-0.31f, -0.78f), FVector2D(-0.67f, -0.70f),
        FVector2D(-0.87f, -0.40f)
    };
    for (int32 PointIndex = 0; PointIndex < IslandOutline.Num(); ++PointIndex)
    {
        const FVector2D PointA = NormalizedToMap(IslandOutline[PointIndex]);
        const FVector2D PointB = NormalizedToMap(IslandOutline[(PointIndex + 1) % IslandOutline.Num()]);
        DrawLine(PointA.X, PointA.Y, PointB.X, PointB.Y, ContourColor, 1.0f * Scale);
    }

    const TArray<FVector2D> RidgeContour = {
        FVector2D(-0.69f, 0.17f), FVector2D(-0.44f, 0.44f), FVector2D(-0.08f, 0.49f),
        FVector2D(0.26f, 0.34f), FVector2D(0.53f, 0.13f), FVector2D(0.46f, -0.19f),
        FVector2D(0.16f, -0.42f), FVector2D(-0.19f, -0.37f), FVector2D(-0.52f, -0.18f)
    };
    for (int32 PointIndex = 0; PointIndex < RidgeContour.Num() - 1; ++PointIndex)
    {
        const FVector2D PointA = NormalizedToMap(RidgeContour[PointIndex]);
        const FVector2D PointB = NormalizedToMap(RidgeContour[PointIndex + 1]);
        DrawLine(PointA.X, PointA.Y, PointB.X, PointB.Y, FLinearColor(ContourColor.R, ContourColor.G, ContourColor.B, 0.32f), 0.75f * Scale);
    }

    const auto WorldToMap = [&](const FVector& World) -> FVector2D
    {
        constexpr float IslandHalfWorld = 403200.0f;
        return FVector2D(
            MapCenterX + FMath::Clamp(World.X / IslandHalfWorld, -1.0f, 1.0f) * InnerWidth * 0.47f,
            MapCenterY - FMath::Clamp(World.Y / IslandHalfWorld, -1.0f, 1.0f) * InnerHeight * 0.47f);
    };
    const auto DrawDiamond = [&](const FVector2D& Point, const FLinearColor& Color, float Radius, bool bCenterDot)
    {
        DrawLine(Point.X, Point.Y - Radius, Point.X + Radius, Point.Y, Color, 1.5f * Scale);
        DrawLine(Point.X + Radius, Point.Y, Point.X, Point.Y + Radius, Color, 1.5f * Scale);
        DrawLine(Point.X, Point.Y + Radius, Point.X - Radius, Point.Y, Color, 1.5f * Scale);
        DrawLine(Point.X - Radius, Point.Y, Point.X, Point.Y - Radius, Color, 1.5f * Scale);
        if (bCenterDot)
        {
            DrawRect(Color, Point.X - 1.5f * Scale, Point.Y - 1.5f * Scale, 3.0f * Scale, 3.0f * Scale);
        }
    };
    const auto DrawLandmark = [&](const FVector& World, const FLinearColor& Color, const FString& Label)
    {
        const FVector2D Point = WorldToMap(World);
        DrawDiamond(Point, Color, 4.0f * Scale, true);
        DrawText(Label, Color, Point.X + 7.0f * Scale, Point.Y - 7.0f * Scale,
            GEngine->GetSmallFont(), 0.56f * TextScale);
    };
    const auto DrawThreatIcon = [&](const FVector& World)
    {
        const FVector2D Point = WorldToMap(World);
        const float Pulse = 1.0f + 0.16f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.0f);
        const float Radius = 5.0f * Scale * Pulse;
        DrawDiamond(Point, Red, Radius, true);
        const float TickInner = 7.0f * Scale;
        const float TickOuter = 10.0f * Scale;
        DrawLine(Point.X - TickOuter, Point.Y, Point.X - TickInner, Point.Y, Red, 1.2f * Scale);
        DrawLine(Point.X + TickInner, Point.Y, Point.X + TickOuter, Point.Y, Red, 1.2f * Scale);
        DrawLine(Point.X, Point.Y - TickOuter, Point.X, Point.Y - TickInner, Red, 1.2f * Scale);
        DrawLine(Point.X, Point.Y + TickInner, Point.X, Point.Y + TickOuter, Red, 1.2f * Scale);
    };

    const FVector HomePad(-236194.1f, -193027.5f, 0.0f);
    const FVector RidgeWarehouse(192243.6f, 131234.1f, 0.0f);
    DrawLandmark(HomePad, FLinearColor(0.35f, 0.95f, 0.55f), TEXT("BASE"));
    DrawLandmark(RotorlineSupportLocations::CentralTownRearmPad,
        FLinearColor(0.20f, 1.0f, 0.48f), TEXT("REARM"));
    DrawLandmark(RidgeWarehouse, FLinearColor(0.92f, 0.76f, 0.38f), TEXT("WAREHOUSE"));

    const FVector2D PlayerPoint = WorldToMap(Helicopter->GetActorLocation());
    const float HeadingRadians = FMath::DegreesToRadians(Heading);
    const FVector2D PlayerForward(FMath::Cos(HeadingRadians), -FMath::Sin(HeadingRadians));
    const FVector2D PlayerRight(-PlayerForward.Y, PlayerForward.X);
    const FVector2D PlayerTip = PlayerPoint + PlayerForward * 11.0f * Scale;
    const FVector2D PlayerLeft = PlayerPoint - PlayerForward * 5.0f * Scale - PlayerRight * 6.0f * Scale;
    const FVector2D PlayerRightPoint = PlayerPoint - PlayerForward * 5.0f * Scale + PlayerRight * 6.0f * Scale;
    DrawLine(PlayerTip.X, PlayerTip.Y, PlayerLeft.X, PlayerLeft.Y, Cyan, 2.0f * Scale);
    DrawLine(PlayerLeft.X, PlayerLeft.Y, PlayerPoint.X, PlayerPoint.Y, Cyan, 2.0f * Scale);
    DrawLine(PlayerPoint.X, PlayerPoint.Y, PlayerRightPoint.X, PlayerRightPoint.Y, Cyan, 2.0f * Scale);
    DrawLine(PlayerRightPoint.X, PlayerRightPoint.Y, PlayerTip.X, PlayerTip.Y, Cyan, 2.0f * Scale);
    DrawRect(Cyan, PlayerPoint.X - 1.5f * Scale, PlayerPoint.Y - 1.5f * Scale, 3.0f * Scale, 3.0f * Scale);

    if (bHasObjective)
    {
        const FVector2D ObjectivePoint = WorldToMap(ObjectiveWorld);
        constexpr int32 RouteSegments = 12;
        for (int32 RouteIndex = 0; RouteIndex < RouteSegments; RouteIndex += 2)
        {
            const FVector2D RouteStart = FMath::Lerp(PlayerPoint, ObjectivePoint, static_cast<float>(RouteIndex) / RouteSegments);
            const FVector2D RouteEnd = FMath::Lerp(PlayerPoint, ObjectivePoint, static_cast<float>(RouteIndex + 1) / RouteSegments);
            DrawLine(RouteStart.X, RouteStart.Y, RouteEnd.X, RouteEnd.Y, ObjectiveColor, 1.1f * Scale);
        }
        if (bHostileObjective)
        {
            DrawThreatIcon(ObjectiveWorld);
        }
        else
        {
            DrawDiamond(ObjectivePoint, ObjectiveColor, 5.0f * Scale, true);
        }
        DrawText(
            FString::Printf(TEXT("OBJ %d/%d  %.1f KM"), ObjectiveIndex + 1, ObjectiveCount, FVector::Dist2D(Helicopter->GetActorLocation(), ObjectiveWorld) / 100000.0f),
            ObjectiveColor,
            MapX + 9.0f * Scale,
            InnerBottom + 6.0f * Scale,
            GEngine->GetSmallFont(),
            0.62f * TextScale);
    }
    else
    {
        DrawText(TEXT("NAV // STANDBY"), FLinearColor(0.50f, 0.67f, 0.67f),
            MapX + 9.0f * Scale, InnerBottom + 6.0f * Scale,
            GEngine->GetSmallFont(), 0.60f * TextScale);
    }
    if (bHasTransitThreat && (!bHasObjective || FVector::Dist2D(ThreatWorld, ObjectiveWorld) > 25000.0f))
    {
        // Hostiles are intentionally icon-only. Their class belongs in the
        // targeting system, not as clutter inside the tactical display.
        DrawThreatIcon(ThreatWorld);
    }
    }
}

void ARotorlineOperationsHUD::DrawAircraftHangar()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController)
    {
        return;
    }

    const FRotorlineAircraftDefinition* Aircraft = OperationsController->GetSelectedAircraft();
    const TArray<FRotorlineAircraftDefinition>& Fleet = OperationsController->GetAircraft();
    const TArray<FRotorlineMissionDefinition>& Missions = OperationsController->GetMissions();
    if (!Aircraft || !Missions.IsValidIndex(OperationsController->GetSelectedMissionIndex()))
    {
        return;
    }

    const FRotorlineMissionDefinition& Mission = Missions[OperationsController->GetSelectedMissionIndex()];
    const bool bAircraftUnlocked = OperationsController->IsAircraftUnlocked(*Aircraft);
    const bool bGroundVehicle = Aircraft->Id.StartsWith(TEXT("jeep_"), ESearchCase::IgnoreCase) ||
        Aircraft->Role.Contains(TEXT("ground"), ESearchCase::IgnoreCase);
    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.72f, 1.35f);
    const FLinearColor White(0.95f, 0.97f, 0.92f);
    const FLinearColor Cyan(0.36f, 0.88f, 0.88f);
    const FLinearColor Amber(0.96f, 0.58f, 0.12f);
    const FLinearColor Green(0.35f, 0.95f, 0.55f);

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.20f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.006f, 0.014f, 0.018f, 0.90f), 0.0f, 0.0f, Width, 104.0f * Scale);
    DrawRect(Amber, 0.0f, 0.0f, Width, 5.0f * Scale);
    DrawText(TEXT("ROTORLINE // FLEET HANGAR"), White, 48.0f * Scale, 30.0f * Scale, GEngine->GetLargeFont(), 1.30f * Scale);
    DrawText(TEXT("VEHICLE SELECTION  //  TECHNICAL EVALUATION BAY"), Cyan,
        49.0f * Scale, 76.0f * Scale, GEngine->GetSmallFont(), 0.90f * Scale);
    DrawText(
        FString::Printf(TEXT("%02d / %02d"), OperationsController->GetSelectedAircraftIndex() + 1, Fleet.Num()),
        Cyan,
        Width - 150.0f * Scale,
        43.0f * Scale,
        GEngine->GetSmallFont(),
        1.05f * Scale);

    const float PanelX = 42.0f * Scale;
    const float PanelY = 132.0f * Scale;
    const float PanelWidth = 420.0f * Scale;
    const float PanelHeight = FMath::Min(650.0f * Scale, Height - PanelY - 102.0f * Scale);
    DrawRect(FLinearColor(0.008f, 0.022f, 0.026f, 0.92f), PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(Cyan, PanelX, PanelY, 5.0f * Scale, PanelHeight);

    DrawText(bAircraftUnlocked ? Aircraft->DisplayName : TEXT("CLASSIFIED VEHICLE"),
        White, PanelX + 28.0f * Scale, PanelY + 25.0f * Scale, GEngine->GetLargeFont(), 1.35f * Scale);
    DrawText(
        bAircraftUnlocked
            ? FString::Printf(TEXT("%s // %s"), *Aircraft->Manufacturer.ToUpper(), *Aircraft->Role.ToUpper())
            : TEXT("ROTORLINE SPECIAL ACCESS // CLEARANCE LEVEL BLACK"),
        Cyan,
        PanelX + 30.0f * Scale,
        PanelY + 82.0f * Scale,
        GEngine->GetSmallFont(),
        1.05f * Scale);
    DrawTextBlock(
        bAircraftUnlocked
            ? Aircraft->Summary
            : FString::Printf(TEXT("Complete every campaign operation to declassify this airframe. Clearance progress: %d/%d."),
                OperationsController->GetCompletedCampaignMissionCount(),
                OperationsController->GetCampaignMissionCount()),
        PanelX + 30.0f * Scale, PanelY + 122.0f * Scale,
        PanelWidth - 58.0f * Scale, 1.00f * Scale, FLinearColor(0.77f, 0.82f, 0.78f));

    DrawText(bAircraftUnlocked ? TEXT("VEHICLE RATINGS // 1-5") : TEXT("VEHICLE RATINGS // REDACTED"),
        Cyan, PanelX + 30.0f * Scale, PanelY + 198.0f * Scale, GEngine->GetSmallFont(), 0.88f * Scale);

    const auto DrawRating = [&](const FString& Label, int32 Rating, float Y)
    {
        Rating = bAircraftUnlocked ? FMath::Clamp(Rating, 1, 5) : 0;
        DrawText(Label, FLinearColor(0.78f, 0.84f, 0.80f), PanelX + 30.0f * Scale, Y, GEngine->GetSmallFont(), 1.00f * Scale);
        if (!bAircraftUnlocked)
        {
            DrawText(TEXT("REDACTED"), Amber, PanelX + 250.0f * Scale, Y,
                GEngine->GetSmallFont(), 0.88f * Scale);
            return;
        }
        DrawText(
            FString::Printf(TEXT("%d/5"), Rating),
            Rating >= 4 ? Green : (Rating >= 3 ? Amber : FLinearColor(0.92f, 0.40f, 0.24f)),
            PanelX + 158.0f * Scale,
            Y,
            GEngine->GetSmallFont(),
            0.92f * Scale);
        const float BoxX = PanelX + 194.0f * Scale;
        for (int32 Index = 0; Index < 5; ++Index)
        {
            DrawRect(
                Index < Rating ? Amber : FLinearColor(0.09f, 0.14f, 0.14f, 1.0f),
                BoxX + Index * 34.0f * Scale,
                Y + 3.0f * Scale,
                25.0f * Scale,
                15.0f * Scale);
        }
    };

    const float RatingsY = PanelY + 224.0f * Scale;
    DrawRating(TEXT("SPEED"), Aircraft->Stats.Speed, RatingsY);
    DrawRating(TEXT("MANEUVERABILITY"), Aircraft->Stats.Maneuverability, RatingsY + 45.0f * Scale);
    DrawRating(TEXT("ARMOR"), Aircraft->Stats.Armor, RatingsY + 90.0f * Scale);
    DrawRating(TEXT("CARGO CAPACITY"), Aircraft->Stats.Cargo, RatingsY + 135.0f * Scale);
    DrawRating(TEXT("MISSION FIT"), OperationsController->GetSelectedAircraftMissionFit(), RatingsY + 195.0f * Scale);

    const float StatusY = RatingsY + 252.0f * Scale;
    const bool bReady = bAircraftUnlocked && Aircraft->bAlphaSelectable && Aircraft->bReadyForHangar;
    DrawText(
        bReady
            ? (bGroundVehicle
                ? TEXT("ALPHA DRIVE READY // SELECT TO DEPLOY")
                : TEXT("ALPHA FLIGHT READY // SELECT TO DEPLOY"))
            : bAircraftUnlocked
                ? TEXT("HANGAR DISPLAY // ASSET QUALIFICATION PENDING")
                : TEXT("CLASSIFIED // COMPLETE ALL MISSIONS TO DECLASSIFY"),
        bReady ? Green : Amber,
        PanelX + 30.0f * Scale,
        StatusY,
        GEngine->GetSmallFont(),
        1.00f * Scale);
    if (bAircraftUnlocked && !bReady && !Aircraft->Gaps.IsEmpty())
    {
        DrawTextBlock(Aircraft->Gaps[0], PanelX + 30.0f * Scale, StatusY + 34.0f * Scale, PanelWidth - 60.0f * Scale, 0.90f * Scale, FLinearColor(0.72f, 0.64f, 0.48f));
    }

    const float MissionWidth = 510.0f * Scale;
    const float MissionX = Width - MissionWidth - 40.0f * Scale;
    const float MissionY = 130.0f * Scale;
    DrawRect(FLinearColor(0.008f, 0.022f, 0.026f, 0.83f), MissionX, MissionY, MissionWidth, 182.0f * Scale);
    DrawText(TEXT("SELECTED OPERATION"), Amber, MissionX + 24.0f * Scale, MissionY + 18.0f * Scale, GEngine->GetSmallFont(), 0.90f * Scale);
    DrawText(Mission.Title.ToUpper(), White, MissionX + 24.0f * Scale, MissionY + 51.0f * Scale, GEngine->GetLargeFont(), 1.08f * Scale);
    DrawText(
        FString::Printf(TEXT("%s // %s"), *Mission.Callsign, *Mission.Type.ToUpper()),
        Cyan,
        MissionX + 24.0f * Scale,
        MissionY + 94.0f * Scale,
        GEngine->GetSmallFont(),
        0.90f * Scale);
    const int32 MissionFit = bAircraftUnlocked
        ? FMath::Clamp(OperationsController->GetSelectedAircraftMissionFit(), 1, 5)
        : 0;
    const FString FitVerdict = MissionFit >= 5
        ? TEXT("EXCELLENT MATCH")
        : (MissionFit >= 4
            ? TEXT("STRONG MATCH")
            : (MissionFit >= 3 ? TEXT("WORKABLE") : TEXT("POOR MATCH")));
    DrawText(
        bAircraftUnlocked
            ? FString::Printf(
                TEXT("MISSION FIT %d/5 // %s"),
                MissionFit,
                *FitVerdict)
            : TEXT("AIRFRAME COMPATIBILITY // CLASSIFIED"),
        bAircraftUnlocked
            ? (MissionFit >= 4 ? Green : (MissionFit >= 3 ? Amber : FLinearColor(1.0f, 0.28f, 0.18f)))
            : Amber,
        MissionX + 24.0f * Scale,
        MissionY + 139.0f * Scale,
        GEngine->GetSmallFont(),
        0.90f * Scale);

    const float DossierX = MissionX;
    const float DossierY = MissionY + 202.0f * Scale;
    const float DossierWidth = MissionWidth;
    const float FooterY = Height - 72.0f * Scale;
    const float DossierHeight = FMath::Max(410.0f * Scale, FooterY - DossierY - 24.0f * Scale);
    DrawRect(FLinearColor(0.004f, 0.016f, 0.022f, 0.96f),
        DossierX, DossierY, DossierWidth, DossierHeight);
    DrawRect(Cyan, DossierX, DossierY, DossierWidth, 4.0f * Scale);
    DrawRect(FLinearColor(0.04f, 0.15f, 0.19f, 0.72f),
        DossierX + 10.0f * Scale, DossierY + 12.0f * Scale,
        DossierWidth - 20.0f * Scale, DossierHeight - 22.0f * Scale);
    DrawRect(FLinearColor(0.004f, 0.016f, 0.022f, 0.98f),
        DossierX + 13.0f * Scale, DossierY + 15.0f * Scale,
        DossierWidth - 26.0f * Scale, DossierHeight - 28.0f * Scale);
    DrawText(bAircraftUnlocked
            ? (bGroundVehicle ? TEXT("GROUND VEHICLE TECHNICAL DOSSIER") : TEXT("AIRFRAME TECHNICAL DOSSIER"))
            : TEXT("CLASSIFIED TECHNICAL DOSSIER"), Cyan,
        DossierX + 26.0f * Scale, DossierY + 24.0f * Scale,
        GEngine->GetSmallFont(), 0.90f * Scale);
    DrawText(bAircraftUnlocked ? FString::Printf(TEXT("REGISTRY  //  %s"), *Aircraft->DisplayName.ToUpper()) : TEXT("REGISTRY  //  REDACTED"),
        FLinearColor(0.52f, 0.68f, 0.70f),
        DossierX + 26.0f * Scale, DossierY + 52.0f * Scale,
        GEngine->GetSmallFont(), 0.80f * Scale);

    const float BlueprintX = DossierX + 18.0f * Scale;
    const float BlueprintY = DossierY + 82.0f * Scale;
    const float BlueprintWidth = DossierWidth - 36.0f * Scale;
    const float BlueprintHeight = BlueprintWidth * (2.0f / 3.0f);
    DrawRect(FLinearColor(0.002f, 0.008f, 0.012f, 1.0f),
        BlueprintX - 5.0f * Scale, BlueprintY - 5.0f * Scale,
        BlueprintWidth + 10.0f * Scale, BlueprintHeight + 10.0f * Scale);
    if (bAircraftUnlocked)
    {
        if (UTexture2D* Blueprint = OperationsController->GetAircraftBlueprintTexture(Aircraft->Id))
        {
            DrawTexture(Blueprint, BlueprintX, BlueprintY, BlueprintWidth, BlueprintHeight,
                0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, BLEND_Opaque);
        }
        else
        {
            DrawRect(FLinearColor(0.01f, 0.055f, 0.075f, 1.0f),
                BlueprintX, BlueprintY, BlueprintWidth, BlueprintHeight);
            DrawText(TEXT("TECHNICAL DRAWING UNAVAILABLE"), Amber,
                BlueprintX + 82.0f * Scale, BlueprintY + BlueprintHeight * 0.48f,
                GEngine->GetSmallFont(), 0.88f * Scale);
        }
    }
    else
    {
        DrawRect(FLinearColor(0.002f, 0.006f, 0.008f, 1.0f),
            BlueprintX, BlueprintY, BlueprintWidth, BlueprintHeight);
        DrawText(TEXT("CLASSIFIED"), Amber,
            BlueprintX + BlueprintWidth * 0.36f, BlueprintY + BlueprintHeight * 0.40f,
            GEngine->GetLargeFont(), 1.24f * Scale);
        DrawText(TEXT("ACCESS DENIED // CAMPAIGN CLEARANCE REQUIRED"), Cyan,
            BlueprintX + BlueprintWidth * 0.17f, BlueprintY + BlueprintHeight * 0.56f,
            GEngine->GetSmallFont(), 0.78f * Scale);
    }

    const float DossierInfoY = BlueprintY + BlueprintHeight + 28.0f * Scale;
    DrawText(bAircraftUnlocked
            ? (bGroundVehicle
                ? FString::Printf(TEXT("VEHICLE  //  %s"), *Aircraft->DisplayName.ToUpper())
                : FString::Printf(TEXT("AIRFRAME  //  %s"), *Aircraft->DisplayName.ToUpper()))
            : TEXT("VEHICLE  //  CLASSIFIED"),
        White, DossierX + 28.0f * Scale, DossierInfoY,
        GEngine->GetSmallFont(), 0.95f * Scale);
    DrawText(bAircraftUnlocked
            ? FString::Printf(TEXT("MISSION PROFILE  //  %s"), *Aircraft->Role.ToUpper())
            : TEXT("MISSION PROFILE  //  REDACTED"),
        Cyan, DossierX + 28.0f * Scale, DossierInfoY + 31.0f * Scale,
        GEngine->GetSmallFont(), 0.90f * Scale);
    DrawText(bAircraftUnlocked
            ? (bGroundVehicle
                ? TEXT("STATUS  //  DRIVE READY  //  TURNTABLE ACTIVE")
                : TEXT("STATUS  //  FLIGHT READY  //  TURNTABLE ACTIVE"))
            : TEXT("STATUS  //  LOCKED  //  COMPLETE CAMPAIGN"),
        bAircraftUnlocked ? Green : Amber, DossierX + 28.0f * Scale, DossierInfoY + 62.0f * Scale,
        GEngine->GetSmallFont(), 0.90f * Scale);

    DrawRect(FLinearColor(0.006f, 0.014f, 0.018f, 0.94f), 0.0f, FooterY - 14.0f * Scale, Width, Height - FooterY + 14.0f * Scale);
    DrawText(
        TEXT("L1/R1 OR LEFT/RIGHT  CYCLE     X/ENTER  SELECT + DEPLOY     CIRCLE/ESC  OPERATIONS     TRIANGLE/V  AUDIO"),
        White,
        46.0f * Scale,
        FooterY,
        GEngine->GetSmallFont(),
        0.95f * Scale);
}

void ARotorlineOperationsHUD::DrawOperationsBoard()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController)
    {
        return;
    }

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.72f, 1.35f);
    if (UTexture2D* Background = OperationsController->GetOperationsBoardBackgroundTexture())
    {
        const float SourceAspect = Background->GetSizeY() > 0
            ? static_cast<float>(Background->GetSizeX()) / static_cast<float>(Background->GetSizeY())
            : 16.0f / 9.0f;
        const float DestinationAspect = Width / FMath::Max(1.0f, Height);
        float U = 0.0f;
        float V = 0.0f;
        float UL = 1.0f;
        float VL = 1.0f;
        if (DestinationAspect > SourceAspect)
        {
            VL = SourceAspect / DestinationAspect;
            V = (1.0f - VL) * 0.5f;
        }
        else
        {
            UL = DestinationAspect / SourceAspect;
            U = (1.0f - UL) * 0.5f;
        }
        DrawTexture(Background, 0.0f, 0.0f, Width, Height, U, V, UL, VL,
            FLinearColor(0.72f, 0.76f, 0.78f, 1.0f), BLEND_Opaque);
    }
    else
    {
        DrawRect(FLinearColor(0.008f, 0.015f, 0.018f, 1.0f), 0.0f, 0.0f, Width, Height);
    }
    DrawRect(FLinearColor(0.004f, 0.014f, 0.018f, 0.74f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.92f, 0.55f, 0.10f, 1.0f), 0.0f, 0.0f, Width, 6.0f * Scale);

    DrawText(TEXT("ROTORLINE // OPERATIONS BOARD"), FLinearColor(0.95f, 0.97f, 0.92f), 54.0f * Scale, 36.0f * Scale, GEngine->GetLargeFont(), 1.18f * Scale);
    const bool bGamepadActive = OperationsController->HasReceivedGamepadInput();
    DrawText(
        bGamepadActive ? TEXT("CONTROLLER INPUT: ACTIVE") : TEXT("CONTROLLER INPUT: PRESS D-PAD"),
        bGamepadActive ? FLinearColor(0.35f, 0.95f, 0.55f) : FLinearColor(0.95f, 0.65f, 0.18f),
        Width - 365.0f * Scale,
        48.0f * Scale,
        GEngine->GetSmallFont(),
        0.9f * Scale);
    DrawText(
        FString::Printf(TEXT("REPUTATION %d"), OperationsController->GetReputation()),
        FLinearColor(0.48f, 0.73f, 0.72f),
        Width - 365.0f * Scale,
        78.0f * Scale,
        GEngine->GetSmallFont(),
        0.9f * Scale);
    DrawText(TEXT("SELECT MISSION"), FLinearColor(0.92f, 0.55f, 0.10f), 55.0f * Scale, 106.0f * Scale, GEngine->GetSmallFont(), 1.0f * Scale);

    const TArray<FRotorlineMissionDefinition>& Missions = OperationsController->GetMissions();
    if (Missions.IsEmpty())
    {
        DrawText(TEXT("MISSION CATALOG OFFLINE"), FLinearColor::Red, 55.0f * Scale, 155.0f * Scale, GEngine->GetLargeFont(), Scale);
        DrawText(OperationsController->GetCatalogError(), FLinearColor::White, 55.0f * Scale, 205.0f * Scale, GEngine->GetSmallFont(), Scale);
        return;
    }

    const int32 Selected = OperationsController->GetSelectedMissionIndex();
    const int32 VisibleRows = FMath::Clamp(FMath::FloorToInt((Height / Scale - 260.0f) / 44.0f), 8, 17);
    const int32 FirstRow = FMath::Clamp(Selected - VisibleRows / 2, 0, FMath::Max(0, Missions.Num() - VisibleRows));
    const float ListX = 54.0f * Scale;
    const float ListY = 142.0f * Scale;
    const float ListWidth = 470.0f * Scale;
    const float RowHeight = 43.0f * Scale;
    DrawRect(FLinearColor(0.03f, 0.05f, 0.055f, 0.94f), ListX - 10.0f * Scale, ListY - 9.0f * Scale, ListWidth, (VisibleRows * RowHeight) + 18.0f * Scale);

    for (int32 Row = 0; Row < VisibleRows && FirstRow + Row < Missions.Num(); ++Row)
    {
        const int32 MissionIndex = FirstRow + Row;
        const FRotorlineMissionDefinition& Mission = Missions[MissionIndex];
        const float Y = ListY + Row * RowHeight;
        const bool bSelected = MissionIndex == Selected;
        const bool bUnlocked = OperationsController->IsMissionUnlocked(Mission);
        const bool bCompleted = OperationsController->IsMissionCompleted(Mission.Id);
        if (bSelected)
        {
            DrawRect(FLinearColor(0.92f, 0.55f, 0.10f, 0.92f), ListX - 4.0f * Scale, Y - 3.0f * Scale, ListWidth - 12.0f * Scale, RowHeight - 3.0f * Scale);
        }
        const FLinearColor TextColor = bSelected
            ? FLinearColor(0.03f, 0.035f, 0.03f)
            : (bUnlocked ? FLinearColor(0.73f, 0.78f, 0.75f) : FLinearColor(0.36f, 0.40f, 0.39f));
        const FString Status = bCompleted ? TEXT("[DONE]") : (bUnlocked ? TEXT("") : TEXT("[LOCKED]"));
        DrawText(FString::Printf(TEXT("%02d  %s %s"), MissionIndex + 1, *Mission.Title.ToUpper(), *Status), TextColor, ListX + 8.0f * Scale, Y + 5.0f * Scale, GEngine->GetSmallFont(), 0.92f * Scale);
        if (!bUnlocked)
        {
            DrawText(FString::Printf(TEXT("%d REP"), Mission.Unlock), TextColor, ListX + 390.0f * Scale, Y + 5.0f * Scale, GEngine->GetSmallFont(), 0.78f * Scale);
        }
    }

    const FRotorlineMissionDefinition& Mission = Missions[Selected];
    const bool bMissionUnlocked = OperationsController->IsMissionUnlocked(Mission);
    const float DetailX = 570.0f * Scale;
    const float DetailWidth = Width - DetailX - 55.0f * Scale;
    DrawText(Mission.Title.ToUpper(), FLinearColor(0.95f, 0.97f, 0.92f), DetailX, 110.0f * Scale, GEngine->GetLargeFont(), 1.34f * Scale);
    DrawText(FString::Printf(TEXT("CALLSIGN %s  //  %s  //  %s  //  %d XP"), *Mission.Callsign, *Mission.Weather.ToUpper(), *Mission.TimeOfDay.ToUpper(), Mission.Reward), FLinearColor(0.48f, 0.73f, 0.72f), DetailX, 165.0f * Scale, GEngine->GetSmallFont(), 0.92f * Scale);
    DrawTextBlock(Mission.Briefing, DetailX, 208.0f * Scale, DetailWidth, 0.96f * Scale, FLinearColor(0.82f, 0.85f, 0.81f));
    DrawText(FString::Printf(TEXT("%d OBJECTIVES  //  TARGET %d:%02d"), Mission.Objectives.Num(), Mission.TimeTarget / 60, Mission.TimeTarget % 60), FLinearColor(0.92f, 0.55f, 0.10f), DetailX, 330.0f * Scale, GEngine->GetSmallFont(), 0.92f * Scale);
    const FString MissionProfile = FString::Printf(
        TEXT("DIFFICULTY %d/5 // %s"),
        FMath::Clamp(Mission.Difficulty, 1, 5),
        Mission.bRequiresWeapons ? TEXT("COMBAT LOADOUT REQUIRED") : TEXT("NON-COMBAT TASKING"));
    DrawText(MissionProfile, Mission.bRequiresWeapons ? FLinearColor(1.0f, 0.22f, 0.10f) : FLinearColor(0.48f, 0.73f, 0.72f), DetailX, 357.0f * Scale, GEngine->GetSmallFont(), 0.88f * Scale);
    if (!bMissionUnlocked)
    {
        DrawText(
            FString::Printf(TEXT("LOCKED // REQUIRES %d REPUTATION"), Mission.Unlock),
            FLinearColor(1.0f, 0.25f, 0.18f),
            DetailX,
            382.0f * Scale,
            GEngine->GetSmallFont(),
            0.92f * Scale);
    }

    const float HangarY = 420.0f * Scale;
    const float HangarHeight = 225.0f * Scale;
    DrawRect(FLinearColor(0.035f, 0.052f, 0.052f, 0.97f), DetailX, HangarY, DetailWidth, HangarHeight);
    DrawRect(FLinearColor(0.92f, 0.55f, 0.10f, 1.0f), DetailX, HangarY, DetailWidth, 6.0f * Scale);
    DrawText(TEXT("ENTER AIRCRAFT HANGAR"), FLinearColor(1.0f, 0.72f, 0.25f), DetailX + 30.0f * Scale, HangarY + 30.0f * Scale, GEngine->GetLargeFont(), 1.10f * Scale);
    DrawText(
        TEXT("Inspect the full fleet in 3D, compare flight ratings and mission suitability, then choose the aircraft for this sortie."),
        FLinearColor(0.72f, 0.77f, 0.73f),
        DetailX + 32.0f * Scale,
        HangarY + 96.0f * Scale,
        GEngine->GetSmallFont(),
        0.84f * Scale);
    DrawText(
        FString::Printf(TEXT("RECOMMENDED ROLE // %s"), *Mission.RecommendedCraft.ToUpper()),
        FLinearColor(0.48f, 0.73f, 0.72f),
        DetailX + 32.0f * Scale,
        HangarY + 157.0f * Scale,
        GEngine->GetSmallFont(),
        0.84f * Scale);

    const float FooterY = Height - 72.0f * Scale;
    DrawRect(FLinearColor(0.02f, 0.035f, 0.038f, 1.0f), 0.0f, FooterY - 18.0f * Scale, Width, Height - FooterY + 18.0f * Scale);
    DrawText(TEXT("UP / DOWN  SELECT     X / ENTER  HANGAR     P  PATCHES     V  AUDIO"),
        FLinearColor(0.88f, 0.91f, 0.86f), 55.0f * Scale, FooterY, GEngine->GetSmallFont(), 0.76f * Scale);

    const float ReturnButtonX = Width - 350.0f * Scale;
    const float ReturnButtonY = FooterY - 9.0f * Scale;
    const float ReturnButtonWidth = 300.0f * Scale;
    const float ReturnButtonHeight = 44.0f * Scale;
    DrawRect(FLinearColor(0.045f, 0.19f, 0.18f, 1.0f), ReturnButtonX, ReturnButtonY, ReturnButtonWidth, ReturnButtonHeight);
    DrawRect(FLinearColor(0.92f, 0.55f, 0.10f, 1.0f), ReturnButtonX, ReturnButtonY, 6.0f * Scale, ReturnButtonHeight);
    DrawText(TEXT("RETURN TO MAIN MENU"), FLinearColor(0.98f, 0.90f, 0.70f),
        ReturnButtonX + 20.0f * Scale, ReturnButtonY + 8.0f * Scale, GEngine->GetSmallFont(), 0.74f * Scale);
    DrawText(TEXT("ESC / CIRCLE"), FLinearColor(0.48f, 0.73f, 0.72f),
        ReturnButtonX + 225.0f * Scale, ReturnButtonY + 9.0f * Scale, GEngine->GetSmallFont(), 0.60f * Scale);
}

void ARotorlineOperationsHUD::DrawFlightPauseOverlay()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController)
    {
        return;
    }

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.72f, 1.35f);
    const float PanelWidth = 760.0f * Scale;
    const float PanelHeight = 650.0f * Scale;
    const float PanelX = (Width - PanelWidth) * 0.5f;
    const float PanelY = (Height - PanelHeight) * 0.5f;

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.70f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.012f, 0.025f, 0.028f, 0.99f), PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(FLinearColor(0.25f, 0.92f, 0.92f, 1.0f), PanelX, PanelY, PanelWidth, 6.0f * Scale);
    DrawText(TEXT("FLIGHT PAUSED"), FLinearColor(0.95f, 0.97f, 0.92f), PanelX + 42.0f * Scale, PanelY + 34.0f * Scale, GEngine->GetLargeFont(), 1.22f * Scale);
    const TArray<FRotorlineMissionDefinition>& Missions = OperationsController->GetMissions();
    const int32 MissionIndex = OperationsController->GetSelectedMissionIndex();
    const FString MissionIdentity = Missions.IsValidIndex(MissionIndex)
        ? FString::Printf(TEXT("MISSION %d - %s"), MissionIndex + 1, *Missions[MissionIndex].Title)
        : TEXT("ACTIVE MISSION");
    DrawText(MissionIdentity, FLinearColor(0.35f, 0.95f, 0.55f), PanelX + 44.0f * Scale, PanelY + 92.0f * Scale, GEngine->GetSmallFont(), 0.84f * Scale);

    static const TCHAR* Labels[] =
    {
        TEXT("RESUME FLIGHT"),
        TEXT("AUDIO MIX"),
        TEXT("CONTROLS"),
        TEXT("GRAPHICS SETTINGS"),
        TEXT("ABORT MISSION")
    };
    static const TCHAR* Details[] =
    {
        TEXT("Continue from this exact position and objective."),
        TEXT("Adjust master, engine, music, radio and weapons volume."),
        TEXT("Configure keyboard, gamepad, joystick calibration and bindings."),
        TEXT("Choose a simple performance preset without entering controller setup."),
        TEXT("End this sortie and return to the Operations Board.")
    };

    const float RowX = PanelX + 42.0f * Scale;
    const float RowY = PanelY + 142.0f * Scale;
    const float RowWidth = PanelWidth - 84.0f * Scale;
    const float RowHeight = 80.0f * Scale;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const bool bSelected = Index == OperationsController->GetSelectedPauseRow();
        const bool bAbort = Index == 4;
        const float Y = RowY + Index * RowHeight;
        if (bSelected)
        {
            DrawRect(bAbort ? FLinearColor(0.20f, 0.04f, 0.025f, 1.0f) : FLinearColor(0.10f, 0.16f, 0.16f, 1.0f), RowX, Y, RowWidth, 78.0f * Scale);
            DrawRect(bAbort ? FLinearColor(1.0f, 0.22f, 0.10f, 1.0f) : FLinearColor(0.92f, 0.55f, 0.10f, 1.0f), RowX, Y, 6.0f * Scale, 78.0f * Scale);
        }
        DrawText(Labels[Index], bSelected ? (bAbort ? FLinearColor(1.0f, 0.35f, 0.20f) : FLinearColor(1.0f, 0.72f, 0.25f)) : FLinearColor(0.76f, 0.81f, 0.77f), RowX + 22.0f * Scale, Y + 10.0f * Scale, GEngine->GetSmallFont(), 0.96f * Scale);
        DrawText(Details[Index], FLinearColor(0.52f, 0.66f, 0.64f), RowX + 22.0f * Scale, Y + 42.0f * Scale, GEngine->GetSmallFont(), 0.76f * Scale);
    }

    if (OperationsController->IsAbortMissionPending())
    {
        DrawText(TEXT("PRESS X / ENTER AGAIN TO CONFIRM ABORT"), FLinearColor(1.0f, 0.22f, 0.10f), PanelX + 44.0f * Scale, PanelY + 535.0f * Scale, GEngine->GetSmallFont(), 0.86f * Scale);
    }
    else
    {
        DrawText(TEXT("D-PAD / LEFT STICK  SELECT     X / ENTER  CONFIRM"), FLinearColor(0.86f, 0.90f, 0.86f), PanelX + 44.0f * Scale, PanelY + 535.0f * Scale, GEngine->GetSmallFont(), 0.78f * Scale);
    }
    DrawText(TEXT("TRIANGLE / V  AUDIO MIX     CIRCLE / OPTIONS / ESC / M  RESUME"), FLinearColor(0.86f, 0.90f, 0.86f), PanelX + 44.0f * Scale, PanelY + 577.0f * Scale, GEngine->GetSmallFont(), 0.76f * Scale);
}

void ARotorlineOperationsHUD::DrawAudioSettingsOverlay()
{
    const ARotorlineOperationsPlayerController* OperationsController = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !OperationsController)
    {
        return;
    }

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.72f, 1.35f);
    const float PanelWidth = 690.0f * Scale;
    const float PanelHeight = 630.0f * Scale;
    const float PanelX = (Width - PanelWidth) * 0.5f;
    const float PanelY = (Height - PanelHeight) * 0.5f;

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.012f, 0.025f, 0.028f, 0.99f), PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(FLinearColor(0.92f, 0.55f, 0.10f, 1.0f), PanelX, PanelY, PanelWidth, 6.0f * Scale);
    DrawText(TEXT("AUDIO MIX"), FLinearColor(0.95f, 0.97f, 0.92f), PanelX + 38.0f * Scale, PanelY + 30.0f * Scale, GEngine->GetLargeFont(), 1.12f * Scale);
    DrawText(TEXT("Saved to your Rotorline profile"), FLinearColor(0.48f, 0.73f, 0.72f), PanelX + 40.0f * Scale, PanelY + 82.0f * Scale, GEngine->GetSmallFont(), 0.84f * Scale);

    static const TCHAR* Labels[] =
    {
        TEXT("MASTER"),
        TEXT("ENVIRONMENT / WIND"),
        TEXT("ENGINE / ROTORS"),
        TEXT("MUSIC"),
        TEXT("RADIO / WARNINGS"),
        TEXT("WEAPONS / EXPLOSIONS")
    };
    static const ERotorlineAudioChannel Channels[] =
    {
        ERotorlineAudioChannel::Master,
        ERotorlineAudioChannel::Environment,
        ERotorlineAudioChannel::Engine,
        ERotorlineAudioChannel::Music,
        ERotorlineAudioChannel::Radio,
        ERotorlineAudioChannel::WeaponsExplosions
    };

    const float RowX = PanelX + 38.0f * Scale;
    const float RowY = PanelY + 126.0f * Scale;
    const float RowWidth = PanelWidth - 76.0f * Scale;
    const float RowHeight = 62.0f * Scale;
    for (int32 Index = 0; Index < 6; ++Index)
    {
        const bool bSelected = Index == OperationsController->GetSelectedAudioRow();
        const float Y = RowY + Index * RowHeight;
        const float Value = OperationsController->GetAudioSetting(Channels[Index]);
        if (bSelected)
        {
            DrawRect(FLinearColor(0.12f, 0.16f, 0.15f, 1.0f), RowX, Y - 5.0f * Scale, RowWidth, 50.0f * Scale);
            DrawRect(FLinearColor(0.92f, 0.55f, 0.10f, 1.0f), RowX, Y - 5.0f * Scale, 5.0f * Scale, 50.0f * Scale);
        }
        DrawText(Labels[Index], bSelected ? FLinearColor(1.0f, 0.72f, 0.25f) : FLinearColor(0.76f, 0.81f, 0.77f), RowX + 18.0f * Scale, Y + 7.0f * Scale, GEngine->GetSmallFont(), 0.90f * Scale);
        const float BarX = RowX + 292.0f * Scale;
        const float BarY = Y + 14.0f * Scale;
        const float BarWidth = 220.0f * Scale;
        DrawRect(FLinearColor(0.05f, 0.08f, 0.08f, 1.0f), BarX, BarY, BarWidth, 14.0f * Scale);
        DrawRect(bSelected ? FLinearColor(0.92f, 0.55f, 0.10f, 1.0f) : FLinearColor(0.28f, 0.62f, 0.60f, 1.0f), BarX, BarY, BarWidth * Value, 14.0f * Scale);
        DrawText(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.0f)), FLinearColor::White, RowX + RowWidth - 64.0f * Scale, Y + 5.0f * Scale, GEngine->GetSmallFont(), 0.88f * Scale);
    }

    DrawText(TEXT("UP / DOWN  SELECT     LEFT / RIGHT  ADJUST     R1 / R  RESET     CIRCLE / ESC / TRIANGLE  CLOSE"), FLinearColor(0.86f, 0.90f, 0.86f), PanelX + 38.0f * Scale, PanelY + PanelHeight - 58.0f * Scale, GEngine->GetSmallFont(), 0.76f * Scale);
}

void ARotorlineOperationsHUD::DrawGraphicsSettingsOverlay()
{
    const ARotorlineOperationsPlayerController* Controller = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !Controller)
    {
        return;
    }

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.72f, 1.35f);
    const float PanelWidth = FMath::Min(1380.0f * Scale, Width - 120.0f * Scale);
    const float PanelHeight = FMath::Min(880.0f * Scale, Height - 100.0f * Scale);
    const float PanelX = (Width - PanelWidth) * 0.5f;
    const float PanelY = (Height - PanelHeight) * 0.5f;
    const FLinearColor Backdrop(0.018f, 0.055f, 0.065f, 0.97f);
    const FLinearColor Panel(0.035f, 0.105f, 0.115f, 0.98f);
    const FLinearColor Cyan(0.48f, 0.94f, 0.91f, 1.0f);
    const FLinearColor Amber(1.0f, 0.72f, 0.23f, 1.0f);
    const FLinearColor White(0.9f, 0.94f, 0.94f, 1.0f);
    const FLinearColor Muted(0.55f, 0.68f, 0.69f, 1.0f);

    DrawRect(Backdrop, 0.0f, 0.0f, Width, Height);
    DrawRect(Panel, PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(Cyan, PanelX, PanelY, 7.0f * Scale, PanelHeight);
    DrawText(TEXT("GRAPHICS SETTINGS"), White, PanelX + 50.0f * Scale, PanelY + 38.0f * Scale, GEngine->GetLargeFont(), 1.35f * Scale);
    DrawText(TEXT("Choose a performance preset. Your selection is saved to your Rotorline profile."), Muted, PanelX + 50.0f * Scale, PanelY + 104.0f * Scale, GEngine->GetSmallFont(), 0.95f * Scale);

    const bool bTurbo = Controller->GetSimpleGraphicsModeLabel().StartsWith(TEXT("TURBO"));
    const float CardY = PanelY + 150.0f * Scale;
    const float CardGap = 30.0f * Scale;
    const float CardW = (PanelWidth - 100.0f * Scale - CardGap) * 0.5f;
    const float CardH = 300.0f * Scale;
    const float LeftX = PanelX + 50.0f * Scale;
    const float RightX = LeftX + CardW + CardGap;
    DrawRect(!bTurbo ? FLinearColor(0.08f, 0.23f, 0.22f, 1.0f) : FLinearColor(0.03f, 0.08f, 0.09f, 1.0f), LeftX, CardY, CardW, CardH);
    DrawRect(bTurbo ? FLinearColor(0.24f, 0.16f, 0.05f, 1.0f) : FLinearColor(0.03f, 0.08f, 0.09f, 1.0f), RightX, CardY, CardW, CardH);
    DrawText(TEXT("SNAIL MODE"), !bTurbo ? Cyan : White, LeftX + 30.0f * Scale, CardY + 30.0f * Scale, GEngine->GetLargeFont(), 1.08f * Scale);
    DrawText(!bTurbo ? TEXT("CURRENT PRESET") : TEXT("PERFORMANCE PRESET"), !bTurbo ? Cyan : Muted, LeftX + 30.0f * Scale, CardY + 86.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);
    DrawText(TEXT("Best for older or entry-level GPUs."), White, LeftX + 30.0f * Scale, CardY + 142.0f * Scale, GEngine->GetSmallFont(), 0.94f * Scale);
    DrawText(TEXT("Ray tracing and expensive effects remain off"), Muted, LeftX + 30.0f * Scale, CardY + 194.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);
    DrawText(TEXT("to preserve smoother gameplay."), Muted, LeftX + 30.0f * Scale, CardY + 228.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);
    DrawText(TEXT("TURBO MODE"), bTurbo ? Amber : White, RightX + 30.0f * Scale, CardY + 30.0f * Scale, GEngine->GetLargeFont(), 1.08f * Scale);
    DrawText(bTurbo ? TEXT("CURRENT PRESET") : TEXT("QUALITY PRESET"), bTurbo ? Amber : Muted, RightX + 30.0f * Scale, CardY + 86.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);
    DrawText(TEXT("Higher visual quality for faster GPUs."), White, RightX + 30.0f * Scale, CardY + 142.0f * Scale, GEngine->GetSmallFont(), 0.94f * Scale);
    DrawText(TEXT("Uses more demanding lighting, shadows, and"), Muted, RightX + 30.0f * Scale, CardY + 194.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);
    DrawText(TEXT("environment settings."), Muted, RightX + 30.0f * Scale, CardY + 228.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);

    static const TCHAR* Rows[] = { TEXT("CHANGE PERFORMANCE PRESET"), TEXT("BACK") };
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const float RowY = PanelY + (508.0f + Index * 96.0f) * Scale;
        const bool bSelected = Controller->GetSelectedGraphicsRow() == Index;
        DrawRect(bSelected ? FLinearColor(0.12f, 0.29f, 0.29f, 1.0f) : FLinearColor(0.025f, 0.075f, 0.08f, 1.0f), PanelX + 50.0f * Scale, RowY, PanelWidth - 100.0f * Scale, 76.0f * Scale);
        DrawRect(bSelected ? Cyan : FLinearColor::Transparent, PanelX + 50.0f * Scale, RowY, 7.0f * Scale, 76.0f * Scale);
        DrawText(Rows[Index], bSelected ? Cyan : White, PanelX + 82.0f * Scale, RowY + 21.0f * Scale, GEngine->GetLargeFont(), 0.88f * Scale);
    }
    DrawText(TEXT("UP / DOWN  SELECT     X / ENTER  CONFIRM     ESC / CIRCLE  BACK"), Muted, PanelX + 50.0f * Scale, PanelY + PanelHeight - 50.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);
}

void ARotorlineOperationsHUD::DrawControlsSettingsOverlay()
{
    const ARotorlineOperationsPlayerController* Controller = Cast<ARotorlineOperationsPlayerController>(PlayerOwner);
    if (!Canvas || !Controller) return;

    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float Scale = FMath::Clamp(Height / 1080.0f, 0.68f, 1.28f);
    const float PanelX = 28.0f * Scale;
    const float PanelY = 24.0f * Scale;
    const float PanelW = Width - 56.0f * Scale;
    const float PanelH = Height - 48.0f * Scale;
    const FLinearColor White(0.93f, 0.97f, 0.94f, 1.0f);
    const FLinearColor Muted(0.50f, 0.67f, 0.65f, 1.0f);
    const FLinearColor Amber(1.0f, 0.69f, 0.22f, 1.0f);
    const FLinearColor Cyan(0.28f, 0.92f, 0.90f, 1.0f);
    DrawRect(FLinearColor(0.0f, 0.012f, 0.016f, 0.92f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.012f, 0.045f, 0.048f, 0.99f), PanelX, PanelY, PanelW, PanelH);
    DrawRect(Amber, PanelX, PanelY, PanelW, 6.0f * Scale);
    DrawText(TEXT("ROTORLINE // CONTROL CONFIGURATION"), White, PanelX + 36.0f * Scale,
        PanelY + 28.0f * Scale, GEngine->GetLargeFont(), 1.10f * Scale);
    DrawText(Controller->GetControlsStatus(), Cyan, PanelX + 38.0f * Scale,
        PanelY + 80.0f * Scale, GEngine->GetSmallFont(), 0.72f * Scale);
    const TCHAR* Tabs[] = { TEXT("KEYBOARD PRESET"), TEXT("CONTROLLER PRESET"), TEXT("HOTAS PRESET") };
    const float TabY = PanelY + 116.0f * Scale;
    const float TabW = (PanelW - 76.0f * Scale) / 3.0f;
    for (int32 Tab = 0; Tab < 3; ++Tab)
    {
        const float X = PanelX + 38.0f * Scale + Tab * TabW;
        const bool Active = Controller->GetSelectedControlsTab() == Tab;
        DrawRect(Active ? FLinearColor(0.08f, 0.22f, 0.22f, 1.0f) : FLinearColor(0.02f, 0.08f, 0.09f, 1.0f),
            X, TabY, TabW - 6.0f * Scale, 48.0f * Scale);
        if (Active) DrawRect(Amber, X, TabY + 44.0f * Scale, TabW - 6.0f * Scale, 4.0f * Scale);
        DrawText(Tabs[Tab], Active ? White : Muted, X + 16.0f * Scale, TabY + 13.0f * Scale,
            GEngine->GetSmallFont(), 0.72f * Scale);
    }

    const float BodyX = PanelX + 38.0f * Scale;
    const float BodyY = TabY + 76.0f * Scale;
    const float BodyW = PanelW - 76.0f * Scale;
    DrawRect(FLinearColor(0.008f, 0.026f, 0.029f, 0.96f), BodyX, BodyY, BodyW, PanelH - 252.0f * Scale);

    const ERotorlineControlsMode Mode = Controller->GetControlsMode();
    const int32 SelectedRow = Controller->GetSelectedControlsRow();
    if (Mode == ERotorlineControlsMode::DeviceSelect)
    {
        const bool bSelectingGamepad = Controller->GetSelectedControlsTab() == 1;
        DrawText(bSelectingGamepad ? TEXT("SELECT CONNECTED GAMEPAD") : TEXT("SELECT CONNECTED FLIGHT CONTROLLER"), Amber, BodyX + 32.0f * Scale,
            BodyY + 28.0f * Scale, GEngine->GetLargeFont(), 0.98f * Scale);
        const URotorlineFlightControllerSubsystem* Input = Controller->GetGameInstance()
            ? Controller->GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
        int32 FlightRow = 0;
        if (Input)
        {
            for (const FRotorlineControllerDeviceInfo& Device : Input->GetDevices())
            {
                if (!Device.bConnected || Device.bGamepadCompatible != bSelectingGamepad) continue;
                const float Y = BodyY + (92.0f + FlightRow * 58.0f) * Scale;
                if (FlightRow == SelectedRow)
                {
                    DrawRect(FLinearColor(0.08f, 0.20f, 0.20f, 1.0f), BodyX + 28.0f * Scale,
                        Y - 8.0f * Scale, BodyW - 56.0f * Scale, 46.0f * Scale);
                    DrawRect(Amber, BodyX + 28.0f * Scale, Y - 8.0f * Scale, 5.0f * Scale, 46.0f * Scale);
                }
                DrawText(Device.DisplayName.ToUpper(), FlightRow == SelectedRow ? White : Muted,
                    BodyX + 48.0f * Scale, Y, GEngine->GetSmallFont(), 0.78f * Scale);
                DrawText(FString::Printf(TEXT("%d AXES // %d BUTTONS // %d HAT // %s"),
                    Device.Capabilities.AxisCount, Device.Capabilities.ButtonCount,
                    Device.Capabilities.HatCount, *Device.BackendName.ToUpper()),
                    FlightRow == SelectedRow ? Cyan : Muted, BodyX + 470.0f * Scale, Y,
                    GEngine->GetSmallFont(), 0.68f * Scale);
                ++FlightRow;
            }
        }
        if (FlightRow == 0)
        {
            DrawText(bSelectingGamepad
                ? TEXT("NO CONFIGURABLE GAMEPAD CONNECTED // KEYBOARD REMAINS AVAILABLE")
                : TEXT("NO FLIGHT CONTROLLER CONNECTED // KEYBOARD AND GAMEPAD REMAIN AVAILABLE"),
                Muted, BodyX + 44.0f * Scale, BodyY + 100.0f * Scale,
                GEngine->GetSmallFont(), 0.78f * Scale);
        }
    }
    else if (Mode == ERotorlineControlsMode::FirstTimePrompt)
    {
        DrawText(TEXT("FLIGHT CONTROLLER FOUND"), Amber, BodyX + 32.0f * Scale, BodyY + 28.0f * Scale,
            GEngine->GetLargeFont(), 1.02f * Scale);
        DrawText(TEXT("Choose how Rotorline should prepare this device. Nothing changes until a profile is saved."),
            Muted, BodyX + 34.0f * Scale, BodyY + 80.0f * Scale, GEngine->GetSmallFont(), 0.78f * Scale);
        const TCHAR* Choices[] = { TEXT("BEGIN GUIDED CALIBRATION"), TEXT("USE COMPATIBLE PROFILE"),
            TEXT("USE GENERIC CAPABILITY DEFAULTS"), TEXT("CONTINUE WITH KEYBOARD / GAMEPAD") };
        for (int32 Row = 0; Row < 4; ++Row)
        {
            const float Y = BodyY + (142.0f + Row * 72.0f) * Scale;
            if (Row == SelectedRow)
            {
                DrawRect(FLinearColor(0.08f, 0.20f, 0.20f, 1.0f), BodyX + 30.0f * Scale, Y,
                    BodyW - 60.0f * Scale, 56.0f * Scale);
                DrawRect(Amber, BodyX + 30.0f * Scale, Y, 6.0f * Scale, 56.0f * Scale);
            }
            DrawText(Choices[Row], Row == SelectedRow ? White : Muted, BodyX + 54.0f * Scale,
                Y + 16.0f * Scale, GEngine->GetSmallFont(), 0.82f * Scale);
        }
    }
    else if (Mode == ERotorlineControlsMode::AxisCalibration)
    {
        const FName RequestedAction = Controller->GetControlsWizardAction();
        const FString RequestedAxisName = RequestedAction == RotorlineFlightControllerActions::Collective
            ? FString(TEXT("THROTTLE"))
            : RequestedAction.ToString().ToUpper();
        DrawText(FString::Printf(TEXT("CALIBRATE %s  //  AXIS %d OF 4"),
            *RequestedAxisName, Controller->GetControlsWizardStep() + 1),
            Amber, BodyX + 32.0f * Scale, BodyY + 28.0f * Scale, GEngine->GetLargeFont(), 0.98f * Scale);
        FString CalibrationInstruction(TEXT("Release every control and hold it still before moving the requested axis."));
        if (RequestedAction == RotorlineFlightControllerActions::Roll)
            CalibrationInstruction = TEXT("Release to center, move FULL LEFT, move FULL RIGHT, then RELEASE to center.");
        else if (RequestedAction == RotorlineFlightControllerActions::Pitch)
            CalibrationInstruction = TEXT("Release to center, move FULL FORWARD, move FULL BACKWARD, then RELEASE to center.");
        else if (RequestedAction == RotorlineFlightControllerActions::Yaw)
            CalibrationInstruction = TEXT("Release to center, move FULL LEFT, move FULL RIGHT, then RELEASE to center.");
        else if (RequestedAction == RotorlineFlightControllerActions::Collective)
            CalibrationInstruction = TEXT("OPTIONAL LEVER: press M to DISABLE + SKIP it. Otherwise move FULL LOW, then FULL HIGH; do not release.");
        DrawText(CalibrationInstruction,
            White, BodyX + 34.0f * Scale, BodyY + 82.0f * Scale, GEngine->GetSmallFont(), 0.78f * Scale);
        const FString CalibrationConfirm = RequestedAction == RotorlineFlightControllerActions::Collective
            ? TEXT("M DISABLES AXIS 3 AND CONTINUES TO YAW // KEYBOARD Q/E THROTTLE REMAINS ACTIVE")
            : TEXT("PRESS X / ENTER ONLY AFTER DETECTED APPEARS AND THE CONTROL HAS BEEN RELEASED.");
        DrawText(CalibrationConfirm,
            Cyan, BodyX + 34.0f * Scale, BodyY + 110.0f * Scale, GEngine->GetSmallFont(), 0.70f * Scale);
        const FRotorlineFlightControllerProfile& Profile = Controller->GetWorkingControllerProfile();
        (void)Profile;
        URotorlineFlightControllerSubsystem* Input = Controller->GetGameInstance()
            ? Controller->GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
        const FRotorlineControllerDeviceInfo* Device = Input ? Input->GetDevices().FindByPredicate([Controller](const FRotorlineControllerDeviceInfo& Entry)
        {
            return Entry.DeviceId == Controller->GetControlsDeviceId();
        }) : nullptr;
        const int32 AxisCount = Device ? Device->Capabilities.AxisCount : 0;
        constexpr int32 VisibleCalibrationAxes = 6;
        const int32 DetectedAxis = Controller->GetControlsDetectedAxis();
        const int32 FirstCalibrationAxis = DetectedAxis == INDEX_NONE ? 0 :
            FMath::Clamp(DetectedAxis - VisibleCalibrationAxes / 2, 0,
                FMath::Max(0, AxisCount - VisibleCalibrationAxes));
        const int32 LastCalibrationAxis = FMath::Min(AxisCount, FirstCalibrationAxis + VisibleCalibrationAxes);
        for (int32 Axis = FirstCalibrationAxis; Axis < LastCalibrationAxis; ++Axis)
        {
            float Raw = 0.0f;
            Input->GetRawAxisValue(Controller->GetControlsDeviceId(), Axis, Raw);
            const float AxisMinimum = Device->Capabilities.Axes.IsValidIndex(Axis)
                ? Device->Capabilities.Axes[Axis].RawMinimum : 0.0f;
            const float AxisMaximum = Device->Capabilities.Axes.IsValidIndex(Axis)
                ? Device->Capabilities.Axes[Axis].RawMaximum : 1.0f;
            const float Normalized = FMath::Clamp((Raw - AxisMinimum) /
                FMath::Max(0.001f, AxisMaximum - AxisMinimum), 0.0f, 1.0f);
            const FRotorlineAxisBinding* Binding = Controller->GetWorkingControllerProfile().AxisBindings.FindByPredicate(
                [Axis](const FRotorlineAxisBinding& Entry) { return Entry.NativeAxisIndex == Axis; });
            const float Calibrated = Binding
                ? URotorlineFlightControllerSubsystem::FilterAxisValue(Raw, *Binding)
                : Normalized;
            const bool bCenteredAxis = Binding ? Binding->bCentered : false;
            const int32 CalibratedPercent = FMath::RoundToInt(Calibrated * 100.0f);
            const float Y = BodyY + (140.0f + (Axis - FirstCalibrationAxis) * 52.0f) * Scale;
            const bool Detected = Axis == Controller->GetControlsDetectedAxis();
            DrawText(FString::Printf(TEXT("AXIS %d%s"), Axis + 1, Detected ? TEXT("  //  DETECTED") : TEXT("")),
                Detected ? Amber : Muted, BodyX + 38.0f * Scale, Y, GEngine->GetSmallFont(), 0.76f * Scale);
            const float BarX = BodyX + 230.0f * Scale;
            const float BarW = BodyW - 570.0f * Scale;
            DrawRect(FLinearColor(0.03f, 0.09f, 0.09f, 1.0f), BarX, Y, BarW, 18.0f * Scale);
            if (bCenteredAxis)
            {
                const float FillX = Calibrated >= 0.0f
                    ? BarX + BarW * 0.5f : BarX + BarW * 0.5f * (1.0f + Calibrated);
                DrawRect(Detected ? Amber : Cyan, FillX, Y,
                    BarW * 0.5f * FMath::Abs(Calibrated), 18.0f * Scale);
                DrawRect(White, BarX + BarW * 0.5f - 1.0f * Scale, Y,
                    2.0f * Scale, 18.0f * Scale);
            }
            else
            {
                DrawRect(Detected ? Amber : Cyan, BarX, Y,
                    BarW * FMath::Clamp(Calibrated, 0.0f, 1.0f), 18.0f * Scale);
            }
            DrawText(FString::Printf(TEXT("RAW %.3f  //  CAL %s%d%%"), Raw,
                CalibratedPercent > 0 && bCenteredAxis ? TEXT("+") : TEXT(""), CalibratedPercent),
                Detected ? Amber : White, BodyX + BodyW - 310.0f * Scale, Y,
                GEngine->GetSmallFont(), 0.69f * Scale);
        }
        if (AxisCount > VisibleCalibrationAxes)
        {
            DrawText(FString::Printf(TEXT("SHOWING AXES %d-%d OF %d // DETECTED AXIS STAYS VISIBLE"),
                FirstCalibrationAxis + 1, LastCalibrationAxis, AxisCount), Muted,
                BodyX + 38.0f * Scale, BodyY + 108.0f * Scale, GEngine->GetSmallFont(), 0.64f * Scale);
        }
    }
    else if (Mode == ERotorlineControlsMode::ButtonBinding)
    {
        DrawText(FString::Printf(TEXT("BIND %s  //  CONTROL %d OF 15"),
            *Controller->GetControlsWizardAction().ToString().ToUpper(), Controller->GetControlsWizardStep() + 1),
            Amber, BodyX + 32.0f * Scale, BodyY + 28.0f * Scale, GEngine->GetLargeFont(), 0.95f * Scale);
        DrawText(FString::Printf(TEXT("CURRENT ASSIGNMENT // %s"),
            *Controller->GetControlsWizardCurrentBinding()), Cyan,
            BodyX + 34.0f * Scale, BodyY + 82.0f * Scale, GEngine->GetSmallFont(), 0.86f * Scale);
        DrawText(TEXT("PRESS A NEW JOYSTICK BUTTON, HAT DIRECTION, OR TRIGGER TO REPLACE IT."),
            White, BodyX + 34.0f * Scale, BodyY + 126.0f * Scale, GEngine->GetSmallFont(), 0.78f * Scale);
        DrawText(Controller->GetControlsCaptureFeedback(), Amber,
            BodyX + 34.0f * Scale, BodyY + 174.0f * Scale, GEngine->GetSmallFont(), 0.76f * Scale);
        DrawText(TEXT("X / ENTER  KEEP CURRENT     R  CLEAR CURRENT     CIRCLE / ESC  EXIT WITHOUT SAVING"),
            Muted, BodyX + 34.0f * Scale, BodyY + 220.0f * Scale, GEngine->GetSmallFont(), 0.72f * Scale);
    }
    else if (Mode == ERotorlineControlsMode::AxisTuning)
    {
        DrawText(TEXT("LIVE AXIS TUNING"), Amber, BodyX + 32.0f * Scale, BodyY + 28.0f * Scale,
            GEngine->GetLargeFont(), 1.0f * Scale);
        const TArray<FRotorlineAxisBinding>& Axes = Controller->GetWorkingControllerProfile().AxisBindings;
        URotorlineFlightControllerSubsystem* Input = Controller->GetGameInstance()
            ? Controller->GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
        constexpr int32 VisibleTuningRows = 5;
        const int32 FirstTuningRow = FMath::Clamp(SelectedRow - VisibleTuningRows / 2, 0,
            FMath::Max(0, Axes.Num() - VisibleTuningRows));
        const int32 LastTuningRow = FMath::Min(Axes.Num(), FirstTuningRow + VisibleTuningRows);
        for (int32 Row = FirstTuningRow; Row < LastTuningRow; ++Row)
        {
            const FRotorlineAxisBinding& Axis = Axes[Row];
            const float Y = BodyY + (92.0f + (Row - FirstTuningRow) * 58.0f) * Scale;
            if (Row == SelectedRow) DrawRect(FLinearColor(0.08f, 0.20f, 0.20f, 1.0f),
                BodyX + 28.0f * Scale, Y - 7.0f * Scale, BodyW - 56.0f * Scale, 48.0f * Scale);
            DrawText(FString::Printf(TEXT("%s  //  AXIS %d  //  %s"),
                *(Axis.Action == RotorlineFlightControllerActions::Collective
                    ? FString(TEXT("THROTTLE LEVER")) : Axis.Action.ToString().ToUpper()), Axis.NativeAxisIndex + 1,
                Axis.bIgnore ? TEXT("DISABLED") : TEXT("ENABLED")),
                Row == SelectedRow ? White : Muted,
                BodyX + 46.0f * Scale, Y, GEngine->GetSmallFont(), 0.75f * Scale);
            DrawText(FString::Printf(TEXT("DZ %d%%   SENS %.2f   CURVE %.2f   SCALE %.2f   CENTER %+.2f   %s"),
                FMath::RoundToInt(Axis.Deadzone * 100.0f), Axis.Sensitivity, Axis.CurveExponent,
                Axis.Scale, Axis.CenterOffset, Axis.bInvert ? TEXT("INVERTED") : TEXT("NORMAL")),
                Row == SelectedRow ? Cyan : Muted, BodyX + 390.0f * Scale, Y,
                GEngine->GetSmallFont(), 0.69f * Scale);
            float Raw = 0.0f;
            if (Input && Input->GetRawAxisValue(Controller->GetControlsDeviceId(), Axis.NativeAxisIndex, Raw))
            {
                const float Filtered = URotorlineFlightControllerSubsystem::FilterAxisValue(Raw, Axis);
                const float BarX = BodyX + 390.0f * Scale;
                const float BarY = Y + 24.0f * Scale;
                const float BarW = BodyW - 450.0f * Scale;
                DrawRect(FLinearColor(0.03f, 0.09f, 0.09f, 1.0f), BarX, BarY, BarW, 8.0f * Scale);
                const float FillX = Filtered >= 0.0f
                    ? BarX + BarW * 0.5f : BarX + BarW * 0.5f * (1.0f + Filtered);
                DrawRect(Cyan, FillX, BarY,
                    BarW * 0.5f * FMath::Abs(Filtered), 8.0f * Scale);
                DrawRect(Amber, BarX + BarW * 0.5f - 1.0f * Scale, BarY,
                    2.0f * Scale, 8.0f * Scale);
            }
        }
        if (Axes.IsValidIndex(SelectedRow))
        {
            const FRotorlineAxisBinding& SelectedAxis = Axes[SelectedRow];
            const float SliderX = BodyX + 250.0f * Scale;
            const float SliderW = BodyW - 320.0f * Scale;
            const float DeadzoneY = BodyY + 344.0f * Scale;
            const float SensitivityY = BodyY + 374.0f * Scale;
            const FString SelectedInputStatus = SelectedAxis.Action == RotorlineFlightControllerActions::Collective
                ? FString::Printf(TEXT("LOGITECH THROTTLE LEVER // AXIS %d // %s // M TO TOGGLE // Q/E REMAINS ACTIVE"),
                    SelectedAxis.NativeAxisIndex + 1, SelectedAxis.bIgnore ? TEXT("DISABLED") : TEXT("ENABLED"))
                : FString::Printf(TEXT("SELECTED AXIS // %s // M TO ENABLE OR DISABLE"),
                    SelectedAxis.bIgnore ? TEXT("DISABLED") : TEXT("ENABLED"));
            DrawText(SelectedInputStatus, SelectedAxis.bIgnore ? Amber : Cyan,
                BodyX + 34.0f * Scale, BodyY + 316.0f * Scale,
                GEngine->GetSmallFont(), 0.68f * Scale);
            DrawText(FString::Printf(TEXT("DEAD ZONE  %d%%"),
                FMath::RoundToInt(SelectedAxis.Deadzone * 100.0f)), Muted,
                BodyX + 34.0f * Scale, DeadzoneY - 2.0f * Scale,
                GEngine->GetSmallFont(), 0.66f * Scale);
            DrawRect(FLinearColor(0.03f, 0.09f, 0.09f, 1.0f), SliderX, DeadzoneY, SliderW, 14.0f * Scale);
            DrawRect(Cyan, SliderX, DeadzoneY,
                SliderW * FMath::Clamp(SelectedAxis.Deadzone / 0.45f, 0.0f, 1.0f), 14.0f * Scale);
            DrawText(FString::Printf(TEXT("SENSITIVITY  %.2fX"), SelectedAxis.Sensitivity), Muted,
                BodyX + 34.0f * Scale, SensitivityY - 2.0f * Scale,
                GEngine->GetSmallFont(), 0.66f * Scale);
            DrawRect(FLinearColor(0.03f, 0.09f, 0.09f, 1.0f), SliderX, SensitivityY, SliderW, 14.0f * Scale);
            DrawRect(Amber, SliderX, SensitivityY,
                SliderW * FMath::Clamp((SelectedAxis.Sensitivity - 0.1f) / 3.9f, 0.0f, 1.0f), 14.0f * Scale);
        }
        if (Axes.Num() > VisibleTuningRows)
        {
            DrawText(FString::Printf(TEXT("AXES %d-%d OF %d"), FirstTuningRow + 1, LastTuningRow, Axes.Num()),
                Muted, BodyX + BodyW - 190.0f * Scale, BodyY + 34.0f * Scale,
                GEngine->GetSmallFont(), 0.64f * Scale);
        }
        DrawText(TEXT("LEFT/RIGHT DEADZONE   L1/R1 SENSITIVITY   L2/R2 OR Q/E SCALE   L3/R3 OR Z/C CENTER"),
            White, BodyX + 34.0f * Scale, BodyY + 405.0f * Scale, GEngine->GetSmallFont(), 0.70f * Scale);
        DrawText(TEXT("M ENABLE/DISABLE AXIS   SQUARE/I INVERT   TRIANGLE CURVE   N NAME AUXILIARY   R RESET   X DONE"),
            White, BodyX + 34.0f * Scale, BodyY + 432.0f * Scale, GEngine->GetSmallFont(), 0.70f * Scale);
    }
    else if (Mode == ERotorlineControlsMode::LiveTest && Controller->GetSelectedControlsTab() == 1)
    {
        DrawText(TEXT("PS5 FLIGHT CONTROL MAP // LIVE INPUT"), Amber,
            BodyX + 32.0f * Scale, BodyY + 24.0f * Scale,
            GEngine->GetLargeFont(), 0.98f * Scale);
        DrawText(TEXT("GAMEPAD FLIGHT AXES ARE FIXED AND DO NOT USE THE HOTAS CALIBRATION WIZARD."), Muted,
            BodyX + 34.0f * Scale, BodyY + 68.0f * Scale,
            GEngine->GetSmallFont(), 0.70f * Scale);

        const float RawPitch = Controller->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
        const float Pitch = Controller->IsGamepadPitchInverted() ? -RawPitch : RawPitch;
        const float Roll = Controller->GetInputAnalogKeyState(EKeys::Gamepad_RightX);
        const float Yaw = Controller->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
        const float Ascend = Controller->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis);
        const float Descend = Controller->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
        const TCHAR* Labels[] = {
            TEXT("PITCH / FORWARD-REVERSE TILT  //  LEFT STICK Y"),
            TEXT("LATERAL DRIFT  //  RIGHT STICK X"),
            TEXT("YAW / TURN  //  LEFT STICK X"),
            TEXT("ASCEND  //  R2"),
            TEXT("DESCEND  //  L2")
        };
        const float Values[] = { Pitch, Roll, Yaw, Ascend, Descend };
        const float BarX = BodyX + 500.0f * Scale;
        const float BarW = BodyW - 570.0f * Scale;
        for (int32 Row = 0; Row < UE_ARRAY_COUNT(Labels); ++Row)
        {
            const float Y = BodyY + (116.0f + Row * 58.0f) * Scale;
            const bool bCentered = Row < 3;
            const float Value = bCentered
                ? FMath::Clamp(Values[Row], -1.0f, 1.0f)
                : FMath::Clamp(Values[Row], 0.0f, 1.0f);
            DrawText(Labels[Row], White, BodyX + 38.0f * Scale, Y,
                GEngine->GetSmallFont(), 0.73f * Scale);
            DrawRect(FLinearColor(0.03f, 0.09f, 0.09f, 1.0f),
                BarX, Y, BarW, 18.0f * Scale);
            if (bCentered)
            {
                const float FillX = Value >= 0.0f
                    ? BarX + BarW * 0.5f
                    : BarX + BarW * 0.5f * (1.0f + Value);
                DrawRect(Cyan, FillX, Y, BarW * 0.5f * FMath::Abs(Value), 18.0f * Scale);
                DrawRect(White, BarX + BarW * 0.5f - 1.0f * Scale,
                    Y, 2.0f * Scale, 18.0f * Scale);
                DrawText(FString::Printf(TEXT("%+d%%"), FMath::RoundToInt(Value * 100.0f)),
                    Cyan, BodyX + BodyW - 48.0f * Scale, Y,
                    GEngine->GetSmallFont(), 0.68f * Scale);
            }
            else
            {
                DrawRect(Cyan, BarX, Y, BarW * Value, 18.0f * Scale);
                DrawText(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.0f)),
                    Cyan, BodyX + BodyW - 48.0f * Scale, Y,
                    GEngine->GetSmallFont(), 0.68f * Scale);
            }
        }
        DrawText(TEXT("LEFT Y = PITCH     LEFT X = YAW     RIGHT X = LATERAL     R2 = ASCEND     L2 = DESCEND"),
            Cyan, BodyX + 38.0f * Scale, BodyY + 420.0f * Scale,
            GEngine->GetSmallFont(), 0.76f * Scale);
        DrawText(TEXT("CIRCLE / ESC  RETURN TO SETUP MAP"), Muted,
            BodyX + 38.0f * Scale, BodyY + 458.0f * Scale,
            GEngine->GetSmallFont(), 0.70f * Scale);
    }
    else if (Mode == ERotorlineControlsMode::LiveTest)
    {
        DrawText(TEXT("LIVE INPUT TEST // GAMEPLAY SUPPRESSED"), Amber, BodyX + 32.0f * Scale,
            BodyY + 28.0f * Scale, GEngine->GetLargeFont(), 0.98f * Scale);
        URotorlineFlightControllerSubsystem* Input = Controller->GetGameInstance()
            ? Controller->GetGameInstance()->GetSubsystem<URotorlineFlightControllerSubsystem>() : nullptr;
        const FRotorlineControllerDeviceInfo* Device = Input ? Input->GetDevices().FindByPredicate([Controller](const FRotorlineControllerDeviceInfo& Entry)
        {
            return Entry.DeviceId == Controller->GetControlsDeviceId();
        }) : nullptr;
        if (Input && Device)
        {
            constexpr int32 AxesPerPage = 7;
            constexpr int32 ButtonsPerPage = 18;
            constexpr int32 HatsPerPage = 4;
            const int32 PageCount = FMath::Max3(
                FMath::DivideAndRoundUp(Device->Capabilities.AxisCount, AxesPerPage),
                FMath::DivideAndRoundUp(Device->Capabilities.ButtonCount, ButtonsPerPage),
                FMath::DivideAndRoundUp(Device->Capabilities.HatCount, HatsPerPage));
            const int32 Page = FMath::Clamp(Controller->GetControlsLiveTestPage(), 0, FMath::Max(0, PageCount - 1));
            DrawText(FString::Printf(TEXT("PAGE %d OF %d // LEFT/RIGHT CHANGE PAGE"), Page + 1, FMath::Max(1, PageCount)),
                Cyan, BodyX + BodyW - 360.0f * Scale, BodyY + 34.0f * Scale,
                GEngine->GetSmallFont(), 0.66f * Scale);
            const int32 FirstAxis = Page * AxesPerPage;
            const int32 LastAxis = FMath::Min(Device->Capabilities.AxisCount, FirstAxis + AxesPerPage);
            for (int32 Axis = FirstAxis; Axis < LastAxis; ++Axis)
            {
                float Raw = 0.0f;
                Input->GetRawAxisValue(Device->DeviceId, Axis, Raw);
                const float Y = BodyY + (92.0f + (Axis - FirstAxis) * 45.0f) * Scale;
                const FRotorlineAxisBinding* Binding = Controller->GetWorkingControllerProfile().AxisBindings.FindByPredicate(
                    [Axis](const FRotorlineAxisBinding& Entry) { return Entry.NativeAxisIndex == Axis; });
                const float Filtered = Binding
                    ? URotorlineFlightControllerSubsystem::FilterAxisValue(Raw, *Binding) : 0.0f;
                const FString BindingName = Binding
                    ? (Binding->Action == RotorlineFlightControllerActions::Collective
                        ? FString(TEXT("THROTTLE")) : Binding->Action.ToString().ToUpper())
                    : FString(TEXT("UNASSIGNED"));
                const int32 CalibratedPercent = FMath::RoundToInt(Filtered * 100.0f);
                DrawText(FString::Printf(TEXT("AXIS %d  //  %s  //  RAW %.3f  //  CAL %s%d%%"),
                    Axis + 1, *BindingName, Raw,
                    CalibratedPercent > 0 && Binding && Binding->bCentered ? TEXT("+") : TEXT(""),
                    CalibratedPercent), Muted,
                    BodyX + 40.0f * Scale, Y, GEngine->GetSmallFont(), 0.70f * Scale);
                DrawRect(FLinearColor(0.03f, 0.09f, 0.09f, 1.0f), BodyX + 200.0f * Scale, Y,
                    360.0f * Scale, 14.0f * Scale);
                const float BarX = BodyX + 200.0f * Scale;
                const float BarW = 360.0f * Scale;
                if (Binding && Binding->bCentered)
                {
                    const float FillX = Filtered >= 0.0f
                        ? BarX + BarW * 0.5f : BarX + BarW * 0.5f * (1.0f + Filtered);
                    DrawRect(Cyan, FillX, Y, BarW * 0.5f * FMath::Abs(Filtered), 14.0f * Scale);
                    DrawRect(Amber, BarX + BarW * 0.5f - 1.0f * Scale, Y,
                        2.0f * Scale, 14.0f * Scale);
                }
                else
                {
                    DrawRect(Cyan, BarX, Y,
                        BarW * FMath::Clamp(Filtered, 0.0f, 1.0f), 14.0f * Scale);
                }
            }
            const int32 FirstButton = Page * ButtonsPerPage;
            const int32 LastButton = FMath::Min(Device->Capabilities.ButtonCount, FirstButton + ButtonsPerPage);
            for (int32 Button = FirstButton; Button < LastButton; ++Button)
            {
                const bool Pressed = Input->IsRawButtonPressed(Device->DeviceId, Button);
                const FRotorlineButtonBinding* Binding = Controller->GetWorkingControllerProfile().ButtonBindings.FindByPredicate(
                    [Button](const FRotorlineButtonBinding& Entry) { return Entry.NativeButtonIndex == Button; });
                const int32 PageButton = Button - FirstButton;
                const int32 Column = PageButton % 3;
                const int32 Row = PageButton / 3;
                const float X = BodyX + (620.0f + Column * 175.0f) * Scale;
                const float Y = BodyY + (86.0f + Row * 42.0f) * Scale;
                DrawRect(Pressed ? Amber : FLinearColor(0.04f, 0.12f, 0.12f, 1.0f),
                    X, Y, 164.0f * Scale, 32.0f * Scale);
                DrawText(FString::Printf(TEXT("B%d  %s"), Button + 1,
                    Binding ? *Binding->Action.ToString().ToUpper() : TEXT("--")),
                    Pressed ? FLinearColor(0.01f, 0.04f, 0.04f, 1.0f) : Muted,
                    X + 7.0f * Scale, Y + 8.0f * Scale,
                    GEngine->GetSmallFont(), 0.57f * Scale);
            }
            const int32 FirstHat = Page * HatsPerPage;
            const int32 LastHat = FMath::Min(Device->Capabilities.HatCount, FirstHat + HatsPerPage);
            for (int32 HatIndex = FirstHat; HatIndex < LastHat; ++HatIndex)
            {
                float Hat = -1.0f;
                Input->GetRawHatAngle(Device->DeviceId, HatIndex, Hat);
                FString Direction(TEXT("CENTERED"));
                if (Hat >= 0.0f)
                {
                    const float Wrapped = FMath::Fmod(Hat + 360.0f, 360.0f);
                    Direction = (Wrapped >= 315.0f || Wrapped < 45.0f) ? TEXT("UP") :
                        Wrapped < 135.0f ? TEXT("RIGHT") : Wrapped < 225.0f ? TEXT("DOWN") : TEXT("LEFT");
                }
                DrawText(FString::Printf(TEXT("HAT %d // %s // %.0f DEG"), HatIndex + 1,
                    *Direction, Hat), Hat >= 0.0f ? Amber : Cyan,
                    BodyX + 620.0f * Scale, BodyY + (350.0f + (HatIndex - FirstHat) * 25.0f) * Scale,
                    GEngine->GetSmallFont(), 0.72f * Scale);
            }
        }
        DrawText(Controller->GetControlsCaptureFeedback(), Amber,
            BodyX + 34.0f * Scale, BodyY + 432.0f * Scale, GEngine->GetSmallFont(), 0.70f * Scale);
        DrawText(TEXT("PRESS ANY BUTTON TO IDENTIFY ITS CURRENT FUNCTION // LEFT/RIGHT PAGE // X DONE"),
            White, BodyX + 34.0f * Scale, BodyY + 460.0f * Scale, GEngine->GetSmallFont(), 0.70f * Scale);
    }
    else if (Controller->GetSelectedControlsTab() == 0)
    {
        DrawText(TEXT("KEYBOARD + MOUSE // ALWAYS AVAILABLE"), Amber, BodyX + 32.0f * Scale, BodyY + 28.0f * Scale,
            GEngine->GetLargeFont(), 0.95f * Scale);
        const float CardY = BodyY + 82.0f * Scale;
        const float CardGap = 16.0f * Scale;
        const float CardW = (BodyW - 88.0f * Scale) / 3.0f;
        const float CardH = 150.0f * Scale;
        const FLinearColor CardColor(0.035f, 0.12f, 0.13f, 0.94f);
        DrawRect(CardColor, BodyX + 28.0f * Scale, CardY, CardW, CardH);
        DrawRect(CardColor, BodyX + 28.0f * Scale + CardW + CardGap, CardY, CardW, CardH);
        DrawRect(CardColor, BodyX + 28.0f * Scale + (CardW + CardGap) * 2.0f, CardY, CardW, CardH);
        DrawText(TEXT("FLIGHT"), Amber, BodyX + 46.0f * Scale, CardY + 18.0f * Scale,
            GEngine->GetSmallFont(), 0.80f * Scale);
        DrawText(TEXT("[W] / [S]   PITCH\n[A] / [D]   ROLL\n[Q] DOWN   [E] UP\n[SHIFT] BOOST"), White,
            BodyX + 46.0f * Scale, CardY + 50.0f * Scale, GEngine->GetSmallFont(), 0.72f * Scale);
        const float WeaponsX = BodyX + 46.0f * Scale + CardW + CardGap;
        DrawText(TEXT("WEAPONS + VIEW"), Amber, WeaponsX, CardY + 18.0f * Scale,
            GEngine->GetSmallFont(), 0.80f * Scale);
        DrawText(TEXT("[LEFT MOUSE] PRIMARY\n[RIGHT MOUSE] SECONDARY\n[V] OPTIC / VIEW\n[Z/C] YAW LEFT / RIGHT"), White,
            WeaponsX, CardY + 50.0f * Scale, GEngine->GetSmallFont(), 0.72f * Scale);
        const float UtilityX = BodyX + 46.0f * Scale + (CardW + CardGap) * 2.0f;
        DrawText(TEXT("UTILITY"), Amber, UtilityX, CardY + 18.0f * Scale,
            GEngine->GetSmallFont(), 0.80f * Scale);
        DrawText(TEXT("[F] INTERACT\n[TAB] MOUSE CAPTURE\n[M] / [ESC] PAUSE"), White,
            UtilityX, CardY + 50.0f * Scale, GEngine->GetSmallFont(), 0.72f * Scale);
        const TCHAR* KeyboardOptions[] = { TEXT("BACK") };
        for (int32 Row = 0; Row < UE_ARRAY_COUNT(KeyboardOptions); ++Row)
        {
            const float Y = BodyY + (260.0f + Row * 58.0f) * Scale;
            if (Row == SelectedRow)
            {
                DrawRect(FLinearColor(0.08f, 0.20f, 0.20f, 1.0f), BodyX + 28.0f * Scale,
                    Y, BodyW - 56.0f * Scale, 46.0f * Scale);
                DrawRect(Amber, BodyX + 28.0f * Scale, Y, 5.0f * Scale, 46.0f * Scale);
            }
            DrawText(KeyboardOptions[Row], Row == SelectedRow ? White : Muted,
                BodyX + 48.0f * Scale, Y + 12.0f * Scale, GEngine->GetSmallFont(), 0.78f * Scale);
        }
    }
    else if (Controller->GetSelectedControlsTab() == 1)
    {
        DrawText(TEXT("SETUP MAP // COMPLETE STEPS 1-5 IN ORDER"), Amber, BodyX + 32.0f * Scale, BodyY + 22.0f * Scale,
            GEngine->GetLargeFont(), 0.92f * Scale);
        DrawText(TEXT("CONTROLLER PRESET // NOTHING IS SAVED OR APPLIED UNTIL STEP 5"), Muted,
            BodyX + 40.0f * Scale, BodyY + 50.0f * Scale, GEngine->GetSmallFont(), 0.64f * Scale);
        const FString PitchDirection = Controller->IsGamepadPitchInverted()
            ? TEXT("PITCH DIRECTION // INVERTED: FORWARD STICK = NOSE UP")
            : TEXT("PITCH DIRECTION // STANDARD: FORWARD STICK = NOSE DOWN");
        const FString Options[] = {
            TEXT("STEP 1 // SELECT CONNECTED GAMEPAD"),
            TEXT("STEP 2 // VERIFY LEFT STICK, RIGHT STICK + R2/L2"),
            TEXT("STEP 3 // BIND ACTION BUTTONS + D-PAD"),
            TEXT("STEP 4 // TEST EVERY CONTROL"),
            TEXT("STEP 5 // SAVE AND RETURN"),
            PitchDirection,
            TEXT("RESTORE SAFE DEFAULTS // SAVE WITH STEP 5"),
            TEXT("BACK WITHOUT SAVING")
        };
        for (int32 Row = 0; Row < UE_ARRAY_COUNT(Options); ++Row)
        {
            const float Y = BodyY + (76.0f + Row * 44.0f) * Scale;
            if (Row == SelectedRow)
            {
                DrawRect(FLinearColor(0.08f, 0.20f, 0.20f, 1.0f), BodyX + 26.0f * Scale,
                    Y - 5.0f * Scale, BodyW - 52.0f * Scale, 39.0f * Scale);
                DrawRect(Amber, BodyX + 26.0f * Scale, Y - 5.0f * Scale, 5.0f * Scale, 39.0f * Scale);
            }
            DrawText(Options[Row], Row == SelectedRow ? White : Muted,
                BodyX + 48.0f * Scale, Y + 5.0f * Scale, GEngine->GetSmallFont(), 0.76f * Scale);
        }
    }
    else
    {
        DrawText(TEXT("SETUP MAP // COMPLETE STEPS 1-5 IN ORDER // PRESS R TO RESCAN DEVICES"), Cyan,
            BodyX + 40.0f * Scale, BodyY + 14.0f * Scale, GEngine->GetSmallFont(), 0.68f * Scale);
        DrawText(TEXT("NOTHING IS PERMANENT UNTIL STEP 5 // BACK RESTORES THE PREVIOUS SAVED PROFILE"), Muted,
            BodyX + 40.0f * Scale, BodyY + 38.0f * Scale, GEngine->GetSmallFont(), 0.66f * Scale);
        const TCHAR* Options[] = {
            TEXT("STEP 1 // SELECT CONNECTED FLIGHT CONTROLLER"),
            TEXT("STEP 2 // CALIBRATE PITCH, ROLL, YAW + THROTTLE"),
            TEXT("STEP 3 // BIND BUTTONS, TRIGGERS + HAT"),
            TEXT("STEP 4 // TEST EVERY CONTROL"),
            TEXT("STEP 5 // SAVE AND RETURN"),
            TEXT("ADVANCED // TUNE AXES"),
            TEXT("RESTORE SAFE DEFAULTS // SAVE WITH STEP 5"),
            TEXT("BACK WITHOUT SAVING")
        };
        for (int32 Row = 0; Row < UE_ARRAY_COUNT(Options); ++Row)
        {
            const float Y = BodyY + (76.0f + Row * 44.0f) * Scale;
            if (Row == SelectedRow)
            {
                DrawRect(FLinearColor(0.08f, 0.20f, 0.20f, 1.0f), BodyX + 28.0f * Scale, Y,
                    BodyW - 56.0f * Scale, 39.0f * Scale);
                DrawRect(Amber, BodyX + 28.0f * Scale, Y, 5.0f * Scale, 39.0f * Scale);
            }
            DrawText(Options[Row], Row == SelectedRow ? White : Muted, BodyX + 48.0f * Scale,
                Y + 9.0f * Scale, GEngine->GetSmallFont(), 0.78f * Scale);
        }
    }

    DrawText(TEXT("LEFT / RIGHT  CATEGORY     D-PAD  SELECT     X / ENTER  CONFIRM     R  RESCAN     ESC  BACK"),
        White, PanelX + 38.0f * Scale, PanelY + PanelH - 42.0f * Scale,
        GEngine->GetSmallFont(), 0.70f * Scale);
}

void ARotorlineOperationsHUD::DrawWrappedText(
    const FString& Text,
    float X,
    float Y,
    float MaxWidth,
    float TextScale,
    const FLinearColor& Color,
    int32 MaxLines)
{
    if (!Canvas || !GEngine || Text.IsEmpty() || MaxLines <= 0)
    {
        return;
    }

    UFont* Font = GEngine->GetMediumFont();
    TArray<FString> Words;
    Text.ParseIntoArrayWS(Words);
    FString Line;
    int32 LineIndex = 0;
    for (int32 WordIndex = 0; WordIndex < Words.Num(); ++WordIndex)
    {
        const FString Candidate = Line.IsEmpty() ? Words[WordIndex] : Line + TEXT(" ") + Words[WordIndex];
        float CandidateWidth = 0.0f;
        float CandidateHeight = 0.0f;
        Canvas->StrLen(Font, Candidate, CandidateWidth, CandidateHeight);
        CandidateWidth *= TextScale;
        if (!Line.IsEmpty() && CandidateWidth > MaxWidth)
        {
            if (LineIndex >= MaxLines - 1)
            {
                FString Truncated = Line + TEXT("...");
                float TruncatedWidth = 0.0f;
                float TruncatedHeight = 0.0f;
                Canvas->StrLen(Font, Truncated, TruncatedWidth, TruncatedHeight);
                while (Truncated.Len() > 4 && TruncatedWidth * TextScale > MaxWidth)
                {
                    Truncated = Truncated.LeftChop(4) + TEXT("...");
                    Canvas->StrLen(Font, Truncated, TruncatedWidth, TruncatedHeight);
                }
                DrawText(Truncated, Color, X, Y + LineIndex * 22.0f * TextScale, Font, TextScale);
                return;
            }
            DrawText(Line, Color, X, Y + LineIndex * 22.0f * TextScale, Font, TextScale);
            ++LineIndex;
            Line = Words[WordIndex];
        }
        else
        {
            Line = Candidate;
        }
    }

    if (!Line.IsEmpty() && LineIndex < MaxLines)
    {
        DrawText(Line, Color, X, Y + LineIndex * 22.0f * TextScale, Font, TextScale);
    }
}

void ARotorlineOperationsHUD::DrawDigitalGaugeCell(
    const FString& Label,
    const FString& Value,
    const FString& Unit,
    float X,
    float Y,
    float Width,
    float Height,
    float LayoutScale,
    float TextScale,
    const FLinearColor& Accent,
    bool bWarning)
{
    const FLinearColor Background = bWarning
        ? FLinearColor(0.13f, 0.018f, 0.008f, 0.72f)
        : FLinearColor(0.01f, 0.035f, 0.040f, 0.68f);
    DrawRect(Background, X, Y, Width, Height);
    DrawLine(X, Y, X + Width, Y, FLinearColor(Accent.R, Accent.G, Accent.B, 0.72f), 1.2f * LayoutScale);
    DrawLine(X, Y + Height, X + Width, Y + Height, FLinearColor(Accent.R, Accent.G, Accent.B, 0.42f), 1.0f * LayoutScale);
    // TextScale intentionally has a 1.0 readability floor while LayoutScale can
    // shrink on 720p/short viewports. Keep vertical typography spacing tied to
    // TextScale so the label never collapses onto the digital value below it.
    const float LabelY = Y + 4.0f * LayoutScale;
    const float ValueY = Y + 16.0f * TextScale;
    const float UnitY = Y + Height - 12.0f * TextScale;
    DrawText(Label, FLinearColor(0.62f, 0.80f, 0.80f), X + 8.0f * LayoutScale, LabelY,
        GEngine->GetSmallFont(), 0.72f * TextScale);
    DrawText(Value, Accent, X + 8.0f * LayoutScale, ValueY,
        GEngine->GetLargeFont(), 0.92f * TextScale);
    DrawText(Unit, FLinearColor(0.66f, 0.80f, 0.80f), X + Width - 42.0f * LayoutScale, UnitY,
        GEngine->GetSmallFont(), 0.68f * TextScale);
}

void ARotorlineOperationsHUD::DrawTextBlock(const FString& Text, float X, float Y, float MaxWidth, float Scale, const FLinearColor& Color)
{
    if (!Canvas || Text.IsEmpty())
    {
        return;
    }

    const int32 ApproxCharacters = FMath::Max(22, FMath::FloorToInt(MaxWidth / (8.0f * Scale)));
    TArray<FString> Words;
    Text.ParseIntoArrayWS(Words);
    FString Line;
    int32 LineIndex = 0;
    for (const FString& Word : Words)
    {
        if (!Line.IsEmpty() && Line.Len() + Word.Len() + 1 > ApproxCharacters)
        {
            DrawText(Line, Color, X, Y + LineIndex * 25.0f * Scale, GEngine->GetSmallFont(), Scale);
            Line = Word;
            ++LineIndex;
        }
        else
        {
            if (!Line.IsEmpty())
            {
                Line += TEXT(" ");
            }
            Line += Word;
        }
    }
    if (!Line.IsEmpty())
    {
        DrawText(Line, Color, X, Y + LineIndex * 25.0f * Scale, GEngine->GetSmallFont(), Scale);
    }
}
