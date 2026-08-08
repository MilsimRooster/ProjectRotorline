using UnrealBuildTool;

public class RotorlineEditor : ModuleRules
{
    public RotorlineEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Rotorline",
            "UnrealEd",
            "Landscape",
            "LandscapeEditor",
            "EditorScriptingUtilities",
            "Foliage",
            "Projects",
            "AssetRegistry",
            "Niagara",
            "NiagaraCore",
            "NiagaraEditor"
        });
    }
}
