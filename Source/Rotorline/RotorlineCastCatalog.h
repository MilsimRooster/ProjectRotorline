#pragma once

#include "CoreMinimal.h"

struct FRotorlineCastMember
{
    FString Id;
    FString Callsign;
    FString Role;
    FString CardAsset;
    FString VoiceAsset;
};

class FRotorlineCastCatalog
{
public:
    static bool Load(TArray<FRotorlineCastMember>& OutMembers, FString& OutError);
};
