#!/usr/bin/env python3
"""Verify pinned Whisper and Silero model artifacts are present with expected size/hash.

Used by CI after packaging so installable builds cannot ship without offline models.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_manifest(path: Path) -> list[dict]:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    models = manifest.get("models")
    if not isinstance(models, list) or not models:
        raise SystemExit(f"{path}: manifest has no models")
    return models


def verify_directory(models: list[dict], directory: Path) -> None:
    if not directory.is_dir():
        raise SystemExit(f"Model directory does not exist: {directory}")
    for model in models:
        filename = model["filename"]
        expected_hash = model["sha256"].lower()
        expected_size = int(model["sizeBytes"])
        target = directory / filename
        if not target.is_file():
            raise SystemExit(f"Missing packaged model: {target}")
        actual_size = target.stat().st_size
        if actual_size != expected_size:
            raise SystemExit(
                f"{filename}: size mismatch expected {expected_size}, got {actual_size}"
            )
        actual_hash = sha256_file(target)
        if actual_hash.lower() != expected_hash:
            raise SystemExit(
                f"{filename}: SHA-256 mismatch expected {expected_hash}, got {actual_hash}"
            )
        print(f"OK {target} ({actual_size} bytes)")


def verify_apk(models: list[dict], apk_path: Path, asset_prefix: str) -> None:
    if not apk_path.is_file():
        raise SystemExit(f"APK not found: {apk_path}")
    prefix = asset_prefix.rstrip("/") + "/"
    with zipfile.ZipFile(apk_path) as archive:
        names = set(archive.namelist())
        for model in models:
            filename = model["filename"]
            entry = prefix + filename
            if entry not in names:
                raise SystemExit(f"APK is missing asset {entry}")
            info = archive.getinfo(entry)
            expected_size = int(model["sizeBytes"])
            if info.file_size != expected_size:
                raise SystemExit(
                    f"{entry}: size mismatch expected {expected_size}, got {info.file_size}"
                )
            digest = hashlib.sha256()
            with archive.open(entry) as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
            actual_hash = digest.hexdigest()
            expected_hash = model["sha256"].lower()
            if actual_hash != expected_hash:
                raise SystemExit(
                    f"{entry}: SHA-256 mismatch expected {expected_hash}, got {actual_hash}"
                )
            print(f"OK apk:{entry} ({info.file_size} bytes)")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", default="models/transcription-models.json")
    parser.add_argument(
        "--directory",
        action="append",
        default=[],
        help="Directory that must contain every pinned model file",
    )
    parser.add_argument("--apk", help="APK path that must embed models under assets/")
    parser.add_argument(
        "--apk-asset-prefix",
        default="assets/models",
        help="Zip entry prefix inside the APK for model assets",
    )
    args = parser.parse_args()

    models = load_manifest(Path(args.manifest))
    if not args.directory and not args.apk:
        raise SystemExit("Provide --directory and/or --apk")

    for directory in args.directory:
        verify_directory(models, Path(directory))
    if args.apk:
        verify_apk(models, Path(args.apk), args.apk_asset_prefix)
    print(f"Verified {len(models)} pinned offline model artifact(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
