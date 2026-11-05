# Creator → Teleprompter → Record flow

**Status:** Active — preference persistence on `feat/teleprompter-prefs-persistence`  
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

## Data ownership

| Data | Owner | Consumers |
|------|--------|-----------|
| Script body / sections | Creator Scripts + `CreatorProjectStore` | Creator Teleprompter (auto), Record studio teleprompter (via Teleprompter mirror) |
| Teleprompter settings | Creator Teleprompter (**persisted**) | Record studio teleprompter |
| Takes | Record engine + project take refs | Library / Editor / Production |

### Preference persistence

| Platform | Store | Keys |
|----------|--------|------|
| Android | `SharedPreferences` name `teleprompter_preferences` | `speed_progress`, `font_progress`, `spacing_progress`, `countdown_index` |
| Windows | `ApplicationData.Current.LocalSettings` | `teleprompter.speed`, `teleprompter.fontSize`, `teleprompter.lineSpacing`, `teleprompter.countdownIndex` |

Helpers: `TeleprompterPreferences.kt` / `TeleprompterPreferences.cs`. CI applies UI wiring via `scripts/ci_apply_teleprompter_ui.py` (load on start, save on control change; pre-roll spinner hosted under Creator Teleprompter).

## Explicitly out of scope for this slice

- Voice-follow scrolling
- Camera / video preview on Record
- Forced alignment of captions to audio
- Phrase replacement (Phase 5)
- Reading-position restore (next slice)

## Validation

- Edit script in Scripts → Teleprompter preview updates without leaving Creator.
- Change speed/size/spacing/pre-roll under Teleprompter → restart app → values restore.
- Record studio teleprompter matches Creator Teleprompter settings after navigate.
- Record screen does not scroll as a page; teleprompter region is large.
- Record start starts scroll; pause stops scroll and recording together; stop ends both.
- Android and Windows CI green; manual device checks for scroll/pause coupling and pref restore.
