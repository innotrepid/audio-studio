using System;
using Windows.Storage;

namespace AudioStudio;

/// <summary>
/// Persisted Creator → Teleprompter settings and reading position shared with the
/// Record studio teleprompter. Stored in ApplicationData.LocalSettings.
/// </summary>
internal static class TeleprompterPreferences
{
    private const string KeySpeed = "teleprompter.speed";
    private const string KeyFontSize = "teleprompter.fontSize";
    private const string KeyLineSpacing = "teleprompter.lineSpacing";
    private const string KeyCountdownIndex = "teleprompter.countdownIndex";
    private const string KeyReadingSectionId = "teleprompter.reading.sectionId";
    private const string KeyReadingScriptId = "teleprompter.reading.scriptId";
    private const string KeyReadingScrollY = "teleprompter.reading.scrollY";

    public sealed class State
    {
        public double Speed { get; init; } = 42;
        public double FontSize { get; init; } = 28;
        public double LineSpacing { get; init; } = 1.5;
        public int CountdownIndex { get; init; }
    }

    public sealed class ReadingPosition
    {
        public string SectionId { get; init; } = "";
        public string ScriptId { get; init; } = "";
        public double ScrollY { get; init; }
    }

    public static State Load()
    {
        var values = ApplicationData.Current.LocalSettings.Values;
        return new State
        {
            Speed = ReadDouble(values, KeySpeed, 42, 15, 120),
            FontSize = ReadDouble(values, KeyFontSize, 28, 20, 48),
            LineSpacing = ReadDouble(values, KeyLineSpacing, 1.5, 1.2, 2.0),
            CountdownIndex = ReadInt(values, KeyCountdownIndex, 0, 0, 3),
        };
    }

    public static void Save(double speed, double fontSize, double lineSpacing, int countdownIndex)
    {
        var values = ApplicationData.Current.LocalSettings.Values;
        values[KeySpeed] = Math.Clamp(speed, 15, 120);
        values[KeyFontSize] = Math.Clamp(fontSize, 20, 48);
        values[KeyLineSpacing] = Math.Clamp(lineSpacing, 1.2, 2.0);
        values[KeyCountdownIndex] = Math.Clamp(countdownIndex, 0, 3);
    }

    public static ReadingPosition? LoadReadingPosition()
    {
        var values = ApplicationData.Current.LocalSettings.Values;
        if (!values.ContainsKey(KeyReadingSectionId))
            return null;
        var sectionId = values[KeyReadingSectionId] as string;
        if (string.IsNullOrWhiteSpace(sectionId))
            return null;
        var scriptId = values.ContainsKey(KeyReadingScriptId)
            ? values[KeyReadingScriptId] as string ?? ""
            : "";
        var scrollY = ReadDouble(values, KeyReadingScrollY, 0, 0, 1_000_000);
        return new ReadingPosition
        {
            SectionId = sectionId.Trim(),
            ScriptId = scriptId?.Trim() ?? "",
            ScrollY = scrollY,
        };
    }

    public static void SaveReadingPosition(string? sectionId, string? scriptId, double scrollY)
    {
        var values = ApplicationData.Current.LocalSettings.Values;
        var section = sectionId?.Trim() ?? "";
        if (string.IsNullOrEmpty(section))
        {
            values.Remove(KeyReadingSectionId);
            values.Remove(KeyReadingScriptId);
            values.Remove(KeyReadingScrollY);
            return;
        }

        values[KeyReadingSectionId] = section;
        values[KeyReadingScriptId] = scriptId?.Trim() ?? "";
        values[KeyReadingScrollY] = Math.Max(0, scrollY);
    }

    public static void ClearReadingPosition() => SaveReadingPosition(null, null, 0);

    private static double ReadDouble(
        Windows.Foundation.Collections.IPropertySet values,
        string key,
        double fallback,
        double min,
        double max)
    {
        if (!values.ContainsKey(key))
            return fallback;
        return values[key] switch
        {
            double d => Math.Clamp(d, min, max),
            int i => Math.Clamp((double)i, min, max),
            float f => Math.Clamp((double)f, min, max),
            long l => Math.Clamp((double)l, min, max),
            _ => fallback,
        };
    }

    private static int ReadInt(
        Windows.Foundation.Collections.IPropertySet values,
        string key,
        int fallback,
        int min,
        int max)
    {
        if (!values.ContainsKey(key))
            return fallback;
        return values[key] switch
        {
            int i => Math.Clamp(i, min, max),
            double d => Math.Clamp((int)Math.Round(d), min, max),
            long l => Math.Clamp((int)l, min, max),
            _ => fallback,
        };
    }
}
