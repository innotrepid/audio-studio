using System;
using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;

namespace AudioStudio;

public sealed partial class MainWindow : Window
{
    [DllImport("audio_studio_native.dll", EntryPoint = "audio_studio_engine_status", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeEngineStatus();

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
        }
    }
}
