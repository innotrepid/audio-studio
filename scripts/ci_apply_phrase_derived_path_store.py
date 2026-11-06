#!/usr/bin/env python3
"""Add optional derivedPath to phrase replacement requests (Android + Windows)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/CreatorProjectStore.kt"
    text = path.read_text(encoding="utf-8")
    if "derivedPath" in text and "phraseReplacementRequests" in text:
        # Ensure field on data class
        if "val derivedPath" in text:
            print("Android derivedPath already present")
            return

    if "val derivedPath" not in text:
        if "val assemblyStatus: String = \"none\"," in text:
            text = text.replace(
                'val assemblyStatus: String = "none",',
                'val assemblyStatus: String = "none",\n        val derivedPath: String? = null,',
                1,
            )
        elif "val assemblyStatus: String = \"none\"" in text:
            text = text.replace(
                'val assemblyStatus: String = "none"',
                'val assemblyStatus: String = "none",\n        val derivedPath: String? = null',
                1,
            )
        else:
            raise SystemExit("Cannot find assemblyStatus field for derivedPath")

    if '.put("derivedPath"' not in text:
        if '.put("assemblyStatus", req.assemblyStatus)' in text:
            text = text.replace(
                '.put("assemblyStatus", req.assemblyStatus)',
                '.put("assemblyStatus", req.assemblyStatus)\n                        .put("derivedPath", req.derivedPath ?: JSONObject.NULL)',
                1,
            )
        else:
            raise SystemExit("Cannot find encode site for derivedPath")

    if 'derivedPath = if (item.isNull("derivedPath"))' not in text:
        if 'assemblyStatus = item.optString("assemblyStatus", "none").ifBlank { "none" },' in text:
            text = text.replace(
                'assemblyStatus = item.optString("assemblyStatus", "none").ifBlank { "none" },',
                'assemblyStatus = item.optString("assemblyStatus", "none").ifBlank { "none" },\n'
                '                derivedPath = if (item.isNull("derivedPath")) null else item.optString("derivedPath").ifBlank { null },',
                1,
            )
        else:
            raise SystemExit("Cannot find decode site for derivedPath")

    path.write_text(text, encoding="utf-8")
    print("Android derivedPath wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/CreatorProjectStore.cs"
    text = path.read_text(encoding="utf-8")
    if "DerivedPath" in text and "PhraseReplacementRequest" in text:
        if "public string? DerivedPath" in text or "public string DerivedPath" in text:
            print("Windows DerivedPath already present")
            return

    if "public string? DerivedPath" not in text and "public string DerivedPath" not in text:
        if "public string AssemblyStatus { get; set; } = \"none\";" in text:
            text = text.replace(
                'public string AssemblyStatus { get; set; } = "none";',
                'public string AssemblyStatus { get; set; } = "none";\n        public string? DerivedPath { get; set; }',
                1,
            )
        else:
            # broader match
            needle = "AssemblyStatus"
            if needle not in text:
                raise SystemExit("Cannot find AssemblyStatus for DerivedPath")
            # inject after AssemblyStatus property line
            import re

            text2, n = re.subn(
                r"(public string AssemblyStatus \{ get; set; \} = \"none\";)",
                r"\1\n        public string? DerivedPath { get; set; }",
                text,
                count=1,
            )
            if n == 0:
                raise SystemExit("Cannot inject DerivedPath property")
            text = text2

    # encode/decode JSON if present as explicit writers - CreatorProjectStore.cs may use System.Text.Json
    if "DerivedPath" not in text:
        raise SystemExit("DerivedPath injection failed")

    path.write_text(text, encoding="utf-8")
    print("Windows DerivedPath wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_derived_path_store:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
