#pragma once

namespace RotorlineCombatTuning
{
    // The airfield must be a real launch sanctuary, not merely a no-spawn
    // marker. This gives a newly started aircraft roughly 1.1 km to lift,
    // accelerate, and choose its departure before hostile fire is enabled.
    constexpr float HomeSanctuaryRadiusCm = 110000.0f;

    // Keep route-defense spawns far enough outside the sanctuary that even
    // the longest-ranged tuned weapon leaves a 200 m transition corridor.
    constexpr float GroundDefenseAirfieldExclusionRadiusCm = 220000.0f;

    constexpr float FlakRangeMeters = 550.0f;
    constexpr float ReconFlakRangeMeters = 850.0f;
    constexpr float TankRangeMeters = 500.0f;
    constexpr float RocketArtilleryRangeMeters = 900.0f;
    constexpr float RadarMissileRangeMeters = 800.0f;
    constexpr float MachineGunshipRangeMeters = 750.0f;
    constexpr float RocketGunshipRangeMeters = 850.0f;
}
