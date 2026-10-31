# Synvidra Record / Teleprompter and Storage Contract

## Workspace behavior
- Record is a capture console, not a script editor or a document-scrolling page.
- Record must fit the available viewport without vertical scrolling. Keep elapsed time, input monitor, interrupted-recording recovery, one clear studio-take start/stop action, pause/resume, and open-latest-verified-take action visible.
- The teleprompter is the visual center of the Record workspace and displays the selected Creator script/section, never the scripts-list UI.
- Teleprompter preferences (scroll speed, text size, line spacing, countdown) belong to the Teleprompter workspace and persist independently of script content.
- Starting a studio take coordinates countdown, recording, and scroll. Pause/resume pauses/resumes both scroll and audio capture; it must not silently stop or discard a take.
- Creator > Teleprompter is a real workspace for script preview and teleprompter preferences, not a placeholder notice. Selection remains linked to Creator Scripts.

## Synvidra user-visible media storage
Use the platform's user-accessible Music location, never the storage root:
```
Music/
  Synvidra/
    Recordings/
    Processed/
    Subtitles/
    Exports/
```
- Recorded WAVs: `Synvidra_Recording_YYYYMMDD_HHMMSS.wav`
- Processed WAVs: `Synvidra_Processed_YYYYMMDD_HHMMSS.wav`
- Subtitle files: `Synvidra_Subtitles_YYYYMMDD_HHMMSS.srt` or `.vtt`
- Other exports: `Synvidra_Export_<descriptive-type>_YYYYMMDD_HHMMSS.<ext>`
- Keep private project metadata, settings, drafts, and crash-recovery partials in app-private storage. Recovery must remain possible after interruption; publish a verified take to the public Synvidra folder only after finalization.
- Android must use scoped-storage-compatible MediaStore/SAF publication, not hard-coded root paths. Native recording may stage privately, then atomically publish/copy verified output.
- Windows uses the user's Music known folder and creates the same Synvidra subfolder structure.
- Existing files are not overwritten or silently moved. Resolve filename collisions with a suffix and preserve source audio for processing/trim operations.

## Acceptance checks
1. Record has no outer vertical scroll; teleprompter occupies the remaining space and stays centered.
2. Speed, text size, line spacing, and countdown are available in Teleprompter workspace and retained after navigation/relaunch.
3. Selected script/section is reflected in both Teleprompter workspace and Record.
4. Pause/resume pauses/resumes native audio capture and teleprompter scrolling together; elapsed time reflects active capture time. On Windows, pause/resume requests are serviced by the WASAPI capture worker thread, and queued pre-pause samples are drained before capture resumes.
5. Recovery and latest verified take handoff remain functional.
6. New recording, processed WAV, subtitles, and exports appear under Music/Synvidra in their respective folders on both platforms.
7. App-private partial recordings remain recoverable; original files are never overwritten.
