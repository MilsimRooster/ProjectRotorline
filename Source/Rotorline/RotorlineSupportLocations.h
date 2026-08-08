#pragma once

#include "CoreMinimal.h"

namespace RotorlineSupportLocations
{
    // Surveyed center of the authored roadside heliport placed in the map.
    // Z is the top of its physical deck, not the underlying landscape. Keeping
    // this exact transform makes the service beacon follow the visible pad.
    inline const FVector CentralTownRearmPad(19011.1f, -25908.1f, 8551.7f);
    // Permanent medical LZ shared by the hospital campus and every mission
    // objective that routes patients to the field hospital.
    inline const FVector FieldHospitalHelipad(222961.0f, -165733.0f, 0.0f);

    // Concealed Bell 222 operations bunker on the island's highest
    // valley-side summit. The runtime lair actor resolves the exact landscape
    // contact Z before it builds the chamber and deploys the aircraft.
    inline const FVector BellLairPeak(54800.0f, 185200.0f, 46810.0f);
    constexpr float BellLairYawDegrees = -107.0f;
    constexpr float BellLairBurialDepthCm = 3800.0f;
    constexpr float BellLairAircraftDeckOffsetCm = 430.0f;

    constexpr float ServiceRadiusCm = 9000.0f;
    constexpr float ServiceResetRadiusCm = 14000.0f;
    constexpr float CityServiceSanctuaryRadiusCm = 12000.0f;
    constexpr float CityDefenseExclusionRadiusCm = 110000.0f;
}
