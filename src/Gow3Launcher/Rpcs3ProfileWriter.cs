namespace Gow3Launcher;

internal sealed record ProfileWriteResult(string ProfilePath, string? BackupPath);

internal static class Rpcs3ProfileWriter
{
    private const string TitleId = "BCES00510";

    internal static string GetProfilePath(string emulatorPath)
    {
        if (!File.Exists(emulatorPath) || !Path.GetFileName(emulatorPath).Equals("rpcs3.exe", StringComparison.OrdinalIgnoreCase))
            throw new FileNotFoundException("Select a valid rpcs3.exe before writing the profile.", emulatorPath);

        var emulatorDirectory = Path.GetDirectoryName(emulatorPath)!;
        var adjacentConfig = Path.Combine(emulatorDirectory, "config");
        var appDataRoot = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "RPCS3");
        var appDataNestedConfig = Path.Combine(appDataRoot, "config");
        var configDirectory = Directory.Exists(adjacentConfig)
            ? adjacentConfig
            : File.Exists(Path.Combine(appDataNestedConfig, "config.yml")) || Directory.Exists(Path.Combine(appDataNestedConfig, "custom_configs"))
                ? appDataNestedConfig
                : appDataRoot;
        var customDirectory = Path.Combine(configDirectory, "custom_configs");
        return Path.Combine(customDirectory, $"config_{TitleId}.yml");
    }

    internal static ProfileWriteResult WriteProfile(string emulatorPath, int resolutionScalePercent)
    {
        if (resolutionScalePercent is < 25 or > 100)
            throw new ArgumentOutOfRangeException(nameof(resolutionScalePercent), "Scale must be between 25% and 100%.");
        var profilePath = GetProfilePath(emulatorPath);
        Directory.CreateDirectory(Path.GetDirectoryName(profilePath)!);
        string? backupPath = null;

        var yaml = $"""
            Video:
              Renderer: Vulkan
              Resolution: 1280x720
              Resolution Scale: {resolutionScalePercent}
              Vulkan:
                Adapter: AMD Radeon(TM) Vega 8 Graphics
            """;

        if (File.Exists(profilePath))
        {
            if (File.ReadAllText(profilePath).Equals(yaml, StringComparison.Ordinal))
                return new ProfileWriteResult(profilePath, null);
            backupPath = profilePath + $".gow3ps3pc-{Guid.NewGuid():N}.bak";
            File.Copy(profilePath, backupPath, overwrite: false);
        }

        File.WriteAllText(profilePath, yaml, new System.Text.UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
        return new ProfileWriteResult(profilePath, backupPath);
    }
}
