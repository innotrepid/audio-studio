# Windows XamlParseException and orphan Click handlers

## Symptom

```
XamlParseException: XAML parsing failed.
at MainWindow.InitializeComponent()
```

CI can still be **green**: MSBuild does not always require XAML `Click` targets to exist;
WinUI binds them at **LoadComponent** time.

## Cause

CI scripts inject Caption/phrase **buttons** into `MainWindow.xaml` with `Click="Some_Click"`.
Matching methods are injected separately into `MainWindow.xaml.cs`. If the CS inject
misses its anchor, the button remains and startup throws.

Examples of handlers that were in XAML injects but not always in committed CS:

- `TeleprompterPrevSection_Click` / `TeleprompterNextSection_Click`
- `LinkTranscriptToFirstTake_Click`, `SavePhraseReplacementRequest_Click`, …

## Fix

`scripts/ci_heal_windows_xaml_handlers.py` runs **after** all Windows apply steps and
**before** MSBuild: any `Click="Foo"` without `Foo` in the `.cs` file is stripped.

The app opens; some Caption buttons may be absent until CS injects are fixed.
