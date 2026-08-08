using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;

namespace ProjectRotorlineAlphaSetup
{
    [DataContract]
    internal sealed class ReleaseManifest
    {
        [DataMember(Name = "schemaVersion", IsRequired = true)]
        public int SchemaVersion { get; set; }

        [DataMember(Name = "releaseTag", IsRequired = true)]
        public string ReleaseTag { get; set; }

        [DataMember(Name = "requiredFreeBytes", IsRequired = true)]
        public long RequiredFreeBytes { get; set; }

        [DataMember(Name = "coreArchive", IsRequired = true)]
        public PayloadFileSpec CoreArchive { get; set; }

        [DataMember(Name = "ucas", IsRequired = true)]
        public UcasPayloadSpec Ucas { get; set; }

        [DataMember(Name = "installedFiles", IsRequired = true)]
        public List<InstalledFileSpec> InstalledFiles { get; set; }
    }

    [DataContract]
    internal sealed class PayloadFileSpec
    {
        [DataMember(Name = "name", IsRequired = true)]
        public string Name { get; set; }

        [DataMember(Name = "size", IsRequired = true)]
        public long Size { get; set; }

        [DataMember(Name = "sha256", IsRequired = true)]
        public string Sha256 { get; set; }
    }

    [DataContract]
    internal sealed class UcasPayloadSpec
    {
        [DataMember(Name = "relativePath", IsRequired = true)]
        public string RelativePath { get; set; }

        [DataMember(Name = "assembledSize", IsRequired = true)]
        public long AssembledSize { get; set; }

        [DataMember(Name = "assembledSha256", IsRequired = true)]
        public string AssembledSha256 { get; set; }

        [DataMember(Name = "parts", IsRequired = true)]
        public List<PayloadFileSpec> Parts { get; set; }
    }

    [DataContract]
    internal sealed class InstalledFileSpec
    {
        [DataMember(Name = "key", IsRequired = true)]
        public string Key { get; set; }

        [DataMember(Name = "relativePath", IsRequired = true)]
        public string RelativePath { get; set; }

        [DataMember(Name = "size", IsRequired = true)]
        public long Size { get; set; }

        [DataMember(Name = "sha256", IsRequired = true)]
        public string Sha256 { get; set; }
    }

    internal static class ReleaseManifestIo
    {
        public static ReleaseManifest Load(string path)
        {
            using (FileStream stream = File.OpenRead(path))
            {
                DataContractJsonSerializer serializer =
                    new DataContractJsonSerializer(typeof(ReleaseManifest));
                ReleaseManifest manifest =
                    serializer.ReadObject(stream) as ReleaseManifest;
                if (manifest == null)
                {
                    throw new InvalidDataException(
                        "The release manifest could not be decoded.");
                }

                return manifest;
            }
        }

        public static void Save(string path, ReleaseManifest manifest)
        {
            DataContractJsonSerializer serializer =
                new DataContractJsonSerializer(typeof(ReleaseManifest));
            using (MemoryStream buffer = new MemoryStream())
            {
                serializer.WriteObject(buffer, manifest);
                string json = Encoding.UTF8.GetString(buffer.ToArray());
                File.WriteAllText(path, json, new UTF8Encoding(false));
            }
        }
    }
}
