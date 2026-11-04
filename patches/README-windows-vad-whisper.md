# Apply remaining Windows UI/capture patches

Native API, CMake, `OfflineSpeechNative.cs`, `windows_transcription.cpp`, and docs are already on this branch.

Still need on this branch (large files):

1. `platforms/windows/native/windows_capture.cpp` — WASAPI dual-submit into `LiveCaptureVad`
2. `platforms/windows/AudioStudio/MainWindow.xaml.cs` — prefer VAD on record start + offline transcribe handler
3. `platforms/windows/AudioStudio/MainWindow.xaml` — Captions button "Transcribe last recording (offline)"

Download the full files from the conversation artifacts or re-apply the logic described in the PR body.
