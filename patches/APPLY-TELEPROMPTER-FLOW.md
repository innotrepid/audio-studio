# Apply Creator teleprompter Record flow

Prefer merging `feat/creator-teleprompter-clean` into Redesign.

CI runs `scripts/ci_apply_teleprompter_ui.py` to:
- inject missing InputLevel/Recovery named controls into `MainWindow.xaml` if needed
- define `SyncStudioTeleprompterFromCreator` in code-behind if call sites exist without the method
- apply Android studio-teleprompter layout transforms when markers are absent

See `docs/CREATOR-TELEPROMPTER-FLOW.md`.
