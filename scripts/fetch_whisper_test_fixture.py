#!/usr/bin/env python3
"""Fetch and verify the pinned JFK speech WAV fixture from whisper.cpp."""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import tempfile
import urllib.request

URL = "https://raw.githubusercontent.com/ggml-org/whisper.cpp/482279b8a18855ba741d7b88c30fcb3874691f59/samples/jfk.wav"
EXPECTED_SIZE = 352078
EXPECTED_GIT_BLOB_SHA1 = "3184d372cd2f8b804d3a540c70ec50d927b335d2"


def git_blob_sha1(data: bytes) -> str:
    return hashlib.sha1(f"blob {len(data)}\\0".encode("ascii") + data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="build/test-data")
    args = parser.parse_args()
    destination = Path(args.output)
    destination.mkdir(parents=True, exist_ok=True)
    target = destination / "whisper-speech.wav"
    if target.exists():
        data = target.read_bytes()
        if len(data) == EXPECTED_SIZE and git_blob_sha1(data) == EXPECTED_GIT_BLOB_SHA1:
            print(f"Verified cached Whisper fixture: {target}")
            return 0
        target.unlink()

    request = urllib.request.Request(URL, headers={"User-Agent": "Synvidra-CI/1.0"})
    temporary = None
    try:
        with urllib.request.urlopen(request, timeout=90) as response:
            with tempfile.NamedTemporaryFile(dir=destination, prefix="whisper-speech.", delete=False) as stream:
                temporary = Path(stream.name)
                while True:
                    chunk = response.read(1024 * 1024)
                    if not chunk:
                        break
                    stream.write(chunk)
        data = temporary.read_bytes()
        digest = git_blob_sha1(data)
        if len(data) != EXPECTED_SIZE or digest != EXPECTED_GIT_BLOB_SHA1:
            raise ValueError(f"Whisper fixture integrity mismatch: size={len(data)}, git_blob_sha1={digest}")
        os.replace(temporary, target)
    finally:
        if temporary and temporary.exists():
            temporary.unlink()

    print(f"Fetched and verified pinned JFK speech fixture ({EXPECTED_SIZE} bytes, git blob {EXPECTED_GIT_BLOB_SHA1})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
