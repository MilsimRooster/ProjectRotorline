using UnrealBuildTool;

public class Rotorline : ModuleRules
{
    public Rotorline(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Landscape",
            "MeshDescription",
            "MeshConversion",
            "StaticMeshDescription",
            "InputCore",
            "EnhancedInput",
            "SlateCore",
            "AssetRegistry",
            "AudioMixer",
            "MediaAssets",
            "MoviePlayer",
            "Json",
            "JsonUtilities",
            "Niagara"
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // GameInput provides arbitrary controller axes/buttons/switches and
            // stable HID identity. WinMM remains as a compatibility fallback.
            PublicDependencyModuleNames.Add("GameInputWindowsLibrary");
            PublicSystemLibraries.Add("winmm.lib");
        }
    }
}
