# Windows application

The Windows shell uses C# and WinUI 3. It calls a narrow C ABI exported by the shared C++ engine DLL; audio processing must remain outside the UI thread.

## Build

Use Visual Studio 2022 with the Windows App SDK / WinUI tooling and .NET 8. The repository CI builds the C++ core and native DLL with CMake, then builds the unpackaged WinUI shell.

## Current limitations

This is an application-shell milestone, not a recorder. It does not open a microphone or write audio files. The next milestone implements capture, device handling, and safe WAV finalization on each platform.
