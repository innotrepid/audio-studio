# Creator → Teleprompter → Record flow

**Status:** Active implementation  
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
| Teleprompter settings | Creator Teleprompter (persisted prefs) | Record studio teleprompter |
| Takes | Record engine + project take refs | Library / Editor / Production |

Settings persistence may use platform preference stores initially; project JSON may later embed a `teleprompter` prefs object without blocking UI work.

## Explicitly out of scope for this slice

- Voice-follow scrolling
- Camera / video preview on Record
- Forced alignment of captions to audio
- Phrase replacement (Phase 5)

## Validation

- Edit script in Scripts → Teleprompter preview updates without leaving Creator.
- Change speed/size/spacing under Teleprompter → Record studio teleprompter matches after navigating to Record.
- Record screen does not scroll as a page; teleprompter region is large.
- Record start starts scroll; pause stops scroll and recording together; stop ends both.
- Android and Windows CI green; manual device checks for scroll/pause coupling.
