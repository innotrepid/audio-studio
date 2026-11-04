using System;
using System.IO;

namespace AudioStudio;

/// <summary>
/// Resolves offline Whisper and Silero model files packaged next to the executable.
/// CI and the project file copy artifacts into a <c>models</c> directory; no network access.
/// </summary>
internal static class BundledModelPaths
{
    public const string WhisperTinyFileName = "ggml-tiny.bin";
    public const string SileroVadFileName = "silero_vad.onnx";

    public static string ModelsDirectory =>
        Path.Combine(AppContext.BaseDirectory, "models");

    public static string WhisperTinyPath =>
        Path.Combine(ModelsDirectory, WhisperTinyFileName);

    public static string SileroVadPath =>
        Path.Combine(ModelsDirectory, SileroVadFileName);

    public static bool TryGetWhisperTinyPath(out string path, out string error)
    {
        return TryGetExistingFile(WhisperTinyPath, "Whisper", out path, out error);
    }

    public static bool TryGetSileroVadPath(out string path, out string error)
    {
        return TryGetExistingFile(SileroVadPath, "Silero VAD", out path, out error);
    }

    private static bool TryGetExistingFile(
        string candidate,
        string label,
        out string path,
        out string error)
    {
        if (File.Exists(candidate))
        {
            path = candidate;
            error = string.Empty;
            return true;
        }

        path = string.Empty;
        error =
            $"{label} model was not found at '{candidate}'. " +
            "Install a build that bundles offline models under the application models folder.";
        return false;
    }
}
