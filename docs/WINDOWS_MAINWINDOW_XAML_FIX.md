# Windows MainWindow.xaml startup fix

## Symptom

```
XamlParseException: XAML parsing failed.
at MainWindow.InitializeComponent()
```

Path in the log like `D:\a\audio-studio\...` means a **GitHub Actions** build artifact.

## Causes seen on this branch

1. **44-byte stub** placeholder instead of a real `<Window>`.
2. **Minimal shell (~9 KB)** committed in #168 with `Click="NavRecord_Click"` etc.
   Those handlers **do not exist** in `MainWindow.xaml.cs`. WinUI resolves Click at
   **LoadComponent** time → `XamlParseException` (same dialog you see).
3. Section-nav buttons injected **without** matching `_Click` methods in code-behind.

## Fix direction

- `ci_restore_mainwindow_xaml.py` loads full known-good XAML from `ebccf66`
  (git or HTTPS) and **rejects** the minimal Nav*_Click shell.
- Section buttons + handlers only from `ci_apply_section_nav.py` (paired).
- After merge: wait for **green Windows CI**, install **new**
  `synvidra-windows-self-contained` artifact (not an older zip).

## Local verify before running the EXE

```powershell
python scripts/ci_restore_mainwindow_xaml.py
# MainWindow.xaml should be ~46 KB and contain NavigateRecordButton
# and must NOT contain NavRecord_Click
Select-String -Path platforms\windows\AudioStudio\MainWindow.xaml -Pattern "NavRecord_Click"
Select-String -Path platforms\windows\AudioStudio\MainWindow.xaml -Pattern "NavigateRecordButton"
```
