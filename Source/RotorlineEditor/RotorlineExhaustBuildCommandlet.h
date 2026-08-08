#pragma once

#include "Commandlets/Commandlet.h"
#include "RotorlineExhaustBuildCommandlet.generated.h"

UCLASS()
class URotorlineExhaustBuildCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    URotorlineExhaustBuildCommandlet();
    virtual int32 Main(const FString& Params) override;
};
