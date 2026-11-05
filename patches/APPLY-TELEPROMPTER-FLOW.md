# Apply Creator teleprompter Record flow

Branch tip still has a **stub** `MainWindow.xaml` (~467 bytes). Full UI is on Redesign tip (~46KB).

## Fix CI + finish feature (local)

```bash
git checkout feat/creator-teleprompter-record-flow

# Restore full teleprompter XAML from Redesign (already has large Record layout)
git show 'Redesign,-Teleprompter,-captions-&-more:platforms/windows/AudioStudio/MainWindow.xaml' \
  > platforms/windows/AudioStudio/MainWindow.xaml

# Windows code-behind: apply call-site patch, then paste method snippet
git apply patches/mainwindow-cs-teleprompter-flow.patch || true
# Insert contents of patches/sync-studio-teleprompter-method.cs.txt
# before RefreshCreatorProductionMarkers() in MainWindow.xaml.cs

# Android
git apply patches/mainactivity-teleprompter-flow.patch

git add platforms/windows/AudioStudio/MainWindow.xaml \
        platforms/windows/AudioStudio/MainWindow.xaml.cs \
        platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt
git commit -m "feat: Creator Teleprompter owns settings; large studio teleprompter on Record"
git push
```

**Verify sizes**
- `MainWindow.xaml` ~46KB, contains `CreatorTeleprompterWorkspace` and `MinHeight="640"`
- `MainActivity.kt` contains `studioTeleprompterHeight` and `creatorTeleprompterPanel`

See `docs/CREATOR-TELEPROMPTER-FLOW.md`.
