using UnrealBuildTool;
using System.Collections.Generic;

public class RotorlineTarget : TargetRules
{
    public RotorlineTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        // The installed UE binary engine uses a shared Shipping environment;
        // project-side Shipping logging would be ABI-incompatible with its
        // precompiled log categories. The startup check uses a runtime probe
        // file plus exact-process survival and any runtime log that is present.
        ExtraModuleNames.Add("Rotorline");
    }
}
