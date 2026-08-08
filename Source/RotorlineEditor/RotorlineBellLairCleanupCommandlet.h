#pragma once

#include "Commandlets/Commandlet.h"
#include "RotorlineBellLairCleanupCommandlet.generated.h"

UCLASS()
class URotorlineBellLairCleanupCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    URotorlineBellLairCleanupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
