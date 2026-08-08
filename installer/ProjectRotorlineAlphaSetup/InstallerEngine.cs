using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace ProjectRotorlineAlphaSetup
{
    internal sealed class InstallerProgress
    {
        public InstallerProgress(string status, string detail, int permille)
        {
            Status = status;
            Detail = detail;
            Permille = Math.Max(0, Math.Min(1000, permille));
        }

        public string Status { get; private set; }
        public string Detail { get; private set; }
        public int Permille { get; private set; }
    }

    internal sealed class InstallerEngine
    {
        internal const string GameExeRelativePath = "Rotorline.exe";
        internal const string PakRelativePath =
            "Rotorline\\Content\\Paks\\Rotorline-Windows.pak";
        internal const string UtocRelativePath =
            "Rotorline\\Content\\Paks\\Rotorline-Windows.utoc";
        internal const string UcasRelativePath =
            "Rotorline\\Content\\Paks\\Rotorline-Windows.ucas";
        internal const string VcRedistRelativePath =
            "Prerequisites\\VC_redist.x64.exe";
        internal const string GameInputRelativePath =
            "Prerequisites\\GameInputRedist.msi";
        internal const int LaunchSurvivalMilliseconds = 8000;

        private const string CoreArchiveName =
            "Rotorline-Alpha-Windows-Core.zip";

        private readonly string _releaseTag;
        private readonly string _releaseBaseUrl;

        public InstallerEngine(string releaseTag, string releaseBaseUrl)
        {
            _releaseTag = releaseTag;
            _releaseBaseUrl = releaseBaseUrl;
        }

        public async Task InstallAsync(Action<InstallerProgress> report)
        {
            string installPath = GetInstallPath();
            string parentPath = Path.GetDirectoryName(installPath);
            Directory.CreateDirectory(parentPath);

            string operationId = Guid.NewGuid().ToString("N");
            string cachePath = Path.Combine(
                parentPath,
                "ProjectRotorlineAlpha.download-" + operationId);
            string stagingPath = Path.Combine(
                parentPath,
                "ProjectRotorlineAlpha.staging-" + operationId);
            string rollbackPath = Path.Combine(
                parentPath,
                "ProjectRotorlineAlpha.rollback-" + operationId);
            string shortcutPath = GetDesktopShortcutPath();
            string shortcutBackupPath = Path.Combine(
                cachePath,
                "previous-desktop-shortcut.lnk");

            Directory.CreateDirectory(cachePath);
            AtomicInstallSwap swap = null;
            Process launchedProcess = null;
            bool shortcutExistedBefore = false;

            try
            {
                Report(report, "Reading release manifest...", _releaseTag, 0);
                string manifestPath = Path.Combine(
                    cachePath,
                    ReleaseConfiguration.ManifestFileName);
                await DownloadUnverifiedFileAsync(
                    BuildReleaseAssetUri(
                        ReleaseConfiguration.ManifestFileName),
                    manifestPath);

                ReleaseManifest manifest = ReleaseManifestIo.Load(manifestPath);
                ReleaseManifestValidator.Validate(manifest, _releaseTag);
                EnsureDiskSpace(installPath, manifest.RequiredFreeBytes);

                long totalDownloadBytes = manifest.CoreArchive.Size;
                foreach (PayloadFileSpec part in manifest.Ucas.Parts)
                {
                    checked
                    {
                        totalDownloadBytes += part.Size;
                    }
                }

                long completedDownloadBytes = 0;
                await DownloadPayloadAsync(
                    manifest.CoreArchive,
                    cachePath,
                    completedDownloadBytes,
                    totalDownloadBytes,
                    report);
                completedDownloadBytes += manifest.CoreArchive.Size;

                foreach (PayloadFileSpec part in manifest.Ucas.Parts)
                {
                    await DownloadPayloadAsync(
                        part,
                        cachePath,
                        completedDownloadBytes,
                        totalDownloadBytes,
                        report);
                    completedDownloadBytes += part.Size;
                }

                Report(
                    report,
                    "Staging verified game files...",
                    "Extracting the core archive beside the live install",
                    1000);
                await Task.Run(delegate
                {
                    ExtractCoreArchive(
                        Path.Combine(cachePath, manifest.CoreArchive.Name),
                        stagingPath,
                        manifest.Ucas.RelativePath);
                    AssembleUcas(cachePath, stagingPath, manifest.Ucas);
                    ValidateInstalledFiles(stagingPath, manifest);
                });

                Report(
                    report,
                    "Installing prerequisites...",
                    "Microsoft Visual C++ x64 runtime",
                    1000);
                await Task.Run(delegate
                {
                    InstallPrerequisites(stagingPath);
                });

                Report(
                    report,
                    "Activating verified installation...",
                    "Existing install remains available for rollback",
                    1000);
                swap = new AtomicInstallSwap(
                    stagingPath,
                    installPath,
                    rollbackPath);
                await Task.Run(delegate { swap.Activate(); });

                Report(
                    report,
                    "Creating desktop shortcut...",
                    "Project Rotorline Alpha",
                    1000);
                shortcutExistedBefore = File.Exists(shortcutPath);
                if (shortcutExistedBefore)
                {
                    File.Copy(shortcutPath, shortcutBackupPath, true);
                }
                await Task.Run(delegate
                {
                    ShortcutManager.CreateAndVerify(
                        installPath,
                        shortcutPath);
                });

                Report(
                    report,
                    "Launching verified game...",
                    "Confirming Rotorline.exe remains running",
                    1000);
                string executable = ResolveUnderRoot(
                    installPath,
                    GameExeRelativePath);
                launchedProcess = LaunchAndVerifyProcess(
                    executable,
                    installPath,
                    string.Empty,
                    LaunchSurvivalMilliseconds);

                swap.Complete();
                launchedProcess.Dispose();
                launchedProcess = null;

                Report(
                    report,
                    "Installation complete",
                    "Project Rotorline Alpha is running",
                    1000);
            }
            catch
            {
                if (launchedProcess != null)
                {
                    TryTerminateProcess(launchedProcess);
                    launchedProcess.Dispose();
                }

                if (swap != null)
                {
                    swap.Rollback();
                }

                RestoreShortcutAfterFailure(
                    shortcutPath,
                    shortcutBackupPath,
                    shortcutExistedBefore);

                throw;
            }
            finally
            {
                TryDeleteDirectory(cachePath);
                TryDeleteDirectory(stagingPath);
            }
        }

        internal Uri BuildReleaseAssetUri(string assetName)
        {
            ReleaseManifestValidator.ValidateAssetName(assetName);
            return new Uri(
                _releaseBaseUrl + Uri.EscapeDataString(assetName),
                UriKind.Absolute);
        }

        private async Task DownloadPayloadAsync(
            PayloadFileSpec payload,
            string cachePath,
            long completedDownloadBytes,
            long totalDownloadBytes,
            Action<InstallerProgress> report)
        {
            string destination = Path.Combine(cachePath, payload.Name);
            Report(
                report,
                "Downloading " + payload.Name,
                FormatBytes(completedDownloadBytes) +
                    " of " + FormatBytes(totalDownloadBytes),
                ScaleProgress(completedDownloadBytes, totalDownloadBytes));

            using (WebClient client = CreateWebClient())
            {
                client.DownloadProgressChanged += delegate(
                    object sender,
                    DownloadProgressChangedEventArgs progress)
                {
                    long downloaded = completedDownloadBytes +
                        progress.BytesReceived;
                    Report(
                        report,
                        "Downloading " + payload.Name,
                        FormatBytes(downloaded) +
                            " of " + FormatBytes(totalDownloadBytes),
                        ScaleProgress(downloaded, totalDownloadBytes));
                };

                await client.DownloadFileTaskAsync(
                    BuildReleaseAssetUri(payload.Name),
                    destination);
            }

            VerifyFile(destination, payload.Size, payload.Sha256);
        }

        private static async Task DownloadUnverifiedFileAsync(
            Uri uri,
            string destination)
        {
            using (WebClient client = CreateWebClient())
            {
                await client.DownloadFileTaskAsync(uri, destination);
            }

            FileInfo file = new FileInfo(destination);
            if (!file.Exists || file.Length == 0 || file.Length > 4L * 1024L * 1024L)
            {
                throw new InvalidDataException(
                    "The release manifest download was empty or unexpectedly large.");
            }
        }

        private static WebClient CreateWebClient()
        {
            WebClient client = new WebClient();
            client.Headers.Add(
                HttpRequestHeader.UserAgent,
                "ProjectRotorlineAlphaSetup/2.0");
            return client;
        }

        internal static void ExtractCoreArchive(
            string archivePath,
            string stagingPath,
            string ucasRelativePath)
        {
            if (Directory.Exists(stagingPath))
            {
                throw new IOException(
                    "The sibling staging directory already exists: " +
                    stagingPath);
            }

            Directory.CreateDirectory(stagingPath);
            string normalizedUcas = NormalizeRelativePath(ucasRelativePath);

            using (ZipArchive archive = ZipFile.OpenRead(archivePath))
            {
                foreach (ZipArchiveEntry entry in archive.Entries)
                {
                    RejectSymbolicLink(entry);
                    string relativePath = NormalizeRelativePath(entry.FullName);
                    if (string.Equals(
                        relativePath,
                        normalizedUcas,
                        StringComparison.OrdinalIgnoreCase))
                    {
                        throw new InvalidDataException(
                            "The core archive must not contain the split UCAS.");
                    }

                    string destination = ResolveUnderRoot(
                        stagingPath,
                        relativePath);
                    if (string.IsNullOrEmpty(entry.Name))
                    {
                        Directory.CreateDirectory(destination);
                        continue;
                    }

                    string destinationDirectory =
                        Path.GetDirectoryName(destination);
                    Directory.CreateDirectory(destinationDirectory);
                    entry.ExtractToFile(destination, false);
                }
            }
        }

        internal static void AssembleUcas(
            string cachePath,
            string stagingPath,
            UcasPayloadSpec ucas)
        {
            string targetPath = ResolveUnderRoot(
                stagingPath,
                ucas.RelativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(targetPath));

            using (FileStream output = new FileStream(
                targetPath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None))
            {
                foreach (PayloadFileSpec part in ucas.Parts)
                {
                    string partPath = Path.Combine(cachePath, part.Name);
                    VerifyFile(partPath, part.Size, part.Sha256);
                    using (FileStream input = File.OpenRead(partPath))
                    {
                        input.CopyTo(output);
                    }
                }

                output.Flush(true);
            }

            VerifyFile(
                targetPath,
                ucas.AssembledSize,
                ucas.AssembledSha256);
        }

        internal static void ValidateInstalledFiles(
            string stagingPath,
            ReleaseManifest manifest)
        {
            foreach (InstalledFileSpec required in manifest.InstalledFiles)
            {
                string path = ResolveUnderRoot(
                    stagingPath,
                    required.RelativePath);
                FileInfo file = new FileInfo(path);
                if (!file.Exists)
                {
                    throw new FileNotFoundException(
                        "Required installed file is missing: " +
                        required.RelativePath,
                        path);
                }

                if ((file.Attributes & FileAttributes.ReparsePoint) != 0)
                {
                    throw new InvalidDataException(
                        "Required installed file cannot be a reparse point: " +
                        required.RelativePath);
                }

                VerifyFile(path, required.Size, required.Sha256);
            }
        }

        internal static void InstallPrerequisites(string stagingPath)
        {
            string vcRedist = ResolveUnderRoot(
                stagingPath,
                VcRedistRelativePath);
            RunElevatedInstaller(
                vcRedist,
                "/install /quiet /norestart",
                "Microsoft Visual C++ x64 runtime");

            string gameInput = ResolveUnderRoot(
                stagingPath,
                GameInputRelativePath);
            string msiexec = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.Windows),
                "System32",
                "msiexec.exe");
            RunElevatedInstaller(
                msiexec,
                "/i \"" + gameInput + "\" /qn /norestart",
                "Microsoft GameInput runtime");
        }

        private static void RunElevatedInstaller(
            string executable,
            string arguments,
            string displayName)
        {
            if (!File.Exists(executable))
            {
                throw new FileNotFoundException(
                    displayName + " installer is missing.",
                    executable);
            }

            ProcessStartInfo startInfo = new ProcessStartInfo(executable);
            startInfo.Arguments = arguments;
            startInfo.UseShellExecute = true;
            startInfo.Verb = "runas";
            startInfo.WindowStyle = ProcessWindowStyle.Hidden;
            startInfo.WorkingDirectory = Path.GetDirectoryName(executable);

            using (Process process = Process.Start(startInfo))
            {
                if (process == null)
                {
                    throw new InvalidOperationException(
                        displayName + " installer did not start.");
                }

                process.WaitForExit();
                int exitCode = process.ExitCode;
                if (exitCode != 0 &&
                    exitCode != 1638 &&
                    exitCode != 1641 &&
                    exitCode != 3010)
                {
                    throw new InvalidOperationException(
                        displayName +
                        " installer failed with exit code " +
                        exitCode + ".");
                }
            }
        }

        internal static Process LaunchAndVerifyProcess(
            string executable,
            string workingDirectory,
            string arguments,
            int survivalMilliseconds)
        {
            string exactExecutable = Path.GetFullPath(executable);
            if (!File.Exists(exactExecutable))
            {
                throw new FileNotFoundException(
                    "Rotorline.exe was not installed.",
                    exactExecutable);
            }

            ProcessStartInfo startInfo = new ProcessStartInfo(exactExecutable);
            startInfo.Arguments = arguments ?? string.Empty;
            startInfo.UseShellExecute = false;
            startInfo.WorkingDirectory = Path.GetFullPath(workingDirectory);

            Process process = Process.Start(startInfo);
            if (process == null)
            {
                throw new InvalidOperationException(
                    "Rotorline.exe did not start.");
            }

            try
            {
                if (process.WaitForExit(survivalMilliseconds))
                {
                    throw new InvalidOperationException(
                        "Rotorline.exe exited during the launch survival check " +
                        "with code " + process.ExitCode + ".");
                }

                string runningExecutable =
                    Path.GetFullPath(process.MainModule.FileName);
                if (!PathsEqual(runningExecutable, exactExecutable))
                {
                    throw new InvalidOperationException(
                        "The launched process does not match the installed " +
                        "Rotorline.exe path.");
                }

                return process;
            }
            catch
            {
                TryTerminateProcess(process);
                process.Dispose();
                throw;
            }
        }

        internal static void VerifyFile(
            string path,
            long expectedSize,
            string expectedSha256)
        {
            FileInfo file = new FileInfo(path);
            if (!file.Exists)
            {
                throw new FileNotFoundException(
                    "Verified file is missing.",
                    path);
            }

            if (file.Length != expectedSize)
            {
                throw new InvalidDataException(
                    file.Name + " has " + file.Length +
                    " bytes; expected " + expectedSize + ".");
            }

            string actualSha256 = ComputeSha256(path);
            if (!string.Equals(
                actualSha256,
                expectedSha256,
                StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    file.Name + " failed SHA-256 verification.");
            }
        }

        internal static string ComputeSha256(string path)
        {
            using (SHA256 sha256 = SHA256.Create())
            using (FileStream stream = File.OpenRead(path))
            {
                byte[] hash = sha256.ComputeHash(stream);
                StringBuilder text = new StringBuilder(hash.Length * 2);
                foreach (byte value in hash)
                {
                    text.Append(value.ToString("x2"));
                }

                return text.ToString();
            }
        }

        internal static string ResolveUnderRoot(
            string rootPath,
            string relativePath)
        {
            string normalized = NormalizeRelativePath(relativePath);
            string root = Path.GetFullPath(rootPath)
                .TrimEnd(Path.DirectorySeparatorChar) +
                Path.DirectorySeparatorChar;
            string candidate = Path.GetFullPath(
                Path.Combine(root, normalized));
            if (!candidate.StartsWith(
                root,
                StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    "A release path escapes the installation root: " +
                    relativePath);
            }

            return candidate;
        }

        internal static string NormalizeRelativePath(string relativePath)
        {
            if (string.IsNullOrWhiteSpace(relativePath))
            {
                throw new InvalidDataException(
                    "Release paths cannot be empty.");
            }

            string normalized = relativePath
                .Replace('/', Path.DirectorySeparatorChar)
                .Replace('\\', Path.DirectorySeparatorChar)
                .TrimStart(Path.DirectorySeparatorChar);
            if (Path.IsPathRooted(relativePath) ||
                normalized.IndexOf(':') >= 0)
            {
                throw new InvalidDataException(
                    "Release paths must be relative: " + relativePath);
            }

            return normalized;
        }

        internal static string GetInstallPath()
        {
            return Path.Combine(
                Environment.GetFolderPath(
                    Environment.SpecialFolder.LocalApplicationData),
                "ProjectRotorlineAlpha");
        }

        internal static string GetDesktopShortcutPath()
        {
            return Path.Combine(
                Environment.GetFolderPath(
                    Environment.SpecialFolder.DesktopDirectory),
                "Project Rotorline Alpha.lnk");
        }

        internal static void EnsureDiskSpace(
            string installPath,
            long manifestRequiredBytes)
        {
            string root = Path.GetPathRoot(installPath);
            if (string.IsNullOrEmpty(root))
            {
                throw new IOException(
                    "The installation drive could not be resolved.");
            }

            long existingInstallBytes = DirectorySize(installPath);
            long requiredBytes;
            checked
            {
                requiredBytes = manifestRequiredBytes + existingInstallBytes;
            }

            DriveInfo drive = new DriveInfo(root);
            if (drive.AvailableFreeSpace < requiredBytes)
            {
                throw new IOException(
                    "Project Rotorline Alpha requires " +
                    FormatBytes(requiredBytes) +
                    " of free disk space for verified staging and rollback.");
            }
        }

        internal static long DirectorySize(string path)
        {
            if (!Directory.Exists(path))
            {
                return 0;
            }

            long total = 0;
            foreach (string filePath in Directory.EnumerateFiles(
                path,
                "*",
                SearchOption.AllDirectories))
            {
                checked
                {
                    total += new FileInfo(filePath).Length;
                }
            }

            return total;
        }

        internal static string FormatBytes(long bytes)
        {
            return string.Format(
                "{0:0.00} GB",
                bytes / 1024d / 1024d / 1024d);
        }

        internal static bool PathsEqual(string first, string second)
        {
            return string.Equals(
                Path.GetFullPath(first).TrimEnd('\\', '/'),
                Path.GetFullPath(second).TrimEnd('\\', '/'),
                StringComparison.OrdinalIgnoreCase);
        }

        internal static void TryDeleteDirectory(string path)
        {
            try
            {
                if (Directory.Exists(path))
                {
                    Directory.Delete(path, true);
                }
            }
            catch
            {
            }
        }

        internal static void TryDeleteFile(string path)
        {
            try
            {
                if (File.Exists(path))
                {
                    File.Delete(path);
                }
            }
            catch
            {
            }
        }

        private static void RestoreShortcutAfterFailure(
            string shortcutPath,
            string backupPath,
            bool existedBefore)
        {
            try
            {
                if (existedBefore && File.Exists(backupPath))
                {
                    File.Copy(backupPath, shortcutPath, true);
                }
                else if (!existedBefore)
                {
                    TryDeleteFile(shortcutPath);
                }
            }
            catch
            {
            }
        }

        internal static void TryTerminateProcess(Process process)
        {
            try
            {
                if (!process.HasExited)
                {
                    process.Kill();
                    process.WaitForExit(5000);
                }
            }
            catch
            {
            }
        }

        private static int ScaleProgress(long current, long total)
        {
            if (total <= 0)
            {
                return 0;
            }

            double scaled = (double)current / (double)total * 1000d;
            return (int)Math.Max(0d, Math.Min(1000d, scaled));
        }

        private static void Report(
            Action<InstallerProgress> report,
            string status,
            string detail,
            int permille)
        {
            if (report != null)
            {
                report(new InstallerProgress(status, detail, permille));
            }
        }

        private static void RejectSymbolicLink(ZipArchiveEntry entry)
        {
            int unixFileType = (entry.ExternalAttributes >> 16) & 0xF000;
            if (unixFileType == 0xA000)
            {
                throw new InvalidDataException(
                    "The core archive contains a symbolic link.");
            }
        }

        private static void TryDeleteRollback(string rollbackPath)
        {
            TryDeleteDirectory(rollbackPath);
        }

        internal sealed class AtomicInstallSwap
        {
            private readonly string _stagingPath;
            private readonly string _installPath;
            private readonly string _rollbackPath;
            private bool _active;
            private bool _hadPreviousInstall;

            public AtomicInstallSwap(
                string stagingPath,
                string installPath,
                string rollbackPath)
            {
                _stagingPath = Path.GetFullPath(stagingPath);
                _installPath = Path.GetFullPath(installPath);
                _rollbackPath = Path.GetFullPath(rollbackPath);

                string parent = Path.GetDirectoryName(_installPath);
                if (!PathsEqual(Path.GetDirectoryName(_stagingPath), parent) ||
                    !PathsEqual(Path.GetDirectoryName(_rollbackPath), parent))
                {
                    throw new InvalidOperationException(
                        "Staging and rollback directories must be siblings " +
                        "of the install directory.");
                }
            }

            public bool HadPreviousInstall
            {
                get { return _hadPreviousInstall; }
            }

            public void Activate()
            {
                if (_active)
                {
                    throw new InvalidOperationException(
                        "The installation swap is already active.");
                }

                if (!Directory.Exists(_stagingPath))
                {
                    throw new DirectoryNotFoundException(
                        "Verified sibling staging directory is missing.");
                }

                if (Directory.Exists(_rollbackPath))
                {
                    throw new IOException(
                        "Rollback directory already exists: " +
                        _rollbackPath);
                }

                _hadPreviousInstall = Directory.Exists(_installPath);
                if (_hadPreviousInstall)
                {
                    RejectReparseDirectory(_installPath);
                    Directory.Move(_installPath, _rollbackPath);
                }

                try
                {
                    Directory.Move(_stagingPath, _installPath);
                    _active = true;
                }
                catch
                {
                    if (_hadPreviousInstall &&
                        Directory.Exists(_rollbackPath) &&
                        !Directory.Exists(_installPath))
                    {
                        Directory.Move(_rollbackPath, _installPath);
                    }

                    throw;
                }
            }

            public void Rollback()
            {
                if (!_active)
                {
                    return;
                }

                if (Directory.Exists(_stagingPath))
                {
                    throw new IOException(
                        "Cannot roll back because the staging path is occupied.");
                }

                if (Directory.Exists(_installPath))
                {
                    Directory.Move(_installPath, _stagingPath);
                }

                if (_hadPreviousInstall && Directory.Exists(_rollbackPath))
                {
                    Directory.Move(_rollbackPath, _installPath);
                }

                _active = false;
            }

            public void Complete()
            {
                if (!_active)
                {
                    throw new InvalidOperationException(
                        "The installation swap is not active.");
                }

                _active = false;
                TryDeleteRollback(_rollbackPath);
            }

            private static void RejectReparseDirectory(string path)
            {
                DirectoryInfo directory = new DirectoryInfo(path);
                if ((directory.Attributes & FileAttributes.ReparsePoint) != 0)
                {
                    throw new InvalidDataException(
                        "The existing install directory cannot be a reparse point.");
                }
            }
        }
    }

    internal static class ReleaseManifestValidator
    {
        private static readonly Dictionary<string, string> ExactInstalledFiles =
            new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                { "gameExe", InstallerEngine.GameExeRelativePath },
                { "pak", InstallerEngine.PakRelativePath },
                { "utoc", InstallerEngine.UtocRelativePath },
                { "ucas", InstallerEngine.UcasRelativePath },
                { "vcRedistX64", InstallerEngine.VcRedistRelativePath },
                { "gameInput", InstallerEngine.GameInputRelativePath }
            };

        public static void Validate(
            ReleaseManifest manifest,
            string expectedReleaseTag)
        {
            if (manifest.SchemaVersion != 1)
            {
                throw new InvalidDataException(
                    "Unsupported release manifest schema.");
            }

            if (!string.Equals(
                manifest.ReleaseTag,
                expectedReleaseTag,
                StringComparison.Ordinal))
            {
                throw new InvalidDataException(
                    "The manifest release tag does not match this setup build.");
            }

            if (manifest.RequiredFreeBytes <= 0)
            {
                throw new InvalidDataException(
                    "requiredFreeBytes must be positive.");
            }

            ValidatePayload(manifest.CoreArchive);
            if (!string.Equals(
                manifest.CoreArchive.Name,
                "Rotorline-Alpha-Windows-Core.zip",
                StringComparison.Ordinal))
            {
                throw new InvalidDataException(
                    "The core archive name is not the release contract name.");
            }

            if (manifest.Ucas == null ||
                manifest.Ucas.Parts == null ||
                manifest.Ucas.Parts.Count == 0)
            {
                throw new InvalidDataException(
                    "The manifest must enumerate at least one UCAS part.");
            }

            if (!string.Equals(
                InstallerEngine.NormalizeRelativePath(
                    manifest.Ucas.RelativePath),
                InstallerEngine.UcasRelativePath,
                StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    "The assembled UCAS path is not the expected game path.");
            }

            if (manifest.Ucas.AssembledSize <= 0)
            {
                throw new InvalidDataException(
                    "The assembled UCAS size must be positive.");
            }

            ValidateSha256(manifest.Ucas.AssembledSha256);

            HashSet<string> assetNames = new HashSet<string>(
                StringComparer.OrdinalIgnoreCase);
            assetNames.Add(manifest.CoreArchive.Name);
            long partSize = 0;
            for (int index = 0; index < manifest.Ucas.Parts.Count; index++)
            {
                PayloadFileSpec part = manifest.Ucas.Parts[index];
                ValidatePayload(part);
                Match match = Regex.Match(
                    part.Name,
                    "^Rotorline-Windows\\.ucas\\.part(?<number>[0-9]+)$",
                    RegexOptions.CultureInvariant);
                int partNumber;
                if (!match.Success ||
                    !int.TryParse(match.Groups["number"].Value, out partNumber) ||
                    partNumber != index + 1)
                {
                    throw new InvalidDataException(
                        "UCAS parts must be ordered and contiguous from part1.");
                }

                if (!assetNames.Add(part.Name))
                {
                    throw new InvalidDataException(
                        "Duplicate release asset name: " + part.Name);
                }

                checked
                {
                    partSize += part.Size;
                }
            }

            if (partSize != manifest.Ucas.AssembledSize)
            {
                throw new InvalidDataException(
                    "UCAS part sizes do not equal the assembled size.");
            }

            long minimumRequiredFreeBytes;
            checked
            {
                minimumRequiredFreeBytes = manifest.CoreArchive.Size +
                    partSize + manifest.Ucas.AssembledSize;
            }
            if (manifest.RequiredFreeBytes < minimumRequiredFreeBytes)
            {
                throw new InvalidDataException(
                    "requiredFreeBytes is smaller than the verified download " +
                    "and staging footprint.");
            }

            if (manifest.InstalledFiles == null ||
                manifest.InstalledFiles.Count != ExactInstalledFiles.Count)
            {
                throw new InvalidDataException(
                    "The manifest must contain exactly the six required " +
                    "installed file records.");
            }

            HashSet<string> installedKeys = new HashSet<string>(
                StringComparer.OrdinalIgnoreCase);
            foreach (InstalledFileSpec installed in manifest.InstalledFiles)
            {
                string expectedPath;
                if (installed == null ||
                    string.IsNullOrWhiteSpace(installed.Key) ||
                    !ExactInstalledFiles.TryGetValue(
                        installed.Key,
                        out expectedPath))
                {
                    throw new InvalidDataException(
                        "The manifest contains an unknown installed file key.");
                }

                if (!installedKeys.Add(installed.Key))
                {
                    throw new InvalidDataException(
                        "Duplicate installed file key: " + installed.Key);
                }

                if (!string.Equals(
                    InstallerEngine.NormalizeRelativePath(
                        installed.RelativePath),
                    expectedPath,
                    StringComparison.OrdinalIgnoreCase))
                {
                    throw new InvalidDataException(
                        "Installed file path mismatch for " +
                        installed.Key + ".");
                }

                if (installed.Size <= 0)
                {
                    throw new InvalidDataException(
                        "Installed file sizes must be positive.");
                }

                ValidateSha256(installed.Sha256);
            }

            InstalledFileSpec ucasInstalled = FindInstalledFile(
                manifest,
                "ucas");
            if (ucasInstalled.Size != manifest.Ucas.AssembledSize ||
                !string.Equals(
                    ucasInstalled.Sha256,
                    manifest.Ucas.AssembledSha256,
                    StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    "Installed UCAS metadata must match assembled UCAS metadata.");
            }
        }

        public static InstalledFileSpec FindInstalledFile(
            ReleaseManifest manifest,
            string key)
        {
            foreach (InstalledFileSpec installed in manifest.InstalledFiles)
            {
                if (string.Equals(
                    installed.Key,
                    key,
                    StringComparison.OrdinalIgnoreCase))
                {
                    return installed;
                }
            }

            throw new InvalidDataException(
                "Missing installed file record: " + key);
        }

        public static void ValidateAssetName(string assetName)
        {
            if (string.IsNullOrWhiteSpace(assetName) ||
                !string.Equals(
                    Path.GetFileName(assetName),
                    assetName,
                    StringComparison.Ordinal) ||
                assetName.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            {
                throw new InvalidDataException(
                    "Release asset names must be plain file names.");
            }
        }

        private static void ValidatePayload(PayloadFileSpec payload)
        {
            if (payload == null)
            {
                throw new InvalidDataException(
                    "The release manifest contains a null payload.");
            }

            ValidateAssetName(payload.Name);
            if (payload.Size <= 0)
            {
                throw new InvalidDataException(
                    "Payload sizes must be positive.");
            }

            ValidateSha256(payload.Sha256);
        }

        private static void ValidateSha256(string sha256)
        {
            if (string.IsNullOrWhiteSpace(sha256) ||
                !Regex.IsMatch(
                    sha256,
                    "^[0-9a-fA-F]{64}$",
                    RegexOptions.CultureInvariant))
            {
                throw new InvalidDataException(
                    "SHA-256 values must contain exactly 64 hexadecimal characters.");
            }
        }
    }

    internal static class ShortcutManager
    {
        public static void CreateAndVerify(
            string installPath,
            string shortcutPath)
        {
            string executable = InstallerEngine.ResolveUnderRoot(
                installPath,
                InstallerEngine.GameExeRelativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(shortcutPath));

            ShellLink shellLink = null;
            try
            {
                shellLink = new ShellLink();
                IShellLinkW link = (IShellLinkW)shellLink;
                link.SetPath(executable);
                link.SetArguments(string.Empty);
                link.SetWorkingDirectory(installPath);
                link.SetDescription("Project Rotorline Alpha");
                link.SetIconLocation(executable, 0);
                ((IPersistFile)link).Save(shortcutPath, false);
            }
            finally
            {
                ReleaseComObject(shellLink);
            }

            VerifyShortcut(shortcutPath, executable, installPath);
        }

        private static void VerifyShortcut(
            string shortcutPath,
            string expectedExecutable,
            string expectedWorkingDirectory)
        {
            ShellLink shellLink = null;
            try
            {
                shellLink = new ShellLink();
                IShellLinkW link = (IShellLinkW)shellLink;
                ((IPersistFile)link).Load(shortcutPath, 0);

                StringBuilder target = new StringBuilder(32768);
                StringBuilder arguments = new StringBuilder(32768);
                StringBuilder workingDirectory = new StringBuilder(32768);
                StringBuilder iconPath = new StringBuilder(32768);
                link.GetPath(target, target.Capacity, IntPtr.Zero, 0);
                link.GetArguments(arguments, arguments.Capacity);
                link.GetWorkingDirectory(
                    workingDirectory,
                    workingDirectory.Capacity);
                int iconIndex;
                link.GetIconLocation(
                    iconPath,
                    iconPath.Capacity,
                    out iconIndex);

                if (!InstallerEngine.PathsEqual(
                    target.ToString(),
                    expectedExecutable) ||
                    !string.IsNullOrEmpty(arguments.ToString()) ||
                    !InstallerEngine.PathsEqual(
                        workingDirectory.ToString(),
                        expectedWorkingDirectory) ||
                    !InstallerEngine.PathsEqual(
                        iconPath.ToString(),
                        expectedExecutable) ||
                    iconIndex != 0)
                {
                    throw new InvalidDataException(
                        "Desktop shortcut readback did not match the exact " +
                        "installed executable contract.");
                }
            }
            finally
            {
                ReleaseComObject(shellLink);
            }
        }

        private static void ReleaseComObject(object value)
        {
            if (value != null && Marshal.IsComObject(value))
            {
                Marshal.FinalReleaseComObject(value);
            }
        }

        [ComImport]
        [Guid("00021401-0000-0000-C000-000000000046")]
        private class ShellLink
        {
        }

        [ComImport]
        [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        [Guid("000214F9-0000-0000-C000-000000000046")]
        private interface IShellLinkW
        {
            void GetPath(
                [Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder file,
                int maxPath,
                IntPtr findData,
                uint flags);
            void GetIDList(out IntPtr itemIdList);
            void SetIDList(IntPtr itemIdList);
            void GetDescription(
                [Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder name,
                int maxName);
            void SetDescription(
                [MarshalAs(UnmanagedType.LPWStr)] string name);
            void GetWorkingDirectory(
                [Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder directory,
                int maxPath);
            void SetWorkingDirectory(
                [MarshalAs(UnmanagedType.LPWStr)] string directory);
            void GetArguments(
                [Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder arguments,
                int maxPath);
            void SetArguments(
                [MarshalAs(UnmanagedType.LPWStr)] string arguments);
            void GetHotkey(out short hotkey);
            void SetHotkey(short hotkey);
            void GetShowCmd(out int showCommand);
            void SetShowCmd(int showCommand);
            void GetIconLocation(
                [Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder iconPath,
                int iconPathLength,
                out int iconIndex);
            void SetIconLocation(
                [MarshalAs(UnmanagedType.LPWStr)] string iconPath,
                int iconIndex);
            void SetRelativePath(
                [MarshalAs(UnmanagedType.LPWStr)] string path,
                uint reserved);
            void Resolve(IntPtr windowHandle, uint flags);
            void SetPath(
                [MarshalAs(UnmanagedType.LPWStr)] string path);
        }

        [ComImport]
        [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        [Guid("0000010B-0000-0000-C000-000000000046")]
        private interface IPersistFile
        {
            void GetClassID(out Guid classId);

            [PreserveSig]
            int IsDirty();

            void Load(
                [MarshalAs(UnmanagedType.LPWStr)] string fileName,
                uint mode);
            void Save(
                [MarshalAs(UnmanagedType.LPWStr)] string fileName,
                [MarshalAs(UnmanagedType.Bool)] bool remember);
            void SaveCompleted(
                [MarshalAs(UnmanagedType.LPWStr)] string fileName);
            void GetCurFile(
                [MarshalAs(UnmanagedType.LPWStr)] out string fileName);
        }
    }
}
