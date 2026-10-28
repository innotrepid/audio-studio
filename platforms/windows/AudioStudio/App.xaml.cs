using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.UI.Xaml;

namespace AudioStudio;

public partial class App : Application
{
    private Window? _window;
    private static readonly string LogPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "AudioStudio",
        "startup.log");

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int MessageBoxW(IntPtr hWnd, string text, string caption, uint type);

    public App()
    {
        AppDomain.CurrentDomain.UnhandledException += (_, args) =>
        {
            if (args.ExceptionObject is Exception exception)
                LogException("Unhandled application exception", exception);
            else
                LogText($"Unhandled application exception: {args.ExceptionObject}");
        };

        UnhandledException += (_, args) =>
        {
            LogException("Unhandled WinUI UI-thread exception", args.Exception);
        };

        System.Threading.Tasks.TaskScheduler.UnobservedTaskException += (_, args) =>
        {
            LogException("Unobserved task exception", args.Exception);
            args.SetObserved();
        };

        try
        {
            InitializeComponent();
        }
        catch (Exception exception)
        {
            ShowStartupFailure("Application initialization failed.", exception);
            throw;
        }
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        try
        {
            _window = new MainWindow();
            _window.Activate();
        }
        catch (Exception exception)
        {
            ShowStartupFailure("Audio Studio could not open its main window.", exception);
            throw;
        }
    }

    private static void ShowStartupFailure(string message, Exception exception)
    {
        LogException(message, exception);
        var details = $"{message}\n\n{exception.GetType().FullName}: {exception.Message}\n\n" +
                      $"A diagnostic log was written to:\n{LogPath}";
        try
        {
            MessageBoxW(IntPtr.Zero, details, "Audio Studio startup error", 0x10);
        }
        catch
        {
            // Keep the original startup exception if the diagnostic dialog itself fails.
        }
    }

    private static void LogException(string context, Exception exception) =>
        LogText($"{context}{Environment.NewLine}{exception}");

    private static void LogText(string message)
    {
        try
        {
            var directory = Path.GetDirectoryName(LogPath)!;
            Directory.CreateDirectory(directory);
            File.AppendAllText(
                LogPath,
                $"[{DateTimeOffset.Now:O}] {message}{Environment.NewLine}{Environment.NewLine}",
                Encoding.UTF8);
        }
        catch
        {
            // Startup diagnostics must never prevent the app from launching.
        }
    }
}
