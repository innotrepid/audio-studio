using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace AudioStudio;

/// <summary>
/// Offline Whisper transcription and live VAD helpers using the bundled models.
/// </summary>
internal static class OfflineSpeechNative
{
    private const int TranscriptionBufferBytes = 256 * 1024;

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recording_start_with_vad",
        CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private static extern int NativeRecordingStartWithVad(
        [MarshalAs(UnmanagedType.LPWStr)] string outputPath,
        [MarshalAs(UnmanagedType.LPWStr)] string? sileroModelPath);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recording_pending_vad_regions",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativePendingVadRegions();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recording_pop_vad_region",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativePopVadRegion(out ulong startMs, out ulong endMs);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_transcribe_wav",
        CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private static extern int NativeTranscribeWav(
        [MarshalAs(UnmanagedType.LPWStr)] string modelPath,
        [MarshalAs(UnmanagedType.LPWStr)] string wavPath,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string languageUtf8,
        int translateToEnglish,
        byte[] outUtf8,
        UIntPtr outCapacity);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_transcription_last_error",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeTranscriptionLastError();

    public static bool TryStartRecordingWithBundledVad(string outputPath, out string status)
    {
        if (!BundledModelPaths.TryGetSileroVadPath(out var sileroPath, out var error))
        {
            status = error;
            return false;
        }

        if (NativeRecordingStartWithVad(outputPath, sileroPath) == 0)
        {
            status = "Could not start recording with live Silero VAD.";
            return false;
        }

        status = "Recording with live Silero VAD.";
        return true;
    }

    public static int PendingVadRegions() => NativePendingVadRegions();

    public static bool TryPopVadRegion(out ulong startMs, out ulong endMs) =>
        NativePopVadRegion(out startMs, out endMs) != 0;

    public static Task<(bool ok, string textOrError)> TranscribeWavAsync(
        string wavPath, string language = "auto", bool translateToEnglish = false)
    {
        return Task.Run(() =>
        {
            if (!BundledModelPaths.TryGetWhisperTinyPath(out var modelPath, out var error))
            {
                return (false, error);
            }

            var buffer = new byte[TranscriptionBufferBytes];
            var ok = NativeTranscribeWav(
                modelPath, wavPath, language ?? "auto", translateToEnglish ? 1 : 0,
                buffer, (UIntPtr)buffer.Length) != 0;
            if (!ok)
            {
                var ptr = NativeTranscriptionLastError();
                var message = ptr == IntPtr.Zero
                    ? "Transcription failed."
                    : Marshal.PtrToStringUTF8(ptr) ?? "Transcription failed.";
                return (false, message);
            }

            var end = Array.IndexOf(buffer, (byte)0);
            if (end < 0) end = buffer.Length;
            var text = Encoding.UTF8.GetString(buffer, 0, end);
            return (true, text);
        });
    }
}
