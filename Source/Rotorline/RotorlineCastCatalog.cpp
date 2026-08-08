#include "RotorlineCastCatalog.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString CastStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        FString Value;
        if (Object.IsValid())
        {
            Object->TryGetStringField(Name, Value);
        }
        return Value;
    }
}

bool FRotorlineCastCatalog::Load(TArray<FRotorlineCastMember>& OutMembers, FString& OutError)
{
    OutMembers.Reset();
    OutError.Reset();

    const FString CatalogPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Cast.json"));
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *CatalogPath))
    {
        OutError = FString::Printf(TEXT("Could not read %s"), *CatalogPath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Cast catalog JSON is invalid");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* MemberValues = nullptr;
    if (!Root->TryGetArrayField(TEXT("cast"), MemberValues) || !MemberValues)
    {
        OutError = TEXT("Cast catalog has no cast array");
        return false;
    }

    TSet<FString> SeenIds;
    for (const TSharedPtr<FJsonValue>& MemberValue : *MemberValues)
    {
        const TSharedPtr<FJsonObject> Object = MemberValue.IsValid() ? MemberValue->AsObject() : nullptr;
        if (!Object.IsValid())
        {
            continue;
        }

        FRotorlineCastMember Member;
        Member.Id = CastStringField(Object, TEXT("id"));
        Member.Callsign = CastStringField(Object, TEXT("callsign"));
        Member.Role = CastStringField(Object, TEXT("role"));
        Member.CardAsset = CastStringField(Object, TEXT("cardAsset"));
        Member.VoiceAsset = CastStringField(Object, TEXT("voiceAsset"));
        const FString NormalizedId = Member.Id.ToLower();

        if (Member.Id.IsEmpty() || Member.Callsign.IsEmpty() || Member.Role.IsEmpty() ||
            Member.CardAsset.IsEmpty() || Member.VoiceAsset.IsEmpty() || SeenIds.Contains(NormalizedId))
        {
            continue;
        }

        SeenIds.Add(NormalizedId);
        OutMembers.Add(MoveTemp(Member));
    }

    if (OutMembers.IsEmpty())
    {
        OutError = TEXT("Cast catalog contained no usable members");
        return false;
    }
    return true;
}
