#pragma once

#include "Commandlets/Commandlet.h"
#include "RotorlineBellLairCarveCommandlet.generated.h"

UCLASS()
class URotorlineBellLairCarveCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    URotorlineBellLairCarveCommandlet();
    virtual int32 Main(const FString& Params) override;
};
