using System;
using System.Collections.Generic;
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

    private readonly DispatcherTimer _meterTimer = new() { Interval = TimeSpan.FromMilliseconds(100) };

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
        Closed += (_, _) =>
        {
            _meterTimer.Stop();
            NativeRecordingStop();
        };
    }

    private async void StartRecording_Click(object sender, RoutedEventArgs e)
    {
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
