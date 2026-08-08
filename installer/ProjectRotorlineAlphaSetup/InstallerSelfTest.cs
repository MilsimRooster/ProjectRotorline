using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Threading;

namespace ProjectRotorlineAlphaSetup
{
    internal static class InstallerSelfTest
    {
        private const string SelfTestSwitch = "--self-test";
        private const string SurvivalProbeSwitch = "--survival-probe";

        public static bool TryRunCommand(string[] args)
        {
            if (args != null &&
                args.Length == 1 &&
                string.Equals(
                    args[0],
                    SurvivalProbeSwitch,
                    StringComparison.Ordinal))
            {
                Thread.Sleep(15000);
                return true;
            }

            if (args == null ||
                args.Length != 2 ||
                !string.Equals(
                    args[0],
                    SelfTestSwitch,
                    StringComparison.Ordinal))
            {
                return false;
            }

            string resultPath = Path.GetFullPath(args[1]);
            try
            {
                Run();
                File.WriteAllText(
                    resultPath,
                    "PASS: manifest, three-part UCAS, SHA-256 rejection, " +
                    "sibling swap rollback, shortcut readback, and exact " +
                    "launch survival checks passed.",
                    new UTF8Encoding(false));
                Environment.ExitCode = 0;
            }
            catch (Exception exception)
            {
                File.WriteAllText(
                    resultPath,
                    "FAIL: " + exception,
                    new UTF8Encoding(false));
                Environment.ExitCode = 1;
            }

            return true;
        }

        private static void Run()
        {
            string root = Path.Combine(
                Path.GetTempPath(),
                "ProjectRotorlineInstallerSelfTest-" +
                Guid.NewGuid().ToString("N"));
            string coreSource = Path.Combine(root, "core-source");
            string cachePath = Path.Combine(root, "download-cache");
            string stagingPath = Path.Combine(root, "install.staging");
            string installPath = Path.Combine(root, "install");
            string rollbackPath = Path.Combine(root, "install.rollback");
            string shortcutPath = Path.Combine(root, "Project Rotorline Alpha.lnk");
            Process probe = null;

            try
            {
                Directory.CreateDirectory(coreSource);
                Directory.CreateDirectory(cachePath);
                CreateCoreFixture(coreSource);

                string coreArchive = Path.Combine(
                    cachePath,
                    "Rotorline-Alpha-Windows-Core.zip");
                ZipFile.CreateFromDirectory(
                    coreSource,
                    coreArchive,
                    CompressionLevel.Optimal,
                    false);

                List<PayloadFileSpec> parts = CreateUcasParts(cachePath);
                string expectedUcas = Path.Combine(root, "expected.ucas");
                ConcatenateParts(cachePath, parts, expectedUcas);

                ReleaseManifest manifest = CreateManifest(
                    coreSource,
                    coreArchive,
                    parts,
                    expectedUcas);
                string manifestPath = Path.Combine(root, "manifest.json");
                ReleaseManifestIo.Save(manifestPath, manifest);
                manifest = ReleaseManifestIo.Load(manifestPath);
                ReleaseManifestValidator.Validate(
                    manifest,
                    ReleaseConfiguration.ReleaseTag);

                InstallerEngine.VerifyFile(
                    coreArchive,
                    manifest.CoreArchive.Size,
                    manifest.CoreArchive.Sha256);
                foreach (PayloadFileSpec part in manifest.Ucas.Parts)
                {
                    InstallerEngine.VerifyFile(
                        Path.Combine(cachePath, part.Name),
                        part.Size,
                        part.Sha256);
                }

                VerifyHashTamperingIsRejected(
                    Path.Combine(cachePath, manifest.Ucas.Parts[0].Name),
                    manifest.Ucas.Parts[0]);

                InstallerEngine.ExtractCoreArchive(
                    coreArchive,
                    stagingPath,
                    manifest.Ucas.RelativePath);
                InstallerEngine.AssembleUcas(
                    cachePath,
                    stagingPath,
                    manifest.Ucas);
                InstallerEngine.ValidateInstalledFiles(stagingPath, manifest);

                Directory.CreateDirectory(installPath);
                File.WriteAllText(
                    Path.Combine(installPath, "previous-install.marker"),
                    "previous",
                    new UTF8Encoding(false));

                InstallerEngine.AtomicInstallSwap swap =
                    new InstallerEngine.AtomicInstallSwap(
                        stagingPath,
                        installPath,
                        rollbackPath);
                swap.Activate();
                Require(
                    File.Exists(Path.Combine(installPath, "Rotorline.exe")),
                    "Atomic activation did not expose the verified install.");
                swap.Rollback();
                Require(
                    File.Exists(
                        Path.Combine(
                            installPath,
                            "previous-install.marker")),
                    "Rollback did not restore the previous installation.");
                Require(
                    Directory.Exists(stagingPath),
                    "Rollback did not recover the verified staging tree.");

                swap.Activate();
                ShortcutManager.CreateAndVerify(installPath, shortcutPath);
                string executable = Path.Combine(installPath, "Rotorline.exe");
                probe = InstallerEngine.LaunchAndVerifyProcess(
                    executable,
                    installPath,
                    SurvivalProbeSwitch,
                    1200);
                InstallerEngine.TryTerminateProcess(probe);
                probe.Dispose();
                probe = null;
                swap.Complete();

                Require(
                    File.Exists(shortcutPath),
                    "Shortcut verification did not leave the shortcut in place.");
                Require(
                    !Directory.Exists(rollbackPath),
                    "Successful completion left a rollback directory behind.");
            }
            finally
            {
                if (probe != null)
                {
                    InstallerEngine.TryTerminateProcess(probe);
                    probe.Dispose();
                }

                InstallerEngine.TryDeleteDirectory(root);
            }
        }

