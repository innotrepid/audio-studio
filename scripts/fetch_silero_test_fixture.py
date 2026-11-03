#!/usr/bin/env python3
"""Fetch and verify the pinned upstream Silero VAD speech WAV fixture."""
from __future__ import annotations
import argparse
import hashlib
import os
from pathlib import Path
import tempfile
import urllib.request

URL = "https://raw.githubusercontent.com/snakers4/silero-vad/60b7ffa243625ebdc1070275a29f18c87843786a/tests/data/test.wav"
EXPECTED_SIZE = 1_920_044
EXPECTED_BLOB_SHA1 = "0b1b64ecd9b316445e06cfd5b6b709b0e5fcf18f"

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="build/test-data")
    args = parser.parse_args()
    destination = Path(args.output)
    destination.mkdir(parents=True, exist_ok=True)
    target = destination / "silero-test.wav"
    if target.exists():
        data = target.read_bytes()
        blob = hashlib.sha1(f"blob {len(data)}\\0".encode("ascii") + data).hexdigest()
        if len(data) == EXPECTED_SIZE and blob == EXPECTED_BLOB_SHA1:
            print(f"Verified cached fixture: {target}")
            return 0
        target.unlink()
    request = urllib.request.Request(URL, headers={"User-Agent": "AudioStudio-CI/1.0"})
    temporary = None
    try:
        with urllib.request.urlopen(request, timeout=90) as response:
            with tempfile.NamedTemporaryFile(dir=destination, prefix="silero-test.", delete=False) as stream:
                temporary = Path(stream.name)
                digest = hashlib.sha1()
                size = 0
                while True:
                    chunk = response.read(1024 * 1024)
                    if not chunk:
                        break
                    stream.write(chunk)
                    digest.update(chunk)
                    size += len(chunk)
        blob_sha = hashlib.sha1(f"blob {size}\\0".encode("ascii") + temporary.read_bytes()).hexdigest()
        if size != EXPECTED_SIZE or blob_sha != EXPECTED_BLOB_SHA1:
            raise ValueError(f"Fixture integrity mismatch: size={size}, git_blob_sha1={blob_sha}")
        os.replace(temporary, target)
    finally:
        if temporary and temporary.exists():
            temporary.unlink()
    print(f"Fetched and verified pinned Silero fixture ({EXPECTED_SIZE} bytes, git blob {EXPECTED_BLOB_SHA1})")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
