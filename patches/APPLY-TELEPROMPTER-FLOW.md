# Apply Creator teleprompter Record flow patches

On `feat/creator-teleprompter-record-flow`:

```bash
# 1) Restore full Windows XAML from Redesign tip (branch currently has a stub)
git show 'Redesign,-Teleprompter,-captions-&-more:platforms/windows/AudioStudio/MainWindow.xaml' \
  > platforms/windows/AudioStudio/MainWindow.xaml

# 2) Apply code-behind + Android patches
git apply patches/mainwindow-cs-teleprompter-flow.patch
git apply patches/mainactivity-teleprompter-flow.patch

# If Redesign already has the teleprompter XAML layout, skip the XAML patch.
# If not, also: git apply patches/mainwindow-teleprompter-flow.patch

git add platforms/windows/AudioStudio/MainWindow.xaml \
        platforms/windows/AudioStudio/MainWindow.xaml.cs \
        platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt
git commit -m "feat: Creator Teleprompter owns settings; large studio teleprompter on Record"
git push
```

**Verify:** `MainWindow.xaml` should be ~46KB and contain `CreatorTeleprompterWorkspace` and `MinHeight="640"`.
