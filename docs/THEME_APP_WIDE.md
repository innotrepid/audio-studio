# App-wide theme (Hybrid Studio Phase E)

**Status:** Runtime apply on Android (window chrome + Settings radios)  
**Principle:** Theme restyles chrome; it **never removes** destinations, controls, or functions.

## Preference

| Choice | Behavior |
|--------|----------|
| **System default** | Outer window / system bars follow OS light/dark; panels stay studio surfaces |
| **Dark studio** | Force charcoal status/nav/window backgrounds |

Stored in SharedPreferences `synvidra_app_preferences` / `theme_mode` (`0` system, `1` dark).

## Implementation

| File | Role |
|------|------|
| `StudioTheme.kt` | Tokens, resolve, `applyWindowChrome` |
| `SettingsScreen.kt` | Appearance radios → `setMode` + `onThemeChanged` |
| `ci_apply_theme_apply_runtime.py` | Early apply in `onCreate`, root background, Settings callback |

## Token story

| Role | Dark studio |
|------|-------------|
| Window | `#101218` |
| Surface | `#1C222D` |
| Status / nav | `#0C0E14` |

Full per-control light palette rewrite is **out of scope** for this slice (avoids MainActivity color churn).

## Acceptance

- [ ] Cold start applies stored mode to system bars and root background
- [ ] Toggling Appearance radios updates chrome immediately
- [ ] CI green