        private static void CreateCoreFixture(string root)
        {
            string currentExecutable =
                Process.GetCurrentProcess().MainModule.FileName;
            File.Copy(
                currentExecutable,
                Path.Combine(root, "Rotorline.exe"));

            WriteFixtureFile(
                root,
                InstallerEngine.PakRelativePath,
                "synthetic pak fixture");
            WriteFixtureFile(
                root,
                InstallerEngine.UtocRelativePath,
                "synthetic utoc fixture");
            WriteFixtureFile(
                root,
                InstallerEngine.VcRedistRelativePath,
                "synthetic VC redist fixture - never executed");
            WriteFixtureFile(
                root,
                InstallerEngine.GameInputRelativePath,
                "synthetic GameInput fixture - never executed");
        }

        private static void WriteFixtureFile(
            string root,
            string relativePath,
            string content)
        {
            string path = InstallerEngine.ResolveUnderRoot(root, relativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, content, new UTF8Encoding(false));
        }

        private static List<PayloadFileSpec> CreateUcasParts(string cachePath)
        {
            byte[][] contents =
            {
                Encoding.UTF8.GetBytes("UCAS-PART-ONE-"),
                Encoding.UTF8.GetBytes("UCAS-PART-TWO-"),
                Encoding.UTF8.GetBytes("UCAS-PART-THREE")
            };
            List<PayloadFileSpec> parts = new List<PayloadFileSpec>();
            for (int index = 0; index < contents.Length; index++)
            {
                string name = "Rotorline-Windows.ucas.part" + (index + 1);
                string path = Path.Combine(cachePath, name);
                File.WriteAllBytes(path, contents[index]);
                parts.Add(CreatePayloadSpec(path));
            }

            return parts;
        }

        private static void ConcatenateParts(
            string cachePath,
            List<PayloadFileSpec> parts,
            string targetPath)
        {
            using (FileStream output = File.Create(targetPath))
            {
                foreach (PayloadFileSpec part in parts)
                {
                    using (FileStream input = File.OpenRead(
                        Path.Combine(cachePath, part.Name)))
                    {
                        input.CopyTo(output);
                    }
                }
            }
        }

        private static ReleaseManifest CreateManifest(
            string coreSource,
            string coreArchive,
            List<PayloadFileSpec> parts,
            string expectedUcas)
        {
            ReleaseManifest manifest = new ReleaseManifest();
            manifest.SchemaVersion = 1;
            manifest.ReleaseTag = ReleaseConfiguration.ReleaseTag;
            manifest.RequiredFreeBytes = 1024L * 1024L;
            manifest.CoreArchive = CreatePayloadSpec(coreArchive);
            manifest.Ucas = new UcasPayloadSpec();
            manifest.Ucas.RelativePath = InstallerEngine.UcasRelativePath;
            manifest.Ucas.AssembledSize = new FileInfo(expectedUcas).Length;
            manifest.Ucas.AssembledSha256 =
                InstallerEngine.ComputeSha256(expectedUcas);
            manifest.Ucas.Parts = parts;
            manifest.InstalledFiles = new List<InstalledFileSpec>();
            manifest.InstalledFiles.Add(CreateInstalledSpec(
                "gameExe",
                coreSource,
                InstallerEngine.GameExeRelativePath));
            manifest.InstalledFiles.Add(CreateInstalledSpec(
                "pak",
                coreSource,
                InstallerEngine.PakRelativePath));
            manifest.InstalledFiles.Add(CreateInstalledSpec(
                "utoc",
                coreSource,
                InstallerEngine.UtocRelativePath));
            manifest.InstalledFiles.Add(new InstalledFileSpec
            {
                Key = "ucas",
                RelativePath = InstallerEngine.UcasRelativePath,
                Size = manifest.Ucas.AssembledSize,
                Sha256 = manifest.Ucas.AssembledSha256
            });
            manifest.InstalledFiles.Add(CreateInstalledSpec(
                "vcRedistX64",
                coreSource,
                InstallerEngine.VcRedistRelativePath));
            manifest.InstalledFiles.Add(CreateInstalledSpec(
                "gameInput",
                coreSource,
                InstallerEngine.GameInputRelativePath));
            return manifest;
        }

        private static PayloadFileSpec CreatePayloadSpec(string path)
        {
            FileInfo file = new FileInfo(path);
            return new PayloadFileSpec
            {
                Name = file.Name,
                Size = file.Length,
                Sha256 = InstallerEngine.ComputeSha256(path)
            };
        }

        private static InstalledFileSpec CreateInstalledSpec(
            string key,
            string root,
            string relativePath)
        {
            string path = InstallerEngine.ResolveUnderRoot(root, relativePath);
            FileInfo file = new FileInfo(path);
            return new InstalledFileSpec
            {
                Key = key,
                RelativePath = relativePath,
                Size = file.Length,
                Sha256 = InstallerEngine.ComputeSha256(path)
            };
        }

        private static void VerifyHashTamperingIsRejected(
            string partPath,
            PayloadFileSpec expected)
        {
            byte[] original = File.ReadAllBytes(partPath);
            bool rejected = false;
            try
            {
                File.WriteAllBytes(
                    partPath,
                    Encoding.UTF8.GetBytes("tampered payload"));
                try
                {
                    InstallerEngine.VerifyFile(
                        partPath,
                        expected.Size,
                        expected.Sha256);
                }
                catch (InvalidDataException)
                {
                    rejected = true;
                }
            }
            finally
            {
                File.WriteAllBytes(partPath, original);
            }

            Require(rejected, "A tampered payload was not rejected.");
        }

        private static void Require(bool condition, string message)
        {
            if (!condition)
            {
                throw new InvalidOperationException(message);
            }
        }
    }
}
