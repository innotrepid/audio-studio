using System;
using System.IO;

namespace AudioStudio;

/// <summary>
/// Centralizes user-visible Synvidra media paths under the Windows Music known folder.
/// App-private drafts and interrupted-recording staging must not use this class.
/// </summary>
internal static class SynvidraStorage
{
    internal enum MediaKind
    {
        Recording,
        Processed,
        Subtitle,
        Export
    }

    internal static string RootDirectory =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyMusic), "Synvidra");

    internal static string GetDirectory(MediaKind kind)
    {
        var folder = kind switch
        {
            MediaKind.Recording => "Recordings",
            MediaKind.Processed => "Processed",
            MediaKind.Subtitle => "Subtitles",
            MediaKind.Export => "Exports",
            _ => throw new ArgumentOutOfRangeException(nameof(kind))
        };

        var path = Path.Combine(RootDirectory, folder);
        Directory.CreateDirectory(path);
        return path;
    }

    internal static string CreateUniquePath(MediaKind kind, string extension, string? descriptor = null, DateTime? timestamp = null)
    {
        var stamp = (timestamp ?? DateTime.Now).ToString("yyyyMMdd_HHmmss");
        var type = kind switch
        {
            MediaKind.Recording => "Recording",
            MediaKind.Processed => "Processed",
            MediaKind.Subtitle => "Subtitles",
            MediaKind.Export => "Export",
            _ => throw new ArgumentOutOfRangeException(nameof(kind))
        };
        var safeDescriptor = string.IsNullOrWhiteSpace(descriptor)
            ? string.Empty
            : "_" + string.Join("_", descriptor.Split(Path.GetInvalidFileNameChars(), StringSplitOptions.RemoveEmptyEntries));
        var safeExtension = extension.StartsWith('.') ? extension : "." + extension;
        var directory = GetDirectory(kind);
        var baseName = $"Synvidra_{type}{safeDescriptor}_{stamp}";
        var candidate = Path.Combine(directory, baseName + safeExtension);
        var suffix = 1;
        while (File.Exists(candidate))
            candidate = Path.Combine(directory, $"{baseName}_{suffix++}{safeExtension}");
        return candidate;
    }
}
