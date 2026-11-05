#!/usr/bin/env python3
"""Bootstrap: materialize full apply script from sibling base64 parts."""
from __future__ import annotations

import base64
import runpy
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PARTS = sorted(HERE.glob("ci_apply_teleprompter_ui.b64.part*"))
if not PARTS:
    raise SystemExit("Missing ci_apply_teleprompter_ui.b64.part* payloads")

payload = "".join(p.read_text(encoding="ascii").strip() for p in PARTS)
code = base64.b64decode(payload)
target = HERE / "_ci_apply_teleprompter_ui_impl.py"
target.write_bytes(code)

# Execute implementation as __main__ with same argv
sys.argv[0] = str(target)
runpy.run_path(str(target), run_name="__main__")
