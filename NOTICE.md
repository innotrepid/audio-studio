# Synvidra — Copyright and third-party notices

This file is the attribution and license notice companion for the **Synvidra** product
(repository name may still appear as `audio-studio`).

---

## 1. Synvidra product copyright (Innotrepid)

**Copyright © Innotrepid and contributors. All rights reserved.**

Synvidra is the product name for the native Android and Windows voice recording and
narration studio in this repository, including without limitation:

- Application source code (Kotlin, C#, C/C++, scripts, build files)
- Product documentation under `docs/` (except third-party text reproduced for attribution)
- Branding, product name **Synvidra**, UI copy, design tokens, and Hybrid Studio interaction design as expressed in this codebase
- Original audio engine logic, Creator Studio project formats, and non-third-party assets authored for this product

### Rights reserved

Unless Innotrepid publishes a separate open-source or commercial license grant that
expressly applies to a given copy of the Software:

1. **No license is granted** by default to copy, modify, merge, publish, distribute,
   sublicense, sell, or create derivative works of Synvidra source or binaries.
2. **Trademarks:** “Synvidra” and “Innotrepid” are used to identify the product and
   rightsholder. No trademark license is granted by this notice.
3. **User content:** Recordings, scripts, captions, and project files created by end
   users remain the users’ content; Synvidra does not claim ownership of user media.
4. **Contributions:** Unless a contributor agreement states otherwise, contributions
   submitted to Innotrepid for Synvidra are provided for inclusion in the product and
   do not alter the default “all rights reserved” posture of the product as a whole.

### Disclaimer

THE SYNVIDRA SOFTWARE IS PROVIDED BY INNOTREPID “AS IS” AND WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT. IN NO EVENT SHALL INNOTREPID
OR AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY ARISING FROM USE OF
THE SOFTWARE.

If a `LICENSE` file is later added to the repository root, that file controls the
terms for the Synvidra codebase to the extent it expressly grants rights; this NOTICE
continues to require preservation of **third-party** notices below.

---

## 2. Third-party components and models

The following components may be linked, vendored, packaged, or downloaded by build
scripts. **Their licenses are not replaced by Innotrepid’s copyright.** Redistribution
of Synvidra builds that include these components must preserve the applicable notices.

| Component | Role in Synvidra | License (as commonly published upstream) | Upstream |
|-----------|------------------|------------------------------------------|----------|
| **Google Oboe** | Android low-latency audio I/O | **Apache License 2.0** | https://github.com/google/oboe |
| **ONNX Runtime** (Microsoft) | ONNX inference (e.g. Silero VAD) | **MIT** | https://github.com/microsoft/onnxruntime |
| **Silero VAD** (model + reference) | Voice activity detection | **MIT** (Copyright © 2020–present Silero Team) | https://github.com/snakers4/silero-vad |
| **whisper.cpp / ggml** | Local speech-to-text runtime | **MIT** (ggml authors) | https://github.com/ggml-org/whisper.cpp |
| **OpenAI Whisper** (model lineage) | ASR model weights converted to ggml | **MIT** (OpenAI Whisper project) | https://github.com/openai/whisper |
| **RNNoise** | Lightweight neural denoise path | **BSD-3-Clause** (typical upstream) | Jean-Marc Valin / RNNoise lineage |
| **DeepFilterNet** | Experimental full-band denoise (BETA) | **MIT** (typical upstream DeepFilterNet) | DeepFilterNet project |
| **Windows App SDK / WinUI 3** | Windows UI shell | Microsoft product terms / redistributable packages as installed by the Windows build | Microsoft |
| **.NET / Windows SDK** | Windows build and runtime | Microsoft product terms | Microsoft |
| **Android SDK / NDK / Gradle plugins** | Android build toolchain | Google / Android SDK terms | Google |
| **DirectML** (if linked) | Optional Windows ML acceleration | Microsoft redistributable terms | Microsoft |

### Notes on models

- **Silero VAD ONNX** weights distributed with the app remain under the Silero **MIT**
  license; include the Silero copyright notice in distributions that ship the model.
- **Whisper `ggml-tiny` (and related) weights** used offline are derived from the
  OpenAI Whisper model family and redistributed in ggml form via the whisper.cpp
  ecosystem; preserve MIT attribution for whisper.cpp and the Whisper project.
- **Checksum-pinned fetches** in `scripts/fetch_transcription_models.py` must continue
  to pull only components whose licenses permit the intended distribution model.

### Notes on noise-cancellation vendor path

CI may build RNNoise / DeepFilterNet C ABI artifacts from a pinned vendor revision
(e.g. noise-cancellation related repositories). Treat the **upstream license files**
in that vendor tree as authoritative for those binaries and keep this table updated
if the pin changes.

---

## 3. Operator obligations (summary)

When you distribute a Synvidra build that embeds third-party code or models:

1. Keep this `NOTICE.md` (or equivalent) with the distribution.
2. Do not remove copyright headers from third-party sources you redistribute.
3. Do not claim third-party MIT/Apache/BSD code is owned by Innotrepid.
4. Do not claim Synvidra product source is open source unless Innotrepid publishes
   an explicit license grant.

---

## 4. Contact

Rights and licensing inquiries regarding **Synvidra** / **Innotrepid** should be
directed through the channels Innotrepid publishes for the product. This file is
not legal advice; verify upstream license files before commercial redistribution.
