# Creator named drafts — implementation contract

## Scope
Named script drafts are snapshots of editable Creator script content. They are distinct from the active Creator project and from audio takes.

## Safety rules
- A draft has a stable UUID, user-visible name, created/updated UTC timestamps, and a script snapshot (script title and ordered sections).
- Saving updates the selected draft snapshot; creating a draft snapshots current editor content.
- Switching drafts first commits pending editor text to the active project, then loads the selected snapshot.
- Deleting a draft requires confirmation. The active draft cannot be deleted if it is the only draft; when deleting the active draft, select another existing draft before returning to the editor.
- Draft operations must never clone, remove, move, rename, or rewrite project takes or their source audio files.
- Draft data is bounded and validated before persistence/import. Malformed draft data must not replace valid data.
- Android and Windows use equivalent behavior and compatible JSON fields.

## Initial JSON shape
```json
{
  "schemaVersion": 1,
  "drafts": [
    {
      "draftId": "uuid",
      "name": "Draft name",
      "createdUtc": "ISO-8601 UTC",
      "updatedUtc": "ISO-8601 UTC",
      "script": {
        "scriptId": "uuid",
        "title": "Script title",
        "createdUtc": "ISO-8601 UTC",
        "updatedUtc": "ISO-8601 UTC",
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
    }
  ],
  "activeDraftId": "uuid"
}
```

This contract does not claim UI or runtime functionality is complete; it guides the native implementation and its tests.
