#pragma once

#include "Commandlets/Commandlet.h"
#include "RotorlineWeatherBuildCommandlet.generated.h"

UCLASS()
class URotorlineWeatherBuildCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    URotorlineWeatherBuildCommandlet();
    virtual int32 Main(const FString& Params) override;
};
