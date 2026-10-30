# Project and Script Data Contract

**Status:** Version 1 contract for Creator foundation implementation  
**Platforms:** Android and Windows  
**Persistence:** Local-first metadata; audio remains in its existing media locations

## Goals

- Give projects, scripts, sections, and takes stable identities independent of display order or file names.
- Keep Creator metadata separate from audio bytes.
- Make writes recoverable and safe against interruption.
- Allow Android and Windows to read and write the same logical format.
- Keep standalone recordings usable without requiring a project.

## Version 1 document

A project document is UTF-8 JSON with a required `schemaVersion` of `1`.

```json
{
  "schemaVersion": 1,
  "projectId": "uuid",
  "title": "Untitled project",
  "createdUtc": "2026-10-30T00:00:00Z",
  "updatedUtc": "2026-10-30T00:00:00Z",
  "scripts": [
    {
      "scriptId": "uuid",
      "title": "Untitled script",
      "createdUtc": "2026-10-30T00:00:00Z",
      "updatedUtc": "2026-10-30T00:00:00Z",
      "sections": [
        {
          "sectionId": "uuid",
          "title": "Opening",
          "text": "",
          "order": 0,
          "notes": "",
          "cues": []
        }
      ]
    }
  ],
  "takes": []
}
```

The example timestamps are illustrative. Implementations generate UTC timestamps at write time.

## Identity and ordering

- IDs are UUIDs generated once when an entity is created.
- Renaming or reordering never changes an ID.
- Section order is an integer; array order should be normalized to match it when saving.
- A section reference is valid only when its `sectionId` exists in the referenced script.
- A take may be standalone: `projectId` and `sectionId` are optional until explicitly associated.
- Never use a display title, array index, or word index as a stable identifier.

## Take metadata

A take record may contain:

- `takeId`: stable UUID.
- `sourceUri` or `sourcePath`: platform-appropriate reference to the existing audio file.
- `createdUtc`, `durationMs`, `sampleRateHz`, and `channels`: verified metadata when available.
- `scriptId` and `sectionId`: optional association.
- `status`: an explicit review state such as `recorded`, `needsReview`, or `approved`.

The project store must not move, rename, rewrite, or delete audio files. File references are metadata only. If a source is unavailable, preserve the project record and report the missing asset; do not silently discard it.

## Verified recording association

The current Creator foundation associates a take only after the native recorder has finalized the WAV and platform-specific WAV validation succeeds.

- Android stores the durable MediaStore content URI when the recording is exported; if public export is unavailable, it stores the app-private file path.
- Windows stores the verified WAV's full path.
- Both platforms record a new take UUID, UTC creation time, duration in milliseconds, sample rate, channel count, script ID, section ID, and the `recorded` status.
- The initial association targets the first script and its first section because the current Scripts workspace edits that section. A future multi-script/section selector must supply explicit stable IDs rather than relying on display order.
- The project save is separate from WAV finalization. If metadata association fails, the UI reports that separately and preserves the already-verified recording.
- Associating a take never copies, moves, renames, rewrites, or deletes its audio source.

## Validation

Before accepting a loaded document:

1. Require a JSON object and supported positive integer `schemaVersion`.
2. Reject unknown future schema versions with a clear unsupported-version message; do not rewrite them.
3. Require non-empty project and script IDs and titles.
4. Reject duplicate project, script, section, and take IDs within the document.
5. Require section IDs to be unique within their script and order values to be non-negative.
6. Verify optional script/section references before linking takes.
7. Bound document size and text lengths in the implementation to prevent accidental unbounded input.
8. Treat malformed or invalid input as a failed load; keep the last known-good in-memory state unchanged.

## Atomic persistence and recovery

- Store metadata in app-private/local application data, not beside source WAVs by default.
- Write the complete next document to a temporary sibling file.
- Flush and close the temporary file before replacing the active document.
- Keep a last-known-good backup or recoverable previous generation.
- On startup, validate the primary document first; if invalid, attempt the previous valid generation and clearly report recovery.
- Never replace the last-known-good file with malformed or partially serialized data.
- Autosave is debounced after edits; explicit save/flush occurs before leaving the editor and on application shutdown where possible.
- UI must distinguish saved, saving, and save-failed states. A failed save must not be reported as successful.

## Cross-platform storage

- Android: app-private files directory. Use an atomic file-replacement mechanism; do not request broad storage permission for project metadata.
- Windows: `%LOCALAPPDATA%/AudioStudio/Projects`. Use a temporary file and replace/move operation on the same volume.
- Both platforms use the same JSON field names, UUID semantics, UTC timestamp format, validation rules, and migration policy.

## Migration policy

- The first persisted format is schema version 1.
- Every future schema change increments `schemaVersion` and has an explicit migration.
- Migrations operate on a copy and write atomically only after the migrated document passes validation.
- Unsupported or failed migrations preserve the original document and provide a recoverable error.
- Standalone WAV files and current Library discovery remain available independently of project migration.

## Implementation and test gates

The implementation is not considered complete until both platforms demonstrate:

- Create, rename, and reopen a project.
- Create, edit, reorder, and delete scripts/sections.
- Autosave survives normal navigation and application restart.
- Simulated interrupted writes recover the previous valid document.
- Malformed JSON, duplicate IDs, missing references, and unsupported versions fail safely.
- A failed save leaves the last-known-good document unchanged.
- Missing source audio is reported without deleting project metadata.
- A standalone recording remains accessible without a project association.

This contract defines the data boundary; it does not claim that project persistence or script editing is already implemented.
