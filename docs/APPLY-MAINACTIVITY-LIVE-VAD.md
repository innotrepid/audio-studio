# Apply MainActivity live-VAD patch

Native Oboe dual-submit is already on Redesign (PR #126). This applies the Kotlin UI wiring.

## Apply (from repo root on this branch)

```bash
git apply patches/mainactivity-live-vad.patch
git add platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt
git commit -m "fix(android): apply MainActivity live Silero VAD patch"
git push
```

## What changes

1. JNI: `nativeStartRecordingWithVad`, `nativePendingVadRegions`, `nativePopVadRegion`
2. Start: `BundledModelStore.tryResolve` → Silero path → live VAD when available; plain recording fallback
3. Poll completed regions every 250ms on the main thread (status line only)
4. Stop: remove VAD poll callbacks

## Verify

```bash
grep -n nativeStartRecordingWithVad platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt
```
