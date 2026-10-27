using System;
using System.Collections.Generic;
using System.IO;
using Windows.Media.Core;
using Windows.Media.Playback;
using Windows.Storage;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Windows.Storage.Pickers;
using WinRT.Interop;

namespace AudioStudio;

public sealed partial class MainWindow : Window
{
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

    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_processing_start", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private static extern int NativeProcessingStart([MarshalAs(UnmanagedType.LPWStr)] string inputPath,
        [MarshalAs(UnmanagedType.LPWStr)] string outputPath, float thresholdDbfs, float attenuationDb);

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
    private readonly MediaPlayer _previewPlayer = new();
    private StorageFile? _originalPreviewFile;
    private StorageFile? _processedPreviewFile;
    private readonly List<CaptureDeviceChoice> _captureDevices = new();

    private sealed record CaptureDeviceChoice(string Name, string Id)
    {
        public override string ToString() => Name;
    }

    public MainWindow()
    {
        InitializeComponent();
        try
        {
            var pointer = NativeEngineStatus();
            EngineStatusText.Text = Marshal.PtrToStringAnsi(pointer)
                ?? "Native engine returned an empty status.";
        }
        catch (Exception exception)
        {
            EngineStatusText.Text = $"Native engine unavailable: {exception.Message}";
            StartRecordingButton.IsEnabled = false;
        }

        _meterTimer.Tick += (_, _) => UpdateInputMeter();
        Loaded += (_, _) => RefreshInputDevices();
        _processingTimer.Tick += (_, _) => UpdateProcessingStatus();
        GateThresholdSlider.ValueChanged += (_, _) =>
            GateThresholdText.Text = $"Noise threshold: {GateThresholdSlider.Value:0} dBFS";
        GateAttenuationSlider.ValueChanged += (_, _) =>
            GateAttenuationText.Text = $"Noise attenuation: {GateAttenuationSlider.Value:0} dB";
        Closed += (_, _) =>
        {
            _meterTimer.Stop();
            _processingTimer.Stop();
            NativeProcessingCancel();
            NativeRecordingStop();
            StopPreview();
            _previewPlayer.Dispose();
        };
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
        StartRecordingButton.IsEnabled = false;
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
                StartRecordingButton.IsEnabled = false;
                StopRecordingButton.IsEnabled = true;
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
                StartRecordingButton.IsEnabled = true;
        }
    }

    private async void StopRecording_Click(object sender, RoutedEventArgs e)
    {
        StopRecordingButton.IsEnabled = false;
        RecordingStatusText.Text = "Finalizing WAV file…";
        try
        {
            _meterTimer.Stop();
            await Task.Run(NativeRecordingStop);
            UpdateInputMeter();
            RecordingStatusText.Text = ReadRecordingStatus();
        }
        catch (Exception exception)
        {
            RecordingStatusText.Text = $"Could not stop recording cleanly: {exception.Message}";
        }
        finally
        {
            StartRecordingButton.IsEnabled = true;
            StopRecordingButton.IsEnabled = false;
            InputLevelBar.Value = 0;
            InputLevelText.Text = "Input level: 0%";
        }
    }

    private async void ProcessWav_Click(object sender, RoutedEventArgs e)
    {
        if (NativeRecordingActive() != 0)
        {
            ProcessingStatusText.Text = "Stop and save the current recording before processing a WAV.";
            return;
        }

        ProcessWavButton.IsEnabled = false;
        try
        {
            var openPicker = new FileOpenPicker();
            openPicker.FileTypeFilter.Add(".wav");
            InitializeWithWindow.Initialize(openPicker, WindowNative.GetWindowHandle(this));
            var input = await openPicker.PickSingleFileAsync();
            if (input is null)
            {
                ProcessingStatusText.Text = "Processing cancelled. No files were changed.";
                return;
            }

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
            var started = NativeProcessingStart(input.Path, outputPath,
                (float)GateThresholdSlider.Value, (float)GateAttenuationSlider.Value);
            if (started == 0)
            {
                ProcessingStatusText.Text = ReadProcessingStatus();
                return;
            }

            StartRecordingButton.IsEnabled = false;
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
                StartRecordingButton.IsEnabled = true;
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
        }
        catch (Exception exception)
        {
            _meterTimer.Stop();
            InputLevelText.Text = $"Input meter unavailable: {exception.Message}";
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
