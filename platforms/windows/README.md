# Windows application

The Windows shell uses C# and WinUI 3. It calls a narrow C ABI exported by the shared C++ engine DLL; audio processing must remain outside the UI thread.

## Build

Use Visual Studio 2022 with the Windows App SDK / WinUI tooling and .NET 8. The repository CI builds the C++ core and native DLL with CMake, then builds the unpackaged WinUI shell.

The CI also publishes an **unpackaged, self-contained Windows x64 build** as the `audio-studio-windows-self-contained` artifact. It opts into Windows App SDK self-contained deployment and includes the native audio engine DLL, so it should not require the Windows App Runtime 1.6 framework package to be preinstalled. Download and extract the artifact, then launch `AudioStudio.exe`. This is a portable deployment folder, not an MSIX installer; the app's Windows OS minimum still applies.

## Current limitations

This is an application-shell milestone, not a recorder. It does not open a microphone or write audio files. The next milestone implements capture, device handling, and safe WAV finalization on each platform.


## Capture failure recovery

If Windows microphone capture fails after recording has begun (for example, a device disconnect or a write error), Audio Studio keeps the writer-owned temporary `.partial-N` file rather than deleting the take. Use **Check for interrupted recordings…** and select the recording folder to attempt validated recovery. Recovery may fail if the file is truncated or damaged; a partial file is not guaranteed to be recoverable. A normal Stop and save still publishes a finalized WAV.
