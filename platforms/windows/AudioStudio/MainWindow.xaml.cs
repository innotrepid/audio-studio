using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using Windows.Media.Core;
using Windows.Media.Playback;
using Windows.Storage;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Media;
using Windows.Foundation;
using Windows.Storage.Pickers;
using WinRT.Interop;

namespace AudioStudio;

public sealed partial class MainWindow : Window
{
    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_analyze_wav_waveform", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private static extern int NativeAnalyzeWavWaveform([MarshalAs(UnmanagedType.LPWStr)] string inputPath, uint bucketCount, [Out] float[] interleavedPeaks, out ulong totalFrames, out uint sampleRate, out ushort channels);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_trim_wav", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private static extern int NativeTrimWav([MarshalAs(UnmanagedType.LPWStr)] string inputPath,
        [MarshalAs(UnmanagedType.LPWStr)] string outputPath, ulong startFrame, ulong endFrame);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_engine_status", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeEngineStatus();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_capture_device_count", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeCaptureDeviceCount();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_capture_device_name", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeCaptureDeviceName(int index);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_capture_device_id", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeCaptureDeviceId(int index);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_capture_device_select", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private static extern int NativeCaptureDeviceSelect([MarshalAs(UnmanagedType.LPWStr)] string deviceId);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recording_start", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private static extern int NativeRecordingStart([MarshalAs(UnmanagedType.LPWStr)] string outputPath);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recording_stop", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeRecordingStop();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recording_active", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeRecordingActive();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recording_status", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRecordingStatus();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recording_peak", CallingConvention = CallingConvention.Cdecl)]
    private static extern float NativeRecordingPeak();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recording_clipping", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeRecordingClipping();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_recover_partial_wav", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private static extern IntPtr NativeRecoverPartialWav(
        [MarshalAs(UnmanagedType.LPWStr)] string partialPath,
        [MarshalAs(UnmanagedType.LPWStr)] string outputPath);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_processing_start", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private static extern int NativeProcessingStart([MarshalAs(UnmanagedType.LPWStr)] string inputPath,
        [MarshalAs(UnmanagedType.LPWStr)] string outputPath, float thresholdDbfs, float attenuationDb,
        float gainDb, float highPassHz, int processingEngine, float denoiserStrength,
        [MarshalAs(UnmanagedType.LPWStr)] string modelDirectory);

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_processing_cancel", CallingConvention = CallingConvention.Cdecl)]
    private static extern void NativeProcessingCancel();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_processing_active", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeProcessingActive();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_processing_progress", CallingConvention = CallingConvention.Cdecl)]
    private static extern float NativeProcessingProgress();

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_processing_status", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeProcessingStatus();

    private readonly DispatcherTimer _meterTimer = new() { Interval = TimeSpan.FromMilliseconds(100) };
    private readonly DispatcherTimer _processingTimer = new() { Interval = TimeSpan.FromMilliseconds(100) };
    private readonly DispatcherTimer _trimPreviewTimer = new() { Interval = TimeSpan.FromMilliseconds(40) };
    private readonly MediaPlayer _previewPlayer = new();
    private TimeSpan _trimPreviewEndPosition;
    private int _trimPreviewGeneration;
    private ulong? _trimExactStartFrame;
    private ulong? _trimExactEndFrame;
    private bool _applyingTrimTimecodes;
    private string? _libraryFolderPath;
    private StorageFile? _originalPreviewFile;
    private StorageFile? _processedPreviewFile;
    private string? _currentRecordingPath;
    private string? _waveformFilePath;
    private ulong _waveformTotalFrames;
    private uint _waveformSampleRate;
    private Microsoft.UI.Xaml.Shapes.Line? _trimStartMarker;
    private Microsoft.UI.Xaml.Shapes.Line? _trimEndMarker;
    private bool? _draggingTrimStart;
    private uint? _trimPointerId;
    private readonly List<CaptureDeviceChoice> _captureDevices = new();
    private bool _recordingUiActive;
    private bool _restoringProcessingPreferences = true;

    private sealed record CaptureDeviceChoice(string Name, string Id)
    {
        public override string ToString() => Name;
    }

    public MainWindow()
    {
        InitializeComponent();
        WaveformCanvas.PointerPressed += WaveformCanvas_PointerPressed;
        WaveformCanvas.PointerMoved += WaveformCanvas_PointerMoved;
        WaveformCanvas.PointerReleased += WaveformCanvas_PointerReleased;
        WaveformCanvas.PointerCanceled += WaveformCanvas_PointerCanceled;
        try
        {
            var pointer = NativeEngineStatus();
            EngineStatusText.Text = Marshal.PtrToStringAnsi(pointer)
                ?? "Native engine returned an empty status.";
        }
        catch (Exception exception)
        {
            EngineStatusText.Text = $"Native engine unavailable: {exception.Message}";
            RecordToggleButton.IsEnabled = false;
        }

        _meterTimer.Tick += (_, _) => UpdateInputMeter();
        _trimPreviewTimer.Tick += (_, _) =>
        {
            if (_trimPreviewEndPosition > TimeSpan.Zero &&
                _previewPlayer.PlaybackSession.Position >= _trimPreviewEndPosition)
                StopTrimRangePreview(updateStatus: true);
        };
        if (Content is FrameworkElement root)
            root.Loaded += (_, _) => RefreshInputDevices();
        _processingTimer.Tick += (_, _) => UpdateProcessingStatus();
        GateThresholdSlider.ValueChanged += (_, _) =>
            GateThresholdText.Text = $"Noise threshold: {GateThresholdSlider.Value:0} dBFS";
        GateAttenuationSlider.ValueChanged += (_, _) =>
            GateAttenuationText.Text = $"Noise attenuation: {GateAttenuationSlider.Value:0} dB";
        DspGainSlider.ValueChanged += (_, _) =>
            DspGainText.Text = $"Output gain: {DspGainSlider.Value:0} dB";
        HighPassSlider.ValueChanged += (_, _) =>
        {
            var cutoff = HighPassSlider.Value < 1 ? 0 : Math.Min(500, 20 + ((int)HighPassSlider.Value - 1) * 10);
            HighPassText.Text = cutoff == 0 ? "High-pass filter: Off" : $"High-pass filter: {cutoff} Hz";
        };
        foreach (var slider in new[] { GateThresholdSlider, GateAttenuationSlider, DspGainSlider, HighPassSlider })
        {
            slider.PointerCaptureLost += (_, _) => SaveProcessingPreferences();
            slider.KeyUp += (_, _) => SaveProcessingPreferences();
        }
        DenoiserStrengthSlider.ValueChanged += (_, _) =>
        {
            DenoiserStrengthText.Text = $"Neural denoiser strength: {DenoiserStrengthSlider.Value:0}%";
            SaveProcessingPreferences();
        };
        DenoiserStrengthSlider.KeyUp += (_, _) => SaveProcessingPreferences();
        LoadProcessingPreferences();
        ShowWorkflowPanel(RecordPanel);
        TrimStartSlider.ValueChanged += (_, _) =>
        {
            if (!_applyingTrimTimecodes) CancelTrimRangePreviewForSelectionChange();
            if (!_applyingTrimTimecodes) _trimExactStartFrame = null;
            TrimStartText.Text = $"Trim start: {TrimStartSlider.Value / 10.0:0.0}%";
            UpdateWaveformSelectionMarkers();
        };
        TrimEndSlider.ValueChanged += (_, _) =>
        {
            if (!_applyingTrimTimecodes) CancelTrimRangePreviewForSelectionChange();
            if (!_applyingTrimTimecodes) _trimExactEndFrame = null;
            TrimEndText.Text = $"Trim end: {TrimEndSlider.Value / 10.0:0.0}%";
            UpdateWaveformSelectionMarkers();
        };
        Closed += (_, _) =>
        {
            _meterTimer.Stop();
            _processingTimer.Stop();
            _trimPreviewTimer.Stop();
            NativeProcessingCancel();
            NativeRecordingStop();
            StopPreview();
            _previewPlayer.Dispose();
        };
    }

    private static readonly string[] KnownProcessingPresets = { "Natural", "Voice clarity", "Rumble cut" };
    private static string ProcessingPreferencesPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "AudioStudio", "processing-preferences.json");

    private sealed class ProcessingPreferenceState
    {
        public double ThresholdDbfs { get; set; } = -54;
        public double AttenuationDb { get; set; } = -12;
        public double GainDb { get; set; } = 0;
        public double HighPassPosition { get; set; } = 0;
        public string SelectedPreset { get; set; } = "Natural";
        public string SelectedEngine { get; set; } = "RNNoise (recommended)";
        public double DenoiserStrength { get; set; } = 85;
    }

    private void LoadProcessingPreferences()
    {
        _restoringProcessingPreferences = true;
        try
        {
            var preferences = new ProcessingPreferenceState();
            try
            {
                var path = ProcessingPreferencesPath;
                if (File.Exists(path))
                    preferences = JsonSerializer.Deserialize<ProcessingPreferenceState>(File.ReadAllText(path))
                        ?? new ProcessingPreferenceState();
            }
            catch (Exception)
            {
                // Malformed or inaccessible settings fall back to safe defaults.
                preferences = new ProcessingPreferenceState();
            }

            var preset = KnownProcessingPresets.Contains(preferences.SelectedPreset, StringComparer.Ordinal)
                ? preferences.SelectedPreset
                : "Natural";
            var engine = preferences.SelectedEngine is "Classic noise gate" or "DeepFilterNet3 (BETA)"
                ? preferences.SelectedEngine
                : "RNNoise (recommended)";
            ProcessingEngineComboBox.SelectedItem = ProcessingEngineComboBox.Items
                .OfType<ComboBoxItem>()
                .FirstOrDefault(item => string.Equals(item.Content?.ToString(), engine, StringComparison.Ordinal));
            DenoiserStrengthSlider.Value = Math.Clamp(preferences.DenoiserStrength, 0, 100);
            DenoiserStrengthText.Text = $"Neural denoiser strength: {DenoiserStrengthSlider.Value:0}%";

            ProcessingPresetComboBox.SelectedItem = ProcessingPresetComboBox.Items
                .OfType<ComboBoxItem>()
                .FirstOrDefault(item => string.Equals(item.Content?.ToString(), preset, StringComparison.Ordinal));

            GateThresholdSlider.Value = Math.Clamp(preferences.ThresholdDbfs, -70, -20);
            GateAttenuationSlider.Value = Math.Clamp(preferences.AttenuationDb, -36, 0);
            DspGainSlider.Value = Math.Clamp(preferences.GainDb, -24, 12);
            HighPassSlider.Value = Math.Clamp(preferences.HighPassPosition, 0, 49);
        }
        finally
        {
            _restoringProcessingPreferences = false;
        }
    }

    private void SaveProcessingPreferences()
    {
        if (_restoringProcessingPreferences) return;

        var preset = ProcessingPresetComboBox.SelectedItem is ComboBoxItem item &&
                     item.Content is string selectedPreset &&
                     KnownProcessingPresets.Contains(selectedPreset, StringComparer.Ordinal)
            ? selectedPreset
            : "Natural";
        var preferences = new ProcessingPreferenceState
        {
            ThresholdDbfs = Math.Clamp(GateThresholdSlider.Value, -70, -20),
            AttenuationDb = Math.Clamp(GateAttenuationSlider.Value, -36, 0),
            GainDb = Math.Clamp(DspGainSlider.Value, -24, 12),
            HighPassPosition = Math.Clamp(HighPassSlider.Value, 0, 49),
            SelectedPreset = preset,
            SelectedEngine = (ProcessingEngineComboBox.SelectedItem as ComboBoxItem)?.Content?.ToString() is "Classic noise gate" or "DeepFilterNet3 (BETA)"
                ? (ProcessingEngineComboBox.SelectedItem as ComboBoxItem)!.Content!.ToString()! : "RNNoise (recommended)",
            DenoiserStrength = Math.Clamp(DenoiserStrengthSlider.Value, 0, 100)
        };

        try
        {
            var path = ProcessingPreferencesPath;
            var directory = Path.GetDirectoryName(path)!;
            Directory.CreateDirectory(directory);
            var temporaryPath = path + ".tmp";
            File.WriteAllText(temporaryPath, JsonSerializer.Serialize(preferences));
            File.Move(temporaryPath, path, overwrite: true);
        }
        catch (Exception exception)
        {
            ProcessingStatusText.Text = $"Could not save processing preferences: {exception.Message}";
        }
    }

    private void ProcessingEngineComboBox_SelectionChanged(object sender, Microsoft.UI.Xaml.Controls.SelectionChangedEventArgs e)
    {
        if (_restoringProcessingPreferences) return;
        SaveProcessingPreferences();
        var engine = (ProcessingEngineComboBox.SelectedItem as ComboBoxItem)?.Content?.ToString()
            ?? "RNNoise (recommended)";
        ProcessingStatusText.Text = engine == "Classic noise gate"
            ? "Classic noise gate selected. Threshold and attenuation controls apply to this engine."
            : engine == "DeepFilterNet3 (BETA)"
                ? "DeepFilterNet3 (BETA) selected. Experimental model-based denoising; compare results with RNNoise."
                : "RNNoise selected. Neural speech denoising is enabled; threshold and attenuation controls are ignored.";
    }

    private void ProcessingPresetComboBox_SelectionChanged(object sender, Microsoft.UI.Xaml.Controls.SelectionChangedEventArgs e)
    {
        if (_restoringProcessingPreferences) return;
        if (ProcessingPresetComboBox.SelectedItem is not Microsoft.UI.Xaml.Controls.ComboBoxItem item ||
            item.Content is not string preset) return;

        switch (preset)
        {
            case "Voice clarity":
                GateThresholdSlider.Value = -48;
                GateAttenuationSlider.Value = -18;
                DspGainSlider.Value = 2;
                HighPassSlider.Value = 7; // 80 Hz
                break;
            case "Rumble cut":
                GateThresholdSlider.Value = -60;
                GateAttenuationSlider.Value = -12;
                DspGainSlider.Value = 0;
                HighPassSlider.Value = 11; // 120 Hz
                break;
            default:
                GateThresholdSlider.Value = -54;
                GateAttenuationSlider.Value = -12;
                DspGainSlider.Value = 0;
                HighPassSlider.Value = 0;
                break;
        }
        SaveProcessingPreferences();
        ProcessingStatusText.Text = $"{preset} preset applied. Fine-tune the sliders and compare the output with the original.";
    }

    private async void LoadWaveform_Click(object sender, RoutedEventArgs e)
    {
        var picker = new FileOpenPicker();
        picker.FileTypeFilter.Add(".wav");
        InitializeWithWindow.Initialize(picker, WindowNative.GetWindowHandle(this));
        var file = await picker.PickSingleFileAsync();
        if (file is null) return;
        await LoadWaveformFileAsync(file);
    }

    private async Task LoadWaveformFileAsync(StorageFile file)
    {
        CancelTrimRangePreviewForSelectionChange();
        const int buckets = 240;
        var peaks = new float[buckets * 4];
        try
        {
            var analysis = await Task.Run(() =>
            {
                var result = NativeAnalyzeWavWaveform(file.Path, (uint)buckets, peaks, out var frames, out var rate, out var channels);
                return (result, frames, rate, channels);
            });
            if (analysis.result <= 0)
            {
                WaveformStatusText.Text = "Could not analyze this WAV. Choose a valid PCM16 WAV file.";
                return;
            }
            WaveformCanvas.Children.Clear();
            var width = WaveformCanvas.Width;
            var center = WaveformCanvas.Height / 2;
            var amplitude = center - 8;

            // A quiet timing grid makes the waveform easier to read without
            // introducing a full-screen visual effect or extra per-peak elements.
            var gridGeometry = new PathGeometry();
            for (var division = 1; division < 8; division++)
            {
                var x = division * width / 8.0;
                var figure = new PathFigure
                {
                    StartPoint = new Point(x, 0),
                    IsClosed = false,
                    IsFilled = false
                };
                figure.Segments.Add(new LineSegment { Point = new Point(x, WaveformCanvas.Height) });
                gridGeometry.Figures.Add(figure);
            }
            var centerLine = new PathFigure
            {
                StartPoint = new Point(0, center),
                IsClosed = false,
                IsFilled = false
            };
            centerLine.Segments.Add(new LineSegment { Point = new Point(width, center) });
            gridGeometry.Figures.Add(centerLine);
            WaveformCanvas.Children.Add(new Microsoft.UI.Xaml.Shapes.Path
            {
                Data = gridGeometry,
                Stroke = new SolidColorBrush(Microsoft.UI.ColorHelper.FromArgb(255, 58, 54, 80)),
                StrokeThickness = 1,
                IsHitTestVisible = false
            });

            var waveformGeometry = new PathGeometry();
            for (var i = 0; i < buckets; i++)
            {
                var min = Math.Min(peaks[i * 4], peaks[i * 4 + 2]);
                var max = Math.Max(peaks[i * 4 + 1], peaks[i * 4 + 3]);
                var x = (i + 0.5) * width / buckets;
                var figure = new PathFigure
                {
                    StartPoint = new Point(x, center - Math.Clamp(max, -1f, 1f) * amplitude),
                    IsClosed = false,
                    IsFilled = false
                };
                figure.Segments.Add(new LineSegment
                {
                    Point = new Point(x, center - Math.Clamp(min, -1f, 1f) * amplitude)
                });
                waveformGeometry.Figures.Add(figure);
            }
            WaveformCanvas.Children.Add(new Microsoft.UI.Xaml.Shapes.Path
            {
                Data = waveformGeometry,
                Stroke = new SolidColorBrush(Microsoft.UI.ColorHelper.FromArgb(255, 79, 125, 255)),
                StrokeThickness = 1.5,
                IsHitTestVisible = false
            });
            _waveformFilePath = file.Path;
            _waveformTotalFrames = analysis.frames;
            _waveformSampleRate = analysis.rate;
            _trimExactStartFrame = null;
            _trimExactEndFrame = null;
            TrimStartSlider.Value = 0;
            TrimEndSlider.Value = 1000;
            _trimStartMarker = new Microsoft.UI.Xaml.Shapes.Line
            {
                Y1 = 0, Y2 = WaveformCanvas.Height,
                Stroke = new SolidColorBrush(Microsoft.UI.Colors.Gold),
                StrokeThickness = 2
            };
            _trimEndMarker = new Microsoft.UI.Xaml.Shapes.Line
            {
                Y1 = 0, Y2 = WaveformCanvas.Height,
                Stroke = new SolidColorBrush(Microsoft.UI.Colors.OrangeRed),
                StrokeThickness = 2
            };
            WaveformCanvas.Children.Add(_trimStartMarker);
            WaveformCanvas.Children.Add(_trimEndMarker);
            UpdateWaveformSelectionMarkers();
            TrimWaveformButton.IsEnabled = _waveformTotalFrames > 1;
            PreviewTrimRangeButton.IsEnabled = _waveformTotalFrames > 1;
            WaveformStatusText.Text = $"{file.Name} · {analysis.rate} Hz · {analysis.channels} channel(s) · {analysis.frames:N0} frames. Select and preview a range below; original unchanged.";
        }
        catch (Exception exception)
        {
            WaveformStatusText.Text = $"Waveform analysis failed: {exception.Message}";
        }
    }



    private void WaveformCanvas_PointerPressed(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
    {
        if (_waveformTotalFrames < 2 || _trimStartMarker is null || _trimEndMarker is null)
            return;

        var point = e.GetCurrentPoint(WaveformCanvas);
        if (!point.Properties.IsLeftButtonPressed)
            return;

        var startX = _trimStartMarker.X1;
        var endX = _trimEndMarker.X1;
        var distanceToStart = Math.Abs(point.Position.X - startX);
        var distanceToEnd = Math.Abs(point.Position.X - endX);
        const double hitRadius = 24.0;
        if (Math.Min(distanceToStart, distanceToEnd) > hitRadius)
            return;

        // When handles overlap closely, keep the selected range usable by choosing
        // the closest handle; ties select the start endpoint deterministically.
        _draggingTrimStart = distanceToStart <= distanceToEnd;
        _trimPointerId = e.Pointer.PointerId;
        WaveformCanvas.CapturePointer(e.Pointer);
        e.Handled = true;
    }

    private void WaveformCanvas_PointerMoved(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
    {
        if (_draggingTrimStart is null || _trimPointerId != e.Pointer.PointerId ||
            _waveformTotalFrames < 2 || _waveformSampleRate == 0)
            return;

        var point = e.GetCurrentPoint(WaveformCanvas);
        if (!point.Properties.IsLeftButtonPressed)
            return;

        var width = Math.Max(1.0, WaveformCanvas.ActualWidth > 0 ? WaveformCanvas.ActualWidth : WaveformCanvas.Width);
        var fraction = Math.Clamp(point.Position.X / width, 0.0, 1.0);
        var requestedFrame = (ulong)Math.Round(fraction * _waveformTotalFrames, MidpointRounding.AwayFromZero);
        CancelTrimRangePreviewForSelectionChange();
        var (rawStart, rawEnd) = GetTrimRangeFrames();
        // Normalize a range left temporarily collapsed by the legacy sliders so
        // a drag can recover it without unsigned underflow or an empty selection.
        var currentStart = Math.Min(rawStart, _waveformTotalFrames - 1);
        var currentEnd = Math.Clamp(rawEnd, currentStart + 1, _waveformTotalFrames);

        if (_draggingTrimStart.Value)
        {
            var frame = Math.Min(requestedFrame, currentEnd - 1);
            _trimExactStartFrame = frame;
        }
        else
        {
            var frame = Math.Max(requestedFrame, currentStart + 1);
            frame = Math.Min(frame, _waveformTotalFrames);
            _trimExactEndFrame = frame;
        }

        // Keep frame precision on the dragged endpoint even when the slider's
        // coarser 0.1% position does not change; never disturb the other endpoint.
        var (startFrame, endFrame) = GetTrimRangeFrames();
        _applyingTrimTimecodes = true;
        try
        {
            TrimStartSlider.Value = Math.Clamp((double)startFrame / _waveformTotalFrames * 1000.0, 0.0, 1000.0);
            TrimEndSlider.Value = Math.Clamp((double)endFrame / _waveformTotalFrames * 1000.0, 0.0, 1000.0);
        }
        finally
        {
            _applyingTrimTimecodes = false;
        }
        UpdateWaveformSelectionMarkers();
        e.Handled = true;
    }

    private void WaveformCanvas_PointerReleased(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
    {
        EndWaveformHandleDrag(e);
    }

    private void WaveformCanvas_PointerCanceled(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
    {
        EndWaveformHandleDrag(e);
    }

    private void EndWaveformHandleDrag(Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
    {
        if (_trimPointerId != e.Pointer.PointerId)
            return;

        _draggingTrimStart = null;
        _trimPointerId = null;
        WaveformCanvas.ReleasePointerCapture(e.Pointer);
        e.Handled = true;
    }

    private (ulong StartFrame, ulong EndFrame) GetTrimRangeFrames()
    {
        var startFrame = _trimExactStartFrame ??
            (ulong)Math.Floor(_waveformTotalFrames * (TrimStartSlider.Value / 1000.0));
        var endFrame = _trimExactEndFrame ??
            (ulong)Math.Floor(_waveformTotalFrames * (TrimEndSlider.Value / 1000.0));
        return (Math.Min(startFrame, _waveformTotalFrames), Math.Min(endFrame, _waveformTotalFrames));
    }

    private static string FormatTrimTime(ulong frame, uint sampleRate)
    {
        if (sampleRate == 0) return "00:00.000";
        var time = TimeSpan.FromSeconds((double)frame / sampleRate);
        return $"{(long)time.TotalMinutes:00}:{time.Seconds:00}.{time.Milliseconds:000}";
    }

    private static bool TryParseTrimTime(string text, out TimeSpan time)
    {
        time = TimeSpan.Zero;
        var parts = text.Trim().Split(':');
        if (parts.Length != 2 || !ulong.TryParse(parts[0], out var minutes))
            return false;

        var secondsParts = parts[1].Split('.');
        if (secondsParts.Length < 1 || secondsParts.Length > 2 ||
            !int.TryParse(secondsParts[0], out var seconds) || seconds < 0 || seconds > 59)
            return false;

        var milliseconds = 0;
        if (secondsParts.Length == 2)
        {
            var fraction = secondsParts[1];
            if (fraction.Length < 1 || fraction.Length > 3 || !int.TryParse(fraction, out milliseconds))
                return false;
            milliseconds *= fraction.Length == 1 ? 100 : fraction.Length == 2 ? 10 : 1;
        }

        try
        {
            time = TimeSpan.FromMinutes(minutes) + TimeSpan.FromSeconds(seconds) +
                TimeSpan.FromMilliseconds(milliseconds);
            return true;
        }
        catch (OverflowException)
        {
            return false;
        }
    }

    private void ApplyTrimTimes_Click(object sender, RoutedEventArgs e)
    {
        if (_waveformTotalFrames < 2 || _waveformSampleRate == 0)
        {
            WaveformStatusText.Text = "Load a WAV waveform before setting exact in/out points.";
            return;
        }
        if (!TryParseTrimTime(TrimStartTimeBox.Text, out var startTime) ||
            !TryParseTrimTime(TrimEndTimeBox.Text, out var endTime))
        {
            WaveformStatusText.Text = "Use minutes:seconds.milliseconds, for example 02:14.350.";
            return;
        }

        var durationSeconds = (double)_waveformTotalFrames / _waveformSampleRate;
        if (endTime <= startTime || endTime.TotalSeconds > durationSeconds)
        {
            WaveformStatusText.Text = $"Choose an in point before the out point, both within the file (duration {FormatTrimTime(_waveformTotalFrames, _waveformSampleRate)}).";
            return;
        }

        var startFrame = (ulong)Math.Round(startTime.TotalSeconds * _waveformSampleRate, MidpointRounding.AwayFromZero);
        var endFrame = (ulong)Math.Round(endTime.TotalSeconds * _waveformSampleRate, MidpointRounding.AwayFromZero);
        if (endFrame <= startFrame || endFrame > _waveformTotalFrames)
        {
            WaveformStatusText.Text = "The selected times do not contain a valid audio-frame range.";
            return;
        }

        _applyingTrimTimecodes = true;
        try
        {
            TrimStartSlider.Value = Math.Clamp((double)startFrame / _waveformTotalFrames * 1000.0, 0.0, 1000.0);
            TrimEndSlider.Value = Math.Clamp((double)endFrame / _waveformTotalFrames * 1000.0, 0.0, 1000.0);
        }
        finally
        {
            _applyingTrimTimecodes = false;
        }
        CancelTrimRangePreviewForSelectionChange();
        _trimExactStartFrame = startFrame;
        _trimExactEndFrame = endFrame;
        UpdateWaveformSelectionMarkers();
        WaveformStatusText.Text = "Exact in/out points applied. Preview or export uses these frame positions; moving a slider adjusts that endpoint.";
    }

    private void UpdateWaveformSelectionMarkers()
    {
        if (_trimStartMarker is null || _trimEndMarker is null || _waveformTotalFrames == 0)
            return;
        var (startFrame, endFrame) = GetTrimRangeFrames();
        var width = WaveformCanvas.Width;
        _trimStartMarker.X1 = _trimStartMarker.X2 = width * startFrame / _waveformTotalFrames;
        _trimEndMarker.X1 = _trimEndMarker.X2 = width * endFrame / _waveformTotalFrames;

        if (_waveformSampleRate > 0)
        {
            var durationFrames = endFrame > startFrame ? endFrame - startFrame : 0;
            var duration = TimeSpan.FromSeconds((double)durationFrames / _waveformSampleRate);
            TrimStartTimeBox.Text = FormatTrimTime(startFrame, _waveformSampleRate);
            TrimEndTimeBox.Text = FormatTrimTime(endFrame, _waveformSampleRate);
            WaveformSelectionText.Text = $"Selection: {TrimStartTimeBox.Text} – {TrimEndTimeBox.Text} · {duration.TotalSeconds:0.00}s";
        }
    }

    private async void PreviewTrimRange_Click(object sender, RoutedEventArgs e)
    {
        var sourcePath = _waveformFilePath;
        if (string.IsNullOrWhiteSpace(sourcePath) || _waveformTotalFrames < 2 || _waveformSampleRate == 0)
        {
            WaveformStatusText.Text = "Load a WAV waveform before previewing a selection.";
            return;
        }
        if (NativeRecordingActive() != 0 || NativeProcessingActive() != 0)
        {
            WaveformStatusText.Text = "Stop recording or processing before previewing a trim range.";
            return;
        }

        var (startFrame, endFrame) = GetTrimRangeFrames();
        if (endFrame <= startFrame)
        {
            WaveformStatusText.Text = "Choose a range with at least one audio frame.";
            return;
        }

        var startPosition = TimeSpan.FromSeconds((double)startFrame / _waveformSampleRate);
        var endPosition = TimeSpan.FromSeconds((double)endFrame / _waveformSampleRate);
        var previewGeneration = ++_trimPreviewGeneration;
        _trimPreviewEndPosition = endPosition;
        PreviewTrimRangeButton.IsEnabled = false;
        _trimPreviewTimer.Stop();
        try
        {
            _previewPlayer.Pause();
            _previewPlayer.Source = null;
            var file = await StorageFile.GetFileFromPathAsync(sourcePath);
            _previewPlayer.Source = MediaSource.CreateFromStorageFile(file);

            // Give the media pipeline a moment to open the WAV before seeking.
            await Task.Delay(150);
            if (previewGeneration != _trimPreviewGeneration ||
                !string.Equals(sourcePath, _waveformFilePath, StringComparison.OrdinalIgnoreCase))
                return;

            _previewPlayer.PlaybackSession.Position = startPosition;
            _previewPlayer.Play();
            _trimPreviewTimer.Start();
            WaveformStatusText.Text = $"Previewing selected range · {startPosition:mm\\:ss} to {_trimPreviewEndPosition:mm\\:ss}. Original unchanged.";
        }
        catch (Exception exception)
        {
            if (previewGeneration == _trimPreviewGeneration)
            {
                _trimPreviewTimer.Stop();
                _trimPreviewEndPosition = TimeSpan.Zero;
                WaveformStatusText.Text = $"Could not preview selected range: {exception.Message}";
            }
        }
        finally
        {
            if (previewGeneration == _trimPreviewGeneration)
                PreviewTrimRangeButton.IsEnabled = !string.IsNullOrWhiteSpace(_waveformFilePath) && _waveformTotalFrames > 1;
        }
    }

    private void CancelTrimRangePreviewForSelectionChange()
    {
        if (_trimPreviewEndPosition <= TimeSpan.Zero && !_trimPreviewTimer.IsEnabled)
            return;
        StopTrimRangePreview(updateStatus: false);
    }

    private void StopTrimRangePreview(bool updateStatus)
    {
        _trimPreviewGeneration++;
        _trimPreviewTimer.Stop();
        _trimPreviewEndPosition = TimeSpan.Zero;
        _previewPlayer.Pause();
        _previewPlayer.Source = null;
        PreviewTrimRangeButton.IsEnabled = !string.IsNullOrWhiteSpace(_waveformFilePath) && _waveformTotalFrames > 1;
        if (updateStatus)
            WaveformStatusText.Text = "Selection preview finished. Adjust the range or export the trim.";
    }

    private async void TrimWaveform_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(_waveformFilePath) || _waveformTotalFrames < 2)
        {
            WaveformStatusText.Text = "Load a WAV waveform before trimming.";
            return;
        }
        var (selectedStartFrame, selectedEndFrame) = GetTrimRangeFrames();
        if (selectedStartFrame >= selectedEndFrame)
        {
            WaveformStatusText.Text = "Trim start must be before trim end.";
            return;
        }

        var folderPicker = new FolderPicker();
        folderPicker.FileTypeFilter.Add("*");
        InitializeWithWindow.Initialize(folderPicker, WindowNative.GetWindowHandle(this));
        var folder = await folderPicker.PickSingleFolderAsync();
        if (folder is null)
        {
            WaveformStatusText.Text = "Trim cancelled. No files were changed.";
            return;
        }

        var baseName = Path.GetFileNameWithoutExtension(_waveformFilePath);
        var outputPath = Path.Combine(folder.Path, $"{baseName}-trimmed.wav");
        for (var suffix = 2; File.Exists(outputPath); suffix++)
            outputPath = Path.Combine(folder.Path, $"{baseName}-trimmed-{suffix}.wav");

        var sourcePath = _waveformFilePath!;
        var (startFrame, endFrame) = GetTrimRangeFrames();
        if (endFrame <= startFrame)
        {
            WaveformStatusText.Text = "The selected range is too short to contain an audio frame.";
            return;
        }

        TrimWaveformButton.IsEnabled = false;
        WaveformStatusText.Text = "Trimming selected frames to a separate WAV…";
        try
        {
            var succeeded = await Task.Run(() => NativeTrimWav(sourcePath, outputPath, startFrame, endFrame) != 0);
            if (!succeeded)
            {
                WaveformStatusText.Text = "Trim failed. The source was preserved; check the range and destination.";
            }
            else
            {
                var details = await Task.Run(() => ReadRecordingTakeDetails(outputPath));
                WaveformStatusText.Text = details is not null
                    ? $"Trimmed WAV verified and saved: {Path.GetFileName(outputPath)}. {details}. Original file preserved."
                    : $"UNVERIFIED TRIM OUTPUT — {Path.GetFileName(outputPath)} was created but could not be validated. The output was retained for inspection; the original file is preserved.";
            }
        }
        catch (Exception exception)
        {
            WaveformStatusText.Text = $"Trim failed: {exception.Message}. Original file preserved.";
        }
        finally
        {
            TrimWaveformButton.IsEnabled = _waveformTotalFrames > 1;
        }
    }

    private void ShowWorkflowPanel(Border selected)
    {
        RecordPanel.Visibility = selected == RecordPanel ? Visibility.Visible : Visibility.Collapsed;
        EditorPanel.Visibility = selected == EditorPanel ? Visibility.Visible : Visibility.Collapsed;
        ProcessingPanel.Visibility = selected == ProcessingPanel ? Visibility.Visible : Visibility.Collapsed;
        ComparePanel.Visibility = selected == ComparePanel ? Visibility.Visible : Visibility.Collapsed;
        LibraryPanel.Visibility = selected == LibraryPanel ? Visibility.Visible : Visibility.Collapsed;
        CreatorPanel.Visibility = selected == CreatorPanel ? Visibility.Visible : Visibility.Collapsed;

        // Keep the four primary destinations stable; processing and A/B comparison
        // remain contextual tools launched from Editor. Existing panel instances stay alive.
        NavigateRecordButton.IsChecked = selected == RecordPanel;
        NavigateEditorButton.IsChecked = selected == EditorPanel;
        NavigateCreatorButton.IsChecked = selected == CreatorPanel;
        NavigateLibraryButton.IsChecked = selected == LibraryPanel;
        NavigateProcessingButton.IsChecked = selected == ProcessingPanel;
        NavigateCompareButton.IsChecked = selected == ComparePanel;

        SetNavigationAppearance(NavigateRecordButton, selected == RecordPanel);
        SetNavigationAppearance(NavigateEditorButton, selected == EditorPanel);
        SetNavigationAppearance(NavigateCreatorButton, selected == CreatorPanel);
        SetNavigationAppearance(NavigateLibraryButton, selected == LibraryPanel);
        SetNavigationAppearance(NavigateProcessingButton, selected == ProcessingPanel);
        SetNavigationAppearance(NavigateCompareButton, selected == ComparePanel);
    }

    private static void SetNavigationAppearance(ToggleButton button, bool isSelected)
    {
        button.Background = new SolidColorBrush(
            Windows.UI.Color.FromArgb(255,
                isSelected ? (byte)79 : (byte)34,
                isSelected ? (byte)125 : (byte)34,
                isSelected ? (byte)255 : (byte)48));
        button.Foreground = new SolidColorBrush(
            Windows.UI.Color.FromArgb(255,
                isSelected ? (byte)12 : (byte)243,
                isSelected ? (byte)12 : (byte)240,
                isSelected ? (byte)20 : (byte)255));
        button.BorderBrush = new SolidColorBrush(
            Windows.UI.Color.FromArgb(255,
                isSelected ? (byte)79 : (byte)58,
                isSelected ? (byte)125 : (byte)54,
                isSelected ? (byte)255 : (byte)80));
    }

    private void NavigateRecord_Click(object sender, RoutedEventArgs e) => ShowWorkflowPanel(RecordPanel);
    private void NavigateEditor_Click(object sender, RoutedEventArgs e) => ShowWorkflowPanel(EditorPanel);
    private void NavigateCreator_Click(object sender, RoutedEventArgs e) => ShowWorkflowPanel(CreatorPanel);
    private void NavigateProcessing_Click(object sender, RoutedEventArgs e) => ShowWorkflowPanel(ProcessingPanel);
    private void NavigateCompare_Click(object sender, RoutedEventArgs e) => ShowWorkflowPanel(ComparePanel);
    private void NavigateLibrary_Click(object sender, RoutedEventArgs e) => ShowWorkflowPanel(LibraryPanel);

    private void LibrarySearchBox_TextChanged(object sender, TextChangedEventArgs e) => ApplyLibrarySearchFilter();

    private void ApplyLibrarySearchFilter()
    {
        var query = LibrarySearchBox.Text?.Trim() ?? string.Empty;
        var visibleCount = 0;
        foreach (var child in LibraryItemsPanel.Children)
        {
            if (child is not FrameworkElement item)
                continue;

            var searchable = item.Tag?.ToString() ?? string.Empty;
            var visible = query.Length == 0 || searchable.Contains(query, StringComparison.OrdinalIgnoreCase);
            item.Visibility = visible ? Visibility.Visible : Visibility.Collapsed;
            if (visible)
                visibleCount++;
        }

        if (LibraryItemsPanel.Children.Count > 0)
        {
            LibraryStatusText.Text = visibleCount == 0
                ? $"No WAVs match “{query}”. Clear the search to see all files."
                : query.Length == 0
                    ? $"{visibleCount} recent WAV file(s) in {_libraryFolderPath} · showing up to 100"
                    : $"{visibleCount} matching WAV file(s) · showing up to 100 recent files";
        }
    }

    private async void ChooseLibraryFolder_Click(object sender, RoutedEventArgs e)
    {
        var picker = new FolderPicker();
        picker.FileTypeFilter.Add("*");
        InitializeWithWindow.Initialize(picker, WindowNative.GetWindowHandle(this));
        var folder = await picker.PickSingleFolderAsync();
        if (folder is null)
        {
            // Cancellation is not a library failure; preserve the current results and status.
            return;
        }

        _libraryFolderPath = folder.Path;
        await RefreshLibraryAsync();
    }

    private async void RefreshLibrary_Click(object sender, RoutedEventArgs e) => await RefreshLibraryAsync();

    private async Task RefreshLibraryAsync()
    {
        if (string.IsNullOrWhiteSpace(_libraryFolderPath) || !Directory.Exists(_libraryFolderPath))
        {
            LibraryItemsPanel.Children.Clear();
            LibraryStatusText.Text = "Choose an accessible folder to list WAV files.";
            return;
        }

        // A refresh replaces the visible result set. Clear stale entries before enumeration
        // so a failed or inaccessible folder cannot leave misleading rows on screen.
        LibraryItemsPanel.Children.Clear();
        RefreshLibraryButton.IsEnabled = false;
        LibraryStatusText.Text = "Reading WAV files…";
        try
        {
            var files = await Task.Run(() => Directory.EnumerateFiles(_libraryFolderPath, "*", SearchOption.TopDirectoryOnly)
                .Where(path => string.Equals(Path.GetExtension(path), ".wav", StringComparison.OrdinalIgnoreCase))
                .Select(path => new FileInfo(path))
                .OrderByDescending(info => info.LastWriteTimeUtc)
                .Take(100)
                .Select(info => new { info.FullName, info.Name, info.Length, info.LastWriteTime })
                .ToList());

            if (files.Count == 0)
            {
                LibraryStatusText.Text = $"No WAV files found in {Path.GetFileName(_libraryFolderPath)}. Choose another folder or export a WAV first.";
                return;
            }

            LibraryStatusText.Text = $"{files.Count} recent WAV file(s) in { _libraryFolderPath } · showing up to 100";
            foreach (var file in files)
            {
                var item = new StackPanel { Spacing = 6, Margin = new Thickness(0, 0, 0, 10) };
                item.Tag = $"{file.Name} {file.FullName}";
                item.Background = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 20, 20, 30));
                item.Children.Add(new TextBlock
                {
                    Text = file.Name,
                    FontSize = 15,
                    FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                    Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 255, 255, 255)),
                    TextWrapping = Microsoft.UI.Xaml.TextWrapping.Wrap
                });
                item.Children.Add(new TextBlock
                {
                    Text = $"{Math.Max(1, file.Length / 1024)} KB · Modified {file.LastWriteTime:g}",
                    FontSize = 12,
                    Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 174, 185, 204)),
                    TextWrapping = Microsoft.UI.Xaml.TextWrapping.Wrap
                });
                var actions = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
                var editButton = new Button { Content = "Edit" };
                editButton.Click += async (_, _) =>
                {
                    try
                    {
                        var storageFile = await StorageFile.GetFileFromPathAsync(file.FullName);
                        ShowWorkflowPanel(EditorPanel);
                        await LoadWaveformFileAsync(storageFile);
                    }
                    catch (Exception exception)
                    {
                        ProcessingStatusText.Text = $"Could not open {file.Name}: {exception.Message}";
                    }
                };
                var processButton = new Button { Content = "Process copy" };
                processButton.Click += async (_, _) =>
                {
                    try
                    {
                        var storageFile = await StorageFile.GetFileFromPathAsync(file.FullName);
                        ShowWorkflowPanel(ProcessingPanel);
                        await ProcessWavFileAsync(storageFile);
                    }
                    catch (Exception exception)
                    {
                        LibraryStatusText.Text = $"Could not open {file.Name}: {exception.Message}";
                    }
                };
                actions.Children.Add(editButton);
                actions.Children.Add(processButton);
                item.Children.Add(actions);
                LibraryItemsPanel.Children.Add(item);
            }
            ApplyLibrarySearchFilter();
        }
        catch (Exception exception)
        {
            LibraryItemsPanel.Children.Clear();
            LibraryStatusText.Text = $"Could not load WAV library: {exception.Message}. Check folder access, choose another folder, or use the file pickers.";
        }
        finally
        {
            RefreshLibraryButton.IsEnabled = true;
        }
    }

    private void RefreshInputDevices_Click(object sender, RoutedEventArgs e) => RefreshInputDevices();

    private void RefreshInputDevices()
    {
        var selectedId = (InputDeviceComboBox.SelectedItem as CaptureDeviceChoice)?.Id;
        _captureDevices.Clear();
        InputDeviceComboBox.Items.Clear();
        try
        {
            var count = NativeCaptureDeviceCount();
            for (var i = 0; i < count; i++)
            {
                var name = Marshal.PtrToStringUni(NativeCaptureDeviceName(i)) ?? $"Input {i + 1}";
                var id = Marshal.PtrToStringUni(NativeCaptureDeviceId(i)) ?? "";
                if (!string.IsNullOrWhiteSpace(id))
                {
                    var choice = new CaptureDeviceChoice(name, id);
                    _captureDevices.Add(choice);
                    InputDeviceComboBox.Items.Add(choice);
                }
            }
            var match = _captureDevices.FindIndex(device => device.Id == selectedId);
            InputDeviceComboBox.SelectedIndex = match >= 0 ? match : (_captureDevices.Count > 0 ? 0 : -1);
            InputDeviceStatusText.Text = _captureDevices.Count == 0
                ? "No active microphone inputs found. Connect a device and refresh."
                : $"{_captureDevices.Count} input device(s) available.";
        }
        catch (Exception exception)
        {
            InputDeviceStatusText.Text = $"Could not enumerate microphones: {exception.Message}";
        }
    }

    private void UpdateRecordToggleVisual(bool isRecording)
    {
        var accent = isRecording
            ? Windows.UI.Color.FromArgb(255, 32, 201, 151)
            : Windows.UI.Color.FromArgb(255, 79, 125, 255);
        RecordToggleButton.Content = isRecording ? "■  Stop & save WAV" : "●  Start recording";
        RecordToggleButton.Background = new SolidColorBrush(accent);
        RecordToggleButton.BorderBrush = new SolidColorBrush(accent);
        RecordToggleButton.Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 8, 18, 28));
    }

    private void RecordToggle_Click(object sender, RoutedEventArgs e)
    {
        if (NativeRecordingActive() != 0)
            StopRecording_Click(sender, e);
        else
            StartRecording_Click(sender, e);
    }

    private async void StartRecording_Click(object sender, RoutedEventArgs e)
    {
        if (NativeProcessingActive() != 0)
        {
            RecordingStatusText.Text = "Wait for WAV processing to finish before recording.";
            return;
        }
        StopPreview();
        if (InputDeviceComboBox.SelectedItem is not CaptureDeviceChoice selectedDevice ||
            NativeCaptureDeviceSelect(selectedDevice.Id) == 0)
        {
            InputDeviceStatusText.Text = "Choose an available input device before recording.";
            return;
        }
        RecordToggleButton.IsEnabled = false;
        RecordingStatusText.Text = "Choose where to save the recording…";
        try
        {
            var picker = new FileSavePicker
            {
                SuggestedFileName = $"AudioStudio-{DateTime.Now:yyyyMMdd-HHmmss}"
            };
            picker.FileTypeChoices.Add("WAV audio", new List<string> { ".wav" });
            InitializeWithWindow.Initialize(picker, WindowNative.GetWindowHandle(this));
            var file = await picker.PickSaveFileAsync();
            if (file is null)
            {
                RecordingStatusText.Text = "Recording cancelled. No file was created.";
                return;
            }

            RecordingStatusText.Text = "Opening microphone…";
            var started = await Task.Run(() => NativeRecordingStart(file.Path));
            RecordingStatusText.Text = ReadRecordingStatus();
            if (started != 0)
            {
                _currentRecordingPath = file.Path;
                _recordingUiActive = true;
                RecordToggleButton.IsEnabled = true;
                UpdateRecordToggleVisual(isRecording: true);
                InputDeviceComboBox.IsEnabled = false;
                RefreshInputDevicesButton.IsEnabled = false;
                ClippingWarningText.Text = "";
                _meterTimer.Start();
            }
        }
        catch (Exception exception)
        {
            RecordingStatusText.Text = $"Could not start recording: {exception.Message}";
        }
        finally
        {
            if (NativeRecordingActive() == 0)
            {
                RecordToggleButton.IsEnabled = NativeProcessingActive() == 0;
                InputDeviceComboBox.IsEnabled = true;
                RefreshInputDevicesButton.IsEnabled = true;
            }
        }
    }

    private async void StopRecording_Click(object sender, RoutedEventArgs e)
    {
        RecordToggleButton.IsEnabled = false;
        RecordingStatusText.Text = "Finalizing WAV file…";
        try
        {
            _meterTimer.Stop();
            var stopResult = await Task.Run(NativeRecordingStop);
            UpdateInputMeter();
            var nativeStatus = ReadRecordingStatus();
            RecordingStatusText.Text = nativeStatus;
            if (!string.IsNullOrWhiteSpace(_currentRecordingPath))
            {
                var takePath = _currentRecordingPath;
                var takeDetails = await Task.Run(() => ReadRecordingTakeDetails(takePath));
                var nativeReportedSaved = nativeStatus.StartsWith("Recording saved", StringComparison.OrdinalIgnoreCase);
                if (stopResult != 0 && nativeReportedSaved && takeDetails is not null)
                {
                    RecordingStatusText.Text = $"FINALIZED AND VERIFIED — {nativeStatus}" +
                        Environment.NewLine + Environment.NewLine + takeDetails;
                }
                else if (nativeReportedSaved)
                {
                    // A success message alone is not enough to claim a usable WAV.
                    // Preserve the file and clearly report the verification gap.
                    RecordingStatusText.Text = "UNVERIFIED TAKE — native finalization reported success, but the WAV could not be validated. The file was not deleted. Inspect it before relying on it." +
                        Environment.NewLine + Environment.NewLine + nativeStatus;
                }
                else if (stopResult == 0)
                {
                    RecordingStatusText.Text = "RECOVERABLE / INCOMPLETE TAKE — finalization did not complete successfully. Any temporary recording file was retained for recovery." +
                        Environment.NewLine + Environment.NewLine + nativeStatus;
                }
                else if (takeDetails is not null)
                {
                    RecordingStatusText.Text = "UNVERIFIED TAKE — the WAV can be read, but native finalization did not confirm a successful save. The file was not deleted; inspect it before relying on it." +
                        Environment.NewLine + Environment.NewLine + nativeStatus +
                        Environment.NewLine + Environment.NewLine + takeDetails;
                }
            }
            _currentRecordingPath = null;
        }
        catch (Exception exception)
        {
            RecordingStatusText.Text = $"Could not stop recording cleanly: {exception.Message}";
        }
        finally
        {
            _recordingUiActive = false;
            RecordToggleButton.IsEnabled = NativeProcessingActive() == 0;
            UpdateRecordToggleVisual(isRecording: false);
            InputDeviceComboBox.IsEnabled = true;
            RefreshInputDevicesButton.IsEnabled = true;
            InputLevelBar.Value = 0;
            InputLevelText.Text = "Input level: 0%";
        }
    }

    private async void RecoverRecordings_Click(object sender, RoutedEventArgs e)
    {
        if (NativeRecordingActive() != 0 || NativeProcessingActive() != 0)
        {
            RecoveryStatusText.Text = "Stop recording or processing before recovering a file.";
            return;
        }

        RecoverRecordingsButton.IsEnabled = false;
        try
        {
            var picker = new FolderPicker();
            picker.FileTypeFilter.Add("*");
            InitializeWithWindow.Initialize(picker, WindowNative.GetWindowHandle(this));
            var folder = await picker.PickSingleFolderAsync();
            if (folder is null)
            {
                RecoveryStatusText.Text = "Recovery cancelled. No files were changed.";
                return;
            }

            var candidates = Directory.GetFiles(folder.Path)
                .Where(path => System.Text.RegularExpressions.Regex.IsMatch(
                    Path.GetFileName(path), @"\.partial-[0-9]+$"))
                .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
                .ToArray();
            if (candidates.Length == 0)
            {
                RecoveryStatusText.Text = "No Audio Studio temporary recording files were found in that folder.";
                return;
            }

            foreach (var partialPath in candidates)
            {
                if (NativeRecordingActive() != 0 || NativeProcessingActive() != 0)
                {
                    RecoveryStatusText.Text = "Recovery stopped because another audio operation became active.";
                    return;
                }

                var outputPath = System.Text.RegularExpressions.Regex.Replace(
                    partialPath, @"\.partial-[0-9]+$", "");
                var prompt = new Microsoft.UI.Xaml.Controls.ContentDialog
                {
                    Title = "Interrupted recording found",
                    Content = $"{Path.GetFileName(partialPath)}\n\nAttempt WAV validation and recovery? Existing recordings will not be overwritten. Invalid temporary files will be retained.",
                    PrimaryButtonText = "Recover",
                    CloseButtonText = "Skip",
                    DefaultButton = Microsoft.UI.Xaml.Controls.ContentDialogButton.Primary,
                    XamlRoot = Content.XamlRoot
                };
                var choice = await prompt.ShowAsync();
                if (choice != Microsoft.UI.Xaml.Controls.ContentDialogResult.Primary)
                    continue;

                RecoveryStatusText.Text = $"Validating {Path.GetFileName(partialPath)}…";
                var nativeMessage = await Task.Run(() =>
                    Marshal.PtrToStringUni(NativeRecoverPartialWav(partialPath, outputPath))
                    ?? "Native recovery returned no status.");
                RecoveryStatusText.Text = nativeMessage == "RECOVERED"
                    ? $"Recovered: {Path.GetFileName(outputPath)}"
                    : nativeMessage;
            }
        }
        catch (Exception exception)
        {
            RecoveryStatusText.Text = $"Recovery could not complete: {exception.Message}";
        }
        finally
        {
            RecoverRecordingsButton.IsEnabled = true;
        }
    }

    private async void ProcessWav_Click(object sender, RoutedEventArgs e)
    {
        if (NativeRecordingActive() != 0)
        {
            ProcessingStatusText.Text = "Stop and save the current recording before processing a WAV.";
            return;
        }

        var openPicker = new FileOpenPicker();
        openPicker.FileTypeFilter.Add(".wav");
        InitializeWithWindow.Initialize(openPicker, WindowNative.GetWindowHandle(this));
        var input = await openPicker.PickSingleFileAsync();
        if (input is null)
        {
            ProcessingStatusText.Text = "Processing cancelled. No files were changed.";
            return;
        }

        await ProcessWavFileAsync(input);
    }

    private async Task ProcessWavFileAsync(StorageFile input)
    {
        if (NativeProcessingActive() != 0)
        {
            ProcessingStatusText.Text = "Another WAV processing job is already running.";
            return;
        }

        if (NativeRecordingActive() != 0)
        {
            ProcessingStatusText.Text = "Stop and save the current recording before processing a WAV.";
            return;
        }

        ProcessWavButton.IsEnabled = false;
        try
        {
            var folderPicker = new FolderPicker();
            folderPicker.FileTypeFilter.Add("*");
            InitializeWithWindow.Initialize(folderPicker, WindowNative.GetWindowHandle(this));
            var folder = await folderPicker.PickSingleFolderAsync();
            if (folder is null)
            {
                ProcessingStatusText.Text = "Choose an output folder to continue.";
                return;
            }

            var outputName = $"{Path.GetFileNameWithoutExtension(input.Name)}-processed.wav";
            var outputPath = Path.Combine(folder.Path, outputName);
            StopPreview();
            ProcessingStatusText.Text = "Starting streamed WAV processing…";
            var highPassHz = HighPassSlider.Value < 1 ? 0 : Math.Min(500, 20 + ((int)HighPassSlider.Value - 1) * 10);
            var started = NativeProcessingStart(input.Path, outputPath,
                (float)GateThresholdSlider.Value, (float)GateAttenuationSlider.Value,
                (float)DspGainSlider.Value, highPassHz,
                (ProcessingEngineComboBox.SelectedItem as ComboBoxItem)?.Content?.ToString() switch
                {
                    "Classic noise gate" => 0,
                    "DeepFilterNet3 (BETA)" => 2,
                    _ => 1
                },
                (float)(DenoiserStrengthSlider.Value / 100.0),
                System.IO.Path.Combine(AppContext.BaseDirectory, "models"));
            if (started == 0)
            {
                ProcessingStatusText.Text = ReadProcessingStatus();
                return;
            }

            RecordToggleButton.IsEnabled = false;
            CancelProcessingButton.IsEnabled = true;
            ProcessingProgressBar.Value = 0;
            _processingTimer.Start();
        }

        catch (Exception exception)
        {
            ProcessingStatusText.Text = $"Could not process WAV: {exception.Message}";
        }
        finally
        {
            if (NativeProcessingActive() == 0)
                ProcessWavButton.IsEnabled = true;
        }
    }

    private void CancelProcessing_Click(object sender, RoutedEventArgs e)
    {
        CancelProcessingButton.IsEnabled = false;
        ProcessingStatusText.Text = "Cancelling and cleaning up the partial output…";
        NativeProcessingCancel();
    }

    private void UpdateProcessingStatus()
    {
        try
        {
            ProcessingProgressBar.Value = Math.Clamp((double)NativeProcessingProgress(), 0.0, 1.0);
            ProcessingStatusText.Text = ReadProcessingStatus();
            if (NativeProcessingActive() == 0)
            {
                _processingTimer.Stop();
                ProcessWavButton.IsEnabled = true;
                CancelProcessingButton.IsEnabled = false;
                RecordToggleButton.IsEnabled = true;
                UpdateRecordToggleVisual(isRecording: false);
            }
        }
        catch (Exception exception)
        {
            _processingTimer.Stop();
            ProcessingStatusText.Text = $"Processing status unavailable: {exception.Message}";
        }
    }

    private static string ReadProcessingStatus()
    {
        try
        {
            return Marshal.PtrToStringUni(NativeProcessingStatus())
                ?? "Native processor returned no status.";
        }
        catch (Exception exception)
        {
            return $"Processing status unavailable: {exception.Message}";
        }
    }

    private async void ChooseOriginalPreview_Click(object sender, RoutedEventArgs e)
    {
        _originalPreviewFile = await ChoosePreviewFileAsync();
        OriginalPreviewFileText.Text = _originalPreviewFile is null
            ? "Original: no file selected"
            : $"Original: {_originalPreviewFile.Name}";
        PreviewOriginalButton.IsEnabled = _originalPreviewFile is not null;
    }

    private async void ChooseProcessedPreview_Click(object sender, RoutedEventArgs e)
    {
        _processedPreviewFile = await ChoosePreviewFileAsync();
        ProcessedPreviewFileText.Text = _processedPreviewFile is null
            ? "Processed: no file selected"
            : $"Processed: {_processedPreviewFile.Name}";
        PreviewProcessedButton.IsEnabled = _processedPreviewFile is not null;
    }

    private async Task<StorageFile?> ChoosePreviewFileAsync()
    {
        try
        {
            var picker = new FileOpenPicker();
            picker.FileTypeFilter.Add(".wav");
            InitializeWithWindow.Initialize(picker, WindowNative.GetWindowHandle(this));
            return await picker.PickSingleFileAsync();
        }
        catch (Exception exception)
        {
            PreviewStatusText.Text = $"Could not choose preview WAV: {exception.Message}";
            return null;
        }
    }

    private async void PreviewOriginal_Click(object sender, RoutedEventArgs e)
    {
        if (_originalPreviewFile is not null)
            await PlayPreviewAsync(_originalPreviewFile, "original");
    }

    private async void PreviewProcessed_Click(object sender, RoutedEventArgs e)
    {
        if (_processedPreviewFile is not null)
            await PlayPreviewAsync(_processedPreviewFile, "processed");
    }

    private async Task PlayPreviewAsync(StorageFile file, string label)
    {
        _trimPreviewTimer.Stop();
        _trimPreviewEndPosition = TimeSpan.Zero;
        PreviewTrimRangeButton.IsEnabled = !string.IsNullOrWhiteSpace(_waveformFilePath) && _waveformTotalFrames > 1;
        try
        {
            if (NativeRecordingActive() != 0 || NativeProcessingActive() != 0)
            {
                PreviewStatusText.Text = "Stop recording or processing before previewing audio.";
                return;
            }
            _previewPlayer.Pause();
            _previewPlayer.Source = null;
            _previewPlayer.Source = MediaSource.CreateFromStorageFile(file);
            _previewPlayer.Play();
            StopPreviewButton.IsEnabled = true;
            PreviewStatusText.Text = $"Playing {label}: {file.Name}";
            await Task.CompletedTask;
        }
        catch (Exception exception)
        {
            PreviewStatusText.Text = $"Could not play preview: {exception.Message}";
        }
    }

    private void StopPreview_Click(object sender, RoutedEventArgs e) => StopPreview();

    private void StopPreview()
    {
        StopTrimRangePreview(updateStatus: false);
        _previewPlayer.Pause();
        _previewPlayer.Source = null;
        StopPreviewButton.IsEnabled = false;
        PreviewStatusText.Text = "Preview is stopped.";
    }

    private void UpdateInputMeter()
    {
        try
        {
            var peak = Math.Clamp((double)NativeRecordingPeak(), 0.0, 1.0);
            InputLevelBar.Value = peak;
            InputLevelText.Text = $"Input level: {peak:P0}";
            if (NativeRecordingClipping() != 0)
                ClippingWarningText.Text = "CLIPPING DETECTED — lower your microphone/input gain.";

            // A disconnected microphone can stop capture without the user pressing Stop.
            // Reconcile the UI from the native state instead of leaving a stale meter
            // and a stale recording toggle indefinitely.
            if (NativeRecordingActive() == 0 && _recordingUiActive)
            {
                _meterTimer.Stop();
                _recordingUiActive = false;
                RecordToggleButton.IsEnabled = NativeProcessingActive() == 0;
                UpdateRecordToggleVisual(isRecording: false);
                InputDeviceComboBox.IsEnabled = true;
                RefreshInputDevicesButton.IsEnabled = true;
                InputLevelBar.Value = 0;
                InputLevelText.Text = "Input level: 0%";
                RecordingStatusText.Text = ReadRecordingStatus();
                RefreshInputDevices();
            }
        }
        catch (Exception exception)
        {
            _meterTimer.Stop();
            InputLevelText.Text = $"Input meter unavailable: {exception.Message}";
        }
    }

    private static string? ReadRecordingTakeDetails(string path)
    {
        try
        {
            var peaks = new float[4];
            var result = NativeAnalyzeWavWaveform(path, 1U, peaks, out var frames, out var sampleRate, out var channels);
            if (result <= 0 || frames == 0 || sampleRate == 0 || (channels != 1 && channels != 2))
                return null;
            var totalSeconds = frames / sampleRate;
            var totalMinutes = totalSeconds / 60UL;
            var remainingSeconds = totalSeconds % 60UL;
            var fileSizeMb = new FileInfo(path).Length / (1024.0 * 1024.0);
            var channelLabel = channels == 1 ? "Mono" : "Stereo";
            return $"Take details: {totalMinutes:00}:{remainingSeconds:00} · {sampleRate / 1000.0:0.#} kHz · {channelLabel} · {fileSizeMb:0.00} MB";
        }
        catch
        {
            // Metadata is helpful, but a metadata read must never invalidate a finalized recording.
            return null;
        }
    }

    private static string ReadRecordingStatus()
    {
        try
        {
            return Marshal.PtrToStringUni(NativeRecordingStatus())
                ?? "Native recorder returned no status.";
        }
        catch (Exception exception)
        {
            return $"Recorder status unavailable: {exception.Message}";
        }
    }
}
