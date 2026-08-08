using UnrealBuildTool;
using System.Collections.Generic;

public class RotorlineEditorTarget : TargetRules
{
    public RotorlineEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new[] { "Rotorline", "RotorlineEditor" });
    }
}
