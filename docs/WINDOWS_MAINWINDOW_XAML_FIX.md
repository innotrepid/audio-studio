# Windows MainWindow.xaml startup fix

## Symptom

```
Synvidra could not open its main window.
Microsoft.UI.Xaml.Markup.XamlParseException: XAML parsing failed.
```

## Cause

On the Redesign branch, `platforms/windows/AudioStudio/MainWindow.xaml` was
committed as a **44-byte placeholder** (`<!-- restored in follow-up; see scripts -->`).
WinUI cannot parse that as a `Window`, so the app never opens.

CI ran `scripts/ci_restore_mainwindow_xaml.py` before build, so CI could pass while
local / published builds from the raw tree still failed.

## Fix

1. Restore script now falls back to **HTTPS raw** fetch of known-good SHA
   `ebccf66112441468d533bb80ed450c8e7257b909` when git history is shallow.
2. After merge, run locally once:
   `python scripts/ci_restore_mainwindow_xaml.py`
   then rebuild the Windows app — or use a CI artifact.

## Verify

- `MainWindow.xaml` is **>10 KB** and contains `<Window` and `PauseTeleprompterButton`
- App starts without the startup error dialog
