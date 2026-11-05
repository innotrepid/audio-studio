# Creator teleprompter Record flow (Windows UI)

Docs are updated on this branch. Apply the Windows UI patches below (or paste the full downloadable files from the agent session).

## Apply

```bash
git checkout feat/creator-teleprompter-record-flow
# Restore stock XAML if the file was corrupted by an intermediate push:
git show Redesign,-Teleprompter,-captions-&-more:platforms/windows/AudioStudio/MainWindow.xaml > platforms/windows/AudioStudio/MainWindow.xaml
git apply patches/mainwindow-teleprompter-flow.patch
git apply patches/mainwindow-cs-teleprompter-flow.patch
```

## Behavior after apply

- Record panel: large teleprompter, compact chrome, no settings sliders on Record
- Creator → Teleprompter: script mirror + settings (speed, size, spacing, pre-roll)
- Record start always starts teleprompter scroll
- Pause pauses recording + scroll

See `docs/CREATOR-TELEPROMPTER-FLOW.md`.
