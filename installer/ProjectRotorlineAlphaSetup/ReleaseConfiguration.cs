namespace ProjectRotorlineAlphaSetup
{
    internal static class ReleaseConfiguration
    {
        public const string ReleaseTag = "alpha-windows-v1";
        public const string ReleaseAssetRoot =
            "https://github.com/MilsimRooster/ProjectRotorline/releases/download/";
        public const string ManifestFileName =
            "Rotorline-Alpha-Windows.manifest.json";

        public static string ReleaseBaseUrl
        {
            get { return ReleaseAssetRoot + ReleaseTag + "/"; }
        }
    }
}
