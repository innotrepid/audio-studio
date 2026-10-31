#!/usr/bin/env python3
"""Fetch and verify Synvidra's pinned offline transcription model artifacts.

Downloads only the exact URLs in models/transcription-models.json. Every artifact
must have a SHA-256 and exact byte count; failures stop packaging immediately.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import urllib.request


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fetch(model: dict, destination: Path) -> None:
    filename = model["filename"]
    expected_hash = model.get("sha256")
    expected_size = model.get("sizeBytes")
    if not expected_hash or not isinstance(expected_size, int) or expected_size <= 0:
        raise ValueError(f"{model['id']}: manifest must pin SHA-256 and positive sizeBytes")

    destination.mkdir(parents=True, exist_ok=True)
    target = destination / filename
    if target.exists():
        actual_size = target.stat().st_size
        actual_hash = sha256_file(target)
        if actual_size == expected_size and actual_hash == expected_hash:
            print(f"Verified cached {filename} ({actual_size} bytes)")
            return
        target.unlink()

    request = urllib.request.Request(
        model["source"],
        headers={"User-Agent": "Synvidra-model-fetch/1.0"},
    )
    with urllib.request.urlopen(request, timeout=90) as response:
        with tempfile.NamedTemporaryFile(dir=destination, prefix=filename + ".", delete=False) as temp:
            temporary = Path(temp.name)
            digest = hashlib.sha256()
            total = 0
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                temp.write(chunk)
                digest.update(chunk)
                total += len(chunk)

    try:
        if total != expected_size:
            raise ValueError(f"{filename}: size mismatch: expected {expected_size}, got {total}")
        actual_hash = digest.hexdigest()
        if actual_hash.lower() != expected_hash.lower():
            raise ValueError(f"{filename}: SHA-256 mismatch: expected {expected_hash}, got {actual_hash}")
        os.replace(temporary, target)
    finally:
        if temporary.exists():
            temporary.unlink()

    print(f"Fetched and verified {filename} ({total} bytes, sha256 {expected_hash})")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", default="models/transcription-models.json")
    parser.add_argument("--output", default="build/transcription-models")
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    models = manifest.get("models")
    if not isinstance(models, list) or not models:
        raise ValueError("Manifest has no models")
    output = Path(args.output)
    for model in models:
        fetch(model, output)
    print(f"All {len(models)} bundled model artifacts verified in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
