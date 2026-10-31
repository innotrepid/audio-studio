# Windows application

The Windows shell uses C# and WinUI 3. It calls a narrow C ABI exported by the shared C++ engine DLL; capture and audio processing work are kept outside the UI thread.

## Build

Use Visual Studio 2022 with the Windows App SDK / WinUI tooling and .NET 8. Repository CI builds and tests the C++ core, then builds the WinUI shell and packages the native engine DLL beside it.

CI publishes an **unpackaged, self-contained Windows x64 build** as the `audio-studio-windows-self-contained` artifact. It opts into Windows App SDK self-contained deployment and includes the native audio engine DLL, so it should not require the Windows App Runtime 1.6 framework package to be preinstalled. Download and extract the complete artifact folder, then launch `Synvidra.exe` without moving it away from its companion files. This is a portable deployment folder, not an MSIX installer; the Windows OS minimum still applies.

## Recording and recovery

The Windows shell supports microphone selection, WAV recording, input metering and clipping indication, WAV noise processing, and original-versus-processed preview. Use **Check for interrupted recordings…** and select the recording folder to attempt recovery of a writer-owned `.partial-N` file after a capture failure. Recovery may fail if a file is truncated or damaged; a partial file is not guaranteed to be recoverable. A normal Stop and save publishes a finalized WAV.

## Windows startup diagnostics

If initialization or main-window creation throws a managed exception, the app attempts to show a Windows error dialog and append details to:

`%LOCALAPPDATA%\AudioStudio\startup.log`

Unhandled application, WinUI UI-thread, and unobserved task exceptions are also logged where possible. These diagnostics cannot capture every failure: for example, Windows may terminate the process before managed code starts or before the logger can run. If no dialog or log appears, check **Event Viewer → Windows Logs → Application** for an `Synvidra.exe`, `.NET Runtime`, or `Application Error` entry.


## UI and product direction

The Windows application follows the Hybrid Studio direction: a persistent left navigation rail and desktop-first workspaces for Record, Editor, Creator, and Library. The Creator destination and its script/teleprompter/transcript workflows are staged product work; do not assume every planned Creator capability is already implemented. The primary interactive accent is blue, with mint reserved for live/healthy audio feedback.
