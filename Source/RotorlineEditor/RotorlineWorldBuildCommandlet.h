#pragma once

#include "Commandlets/Commandlet.h"
#include "RotorlineWorldBuildCommandlet.generated.h"

UCLASS()
class URotorlineWorldBuildCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    URotorlineWorldBuildCommandlet();
    virtual int32 Main(const FString& Params) override;
};
