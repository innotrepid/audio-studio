using System;
using Windows.Storage;

namespace AudioStudio;

/// <summary>
/// Persisted Creator → Teleprompter settings shared with the Record studio teleprompter.
/// Stored in ApplicationData.LocalSettings under stable keys.
/// </summary>
internal static class TeleprompterPreferences
{
    private const string KeySpeed = "teleprompter.speed";
    private const string KeyFontSize = "teleprompter.fontSize";
    private const string KeyLineSpacing = "teleprompter.lineSpacing";
    private const string KeyCountdownIndex = "teleprompter.countdownIndex";

    public sealed class State
    {
        public double Speed { get; init; } = 42;
        public double FontSize { get; init; } = 28;
        public double LineSpacing { get; init; } = 1.5;
        public int CountdownIndex { get; init; }
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
