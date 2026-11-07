#!/usr/bin/env python3
"""Phase G: Android Settings parity — docs only; no MainActivity injects."""
from __future__ import annotations

import sys


def ensure_android() -> None:
    # Intentionally no MainActivity mutation. Implement SettingsScreen.kt + nav
    # branch on a future PR; see docs/PHASE_G_ANDROID_SETTINGS.md.
    print("Android: Phase G Settings parity no-op (docs-only; safe)")


def ensure_windows() -> None:
    print("Windows: Phase G no-op (Settings already additive where applied)")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phase_g_android_settings:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
