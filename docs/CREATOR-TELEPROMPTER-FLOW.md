# Creator → Teleprompter → Record flow

**Status:** Active — reading-position persistence on `feat/teleprompter-reading-position`  
**Platforms:** Android and Windows

## Product rule

1. **Scripts** (Creator → Scripts) is the place the user writes and organizes script text.
2. **Teleprompter** (Creator → Teleprompter) is the reading surface and **owns teleprompter settings** (scroll speed, text size, line spacing, default countdown). Script text appears here **automatically** from the active script/section in Scripts—no re-paste.
3. **Record** shows a **studio teleprompter** that consumes the **same text and settings** as Creator → Teleprompter. Record does **not** host teleprompter settings and does **not** read script text from the Scripts editor widgets directly.
4. **Record UI is not page-scrollable.** The studio teleprompter uses as much vertical space as possible. Keep only:
   - Studio teleprompter (dominant)
   - Essential recording controls (start/stop, pause)
   - Clipping / level indication
   - Compact status (elapsed, device, engine/recording status as needed)
5. **Start recording also starts teleprompter scroll** (after any pre-roll countdown).
6. **Pause pauses both** recording and teleprompter scroll; resume resumes both.
7. **Reading position** (scroll offset for the active section) is saved on pause/stop and restored when starting the same section again.

## Data ownership

| Data | Owner | Consumers |
|------|--------|-----------|
| Script body / sections | Creator Scripts + `CreatorProjectStore` | Creator Teleprompter (auto), Record studio teleprompter (via Teleprompter mirror) |
| Teleprompter settings | Creator Teleprompter (**persisted**) | Record studio teleprompter |
| Reading position | Teleprompter prefs (section + script + scrollY) | Record studio teleprompter on take start |
| Takes | Record engine + project take refs | Library / Editor / Production |

### Preference persistence

| Platform | Store | Keys |
|----------|--------|------|
| Android | `SharedPreferences` name `teleprompter_preferences` | `speed_progress`, `font_progress`, `spacing_progress`, `countdown_index`, `reading_section_id`, `reading_script_id`, `reading_scroll_y` |
| Windows | `ApplicationData.Current.LocalSettings` | `teleprompter.speed`, `teleprompter.fontSize`, `teleprompter.lineSpacing`, `teleprompter.countdownIndex`, `teleprompter.reading.sectionId`, `teleprompter.reading.scriptId`, `teleprompter.reading.scrollY` |

Helpers: `TeleprompterPreferences.kt` / `TeleprompterPreferences.cs`. CI applies UI wiring via `scripts/ci_apply_teleprompter_ui.py` and `scripts/ci_apply_reading_position.py`.

### Reading position behavior

- **Save** on teleprompter pause, scroll stop, and (Android) activity `onStop`.
- **Restore** when beginning a studio take countdown if the active section (and script when known) matches the saved ids; otherwise start at the top.
- Changing to a different section does not reuse another section’s offset.

## Explicitly out of scope for this slice

- Voice-follow scrolling
- Camera / video preview on Record
- Forced alignment of captions to audio
- Phrase replacement (Phase 5)
- Per-section position map beyond the last active section

## Validation

- Edit script in Scripts → Teleprompter preview updates without leaving Creator.
- Change speed/size/spacing/pre-roll under Teleprompter → restart app → values restore.
- Scroll mid-script, pause or leave Record, return and start a take on the **same** section → scroll resumes near the saved offset.
- Switch section → reading position starts at top for the new section.
- Record studio teleprompter matches Creator Teleprompter settings after navigate.
- Record screen does not scroll as a page; teleprompter region is large.
- Record start starts scroll; pause stops scroll and recording together; stop ends both.
- Android and Windows CI green; manual device checks for scroll/pause coupling, pref restore, and reading-position restore.
